/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v3)
 * Roof Controller Implementation
 */

#include "roof_controller.h"
#include "mqtt_handler.h"
#include "park_sensor_udp.h"
#include "Debug.h"
#include <Arduino.h>

// Define the global variables declared as extern in config.h
int LIMIT_SWITCH_OPEN_PIN = DEFAULT_OPEN_SWITCH_PIN;    // Default to pin 35
int LIMIT_SWITCH_CLOSED_PIN = DEFAULT_CLOSED_SWITCH_PIN; // Default to pin 36
int TRIGGERED = DEFAULT_TRIGGER_STATE;                  // Default to LOW trigger state
int TELESCOPE_PARKED = DEFAULT_PARK_STATE;              // Park sensor is HIGH until triggered
unsigned long movementTimeout = DEFAULT_MOVEMENT_TIMEOUT; // Movement timeout in ms
bool movementTimeoutEnabled = DEFAULT_TIMEOUT_ENABLED;  // Timeout monitoring enabled (default: true)
unsigned long limitSwitchTimeout = DEFAULT_LIMIT_SWITCH_TIMEOUT; // Limit switch change timeout (default: 5 seconds)
bool limitSwitchTimeoutEnabled = DEFAULT_LIMIT_SWITCH_TIMEOUT_ENABLED; // Limit switch timeout monitoring enabled (default: true)
unsigned long inverterDelay1 = DEFAULT_INVERTER_DELAY1; // Delay between K1 and K3 (default: 750ms)
unsigned long inverterDelay2 = DEFAULT_INVERTER_DELAY2; // Delay between inverter power-on and K2 (default: 1500ms)

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

// Error tracking for ASCOM compliance (error reason for Slewing exception)
String roofErrorReason = "";

// Timestamps for various operations
unsigned long lastSwitchTime = 0;
unsigned long movementStartTime = 0;

// Non-blocking state machine variables
RoofOperationState roofOpState = OP_IDLE;
RoofOperationTarget roofOpTarget = TARGET_NONE;
unsigned long roofOpStepStartTime = 0;
bool roofOpNeedsInverterButton = false;

