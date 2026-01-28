/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 *
 * This firmware implements an ASCOM Alpaca compatible Roll-Off Roof controller
 * using the ESP32-S3 microcontroller. It implements the ASCOM Dome interface
 * for controlling the roof movement. It also provides MQTT integration for
 * Home Assistant.
 *
 * HARDWARE: v3 (2026-01-07) - ESP32-S3 44-pin with 3 relays
 *        - K1: Inverter 12V power relay (GPIO4)
 *        - K2: Roof opener button relay (GPIO5)
 *        - K3: Inverter soft-power button relay (GPIO6) - NEW
 *        - AC Power Detection via optocoupler (GPIO7) - NEW
 *        - Support for 12V snow/rain sensor - NEW
 *
 * COMPILE SETTINGS for ESP32-S3:
 *        - Board: ESP32-S3 Dev Module
 *        - USB CDC On Boot: Enabled
 *        - Flash Size: 4MB (or larger)
 *        - Partition Scheme: Default 4MB with SPIFFS
 *        - Upload Speed: 921600
 *        - USB Mode: Hardware CDC and JTAG
 *
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Project includes
#include "config.h"
#include "Debug.h"
#include "roof_controller.h"
#include "alpaca_handler.h"
#include "mqtt_handler.h"
#include "web_ui_handler.h"
#include "park_sensor_udp.h"
#include "gps_handler.h"

// For reset reason detection
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"

// WiFi credentials and configuration
char ssid[SSID_SIZE] = DEFAULT_WIFI_SSID;
char password[PASSWORD_SIZE] = DEFAULT_WIFI_PASSWORD;
bool apMode = false;
unsigned long apStartTime = 0;

// Timing variables
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastStatusUpdate = 0;

// Reset diagnostics (accessible from web UI)
String lastResetReason = "Unknown";
uint32_t rebootCount = 0;

// Get human-readable reset reason
String getResetReasonString() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
    case ESP_RST_POWERON:   return "Power-on reset";
    case ESP_RST_EXT:       return "External reset (pin)";
    case ESP_RST_SW:        return "Software reset";
    case ESP_RST_PANIC:     return "Exception/panic reset";
    case ESP_RST_INT_WDT:   return "Interrupt watchdog reset";
    case ESP_RST_TASK_WDT:  return "Task watchdog reset";
    case ESP_RST_WDT:       return "Other watchdog reset";
    case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
    case ESP_RST_BROWNOUT:  return "BROWNOUT RESET";  // <-- This is what we're looking for
    case ESP_RST_SDIO:      return "SDIO reset";
    default:                return "Unknown reset (" + String(reason) + ")";
  }
}

