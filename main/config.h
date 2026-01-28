/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * Configuration Header File
 * Hardware: ESP32-S3 44-pin with 3 relays
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>  // Include Arduino.h to get LOW/HIGH and uint32_t definitions

// Debug level setting
#define DEBUG_LEVEL 1  // 0=Off, 1=Basic, 2=Verbose

// Brown-out detection setting
// Set to 1 to DISABLE brown-out detection (use if motor causes resets)
// WARNING: Disabling brown-out detection can cause data corruption if voltage drops too low
#define DISABLE_BROWNOUT_DETECTION 0

// Version Information
#define DEVICE_VERSION "3.1.2"  // v3 hardware with ESP32-S3, GPS/NTP support
#define DEVICE_MANUFACTURER "Corey Smart"
#define DEVICE_NAME "ESP32-S3 Roll-Off Roof Controller (v3)"

// GPIO pin definitions for ESP32-S3 v3 Hardware
// Control Pins - Relays
const int INVERTER_PIN = 4;                 // K1: Inverter 12V power relay
const int ROOF_CONTROL_PIN = 5;             // K2: Roof opener button relay
const int INVERTER_BUTTON_PIN = 6;          // K3: Inverter soft-power button relay (NEW in v3)

// Input Pins - Status and Sensors
const int INVERTER_AC_POWER_PIN = 7;        // AC power state detection via optocoupler (NEW in v3)
const int TELESCOPE_PARKED_PIN = 42;        // Safety interlock on telescope park position
const int RAIN_SENSOR_PIN = 37;             // RG9 Rain sensor input

// Snow Sensor Pins (NEW in v3)
const int SNOW_SENSOR_DIGITAL_PIN = 38;     // 12V Snow sensor digital input
const int SNOW_SENSOR_RS485_RO = 41;        // RS485 RO (Receiver Output)
const int SNOW_SENSOR_RS485_RE_DE = 39;     // RS485 RE/DE (Receiver Enable / Driver Enable)
const int SNOW_SENSOR_RS485_DI = 40;        // RS485 DI (Driver Input)

// GPS Module Pins - configurable via WebUI
extern int gpsTxPin;                        // GPS TX -> ESP32 RX (receives GPS data)
extern int gpsRxPin;                        // GPS RX -> ESP32 TX (send commands to GPS, -1 to disable)
extern int gpsPpsPin;                       // GPS PPS pin for precise timing (-1 to disable)

// Default GPS Pin Settings
const int DEFAULT_GPS_TX_PIN = 14;          // Default: GPIO14
const int DEFAULT_GPS_RX_PIN = -1;          // Default: disabled (not needed for receive-only)
const int DEFAULT_GPS_PPS_PIN = -1;         // Default: disabled (not yet wired)

// I2C Pins for RTC (DS3231)
const int I2C_SDA_PIN = 8;                  // I2C SDA
const int I2C_SCL_PIN = 9;                  // I2C SCL
const uint8_t DS3231_ADDRESS = 0x68;        // DS3231 I2C address

// Sensor Pins - defined as extern, will be set at runtime based on preferences
extern int LIMIT_SWITCH_OPEN_PIN;           // Limit switch at roof open position
extern int LIMIT_SWITCH_CLOSED_PIN;         // Limit switch at roof closed position

// Default Pin Settings for ESP32-S3
const int DEFAULT_OPEN_SWITCH_PIN = 35;
const int DEFAULT_CLOSED_SWITCH_PIN = 36;

// Pin States
extern int TRIGGERED;                   // Define whether pin is HIGH or LOW when limit switch is triggered
extern int TELESCOPE_PARKED;            // Define whether pin is HIGH or LOW when telescope is parked
const int DEFAULT_TRIGGER_STATE = LOW;  // Default trigger state
const int DEFAULT_PARK_STATE = LOW;     // Default park sensor state (LOW = normally open switch, pulled low when parked)

// Timing Settings
const uint32_t DEBOUNCE_DELAY = 100;        // Debounce delay in ms
const unsigned long SWITCH_STABLE_TIME = 500; // Time in ms a switch must be stable
extern unsigned long movementTimeout;        // Roof movement timeout in ms (configurable)
const unsigned long DEFAULT_MOVEMENT_TIMEOUT = 90000; // Default: 90 seconds
extern bool movementTimeoutEnabled;          // Enable/disable movement timeout monitoring (configurable)
const bool DEFAULT_TIMEOUT_ENABLED = true;   // Default: enabled
extern unsigned long limitSwitchTimeout;     // Time to wait for limit switch state change after movement starts (configurable)
const unsigned long DEFAULT_LIMIT_SWITCH_TIMEOUT = 5000; // Default: 5 seconds
extern bool limitSwitchTimeoutEnabled;       // Enable/disable limit switch timeout monitoring (configurable)
const bool DEFAULT_LIMIT_SWITCH_TIMEOUT_ENABLED = true; // Default: enabled

// Inverter Timing Settings (NEW in v3)
extern unsigned long inverterDelay1;         // Delay between K1 relay and K3 soft-power button (ms)
extern unsigned long inverterDelay2;         // Delay between inverter power-on and K2 roof button (ms)
const unsigned long DEFAULT_INVERTER_DELAY1 = 750;   // Default: 750ms
const unsigned long DEFAULT_INVERTER_DELAY2 = 1500;  // Default: 1500ms

