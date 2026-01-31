/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * GPS and RTC Handler - Provides GPS time, RTC backup, and NTP server functionality
 */

#include "gps_handler.h"
#include "Debug.h"
#include <WiFiUdp.h>
#include <Wire.h>
#include <Preferences.h>

// GPS Serial port (using HardwareSerial 1)
HardwareSerial GPSSerial(1);

// NTP UDP server
WiFiUDP ntpUdp;

// Global GPS and RTC variables
bool gpsEnabled = false;
bool gpsNtpEnabled = false;
bool rtcPresent = false;
bool timeSynced = false;
int16_t timezoneOffset = 0;     // Timezone offset in minutes from UTC
bool dstEnabled = false;        // Daylight Saving Time enabled
TimeSource currentTimeSource = TIME_SOURCE_NONE;
GPSStatus gpsStatus = {false, false, 0, 0, 0.0, 0.0, 0.0, 0.0, {0, 0, 0, 0, 0, 0, false}, 0};

// GPS Pin configuration (user-configurable via WebUI)
int gpsTxPin = DEFAULT_GPS_TX_PIN;    // GPS TX -> ESP32 RX
int gpsRxPin = DEFAULT_GPS_RX_PIN;    // GPS RX -> ESP32 TX (-1 = disabled)
int gpsPpsPin = DEFAULT_GPS_PPS_PIN;  // GPS PPS pin (-1 = disabled)

// RTC time storage (when GPS not available)
static GPSTime rtcTime = {0, 0, 0, 0, 0, 0, false};
static unsigned long lastRTCReadMillis = 0;
static uint32_t lastRTCUnixTime = 0;

// NMEA parsing buffer
static char nmeaBuffer[128];
static uint8_t nmeaIndex = 0;

// Last valid GPS time (for interpolation between GPS updates)
static unsigned long lastGPSSecondMillis = 0;
static uint32_t lastGPSUnixTime = 0;
static unsigned long lastGPSUpdateMicros = 0;  // micros() when lastGPSUnixTime was set (for PPS correlation)

// PPS (Pulse Per Second) timing variables
static volatile unsigned long ppsLastMicros = 0;     // Microsecond timestamp of last PPS rising edge
static volatile unsigned long ppsPrevMicros = 0;     // Previous PPS edge (for period measurement)
static volatile uint32_t ppsCount = 0;               // Total PPS pulses received
static volatile bool ppsTriggered = false;            // Flag: new PPS edge received
static bool ppsActive = false;                        // PPS signal is healthy and active
static uint32_t ppsUnixTimeAtEdge = 0;               // Unix time corresponding to last PPS edge
static unsigned long ppsLastValidMillis = 0;          // millis() of last validated PPS edge

// PPS interrupt handler - called on rising edge of PPS signal
static void IRAM_ATTR ppsInterruptHandler() {
  ppsPrevMicros = ppsLastMicros;
  ppsLastMicros = micros();
  ppsCount++;
  ppsTriggered = true;
}

// Track if we've synced RTC from GPS
static bool rtcSyncedFromGPS = false;

// NTP timestamp epoch offset (1900 to 1970 = 70 years)
#define NTP_TIMESTAMP_DELTA 2208988800UL

// PPS timing constants
#define PPS_TIMEOUT_MS 3000            // PPS considered lost if no pulse for 3 seconds
#define PPS_PERIOD_MIN_US 950000       // Minimum valid PPS period (950ms)
#define PPS_PERIOD_MAX_US 1050000      // Maximum valid PPS period (1050ms)

// Forward declarations for internal functions
static void parseNMEA(const char* sentence);
static void parseGPRMC(const char* sentence);
static void parseGPGGA(const char* sentence);
static void parseGPGSV(const char* sentence);
static double parseLatLon(const char* str, char dir);
static uint32_t dateTimeToUnix(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second);
static uint8_t bcdToDec(uint8_t val);
static uint8_t decToBcd(uint8_t val);

// ============== RTC Functions (DS3231) ==============

