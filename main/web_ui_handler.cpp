/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * Web UI Handler Implementation
 */

#include "web_ui_handler.h"
#include "html_templates.h"
#include "mqtt_handler.h"
#include "roof_controller.h"
#include "park_sensor_udp.h"
#include "Debug.h"
#include <HTTPClient.h>

// Standard Web Server
WebServer webUiServer(WEB_UI_PORT);

// Preferences instance
Preferences preferences;

// Load configuration from preferences
void loadConfiguration() {
  preferences.begin(PREFERENCES_NAMESPACE, false); // Open preferences in read-write mode
  
  // Load WiFi settings if they exist
  if (preferences.isKey(PREF_WIFI_SSID)) {
    String savedSSID = preferences.getString(PREF_WIFI_SSID, "");
    String savedPassword = preferences.getString(PREF_WIFI_PASSWORD, "");
    
    if (savedSSID.length() > 0) {
      strncpy(ssid, savedSSID.c_str(), sizeof(ssid) - 1);
      ssid[sizeof(ssid) - 1] = '\0'; // Ensure null-termination
    }
    
    if (savedPassword.length() > 0) {
      strncpy(password, savedPassword.c_str(), sizeof(password) - 1);
      password[sizeof(password) - 1] = '\0';
    }
  }
  
  // Load MQTT settings if they exist
  if (preferences.isKey(PREF_MQTT_SERVER)) {
    String savedMqttServer = preferences.getString(PREF_MQTT_SERVER, "");
    int savedMqttPort = preferences.getInt(PREF_MQTT_PORT, DEFAULT_MQTT_PORT);
    String savedMqttUser = preferences.getString(PREF_MQTT_USER, "");
    String savedMqttPassword = preferences.getString(PREF_MQTT_PASSWORD, "");
    String savedMqttClientId = preferences.getString(PREF_MQTT_CLIENT_ID, "");
    String savedMqttTopicPrefix = preferences.getString(PREF_MQTT_TOPIC_PREFIX, "");

    if (savedMqttServer.length() > 0) {
      strncpy(mqttServer, savedMqttServer.c_str(), sizeof(mqttServer) - 1);
      mqttServer[sizeof(mqttServer) - 1] = '\0';
    }

    mqttPort = savedMqttPort;

    if (savedMqttUser.length() > 0) {
      strncpy(mqttUser, savedMqttUser.c_str(), sizeof(mqttUser) - 1);
      mqttUser[sizeof(mqttUser) - 1] = '\0';
    }

    if (savedMqttPassword.length() > 0) {
      strncpy(mqttPassword, savedMqttPassword.c_str(), sizeof(mqttPassword) - 1);
      mqttPassword[sizeof(mqttPassword) - 1] = '\0';
    }

    if (savedMqttClientId.length() > 0) {
      strncpy(mqttClientId, savedMqttClientId.c_str(), sizeof(mqttClientId) - 1);
      mqttClientId[sizeof(mqttClientId) - 1] = '\0';
    }

    if (savedMqttTopicPrefix.length() > 0) {
      strncpy(mqttTopicPrefix, savedMqttTopicPrefix.c_str(), sizeof(mqttTopicPrefix) - 1);
      mqttTopicPrefix[sizeof(mqttTopicPrefix) - 1] = '\0';
    }
  }
  
  // Load pin/switch configuration settings
  if (preferences.isKey(PREF_TRIGGER_STATE)) {
    TRIGGERED = preferences.getInt(PREF_TRIGGER_STATE, DEFAULT_TRIGGER_STATE);
  }
  
  // Load telescope park state
  if (preferences.isKey(PREF_PARK_STATE)) {
    TELESCOPE_PARKED = preferences.getInt(PREF_PARK_STATE, DEFAULT_PARK_STATE);
  }
  
  if (preferences.isKey(PREF_SWAP_SWITCHES)) {
    swapLimitSwitches = preferences.getBool(PREF_SWAP_SWITCHES, false);
    
    // Apply the switch swap if needed
    if (swapLimitSwitches) {
      // Swap the pin assignments
      LIMIT_SWITCH_OPEN_PIN = DEFAULT_CLOSED_SWITCH_PIN;
      LIMIT_SWITCH_CLOSED_PIN = DEFAULT_OPEN_SWITCH_PIN;
    } else {
      // Use default pin assignments
      LIMIT_SWITCH_OPEN_PIN = DEFAULT_OPEN_SWITCH_PIN;
      LIMIT_SWITCH_CLOSED_PIN = DEFAULT_CLOSED_SWITCH_PIN;
    }
  }

  // Load MQTT enabled setting
  if (preferences.isKey(PREF_MQTT_ENABLED)) {
    mqttEnabled = preferences.getBool(PREF_MQTT_ENABLED, true);  // Default to true if not found
  }

  // Load park sensor bypass setting
  if (preferences.isKey(PREF_BYPASS_SENSOR)) {
    bypassParkSensor = preferences.getBool(PREF_BYPASS_SENSOR, false);
  }

  // Load movement timeout setting
  if (preferences.isKey(PREF_MOVEMENT_TIMEOUT)) {
    movementTimeout = preferences.getULong(PREF_MOVEMENT_TIMEOUT, DEFAULT_MOVEMENT_TIMEOUT);
  }

  // Load movement timeout enabled setting
  if (preferences.isKey(PREF_TIMEOUT_ENABLED)) {
    movementTimeoutEnabled = preferences.getBool(PREF_TIMEOUT_ENABLED, DEFAULT_TIMEOUT_ENABLED);
  }

  // Load limit switch timeout setting
  if (preferences.isKey(PREF_LIMIT_SWITCH_TIMEOUT)) {
    limitSwitchTimeout = preferences.getULong(PREF_LIMIT_SWITCH_TIMEOUT, DEFAULT_LIMIT_SWITCH_TIMEOUT);
  }

  // Load limit switch timeout enabled setting
  if (preferences.isKey(PREF_LIMIT_SWITCH_TIMEOUT_ENABLED)) {
    limitSwitchTimeoutEnabled = preferences.getBool(PREF_LIMIT_SWITCH_TIMEOUT_ENABLED, DEFAULT_LIMIT_SWITCH_TIMEOUT_ENABLED);
  }

  // Load inverter relay enabled setting
  if (preferences.isKey(PREF_INVERTER_RELAY_ENABLED)) {
    inverterRelayEnabled = preferences.getBool(PREF_INVERTER_RELAY_ENABLED, true);  // Default to true
  }

  // Load inverter soft-power enabled setting
  if (preferences.isKey(PREF_INVERTER_SOFTPWR_ENABLED)) {
    inverterSoftPwrEnabled = preferences.getBool(PREF_INVERTER_SOFTPWR_ENABLED, true);  // Default to true
  }

  // Load inverter delay settings
  if (preferences.isKey(PREF_INVERTER_DELAY1)) {
    inverterDelay1 = preferences.getULong(PREF_INVERTER_DELAY1, DEFAULT_INVERTER_DELAY1);
  }
  if (preferences.isKey(PREF_INVERTER_DELAY2)) {
    inverterDelay2 = preferences.getULong(PREF_INVERTER_DELAY2, DEFAULT_INVERTER_DELAY2);
  }

  preferences.end();

  Debug.println("Configuration loaded from preferences");
  Debug.printf("Movement timeout: %lu ms (%lu seconds)\n", movementTimeout, movementTimeout / 1000);
  Debug.printf("Inverter Delay 1: %lu ms, Delay 2: %lu ms\n", inverterDelay1, inverterDelay2);
}

