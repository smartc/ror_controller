/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * GPS Handler Implementation - Provides GPS time and NTP server functionality
 */

#include "gps_handler.h"
#include "Debug.h"
#include <WiFiUdp.h>
#include <Preferences.h>

// GPS Serial port (using HardwareSerial 1)
HardwareSerial GPSSerial(1);

// NTP UDP server
WiFiUDP ntpUdp;

// Global GPS variables
bool gpsEnabled = false;
bool gpsNtpEnabled = false;
GPSStatus gpsStatus = {false, false, 0, 0.0, 0.0, 0.0, {0, 0, 0, 0, 0, 0, false}, 0};

// NMEA parsing buffer
static char nmeaBuffer[128];
static uint8_t nmeaIndex = 0;

// Last valid time (for interpolation between GPS updates)
static unsigned long lastGPSSecondMillis = 0;
static uint32_t lastGPSUnixTime = 0;

// NTP timestamp epoch offset (1900 to 1970 = 70 years)
#define NTP_TIMESTAMP_DELTA 2208988800UL

// Forward declarations for internal functions
static void parseNMEA(const char* sentence);
static void parseGPRMC(const char* sentence);
static void parseGPGGA(const char* sentence);
static double parseLatLon(const char* str, char dir);
static uint32_t dateTimeToUnix(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second);

// Initialize GPS module
void initGPS() {
  if (!gpsEnabled) {
    Debug.println("GPS disabled, skipping initialization");
    return;
  }

  Debug.println("Initializing GPS module...");

  // Initialize GPS serial port
  // GPS TX -> ESP32 RX (GPIO14), GPS RX -> ESP32 TX (GPIO13)
  GPSSerial.begin(9600, SERIAL_8N1, GPS_TX_PIN, GPS_RX_PIN);

  // Clear any existing data
  while (GPSSerial.available()) {
    GPSSerial.read();
  }

  // Initialize status
  gpsStatus.enabled = true;
  gpsStatus.hasFix = false;
  gpsStatus.satellites = 0;
  gpsStatus.time.valid = false;
  gpsStatus.lastUpdate = 0;

  // Initialize NTP server if enabled
  if (gpsNtpEnabled) {
    if (ntpUdp.begin(NTP_PORT)) {
      Debug.printf("NTP server started on port %d\n", NTP_PORT);
    } else {
      Debug.println("Failed to start NTP server");
    }
  }

  Debug.println("GPS initialized");
}

// Process GPS data in main loop
void handleGPS() {
  if (!gpsEnabled) {
    return;
  }

  // Read available GPS data
  while (GPSSerial.available()) {
    char c = GPSSerial.read();

    // Start of new sentence
    if (c == '$') {
      nmeaIndex = 0;
      nmeaBuffer[nmeaIndex++] = c;
    }
    // End of sentence
    else if (c == '\n' || c == '\r') {
      if (nmeaIndex > 0) {
        nmeaBuffer[nmeaIndex] = '\0';
        parseNMEA(nmeaBuffer);
        nmeaIndex = 0;
      }
    }
    // Add to buffer
    else if (nmeaIndex < sizeof(nmeaBuffer) - 1) {
      nmeaBuffer[nmeaIndex++] = c;
    }
  }
}

// Handle NTP server requests
void handleNTP() {
  if (!gpsEnabled || !gpsNtpEnabled) {
    return;
  }

  int packetSize = ntpUdp.parsePacket();
  if (packetSize >= 48) {
    uint8_t packetBuffer[48];
    ntpUdp.read(packetBuffer, 48);

    IPAddress remoteIP = ntpUdp.remoteIP();
    uint16_t remotePort = ntpUdp.remotePort();

    Debug.printf(2, "NTP request from %s:%d\n", remoteIP.toString().c_str(), remotePort);

    sendNTPResponse(packetBuffer, 48, remoteIP, remotePort);
  }
}