// Initialize RTC module
void initRTC() {
  Debug.println("Initializing RTC (DS3231)...");

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Check if DS3231 is present
  Wire.beginTransmission(DS3231_ADDRESS);
  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    rtcPresent = true;
    Debug.println("DS3231 RTC detected");

    // Try to read time from RTC
    GPSTime rtcReadTime;
    if (readRTC(&rtcReadTime) && rtcReadTime.valid) {
      rtcTime = rtcReadTime;
      lastRTCUnixTime = dateTimeToUnix(rtcTime.year, rtcTime.month, rtcTime.day,
                                        rtcTime.hour, rtcTime.minute, rtcTime.second);
      lastRTCReadMillis = millis();
      timeSynced = true;
      currentTimeSource = TIME_SOURCE_RTC;
      Debug.printf("RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                   rtcTime.year, rtcTime.month, rtcTime.day,
                   rtcTime.hour, rtcTime.minute, rtcTime.second);
    } else {
      Debug.println("RTC time not set or invalid");
    }
  } else {
    rtcPresent = false;
    Debug.println("DS3231 RTC not detected");
  }
}

// Check if RTC is present
bool isRTCPresent() {
  return rtcPresent;
}

// Read time from DS3231 RTC
bool readRTC(GPSTime* time) {
  if (!rtcPresent) {
    return false;
  }

  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(0x00);  // Start at register 0
  if (Wire.endTransmission() != 0) {
    return false;
  }

  Wire.requestFrom(DS3231_ADDRESS, (uint8_t)7);
  if (Wire.available() < 7) {
    return false;
  }

  time->second = bcdToDec(Wire.read() & 0x7F);
  time->minute = bcdToDec(Wire.read());
  time->hour = bcdToDec(Wire.read() & 0x3F);  // 24-hour mode
  Wire.read();  // Day of week (skip)
  time->day = bcdToDec(Wire.read());
  uint8_t monthRaw = Wire.read();
  time->month = bcdToDec(monthRaw & 0x1F);
  time->year = 2000 + bcdToDec(Wire.read());

  // Century bit is in month register bit 7
  if (monthRaw & 0x80) {
    time->year += 100;
  }

  // Basic validation
  if (time->year >= 2020 && time->year <= 2099 &&
      time->month >= 1 && time->month <= 12 &&
      time->day >= 1 && time->day <= 31 &&
      time->hour <= 23 && time->minute <= 59 && time->second <= 59) {
    time->valid = true;
    return true;
  }

  time->valid = false;
  return false;
}

// Write time to DS3231 RTC
bool writeRTC(const GPSTime* time) {
  if (!rtcPresent || !time->valid) {
    return false;
  }

  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(0x00);  // Start at register 0
  Wire.write(decToBcd(time->second));
  Wire.write(decToBcd(time->minute));
  Wire.write(decToBcd(time->hour));  // 24-hour mode
  Wire.write(1);  // Day of week (not used, set to 1)
  Wire.write(decToBcd(time->day));
  Wire.write(decToBcd(time->month));
  Wire.write(decToBcd(time->year - 2000));

  if (Wire.endTransmission() != 0) {
    return false;
  }

  Debug.printf("RTC time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
               time->year, time->month, time->day,
               time->hour, time->minute, time->second);
  return true;
}

// Sync RTC from GPS when GPS has fix
void syncRTCFromGPS() {
  if (!rtcPresent || !gpsStatus.hasFix || !gpsStatus.time.valid) {
    return;
  }

  // Only sync once per session (or when GPS time changes significantly)
  if (!rtcSyncedFromGPS) {
    if (writeRTC(&gpsStatus.time)) {
      rtcSyncedFromGPS = true;
      Debug.println("RTC synced from GPS");
    }
  }
}

// BCD conversion helpers
static uint8_t bcdToDec(uint8_t val) {
  return ((val >> 4) * 10) + (val & 0x0F);
}

static uint8_t decToBcd(uint8_t val) {
  return ((val / 10) << 4) + (val % 10);
}

// ============== GPS Functions ==============