// Inverter power state variables (NEW in v3)
bool inverterRelayState = false;                // State of K1 (12V power relay)
bool inverterACPowerState = false;              // State of AC power (detected via optocoupler)
bool lastInverterACPowerState = false;          // Last AC power state for change detection
unsigned long lastInverterACPowerChangeTime = 0;
bool inverterRelayEnabled = true;               // Enable K1 power relay control for roof movement (default: enabled)
bool inverterSoftPwrEnabled = true;             // Enable K3 soft-power button control for roof movement (default: enabled)

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

  // Configure power inverter relay (K1)
  pinMode(INVERTER_PIN, OUTPUT);
  digitalWrite(INVERTER_PIN, LOW);  // Start with inverter OFF
  inverterRelayState = false;

  // Configure roof control button relay (K2)
  pinMode(ROOF_CONTROL_PIN, OUTPUT);
  digitalWrite(ROOF_CONTROL_PIN, LOW);  // Start with button NOT pressed

  // Configure inverter soft-power button relay (K3) - NEW in v3
  pinMode(INVERTER_BUTTON_PIN, OUTPUT);
  digitalWrite(INVERTER_BUTTON_PIN, LOW);  // Start with button NOT pressed

  // Configure AC power detection input - NEW in v3
  pinMode(INVERTER_AC_POWER_PIN, INPUT);
  inverterACPowerState = (digitalRead(INVERTER_AC_POWER_PIN) == LOW);
  lastInverterACPowerState = inverterACPowerState;
  lastInverterACPowerChangeTime = millis() - SWITCH_STABLE_TIME - 1;

  // Apply pin settings
  applyPinSettings();

  // Print initial pin states for debugging
  Debug.println("Initial pin states:");
  Debug.print("INVERTER_PIN (K1): "); Debug.println(digitalRead(INVERTER_PIN));
  Debug.print("ROOF_CONTROL_PIN (K2): "); Debug.println(digitalRead(ROOF_CONTROL_PIN));
  Debug.print("INVERTER_BUTTON_PIN (K3): "); Debug.println(digitalRead(INVERTER_BUTTON_PIN));
  Debug.print("INVERTER_AC_POWER_PIN: "); Debug.println(digitalRead(INVERTER_AC_POWER_PIN));
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
    roofErrorReason = "Controller startup detected both limit switches triggered. "
                      "This indicates a hardware fault - check limit switch wiring and mechanical alignment.";
    roofStatus = ROOF_ERROR;
    Debug.println("INITIAL STATUS: ERROR - Both limit switches triggered!");
  }
  else if (openTriggeredCount >= 3) {
    // Roof is open
    roofErrorReason = "";  // Clear any previous error
    roofStatus = ROOF_OPEN;
    Debug.println("INITIAL STATUS: Roof is OPEN");
  }
  else if (closedTriggeredCount >= 3) {
    // Roof is closed
    roofErrorReason = "";  // Clear any previous error
    roofStatus = ROOF_CLOSED;
    Debug.println("INITIAL STATUS: Roof is CLOSED");
  }
  else {
    // Roof is in between - assume it's stationary
    roofErrorReason = "Controller startup detected roof in unknown position (neither limit switch triggered). "
                      "Manual intervention may be required to move roof to a known position.";
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
  if (currentTime - movementStartTime < limitSwitchTimeout) {
    return;     // Movement started recently.  Let's wait for limit switch state to change!
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
    roofErrorReason = "Both open and closed limit switches are triggered simultaneously. "
                      "This indicates a hardware fault - check limit switch wiring and mechanical alignment.";
    roofStatus = ROOF_ERROR;
    digitalWrite(INVERTER_PIN, LOW); // Turn off inverter for safety
    inverterRelayState = false;
    statusMessage = "ERROR: Both limit switches triggered!";
  }
  else if (isOpenLimitTriggered) {
    // Open switch is triggered.
    if (roofStatus == ROOF_CLOSING) {
      // We're trying to CLOSE but open switch is still triggered after limitSwitchTimeout.
      // This means the roof failed to START moving - immediate error.
      roofErrorReason = "Roof failed to start closing. Open limit switch still triggered after " +
                        String(limitSwitchTimeout / 1000) + " seconds. Check motor, relay, or mechanical obstruction.";
      statusMessage = "ERROR: Roof failed to start closing";
      Debug.println("Error reason: " + roofErrorReason);
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      roofStatus = ROOF_ERROR;
    }
    else if (roofStatus == ROOF_OPENING) {
      // We were opening and reached the open position - success!
      statusMessage = "Roof fully open";
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      roofStatus = ROOF_OPEN;
    }
    else if (roofStatus == ROOF_ERROR) {
      // Stay in ERROR state - don't auto-clear just because limit switch is triggered.
      // Error must be explicitly cleared via clearRoofError() or by starting a new movement.
    }
    else if (roofStatus != ROOF_OPEN) {
      // Transitioning to OPEN from CLOSED state (manual movement detected)
      statusMessage = "Roof fully open";
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      roofStatus = ROOF_OPEN;
    }
  }
  else if (isClosedLimitTriggered) {
    // Closed switch is triggered.
    if (roofStatus == ROOF_OPENING) {
      // We're trying to OPEN but closed switch is still triggered after limitSwitchTimeout.
      // This means the roof failed to START moving - immediate error.
      roofErrorReason = "Roof failed to start opening. Closed limit switch still triggered after " +
                        String(limitSwitchTimeout / 1000) + " seconds. Check motor, relay, or mechanical obstruction.";
      statusMessage = "ERROR: Roof failed to start opening";
      Debug.println("Error reason: " + roofErrorReason);
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      roofStatus = ROOF_ERROR;
    }
    else if (roofStatus == ROOF_CLOSING) {
      // We were closing and reached the closed position - success!
      statusMessage = "Roof fully closed";
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      roofStatus = ROOF_CLOSED;
    }
    else if (roofStatus == ROOF_ERROR) {
      // Stay in ERROR state - don't auto-clear just because limit switch is triggered.
      // Error must be explicitly cleared via clearRoofError() or by starting a new movement.
    }
    else if (roofStatus != ROOF_CLOSED) {
      // Transitioning to CLOSED from OPEN state (manual movement detected)
      statusMessage = "Roof fully closed";
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      roofStatus = ROOF_CLOSED;
    }
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
  // Skip timeout check if monitoring is disabled
  if (!movementTimeoutEnabled) {
    return;
  }

  // Check for timeout during roof movement
  if ((roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING) &&
      (millis() - movementStartTime > movementTimeout)) {

    // Set error reason for ASCOM Slewing exception
    String operation = (roofStatus == ROOF_OPENING) ? "open" : "close";
    roofErrorReason = "Roof movement timed out after " + String(movementTimeout / 1000) +
                      " seconds while trying to " + operation +
                      ". Limit switch did not trigger. Check mechanical obstruction or motor failure.";

    // IMPORTANT: Set error state BEFORE stopping to avoid race condition.
    // If we call stopRoofMovement() first, it calls updateRoofStatus() which might
    // set roofStatus to ROOF_CLOSED (based on limit switches), creating a window
    // where ASCOM clients polling Slewing would see false instead of an exception.
    roofStatus = ROOF_ERROR;

    // Stop the roof due to timeout (don't update status - we already set ERROR)
    Debug.println("Roof movement timed out!");
    Debug.println("Error reason: " + roofErrorReason);
    stopRoofMovement(false);  // Pass false to skip status update

    // Publish status change due to timeout
    publishStatusToMQTT();
    lastPublishedStatus = roofStatus;
  }
}

// Complete startOpeningRoof() function - NON-BLOCKING version
// This initiates the roof opening sequence using a state machine.
// The actual relay operations are performed by processRoofOperation() in the main loop.
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

  // Check if an operation is already in progress
  if (roofOpState != OP_IDLE) {
    Debug.println("Roof operation already in progress");
    return false;
  }

  // Check telescope safety interlock - only if bypass is not enabled
  Debug.println("=== ROOF OPENING SAFETY CHECK ===");
  Debug.printf("bypassParkSensor: %s\n", bypassParkSensor ? "TRUE (bypass enabled)" : "FALSE (bypass disabled)");
  Debug.printf("telescopeParked: %s\n", telescopeParked ? "TRUE (parked)" : "FALSE (not parked)");
  Debug.printf("Park sensor type: %d (0=Physical, 1=UDP, 2=Both)\n", parkSensorType);

  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("SAFETY CHECK FAILED: Telescope not parked and bypass not enabled");
    Debug.println("=== ROOF OPENING BLOCKED ===");
    return false; // Telescope not parked and bypass not enabled
  }

  Debug.println("SAFETY CHECK PASSED: Opening roof (non-blocking)");

  // Set target direction
  roofOpTarget = TARGET_OPEN;

  // Determine if we need the soft-power button press
  if (inverterSoftPwrEnabled) {
    inverterACPowerState = (digitalRead(INVERTER_AC_POWER_PIN) == LOW);
    roofOpNeedsInverterButton = !inverterACPowerState;
    Debug.printf("AC power detected: %s, will %spress K3\n",
                 inverterACPowerState ? "YES" : "NO",
                 roofOpNeedsInverterButton ? "" : "NOT ");
  } else {
    roofOpNeedsInverterButton = false;
  }

  // Start the state machine
  if (inverterRelayEnabled) {
    // Step 1: Turn on K1 power relay
    Debug.println("Inverter relay enabled - turning on K1 power relay");
    digitalWrite(INVERTER_PIN, HIGH);
    inverterRelayState = true;
    Debug.println("K1 relay turned ON");

    // Start waiting for delay
    if (inverterSoftPwrEnabled) {
      Debug.printf("Waiting %lums (Delay 1: K1 to K3) - non-blocking\n", inverterDelay1);
    } else {
      Debug.printf("Waiting %lums (Delay 2: inverter to roof button) - non-blocking\n", inverterDelay2);
    }
    roofOpState = OP_INVERTER_POWER_ON;
    roofOpStepStartTime = millis();
  } else if (inverterSoftPwrEnabled && roofOpNeedsInverterButton) {
    // No inverter relay, but need soft-power button press (K3) before roof button
    Debug.println("Inverter relay disabled - pressing K3 soft-power button first");
    digitalWrite(INVERTER_BUTTON_PIN, HIGH);
    Debug.println("Inverter button PRESSED (K3 relay energized)");
    roofOpState = OP_INVERTER_BUTTON_PRESS;
    roofOpStepStartTime = millis();
  } else {
    // No inverter relay, no K3 needed - go directly to roof button
    Debug.println("Inverter relay disabled - pressing roof button directly");
    digitalWrite(ROOF_CONTROL_PIN, HIGH);
    Debug.println("Button PRESSED (K2 relay energized)");
    roofOpState = OP_ROOF_BUTTON_PRESS;
    roofOpStepStartTime = millis();
  }

  return true;
}