// Save configuration to preferences
void saveConfiguration() {
  preferences.begin(PREFERENCES_NAMESPACE, false);
  
  // Save WiFi settings
  preferences.putString(PREF_WIFI_SSID, ssid);
  preferences.putString(PREF_WIFI_PASSWORD, password);
  
  // Save MQTT settings
  preferences.putString(PREF_MQTT_SERVER, mqttServer);
  preferences.putInt(PREF_MQTT_PORT, mqttPort);
  preferences.putString(PREF_MQTT_USER, mqttUser);
  preferences.putString(PREF_MQTT_PASSWORD, mqttPassword);
  preferences.putString(PREF_MQTT_CLIENT_ID, mqttClientId);
  preferences.putString(PREF_MQTT_TOPIC_PREFIX, mqttTopicPrefix);

  // Save movement timeout
  preferences.putULong(PREF_MOVEMENT_TIMEOUT, movementTimeout);

  // Save movement timeout enabled setting
  preferences.putBool(PREF_TIMEOUT_ENABLED, movementTimeoutEnabled);

  // Save limit switch timeout
  preferences.putULong(PREF_LIMIT_SWITCH_TIMEOUT, limitSwitchTimeout);

  // Save limit switch timeout enabled setting
  preferences.putBool(PREF_LIMIT_SWITCH_TIMEOUT_ENABLED, limitSwitchTimeoutEnabled);

  // Save inverter relay enabled setting
  preferences.putBool(PREF_INVERTER_RELAY_ENABLED, inverterRelayEnabled);

  // Save inverter soft-power enabled setting
  preferences.putBool(PREF_INVERTER_SOFTPWR_ENABLED, inverterSoftPwrEnabled);

  // Save inverter delay settings
  preferences.putULong(PREF_INVERTER_DELAY1, inverterDelay1);
  preferences.putULong(PREF_INVERTER_DELAY2, inverterDelay2);

  preferences.end();

  Debug.println("Configuration saved to preferences");
}

