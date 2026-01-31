/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * GPS and RTC Handler - Provides GPS time, RTC backup, and NTP server functionality
 */

#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <Arduino.h>
#include "config.h"

// GPS time structure (also used for RTC)
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
  uint8_t satellites_in_view;
  double latitude;
  double longitude;
  double altitude;
  double hdop;
  GPSTime time;
  unsigned long lastUpdate;
};

// Time source enumeration
enum TimeSource {
  TIME_SOURCE_NONE = 0,
  TIME_SOURCE_GPS = 1,
  TIME_SOURCE_RTC = 2
};

// Global GPS and RTC variables
extern bool gpsEnabled;
extern bool gpsNtpEnabled;
extern bool rtcPresent;
extern bool timeSynced;
extern int16_t timezoneOffset;            // Timezone offset in minutes from UTC
extern bool dstEnabled;                   // Daylight Saving Time enabled
extern GPSStatus gpsStatus;
extern TimeSource currentTimeSource;

// GPS Pin configuration (user-configurable)
extern int gpsTxPin;                      // GPS TX -> ESP32 RX (receives GPS data)
extern int gpsRxPin;                      // GPS RX -> ESP32 TX (-1 = disabled)
extern int gpsPpsPin;                     // GPS PPS pin (-1 = disabled)

// Function prototypes - GPS
void initGPS();                           // Initialize GPS module
void handleGPS();                         // Process GPS data in main loop

// Function prototypes - RTC
void initRTC();                           // Initialize RTC module (DS3231)
bool isRTCPresent();                      // Check if RTC is detected
bool readRTC(GPSTime* time);              // Read time from RTC
bool writeRTC(const GPSTime* time);       // Write time to RTC
void syncRTCFromGPS();                    // Sync RTC from GPS when GPS has fix

// Function prototypes - NTP
void initNTP();                           // Initialize NTP server
void handleNTP();                         // Handle NTP server requests

// GPS control functions
void setGPSEnabled(bool enabled);         // Enable/disable GPS
void setGPSNtpEnabled(bool enabled);      // Enable/disable NTP server

// Status functions
bool isGPSEnabled();                      // Check if GPS is enabled
bool isGPSNtpEnabled();                   // Check if NTP server is enabled
bool hasGPSFix();                         // Check if GPS has valid fix
bool isTimeSynced();                      // Check if time is synced (GPS or RTC)
bool isPPSActive();                       // Check if PPS signal is active and healthy
uint32_t getPPSCount();                   // Get total PPS pulse count
TimeSource getTimeSource();               // Get current time source
GPSStatus getGPSStatus();                 // Get current GPS status
String getTimeString();                   // Get formatted UTC time string (from best source)
String getDateString();                   // Get formatted UTC date string (from best source)
String getLocalTimeString();              // Get formatted local time string (with TZ + DST)
String getLocalDateString();              // Get formatted local date string (with TZ + DST)
String getGPSTimeString();                // Get formatted GPS time string
String getGPSDateString();                // Get formatted GPS date string
uint32_t getCurrentUnixTime();            // Get Unix timestamp from best available source
uint32_t getLocalUnixTime();              // Get local Unix timestamp (with TZ + DST offset)
uint32_t getGPSUnixTime();                // Get Unix timestamp from GPS only

// Timezone functions
void setTimezoneOffset(int16_t offset);   // Set timezone offset in minutes
void setDSTEnabled(bool enabled);         // Enable/disable DST
int16_t getTimezoneOffset();              // Get current timezone offset
bool isDSTEnabled();                      // Check if DST is enabled
int16_t getTotalOffset();                 // Get total offset (TZ + DST) in minutes

// NTP response helpers
void sendNTPResponse(uint8_t* buffer, int len, IPAddress remoteIP, uint16_t remotePort);

#endif // GPS_HANDLER_H
