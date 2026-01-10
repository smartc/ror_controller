/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * Web UI Handler
 */

#ifndef WEB_UI_HANDLER_H
#define WEB_UI_HANDLER_H

#include <WebServer.h>
#include <Preferences.h>
#include "config.h"
#include "park_sensor_udp.h"
#include <ElegantOTA.h>

// External references
extern WebServer webUiServer;
extern char ssid[SSID_SIZE];
extern char password[PASSWORD_SIZE];
extern char mqttServer[MQTT_SERVER_SIZE];
extern int mqttPort;
extern char mqttUser[MQTT_USER_SIZE];
extern char mqttPassword[MQTT_PASSWORD_SIZE];
extern char mqttTopicPrefix[MQTT_TOPIC_SIZE];
extern RoofStatus roofStatus;
extern bool slaved;
extern bool swapLimitSwitches;
extern bool apMode;
extern bool mqttEnabled;
extern bool bypassParkSensor;  // Bypass park sensor variable

// Function prototypes
void loadConfiguration();
void saveConfiguration();
void initWebUI();
void handleWebUI();
void handleRoot();
void handleControl();  // Roof control page
void handleSetup();
void handleSetupPost();
void handleForceDiscovery();
void handleSetPins();  // Handler for pin settings
void handleWifiConfig();
void handleWifiConfigPost();
void handleBypassToggle();  // Handler for bypass toggle

// Park sensor handlers
void handleParkSensorEnabled();
void handleParkSensorBypass();
void handleParkSensorRemove();
void handleParkSensorRemoveAll();
void handleParkSensorType();

// Inverter control handlers (NEW in v3)
void handleInverterToggle();         // Toggle K1 inverter power relay
void handleInverterButton();         // Send K3 soft-power button press
void handleInverterStatus();         // Get inverter power states (JSON)

// Roof control handlers
void handleRoofControl();            // Handle roof open/close/stop commands
void handleRoofButton();             // Handle single roof button press (mimics physical button)

// API endpoint for real-time status updates
void handleApiStatus();              // Return JSON status for AJAX polling

#endif // WEB_UI_HANDLER_H