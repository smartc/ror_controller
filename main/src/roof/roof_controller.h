/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * Roof Controller Header
 */

#ifndef ROOF_CONTROLLER_H
#define ROOF_CONTROLLER_H

#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Non-blocking state machine for roof operations
// This allows the main loop to continue running WiFi/MQTT while relay sequences execute
enum RoofOperationState {
  OP_IDLE,                    // No operation in progress
  OP_INVERTER_POWER_ON,       // K1 turned on, waiting for delay
  OP_INVERTER_BUTTON_PRESS,   // K3 pressed, waiting 500ms
  OP_INVERTER_BUTTON_RELEASE, // K3 released, waiting 100ms
  OP_INVERTER_DELAY2,         // Waiting delay2 after soft-power
  OP_ROOF_BUTTON_PRESS,       // K2 pressed, waiting 500ms
  OP_ROOF_BUTTON_RELEASE,     // K2 released, operation complete
  OP_STOP_BUTTON_PRESS,       // K2 pressed for stop, waiting 500ms
  OP_STOP_BUTTON_RELEASE,     // K2 released for stop, then shutdown inverter
  OP_SHUTDOWN_K1_WAIT,        // K1 turned off, waiting ~1s before checking AC power
  OP_SHUTDOWN_K3_PRESS,       // AC still on after K1 off, K3 pressed to toggle soft-power off
  OP_SHUTDOWN_K3_RELEASE      // K3 released, shutdown complete
};

// Target direction for current operation
enum RoofOperationTarget {
  TARGET_NONE,
  TARGET_OPEN,
  TARGET_CLOSE,
  TARGET_STOP
};

// State machine variables (extern declarations)
extern RoofOperationState roofOpState;
extern RoofOperationTarget roofOpTarget;
extern unsigned long roofOpStepStartTime;
extern bool roofOpNeedsInverterButton;  // Whether K3 press is needed (AC power not detected)

// Global state variables
extern RoofStatus roofStatus;
extern RoofStatus lastPublishedStatus; // Track last published status
extern bool slaved;
extern unsigned long lastSwitchTime;
extern unsigned long movementStartTime;
extern bool swapLimitSwitches;  // Flag to swap open/closed limit switch pins
extern bool lastOpenSwitchState;
extern bool lastClosedSwitchState;
extern unsigned long lastOpenStateTime;
extern unsigned long lastClosedStateTime;
extern bool bypassParkSensor;
extern bool lastTelescopeParkedState;
extern unsigned long lastTelescopeParkedStateTime;
extern bool telescopeParked;

// Error tracking for ASCOM compliance (error reason for Slewing exception)
extern String roofErrorReason;

// Inverter power state variables (NEW in v3)
extern bool inverterRelayState;      // State of K1 (12V power relay)
extern bool inverterACPowerState;    // State of AC power (detected via optocoupler)
extern bool lastInverterACPowerState; // Last AC power state for change detection
extern unsigned long lastInverterACPowerChangeTime;
extern bool inverterRelayEnabled;    // Enable K1 power relay control for roof movement
extern bool inverterSoftPwrEnabled;  // Enable K3 soft-power button control for roof movement
extern bool roofMotorCommandedRunning; // True from K2 press (movement start) until stop, limit switch, or timeout

// Function prototypes
void initializeRoofController();
void updateRoofStatus();
void checkMovementTimeout();
bool startOpeningRoof();
bool startClosingRoof();
bool stopRoofMovement(bool updateStatus = true);
void sendButtonPress();
void applyPinSettings();  // Function to apply pin settings
void determineInitialRoofStatus();
void updateTelescopeStatus();  // Function to update telescope park status
void clearRoofError();         // Clear error state and reason (for recovery)
bool roofStoppedMidTravel();   // ERROR because a stop left the roof between the limits (not a fault)
void processRoofOperation();        // Non-blocking state machine - call from main loop
void checkWeatherAutoClose();       // Auto-close roof on unsafe weather (call from main loop)
void checkParkSensorInterlock();    // Emergency stop if telescope unparks during roof movement (call from main loop)

// ── Roof task (v4) ──────────────────────────────────────────────────────────
// All roof monitoring (limit switches, park interlock, timeouts, relay sequences,
// weather auto-close) runs in a dedicated high-priority task every 10 ms, so a
// slow web request, MQTT connect or log flush can never delay it.
//
// Roof state is guarded by one recursive mutex. The public roof functions
// (startOpeningRoof, stopRoofMovement, ...) take it themselves; code that
// changes several roof settings at once wraps the change in a RoofLock.
// Never hold the lock across network, flash or I2C I/O.
SemaphoreHandle_t roofLockHandle();

class RoofLock {
public:
  RoofLock()  { if (roofLockHandle()) xSemaphoreTakeRecursive(roofLockHandle(), portMAX_DELAY); }
  ~RoofLock() { if (roofLockHandle()) xSemaphoreGiveRecursive(roofLockHandle()); }
  RoofLock(const RoofLock&) = delete;
  RoofLock& operator=(const RoofLock&) = delete;
};

void startRoofTask();                 // Call once the roof hardware is initialised
uint32_t roofTaskTicks();             // Diagnostics: ticks run so far
uint32_t roofTaskMaxTickUs();         // Diagnostics: longest tick (incl. lock wait) in us
bool roofTakeChanged();               // True once after any roof state change (loop task publishes MQTT)
String getRoofErrorReason();          // Thread-safe copy of roofErrorReason

// Network-fed safety inputs (UDP park sensors, weather). The loop task refreshes
// them every pass; if they go stale (loop stalled) the roof task lets a move in
// progress finish, holds the last known park state, and refuses new moves.
void roofUpdateNetInputs(bool udpParked, bool weatherSafe);
bool roofNetInputsFresh();
const unsigned long NET_INPUT_STALE_MS = 5000;

// Inverter control functions (NEW in v3)
void toggleInverterPower();           // Toggle K1 inverter power relay
void sendInverterButtonPress();       // Send K3 soft-power button press
bool getInverterRelayState();         // Get state of K1 relay
bool getInverterACPowerState();       // Get state of AC power (via optocoupler)
void updateInverterPowerStatus();     // Update and monitor inverter AC power state
void shutdownInverterPower();         // Non-blocking inverter shutdown: K1 off, check AC, toggle K3 if needed

#endif // ROOF_CONTROLLER_H