// Handle pin settings
void handleSetPins() {
  bool settingsChanged = false;
  String message = "";
  
  // Check for trigger state parameter
  if (webUiServer.hasArg("triggerState")) {
    int newTriggerState = webUiServer.arg("triggerState").equals("high") ? HIGH : LOW;
    if (newTriggerState != TRIGGERED) {
      TRIGGERED = newTriggerState;
      
      // Open preferences for writing
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putInt(PREF_TRIGGER_STATE, TRIGGERED);
      preferences.end();
      
      settingsChanged = true;
      message += "Trigger state changed to " + String(TRIGGERED == HIGH ? "HIGH" : "LOW") + ". ";
      Debug.printf("Trigger state changed to %s\n", TRIGGERED == HIGH ? "HIGH" : "LOW");
    }
  }
  
  // Check for swap switches parameter
  if (webUiServer.hasArg("swapSwitches")) {
    bool newSwapState = webUiServer.arg("swapSwitches").equals("true");
    if (newSwapState != swapLimitSwitches) {
      swapLimitSwitches = newSwapState;
      
      // Update pin assignments based on the swap setting
      if (swapLimitSwitches) {
        LIMIT_SWITCH_OPEN_PIN = DEFAULT_CLOSED_SWITCH_PIN;
        LIMIT_SWITCH_CLOSED_PIN = DEFAULT_OPEN_SWITCH_PIN;
      } else {
        LIMIT_SWITCH_OPEN_PIN = DEFAULT_OPEN_SWITCH_PIN;
        LIMIT_SWITCH_CLOSED_PIN = DEFAULT_CLOSED_SWITCH_PIN;
      }
      
      // Save the setting
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putBool(PREF_SWAP_SWITCHES, swapLimitSwitches);
      preferences.end();
      
      settingsChanged = true;
      message += "Switch pins " + String(swapLimitSwitches ? "swapped" : "restored to defaults") + ". ";
      Debug.printf("Switch pins %s\n", swapLimitSwitches ? "swapped" : "restored to defaults");
    }
  }

  // Check for park switch type parameter (normally open vs normally closed)
  if (webUiServer.hasArg("parkSwitchType")) {
    int newParkState = webUiServer.arg("parkSwitchType").equals("high") ? HIGH : LOW;
    if (newParkState != TELESCOPE_PARKED) {
      TELESCOPE_PARKED = newParkState;

      // Open preferences for writing
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putInt(PREF_PARK_STATE, TELESCOPE_PARKED);
      preferences.end();

      settingsChanged = true;
      message += "Park switch type changed to " + String(TELESCOPE_PARKED == HIGH ? "Normally Closed" : "Normally Open") + ". ";
      Debug.printf("Park switch type changed to %s (pin %s when parked)\n",
                   TELESCOPE_PARKED == HIGH ? "Normally Closed" : "Normally Open",
                   TELESCOPE_PARKED == HIGH ? "HIGH" : "LOW");
    }
  }

  // Check for MQTT enabled parameter
  if (webUiServer.hasArg("mqttEnabled")) {
    bool newMqttEnabled = webUiServer.arg("mqttEnabled").equals("true");
    if (newMqttEnabled != mqttEnabled) {
      mqttEnabled = newMqttEnabled;
      
      // Save the setting
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putBool(PREF_MQTT_ENABLED, mqttEnabled);
      preferences.end();
      
      settingsChanged = true;
      message += "MQTT " + String(mqttEnabled ? "enabled" : "disabled") + ". ";
      Debug.printf("MQTT %s\n", mqttEnabled ? "enabled" : "disabled");
      
      // If MQTT was disabled, disconnect
      if (!mqttEnabled && mqttClient.connected()) {
        mqttClient.disconnect();
        message += "MQTT disconnected. ";
        Debug.println("MQTT disconnected");
      }
      // If MQTT was enabled, try to connect
      else if (mqttEnabled && !mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
        reconnectMQTT();
        message += "MQTT connection attempted. ";
        Debug.println("MQTT connection attempted");
      }
    }
  }

  // Check for timeout parameter
  if (webUiServer.hasArg("timeout")) {
    unsigned long newTimeout = webUiServer.arg("timeout").toInt() * 1000; // Convert seconds to ms
    if (newTimeout >= 10000 && newTimeout <= 600000) { // 10 seconds to 10 minutes
      if (newTimeout != movementTimeout) {
        movementTimeout = newTimeout;

        // Save the setting
        preferences.begin(PREFERENCES_NAMESPACE, false);
        preferences.putULong(PREF_MOVEMENT_TIMEOUT, movementTimeout);
        preferences.end();

        settingsChanged = true;
        message += "Movement timeout set to " + String(movementTimeout / 1000) + " seconds. ";
        Debug.printf("Movement timeout set to %lu seconds\n", movementTimeout / 1000);
      }
    } else {
      message += "Invalid timeout value (must be 10-600 seconds). ";
      Debug.println("Invalid timeout value received");
    }
  }

  // Check for timeout enabled parameter
  if (webUiServer.hasArg("timeoutEnabled")) {
    bool newTimeoutEnabled = webUiServer.arg("timeoutEnabled").equals("true");
    if (newTimeoutEnabled != movementTimeoutEnabled) {
      movementTimeoutEnabled = newTimeoutEnabled;

      // Save the setting
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putBool(PREF_TIMEOUT_ENABLED, movementTimeoutEnabled);
      preferences.end();

      settingsChanged = true;
      message += "Movement timeout monitoring " + String(movementTimeoutEnabled ? "enabled" : "disabled") + ". ";
      Debug.printf("Movement timeout monitoring %s\n", movementTimeoutEnabled ? "enabled" : "disabled");
    }
  }

  // Check for limit switch timeout parameter
  if (webUiServer.hasArg("limitSwitchTimeout")) {
    unsigned long newLimitSwitchTimeout = webUiServer.arg("limitSwitchTimeout").toInt() * 1000; // Convert seconds to ms
    if (newLimitSwitchTimeout >= 1000 && newLimitSwitchTimeout <= 30000) { // 1 to 30 seconds
      if (newLimitSwitchTimeout != limitSwitchTimeout) {
        limitSwitchTimeout = newLimitSwitchTimeout;

        // Save the setting
        preferences.begin(PREFERENCES_NAMESPACE, false);
        preferences.putULong(PREF_LIMIT_SWITCH_TIMEOUT, limitSwitchTimeout);
        preferences.end();

        settingsChanged = true;
        message += "Limit switch timeout set to " + String(limitSwitchTimeout / 1000) + " seconds. ";
        Debug.printf("Limit switch timeout set to %lu seconds\n", limitSwitchTimeout / 1000);
      }
    } else {
      message += "Invalid limit switch timeout value (must be 1-30 seconds). ";
      Debug.println("Invalid limit switch timeout value received");
    }
  }

  // Check for limit switch timeout enabled parameter
  if (webUiServer.hasArg("limitSwitchTimeoutEnabled")) {
    bool newLimitSwitchTimeoutEnabled = webUiServer.arg("limitSwitchTimeoutEnabled").equals("true");
    if (newLimitSwitchTimeoutEnabled != limitSwitchTimeoutEnabled) {
      limitSwitchTimeoutEnabled = newLimitSwitchTimeoutEnabled;

      // Save the setting
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putBool(PREF_LIMIT_SWITCH_TIMEOUT_ENABLED, limitSwitchTimeoutEnabled);
      preferences.end();

      settingsChanged = true;
      message += "Limit switch timeout monitoring " + String(limitSwitchTimeoutEnabled ? "enabled" : "disabled") + ". ";
      Debug.printf("Limit switch timeout monitoring %s\n", limitSwitchTimeoutEnabled ? "enabled" : "disabled");
    }
  }

  // Check for inverter relay enabled parameter
  if (webUiServer.hasArg("inverterRelay")) {
    bool newInverterRelayEnabled = webUiServer.arg("inverterRelay").equals("true");
    if (newInverterRelayEnabled != inverterRelayEnabled) {
      inverterRelayEnabled = newInverterRelayEnabled;

      // Save the setting
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putBool(PREF_INVERTER_RELAY_ENABLED, inverterRelayEnabled);
      preferences.end();

      settingsChanged = true;
      message += "Inverter relay control " + String(inverterRelayEnabled ? "enabled" : "disabled") + ". ";
      Debug.printf("Inverter relay control %s\n", inverterRelayEnabled ? "enabled" : "disabled");
    }
  }

  // Check for inverter soft-power enabled parameter
  if (webUiServer.hasArg("inverterSoftPwr")) {
    bool newInverterSoftPwrEnabled = webUiServer.arg("inverterSoftPwr").equals("true");
    if (newInverterSoftPwrEnabled != inverterSoftPwrEnabled) {
      inverterSoftPwrEnabled = newInverterSoftPwrEnabled;

      // Save the setting
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putBool(PREF_INVERTER_SOFTPWR_ENABLED, inverterSoftPwrEnabled);
      preferences.end();

      settingsChanged = true;
      message += "Inverter soft-power button " + String(inverterSoftPwrEnabled ? "enabled" : "disabled") + ". ";
      Debug.printf("Inverter soft-power button %s\n", inverterSoftPwrEnabled ? "enabled" : "disabled");
    }
  }

  // Check for inverter delay 1 parameter
  if (webUiServer.hasArg("delay1")) {
    unsigned long newDelay1 = webUiServer.arg("delay1").toInt(); // In milliseconds
    if (newDelay1 >= 0 && newDelay1 <= 10000) { // 0 to 10 seconds
      if (newDelay1 != inverterDelay1) {
        inverterDelay1 = newDelay1;

        // Save the setting
        preferences.begin(PREFERENCES_NAMESPACE, false);
        preferences.putULong(PREF_INVERTER_DELAY1, inverterDelay1);
        preferences.end();

        settingsChanged = true;
        message += "Inverter Delay 1 set to " + String(inverterDelay1) + "ms. ";
        Debug.printf("Inverter Delay 1 set to %lums\n", inverterDelay1);
      }
    } else {
      message += "Invalid Delay 1 value (must be 0-10000 ms). ";
      Debug.println("Invalid Delay 1 value received");
    }
  }

  // Check for inverter delay 2 parameter
  if (webUiServer.hasArg("delay2")) {
    unsigned long newDelay2 = webUiServer.arg("delay2").toInt(); // In milliseconds
    if (newDelay2 >= 0 && newDelay2 <= 10000) { // 0 to 10 seconds
      if (newDelay2 != inverterDelay2) {
        inverterDelay2 = newDelay2;

        // Save the setting
        preferences.begin(PREFERENCES_NAMESPACE, false);
        preferences.putULong(PREF_INVERTER_DELAY2, inverterDelay2);
        preferences.end();

        settingsChanged = true;
        message += "Inverter Delay 2 set to " + String(inverterDelay2) + "ms. ";
        Debug.printf("Inverter Delay 2 set to %lums\n", inverterDelay2);
      }
    } else {
      message += "Invalid Delay 2 value (must be 0-10000 ms). ";
      Debug.println("Invalid Delay 2 value received");
    }
  }

  if (settingsChanged) {
    // Apply new pin settings
    applyPinSettings();
    message += "Settings applied. Restarting may be required for stable operation.";
    Debug.println("Pin settings applied");
  } else {
    message = "No changes were made.";
    Debug.println("No pin setting changes made");
  }
  
  webUiServer.send(200, "text/plain", message);
}

