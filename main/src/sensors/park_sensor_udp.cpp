/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * UDP Park Sensor Handler Implementation - FIXED VERSION
 */

#include "park_sensor_udp.h"
#include "Debug.h"
#include "web_ui_handler.h"
#include <Preferences.h>

// Global variables
WiFiUDP parkSensorUdp;
std::map<String, ParkSensor> discoveredSensors;
std::vector<String> enabledSensorUuids;
ParkSensorType parkSensorType = PARK_SENSOR_PHYSICAL;  // Default to physical sensor
bool udpParkSensorSystemEnabled = false;

// Preferences keys for park sensors
#define PREF_PARK_SENSOR_TYPE "parkSensorType"
#define PREF_PARK_SENSOR_COUNT "parkSensorCount"
#define PREF_PARK_SENSOR_PREFIX "parkSensor_"

// Initialize UDP park sensor listener
void initParkSensorUDP() {
  Debug.println("Initializing UDP Park Sensor listener...");
  
  // Load configuration from preferences
  loadParkSensorConfiguration();
  
  // Start UDP listener on the park sensor port
  if (parkSensorUdp.begin(PARK_SENSOR_UDP_PORT)) {
    Debug.printf("UDP Park Sensor listener started on port %d\n", PARK_SENSOR_UDP_PORT);
    udpParkSensorSystemEnabled = true;
  } else {
    Debug.printf("Failed to start UDP Park Sensor listener on port %d\n", PARK_SENSOR_UDP_PORT);
    udpParkSensorSystemEnabled = false;
  }
}

// Handle incoming UDP park sensor messages
void handleParkSensorUDP() {
  if (!udpParkSensorSystemEnabled) {
    return;
  }
  
  int packetSize = parkSensorUdp.parsePacket();
  if (packetSize) {
    char packet[512];
    memset(packet, 0, sizeof(packet));
    
    int len = parkSensorUdp.read(packet, sizeof(packet) - 1);
    if (len > 0) {
      packet[len] = '\0';
      
      Debug.printf(2, "Received park sensor UDP packet (%d bytes) from %s:%d\n", 
                   len, parkSensorUdp.remoteIP().toString().c_str(), parkSensorUdp.remotePort());
      Debug.printf(2, "Content: %s\n", packet);
      
      processParkSensorMessage(String(packet), parkSensorUdp.remoteIP());
    }
  }
  
  // Update sensor status (check for timeouts)
  updateParkSensorStatus();
}

// Process a park sensor message - FIXED VERSION
void processParkSensorMessage(const String& message, IPAddress remoteIP) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Debug.printf("Failed to parse park sensor JSON: %s\n", error.c_str());
    return;
  }
  
  // Extract required fields
  if (!doc.containsKey("serialNumber") || !doc.containsKey("deviceType") || !doc.containsKey("isSafeToMove")) {
    Debug.println("Park sensor message missing required fields");
    return;
  }
  
  String uuid = doc["serialNumber"].as<String>();
  String deviceType = doc["deviceType"].as<String>();
  bool isSafeToMove = doc["isSafeToMove"].as<bool>();
  
  // FIXED: Check if this is a new sensor BEFORE accessing the map
  bool isNewSensor = (discoveredSensors.find(uuid) == discoveredSensors.end());
  
  // Now create or update sensor data
  ParkSensor& sensor = discoveredSensors[uuid];
  
  // Always update these core fields
  sensor.uuid = uuid;
  sensor.deviceType = deviceType;
  sensor.isSafeToMove = isSafeToMove;
  sensor.lastSeen = millis();
  sensor.ipAddress = remoteIP.toString();
  
  // Update optional fields if present
  // if (doc.containsKey("uuid")) {
  //   sensor.serialNumber = doc["uuid"].as<String>();
  // }
  if (doc.containsKey("name")) {
    sensor.name = doc["name"].as<String>();
  }
  if (doc.containsKey("pitch")) {
    sensor.pitch = doc["pitch"].as<float>();
  }
  if (doc.containsKey("roll")) {
    sensor.roll = doc["roll"].as<float>();
  }
  
  // Update status based on isSafeToMove
  sensor.status = isSafeToMove ? SENSOR_PARKED : SENSOR_UNPARKED;
  
  // Only log new sensor discovery, not every update
  if (isNewSensor) {
    Debug.printf("NEW park sensor discovered: %s (%s) at %s - %s\n", 
                 sensor.name.c_str(), uuid.c_str(), remoteIP.toString().c_str(),
                 sensor.serialNumber.c_str());
  } else {
    Debug.printf(2, "Updated existing park sensor: %s - %s\n", 
                 sensor.name.c_str(), isSafeToMove ? "PARKED" : "UNPARKED");
  }
}

