/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Roof Controller Implementation
 */

#include "roof_controller.h"
#include "mqtt_handler.h"
#include "park_sensor_udp.h"
#include "Debug.h"
#include <Arduino.h>

// Define the global variables declared as extern in config.h
int LIMIT_SWITCH_OPEN_PIN = DEFAULT_OPEN_SWITCH_PIN;    // Default to pin 34
int LIMIT_SWITCH_CLOSED_PIN = DEFAULT_CLOSED_SWITCH_PIN; // Default to pin 35
int TRIGGERED = DEFAULT_TRIGGER_STATE;                  // Default to LOW trigger state
int TELESCOPE_PARKED = DEFAULT_PARK_STATE;              // Park sensor is HIGH until triggered

// Limit switch states
bool lastOpenSwitchState = false;
bool lastClosedSwitchState = false;
bool bypassParkSensor = false;
bool lastTelescopeParkedState = false;
unsigned long lastTelescopeParkedStateTime = 0;
bool telescopeParked = false;
unsigned long lastOpenStateTime = 0;
unsigned long lastClosedStateTime = 0;

// Current device state
RoofStatus roofStatus = ROOF_CLOSED;
RoofStatus lastPublishedStatus = ROOF_CLOSED;   // Track last published status for change detection
bool slaved = false;
bool isConnected = true;                        // Device is always connected in this implementation
bool swapLimitSwitches = false;                 // Flag for swapping limit switch pins

// Timestamps for various operations
unsigned long lastSwitchTime = 0;
unsigned long movementStartTime = 0;

// Apply pin settings - useful after changing pin assignments or trigger state
void applyPinSettings() {
  // Configure input pins with internal pull-ups
  pinMode(TELESCOPE_PARKED_PIN, INPUT);
  pinMode(LIMIT_SWITCH_OPEN_PIN, INPUT_PULLUP);
  pinMode(LIMIT_SWITCH_CLOSED_PIN, INPUT_PULLUP);
  
  Debug.println("Pin settings applied:");
  Debug.print("TELESCOPE_PARKED_PIN: "); Debug.println(TELESCOPE_PARKED_PIN);
  Debug.print("LIMIT_SWITCH_OPEN_PIN: "); Debug.println(LIMIT_SWITCH_OPEN_PIN);
  Debug.print("LIMIT_SWITCH_CLOSED_PIN: "); Debug.println(LIMIT_SWITCH_CLOSED_PIN);
  Debug.print("TRIGGERED state: "); Debug.println(TRIGGERED == HIGH ? "HIGH" : "LOW");
  Debug.print("TELESCOPE_PARKED state: "); Debug.println(TELESCOPE_PARKED == HIGH ? "HIGH" : "LOW");
  
  // Immediately read and report the state of the telescope park pin
  int parkPinValue = digitalRead(TELESCOPE_PARKED_PIN);
  bool isParked = (parkPinValue == TELESCOPE_PARKED);
  Debug.print("Current TELESCOPE_PARKED_PIN value: "); Debug.println(parkPinValue);
  Debug.print("Telescope is currently interpreted as: "); Debug.println(isParked ? "PARKED" : "NOT PARKED");
}

void initializeRoofController() {
  // Initialize GPIO pins
  
  // Configure power inverter relay
  pinMode(INVERTER_PIN, OUTPUT);
  digitalWrite(INVERTER_PIN, LOW);  // Start with inverter OFF
  
  // Configure roof control button relay
  pinMode(ROOF_CONTROL_PIN, OUTPUT);
  digitalWrite(ROOF_CONTROL_PIN, LOW);  // Start with button NOT pressed
  
  // Apply pin settings
  applyPinSettings();

  // Print initial pin states for debugging
  Debug.println("Initial pin states:");
  Debug.print("INVERTER_PIN: "); Debug.println(digitalRead(INVERTER_PIN));
  Debug.print("ROOF_CONTROL_PIN: "); Debug.println(digitalRead(ROOF_CONTROL_PIN));
  Debug.print("TELESCOPE_PARKED_PIN: "); Debug.println(digitalRead(TELESCOPE_PARKED_PIN));
  Debug.print("LIMIT_SWITCH_OPEN_PIN: "); Debug.println(digitalRead(LIMIT_SWITCH_OPEN_PIN));
  Debug.print("LIMIT_SWITCH_CLOSED_PIN: "); Debug.println(digitalRead(LIMIT_SWITCH_CLOSED_PIN));
  Debug.print("Trigger state: "); Debug.println(TRIGGERED == HIGH ? "HIGH" : "LOW");
  Debug.print("Switches swapped: "); Debug.println(swapLimitSwitches ? "YES" : "NO");
  
  // Allow time for the pull-up resistors to fully settle
  delay(50);
  
  // Initialize status variables for debouncing
  lastOpenSwitchState = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED);
  lastClosedSwitchState = (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED);
  lastOpenStateTime = millis() - SWITCH_STABLE_TIME - 1; // Make initial reading valid immediately
  lastClosedStateTime = millis() - SWITCH_STABLE_TIME - 1;

  // Initialize telescope park state
  lastTelescopeParkedStateTime = millis() - SWITCH_STABLE_TIME - 1; // Make initial reading valid immediately
  updateTelescopeStatus();
  
  // Explicitly check initial roof status based on limit switches with extra care
  determineInitialRoofStatus();
}

