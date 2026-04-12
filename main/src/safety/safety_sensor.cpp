/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller
 * Safety Sensor UDP Handler Implementation
 */

#include "safety_sensor.h"
#include "../debug/Debug.h"
#include <Preferences.h>

// Global state
std::map<String, SafetySensor> discoveredSafetySensors;
bool bypassSafetySensor = false;
bool weatherAutoClose   = false;

// NVS keys (private to this file)
#define PREF_BYPASS_SAFETY       "bypassSafety"
#define PREF_WEATHER_AUTOCLOSE   "weatherAutoCls"
#define PREF_SAFETY_SENSOR_COUNT "safetySensorCnt"
#define PREF_SAFETY_SENSOR_PFX   "safetySensor_"

// ---------------------------------------------------------------------------
// Packet handling
// ---------------------------------------------------------------------------

void processSafetySensorMessage(const String& message, IPAddress remoteIP) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Debug.printf("SafetySensor JSON parse error: %s\n", error.c_str());
    return;
  }

  if (!doc.containsKey("serialNumber") || !doc.containsKey("isSafe")) {
    Debug.println("SafetySensor message missing serialNumber or isSafe");
    return;
  }

  String serial = doc["serialNumber"].as<String>();
  bool   isSafe = doc["isSafe"].as<bool>();

  bool isNew    = (discoveredSafetySensors.find(serial) == discoveredSafetySensors.end());
  SafetySensor& sensor = discoveredSafetySensors[serial];

  bool prevSafe   = sensor.isSafe;
  bool prevOnline = sensor.online;

  sensor.serialNumber = serial;
  sensor.isSafe       = isSafe;
  sensor.lastSeen     = millis();
  sensor.online       = true;
  sensor.ipAddress    = remoteIP.toString();

  if (doc.containsKey("name")) {
    sensor.name = doc["name"].as<String>();
  }
  if (doc.containsKey("ambTemp") && !doc["ambTemp"].isNull()) {
    sensor.ambTemp = doc["ambTemp"].as<float>();
  } else {
    sensor.ambTemp = NAN;
  }

  if (isNew) {
    Debug.printf("NEW safety sensor discovered: %s (%s) at %s — %s\n",
                 sensor.name.c_str(), serial.c_str(),
                 remoteIP.toString().c_str(),
                 isSafe ? "SAFE" : "UNSAFE");
  } else if (isSafe != prevSafe || !prevOnline) {
    Debug.printf("Safety sensor %s state changed: %s\n",
                 sensor.name.c_str(), isSafe ? "SAFE" : "UNSAFE");
  } else {
    Debug.printf(2, "Safety sensor %s: %s\n",
                 sensor.name.c_str(), isSafe ? "SAFE" : "UNSAFE");
  }
}

// ---------------------------------------------------------------------------
// Timeout monitoring
// ---------------------------------------------------------------------------

void updateSafetySensorStatus() {
  unsigned long now = millis();
  for (auto& pair : discoveredSafetySensors) {
    SafetySensor& sensor = pair.second;
    bool wasOnline = sensor.online;
    sensor.online  = (now - sensor.lastSeen < SAFETY_SENSOR_TIMEOUT);
    if (wasOnline && !sensor.online) {
      Debug.printf("Safety sensor %s (%s) has gone offline\n",
                   sensor.name.c_str(), sensor.serialNumber.c_str());
    }
  }
}

// ---------------------------------------------------------------------------
// Core safety predicate
// ---------------------------------------------------------------------------

bool isWeatherSafe() {
  if (bypassSafetySensor) return true;

  bool hasActiveSensor = false;
  for (const auto& pair : discoveredSafetySensors) {
    const SafetySensor& sensor = pair.second;
    if (!sensor.enabled)     continue;
    if (sensor.bypassEnabled) continue;
    hasActiveSensor = true;
    if (!sensor.online)  return false;   // offline enabled sensor → fail-safe
    if (!sensor.isSafe)  return false;
  }
  // No enabled sensors → don't block
  return true;
}

// ---------------------------------------------------------------------------
// Per-sensor management
// ---------------------------------------------------------------------------