// Update park sensor status (check for timeouts)
void updateParkSensorStatus() {
  unsigned long currentTime = millis();
  
  for (auto& pair : discoveredSensors) {
    ParkSensor& sensor = pair.second;
    
    // Check if sensor has timed out
    if (currentTime - sensor.lastSeen > PARK_SENSOR_TIMEOUT) {
      if (sensor.status != SENSOR_OFFLINE && sensor.status != SENSOR_UNKNOWN) {
        Debug.printf("Park sensor %s (%s) has gone offline\n", 
                     sensor.name.c_str(), sensor.uuid.c_str());
        sensor.status = SENSOR_OFFLINE;
      }
    }
  }
}

// Check if telescope is parked based on UDP sensors
bool isTelescopeParkedUDP() {
  // If no enabled sensors, return true (assume parked)
  if (enabledSensorUuids.empty()) {
    return true;
  }
  
  bool allParked = true;
  bool hasActiveSensor = false;
  
  for (const String& uuid : enabledSensorUuids) {
    auto it = discoveredSensors.find(uuid);
    if (it != discoveredSensors.end()) {
      const ParkSensor& sensor = it->second;
      
      // Skip bypassed sensors
      if (sensor.bypassEnabled) {
        Debug.printf(2, "Sensor %s is bypassed\n", sensor.name.c_str());
        continue;
      }
      
      hasActiveSensor = true;
      
      // If sensor is offline or unknown, consider it as not parked for safety
      if (sensor.status == SENSOR_OFFLINE || sensor.status == SENSOR_UNKNOWN) {
        Debug.printf(3, "Sensor %s is offline/unknown - considering as NOT PARKED\n", sensor.name.c_str());
        allParked = false;
        break;
      }
      
      // If sensor reports not safe to move, telescope is not parked
      if (sensor.status == SENSOR_UNPARKED) {
        Debug.printf(2, "Sensor %s reports NOT PARKED\n", sensor.name.c_str());
        allParked = false;
        break;
      }
    }
  }
  
  // If no active sensors (all bypassed), assume parked
  if (!hasActiveSensor) {
    return true;
  }
  
  return allParked;
}

// Add a park sensor to the enabled list
void addParkSensor(const String& uuid) {
  // Check if sensor exists in discovered sensors
  auto it = discoveredSensors.find(uuid);
  if (it == discoveredSensors.end()) {
    Debug.printf("Cannot add park sensor %s - not found in discovered sensors\n", uuid.c_str());
    return;
  }
  
  // Check if already enabled
  auto enabledIt = std::find(enabledSensorUuids.begin(), enabledSensorUuids.end(), uuid);
  if (enabledIt != enabledSensorUuids.end()) {
    Debug.printf("Park sensor %s is already enabled\n", uuid.c_str());
    return;
  }
  
  // Add to enabled list
  enabledSensorUuids.push_back(uuid);
  discoveredSensors[uuid].enabled = true;
  
  Debug.printf("Added park sensor %s (%s) to enabled list\n", 
               discoveredSensors[uuid].name.c_str(), uuid.c_str());
  
  // Save configuration
  saveParkSensorConfiguration();
}

// Remove a park sensor from the enabled list
void removeParkSensor(const String& uuid) {
  auto it = std::find(enabledSensorUuids.begin(), enabledSensorUuids.end(), uuid);
  if (it != enabledSensorUuids.end()) {
    enabledSensorUuids.erase(it);
    
    // Update sensor enabled flag if it still exists
    auto sensorIt = discoveredSensors.find(uuid);
    if (sensorIt != discoveredSensors.end()) {
      sensorIt->second.enabled = false;
    }
    
    Debug.printf("Removed park sensor %s from enabled list\n", uuid.c_str());
    
    // Save configuration
    saveParkSensorConfiguration();
  }
}

// Remove all park sensors
void removeAllParkSensors() {
  enabledSensorUuids.clear();
  
  // Update all sensor enabled flags
  for (auto& pair : discoveredSensors) {
    pair.second.enabled = false;
  }
  
  Debug.println("Removed all park sensors from enabled list");
  
  // Save configuration
  saveParkSensorConfiguration();
}

// Set park sensor bypass status
void setParkSensorBypass(const String& uuid, bool bypassed) {
  auto it = discoveredSensors.find(uuid);
  if (it != discoveredSensors.end()) {
    it->second.bypassEnabled = bypassed;
    Debug.printf("Set park sensor %s bypass to %s\n", 
                 it->second.name.c_str(), bypassed ? "ENABLED" : "DISABLED");
    
    // Save configuration
    saveParkSensorConfiguration();
  }
}

// Set park sensor enabled status
void setParkSensorEnabled(const String& uuid, bool enabled) {
  if (enabled) {
    addParkSensor(uuid);
  } else {
    removeParkSensor(uuid);
  }
}