// Initialize GPS module
void initGPS() {
  if (!gpsEnabled) {
    Debug.println("GPS disabled, skipping initialization");
    return;
  }

  Debug.println("Initializing GPS module...");

  // Initialize GPS serial port with configurable pins
  // If gpsRxPin is -1, use -1 for TX (receive-only mode)
  int rxPin = gpsTxPin;   // ESP32 RX <- GPS TX (receives data FROM GPS)
  int txPin = gpsRxPin;   // ESP32 TX -> GPS RX (sends commands TO GPS, -1 = disabled)

  GPSSerial.begin(9600, SERIAL_8N1, rxPin, txPin);
  Debug.printf("GPS Serial: RX=%d (from GPS), TX=%d (to GPS)\n", rxPin, txPin);

  // Clear any existing data
  while (GPSSerial.available()) {
    GPSSerial.read();
  }

  // Initialize status
  gpsStatus.enabled = true;
  gpsStatus.hasFix = false;
  gpsStatus.satellites = 0;
  gpsStatus.satellites_in_view = 0;
  gpsStatus.hdop = 0.0;
  gpsStatus.time.valid = false;
  gpsStatus.lastUpdate = 0;

  // PPS pin setup with interrupt for sub-second timing
  if (gpsPpsPin >= 0) {
    pinMode(gpsPpsPin, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(gpsPpsPin), ppsInterruptHandler, RISING);
    Debug.printf("GPS PPS interrupt attached: GPIO%d\n", gpsPpsPin);
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

  // Process PPS edge: correlate with latest NMEA time
  if (ppsTriggered && gpsPpsPin >= 0) {
    ppsTriggered = false;

    // Validate PPS period (should be ~1 second between pulses)
    unsigned long lastMicros = ppsLastMicros;
    unsigned long prevMicros = ppsPrevMicros;
    unsigned long period = lastMicros - prevMicros;

    bool periodValid = (prevMicros != 0) &&
                       (period >= PPS_PERIOD_MIN_US) &&
                       (period <= PPS_PERIOD_MAX_US);

    if (periodValid && gpsStatus.hasFix && gpsStatus.time.valid) {
      // Determine PPS-to-NMEA timing relationship.
      // handleGPS() reads serial BEFORE checking ppsTriggered, so:
      //   - If NMEA was parsed in this call (after PPS edge): lastGPSUpdateMicros > ppsLastMicros
      //     → NMEA describes THIS PPS second → ppsUnixTimeAtEdge = lastGPSUnixTime
      //   - If NMEA was parsed in a previous call (before PPS edge): lastGPSUpdateMicros < ppsLastMicros
      //     → NMEA describes the PREVIOUS second → ppsUnixTimeAtEdge = lastGPSUnixTime + 1
      bool nmeaIsCurrentSecond = ((long)(lastGPSUpdateMicros - lastMicros) > 0);
      ppsUnixTimeAtEdge = nmeaIsCurrentSecond ? lastGPSUnixTime : (lastGPSUnixTime + 1);
      ppsActive = true;
      ppsLastValidMillis = millis();

      Debug.printf(2, "PPS #%lu: period=%lu us, time=%lu, nmea=%s\n",
                   (unsigned long)ppsCount, period, ppsUnixTimeAtEdge,
                   nmeaIsCurrentSecond ? "current" : "previous");
    } else if (periodValid) {
      // PPS pulses are coming but no GPS fix yet - track but don't activate
      ppsLastValidMillis = millis();
    }
  }

  // Mark PPS as inactive if no valid pulse recently
  if (ppsActive && (millis() - ppsLastValidMillis > PPS_TIMEOUT_MS)) {
    ppsActive = false;
    Debug.println("PPS signal lost");
  }

  // Update time source and sync RTC if GPS has fix
  if (gpsStatus.hasFix && gpsStatus.time.valid) {
    timeSynced = true;
    currentTimeSource = TIME_SOURCE_GPS;
    syncRTCFromGPS();
  }
}

// ============== NTP Functions ==============

// Initialize NTP server
void initNTP() {
  if (!gpsNtpEnabled) {
    Debug.println("NTP server disabled");
    return;
  }

  if (ntpUdp.begin(NTP_PORT)) {
    Debug.printf("NTP server started on port %d\n", NTP_PORT);
  } else {
    Debug.println("Failed to start NTP server");
  }
}

// Handle NTP server requests
void handleNTP() {
  // NTP requires time to be synced (from GPS or RTC), and NTP to be enabled
  if (!gpsNtpEnabled || !timeSynced) {
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

// Calculate NTP fractional second (32-bit) from microseconds elapsed since PPS edge
static uint32_t microsToNTPFraction(unsigned long elapsedMicros) {
  // NTP fraction is 2^32 per second, so 1 microsecond = 2^32 / 1000000 ≈ 4295
  // Use 64-bit arithmetic to avoid overflow
  return (uint32_t)(((uint64_t)elapsedMicros * 4294967296ULL) / 1000000ULL);
}

// Get high-resolution NTP timestamp using PPS if available
// Sets ntpSeconds and ntpFraction; returns true if PPS-disciplined
static bool getPPSNTPTimestamp(uint32_t* ntpSeconds, uint32_t* ntpFraction) {
  if (!ppsActive || ppsUnixTimeAtEdge == 0) {
    // No PPS - fall back to second-level accuracy
    uint32_t currentTime = getCurrentUnixTime();
    if (currentTime == 0) return false;
    *ntpSeconds = currentTime + NTP_TIMESTAMP_DELTA;
    *ntpFraction = 0;
    return false;
  }

  // Calculate elapsed microseconds since last PPS edge
  unsigned long nowMicros = micros();
  unsigned long edgeMicros = ppsLastMicros;  // Read volatile once
  unsigned long elapsedMicros = nowMicros - edgeMicros;

  // If more than 1 second since PPS edge, the second has rolled over
  uint32_t extraSeconds = elapsedMicros / 1000000UL;
  unsigned long fracMicros = elapsedMicros % 1000000UL;

  uint32_t unixTime = ppsUnixTimeAtEdge + extraSeconds;
  *ntpSeconds = unixTime + NTP_TIMESTAMP_DELTA;
  *ntpFraction = microsToNTPFraction(fracMicros);

  return true;
}

// Write a 64-bit NTP timestamp (seconds + fraction) into a buffer at offset
static void writeNTPTimestamp(uint8_t* buf, int offset, uint32_t seconds, uint32_t fraction) {
  buf[offset]     = (seconds >> 24) & 0xFF;
  buf[offset + 1] = (seconds >> 16) & 0xFF;
  buf[offset + 2] = (seconds >> 8) & 0xFF;
  buf[offset + 3] = seconds & 0xFF;
  buf[offset + 4] = (fraction >> 24) & 0xFF;
  buf[offset + 5] = (fraction >> 16) & 0xFF;
  buf[offset + 6] = (fraction >> 8) & 0xFF;
  buf[offset + 7] = fraction & 0xFF;
}

// Send NTP response
void sendNTPResponse(uint8_t* buffer, int len, IPAddress remoteIP, uint16_t remotePort) {
  // Get receive timestamp as early as possible for best accuracy
  uint32_t rxSeconds, rxFraction;
  bool hasPPS = getPPSNTPTimestamp(&rxSeconds, &rxFraction);

  if (rxSeconds == 0) {
    Debug.println("No time available for NTP response");
    return;
  }

  // Prepare response packet
  uint8_t response[48];
  memset(response, 0, 48);

  // LI (leap indicator) = 0, VN (version) = 4, Mode = 4 (server)
  response[0] = 0b00100100;  // LI=0, VN=4, Mode=4

  // Stratum = 1 (primary reference - GPS/RTC)
  response[1] = 1;

  // Poll interval = 6 (64 seconds)
  response[2] = 6;

  // Precision: with PPS = -20 (~1μs), without PPS = -6 (~15ms NMEA jitter)
  response[3] = hasPPS ? 0xEC : 0xFA;  // -20 or -6 in signed 8-bit

  // Root delay = 0 (we are the reference clock)
  response[4] = 0; response[5] = 0; response[6] = 0; response[7] = 0;

  // Root dispersion: with PPS ~15μs (0x0001), without PPS ~500ms (0x8000)
  if (hasPPS) {
    response[8] = 0; response[9] = 0; response[10] = 0x00; response[11] = 0x01;
  } else {
    response[8] = 0; response[9] = 0; response[10] = 0x80; response[11] = 0x00;
  }

  // Reference ID based on time source and PPS
  if (currentTimeSource == TIME_SOURCE_GPS) {
    if (hasPPS) {
      response[12] = 'P'; response[13] = 'P'; response[14] = 'S'; response[15] = '\0';
    } else {
      response[12] = 'G'; response[13] = 'P'; response[14] = 'S'; response[15] = '\0';
    }
  } else {
    response[12] = 'L'; response[13] = 'O'; response[14] = 'C'; response[15] = 'L';
  }

  // Reference timestamp (last PPS edge or last GPS update)
  writeNTPTimestamp(response, 16, rxSeconds, 0);

  // Origin timestamp (copy from request's transmit timestamp)
  memcpy(&response[24], &buffer[40], 8);

  // Receive timestamp (captured at top of function)
  writeNTPTimestamp(response, 32, rxSeconds, rxFraction);

  // Transmit timestamp (capture now, just before sending)
  uint32_t txSeconds, txFraction;
  getPPSNTPTimestamp(&txSeconds, &txFraction);
  writeNTPTimestamp(response, 40, txSeconds, txFraction);

  // Send response
  ntpUdp.beginPacket(remoteIP, remotePort);
  ntpUdp.write(response, 48);
  ntpUdp.endPacket();

  Debug.printf(2, "NTP response sent to %s:%d (source: %s, PPS: %s)\n",
               remoteIP.toString().c_str(), remotePort,
               currentTimeSource == TIME_SOURCE_GPS ? "GPS" : "RTC",
               hasPPS ? "Yes" : "No");
}

// ============== Control Functions ==============

// Enable/disable GPS
void setGPSEnabled(bool enabled) {
  if (enabled == gpsEnabled) {
    return;
  }

  gpsEnabled = enabled;

  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_GPS_ENABLED, gpsEnabled);
  prefs.end();

  if (enabled) {
    initGPS();
  } else {
    // Detach PPS interrupt when disabling GPS
    if (gpsPpsPin >= 0) {
      detachInterrupt(digitalPinToInterrupt(gpsPpsPin));
      ppsActive = false;
      ppsCount = 0;
      ppsUnixTimeAtEdge = 0;
    }

    gpsStatus.enabled = false;
    gpsStatus.hasFix = false;
    gpsStatus.time.valid = false;

    // Fall back to RTC if available
    if (rtcPresent && rtcTime.valid) {
      currentTimeSource = TIME_SOURCE_RTC;
    } else if (currentTimeSource == TIME_SOURCE_GPS) {
      currentTimeSource = TIME_SOURCE_NONE;
      timeSynced = false;
    }

    Debug.println("GPS disabled");
  }
}

// Enable/disable NTP server
void setGPSNtpEnabled(bool enabled) {
  if (enabled == gpsNtpEnabled) {
    return;
  }

  gpsNtpEnabled = enabled;

  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_GPS_NTP_ENABLED, gpsNtpEnabled);
  prefs.end();

  if (enabled) {
    initNTP();
  } else {
    ntpUdp.stop();
    Debug.println("NTP server stopped");
  }
}

// ============== Status Functions ==============

bool isGPSEnabled() {
  return gpsEnabled;
}

bool isGPSNtpEnabled() {
  return gpsNtpEnabled;
}

bool hasGPSFix() {
  return gpsStatus.hasFix;
}

bool isTimeSynced() {
  return timeSynced;
}

bool isPPSActive() {
  return ppsActive;
}

uint32_t getPPSCount() {
  return ppsCount;
}

TimeSource getTimeSource() {
  return currentTimeSource;
}

GPSStatus getGPSStatus() {
  return gpsStatus;
}

// Get formatted time string from best source (HH:MM:SS)
String getTimeString() {
  if (currentTimeSource == TIME_SOURCE_GPS && gpsStatus.time.valid) {
    return getGPSTimeString();
  } else if (currentTimeSource == TIME_SOURCE_RTC && rtcTime.valid) {
    // Calculate interpolated time from RTC
    uint32_t currentUnix = getCurrentUnixTime();
    uint8_t hour = (currentUnix % 86400) / 3600;
    uint8_t minute = (currentUnix % 3600) / 60;
    uint8_t second = currentUnix % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
    return String(buf);
  }
  return "No Sync";
}

// Get formatted date string from best source (YYYY-MM-DD)
String getDateString() {
  if (currentTimeSource == TIME_SOURCE_GPS && gpsStatus.time.valid) {
    return getGPSDateString();
  } else if (currentTimeSource == TIME_SOURCE_RTC && rtcTime.valid) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             rtcTime.year, rtcTime.month, rtcTime.day);
    return String(buf);
  }
  return "No Sync";
}

