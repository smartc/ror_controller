/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller
 * Safety Sensor UDP Handler Header
 *
 * Receives SafetySensor UDP broadcasts on the shared park-sensor port (23435).
 * Any device broadcasting deviceType=="SafetySensor" is auto-discovered.
 * Newly discovered sensors default to disabled — they must be explicitly
 * enabled before they participate in the isWeatherSafe() decision.
 */

#ifndef SAFETY_SENSOR_H
#define SAFETY_SENSOR_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "config.h"

// 90 seconds timeout — 3× the default 30 s broadcast interval
#define SAFETY_SENSOR_TIMEOUT 90000

// Per-sensor data structure (mirrors ParkSensor pattern for consistency)
struct SafetySensor {
  String serialNumber;
  String name;
  String ipAddress;
  bool   isSafe;
  float  ambTemp;         // NaN if JSON null was received
  unsigned long lastSeen;
  bool   online;          // false when timed out
  bool   enabled;         // user has opted this sensor into the safety decision
  bool   bypassEnabled;   // per-sensor temporary bypass

  SafetySensor()
    : isSafe(true), ambTemp(NAN), lastSeen(0),
      online(false), enabled(false), bypassEnabled(false) {}
};

// Global state
extern std::map<String, SafetySensor> discoveredSafetySensors;
extern bool bypassSafetySensor;   // global override — skips ALL safety checks
extern bool weatherAutoClose;     // auto-close roof when weather is unsafe (default: false)

// Called from park_sensor_udp.cpp dispatcher
void processSafetySensorMessage(const String& message, IPAddress remoteIP);

// Called from handleParkSensorUDP() to age-out stale sensors
void updateSafetySensorStatus();

// Core safety predicate used by roof_controller and web UI
bool isWeatherSafe();

// Per-sensor management (called from web UI handlers)
void setSafetySensorEnabled(const String& serialNumber, bool enabled);
void setSafetySensorBypass(const String& serialNumber, bool bypassed);

// Persistent storage
void saveSafetySensorConfiguration();
void loadSafetySensorConfiguration();

// Accessor for HTML templates / MQTT
std::vector<SafetySensor> getDiscoveredSafetySensors();

#endif // SAFETY_SENSOR_H