// Send NTP response
void sendNTPResponse(uint8_t* buffer, int len, IPAddress remoteIP, uint16_t remotePort) {
  // Get current time
  uint32_t currentTime = getGPSUnixTime();

  if (currentTime == 0) {
    Debug.println("No GPS time available for NTP response");
    return;
  }

  // Convert to NTP timestamp (seconds since 1900)
  uint32_t ntpTime = currentTime + NTP_TIMESTAMP_DELTA;

  // Prepare response packet
  uint8_t response[48];
  memset(response, 0, 48);

  // LI (leap indicator) = 0, VN (version) = 4, Mode = 4 (server)
  response[0] = 0b00100100;  // LI=0, VN=4, Mode=4

  // Stratum = 1 (primary reference - GPS)
  response[1] = 1;

  // Poll interval = 6 (64 seconds)
  response[2] = 6;

  // Precision = -20 (about 1 microsecond)
  response[3] = 0xEC;  // -20 in signed 8-bit

  // Root delay = 0
  response[4] = 0;
  response[5] = 0;
  response[6] = 0;
  response[7] = 0;

  // Root dispersion = 0
  response[8] = 0;
  response[9] = 0;
  response[10] = 0;
  response[11] = 0;

  // Reference ID = "GPS " (for stratum 1)
  response[12] = 'G';
  response[13] = 'P';
  response[14] = 'S';
  response[15] = ' ';

  // Reference timestamp (last GPS update)
  uint32_t refTime = ntpTime;
  response[16] = (refTime >> 24) & 0xFF;
  response[17] = (refTime >> 16) & 0xFF;
  response[18] = (refTime >> 8) & 0xFF;
  response[19] = refTime & 0xFF;
  response[20] = 0; response[21] = 0; response[22] = 0; response[23] = 0;  // Fraction

  // Origin timestamp (copy from request - transmit timestamp)
  memcpy(&response[24], &buffer[40], 8);

  // Receive timestamp (current time when request received)
  response[32] = (ntpTime >> 24) & 0xFF;
  response[33] = (ntpTime >> 16) & 0xFF;
  response[34] = (ntpTime >> 8) & 0xFF;
  response[35] = ntpTime & 0xFF;
  response[36] = 0; response[37] = 0; response[38] = 0; response[39] = 0;  // Fraction

  // Transmit timestamp (current time)
  response[40] = (ntpTime >> 24) & 0xFF;
  response[41] = (ntpTime >> 16) & 0xFF;
  response[42] = (ntpTime >> 8) & 0xFF;
  response[43] = ntpTime & 0xFF;
  response[44] = 0; response[45] = 0; response[46] = 0; response[47] = 0;  // Fraction

  // Send response
  ntpUdp.beginPacket(remoteIP, remotePort);
  ntpUdp.write(response, 48);
  ntpUdp.endPacket();

  Debug.printf(2, "NTP response sent to %s:%d (time: %lu)\n",
               remoteIP.toString().c_str(), remotePort, ntpTime);
}

// Enable/disable GPS
void setGPSEnabled(bool enabled) {
  if (enabled == gpsEnabled) {
    return;
  }

  gpsEnabled = enabled;

  // Save to preferences
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_GPS_ENABLED, gpsEnabled);
  prefs.end();

  if (enabled) {
    initGPS();
  } else {
    // Disable GPS
    gpsStatus.enabled = false;
    gpsStatus.hasFix = false;
    gpsStatus.time.valid = false;

    // Stop NTP server
    ntpUdp.stop();

    Debug.println("GPS disabled");
  }
}

// Enable/disable NTP server
void setGPSNtpEnabled(bool enabled) {
  if (enabled == gpsNtpEnabled) {
    return;
  }

  gpsNtpEnabled = enabled;

  // Save to preferences
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_GPS_NTP_ENABLED, gpsNtpEnabled);
  prefs.end();

  if (enabled && gpsEnabled) {
    if (ntpUdp.begin(NTP_PORT)) {
      Debug.printf("NTP server started on port %d\n", NTP_PORT);
    } else {
      Debug.println("Failed to start NTP server");
    }
  } else if (!enabled) {
    ntpUdp.stop();
    Debug.println("NTP server stopped");
  }
}