// Get formatted GPS time string (HH:MM:SS)
String getGPSTimeString() {
  if (!gpsStatus.time.valid) {
    return "No Fix";
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           gpsStatus.time.hour, gpsStatus.time.minute, gpsStatus.time.second);
  return String(buf);
}

// Get formatted GPS date string (YYYY-MM-DD)
String getGPSDateString() {
  if (!gpsStatus.time.valid) {
    return "No Fix";
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           gpsStatus.time.year, gpsStatus.time.month, gpsStatus.time.day);
  return String(buf);
}

// Get Unix timestamp from best available source
uint32_t getCurrentUnixTime() {
  if (currentTimeSource == TIME_SOURCE_GPS && gpsStatus.time.valid) {
    return getGPSUnixTime();
  } else if (currentTimeSource == TIME_SOURCE_RTC && rtcTime.valid) {
    // Interpolate from last RTC read
    unsigned long elapsed = (millis() - lastRTCReadMillis) / 1000;
    return lastRTCUnixTime + elapsed;
  }
  return 0;
}

// Get Unix timestamp from GPS only
uint32_t getGPSUnixTime() {
  if (!gpsStatus.time.valid) {
    return 0;
  }
  uint32_t baseTime = lastGPSUnixTime;
  unsigned long elapsed = (millis() - lastGPSSecondMillis) / 1000;
  return baseTime + elapsed;
}