void determineInitialRoofStatus() {
  // Read switch states multiple times to ensure stability
  int openTriggeredCount = 0;
  int closedTriggeredCount = 0;
  
  // Sample switch states multiple times to determine reliable initial state
  for (int i = 0; i < 5; i++) {
    if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED) openTriggeredCount++;
    if (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED) closedTriggeredCount++;
    delay(10);
  }
  
  Debug.print("Initial sampling - Open switch triggered: ");
  Debug.print(openTriggeredCount);
  Debug.print("/5, Closed switch triggered: ");
  Debug.println(closedTriggeredCount);
  
  // Determine status based on majority readings
  if (openTriggeredCount >= 3 && closedTriggeredCount >= 3) {
    // Both switches triggered is an error condition
    roofStatus = ROOF_ERROR;
    Debug.println("INITIAL STATUS: ERROR - Both limit switches triggered!");
  } 
  else if (openTriggeredCount >= 3) {
    // Roof is open
    roofStatus = ROOF_OPEN;
    Debug.println("INITIAL STATUS: Roof is OPEN");
  } 
  else if (closedTriggeredCount >= 3) {
    // Roof is closed
    roofStatus = ROOF_CLOSED;
    Debug.println("INITIAL STATUS: Roof is CLOSED");
  } 
  else {
    // Roof is in between - assume it's stationary
    roofStatus = ROOF_ERROR;  // Use ERROR state for an in-between position that's not moving
    Debug.println("INITIAL STATUS: Roof is IN BETWEEN (not at either limit)");
  }
  
  // Set the last published status
  lastPublishedStatus = roofStatus;
  
  // Publish initial status
  publishStatusToMQTT();
}

// Update roof status based on limit switches
void updateRoofStatus() {
  unsigned long currentTime = millis();
  
  // Check whether we have just started moving the roof.  If so, give it time before we revise the roof state.
  if (currentTime - movementStartTime < 5000) {
    return;     // Movement started less than 5 seconds ago.  Let's wait!
  }

  // Read current switch states
  bool currentOpenState = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED);
  bool currentClosedState = (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED);
  
  // Check if open switch state changed
  if (currentOpenState != lastOpenSwitchState) {
    lastOpenSwitchState = currentOpenState;
    lastOpenStateTime = currentTime;
    Debug.println("Open switch state changed to: " + String(currentOpenState ? "TRIGGERED" : "NOT TRIGGERED"));
  }
  
  // Check if closed switch state changed
  if (currentClosedState != lastClosedSwitchState) {
    lastClosedSwitchState = currentClosedState;
    lastClosedStateTime = currentTime;
    Debug.println("Closed switch state changed to: " + String(currentClosedState ? "TRIGGERED" : "NOT TRIGGERED"));
  }
  
  // Only consider a switch triggered if its state has been stable for SWITCH_STABLE_TIME
  bool isOpenLimitTriggered = currentOpenState && (currentTime - lastOpenStateTime > SWITCH_STABLE_TIME);
  bool isClosedLimitTriggered = currentClosedState && (currentTime - lastClosedStateTime > SWITCH_STABLE_TIME);
  
  // Save previous status for change detection
  RoofStatus previousStatus = roofStatus;
  String statusMessage = "";
  
  // Handle clear terminal states first
  if (isOpenLimitTriggered && isClosedLimitTriggered) {
    // Both switches triggered is an error condition
    roofStatus = ROOF_ERROR;
    digitalWrite(INVERTER_PIN, LOW); // Turn off inverter for safety
    statusMessage = "ERROR: Both limit switches triggered!";
  } 
  else if (isOpenLimitTriggered) {
    // If the open switch is triggered, the roof is open regardless of previous state
    if (roofStatus != ROOF_OPEN) {
      statusMessage = "Roof fully open";
      digitalWrite(INVERTER_PIN, LOW); // Turn off inverter
    }
    roofStatus = ROOF_OPEN;
  } 
  else if (isClosedLimitTriggered) {
    // If the closed switch is triggered, the roof is closed regardless of previous state
    if (roofStatus != ROOF_CLOSED) {
      statusMessage = "Roof fully closed";
      digitalWrite(INVERTER_PIN, LOW); // Turn off inverter
    }
    roofStatus = ROOF_CLOSED;
  } 
  else {
    // Neither limit switch is triggered - roof is in between
    
    // Only transition to moving states if we're not already in a moving state
    // This prevents oscillation between OPENING and CLOSING
    if (roofStatus == ROOF_OPEN) {
      roofStatus = ROOF_CLOSING;
      movementStartTime = currentTime;
      statusMessage = "Roof state changed from OPEN, now CLOSING";
    } 
    else if (roofStatus == ROOF_CLOSED) {
      roofStatus = ROOF_OPENING;
      movementStartTime = currentTime;
      statusMessage = "Roof state changed from CLOSED, now OPENING";
    }
    // Otherwise, maintain the current state (OPENING, CLOSING, or ERROR)
  }
  
  // If status has changed, publish to MQTT immediately
  if (roofStatus != previousStatus || roofStatus != lastPublishedStatus) {
    // Only print status messages when there's a change
    if (statusMessage.length() > 0) {
      Debug.println(statusMessage);
    }
    
    Debug.print("Roof status changed from ");
    Debug.print(getRoofStatusString(previousStatus));
    Debug.print(" to ");
    Debug.println(getRoofStatusString(roofStatus));
    
    publishStatusToMQTT();
    lastPublishedStatus = roofStatus;
  }
}

