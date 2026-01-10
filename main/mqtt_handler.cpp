/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * MQTT Handler Implementation - Fixed Discovery Functions
 */

#include "mqtt_handler.h"
#include "roof_controller.h"
#include "park_sensor_udp.h"
#include "Debug.h"
#include <Arduino.h>

// Global MQTT configuration variables
bool mqttEnabled = true; // Default to enabled
char mqttServer[MQTT_SERVER_SIZE] = DEFAULT_MQTT_SERVER;
int mqttPort = DEFAULT_MQTT_PORT;
char mqttUser[MQTT_USER_SIZE] = DEFAULT_MQTT_USER;
char mqttPassword[MQTT_PASSWORD_SIZE] = DEFAULT_MQTT_PASSWORD;
char mqttClientId[MQTT_CLIENTID_SIZE] = DEFAULT_MQTT_CLIENT_ID;
char mqttTopicPrefix[MQTT_TOPIC_SIZE] = DEFAULT_MQTT_TOPIC_PREFIX;

// Derived topic paths - will be constructed in setupMQTT based on prefix
char mqttTopicStatus[MQTT_TOPIC_SIZE];
char mqttTopicCommand[MQTT_TOPIC_SIZE];
char mqttTopicAvailability[MQTT_TOPIC_SIZE];

// Initialize MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Setup MQTT connection
void setupMQTT() {
  // If MQTT is disabled, don't set up
  if (!mqttEnabled) {
    Serial.println("MQTT is disabled, skipping setup");
    return;
  }

  // If mqttClientId is still the default, append chip ID to make it unique
  // This prevents conflicts when multiple controllers are on the same network
  if (String(mqttClientId) == String(DEFAULT_MQTT_CLIENT_ID) ||
      String(mqttClientId).indexOf("_") == -1) {  // No underscore means no chip ID appended yet

    uint64_t chipId = ESP.getEfuseMac();
    String chipIdStr = String((uint32_t)(chipId >> 32), HEX) + String((uint32_t)chipId, HEX);

    String uniqueClientId = String(mqttClientId) + "_" + chipIdStr;
    if (uniqueClientId.length() >= MQTT_CLIENTID_SIZE) {
      int maxOriginalLength = MQTT_CLIENTID_SIZE - chipIdStr.length() - 2;
      String truncatedId = String(mqttClientId).substring(0, maxOriginalLength);
      uniqueClientId = truncatedId + "_" + chipIdStr;
    }

    uniqueClientId.toCharArray(mqttClientId, MQTT_CLIENTID_SIZE);

    Serial.println("Generated unique MQTT client ID");
  }

  Serial.print("Using MQTT client ID: ");
  Serial.println(mqttClientId);

  // Increase MQTT buffer size to handle larger discovery messages
  mqttClient.setBufferSize(2048);  // Increase to 2KB for safety

  // Construct full topic paths based on prefix
  snprintf(mqttTopicStatus, MQTT_TOPIC_SIZE, "%s/status", mqttTopicPrefix);
  snprintf(mqttTopicCommand, MQTT_TOPIC_SIZE, "%s/command", mqttTopicPrefix);
  snprintf(mqttTopicAvailability, MQTT_TOPIC_SIZE, "%s/availability", mqttTopicPrefix);
  
  // Debug print the constructed topics
  Serial.println("MQTT Topics:");
  Serial.printf("  Status: %s\n", mqttTopicStatus);
  Serial.printf("  Command: %s\n", mqttTopicCommand);
  Serial.printf("  Availability: %s\n", mqttTopicAvailability);
  
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  // Try to connect
  reconnectMQTT();
}

// Reconnect to MQTT broker
void reconnectMQTT() {
  // If MQTT is disabled, don't reconnect
  if (!mqttEnabled) {
    return;
  }

  // Only try once per call - don't loop here
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Set LWT (Last Will and Testament) for availability status
    // This will be sent if device unexpectedly disconnects
    if (mqttClient.connect(mqttClientId, mqttUser, mqttPassword, 
                          mqttTopicAvailability, 1, true, "offline")) {
      Serial.println("connected");
      
      // Publish online status with retain flag
      mqttClient.publish(mqttTopicAvailability, "online", true);
      
      // Subscribe to command topic
      mqttClient.subscribe(mqttTopicCommand);
      
      // Publish initial status
      publishStatusToMQTT();
      
      // If we have a valid uniqueID, publish discovery messages
      if (uniqueID.length() > 0) {
        publishDiscovery();
      } else {
        Serial.println("Skipping discovery - no unique ID available yet");
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" will try again later");
    }
  }
}