// Get park sensor status as string
String getParkSensorStatusString(ParkSensorStatus status) {
  switch (status) {
    case SENSOR_UNKNOWN:
      return "Unknown";
    case SENSOR_PARKED:
      return "Parked";
    case SENSOR_UNPARKED:
      return "Unparked";
    case SENSOR_OFFLINE:
      return "Offline";
    default:
      return "Error";
  }
}

// Save park sensor configuration to preferences
void saveParkSensorConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  
  // Save park sensor type
  prefs.putInt(PREF_PARK_SENSOR_TYPE, static_cast<int>(parkSensorType));
  
  // Save enabled sensor count
  prefs.putInt(PREF_PARK_SENSOR_COUNT, enabledSensorUuids.size());
  
  // Save each enabled sensor's configuration
  for (size_t i = 0; i < enabledSensorUuids.size(); i++) {
    String key = PREF_PARK_SENSOR_PREFIX + String(i);
    String uuid = enabledSensorUuids[i];
    
    // Create a JSON object with sensor configuration
    DynamicJsonDocument doc(512);
    doc["uuid"] = uuid;
    
    auto it = discoveredSensors.find(uuid);
    if (it != discoveredSensors.end()) {
      const ParkSensor& sensor = it->second;
      doc["name"] = sensor.name;
      doc["bypassEnabled"] = sensor.bypassEnabled;
      doc["serialNumber"] = sensor.serialNumber;
      doc["deviceType"] = sensor.deviceType;
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    prefs.putString(key.c_str(), jsonString);
  }
  
  prefs.end();
  Debug.printf("Saved park sensor configuration: %d enabled sensors\n", enabledSensorUuids.size());
}

// Load park sensor configuration from preferences
void loadParkSensorConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, true);  // Read-only
  
  // Load park sensor type
  parkSensorType = static_cast<ParkSensorType>(prefs.getInt(PREF_PARK_SENSOR_TYPE, PARK_SENSOR_PHYSICAL));
  
  // Load enabled sensors
  int sensorCount = prefs.getInt(PREF_PARK_SENSOR_COUNT, 0);
  
  enabledSensorUuids.clear();
  
  for (int i = 0; i < sensorCount; i++) {
    String key = PREF_PARK_SENSOR_PREFIX + String(i);
    String jsonString = prefs.getString(key.c_str(), "");
    
    if (jsonString.length() > 0) {
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, jsonString);
      
      if (!error && doc.containsKey("uuid")) {
        String uuid = doc["uuid"].as<String>();
        enabledSensorUuids.push_back(uuid);
        
        // Create a placeholder sensor entry if it doesn't exist
        if (discoveredSensors.find(uuid) == discoveredSensors.end()) {
          ParkSensor& sensor = discoveredSensors[uuid];
          sensor.uuid = uuid;
          sensor.status = SENSOR_UNKNOWN;
          sensor.enabled = true;
          sensor.lastSeen = 0;
          
          if (doc.containsKey("name")) {
            sensor.name = doc["name"].as<String>();
          }
          if (doc.containsKey("bypassEnabled")) {
            sensor.bypassEnabled = doc["bypassEnabled"].as<bool>();
          }
          if (doc.containsKey("serialNumber")) {
            sensor.serialNumber = doc["serialNumber"].as<String>();
          }
          if (doc.containsKey("deviceType")) {
            sensor.deviceType = doc["deviceType"].as<String>();
          }
        }
      }
    }
  }
  
  prefs.end();
  Debug.printf("Loaded park sensor configuration: type=%d, %d enabled sensors\n", 
               parkSensorType, enabledSensorUuids.size());
}

// Clear sensors that have timed out from the discovered list
void clearTimeoutSensors() {
  unsigned long currentTime = millis();
  auto it = discoveredSensors.begin();
  
  while (it != discoveredSensors.end()) {
    if (currentTime - it->second.lastSeen > (PARK_SENSOR_TIMEOUT * 2)) {
      // Only remove if it's not in the enabled list
      auto enabledIt = std::find(enabledSensorUuids.begin(), enabledSensorUuids.end(), it->first);
      if (enabledIt == enabledSensorUuids.end()) {
        Debug.printf("Removing timed out sensor %s from discovered list\n", it->second.name.c_str());
        it = discoveredSensors.erase(it);
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
}

// Get list of active (enabled) sensors
std::vector<ParkSensor> getActiveSensors() {
  std::vector<ParkSensor> activeSensors;
  
  for (const String& uuid : enabledSensorUuids) {
    auto it = discoveredSensors.find(uuid);
    if (it != discoveredSensors.end()) {
      activeSensors.push_back(it->second);
    }
  }
  
  return activeSensors;
}

// Get list of all discovered sensors
std::vector<ParkSensor> getDiscoveredSensors() {
  std::vector<ParkSensor> sensors;
  
  for (const auto& pair : discoveredSensors) {
    sensors.push_back(pair.second);
  }
  
  return sensors;
}