void setup() {
  // Initialize debug output
  Debug.begin(115200);

  // Optionally disable brown-out detection (if motor causes voltage drops)
  #if DISABLE_BROWNOUT_DETECTION
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Debug.println("WARNING: Brown-out detection DISABLED");
  #endif

  // Print reset reason FIRST - critical for diagnosing reboots
  lastResetReason = getResetReasonString();
  Debug.println("\n\n========================================");
  Debug.println("RESET REASON: " + lastResetReason);
  Debug.println("========================================\n");

  // Load and increment reboot counter from preferences
  Preferences bootPrefs;
  bootPrefs.begin("bootDiag", false);
  rebootCount = bootPrefs.getULong("rebootCount", 0) + 1;
  bootPrefs.putULong("rebootCount", rebootCount);
  bootPrefs.putString("lastReset", lastResetReason);
  bootPrefs.end();
  Debug.printf("Reboot count: %lu\n", rebootCount);

  // If brown-out detected, log a warning
  if (esp_reset_reason() == ESP_RST_BROWNOUT) {
    Debug.println("WARNING: Brown-out detected! Check power supply.");
    Debug.println("The motor may be causing voltage drops.");
    Debug.println("Consider setting DISABLE_BROWNOUT_DETECTION=1 in config.h");
  }

  Debug.println("ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)");
  Debug.println("Version: " + String(DEVICE_VERSION));
  Debug.println("Manufacturer: " + String(DEVICE_MANUFACTURER));
  
  // Load configuration from preferences
  loadConfiguration();
  
  // Initialize roof controller hardware
  initializeRoofController();

  // Initialize RTC (DS3231) - do this early to have time available
  initRTC();

  // Initialize WiFi
  initWiFi();
  
  // Initialize UDP park sensor listener
  initParkSensorUDP();
  
  // Initialize MQTT if enabled
  if (mqttEnabled) {
    setupMQTT();
  } else {
    Debug.println("MQTT is disabled");
  }
  
  // Initialize Alpaca API
  setupAlpacaAPI();

  // Initialize Web UI
  initWebUI();

  // Initialize GPS if enabled
  if (gpsEnabled) {
    initGPS();
  } else {
    Debug.println("GPS is disabled");
  }

  // Initialize NTP server if enabled (works with GPS or RTC time)
  if (gpsNtpEnabled) {
    initNTP();
  }

  Debug.println("Setup complete!");
  Debug.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle WiFi connection
  handleWiFi();
  
  // Handle UDP park sensor messages
  handleParkSensorUDP();

  // Process non-blocking roof operations (state machine)
  // This handles relay timing without blocking WiFi/MQTT
  processRoofOperation();

  // Update roof status
  updateRoofStatus();

  // Update telescope park status
  updateTelescopeStatus();

  // Update inverter power status (NEW in v3)
  updateInverterPowerStatus();

  // Check for movement timeout
  checkMovementTimeout();
  
  // Handle Alpaca discovery
  handleAlpacaDiscovery();

  // Handle Alpaca endpoint requests
  alpacaServer.handleClient();
  
  // Handle MQTT
  if (mqttEnabled) {
    if (!mqttClient.connected()) {
      if (currentTime - lastMqttReconnectAttempt > 5000) {
        lastMqttReconnectAttempt = currentTime;
        reconnectMQTT();
      }
    } else {
      mqttClient.loop();
      
      // Publish status periodically
      if (currentTime - lastMqttPublish > MQTT_PUBLISH_INTERVAL) {
        publishStatusToMQTT();
        lastMqttPublish = currentTime;
      }
    }
  }
  
  // Handle web UI requests
  handleWebUI();

  // Handle GPS data
  if (gpsEnabled) {
    handleGPS();
  }

  // Handle NTP server (independent of GPS - uses GPS or RTC time)
  handleNTP();

  // Clear timed out park sensors periodically (every 5 minutes)
  static unsigned long lastSensorCleanup = 0;
  if (currentTime - lastSensorCleanup > 300000) {
    clearTimeoutSensors();
    lastSensorCleanup = currentTime;
  }
  
  // Periodic status update (every 30 seconds)
  if (currentTime - lastStatusUpdate > 30000) {
    Debug.printf(2, "Status: Roof=%s, Telescope=%s, WiFi=%s, MQTT=%s, Heap=%d\n",
                 getRoofStatusString().c_str(),
                 telescopeParked ? "Parked" : "Unparked",
                 WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
                 mqttClient.connected() ? "Connected" : "Disconnected",
                 ESP.getFreeHeap());
    // Time source status
    TimeSource ts = getTimeSource();
    Debug.printf(2, "Time: %s %s UTC (Source: %s, RTC: %s)\n",
                 getDateString().c_str(),
                 getTimeString().c_str(),
                 ts == TIME_SOURCE_GPS ? "GPS" : (ts == TIME_SOURCE_RTC ? "RTC" : "None"),
                 isRTCPresent() ? "Present" : "Not found");
    Debug.printf(2, "Local: %s %s (TZ: %+d min, DST: %s)\n",
                 getLocalDateString().c_str(),
                 getLocalTimeString().c_str(),
                 timezoneOffset,
                 dstEnabled ? "On" : "Off");
    if (gpsEnabled) {
      GPSStatus gpsStatusData = getGPSStatus();
      Debug.printf(2, "GPS: Fix=%s, Sats=%d, Lat=%.6f, Lon=%.6f\n",
                   gpsStatusData.hasFix ? "Yes" : "No",
                   gpsStatusData.satellites,
                   gpsStatusData.latitude,
                   gpsStatusData.longitude);
    }
    lastStatusUpdate = currentTime;
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}

// Initialize WiFi connection
void initWiFi() {
  Debug.println("Initializing WiFi...");

  // Clean up any existing WiFi state
  WiFi.disconnect(true);  // Disconnect and clear stored credentials
  WiFi.softAPdisconnect(true);  // Stop any running AP
  delay(100);  // Allow cleanup to complete

  // Check if we have credentials to connect with
  if (strlen(ssid) > 0 && strcmp(ssid, "YOUR_SSID") != 0) {
    Debug.printf("Connecting to WiFi network: %s\n", ssid);

    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    delay(100);

    WiFi.begin(ssid, password);

    // Wait up to 30 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
      delay(500);
      Debug.print(".");
      attempts++;
    }
    Debug.println("");

    if (WiFi.status() == WL_CONNECTED) {
      Debug.println("WiFi connected successfully!");
      Debug.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
      Debug.printf("Signal strength: %d dBm\n", WiFi.RSSI());
      apMode = false;
    } else {
      Debug.printf("Failed to connect to WiFi (status: %d), starting AP mode\n", WiFi.status());
      startAPMode();
    }
  } else {
    Debug.println("No valid WiFi credentials configured, starting AP mode");
    startAPMode();
  }
}

