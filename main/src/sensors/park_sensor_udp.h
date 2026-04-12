/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * UDP Park Sensor Handler Header
 */

#ifndef PARK_SENSOR_UDP_H
#define PARK_SENSOR_UDP_H

#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "config.h"

// UDP Park Sensor Configuration
#define PARK_SENSOR_UDP_PORT 23435
#define PARK_SENSOR_TIMEOUT 30000  // 30 seconds timeout
#define MAX_PARK_SENSORS 10        // Maximum number of sensors to track

// Park Sensor Status
enum ParkSensorStatus {
  SENSOR_UNKNOWN = 0,    // Never seen or timed out
  SENSOR_PARKED = 1,     // Safe to move (isSafeToMove = true)
  SENSOR_UNPARKED = 2,   // Not safe to move (isSafeToMove = false)
  SENSOR_OFFLINE = 3     // Sensor has timed out
};

// Park Sensor Data Structure
struct ParkSensor {
  String uuid;
  String deviceType;
  String serialNumber;
  String ipAddress;
  String name;
  float pitch;
  float roll;
  bool isSafeToMove;
  ParkSensorStatus status;
  unsigned long lastSeen;
  bool enabled;          // Whether this sensor is enabled for park checking
  bool bypassEnabled;    // Individual bypass for this sensor
  
  // Constructor
  ParkSensor() : 
    pitch(0.0), roll(0.0), isSafeToMove(false), 
    status(SENSOR_UNKNOWN), lastSeen(0), 
    enabled(false), bypassEnabled(false) {}
};

// Park Sensor Type Selection
enum ParkSensorType {
  PARK_SENSOR_PHYSICAL = 0,  // Use physical park sensor circuit
  PARK_SENSOR_UDP = 1,       // Use UDP-based park sensors
  PARK_SENSOR_BOTH = 2       // Use both (AND logic - all must be parked)
};

// Global variables
extern WiFiUDP parkSensorUdp;
extern std::map<String, ParkSensor> discoveredSensors;
extern std::vector<String> enabledSensorUuids;
extern ParkSensorType parkSensorType;
extern bool udpParkSensorSystemEnabled;

// Function prototypes
void initParkSensorUDP();
void handleParkSensorUDP();
void processParkSensorMessage(const String& message, IPAddress remoteIP);
void updateParkSensorStatus();
bool isTelescopeParkedUDP();
void addParkSensor(const String& uuid);
void removeParkSensor(const String& uuid);
void removeAllParkSensors();
void setParkSensorBypass(const String& uuid, bool bypassed);
void setParkSensorEnabled(const String& uuid, bool enabled);
String getParkSensorStatusString(ParkSensorStatus status);
void saveParkSensorConfiguration();
void loadParkSensorConfiguration();
void clearTimeoutSensors();
std::vector<ParkSensor> getActiveSensors();
std::vector<ParkSensor> getDiscoveredSensors();

#endif // PARK_SENSOR_UDP_H