// Safety Settings
extern bool bypassParkSensor;           // Software bypass state for telescope park sensors

// GPS and RTC Settings
extern bool gpsEnabled;                 // Enable/disable GPS module
extern bool gpsNtpEnabled;              // Enable/disable NTP server functionality
extern bool rtcPresent;                 // RTC hardware detected
extern bool timeSynced;                 // Time has been synced (from GPS or RTC)
extern int16_t timezoneOffset;          // Timezone offset in minutes from UTC
extern bool dstEnabled;                 // Daylight Saving Time enabled

// UDP Park Sensor Settings
#define PREF_PARK_SENSOR_TYPE "parkSensorType"
#define PREF_UDP_PARK_ENABLED "udpParkEnabled"

// Default WiFi credentials (will be overridden by stored settings if available)
#define DEFAULT_WIFI_SSID "YOUR_SSID"
#define DEFAULT_WIFI_PASSWORD "SU93R53CR3tP@55w0Rd"

// AP Mode SSID and password
#define AP_SSID "RoofController"
#define AP_PASSWORD "RoofController"
#define AP_TIMEOUT 300000               // 5 minutes in milliseconds

// Default MQTT Configuration
#define PREF_MQTT_ENABLED "mqttEnabled"
#define DEFAULT_MQTT_SERVER "192.168.2.235"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER "mqtt_username"
#define DEFAULT_MQTT_PASSWORD "MQTT_P@55w0rd"
#define DEFAULT_MQTT_CLIENT_ID "RollOffRoof"
#define DEFAULT_MQTT_TOPIC_PREFIX "observatory/roof"
#define DEFAULT_MQTT_KEEPALIVE 60       // 60 seconds - broker waits ~1.5x before triggering LWT
#define MQTT_PUBLISH_INTERVAL 30000     // 30 seconds

// ASCOM Alpaca Configuration
const int ALPACA_PORT = 11111;
const int WEB_UI_PORT = 80;
const int ALPACA_DISCOVERY_PORT = 32227;
inline const char* ALPACA_DISCOVERY_MESSAGE = "alpacadiscovery1";

// Buffer sizes
#define SSID_SIZE 32
#define PASSWORD_SIZE 64
#define MQTT_SERVER_SIZE 64
#define MQTT_USER_SIZE 32
#define MQTT_PASSWORD_SIZE 64
#define MQTT_CLIENTID_SIZE 32
#define MQTT_TOPIC_SIZE 64

// Preferences namespace and keys
#define PREFERENCES_NAMESPACE "roofConfig"
#define PREF_TRIGGER_STATE "triggerState"
#define PREF_PARK_STATE "parkState"
#define PREF_SWAP_SWITCHES "swapSwitches"
#define PREF_BYPASS_SENSOR "bypassParkSensor"
#define PREF_MOVEMENT_TIMEOUT "moveTimeout"
#define PREF_TIMEOUT_ENABLED "timeoutEnabled"
#define PREF_LIMIT_SWITCH_TIMEOUT "limitSwitchTimeout"
#define PREF_LIMIT_SWITCH_TIMEOUT_ENABLED "limitSwitchTimeoutEn"
#define PREF_WIFI_SSID "ssid"
#define PREF_WIFI_PASSWORD "wifiPassword"
#define PREF_MQTT_SERVER "mqttServer"
#define PREF_MQTT_PORT "mqttPort"
#define PREF_MQTT_USER "mqttUser"
#define PREF_MQTT_PASSWORD "mqttPassword"
#define PREF_MQTT_CLIENT_ID "mqttClientId"
#define PREF_MQTT_TOPIC_PREFIX "mqttTopicPrefix"
#define PREF_MQTT_KEEPALIVE "mqttKeepalive"
#define PREF_INVERTER_RELAY_ENABLED "inverterRelayEn"
#define PREF_INVERTER_SOFTPWR_ENABLED "inverterSoftEn"
#define PREF_INVERTER_DELAY1 "inverterDelay1"
#define PREF_INVERTER_DELAY2 "inverterDelay2"

// GPS and RTC Configuration
#define PREF_GPS_ENABLED "gpsEnabled"
#define PREF_GPS_NTP_ENABLED "gpsNtpEnabled"
#define PREF_GPS_TX_PIN "gpsTxPin"
#define PREF_GPS_RX_PIN "gpsRxPin"
#define PREF_GPS_PPS_PIN "gpsPpsPin"
#define PREF_TIMEZONE_OFFSET "tzOffset"
#define PREF_DST_ENABLED "dstEnabled"
const int NTP_PORT = 123;                   // Standard NTP port
const int16_t DEFAULT_TIMEZONE_OFFSET = 0;  // Default: UTC (0 minutes)
const bool DEFAULT_DST_ENABLED = false;     // Default: DST disabled
// NTP server runs independently once time is synced from GPS or RTC
// TODO: Add PPS (Pulse Per Second) sync support for sub-second GPS accuracy

// Enum for roof status - matches ASCOM ShutterStatus values
enum RoofStatus {
  ROOF_OPEN = 0,       // ShutterStatus.shutterOpen
  ROOF_CLOSED = 1,     // ShutterStatus.shutterClosed
  ROOF_OPENING = 2,    // ShutterStatus.shutterOpening
  ROOF_CLOSING = 3,    // ShutterStatus.shutterClosing
  ROOF_ERROR = 4       // ShutterStatus.shutterError
};

#endif // CONFIG_H