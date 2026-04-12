/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Debug Utility
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Debug level control
// 0 = No debug output
// 1 = Basic debug output
// 2 = Verbose debug output
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1  // Default to basic debug level
#endif

class DebugClass {
public:
  // Initialize debug output
  void begin(unsigned long baud) {
    #if DEBUG_LEVEL > 0
      Serial.begin(baud);
      while (!Serial && millis() < 5000) {
        // Wait for serial connection for up to 5 seconds
        delay(100);
      }
      Serial.println();
      Serial.println(F("Debug output initialized"));
    #endif
  }
  
  // Print methods for different debug levels
  template <typename T>
  void print(T message, int level = 1) {
    #if DEBUG_LEVEL > 0
      if (level <= DEBUG_LEVEL) {
        Serial.print(message);
      }
    #endif
  }
  
  template <typename T>
  void println(T message, int level = 1) {
    #if DEBUG_LEVEL > 0
      if (level <= DEBUG_LEVEL) {
        Serial.println(message);
      }
    #endif
  }
  
  // Print methods with prefix for organization
  template <typename T>
  void print(const char* prefix, T message, int level = 1) {
    #if DEBUG_LEVEL > 0
      if (level <= DEBUG_LEVEL) {
        Serial.print(prefix);
        Serial.print(": ");
        Serial.print(message);
      }
    #endif
  }
  
  template <typename T>
  void println(const char* prefix, T message, int level = 1) {
    #if DEBUG_LEVEL > 0
      if (level <= DEBUG_LEVEL) {
        Serial.print(prefix);
        Serial.print(": ");
        Serial.println(message);
      }
    #endif
  }
  
  // For formatting like printf
  void printf(const char* format, ...) {
    #if DEBUG_LEVEL > 0
      char buffer[256];
      va_list args;
      va_start(args, format);
      vsnprintf(buffer, sizeof(buffer), format, args);
      va_end(args);
      Serial.print(buffer);
    #endif
  }
  
  void printf(int level, const char* format, ...) {
    #if DEBUG_LEVEL > 0
      if (level <= DEBUG_LEVEL) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.print(buffer);
      }
    #endif
  }
};

// Create a global instance
extern DebugClass Debug;

#endif // DEBUG_H