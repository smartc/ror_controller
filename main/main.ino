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
 * COMPILE SETTINGS for ESP32-S3 (v4):
 *        - Board: ESP32-S3 Dev Module
 *        - USB CDC On Boot: Enabled
 *        - Flash Size: 16MB (128Mb)        <-- required
 *        - Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)  <-- sets the 3MB size limit;
 *          the actual table comes from partitions.csv in this folder (same app sizes)
 *        - PSRAM: Disabled                 <-- required: octal PSRAM shares GPIO35/36 (limit switches)
 *        - Upload Speed: 921600
 *        - USB Mode: Hardware CDC and JTAG
 *
 *        v3 -> v4 needs one USB flash (bootloader + partition table change); OTA
 *        from v3 is rejected because v4 no longer fits the old 1.25MB app slot.
 *
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Project includes
#include "config.h"
#include "src/debug/Debug.h"
#include "src/roof/roof_controller.h"
#include "src/alpaca/alpaca_handler.h"
#include "src/mqtt/mqtt_handler.h"
#include "src/webserver/web_ui_handler.h"
#include "src/sensors/park_sensor_udp.h"
#include "src/safety/safety_sensor.h"
#include "src/sensors/gps_handler.h"
#include "src/net/network_manager.h"
#include "src/log/fram_log.h"
#include "src/log/log_archive.h"
#include "src/hw/i2c_bus.h"

// For reset reason detection
#include "esp_system.h"
#include "esp_partition.h"
#include "soc/rtc_cntl_reg.h"

// Timing variables
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastStatusUpdate = 0;

// Reset diagnostics (accessible from web UI)
String lastResetReason = "Unknown";
uint32_t rebootCount = 0;

// Cause of a deliberate restart, kept in RTC memory: it survives a software reset
// but not a power cycle, so it's only trusted with the magic and ESP_RST_SW.
#define RESTART_CAUSE_MAGIC 0x52535400UL  // "RST\0" + cause in the low byte
RTC_NOINIT_ATTR static uint32_t restartCauseWord;

void noteRestartCause(RestartCause cause) {
  restartCauseWord = RESTART_CAUSE_MAGIC | cause;
}

// Reads and clears the noted cause (one boot only)
static RestartCause takeRestartCause() {
  uint32_t word = restartCauseWord;
  restartCauseWord = 0;
  if ((word & 0xFFFFFF00UL) != RESTART_CAUSE_MAGIC) return RESTART_CAUSE_NONE;
  return (RestartCause)(word & 0xFF);
}

static String softwareResetString(RestartCause cause) {
  switch (cause) {
    case RESTART_CAUSE_OTA:           return "Software reset (OTA update)";
    case RESTART_CAUSE_WEB:           return "Software reset (Web UI restart)";
    case RESTART_CAUSE_FACTORY_RESET: return "Software reset (factory reset)";
    default:                          return "Software reset";
  }
}