// Complete startClosingRoof() function - NON-BLOCKING version
// This initiates the roof closing sequence using a state machine.
// The actual relay operations are performed by processRoofOperation() in the main loop.
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

  // Check if an operation is already in progress
  if (roofOpState != OP_IDLE) {
    Debug.println("Roof operation already in progress");
    return false;
  }

  // Check telescope safety interlock - only if bypass is not enabled
  Debug.println("=== ROOF CLOSING SAFETY CHECK ===");
  Debug.printf("bypassParkSensor: %s\n", bypassParkSensor ? "TRUE (bypass enabled)" : "FALSE (bypass disabled)");
  Debug.printf("telescopeParked: %s\n", telescopeParked ? "TRUE (parked)" : "FALSE (not parked)");
  Debug.printf("Park sensor type: %d (0=Physical, 1=UDP, 2=Both)\n", parkSensorType);

  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("SAFETY CHECK FAILED: Telescope not parked and bypass not enabled");
    Debug.println("=== ROOF CLOSING BLOCKED ===");
    return false; // Telescope not parked and bypass not enabled
  }

  Debug.println("SAFETY CHECK PASSED: Closing roof (non-blocking)");

  // Set target direction
  roofOpTarget = TARGET_CLOSE;

  // Determine if we need the soft-power button press
  if (inverterSoftPwrEnabled) {
    inverterACPowerState = (digitalRead(INVERTER_AC_POWER_PIN) == LOW);
    roofOpNeedsInverterButton = !inverterACPowerState;
    Debug.printf("AC power detected: %s, will %spress K3\n",
                 inverterACPowerState ? "YES" : "NO",
                 roofOpNeedsInverterButton ? "" : "NOT ");
  } else {
    roofOpNeedsInverterButton = false;
  }

  // Start the state machine
  if (inverterRelayEnabled) {
    // Step 1: Turn on K1 power relay
    Debug.println("Inverter relay enabled - turning on K1 power relay");
    digitalWrite(INVERTER_PIN, HIGH);
    inverterRelayState = true;
    Debug.println("K1 relay turned ON");

    // Start waiting for delay
    if (inverterSoftPwrEnabled) {
      Debug.printf("Waiting %lums (Delay 1: K1 to K3) - non-blocking\n", inverterDelay1);
    } else {
      Debug.printf("Waiting %lums (Delay 2: inverter to roof button) - non-blocking\n", inverterDelay2);
    }
    roofOpState = OP_INVERTER_POWER_ON;
    roofOpStepStartTime = millis();
  } else if (inverterSoftPwrEnabled && roofOpNeedsInverterButton) {
    // No inverter relay, but need soft-power button press (K3) before roof button
    Debug.println("Inverter relay disabled - pressing K3 soft-power button first");
    digitalWrite(INVERTER_BUTTON_PIN, HIGH);
    Debug.println("Inverter button PRESSED (K3 relay energized)");
    roofOpState = OP_INVERTER_BUTTON_PRESS;
    roofOpStepStartTime = millis();
  } else {
    // No inverter relay, no K3 needed - go directly to roof button
    Debug.println("Inverter relay disabled - pressing roof button directly");
    digitalWrite(ROOF_CONTROL_PIN, HIGH);
    Debug.println("Button PRESSED (K2 relay energized)");
    roofOpState = OP_ROOF_BUTTON_PRESS;
    roofOpStepStartTime = millis();
  }

  return true;
}