// Check for movement timeout
void checkMovementTimeout() {
  // Check for timeout during roof movement
  if ((roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING) && 
      (millis() - movementStartTime > MOVEMENT_TIMEOUT)) {
    
    // Stop the roof due to timeout
    Debug.println("Roof movement timed out!");
    stopRoofMovement();
    roofStatus = ROOF_ERROR;
    
    // Publish status change due to timeout
    publishStatusToMQTT();
    lastPublishedStatus = roofStatus;
  }
}

// Complete startOpeningRoof() function
bool startOpeningRoof() {
  // Check if roof is already open
  if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED) {
    roofStatus = ROOF_OPEN;
    return true;
  }
  
  // Check if we're currently moving
  if (roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING) {
    return false; // Already in motion
  }
  
  // Check telescope safety interlock - only if bypass is not enabled
  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("Cannot open roof: Telescope not parked and bypass not enabled");
    return false; // Telescope not parked and bypass not enabled
  } 
  
  // Turn on the inverter
  digitalWrite(INVERTER_PIN, HIGH);
  Debug.println("Inverter turned ON");
  delay(1000); // Give inverter time to start
  
  // Send a button press to the roof controller
  sendButtonPress();
  
  // Update roof status
  roofStatus = ROOF_OPENING;
  movementStartTime = millis();
  
  // Publish status change immediately
  publishStatusToMQTT();
  lastPublishedStatus = roofStatus;
  
  Debug.println("Roof opening started");
  return true;
}

// Complete startClosingRoof() function
bool startClosingRoof() {
  // Check if roof is already closed
  if (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED) {
    roofStatus = ROOF_CLOSED;
    return true;
  }
  
  // Check if we're currently moving
  if (roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING) {
    return false; // Already in motion
  }
  
  // Check telescope safety interlock - only if bypass is not enabled
  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("Cannot close roof: Telescope not parked and bypass not enabled");
    return false; // Telescope not parked and bypass not enabled
  } 
    
  // Turn on the inverter
  digitalWrite(INVERTER_PIN, HIGH);
  Debug.println("Inverter turned ON");
  delay(1000); // Give inverter time to start
  
  // Send a button press to the roof controller
  sendButtonPress();
  
  // Update roof status
  roofStatus = ROOF_CLOSING;
  movementStartTime = millis();
  
  // Publish status change immediately
  publishStatusToMQTT();
  lastPublishedStatus = roofStatus;
  
  Debug.println("Roof closing started");
  return true;
}