// Get total timezone offset including DST (in minutes)
int16_t getTotalOffset() {
  return timezoneOffset + (dstEnabled ? 60 : 0);
}

// Get local Unix timestamp (UTC + timezone + DST offset)
uint32_t getLocalUnixTime() {
  uint32_t utcTime = getCurrentUnixTime();
  if (utcTime == 0) return 0;

  int32_t offsetSeconds = getTotalOffset() * 60;
  return utcTime + offsetSeconds;
}

// Get formatted local time string (HH:MM:SS)
String getLocalTimeString() {
  uint32_t localUnix = getLocalUnixTime();
  if (localUnix == 0) {
    return "No Sync";
  }
  uint8_t hour = (localUnix % 86400) / 3600;
  uint8_t minute = (localUnix % 3600) / 60;
  uint8_t second = localUnix % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
  return String(buf);
}

// Get formatted local date string (YYYY-MM-DD)
String getLocalDateString() {
  uint32_t localUnix = getLocalUnixTime();
  if (localUnix == 0) {
    return "No Sync";
  }

  // Convert Unix timestamp to date
  uint32_t days = localUnix / 86400;
  uint16_t year = 1970;

  while (true) {
    uint16_t daysInYear = 365;
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) daysInYear = 366;
    if (days < daysInYear) break;
    days -= daysInYear;
    year++;
  }

  static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  uint8_t month = 0;

  while (month < 12) {
    uint8_t dim = daysInMonth[month];
    if (month == 1 && isLeap) dim = 29;
    if (days < dim) break;
    days -= dim;
    month++;
  }

  char buf[12];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month + 1, (uint8_t)(days + 1));
  return String(buf);
}