// Stop roof movement - NON-BLOCKING version
// updateStatus: if true (default), updates roof status based on limit switches
//               if false, preserves current status (used during timeout to keep ERROR state)
bool stopRoofMovement(bool updateStatus) {
  // If an operation is in progress, we need to abort it and do a stop
  if (roofOpState != OP_IDLE) {
    Debug.println("Aborting in-progress operation for stop");
    // Make sure all relays are released first
    digitalWrite(ROOF_CONTROL_PIN, LOW);
    digitalWrite(INVERTER_BUTTON_PIN, LOW);
  }

  // If not currently moving, just turn off inverter and return
  if (roofStatus != ROOF_OPENING && roofStatus != ROOF_CLOSING) {
    if (inverterRelayEnabled) {
      digitalWrite(INVERTER_PIN, LOW);
      inverterRelayState = false;
      Debug.println("Inverter turned OFF (K1 relay de-energized)");
    }
    roofOpState = OP_IDLE;
    roofOpTarget = TARGET_NONE;
    return true;
  }

  // Start stop sequence using state machine
  roofOpTarget = TARGET_STOP;

  // Press the roof button to stop movement
  digitalWrite(ROOF_CONTROL_PIN, HIGH);
  Debug.println("Stop: Button PRESSED (K2 relay energized)");
  roofOpState = OP_STOP_BUTTON_PRESS;
  roofOpStepStartTime = millis();

  // Note: The actual button release and inverter shutdown happens in processRoofOperation()

  // If caller wants to preserve status (timeout case), set error before state machine completes
  if (!updateStatus) {
    // Status will be preserved - the state machine won't call updateRoofStatus()
    // Actually we need to handle this differently - let's add a flag
  }

  Debug.println("Stop sequence initiated (non-blocking)");
  return true;
}

