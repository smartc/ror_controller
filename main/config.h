/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Configuration Header File
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>  // Include Arduino.h to get LOW/HIGH and uint32_t definitions

// Debug level setting
#define DEBUG_LEVEL 1  // 0=Off, 1=Basic, 2=Verbose

// Version Information
#define DEVICE_VERSION "2.1.0"  // Updated version number
#define DEVICE_MANUFACTURER "Corey Smart"
#define DEVICE_NAME "ESP32 Roll-Off Roof Controller"

// GPIO pin definitions - these are default values, actual values may be adjusted in runtime settings
// Control Pins
const int INVERTER_PIN = 2;             // Inverter relay
const int ROOF_CONTROL_PIN = 4;         // Opener push button 
const int TELESCOPE_PARKED_PIN = 12;    // Safety interlock on telescope park position

// Sensor Pins - defined as extern, will be set at runtime based on preferences
extern int LIMIT_SWITCH_OPEN_PIN;       // Default: Pin 34 - Limit switch at roof open position
extern int LIMIT_SWITCH_CLOSED_PIN;     // Default: Pin 35 - Limit switch at roof closed position

// Default Pin Settings
const int DEFAULT_OPEN_SWITCH_PIN = 35;
const int DEFAULT_CLOSED_SWITCH_PIN = 34;

// Pin States
extern int TRIGGERED;                   // Define whether pin is HIGH or LOW when limit switch is triggered
extern int TELESCOPE_PARKED;            // Define whether pin is HIGH or LOW when telescope is parked
const int DEFAULT_TRIGGER_STATE = LOW;  // Default trigger state
const int DEFAULT_PARK_STATE = HIGH;    // Default park sensor state

// Timing Settings
const uint32_t DEBOUNCE_DELAY = 100;        // Debounce delay in ms
const unsigned long SWITCH_STABLE_TIME = 500; // Time in ms a switch must be stable
const unsigned long MOVEMENT_TIMEOUT = 90000; // 90 seconds for roof movement timeout

// Safety Settings
extern bool bypassParkSensor;           // Software bypass state for telescope park sensors

// UDP Park Sensor Settings
#define PREF_PARK_SENSOR_TYPE "parkSensorType"
#define PREF_UDP_PARK_ENABLED "udpParkEnabled"

// Default WiFi credentials (will be overridden by stored settings if available)
#define DEFAULT_WIFI_SSID "IsolationWard"
#define DEFAULT_WIFI_PASSWORD "hunterelephant"

// AP Mode SSID and password
#define AP_SSID "RoofController"
#define AP_PASSWORD "RoofController"
#define AP_TIMEOUT 300000               // 5 minutes in milliseconds

// Default MQTT Configuration
#define PREF_MQTT_ENABLED "mqttEnabled"
#define DEFAULT_MQTT_SERVER "192.168.2.235"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER "corey"
#define DEFAULT_MQTT_PASSWORD "MQTT2020"
#define DEFAULT_MQTT_CLIENT_ID "RollOffRoof"
#define DEFAULT_MQTT_TOPIC_PREFIX "observatory/roof"
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
#define PREF_WIFI_SSID "ssid"
#define PREF_WIFI_PASSWORD "wifiPassword"
#define PREF_MQTT_SERVER "mqttServer"
#define PREF_MQTT_PORT "mqttPort"
#define PREF_MQTT_USER "mqttUser"
#define PREF_MQTT_PASSWORD "mqttPassword"
#define PREF_MQTT_TOPIC_PREFIX "mqttTopicPrefix"

// Enum for roof status - matches ASCOM ShutterStatus values
enum RoofStatus {
  ROOF_OPEN = 0,       // ShutterStatus.shutterOpen
  ROOF_CLOSED = 1,     // ShutterStatus.shutterClosed
  ROOF_OPENING = 2,    // ShutterStatus.shutterOpening
  ROOF_CLOSING = 3,    // ShutterStatus.shutterClosing
  ROOF_ERROR = 4       // ShutterStatus.shutterError
};

#endif // CONFIG_H