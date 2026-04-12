# ROR Controller – Safety Sensor Integration Handoff

## Context

The SkyConditions_90640 device (ESP32-S3 ASCOM sky conditions sensor, separate project at
`../SkyConditions_90640`) has been updated to broadcast a UDP safety packet on the local
subnet using the Dark Dragon Astronomy park-sensor protocol. This document describes the
changes needed to the ROR controller to receive and act on those broadcasts.

The design should also accommodate **future distributed safety sensors** of the same type
(e.g. a standalone cloud sensor, an all-sky camera, a wind sensor) — any device that can
broadcast a `SafetySensor` UDP packet on the same port should be automatically discovered
and incorporated into the safety decision, using the same per-sensor enable/bypass model
that already exists for park sensors.

---

## What the SkyConditions device broadcasts

**UDP port:** 23435 (same port the ROR controller already listens on for park sensors)

**Interval:** Every 30 seconds by default; immediately on any wet ↔ dry state change.

**Packet format (JSON):**
```json
{
  "serialNumber": "a4cf1234abcd",
  "deviceType":   "SafetySensor",
  "name":         "Observatory Safety Sensor",
  "isSafe":       true,
  "rainState":    "dry",
  "ambTemp":      12.5
}
```

Key differences from park sensor packets:
- `deviceType` is `"SafetySensor"` (park sensors use `"ScopeParkSensor"` or similar)
- Safety state field is `"isSafe"` (park sensors use `"isSafeToMove"`)
- No `pitch` or `roll` fields
- Extra fields: `rainState` (`"wet"` or `"dry"`), `ambTemp` (°C, or JSON `null` before first sensor read)
- `serialNumber` is the device MAC address (hex, no colons) — stable and unique per device

**Current ROR behaviour:** The existing `processParkSensorMessage()` in `park_sensor_udp.cpp`
checks for the presence of `isSafeToMove` and silently drops any packet missing it, so
SafetySensor packets are already harmlessly ignored. Nothing is broken — this is purely additive.

---

## Changes required

### 1. Parse SafetySensor packets in `park_sensor_udp.cpp`

Add handling alongside the existing park sensor parser. When a packet arrives with
`deviceType == "SafetySensor"`:
- Read `isSafe` (bool) instead of `isSafeToMove`
- Store in a dedicated map keyed by `serialNumber`, separate from `discoveredSensors`
- Track `lastSeen` timestamp; mark offline after 90 seconds (3× the default broadcast interval)
- Log new discoveries and state changes; suppress repeated identical-state packets

Suggested data structure — mirrors the park sensor pattern closely for consistency:

```cpp
struct SafetySensor {
  String        serialNumber;
  String        name;
  String        ipAddress;
  bool          isSafe;
  float         ambTemp;         // NaN if JSON null was received
  unsigned long lastSeen;
  bool          online;          // false if timed out
  bool          enabled;         // user has opted this sensor into the safety decision
  bool          bypassEnabled;   // per-sensor bypass (excluded from safety check)
};

extern std::map<String, SafetySensor> discoveredSafetySensors;
```

### 2. Per-sensor enable and bypass — mirroring park sensor model

Each discovered SafetySensor should support:
- **Enabled** — user explicitly opts the sensor into the safety interlock. Newly discovered
  sensors default to **disabled** (discovered but not yet trusted) so an unexpected device
  on the network cannot silently gate the roof.
- **Bypass** — temporarily excludes an individual sensor from the safety decision without
  removing it. Useful for testing or known-bad sensor states.

Additionally, a **global safety sensor bypass** flag (`bypassSafetySensor`, default `false`)
overrides all sensors at once — the same pattern as `bypassParkSensor` for park sensors.
This is the testing / emergency override the user can toggle from the Setup or home page.

```cpp
bool bypassSafetySensor = false;   // global override — skips ALL safety sensor checks

// Returns true if weather is safe to open:
//   - global bypass enabled, OR
//   - no sensors are enabled, OR
//   - all enabled, non-bypassed, online sensors report isSafe=true
bool isWeatherSafe();
```

**`isWeatherSafe()` logic:**
```
if (bypassSafetySensor) return true;

hasActiveSensor = false
for each sensor in discoveredSafetySensors:
    if not sensor.enabled: continue
    if sensor.bypassEnabled: continue
    hasActiveSensor = true
    if not sensor.online: return false   // offline enabled sensor → fail-safe
    if not sensor.isSafe: return false

if not hasActiveSensor: return true      // no enabled sensors → don't block
return true
```

### 3. Gate roof OPENING on weather safety — do NOT gate closing

In `roof_controller.cpp`, the open safety check is around line 381.
Add a weather-safe interlock here:

```cpp
// === ROOF OPENING SAFETY CHECK ===
// Existing park sensor check ...
if (!bypassParkSensor && !telescopeParked) { return false; }

// NEW: weather safety — never open into rain/unsafe conditions
if (!isWeatherSafe()) {
    Debug.println("SAFETY CHECK FAILED: Weather unsafe");
    return false;
}
```