// Initialize Web UI
void initWebUI() {
  // Handle root page
  webUiServer.on("/", HTTP_GET, handleRoot);
  
  // Handle setup page
  webUiServer.on("/setup", HTTP_GET, handleSetup);
  webUiServer.on("/setup", HTTP_POST, handleSetupPost);

  // Handle roof control page
  webUiServer.on("/control", HTTP_GET, handleControl);

  // Force discovery handler
  webUiServer.on("/force_discovery", HTTP_GET, handleForceDiscovery);
  
  // Pin settings handler
  webUiServer.on("/set_pins", HTTP_POST, handleSetPins);
  
  // Restart handler
  webUiServer.on("/restart", HTTP_POST, []() {
    webUiServer.send(200, "text/plain", "Restarting device...");
    Debug.println("Device restart requested via web interface");
    delay(1000);
    ESP.restart();
  });

  // Toggle park sensor bypass
  webUiServer.on("/toggle_bypass", HTTP_POST, handleBypassToggle);
  
  // Park sensor endpoints
  webUiServer.on("/park_sensor_enabled", HTTP_POST, handleParkSensorEnabled);
  webUiServer.on("/park_sensor_bypass", HTTP_POST, handleParkSensorBypass);
  webUiServer.on("/park_sensor_remove", HTTP_POST, handleParkSensorRemove);
  webUiServer.on("/park_sensor_remove_all", HTTP_POST, handleParkSensorRemoveAll);
  webUiServer.on("/park_sensor_type", HTTP_POST, handleParkSensorType);

  // Inverter control endpoints (NEW in v3)
  webUiServer.on("/inverter_toggle", HTTP_POST, handleInverterToggle);
  webUiServer.on("/inverter_button", HTTP_POST, handleInverterButton);
  webUiServer.on("/inverter_status", HTTP_GET, handleInverterStatus);

  // Roof control endpoint
  webUiServer.on("/roof_control", HTTP_POST, handleRoofControl);
  webUiServer.on("/roof_button", HTTP_POST, handleRoofButton);
  webUiServer.on("/roof_openclose", HTTP_POST, handleRoofOpenClose);

  // API endpoint for real-time status
  webUiServer.on("/api/status", HTTP_GET, handleApiStatus);

  // Add WiFi configuration routes
  webUiServer.on("/wificonfig", HTTP_GET, handleWifiConfig);
  webUiServer.on("/wificonfig", HTTP_POST, handleWifiConfigPost);

  // Add ElegantOTA functionality
  ElegantOTA.begin(&webUiServer);

  // Start server
  webUiServer.begin();
  Debug.printf("Web UI server started on port %d\n", WEB_UI_PORT);
}

