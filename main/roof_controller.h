/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
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

// Function prototypes
void initializeRoofController();
void updateRoofStatus();
void checkMovementTimeout();
bool startOpeningRoof();
bool startClosingRoof();
bool stopRoofMovement();
void sendButtonPress();
void applyPinSettings();  // Function to apply pin settings
void determineInitialRoofStatus();
void updateTelescopeStatus();  // Function to update telescope park status

#endif // ROOF_CONTROLLER_H