// Status functions
bool isGPSEnabled() {
  return gpsEnabled;
}

bool isGPSNtpEnabled() {
  return gpsNtpEnabled;
}

bool hasGPSFix() {
  return gpsStatus.hasFix;
}

GPSStatus getGPSStatus() {
  return gpsStatus;
}

// Get formatted time string (HH:MM:SS)
String getGPSTimeString() {
  if (!gpsStatus.time.valid) {
    return "No Fix";
  }

  char buf[12];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           gpsStatus.time.hour, gpsStatus.time.minute, gpsStatus.time.second);
  return String(buf);
}

// Get formatted date string (YYYY-MM-DD)
String getGPSDateString() {
  if (!gpsStatus.time.valid) {
    return "No Fix";
  }

  char buf[12];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           gpsStatus.time.year, gpsStatus.time.month, gpsStatus.time.day);
  return String(buf);
}

// Get Unix timestamp from GPS
uint32_t getGPSUnixTime() {
  if (!gpsStatus.time.valid) {
    return 0;
  }

  // Calculate current time with interpolation
  uint32_t baseTime = lastGPSUnixTime;
  unsigned long elapsed = (millis() - lastGPSSecondMillis) / 1000;

  return baseTime + elapsed;
}

// ============== NMEA Parsing Functions ==============

static void parseNMEA(const char* sentence) {
  // Check for valid NMEA sentence
  if (sentence[0] != '$') {
    return;
  }

  // Check sentence type
  if (strncmp(sentence + 3, "RMC", 3) == 0) {
    parseGPRMC(sentence);
  }
  else if (strncmp(sentence + 3, "GGA", 3) == 0) {
    parseGPGGA(sentence);
  }
}

// Parse GPRMC sentence (Recommended Minimum Navigation Information)
// $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
static void parseGPRMC(const char* sentence) {
  char* p = (char*)sentence;
  int fieldIndex = 0;
  char field[20];
  int fieldLen = 0;

  char timeStr[12] = "";
  char statusChar = 'V';
  char latStr[15] = "";
  char latDir = 'N';
  char lonStr[15] = "";
  char lonDir = 'E';
  char dateStr[10] = "";

  while (*p) {
    if (*p == ',' || *p == '*') {
      field[fieldLen] = '\0';

      switch (fieldIndex) {
        case 1:  // Time
          strncpy(timeStr, field, sizeof(timeStr) - 1);
          break;
        case 2:  // Status (A=Active, V=Void)
          statusChar = field[0];
          break;
        case 3:  // Latitude
          strncpy(latStr, field, sizeof(latStr) - 1);
          break;
        case 4:  // Latitude direction
          latDir = field[0];
          break;
        case 5:  // Longitude
          strncpy(lonStr, field, sizeof(lonStr) - 1);
          break;
        case 6:  // Longitude direction
          lonDir = field[0];
          break;
        case 9:  // Date
          strncpy(dateStr, field, sizeof(dateStr) - 1);
          break;
      }

      fieldIndex++;
      fieldLen = 0;
    } else {
      if (fieldLen < sizeof(field) - 1) {
        field[fieldLen++] = *p;
      }
    }
    p++;
  }

  // Update GPS status
  gpsStatus.hasFix = (statusChar == 'A');

  if (gpsStatus.hasFix && strlen(timeStr) >= 6 && strlen(dateStr) >= 6) {
    // Parse time (hhmmss.ss)
    gpsStatus.time.hour = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    gpsStatus.time.minute = (timeStr[2] - '0') * 10 + (timeStr[3] - '0');
    gpsStatus.time.second = (timeStr[4] - '0') * 10 + (timeStr[5] - '0');

    // Parse date (ddmmyy)
    uint8_t day = (dateStr[0] - '0') * 10 + (dateStr[1] - '0');
    uint8_t month = (dateStr[2] - '0') * 10 + (dateStr[3] - '0');
    uint8_t year2 = (dateStr[4] - '0') * 10 + (dateStr[5] - '0');

    gpsStatus.time.day = day;
    gpsStatus.time.month = month;
    gpsStatus.time.year = 2000 + year2;  // Assume 21st century
    gpsStatus.time.valid = true;

    // Parse position
    if (strlen(latStr) > 0) {
      gpsStatus.latitude = parseLatLon(latStr, latDir);
    }
    if (strlen(lonStr) > 0) {
      gpsStatus.longitude = parseLatLon(lonStr, lonDir);
    }

    // Update Unix time reference
    lastGPSUnixTime = dateTimeToUnix(gpsStatus.time.year, gpsStatus.time.month,
                                      gpsStatus.time.day, gpsStatus.time.hour,
                                      gpsStatus.time.minute, gpsStatus.time.second);
    lastGPSSecondMillis = millis();

    gpsStatus.lastUpdate = millis();

    Debug.printf(2, "GPS Time: %s %s, Lat: %.6f, Lon: %.6f\n",
                 getGPSDateString().c_str(), getGPSTimeString().c_str(),
                 gpsStatus.latitude, gpsStatus.longitude);
  }
}

