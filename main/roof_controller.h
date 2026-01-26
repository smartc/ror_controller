/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * Roof Controller Header
 */

#ifndef ROOF_CONTROLLER_H
#define ROOF_CONTROLLER_H

#include "config.h"

// Global state variables
extern RoofStatus roofStatus;
extern RoofStatus lastPublishedStatus; // Track last published status
extern bool slaved;
extern bool isConnected;
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

// Inverter control functions (NEW in v3)
void toggleInverterPower();           // Toggle K1 inverter power relay
void sendInverterButtonPress();       // Send K3 soft-power button press
bool getInverterRelayState();         // Get state of K1 relay
bool getInverterACPowerState();       // Get state of AC power (via optocoupler)
void updateInverterPowerStatus();     // Update and monitor inverter AC power state

#endif // ROOF_CONTROLLER_H