// Handle Web UI requests in the main loop
void handleWebUI() {
  webUiServer.handleClient();
}

// Handle the root page - shows a simple status page
void handleRoot() {
  String html = getHomePage(roofStatus, apMode);
  webUiServer.send(200, "text/html", html);
}

// Handle setup page
void handleSetup() {
  webUiServer.send(200, "text/html", getSetupPage());
}

// Handle setup form submission
void handleSetupPost() {
  bool settingsChanged = false;
  
  // Process WiFi settings
  if (webUiServer.hasArg("ssid") && webUiServer.hasArg("password")) {
    String newSSID = webUiServer.arg("ssid");
    String newPassword = webUiServer.arg("password");
    
    if (newSSID.length() > 0 && strcmp(newSSID.c_str(), ssid) != 0) {
      strncpy(ssid, newSSID.c_str(), sizeof(ssid) - 1);
      ssid[sizeof(ssid) - 1] = '\0';
      settingsChanged = true;
      Debug.println("WiFi SSID changed");
    }
    
    if (newPassword.length() > 0 && strcmp(newPassword.c_str(), password) != 0) {
      strncpy(password, newPassword.c_str(), sizeof(password) - 1);
      password[sizeof(password) - 1] = '\0';
      settingsChanged = true;
      Debug.println("WiFi password changed");
    }
  }
  
  // Process MQTT settings
  if (webUiServer.hasArg("mqttServer") && webUiServer.hasArg("mqttPort") &&
      webUiServer.hasArg("mqttUser") && webUiServer.hasArg("mqttPassword") &&
      webUiServer.hasArg("mqttClientId") && webUiServer.hasArg("mqttTopicPrefix")) {

    String newMqttServer = webUiServer.arg("mqttServer");
    int newMqttPort = webUiServer.arg("mqttPort").toInt();
    String newMqttUser = webUiServer.arg("mqttUser");
    String newMqttPassword = webUiServer.arg("mqttPassword");
    String newMqttClientId = webUiServer.arg("mqttClientId");
    String newMqttTopicPrefix = webUiServer.arg("mqttTopicPrefix");
    
    if (newMqttServer.length() > 0 && strcmp(newMqttServer.c_str(), mqttServer) != 0) {
      strncpy(mqttServer, newMqttServer.c_str(), sizeof(mqttServer) - 1);
      mqttServer[sizeof(mqttServer) - 1] = '\0';
      settingsChanged = true;
      Debug.println("MQTT server changed");
    }
    
    if (newMqttPort > 0 && newMqttPort != mqttPort) {
      mqttPort = newMqttPort;
      settingsChanged = true;
      Debug.println("MQTT port changed");
    }
    
    if (newMqttUser.length() > 0 && strcmp(newMqttUser.c_str(), mqttUser) != 0) {
      strncpy(mqttUser, newMqttUser.c_str(), sizeof(mqttUser) - 1);
      mqttUser[sizeof(mqttUser) - 1] = '\0';
      settingsChanged = true;
      Debug.println("MQTT username changed");
    }
    
    if (newMqttPassword.length() > 0 && strcmp(newMqttPassword.c_str(), mqttPassword) != 0) {
      strncpy(mqttPassword, newMqttPassword.c_str(), sizeof(mqttPassword) - 1);
      mqttPassword[sizeof(mqttPassword) - 1] = '\0';
      settingsChanged = true;
      Debug.println("MQTT password changed");
    }

    if (newMqttClientId.length() > 0 && strcmp(newMqttClientId.c_str(), mqttClientId) != 0) {
      strncpy(mqttClientId, newMqttClientId.c_str(), sizeof(mqttClientId) - 1);
      mqttClientId[sizeof(mqttClientId) - 1] = '\0';
      settingsChanged = true;
      Debug.println("MQTT client ID changed");
    }

    if (newMqttTopicPrefix.length() > 0 && strcmp(newMqttTopicPrefix.c_str(), mqttTopicPrefix) != 0) {
      strncpy(mqttTopicPrefix, newMqttTopicPrefix.c_str(), sizeof(mqttTopicPrefix) - 1);
      mqttTopicPrefix[sizeof(mqttTopicPrefix) - 1] = '\0';
      settingsChanged = true;
      Debug.println("MQTT topic prefix changed");
    }
  }
  
  // Save settings if changed
  if (settingsChanged) {
    saveConfiguration();
    
    // Reconnect to MQTT with new settings
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    // Re-setup MQTT with new settings
    setupMQTT();
  }
  
  String message = "Settings updated. ";
  if (settingsChanged) {
    message += "Changes will take effect immediately for MQTT. ";
    message += "For WiFi changes, please restart the device.";
  } else {
    message += "No changes detected.";
  }
  message += "<br><a href='/setup'>Back to setup page</a>";
  
  webUiServer.send(200, "text/html", message);
}

