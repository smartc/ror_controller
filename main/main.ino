/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * 
 * This firmware implements an ASCOM Alpaca compatible Roll-Off Roof controller
 * using the ESP32 microcontroller. It implements the ASCOM Dome interface
 * for controlling the roof movement. It also provides MQTT integration for
 * Home Assistant.
 *
 * NOTES: v2 hardware (2024-08-05) is missing a pulldown resistor on D12.
 *        To use D12 as Park sensor input, jumper right-hand side of [PARK] terminal to ground with a 1M resistor and set TELESCOPE_PARKED to HIGH or use a Normally Closed limit switch.
 *        Should be resolved in later hardware versions, which align input configuration with the [OPEN] / [CLOSED] switches. (But you may need to set TELESCOPE_PARKED to LOW in that case.)
 *
 *        For the ESP32 S Devkit boards I have been using, compile using the `VintLabs ESP32 Devkit` board profile.
 *        - Set Flash Frequency to 40 MHz
 *        - Baud rate of 921600 seems to work with the USB-c version of the boards
 *        - 4MB with SPIFFS (default) seems to work
 *        - Compiling with other board profiles may not work or does not provide partition for OTA updates
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

// WiFi credentials and configuration
char ssid[SSID_SIZE] = DEFAULT_WIFI_SSID;
char password[PASSWORD_SIZE] = DEFAULT_WIFI_PASSWORD;
bool apMode = false;
unsigned long apStartTime = 0;

// Timing variables
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastStatusUpdate = 0;

void setup() {
  // Initialize debug output
  Debug.begin(115200);
  Debug.println("ESP32 ASCOM Alpaca Roll-Off Roof Controller");
  Debug.println("Version: " + String(DEVICE_VERSION));
  Debug.println("Manufacturer: " + String(DEVICE_MANUFACTURER));
  
  // Load configuration from preferences
  loadConfiguration();
  
  // Initialize roof controller hardware
  initializeRoofController();
  
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
  
  Debug.println("Setup complete!");
  Debug.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle WiFi connection
  handleWiFi();
  
  // Handle UDP park sensor messages
  handleParkSensorUDP();
  
  // Update roof status
  updateRoofStatus();
  
  // Update telescope park status
  updateTelescopeStatus();
  
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
    lastStatusUpdate = currentTime;
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}

// Initialize WiFi connection
void initWiFi() {
  Debug.println("Initializing WiFi...");
  
  // Set WiFi mode
  WiFi.mode(WIFI_STA);
  
  // Try to connect to saved network
  if (strlen(ssid) > 0) {
    Debug.printf("Connecting to WiFi network: %s\n", ssid);
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
      Debug.println("Failed to connect to WiFi, starting AP mode");
      startAPMode();
    }
  } else {
    Debug.println("No WiFi credentials saved, starting AP mode");
    startAPMode();
  }
}

// Start Access Point mode for configuration
void startAPMode() {
  WiFi.mode(WIFI_AP);
  
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Debug.printf("AP mode started - SSID: %s, Password: %s\n", AP_SSID, AP_PASSWORD);
    Debug.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    apMode = true;
    apStartTime = millis();
  } else {
    Debug.println("Failed to start AP mode!");
  }
}

// Handle WiFi connection in main loop
void handleWiFi() {
  if (apMode) {
    // Check if we should exit AP mode after timeout
    if (millis() - apStartTime > AP_TIMEOUT) {
      Debug.println("AP mode timeout, attempting to reconnect to WiFi");
      initWiFi();
    }
  } else {
    // Check if WiFi connection is lost
    if (WiFi.status() != WL_CONNECTED) {
      Debug.println("WiFi connection lost, attempting to reconnect...");
      WiFi.reconnect();
      
      // If reconnection fails after 30 seconds, start AP mode
      unsigned long reconnectStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - reconnectStart < 30000) {
        delay(500);
        Debug.print(".");
      }
      
      if (WiFi.status() != WL_CONNECTED) {
        Debug.println("\nFailed to reconnect, starting AP mode");
        startAPMode();
      } else {
        Debug.println("\nWiFi reconnected successfully!");
      }
    }
  }
}