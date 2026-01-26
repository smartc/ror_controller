/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * GPS Handler - Provides GPS time and NTP server functionality
 */

#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <Arduino.h>
#include "config.h"

// GPS time structure
struct GPSTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  bool valid;
};

// GPS status structure
struct GPSStatus {
  bool enabled;
  bool hasFix;
  uint8_t satellites;
  double latitude;
  double longitude;
  double altitude;
  GPSTime time;
  unsigned long lastUpdate;
};

// Global GPS variables
extern bool gpsEnabled;
extern bool gpsNtpEnabled;
extern GPSStatus gpsStatus;

// Function prototypes
void initGPS();                           // Initialize GPS module
void handleGPS();                         // Process GPS data in main loop
void handleNTP();                         // Handle NTP server requests

// GPS control functions
void setGPSEnabled(bool enabled);         // Enable/disable GPS
void setGPSNtpEnabled(bool enabled);      // Enable/disable NTP server

// Status functions
bool isGPSEnabled();                      // Check if GPS is enabled
bool isGPSNtpEnabled();                   // Check if NTP server is enabled
bool hasGPSFix();                         // Check if GPS has valid fix
GPSStatus getGPSStatus();                 // Get current GPS status
String getGPSTimeString();                // Get formatted time string
String getGPSDateString();                // Get formatted date string
uint32_t getGPSUnixTime();                // Get Unix timestamp from GPS

// NTP response helpers
void sendNTPResponse(uint8_t* buffer, int len, IPAddress remoteIP, uint16_t remotePort);

#endif // GPS_HANDLER_H