// Stop roof movement
bool stopRoofMovement() {
  // Send a button press to stop the movement
  sendButtonPress();
  
  // Update status based on limit switches
  updateRoofStatus();
  
  // Turn off the inverter if we've reached a limit
  digitalWrite(INVERTER_PIN, LOW);
  Debug.println("Inverter turned OFF");
  
  // Publish status change immediately
  publishStatusToMQTT();
  lastPublishedStatus = roofStatus;
  
  Debug.println("Roof movement stopped");
  return true;
}

// Send a button press to the roof controller
void sendButtonPress() {
  // For a normally open relay:
  // LOW = Relay not energized = Button NOT pressed
  // HIGH = Relay energized = Button pressed
  
  // Before button press
  Debug.print("Button relay before: ");
  Debug.println(digitalRead(ROOF_CONTROL_PIN) ? "HIGH (pressed)" : "LOW (not pressed)");
  
  // Press button (energize relay)
  digitalWrite(ROOF_CONTROL_PIN, HIGH);
  Debug.println("Button PRESSED (relay energized)");
  delay(500); // Hold the button for half a second
  
  // Release button (de-energize relay)
  digitalWrite(ROOF_CONTROL_PIN, LOW);
  Debug.println("Button RELEASED (relay de-energized)");
  
  // After button press
  Debug.print("Button relay after: ");
  Debug.println(digitalRead(ROOF_CONTROL_PIN) ? "HIGH (pressed)" : "LOW (not pressed)");
  
  // We need to debounce after a button press
  lastSwitchTime = millis();
}

// Update telescope park status
void updateTelescopeStatus() {
  unsigned long currentTime = millis();
  bool currentParkedState = false;
  
  // Determine telescope parked status based on park sensor type
  switch (parkSensorType) {
    case PARK_SENSOR_PHYSICAL:
      // Use only physical park sensor circuit
      {
        int parkPinValue = digitalRead(TELESCOPE_PARKED_PIN);
        currentParkedState = (parkPinValue == TELESCOPE_PARKED);
      }
      break;
      
    case PARK_SENSOR_UDP:
      // Use only UDP park sensors
      currentParkedState = isTelescopeParkedUDP();
      break;
      
    case PARK_SENSOR_BOTH:
      // Use both - both must indicate parked (AND logic)
      {
        int parkPinValue = digitalRead(TELESCOPE_PARKED_PIN);
        bool physicalParked = (parkPinValue == TELESCOPE_PARKED);
        bool udpParked = isTelescopeParkedUDP();
        currentParkedState = physicalParked && udpParked;
      }
      break;
  }
  
  // Debug output every 10 seconds
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime > 10000) {
    if (parkSensorType == PARK_SENSOR_PHYSICAL) {
      int parkPinValue = digitalRead(TELESCOPE_PARKED_PIN);
      Debug.printf(2, "TELESCOPE_PARKED_PIN value: %d, TELESCOPE_PARKED reference: %d, Interpreted as: %s\n", 
                   parkPinValue, TELESCOPE_PARKED, currentParkedState ? "PARKED" : "NOT PARKED");
    } else if (parkSensorType == PARK_SENSOR_UDP) {
      Debug.printf(2, "UDP Park Sensors: %s\n", currentParkedState ? "PARKED" : "NOT PARKED");
    } else if (parkSensorType == PARK_SENSOR_BOTH) {
      int parkPinValue = digitalRead(TELESCOPE_PARKED_PIN);
      bool physicalParked = (parkPinValue == TELESCOPE_PARKED);
      bool udpParked = isTelescopeParkedUDP();
      Debug.printf(2, "Physical: %s, UDP: %s, Combined: %s\n", 
                   physicalParked ? "PARKED" : "NOT PARKED",
                   udpParked ? "PARKED" : "NOT PARKED",
                   currentParkedState ? "PARKED" : "NOT PARKED");
    }
    lastDebugTime = currentTime;
  }
  
  // Check if telescope park state changed
  if (currentParkedState != lastTelescopeParkedState) {
    lastTelescopeParkedState = currentParkedState;
    lastTelescopeParkedStateTime = currentTime;
    Debug.println("Telescope park state CHANGED to: " + String(currentParkedState ? "PARKED" : "NOT PARKED"));
  }
  
  // Only consider the state change valid if it has been stable for SWITCH_STABLE_TIME
  if (currentTime - lastTelescopeParkedStateTime > SWITCH_STABLE_TIME) {
    if (telescopeParked != currentParkedState) {
      telescopeParked = currentParkedState;
      Debug.println("Telescope parked status UPDATED to: " + String(telescopeParked ? "PARKED" : "NOT PARKED"));
      // Publish status to MQTT if telescope park state changed
      publishStatusToMQTT();
    }
  }
}