// Start Access Point mode for configuration
void startAPMode() {
  Debug.println("Starting AP mode...");

  // Clean up WiFi state before starting AP
  WiFi.disconnect(true);
  delay(100);

  // Set WiFi mode to AP
  WiFi.mode(WIFI_AP);
  delay(100);

  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Debug.printf("AP mode started - SSID: %s, Password: %s\n", AP_SSID, AP_PASSWORD);
    Debug.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    apMode = true;
    apStartTime = millis();
  } else {
    Debug.println("Failed to start AP mode!");
    // Try one more time after a longer delay
    delay(1000);
    if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
      Debug.println("AP mode started on retry");
      Debug.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
      apMode = true;
      apStartTime = millis();
    } else {
      Debug.println("AP mode failed to start after retry!");
    }
  }
}

// WiFi reconnection state (non-blocking)
bool wifiReconnecting = false;
unsigned long wifiReconnectStartTime = 0;
const unsigned long WIFI_RECONNECT_TIMEOUT = 30000;  // 30 seconds max

// Handle WiFi connection in main loop (NON-BLOCKING)
void handleWiFi() {
  if (apMode) {
    // Check if we should exit AP mode after timeout
    if (millis() - apStartTime > AP_TIMEOUT) {
      Debug.println("AP mode timeout, attempting to reconnect to WiFi");
      // Stop AP before trying to reconnect
      WiFi.softAPdisconnect(true);
      delay(100);
      initWiFi();
    }
  } else {
    // Check if WiFi connection is lost
    if (WiFi.status() != WL_CONNECTED) {
      if (!wifiReconnecting) {
        // Start reconnection attempt
        Debug.printf("WiFi connection lost (status: %d), attempting to reconnect...\n", WiFi.status());
        WiFi.reconnect();
        wifiReconnecting = true;
        wifiReconnectStartTime = millis();
      } else {
        // Already reconnecting - check if we've timed out
        if (millis() - wifiReconnectStartTime > WIFI_RECONNECT_TIMEOUT) {
          Debug.printf("\nFailed to reconnect after %lu seconds (status: %d), starting AP mode\n",
                       WIFI_RECONNECT_TIMEOUT / 1000, WiFi.status());
          wifiReconnecting = false;
          startAPMode();
        }
        // Otherwise, just let the reconnection continue in the background
        // WiFi.reconnect() works asynchronously - no need to block
      }
    } else {
      // WiFi is connected
      if (wifiReconnecting) {
        Debug.println("WiFi reconnected successfully!");
        Debug.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        wifiReconnecting = false;
      }
    }
  }
}