// Timezone setting functions
void setTimezoneOffset(int16_t offset) {
  timezoneOffset = offset;

  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putShort(PREF_TIMEZONE_OFFSET, timezoneOffset);
  prefs.end();

  Debug.printf("Timezone offset set to %d minutes\n", timezoneOffset);
}

void setDSTEnabled(bool enabled) {
  dstEnabled = enabled;

  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_DST_ENABLED, dstEnabled);
  prefs.end();

  Debug.printf("DST %s\n", dstEnabled ? "enabled" : "disabled");
}

int16_t getTimezoneOffset() {
  return timezoneOffset;
}

bool isDSTEnabled() {
  return dstEnabled;
}

// ============== NMEA Parsing Functions ==============

static void parseNMEA(const char* sentence) {
  if (sentence[0] != '$') {
    return;
  }
  if (strncmp(sentence + 3, "RMC", 3) == 0) {
    parseGPRMC(sentence);
  }
  else if (strncmp(sentence + 3, "GGA", 3) == 0) {
    parseGPGGA(sentence);
  }
  else if (strncmp(sentence + 3, "GSV", 3) == 0) {
    parseGPGSV(sentence);
  }
}

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
        case 1: strncpy(timeStr, field, sizeof(timeStr) - 1); break;
        case 2: statusChar = field[0]; break;
        case 3: strncpy(latStr, field, sizeof(latStr) - 1); break;
        case 4: latDir = field[0]; break;
        case 5: strncpy(lonStr, field, sizeof(lonStr) - 1); break;
        case 6: lonDir = field[0]; break;
        case 9: strncpy(dateStr, field, sizeof(dateStr) - 1); break;
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

  gpsStatus.hasFix = (statusChar == 'A');

  if (gpsStatus.hasFix && strlen(timeStr) >= 6 && strlen(dateStr) >= 6) {
    gpsStatus.time.hour = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    gpsStatus.time.minute = (timeStr[2] - '0') * 10 + (timeStr[3] - '0');
    gpsStatus.time.second = (timeStr[4] - '0') * 10 + (timeStr[5] - '0');

    uint8_t day = (dateStr[0] - '0') * 10 + (dateStr[1] - '0');
    uint8_t month = (dateStr[2] - '0') * 10 + (dateStr[3] - '0');
    uint8_t year2 = (dateStr[4] - '0') * 10 + (dateStr[5] - '0');

    gpsStatus.time.day = day;
    gpsStatus.time.month = month;
    gpsStatus.time.year = 2000 + year2;
    gpsStatus.time.valid = true;

    if (strlen(latStr) > 0) {
      gpsStatus.latitude = parseLatLon(latStr, latDir);
    }
    if (strlen(lonStr) > 0) {
      gpsStatus.longitude = parseLatLon(lonStr, lonDir);
    }

    lastGPSUnixTime = dateTimeToUnix(gpsStatus.time.year, gpsStatus.time.month,
                                      gpsStatus.time.day, gpsStatus.time.hour,
                                      gpsStatus.time.minute, gpsStatus.time.second);
    lastGPSUpdateMicros = micros();
    lastGPSSecondMillis = millis();
    gpsStatus.lastUpdate = millis();

    Debug.printf(2, "GPS Time: %s %s, Lat: %.6f, Lon: %.6f\n",
                 getGPSDateString().c_str(), getGPSTimeString().c_str(),
                 gpsStatus.latitude, gpsStatus.longitude);
  }
}