// Handle MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  payload[length] = '\0';
  String message = String((char*)payload);
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);
  
  // Handle commands
  if (String(topic) == mqttTopicCommand) {
    if (message == "OPEN") {
      startOpeningRoof();
    } else if (message == "CLOSE") {
      startClosingRoof();
    } else if (message == "STOP") {
      stopRoofMovement();
    } else if (message == "DISCOVER") {
      // Special command to force discovery
      forceDiscovery();
    }
  }
}

// Publish roof status to MQTT
void publishStatusToMQTT() {
  // If MQTT is disabled or not connected, don't publish
  if (!mqttEnabled || !mqttClient.connected()) {
    if (!mqttEnabled) {
      Serial.println("MQTT is disabled - not publishing status");
    } else {
      Serial.println("MQTT not connected - cannot publish status");
    }
    return;
  }
  
  // Create JSON document
  DynamicJsonDocument doc(512);
  
  // Ensure the status field is present and correctly formatted
  String statusString = getRoofStatusString();
  doc["status"] = statusString;
  doc["slaved"] = slaved;
  doc["telescope_parked"] = telescopeParked;
  doc["limit_open"] = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED);
  doc["limit_closed"] = (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED);
  doc["bypass_enabled"] = bypassParkSensor;

  // Add device identification information
  doc["device_id"] = uniqueID;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["version"] = DEVICE_VERSION;
  
  // Add configuration information about the switches
  doc["switches_swapped"] = swapLimitSwitches;
  doc["trigger_state"] = (TRIGGERED == HIGH ? "HIGH" : "LOW");
  doc["open_switch_pin"] = LIMIT_SWITCH_OPEN_PIN;
  doc["closed_switch_pin"] = LIMIT_SWITCH_CLOSED_PIN;

  // Add inverter power state information (NEW in v3)
  doc["inverter_relay_state"] = getInverterRelayState();
  doc["inverter_ac_power_state"] = getInverterACPowerState();

  // Add park sensor information
  doc["park_sensor_type"] = static_cast<int>(parkSensorType);
  doc["park_sensor_type_name"] = (parkSensorType == PARK_SENSOR_PHYSICAL ? "Physical" : 
                                  parkSensorType == PARK_SENSOR_UDP ? "UDP" : "Both");
  
  // Add UDP park sensor status if enabled
  if (parkSensorType == PARK_SENSOR_UDP || parkSensorType == PARK_SENSOR_BOTH) {
    std::vector<ParkSensor> activeSensors = getActiveSensors();
    doc["udp_sensors_enabled"] = activeSensors.size();
    
    if (!activeSensors.empty()) {
      JsonArray sensorArray = doc.createNestedArray("udp_sensors");
      for (const auto& sensor : activeSensors) {
        JsonObject sensorObj = sensorArray.createNestedObject();
        sensorObj["name"] = sensor.name;
        sensorObj["uuid"] = sensor.uuid.substring(0, 8); // First 8 chars for brevity
        sensorObj["status"] = getParkSensorStatusString(sensor.status);
        sensorObj["ip_address"] = sensor.ipAddress;
        sensorObj["bypassed"] = sensor.bypassEnabled;
        sensorObj["is_safe_to_move"] = sensor.isSafeToMove;
        
        if (sensor.lastSeen > 0) {
          sensorObj["last_seen_seconds"] = (millis() - sensor.lastSeen) / 1000;
        }
      }
    }
  }
  
  String statusJson;
  serializeJson(doc, statusJson);
  
  // Always print the full status for debugging
  Serial.print("Publishing status: ");
  Serial.println(statusJson);
  
  if (mqttClient.publish(mqttTopicStatus, statusJson.c_str())) {
    Serial.println("Successfully published status to MQTT");
  } else {
    Serial.println("Failed to publish MQTT status!");
  }
}

