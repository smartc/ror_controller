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
  
  preferences.end();
  
  Debug.println("Configuration loaded from preferences");
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
  preferences.putString(PREF_MQTT_TOPIC_PREFIX, mqttTopicPrefix);
  
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
      webUiServer.hasArg("mqttTopicPrefix")) {
    
    String newMqttServer = webUiServer.arg("mqttServer");
    int newMqttPort = webUiServer.arg("mqttPort").toInt();
    String newMqttUser = webUiServer.arg("mqttUser");
    String newMqttPassword = webUiServer.arg("mqttPassword");
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