// Handle force discovery
void handleForceDiscovery() {
  Debug.println("Forcing Home Assistant discovery");
  forceDiscovery();
  webUiServer.send(200, "text/plain", "Home Assistant discovery messages forced. Check your Home Assistant instance for devices.");
}

// WiFi Configuration page handler
void handleWifiConfig() {
  webUiServer.send(200, "text/html", getWifiConfigPage());
}

// WiFi Configuration form submission handler
void handleWifiConfigPost() {
  if (webUiServer.hasArg("ssid") && webUiServer.hasArg("password")) {
    String newSSID = webUiServer.arg("ssid");
    String newPassword = webUiServer.arg("password");
    
    if (newSSID.length() > 0) {
      strncpy(ssid, newSSID.c_str(), sizeof(ssid) - 1);
      ssid[sizeof(ssid) - 1] = '\0'; // Ensure null-termination
      
      if (newPassword.length() > 0) {
        strncpy(password, newPassword.c_str(), sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
      }
      
      // Save to preferences
      preferences.begin(PREFERENCES_NAMESPACE, false);
      preferences.putString(PREF_WIFI_SSID, ssid);
      preferences.putString(PREF_WIFI_PASSWORD, password);
      preferences.end();
      
      Debug.println("WiFi settings saved via config page");
      
      // Display success message and restart options
      String html = "<!DOCTYPE html><html>";
      html += "<head><title>WiFi Configuration</title>";
      html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
      html += "<style>";
      html += "body { font-family: Arial, sans-serif; margin: 20px; text-align: center; background-color: #f0f8ff; }";
      html += "h1 { color: #2c3e50; }";
      html += ".container { max-width: 500px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
      html += "button { background: #3498db; color: white; border: none; padding: 12px 20px; margin: 10px; border-radius: 4px; cursor: pointer; font-size: 16px; }";
      html += "button:hover { background: #2980b9; }";
      html += ".success { color: green; font-weight: bold; }";
      html += "</style></head>";
      html += "<body>";
      html += "<div class='container'>";
      html += "<h1>WiFi Configuration</h1>";
      html += "<p class='success'>WiFi settings saved successfully!</p>";
      html += "<p>SSID: " + String(ssid) + "</p>";
      html += "<p>To apply the new settings, the device needs to restart.</p>";
      html += "<button onclick='restart()'>Restart Now</button>";
      html += "<button onclick='goBack()'>Back to Home</button>";
      html += "<script>";
      html += "function restart() {";
      html += "  fetch('/restart', { method: 'POST' })";
      html += "    .then(response => {";
      html += "      alert('Device is restarting with new WiFi settings. If the device connects to a different network, you may need to connect to that network to access it again.');";
      html += "    });";
      html += "}";
      html += "function goBack() {";
      html += "  window.location.href = '/';";
      html += "}";
      html += "</script>";
      html += "</div></body></html>";
      
      webUiServer.send(200, "text/html", html);
    } else {
      // Error - SSID is required
      webUiServer.send(400, "text/plain", "SSID is required!");
      Debug.println("WiFi config error: SSID is required");
    }
  } else {
    // Error - missing arguments
    webUiServer.send(400, "text/plain", "Missing required parameters!");
    Debug.println("WiFi config error: Missing required parameters");
  }
}

// Handler for bypass toggle
void handleBypassToggle() {
  if (webUiServer.hasArg("bypass")) {
    bypassParkSensor = (webUiServer.arg("bypass") == "true");
    
    // Save the setting to preferences
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool(PREF_BYPASS_SENSOR, bypassParkSensor);
    preferences.end();
    
    Debug.printf("Park sensor bypass %s\n", bypassParkSensor ? "enabled" : "disabled");
    webUiServer.send(200, "text/plain", "Bypass " + String(bypassParkSensor ? "enabled" : "disabled"));
  } else {
    webUiServer.send(400, "text/plain", "Missing bypass parameter");
    Debug.println("Bypass toggle error: Missing parameter");
  }
}

// Handler for park sensor enabled toggle
void handleParkSensorEnabled() {
  if (webUiServer.hasArg("uuid") && webUiServer.hasArg("enabled")) {
    String uuid = webUiServer.arg("uuid");
    bool enabled = (webUiServer.arg("enabled") == "true");
    
    setParkSensorEnabled(uuid, enabled);
    
    Debug.printf("Park sensor %s %s\n", uuid.c_str(), enabled ? "enabled" : "disabled");
    webUiServer.send(200, "text/plain", "Park sensor " + String(enabled ? "enabled" : "disabled"));
  } else {
    webUiServer.send(400, "text/plain", "Missing required parameters");
    Debug.println("Park sensor enabled error: Missing parameters");
  }
}

// Handler for park sensor bypass toggle
void handleParkSensorBypass() {
  if (webUiServer.hasArg("uuid") && webUiServer.hasArg("bypass")) {
    String uuid = webUiServer.arg("uuid");
    bool bypassed = (webUiServer.arg("bypass") == "true");
    
    setParkSensorBypass(uuid, bypassed);
    
    Debug.printf("Park sensor %s bypass %s\n", uuid.c_str(), bypassed ? "enabled" : "disabled");
    webUiServer.send(200, "text/plain", "Park sensor bypass " + String(bypassed ? "enabled" : "disabled"));
  } else {
    webUiServer.send(400, "text/plain", "Missing required parameters");
    Debug.println("Park sensor bypass error: Missing parameters");
  }
}

// Handler for park sensor removal
void handleParkSensorRemove() {
  if (webUiServer.hasArg("uuid")) {
    String uuid = webUiServer.arg("uuid");
    
    removeParkSensor(uuid);
    
    Debug.printf("Park sensor %s removed\n", uuid.c_str());
    webUiServer.send(200, "text/plain", "Park sensor removed successfully");
  } else {
    webUiServer.send(400, "text/plain", "Missing UUID parameter");
    Debug.println("Park sensor remove error: Missing UUID");
  }
}

// Handler for removing all park sensors
void handleParkSensorRemoveAll() {
  removeAllParkSensors();
  
  Debug.println("All park sensors removed");
  webUiServer.send(200, "text/plain", "All park sensors removed successfully");
}

// Handler for park sensor type change
void handleParkSensorType() {
  if (webUiServer.hasArg("type")) {
    int type = webUiServer.arg("type").toInt();
    
    if (type >= 0 && type <= 2) {
      parkSensorType = static_cast<ParkSensorType>(type);
      
      // Save to preferences
      Preferences prefs;
      prefs.begin(PREFERENCES_NAMESPACE, false);
      prefs.putInt(PREF_PARK_SENSOR_TYPE, type);
      prefs.end();
      
      String typeStr = "";
      switch (parkSensorType) {
        case PARK_SENSOR_PHYSICAL:
          typeStr = "Physical Sensor Only";
          break;
        case PARK_SENSOR_UDP:
          typeStr = "UDP Sensors Only";
          break;
        case PARK_SENSOR_BOTH:
          typeStr = "Both (AND Logic)";
          break;
      }
      
      Debug.printf("Park sensor type changed to: %s\n", typeStr.c_str());
      webUiServer.send(200, "text/plain", "Park sensor type set to: " + typeStr);
    } else {
      webUiServer.send(400, "text/plain", "Invalid park sensor type");
      Debug.println("Park sensor type error: Invalid type");
    }
  } else {
    webUiServer.send(400, "text/plain", "Missing type parameter");
    Debug.println("Park sensor type error: Missing parameter");
  }
}

// ========== NEW INVERTER CONTROL HANDLERS (v3 Hardware) ==========

// Handler for toggling K1 inverter power relay
void handleInverterToggle() {
  toggleInverterPower();

  bool state = getInverterRelayState();
  String stateStr = state ? "ON" : "OFF";

  Debug.printf("Inverter power relay toggled via web interface to: %s\n", stateStr.c_str());
  webUiServer.send(200, "text/plain", "Inverter power relay: " + stateStr);
}

// Handler for sending K3 soft-power button press
void handleInverterButton() {
  sendInverterButtonPress();

  Debug.println("Inverter button press sent via web interface");
  webUiServer.send(200, "text/plain", "Inverter button pressed");
}

// Handler for getting inverter power states
void handleInverterStatus() {
  bool relayState = getInverterRelayState();
  bool acPowerState = getInverterACPowerState();

  // Create JSON response
  String json = "{";
  json += "\"relay_state\":" + String(relayState ? "true" : "false") + ",";
  json += "\"ac_power_state\":" + String(acPowerState ? "true" : "false");
  json += "}";

  webUiServer.send(200, "application/json", json);
}

// Handler for roof control page
void handleControl() {
  String html = getRoofControlPage();
  webUiServer.send(200, "text/html", html);
}

// Handler for roof control commands
void handleRoofControl() {
  if (webUiServer.hasArg("action")) {
    String action = webUiServer.arg("action");

    if (action == "open") {
      bool success = startOpeningRoof();
      if (success) {
        Debug.println("Roof opening command sent via web interface");
        webUiServer.send(200, "text/plain", "Roof opening");
      } else {
        Debug.println("Roof opening command failed - check safety interlocks");
        webUiServer.send(400, "text/plain", "Cannot open roof - check telescope park status");
      }
    } else if (action == "close") {
      bool success = startClosingRoof();
      if (success) {
        Debug.println("Roof closing command sent via web interface");
        webUiServer.send(200, "text/plain", "Roof closing");
      } else {
        Debug.println("Roof closing command failed - check safety interlocks");
        webUiServer.send(400, "text/plain", "Cannot close roof - check telescope park status");
      }
    } else if (action == "stop") {
      stopRoofMovement();
      Debug.println("Roof stop command sent via web interface");
      webUiServer.send(200, "text/plain", "Roof stopped");
    } else {
      webUiServer.send(400, "text/plain", "Invalid action");
      Debug.println("Roof control error: Invalid action");
    }
  } else {
    webUiServer.send(400, "text/plain", "Missing action parameter");
    Debug.println("Roof control error: Missing parameter");
  }
}

// Handle single roof button press (mimics physical button)
void handleRoofButton() {
  Debug.println("Roof button pressed via web interface");

  // Check telescope safety interlock - only if bypass is not enabled
  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("Cannot control roof: Telescope not parked and bypass not enabled");
    webUiServer.send(400, "text/plain", "Cannot control roof - telescope not parked. Enable bypass to override.");
    return;
  }

  // Just send a button press - exactly like the physical button
  // The roof controller hardware will handle the logic
  sendButtonPress();

  Debug.println("Roof button press sent");
  webUiServer.send(200, "text/plain", "Button press sent");
}