// Get human-readable reset reason
String getResetReasonString() {
  esp_reset_reason_t reason = esp_reset_reason();
  RestartCause cause = takeRestartCause();
  switch (reason) {
    case ESP_RST_POWERON:   return "Power-on reset";
    case ESP_RST_EXT:       return "External reset (pin)";
    case ESP_RST_SW:        return softwareResetString(cause);
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

// Timestamp source for log lines: UTC once any time source is synced, else 0
static uint32_t logUtcNow() {
  return isTimeSynced() ? getCurrentUnixTime() : 0;
}

// v4 needs the 16MB layout from partitions.csv. A board flashed with the wrong
// IDE settings still runs, but without the log archive, so make that loud.
void checkFlashLayout() {
  uint32_t flashSize = ESP.getFlashChipSize();
  const esp_partition_t* logPart =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  Debug.printf("Flash: %lu MB, log partition: %s\n", flashSize / (1024 * 1024),
               logPart ? (String(logPart->size / 1024) + " KB").c_str() : "MISSING");
  if (flashSize < 16UL * 1024 * 1024 || logPart == nullptr || logPart->size < 0x100000) {
    Debug.println("ERROR: firmware not built for the 16MB layout (Flash Size 16MB + partitions.csv).");
    Debug.println("ERROR: log archive disabled; FRAM log and console still work.");
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

  // Shared I2C bus (DS3231 RTC + FRAM log). Start it once, before either uses it.
  i2cBusBegin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  // Persistent log: from here on every line is queued for FRAM the moment it's
  // logged (the logger task writes it), and the lines above are replayed too.
  logSetTimeSource(logUtcNow);
  framLogBegin((uint16_t)rebootCount);
  logArchiveSetBoot((uint16_t)rebootCount);
  logEnablePersistQueue(64);
  Debug.printf("=== BOOT #%lu - v%s - %s ===\n", rebootCount, DEVICE_VERSION, lastResetReason.c_str());

  // If brown-out detected, log a warning
  if (esp_reset_reason() == ESP_RST_BROWNOUT) {
    Debug.println("WARNING: Brown-out detected! Check power supply.");
    Debug.println("The motor may be causing voltage drops.");
    Debug.println("Consider setting DISABLE_BROWNOUT_DETECTION=1 in config.h");
  }

  Debug.println("ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)");
  Debug.println("Version: " + String(DEVICE_VERSION));
  Debug.println("Manufacturer: " + String(DEVICE_MANUFACTURER));
  checkFlashLayout();
  logArchiveBegin();
  logArchiveStartTask();

  // Load configuration from preferences
  loadConfiguration();
  loadNetworkConfiguration();

  // Initialize roof controller hardware
  initializeRoofController();

  // Roof monitoring runs in its own task from here on - before networking,
  // which can block for up to 30 s while WiFi connects
  startRoofTask();

  // Initialize RTC (DS3231) - do this early to have time available
  initRTC();

  // Initialize networking (WiFi primary, optional W5500 Ethernet)
  initNetwork();

  // Network time is the fallback when neither GPS nor the RTC has time
  initNetworkTime();
  
  // Initialize UDP park sensor listener
  initParkSensorUDP();

  // Load safety sensor configuration (per-sensor enable/bypass, global bypass, auto-close)
  loadSafetySensorConfiguration();
  
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
  
  // Handle WiFi/Ethernet connection state
  handleNetwork();
  
  // Handle UDP park sensor messages
  handleParkSensorUDP();

  // Hand the network-fed safety inputs to the roof task (it treats them as
  // stale if this loop stops refreshing them). Roof monitoring itself -
  // relays, limits, interlock, timeouts, auto-close - runs in the roof task.
  roofUpdateNetInputs(isTelescopeParkedUDP(), isWeatherSafe());

  // Publish roof state changes to MQTT (the roof task only flags them)
  if (roofTakeChanged()) {
    publishStatusToMQTT();
  }

  // Handle Alpaca discovery
  handleAlpacaDiscovery();

  // Handle Alpaca endpoint requests
  alpacaServer.handleClient();
  
  // Handle MQTT
  if (mqttEnabled && netIsConnected()) {
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

  // Required by ElegantOTA v3 to trigger reboot after successful upload
  ElegantOTA.loop();

  // Handle GPS data
  if (gpsEnabled) {
    handleGPS();
  }

  // Handle NTP server (independent of GPS - uses GPS or RTC time)
  handleNTP();

  // Pick up network (SNTP) time when GPS/RTC have none
  handleTimeSync();


  // Clear timed out park sensors periodically (every 5 minutes)
  static unsigned long lastSensorCleanup = 0;
  if (currentTime - lastSensorCleanup > 300000) {
    clearTimeoutSensors();
    lastSensorCleanup = currentTime;
  }
  
  // Periodic status update (every 30 seconds)
  if (currentTime - lastStatusUpdate > 30000) {
    Debug.printf(2, "Status: Roof=%s, Telescope=%s, WiFi=%s, Ethernet=%s, Active=%s, MQTT=%s, Heap=%d\n",
                 getRoofStatusString().c_str(),
                 telescopeParked ? "Parked" : "Unparked",
                 wifiIsConnected() ? "Connected" : "Disconnected",
                 ethHasIP() ? "Connected" : (ethIsStarted() ? "No link/IP" : "Off"),
                 netActiveInterfaceName(),
                 mqttClient.connected() ? "Connected" : "Disconnected",
                 ESP.getFreeHeap());
    // Time source status
    Debug.printf(2, "Time: %s %s UTC (Source: %s, RTC: %s)\n",
                 getDateString().c_str(),
                 getTimeString().c_str(),
                 getTimeSourceName(),
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
