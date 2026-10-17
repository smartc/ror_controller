# ESP32-S3 Roll-Off Roof Controller (v4)

ASCOM Alpaca compatible roll-off roof controller for remote observatory automation, built on ESP32-S3 with MQTT integration, weather safety interlock, and a browser-based control interface.

![Version](https://img.shields.io/badge/version-4.0.0-blue)
![Hardware](https://img.shields.io/badge/hardware-ESP32--S3-green)
![License](https://img.shields.io/badge/license-MIT-orange)

---

## SAFETY WARNING

**Relays on this board carry low-voltage control signals only.** They trigger external contactors -- never connect motors, inverters, or other high-current loads directly to the relay outputs. Use appropriately rated external contactors for all high-power switching.

---

## Features

### Roof Control
- Non-blocking state machine sequences relay operations without interrupting networking/MQTT
- Open, close, and stop via ASCOM, MQTT, or web interface
- Limit switch detection with configurable trigger state and swap option
- Two independent configurable timeouts: limit switch response (default 5s) and total movement (default 90s)

### Safety Interlocks
- **Park sensor (pre-movement)**: blocks roof movement when telescope is unparked. Supports physical GPIO, UDP network sensors, or both (AND logic)
- **Park sensor (runtime)**: if the telescope transitions from parked to unparked while the roof is actively moving, the controller immediately kills inverter power (K1), sets `ROOF_ERROR`, and publishes an alert. The operator must acknowledge the error before the roof can move again. See caution below regarding shore-power installs.
- **Weather/sky safety**: receives UDP broadcasts from safety sensor devices; blocks roof opening when any enabled sensor reports unsafe conditions. Fail-safe: an enabled sensor that goes offline is treated as unsafe
- **Auto-close**: optionally closes the roof automatically when weather becomes unsafe (requires telescope parked)
- All safety interlocks have bypass toggles accessible from both the Setup and Roof Control pages
- **Error auto-recovery**: an error (e.g. movement timeout, or a manual move that left the roof between limits) clears itself once the roof reaches a limit switch *after* the error began; the original reason is kept in the log. Errors where the switch never changed ("failed to start", both limits triggered) and the park-interlock emergency stop still need a manual clear
- **Firmware updates blocked while moving**: OTA uploads are refused (HTTP 409) while the roof is moving or a relay sequence is running, and an upload in progress is aborted if a move starts (flash writes stall both CPU cores)

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

### Networking (v4)
- **WiFi** is used for first-time setup (AP mode) and is always available as a way back in
- **Optional W5500 wired Ethernet** (SPI on the H4 breakout) alongside WiFi
- **Network mode** (Setup > Network):
  - *WiFi only (no Ethernet fitted)*
  - *Dual*: both interfaces up with their own IPs; the web UI, Alpaca, discovery, NTP and sensor listeners answer on either. A **preferred interface** setting picks the default route for outbound traffic (MQTT)
  - *WiFi primary, Ethernet backup* / *Ethernet primary, WiFi backup*: only the primary runs; the backup starts after the primary has been down 15 s and stops once the primary has been back 60 s
  - There is deliberately no Ethernet-only mode, so a broken wired network can't lock you out
- **Gateway check** (on by default): the primary interface pings its router every 30 s; after ~90 s without replies the link counts as down even though it has an IP (dead switch port, wrong VLAN), so the backup starts and outbound traffic moves over
- If the W5500 fails to start, the controller falls back to WiFi only
- AP mode only starts when WiFi fails **and** Ethernet has no IP, so a cabled controller with bad WiFi credentials stays reachable
- Link, IP and active-interface changes are logged, which makes WiFi drops easy to tell apart from resets
- Alpaca UniqueID and Home Assistant device IDs come from the factory WiFi MAC, so they don't change when Ethernet is added

### Logging (v4)
- Every log line is written to **FRAM** (MB85RC256V, I2C 0x50) the moment it's logged, so the lines leading up to a brownout or crash survive the reset
- FRAM is copied to **flash** (LittleFS, `/logs/YYYY-MM-DD.log` per UTC day) every 15 minutes, when the FRAM ring is half full, before a web restart and after OTA; never while the roof is moving
- Retention: 90 days and 2 MB total, oldest deleted first
- Console page: live view, history across reboots (from FRAM), file list with download/delete, download-all, download FRAM buffer, flush now, clear all
- Lines are timestamped in UTC once any time source is synced; lines logged before sync in the same boot are back-dated from uptime (marked `~`)

### Web Interface
- Dark theme, responsive layout, 2-second live status polling
- Consistent navigation on all pages: Home, Roof Control, Setup, Console, Update
- **Setup page tabs**: Device Configuration, Park Sensor, Safety Sensor, MQTT, GPS/RTC/NTP, Network
- **Console page**: live debug log (200 lines), FRAM history across reboots, and log file management
- Park sensor and safety sensor management: per-sensor enable, bypass, and remove
- OTA firmware updates via ElegantOTA (auto-reboot after upload)

### MQTT / Home Assistant
- Publishes status every 30 seconds and on state change
- Accepts open/close/stop commands
- LWT availability topic
- Status payload includes weather safety state and safety bypass flag

### GPS / NTP / RTC
- Optional GPS module for time sync (configurable pins)
- DS3231 RTC backup (I2C). If its oscillator-stop flag is set (e.g. dead backup battery) the RTC time is not trusted until GPS or network time rewrites it
- Network time (SNTP) as a fallback when neither GPS nor the RTC has time; it also repairs an untrusted RTC. The built-in NTP server only serves GPS/RTC time
- NTP server mode: serves time to local network devices
- PPS signal support for sub-second NTP accuracy
- Configurable timezone offset and DST

---

## Hardware (v3 PCB, v4 firmware)

**Microcontroller**: ESP32-S3 44-pin DevKit, **16 MB flash** (e.g. N16R8)  
**Power**: 12V DC input, onboard 5V regulator  
**Optional add-ons**: W5500 Ethernet module on the H4 breakout, MB85RC256V FRAM on I2C-EXT

### GPIO Assignments

| Pin | Function |
|-----|----------|
| GPIO4 | K1 - Inverter power relay |
| GPIO5 | K2 - Roof button relay |
| GPIO6 | K3 - Inverter soft-power relay |
| GPIO7 | AC power detection (optocoupler) |
| GPIO8 | I2C SDA (RTC 0x68, FRAM 0x50) |
| GPIO9 | I2C SCL (RTC 0x68, FRAM 0x50) |
| GPIO10 | W5500 CS (optional) |
| GPIO11 | W5500 MOSI (optional) |
| GPIO12 | GPS PPS (optional) |
| GPIO13 | W5500 MISO (optional) |
| GPIO14 | GPS data in (optional) |
| GPIO35 | Open limit switch |
| GPIO36 | Closed limit switch |
| GPIO37 | Rain sensor (RG9) |
| GPIO38-41 | Snow sensor (digital + RS485) |
| GPIO42 | Physical park sensor |
| GPIO46 | W5500 SCK (optional) |

GPS and W5500 pins are configurable on the Setup page (GPS/RTC/NTP and Network tabs). The W5500 runs without its INT pin (the driver polls every 10 ms). SCK sits on GPIO46 because only the ESP32 drives it, so it can't disturb that strapping pin during USB flashing. A GPS PPS signal on GPIO46 could.

**PSRAM must stay disabled.** On boards with octal PSRAM (N8R8/N16R8) GPIO35-37 are shared with the PSRAM. They work as the limit-switch inputs only while PSRAM is off.

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
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** (sets the 3 MB size limit; the actual table comes from `main/partitions.csv`) |
| PSRAM | **Disabled** |
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |

`main/partitions.csv` gives two 3 MB OTA app slots and a 9.9 MB LittleFS partition (`spiffs`) for the log archive. The Arduino build picks it up automatically from the sketch folder.

### Upgrading from v3 (one-time USB flash)

v4 changes the partition table and no longer fits v3's 1.25 MB app slot, so **OTA from v3 to v4 is rejected**. Flash once over USB; after that, OTA works as before.

1. In the IDE, use the board settings above and make sure **Erase All Flash Before Sketch Upload is Disabled**.
2. Upload. The IDE writes the bootloader, partition table, boot_app0 and the app. The settings partition (NVS at 0x9000) is not touched, so all saved settings carry over.
3. On first boot the log partition is formatted automatically.

Alternatively, flash a spare board with v4, configure it, and swap it in.

### First Boot

1. Device starts in AP mode: SSID `RoofController`, password `RoofController`
2. Connect and open `http://192.168.4.1`
3. Enter WiFi credentials on the Setup page, Network tab (or `/wificonfig` to scan)
4. Device reboots and connects to your network

---

## Configuration

All settings are saved to NVS (survives reboots) and configurable via the web interface.

**Setup page** (`/setup`), in tabs:
- **Device Configuration**: trigger states, switch swap, park switch type, inverter and timeout settings, restart/factory reset
- **Park Sensor** / **Safety Sensor**: sensor types, per-sensor enable/bypass/remove, auto-close
- **MQTT**: broker, credentials, client ID, topic prefix, keepalive
- **GPS / RTC / NTP**: GPS enable and pins, NTP server, timezone/DST
- **Network**: live WiFi/Ethernet status, WiFi credentials, W5500 enable and pins, preferred interface, hostname

**WiFi scan** (`/wificonfig`): scan for networks (also used in AP mode)

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
  "network_interface": "WiFi",
  "version": "4.0.0"
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
| `/console.json` | GET | Raw log JSON (current boot, RAM) |
| `/logs.json` | GET | FRAM/archive status and log file list |
| `/logs/download?f=NAME` | GET | Download one archived log file |
| `/logs/download_all` | GET | All archived logs as one file (flushes FRAM first) |
| `/logs/fram` | GET | Current FRAM buffer as text |
| `/logs/recent.json?n=N` | GET | Last N FRAM records (spans reboots) |
| `/logs/flush` | POST | Copy FRAM to flash now |
| `/logs/delete?f=NAME` | POST | Delete one archived log file |
| `/logs/clear` | POST | Clear FRAM and all archived logs |
| `/network_settings` | POST | Ethernet pins, preferred interface, hostname (restart to apply) |
| `/api/status` | GET | Live status JSON (includes `network` object) |
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

### v4.0.0 (2026-10-17)
- **16 MB flash layout** (`main/partitions.csv`): 3 MB OTA app slots plus a 9.9 MB log partition. One-time USB flash needed when upgrading from v3; settings carry over.
- **W5500 Ethernet** alongside WiFi: network modes (WiFi only, dual, WiFi or Ethernet primary with the other as backup), gateway reachability check, no AP mode while Ethernet is up. Network tab on the Setup page.
- **Persistent logging**: FRAM black box (every line, survives power loss) plus a LittleFS archive (daily files, 90 days / 2 MB). Console page gains history across reboots, downloads, flush and clear.
- **Time**: network time (SNTP) fallback; DS3231 oscillator-stop flag honoured and cleared; date rollover fixed for RTC/network time.
- **Setup page** split into tabs and streamed (lower peak heap); saving a form returns to the same tab.
- **Roof task**: limit switches, interlocks, timeouts, relay sequences and weather auto-close run in a dedicated 10 ms FreeRTOS task with a watchdog, independent of web/MQTT/logging. Stale network inputs let a move finish but block new ones.
- OTA refused while the roof is moving; an in-flight upload aborts if a move starts.
- Roof errors clear themselves when the roof next reaches a limit switch (the log keeps the reason); the park-interlock stop still needs a manual clear.
- Log hygiene: AP password no longer logged; chatty page-size lines moved to verbose.
- Fix: console live view never refreshing.

### v3.4.2 (2026-10-17)
- **Runtime park sensor interlock**: if the telescope becomes unparked while the roof is actively moving (or in the inverter startup sequence), the controller immediately kills K1 inverter power, sets `ROOF_ERROR`, and publishes an MQTT alert. Requires operator acknowledgement before further movement.
- K2 (stop button) is intentionally not pressed during this interlock — single-toggle openers would restart movement if K2 were sent to an already-stopped opener. K1 kill is the definitive action for inverter-based installs.
- Added `roofMotorCommandedRunning` flag to track controller-commanded movement, preventing false-positive interlock triggers when the roof is stopped mid-travel but `roofStatus` still shows `OPENING`/`CLOSING`.
- Added shore-power caution to documentation: installs where K1 is not wired into the opener's power supply cannot be stopped electrically by this interlock.

### v3.4.1 (2026-10-15)
- Manual roof movement is inferred only after both limit switches read NOT triggered for 2 s; a brief limit glitch no longer causes a phantom OPENING/CLOSING and a sticky "failed to start" error
- Safety sensor offline timeout raised from 90 s to 5 min so a couple of dropped UDP broadcasts no longer mark the sensor unsafe
- Alpaca OpenShutter refused for weather returns a weather-specific 1035 message
- Web UI OPEN/CLOSE clears ERROR when exactly one limit switch is triggered; refusals are shown in an alert

### v3.4.0
- UI styling consistency with SkyConditions_90640 (navy theme, nav, setup page layout)

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