// Clear error state and reason (for recovery from error conditions)
void clearRoofError() {
  if (roofStatus == ROOF_ERROR) {
    Debug.println("Clearing roof error state");
    Debug.println("Previous error: " + roofErrorReason);

    // Clear the error reason
    roofErrorReason = "";

    // Re-determine status based on current limit switch states
    determineInitialRoofStatus();

    Debug.println("Roof status after clearing error: " + getRoofStatusString());
  }
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

// ========== NEW INVERTER CONTROL FUNCTIONS (v3 Hardware) ==========

// Toggle K1 inverter power relay (manual control)
void toggleInverterPower() {
  inverterRelayState = !inverterRelayState;
  digitalWrite(INVERTER_PIN, inverterRelayState ? HIGH : LOW);

  Debug.print("Inverter power relay (K1) manually toggled to: ");
  Debug.println(inverterRelayState ? "ON" : "OFF");

  // Publish status change to MQTT
  publishStatusToMQTT();
}

// Send K3 soft-power button press to inverter
void sendInverterButtonPress() {
  // For a normally open relay:
  // LOW = Relay not energized = Button NOT pressed
  // HIGH = Relay energized = Button pressed

  Debug.println("Inverter button (K3) press initiated");

  // Press button (energize relay)
  digitalWrite(INVERTER_BUTTON_PIN, HIGH);
  Debug.println("Inverter button PRESSED (K3 relay energized)");
  delay(500); // Hold the button for half a second

  // Release button (de-energize relay)
  digitalWrite(INVERTER_BUTTON_PIN, LOW);
  Debug.println("Inverter button RELEASED (K3 relay de-energized)");

  // Give the inverter time to process the button press
  delay(100);
}

// Get state of K1 inverter power relay
bool getInverterRelayState() {
  return inverterRelayState;
}

// ========== NON-BLOCKING STATE MACHINE ==========
// This function must be called from the main loop to process roof operations
// without blocking WiFi/MQTT communication

void processRoofOperation() {
  // Nothing to do if idle
  if (roofOpState == OP_IDLE) {
    return;
  }

  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - roofOpStepStartTime;

  switch (roofOpState) {
    case OP_INVERTER_POWER_ON:
      // K1 was turned on, waiting for delay before next step
      {
        unsigned long requiredDelay;
        if (inverterSoftPwrEnabled) {
          requiredDelay = inverterDelay1;  // Delay 1: K1 to K3
        } else {
          requiredDelay = inverterDelay2;  // Delay 2: K1 to K2
        }

        if (elapsed >= requiredDelay) {
          Debug.printf("Delay complete (%lums)\n", requiredDelay);

          // Move to next step based on configuration
          if (inverterSoftPwrEnabled && roofOpNeedsInverterButton) {
            // Need to press K3 soft-power button
            digitalWrite(INVERTER_BUTTON_PIN, HIGH);
            Debug.println("Inverter button PRESSED (K3 relay energized)");
            roofOpState = OP_INVERTER_BUTTON_PRESS;
            roofOpStepStartTime = currentTime;
          } else if (inverterSoftPwrEnabled && !roofOpNeedsInverterButton) {
            // Soft-power enabled but AC already on, skip to roof button
            digitalWrite(ROOF_CONTROL_PIN, HIGH);
            Debug.println("Button PRESSED (K2 relay energized)");
            roofOpState = OP_ROOF_BUTTON_PRESS;
            roofOpStepStartTime = currentTime;
          } else {
            // Soft-power disabled, go directly to roof button
            digitalWrite(ROOF_CONTROL_PIN, HIGH);
            Debug.println("Button PRESSED (K2 relay energized)");
            roofOpState = OP_ROOF_BUTTON_PRESS;
            roofOpStepStartTime = currentTime;
          }
        }
      }
      break;

    case OP_INVERTER_BUTTON_PRESS:
      // K3 is pressed, wait 500ms then release
      if (elapsed >= 500) {
        digitalWrite(INVERTER_BUTTON_PIN, LOW);
        Debug.println("Inverter button RELEASED (K3 relay de-energized)");
        roofOpState = OP_INVERTER_BUTTON_RELEASE;
        roofOpStepStartTime = currentTime;
      }
      break;

    case OP_INVERTER_BUTTON_RELEASE:
      // K3 released, wait 100ms then proceed to delay2
      if (elapsed >= 100) {
        Debug.printf("Waiting %lums (Delay 2: inverter to roof button)\n", inverterDelay2);
        roofOpState = OP_INVERTER_DELAY2;
        roofOpStepStartTime = currentTime;
      }
      break;

    case OP_INVERTER_DELAY2:
      // Waiting delay2 after soft-power button
      if (elapsed >= inverterDelay2) {
        Debug.println("Delay 2 complete");
        // Press roof button
        digitalWrite(ROOF_CONTROL_PIN, HIGH);
        Debug.println("Button PRESSED (K2 relay energized)");
        roofOpState = OP_ROOF_BUTTON_PRESS;
        roofOpStepStartTime = currentTime;
      }
      break;

    case OP_ROOF_BUTTON_PRESS:
      // K2 is pressed, wait 500ms then release
      if (elapsed >= 500) {
        digitalWrite(ROOF_CONTROL_PIN, LOW);
        Debug.println("Button RELEASED (K2 relay de-energized)");
        roofOpState = OP_ROOF_BUTTON_RELEASE;
        roofOpStepStartTime = currentTime;
      }
      break;

    case OP_ROOF_BUTTON_RELEASE:
      // K2 released, operation sequence complete
      {
        // Update roof status based on target
        roofErrorReason = "";  // Clear any previous error
        if (roofOpTarget == TARGET_OPEN) {
          roofStatus = ROOF_OPENING;
          Debug.println("Roof opening started");
        } else if (roofOpTarget == TARGET_CLOSE) {
          roofStatus = ROOF_CLOSING;
          Debug.println("Roof closing started");
        }
        movementStartTime = currentTime;
        lastSwitchTime = currentTime;  // Debounce after button press

        // Publish status change immediately
        publishStatusToMQTT();
        lastPublishedStatus = roofStatus;

        // Return to idle
        roofOpState = OP_IDLE;
        roofOpTarget = TARGET_NONE;
      }
      break;

    case OP_STOP_BUTTON_PRESS:
      // K2 is pressed for stop, wait 500ms then release
      if (elapsed >= 500) {
        digitalWrite(ROOF_CONTROL_PIN, LOW);
        Debug.println("Button RELEASED (K2 relay de-energized)");
        roofOpState = OP_STOP_BUTTON_RELEASE;
        roofOpStepStartTime = currentTime;
      }
      break;

    case OP_STOP_BUTTON_RELEASE:
      // K2 released for stop, turn off K1 and finish
      {
        // Turn off inverter if relay control is enabled
        if (inverterRelayEnabled) {
          digitalWrite(INVERTER_PIN, LOW);
          inverterRelayState = false;
          Debug.println("Inverter turned OFF (K1 relay de-energized)");
        }

        // Update status based on limit switches
        updateRoofStatus();

        // Publish status change
        publishStatusToMQTT();
        lastPublishedStatus = roofStatus;

        Debug.println("Roof movement stopped");

        // Return to idle
        roofOpState = OP_IDLE;
        roofOpTarget = TARGET_NONE;
      }
      break;

    default:
      // Unknown state, reset to idle
      roofOpState = OP_IDLE;
      roofOpTarget = TARGET_NONE;
      break;
  }
}

// Get state of AC power (via optocoupler on GPIO7)
bool getInverterACPowerState() {
  // Read the AC power detection pin
  // When AC power is present, the optocoupler pulls the pin LOW
  return digitalRead(INVERTER_AC_POWER_PIN) == LOW;
}

// Update and monitor inverter AC power state (call in main loop)
void updateInverterPowerStatus() {
  unsigned long currentTime = millis();

  // Read current AC power state
  bool currentACPowerState = getInverterACPowerState();

  // Check if AC power state changed
  if (currentACPowerState != lastInverterACPowerState) {
    lastInverterACPowerState = currentACPowerState;
    lastInverterACPowerChangeTime = currentTime;
    Debug.println("AC power state CHANGED to: " + String(currentACPowerState ? "ON" : "OFF"));
  }

  // Only consider the state change valid if it has been stable for SWITCH_STABLE_TIME
  if (currentTime - lastInverterACPowerChangeTime > SWITCH_STABLE_TIME) {
    if (inverterACPowerState != currentACPowerState) {
      inverterACPowerState = currentACPowerState;
      Debug.println("AC power state UPDATED to: " + String(inverterACPowerState ? "ON" : "OFF"));

      // Publish status to MQTT if AC power state changed
      publishStatusToMQTT();
    }
  }

  // Debug output every 30 seconds
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime > 30000) {
    Debug.printf(2, "Inverter Status - Relay (K1): %s, AC Power: %s\n",
                 inverterRelayState ? "ON" : "OFF",
                 inverterACPowerState ? "ON" : "OFF");
    lastDebugTime = currentTime;
  }
}