The closing check (around line 465) must **not** include a weather interlock —
rain is a reason to close, not a reason to refuse to close.

### 4. Auto-close on unsafe weather (optional, configurable)

Add a runtime-configurable NVS-backed setting:
- `weatherAutoClose` (bool, default: **false**)

When enabled, if `isWeatherSafe()` returns false AND the roof is currently open,
trigger a close command automatically.

**Critical safety requirement — check telescope park status before auto-closing:**

Auto-close must respect park sensor state including per-sensor bypass flags.
The existing `isTelescopeParked()` and `bypassParkSensor` already encapsulate this
correctly — use them directly:

```
if (weatherAutoClose && roofIsOpen && !isWeatherSafe()) {
    if (bypassParkSensor || isTelescopeParked()) {
        triggerClose("Auto-close: weather unsafe");
    } else {
        // Rain detected but telescope not confirmed parked — cannot safely close
        Debug.println("Auto-close SUPPRESSED: telescope not confirmed parked");
        // Surface a prominent warning in UI and MQTT
    }
}
```

Rain may be a cheaper problem than a smashed telescope. **Never auto-close if any
enabled, non-bypassed park sensor is offline, unparked, or in an unknown state.**

### 5. UI changes

**Setup page — new "Safety Sensors" section:**
- Table of all discovered SafetySensor devices (name, IP, serial, current state, last-seen)
- Per-sensor Enable checkbox and Bypass checkbox
- Global bypass toggle (`bypassSafetySensor`) with a clear warning label
- Toggle: "Auto-close on unsafe weather" (`weatherAutoClose`, default OFF)
  — display a warning that auto-close is suppressed if the telescope is not parked

**Home / status page:**
- Weather safety status badge: Safe / Unsafe / Bypassed / No sensor
- If auto-close is enabled and was suppressed by park sensor state, show a prominent alert

**MQTT:**
- Include `weather_safe: true/false` and `safety_bypass: true/false` in state payload
- Publish an event when auto-close is triggered or suppressed

---

## Extensibility — future safety sensors

The map-based design (`std::map<String, SafetySensor>`) naturally accommodates multiple
simultaneous devices. Any device on the same subnet that broadcasts a UDP packet with
`deviceType == "SafetySensor"` and `isSafe: true/false` will be automatically discovered
and appear in the UI for the user to enable. The `isWeatherSafe()` AND-logic then
incorporates all enabled, non-bypassed sensors.

Future sensor types that could use this same protocol:
- Standalone cloud/rain sensor
- Wind speed monitor
- All-sky camera safety output
- Generic "observatory is safe" relay device

No code changes are needed on the ROR controller to add new sensor instances — they
just need to broadcast the same JSON format.

---

## Relevant files to modify

| File | Changes needed |
|------|---------------|
| `park_sensor_udp.h/.cpp` | Add `SafetySensor` struct, map, parser, `isWeatherSafe()`, enable/bypass functions, NVS save/load |
| `roof_controller.cpp` | Weather interlock in open check (~line 381); auto-close logic in main loop |
| `roof_controller.h` | Expose `bypassSafetySensor`, `weatherAutoClose` |
| `config.h` | Add `SAFETY_SENSOR_TIMEOUT 90000` constant |
| `web_ui_handler.cpp` | POST handler for new settings |
| `html_templates.h` | Safety Sensors section on Setup page; status badge on home page |
| `mqtt_handler.cpp` | Publish `weather_safe` and `safety_bypass` in state payload |

Follow the existing NVS load/save pattern from `park_sensor_udp.cpp` for all new settings.

---

## Safety decision matrix

| Scenario | Open roof? | Close roof? | Auto-close? |
|----------|-----------|-------------|-------------|
| Global safety bypass enabled | ✓ | ✓ | n/a |
| Weather safe, telescope parked | ✓ | ✓ | n/a |
| Weather safe, telescope unparked | ✗ | ✗ | n/a |
| Unsafe weather, telescope parked | ✗ | ✓ | ✓ if enabled |
| Unsafe weather, telescope unparked | ✗ | ✗ | ✗ suppressed |
| Unsafe weather, park sensor offline | ✗ | ✗ | ✗ suppressed |
| Unsafe weather, park sensor bypassed | ✗ | ✓ | ✓ if enabled |
| Safety sensor offline (was enabled) | ✗ | ✓ | ✓ if enabled + telescope parked |
| Safety sensor bypassed individually | ignored | ✓ | ✓ if enabled + telescope parked |
| No safety sensors enabled | ✓ | ✓ | n/a |

**Fail-safe principle:** an enabled safety sensor that goes offline should be treated as
unsafe for the purpose of the open interlock. The user can bypass it explicitly if they
know the sensor has failed and conditions are actually safe.