// Parse GPGGA sentence (Global Positioning System Fix Data)
// $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
static void parseGPGGA(const char* sentence) {
  char* p = (char*)sentence;
  int fieldIndex = 0;
  char field[20];
  int fieldLen = 0;

  while (*p) {
    if (*p == ',' || *p == '*') {
      field[fieldLen] = '\0';

      switch (fieldIndex) {
        case 7:  // Number of satellites
          if (fieldLen > 0) {
            gpsStatus.satellites = atoi(field);
          }
          break;
        case 9:  // Altitude
          if (fieldLen > 0) {
            gpsStatus.altitude = atof(field);
          }
          break;
      }

      fieldIndex++;
      fieldLen = 0;
    } else {
      if (fieldLen < sizeof(field) - 1) {
        field[fieldLen++] = *p;
      }
    }
    p++;
  }
}

// Parse latitude/longitude from NMEA format
static double parseLatLon(const char* str, char dir) {
  if (strlen(str) == 0) {
    return 0.0;
  }

  // Find decimal point
  const char* dot = strchr(str, '.');
  if (!dot) {
    return 0.0;
  }

  // NMEA format: dddmm.mmmm or ddmm.mmmm
  int degLen = (dot - str) - 2;  // Degrees are everything before the last 2 digits before dot
  if (degLen < 1 || degLen > 3) {
    return 0.0;
  }

  char degStr[4] = "";
  strncpy(degStr, str, degLen);
  degStr[degLen] = '\0';

  double degrees = atof(degStr);
  double minutes = atof(str + degLen);

  double result = degrees + (minutes / 60.0);

  if (dir == 'S' || dir == 'W') {
    result = -result;
  }

  return result;
}

// Convert date/time to Unix timestamp
static uint32_t dateTimeToUnix(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second) {
  // Days in each month (non-leap year)
  static const uint16_t daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  // Calculate days since 1970
  uint32_t days = 0;

  // Add days for complete years
  for (uint16_t y = 1970; y < year; y++) {
    days += 365;
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
      days++;  // Leap year
    }
  }

  // Add days for complete months
  days += daysBeforeMonth[month - 1];

  // Add leap day if applicable
  if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    days++;
  }

  // Add days in current month
  days += day - 1;

  // Convert to seconds
  uint32_t unixTime = days * 86400UL;
  unixTime += hour * 3600UL;
  unixTime += minute * 60UL;
  unixTime += second;

  return unixTime;
}
