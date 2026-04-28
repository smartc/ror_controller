# ESP32-S3 Roll-Off Roof Controller (v3)

ASCOM Alpaca compatible roll-off roof controller for remote observatory automation, built on ESP32-S3 with MQTT integration, weather safety interlock, and a browser-based control interface.

![Version](https://img.shields.io/badge/version-3.4.1-blue)
![Hardware](https://img.shields.io/badge/hardware-ESP32--S3-green)
![License](https://img.shields.io/badge/license-MIT-orange)

---

## SAFETY WARNING

**Relays on this board carry low-voltage control signals only.** They trigger external contactors -- never connect motors, inverters, or other high-current loads directly to the relay outputs. Use appropriately rated external contactors for all high-power switching.

---

## Features

### Roof Control
- Non-blocking state machine sequences relay operations without interrupting WiFi/MQTT
- Open, close, and stop via ASCOM, MQTT, or web interface
- Limit switch detection with configurable trigger state and swap option
- Two independent configurable timeouts: limit switch response (default 5s) and total movement (default 90s)

### Safety Interlocks
- **Park sensor (pre-movement)**: blocks roof movement when telescope is unparked. Supports physical GPIO, UDP network sensors, or both (AND logic)
- **Park sensor (runtime)**: if the telescope transitions from parked to unparked while the roof is actively moving, the controller immediately kills inverter power (K1), sets `ROOF_ERROR`, and publishes an alert. The operator must acknowledge the error before the roof can move again. See caution below regarding shore-power installs.
- **Weather/sky safety**: receives UDP broadcasts from safety sensor devices; blocks roof opening when any enabled sensor reports unsafe conditions. Fail-safe: an enabled sensor that goes offline is treated as unsafe
- **Auto-close**: optionally closes the roof automatically when weather becomes unsafe (requires telescope parked)
- All safety interlocks have bypass toggles accessible from both the Setup and Roof Control pages

> **CAUTION — Shore-power openers with no relay on K1:**
> The runtime park sensor interlock kills K1 (inverter power relay) as its primary stop action. If your roof opener runs directly on shore power and K1 is not wired into its supply, the controller **cannot electrically stop the opener** when this interlock fires. The system will still set `ROOF_ERROR`, kill K1 (no-op in this wiring), and publish an immediate MQTT alert — but the opener will continue running until it reaches a limit switch or is stopped manually.
>
> The stop button (K2) is intentionally **not** pressed during this interlock. Most openers use a single toggle button (press to start, press to stop, press to start again). Pressing K2 on an opener that is already stationary would restart movement, which is the opposite of the intent. The controller cannot reliably distinguish a running opener from a stopped one mid-travel, so K2 is left untouched.
>
> **Recommendation for shore-power installs**: wire K1 as a power interruptor in the opener's supply circuit so the controller has a reliable, non-ambiguous means of cutting power. Alternatively, ensure the park sensor bypass is never enabled when the telescope may be unparked during a session.

### Inverter Control (v3 hardware)
- K1: 12V power relay (triggers external contactor)
- K3: Soft-power button relay (500ms pulse)
- AC power detection via optocoupler distinguishes relay state from actual inverter output
- Manual controls available from Roof Control page

### Web Interface
- Dark theme, responsive layout, 2-second live status polling
- Consistent navigation on all pages: Home, Roof Control, Setup, WiFi Config, Console, Update
- **Console page**: live ring-buffer debug log (50 entries), accessible remotely without serial monitor
- Park sensor and safety sensor management: per-sensor enable, bypass, and remove
- OTA firmware updates via ElegantOTA (auto-reboot after upload)

### MQTT / Home Assistant
- Publishes status every 30 seconds and on state change
- Accepts open/close/stop commands
- LWT availability topic
- Status payload includes weather safety state and safety bypass flag

### GPS / NTP / RTC
- Optional GPS module for time sync (configurable pins)
- DS3231 RTC backup (I2C)
- NTP server mode: serves time to local network devices
- PPS signal support for sub-second NTP accuracy
- Configurable timezone offset and DST

---

## Hardware (v3)

**Microcontroller**: ESP32-S3 44-pin  
**Power**: 12V DC input, onboard 5V regulator

### GPIO Assignments

| Pin | Function |
|-----|----------|
| GPIO4 | K1 - Inverter power relay |
| GPIO5 | K2 - Roof button relay |
| GPIO6 | K3 - Inverter soft-power relay |
| GPIO7 | AC power detection (optocoupler) |
| GPIO8 | I2C SDA (RTC) |
| GPIO9 | I2C SCL (RTC) |
| GPIO35 | Open limit switch |
| GPIO36 | Closed limit switch |
| GPIO37 | Rain sensor (RG9) |
| GPIO38-41 | Snow sensor (digital + RS485) |
| GPIO42 | Physical park sensor |

GPS pins are configurable via the web interface (default TX: GPIO14).

---

## Installation

### Requirements

- Arduino IDE 2.0+
- Board: ESP32-S3 Dev Module
- Libraries (via Library Manager): ArduinoJson 6.x, PubSubClient, ElegantOTA

### Board Settings

| Setting | Value |
|---------|-------|
| USB CDC On Boot | Enabled |
| Flash Size | 4MB |
| Partition Scheme | Default 4MB with SPIFFS |
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |

### First Boot

1. Device starts in AP mode: SSID `RoofController`, password `RoofController`
2. Connect and open `http://192.168.4.1`
3. Enter WiFi credentials under WiFi Config
4. Device reboots and connects to your network

---

## Configuration

All settings are saved to NVS (survives reboots) and configurable via the web interface.

**Setup page** (`/setup`): pin assignments, trigger states, timeout values, MQTT broker, park sensor type, safety sensor enable/bypass, GPS/NTP settings, timezone

**WiFi Config** (`/wificonfig`): SSID and password

**Roof Control** (`/control`): park sensor bypass and safety sensor bypass toggles accessible here for quick override without navigating to setup

---

## MQTT

**Default topic prefix**: `observatory/roof`

| Topic | Direction | Description |
|-------|-----------|-------------|
| `<prefix>/status` | Publish | JSON status (30s interval + on change) |
| `<prefix>/command` | Subscribe | `{"command":"open\|close\|stop"}` |
| `<prefix>/availability` | Publish | `online` / `offline` (LWT) |

### Status payload

```json
{
  "status": "Closed",
  "telescope_parked": true,
  "bypass_enabled": false,
  "weather_safe": true,
  "safety_bypass": false,
  "inverter_relay_state": false,
  "inverter_ac_power_state": false,
  "limit_open": false,
  "limit_closed": true,
  "device_id": "ESP32-XXXXXX",
  "ip_address": "192.168.1.100",
  "version": "3.3.0"
}
```

### Home Assistant example

```yaml
mqtt:
  cover:
    - name: "Observatory Roof"
      command_topic: "observatory/roof/command"
      state_topic: "observatory/roof/status"
      payload_open: '{"command":"open"}'
      payload_close: '{"command":"close"}'
      payload_stop: '{"command":"stop"}'
      value_template: "{{ value_json.status }}"
      state_open: "Open"
      state_closed: "Closed"
      availability_topic: "observatory/roof/availability"

  binary_sensor:
    - name: "Weather Safe"
      state_topic: "observatory/roof/status"
      value_template: "{{ value_json.weather_safe }}"
      payload_on: true
      payload_off: false
```

---

## ASCOM Alpaca

- Discovery: UDP port 32227, responds to `alpacadiscovery1`
- API base: `http://<device-ip>:11111/api/v1/dome/0/`
- Device type: Dome (RollOffRoof)

| Method | Description |
|--------|-------------|
| `GET shutterstatus` | 0=Open, 1=Closed, 2=Opening, 3=Closing, 4=Error |
| `PUT openshutter` | Open roof (checks park + weather safety) |
| `PUT closeshutter` | Close roof |
| `PUT abortslew` | Stop movement |
| `GET athome` | True when closed |
| `GET atpark` | True when open |

---

## Web API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Home / status page |
| `/control` | GET | Roof control page |
| `/setup` | GET/POST | Configuration page |
| `/console` | GET | Debug log viewer |
| `/console.json` | GET | Raw log JSON |
| `/api/status` | GET | Live status JSON |
| `/open` | POST | Open roof |
| `/close` | POST | Close roof |
| `/stop` | POST | Stop movement |
| `/toggle_bypass` | POST | Toggle park sensor bypass |
| `/toggle_safety_bypass` | POST | Toggle safety sensor bypass |
| `/safety_sensor_enabled` | POST | Enable/disable a safety sensor |
| `/safety_sensor_bypass` | POST | Bypass a safety sensor |
| `/safety_sensor_remove` | POST | Remove a safety sensor |
| `/inverter_toggle` | POST | Toggle K1 power relay |
| `/inverter_button` | POST | K3 soft-power button press |
| `/update` | GET | OTA firmware update |
| `/restart` | POST | Reboot device |

---

## PCB Files

The `PCB Files/` directory contains v3.0 manufacturing files: schematic, layout, Gerbers, BOM, pick-and-place, and 3D model. Designed for JLCPCB, PCBWay, or equivalent.

---

## Changelog

### v3.4.1 (2026-04-28)
- **Runtime park sensor interlock**: if the telescope becomes unparked while the roof is actively moving (or in the inverter startup sequence), the controller immediately kills K1 inverter power, sets `ROOF_ERROR`, and publishes an MQTT alert. Requires operator acknowledgement before further movement.
- K2 (stop button) is intentionally not pressed during this interlock — single-toggle openers would restart movement if K2 were sent to an already-stopped opener. K1 kill is the definitive action for inverter-based installs.
- Added `roofMotorCommandedRunning` flag to track controller-commanded movement, preventing false-positive interlock triggers when the roof is stopped mid-travel but `roofStatus` still shows `OPENING`/`CLOSING`.
- Added shore-power caution to documentation: installs where K1 is not wired into the opener's power supply cannot be stopped electrically by this interlock.

### v3.3.0 (2026-04-12)
- Safety sensor integration: weather/sky condition interlock via UDP broadcasts
- Auto-close on unsafe weather (configurable, requires telescope parked)
- Safety sensor bypass controls on Setup and Roof Control pages
- Weather safe status and safety bypass state added to MQTT and `/api/status`
- Unified navigation bar across all pages with Console link
- Web-based debug console at `/console` (ring buffer, no serial monitor needed)
- Toggle switches replace checkboxes in safety sensor table
- Per-sensor remove and remove-all for both park and safety sensors
- Park sensor type saves immediately on selection (no Apply button)
- ElegantOTA auto-reboot fix
- HTML entity encoding for all non-ASCII characters

### v3.2.0 (2026-01-18)
- GPS/RTC/NTP support with PPS signal for sub-second accuracy
- DS3231 RTC backup with I2C detection
- NTP server mode for local network devices
- Timezone and DST configuration

### v3.0.0 (2026-01-11)
- Migrated to ESP32-S3 (44-pin)
- Triple relay system (K1, K2, K3)
- AC power detection via optocoupler
- Snow sensor input (digital + RS485)
- Dual inverter state tracking (relay vs AC power)
- Non-blocking roof state machine
- Dual movement timeout system

### v2.1.0
- Initial release: ESP32, ASCOM Alpaca, MQTT, UDP park sensors

---

## License

MIT License. See [LICENSE](LICENSE) for details.

**Author**: Corey Smart