void setSafetySensorEnabled(const String& serial, bool enabled) {
  auto it = discoveredSafetySensors.find(serial);
  if (it != discoveredSafetySensors.end()) {
    it->second.enabled = enabled;
    Debug.printf("Safety sensor %s %s\n",
                 it->second.name.c_str(), enabled ? "enabled" : "disabled");
    saveSafetySensorConfiguration();
  }
}

void setSafetySensorBypass(const String& serial, bool bypassed) {
  auto it = discoveredSafetySensors.find(serial);
  if (it != discoveredSafetySensors.end()) {
    it->second.bypassEnabled = bypassed;
    Debug.printf("Safety sensor %s bypass %s\n",
                 it->second.name.c_str(), bypassed ? "enabled" : "disabled");
    saveSafetySensorConfiguration();
  }
}

void removeSafetySensor(const String& serial) {
  auto it = discoveredSafetySensors.find(serial);
  if (it != discoveredSafetySensors.end()) {
    Debug.printf("Safety sensor %s (%s) removed\n",
                 it->second.name.c_str(), serial.c_str());
    discoveredSafetySensors.erase(it);
    saveSafetySensorConfiguration();
  }
}

void removeAllSafetySensors() {
  discoveredSafetySensors.clear();
  saveSafetySensorConfiguration();
  Debug.println("All safety sensors removed");
}

// ---------------------------------------------------------------------------
// Persistent storage
// ---------------------------------------------------------------------------

void saveSafetySensorConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_BYPASS_SAFETY, bypassSafetySensor);
  prefs.putBool(PREF_WEATHER_AUTOCLOSE, weatherAutoClose);

  int count = 0;
  for (const auto& pair : discoveredSafetySensors) {
    const SafetySensor& sensor = pair.second;
    String key = String(PREF_SAFETY_SENSOR_PFX) + String(count);
    DynamicJsonDocument doc(256);
    doc["serial"]  = sensor.serialNumber;
    doc["name"]    = sensor.name;
    doc["enabled"] = sensor.enabled;
    doc["bypass"]  = sensor.bypassEnabled;
    String json;
    serializeJson(doc, json);
    prefs.putString(key.c_str(), json);
    count++;
  }
  prefs.putInt(PREF_SAFETY_SENSOR_COUNT, count);
  prefs.end();
  Debug.printf("Saved safety sensor config: bypass=%s, autoClose=%s, %d sensors\n",
               bypassSafetySensor ? "on" : "off",
               weatherAutoClose   ? "on" : "off", count);
}

void loadSafetySensorConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, true);
  bypassSafetySensor = prefs.getBool(PREF_BYPASS_SAFETY, false);
  weatherAutoClose   = prefs.getBool(PREF_WEATHER_AUTOCLOSE, false);

  int count = prefs.getInt(PREF_SAFETY_SENSOR_COUNT, 0);
  for (int i = 0; i < count; i++) {
    String key  = String(PREF_SAFETY_SENSOR_PFX) + String(i);
    String json = prefs.getString(key.c_str(), "");
    if (json.length() == 0) continue;

    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, json) || !doc.containsKey("serial")) continue;

    String serial = doc["serial"].as<String>();
    SafetySensor& sensor  = discoveredSafetySensors[serial];
    sensor.serialNumber   = serial;
    if (doc.containsKey("name")) sensor.name = doc["name"].as<String>();
    sensor.enabled        = doc["enabled"] | false;
    sensor.bypassEnabled  = doc["bypass"]  | false;
    sensor.online         = false;
    sensor.lastSeen       = 0;
  }
  prefs.end();
  Debug.printf("Loaded safety sensor config: bypass=%s, autoClose=%s, %d sensors\n",
               bypassSafetySensor ? "on" : "off",
               weatherAutoClose   ? "on" : "off", count);
}

// ---------------------------------------------------------------------------
// Accessor
// ---------------------------------------------------------------------------

std::vector<SafetySensor> getDiscoveredSafetySensors() {
  std::vector<SafetySensor> result;
  for (const auto& pair : discoveredSafetySensors) {
    result.push_back(pair.second);
  }
  return result;
}