static void parseGPGGA(const char* sentence) {
  char* p = (char*)sentence;
  int fieldIndex = 0;
  char field[20];
  int fieldLen = 0;

  while (*p) {
    if (*p == ',' || *p == '*') {
      field[fieldLen] = '\0';
      switch (fieldIndex) {
        case 7:
          if (fieldLen > 0) gpsStatus.satellites = atoi(field);
          break;
        case 8:
          if (fieldLen > 0) gpsStatus.hdop = atof(field);
          break;
        case 9:
          if (fieldLen > 0) gpsStatus.altitude = atof(field);
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

// Parse GPGSV sentence to get satellites in view
static void parseGPGSV(const char* sentence) {
  char* p = (char*)sentence;
  int fieldIndex = 0;
  char field[20];
  int fieldLen = 0;

  while (*p) {
    if (*p == ',' || *p == '*') {
      field[fieldLen] = '\0';
      // Field 3 contains the total number of satellites in view
      if (fieldIndex == 3) {
        if (fieldLen > 0) gpsStatus.satellites_in_view = atoi(field);
        return;  // We only need this field
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

static double parseLatLon(const char* str, char dir) {
  if (strlen(str) == 0) return 0.0;
  const char* dot = strchr(str, '.');
  if (!dot) return 0.0;

  int degLen = (dot - str) - 2;
  if (degLen < 1 || degLen > 3) return 0.0;

  char degStr[4] = "";
  strncpy(degStr, str, degLen);
  degStr[degLen] = '\0';

  double degrees = atof(degStr);
  double minutes = atof(str + degLen);
  double result = degrees + (minutes / 60.0);

  if (dir == 'S' || dir == 'W') result = -result;
  return result;
}

static uint32_t dateTimeToUnix(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second) {
  static const uint16_t daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  uint32_t days = 0;

  for (uint16_t y = 1970; y < year; y++) {
    days += 365;
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) days++;
  }

  days += daysBeforeMonth[month - 1];
  if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) days++;
  days += day - 1;

  uint32_t unixTime = days * 86400UL;
  unixTime += hour * 3600UL;
  unixTime += minute * 60UL;
  unixTime += second;

  return unixTime;
}