// Handle intelligent open/close command (replicates ASCOM/MQTT logic)
void handleRoofOpenClose() {
  Debug.println("Intelligent roof control via web interface");

  // Check telescope safety interlock - only if bypass is not enabled
  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("Cannot control roof: Telescope not parked and bypass not enabled");
    webUiServer.send(400, "text/plain", "Cannot control roof - telescope not parked. Enable bypass to override.");
    return;
  }

  // Determine action based on current roof state
  bool success = false;
  String action = "";

  if (roofStatus == ROOF_CLOSED || roofStatus == ROOF_CLOSING) {
    // Roof is closed or closing, so open it
    action = "Opening";
    success = startOpeningRoof();
  } else if (roofStatus == ROOF_OPEN || roofStatus == ROOF_OPENING) {
    // Roof is open or opening, so close it
    action = "Closing";
    success = startClosingRoof();
  } else {
    // Unknown state - return error
    Debug.println("Cannot determine roof action - unknown state");
    webUiServer.send(400, "text/plain", "Cannot control roof - unknown state");
    return;
  }

  if (success) {
    Debug.printf("Intelligent roof control: %s roof\n", action.c_str());
    webUiServer.send(200, "text/plain", action + " roof");
  } else {
    Debug.printf("Intelligent roof control failed: Cannot %s roof\n", action.c_str());
    webUiServer.send(400, "text/plain", "Cannot " + action + " roof - check safety interlocks");
  }
}

// API endpoint for real-time status updates (returns JSON)
void handleApiStatus() {
  DynamicJsonDocument doc(512);

  // Roof status
  doc["status"] = getRoofStatusString();
  doc["telescope_parked"] = telescopeParked;
  doc["bypass_enabled"] = bypassParkSensor;

  // Limit switch states
  doc["limit_open"] = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED);
  doc["limit_closed"] = (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED);

  // Inverter states
  doc["inverter_relay"] = getInverterRelayState();
  doc["inverter_ac_power"] = getInverterACPowerState();

  // Park sensor type
  doc["park_sensor_type"] = static_cast<int>(parkSensorType);

  String jsonResponse;
  serializeJson(doc, jsonResponse);

  webUiServer.send(200, "application/json", jsonResponse);
}