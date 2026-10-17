/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Debug Utility
 *
 * All output is mirrored to an in-RAM ring buffer regardless of DEBUG_LEVEL,
 * so the web console at /console always shows recent log lines even when
 * no serial monitor is available (e.g. remote OTA deployments).
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Debug level control
// 0 = No serial output  (web console ring buffer still active)
// 1 = Basic debug output
// 2 = Verbose debug output
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1  // Default to basic debug level
#endif

// Ring buffer API — implemented in Debug.cpp, used by web_ui_handler
void logAppend(const char* text);
void logGetJSON(String& out);

#define LOG_LINE_MAX 120

// A completed log line on its way to persistent storage (FRAM)
struct LogLine {
  uint32_t utc;
  uint32_t ms;
  char     msg[LOG_LINE_MAX];
};

void logInit();                                  // Create the ring lock (call before starting other tasks)

// Persistence queue: every new line is queued without blocking; the logger task
// receives and writes them. Enabling it replays the lines already in the RAM
// ring so boot messages logged earlier are kept too.
void logEnablePersistQueue(size_t depth);
bool logPersistReceive(LogLine& out, uint32_t waitMs);
uint32_t logPersistPending();
uint32_t logPersistDropped();                    // lines not persisted because the queue was full

// UTC source for timestamps (returns 0 when no time source is synced)
typedef uint32_t (*LogTimeSource)();
void logSetTimeSource(LogTimeSource fn);

class DebugClass {
public:
  void begin(unsigned long baud) {
    logInit();
    #if DEBUG_LEVEL > 0
      Serial.begin(baud);
      while (!Serial && millis() < 5000) { delay(100); }
      Serial.println();
      Serial.println(F("Debug output initialized"));
    #endif
  }

  // print / println — level 1 by default; level 2 = verbose only
  template <typename T>
  void print(T message, int level = 1) {
    if (level <= DEBUG_LEVEL) {
      #if DEBUG_LEVEL > 0
        Serial.print(message);
      #endif
      String s(message);
      logAppend(s.c_str());
    }
  }

  template <typename T>
  void println(T message, int level = 1) {
    if (level <= DEBUG_LEVEL) {
      #if DEBUG_LEVEL > 0
        Serial.println(message);
      #endif
      String s(message);
      s += '\n';
      logAppend(s.c_str());
    }
  }

  // Prefixed variants
  template <typename T>
  void print(const char* prefix, T message, int level = 1) {
    if (level <= DEBUG_LEVEL) {
      #if DEBUG_LEVEL > 0
        Serial.print(prefix); Serial.print(": "); Serial.print(message);
      #endif
      String s = String(prefix) + ": " + String(message);
      logAppend(s.c_str());
    }
  }

  template <typename T>
  void println(const char* prefix, T message, int level = 1) {
    if (level <= DEBUG_LEVEL) {
      #if DEBUG_LEVEL > 0
        Serial.print(prefix); Serial.print(": "); Serial.println(message);
      #endif
      String s = String(prefix) + ": " + String(message) + "\n";
      logAppend(s.c_str());
    }
  }

  // printf — no level (always level 1)
  void printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    #if DEBUG_LEVEL > 0
      Serial.print(buffer);
    #endif
    logAppend(buffer);
  }

  // printf with explicit level
  void printf(int level, const char* format, ...) {
    if (level <= DEBUG_LEVEL) {
      char buffer[256];
      va_list args;
      va_start(args, format);
      vsnprintf(buffer, sizeof(buffer), format, args);
      va_end(args);
      #if DEBUG_LEVEL > 0
        Serial.print(buffer);
      #endif
      logAppend(buffer);
    }
  }
};

extern DebugClass Debug;

#endif // DEBUG_H