// Simplified publishDiscovery function to add configuration URL without using helper functions
void publishDiscovery() {
  // If MQTT is disabled or not connected, don't publish discovery
  if (!mqttEnabled || !mqttClient.connected()) {
    if (!mqttEnabled) {
      Debug.println("MQTT is disabled - not publishing discovery");
    } else {
      Debug.println("Cannot publish Home Assistant discovery - MQTT not connected");
    }
    return;
  }
  
  if (uniqueID.length() == 0) {
    Debug.println("Cannot publish discovery without a valid unique ID");
    return;
  }
  
  Debug.println("Publishing Home Assistant discovery messages...");
  
  // Define the base topic for Home Assistant discovery
  String discoveryPrefix = "homeassistant";
  String deviceId = uniqueID;

  // COVER ENTITY
  {
    // Create document for the cover entity
    DynamicJsonDocument doc(1024);
    
    doc["name"] = "Observatory Roof";
    doc["object_id"] = "observatory_roof"; // Add explicit object_id
    doc["unique_id"] = String(deviceId + "_roof_cover"); // Change unique_id
    doc["device_class"] = "garage";
    doc["command_topic"] = mqttTopicCommand;
    doc["state_topic"] = mqttTopicStatus;
    
    // Modified template to handle all states including the error state
    // This is the key change - we'll return "unknown" for error states
    // which will make the cover entity show a question mark icon
    doc["value_template"] = "{% if value_json.status == 'Open' %}open{% elif value_json.status == 'Closed' %}closed{% elif value_json.status == 'Opening' %}opening{% elif value_json.status == 'Closing' %}closing{% elif value_json.status == 'Error' %}unknown{% else %}unknown{% endif %}";
    
    // Add the status text as a separate attribute
    doc["json_attributes_topic"] = mqttTopicStatus;
    // Include error information as attribute
    doc["json_attributes_template"] = "{ \"status_text\": \"{{ value_json.status }}\", \"is_error\": {{ 'true' if value_json.status == 'Error' else 'false' }} }";
    
    // Standard commands
    doc["payload_open"] = "OPEN";
    doc["payload_close"] = "CLOSE";
    doc["payload_stop"] = "STOP";
    
    // Define states with icons
    doc["state_open"] = "open";
    doc["state_closed"] = "closed";
    doc["state_opening"] = "opening";
    doc["state_closing"] = "closing";
    
    // Custom icons for different states
    doc["icon_open"] = "mdi:garage-open";
    doc["icon_closed"] = "mdi:garage";
    doc["icon_opening"] = "mdi:garage-open";  // Same as open but will blink
    doc["icon_closing"] = "mdi:garage";       // Same as closed but will blink
    
    // Enable blinking for transitional states
    JsonObject opening_effect = doc.createNestedObject("opening_effect");
    opening_effect["style"] = "blink";
    opening_effect["duration"] = 1000;
    
    JsonObject closing_effect = doc.createNestedObject("closing_effect");
    closing_effect["style"] = "blink";
    closing_effect["duration"] = 1000;
    
    // Availability
    doc["availability_topic"] = mqttTopicAvailability;
    doc["availability_template"] = "{{ value }}"; // Explicit availability template
    
    // Define device info directly with configuration URL
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    device["name"] = "Observatory Roof Controller";
    device["model"] = "ESP32 Roll-Off Roof Controller";
    device["manufacturer"] = DEVICE_MANUFACTURER;
    device["sw_version"] = DEVICE_VERSION;
    // Add configuration URL to make it clickable in HA
    device["configuration_url"] = "http://" + WiFi.localIP().toString();
    
    // Serialize and publish
    String topicPath = discoveryPrefix + "/cover/" + deviceId + "_roof_cover/config"; // Modified topic path
    String payload;
    serializeJson(doc, payload);
    
    Debug.println("Cover Discovery Message:");
    Debug.println(payload, 2);
    
    if (mqttClient.publish(topicPath.c_str(), payload.c_str(), true)) {
      Debug.println("Cover discovery published successfully");
    } else {
      Debug.println("Failed to publish cover discovery message");
    }
    
    // Brief pause to let the broker process
    delay(100);
  }
  
  // NEW ERROR BINARY SENSOR - Add a dedicated error sensor
  {
    DynamicJsonDocument doc(1024);
    
    doc["name"] = "Observatory Roof Error";
    doc["unique_id"] = String(deviceId + "_roof_error");
    doc["state_topic"] = mqttTopicStatus;
    doc["value_template"] = "{% if value_json.status == 'Error' %}ON{% else %}OFF{% endif %}";
    doc["device_class"] = "problem";  // Use problem device class for error states
    doc["availability_topic"] = mqttTopicAvailability;
    doc["icon"] = "mdi:alert-circle";
    
    // Define device info directly with configuration URL
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    device["name"] = "Observatory Roof Controller";
    device["model"] = "ESP32 Roll-Off Roof Controller";
    device["manufacturer"] = DEVICE_MANUFACTURER;
    device["sw_version"] = DEVICE_VERSION;
    // Add configuration URL to make it clickable in HA
    device["configuration_url"] = "http://" + WiFi.localIP().toString();
    
    // Serialize and publish
    String topicPath = discoveryPrefix + "/binary_sensor/" + deviceId + "_roof_error/config";
    String payload;
    serializeJson(doc, payload);
    
    Debug.println("Roof Error Sensor Discovery Message:");
    Debug.println(payload, 2);
    
    if (mqttClient.publish(topicPath.c_str(), payload.c_str(), true)) {
      Debug.println("Roof error sensor discovery published successfully");
    } else {
      Debug.println("Failed to publish roof error sensor discovery message");
    }
    
    // Brief pause
    delay(100);
  }
  
  // Add a separate sensor for the status text to make it easier to display
  {
    DynamicJsonDocument doc(1024);
    
    doc["name"] = "Observatory Roof Status";
    doc["unique_id"] = String(deviceId + "_roof_status");
    doc["state_topic"] = mqttTopicStatus;
    doc["value_template"] = "{{ value_json.status }}";
    doc["availability_topic"] = mqttTopicAvailability;
    doc["icon"] = "mdi:information-outline";
    
    // Define device info - must match the cover entity
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    device["name"] = "Observatory Roof Controller";
    device["model"] = "ESP32 Roll-Off Roof Controller";
    device["manufacturer"] = DEVICE_MANUFACTURER;
    device["sw_version"] = DEVICE_VERSION;
    // Add configuration URL to make it clickable in HA
    device["configuration_url"] = "http://" + WiFi.localIP().toString();
    
    // Serialize and publish
    String topicPath = discoveryPrefix + "/sensor/" + deviceId + "_roof_status/config";
    String payload;
    serializeJson(doc, payload);
    
    Debug.println("Roof Status Sensor Discovery Message:");
    Debug.println(payload, 2);
    
    if (mqttClient.publish(topicPath.c_str(), payload.c_str(), true)) {
      Debug.println("Roof status sensor discovery published successfully");
    } else {
      Debug.println("Failed to publish roof status sensor discovery message");
    }
    
    // Brief pause
    delay(100);
  }
  
  // Force an immediate status update after discovery
  delay(100);
  publishStatusToMQTT();
  Debug.println("Discovery complete");
}

// Force publishing of discovery messages
void forceDiscovery() {
  Serial.println("Forcing Home Assistant discovery message publication...");
  
  // Ensure we have a valid device ID
  if (uniqueID.length() == 0) {
    // If for some reason uniqueID is empty, regenerate it
    uint8_t mac[6];
    WiFi.macAddress(mac);
    uniqueID = "ESP32_ROF_";
    for (int i = 0; i < 6; i++) {
      char buf[3];
      sprintf(buf, "%02X", mac[i]);
      uniqueID += buf;
    }
    Serial.print("Generated new unique ID: ");
    Serial.println(uniqueID);
  }
  
  // Make sure MQTT is connected
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  
  if (mqttClient.connected()) {
    // Use the new discovery function
    publishDiscovery();
  } else {
    Serial.println("Failed to connect to MQTT for discovery");
  }
}

// Get roof status as string
String getRoofStatusString() {
  return getRoofStatusString(roofStatus);
}

// Overloaded function to get status string for a specific status
String getRoofStatusString(RoofStatus status) {
  switch (status) {
    case ROOF_OPEN:
      return "Open";
    case ROOF_CLOSED:
      return "Closed";
    case ROOF_OPENING:
      return "Opening";
    case ROOF_CLOSING:
      return "Closing";
    case ROOF_ERROR:
      return "Error";
    default:
      return "Unknown";
  }
}