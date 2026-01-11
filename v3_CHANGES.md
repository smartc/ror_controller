# ESP32-S3 Roll-Off Roof Controller v3 Hardware Changes

## Hardware Changes

### Microcontroller Upgrade
- **From:** ESP32 (standard variant)
- **To:** ESP32-S3 (44-pin variant)
- **Reason:** Improved availability, larger ROM size

### New GPIO Pin Assignments (ESP32-S3)
- **K1 (Inverter Power Relay):** GPIO4
- **K2 (Roof Opener Button Relay):** GPIO5
- **K3 (Inverter Soft-Power Button - NEW):** GPIO6
- **AC Power State Detection (NEW):** GPIO7
- **Open Limit Switch:** GPIO35
- **Closed Limit Switch:** GPIO36
- **Park Sensor:** GPIO42
- **RG9 Rain Sensor:** GPIO37
- **Snow Sensor (Digital - NEW):** GPIO38
- **Snow Sensor (RS485 RO):** GPIO41
- **Snow Sensor (RS485 RE/DE):** GPIO39
- **Snow Sensor (RS485 DI):** GPIO40

### New Relays
- **K1:** Inverter 12V power relay (existing functionality)
- **K2:** Roof opener button relay (existing functionality)
- **K3:** Inverter soft-power button relay (**NEW** - for inverters requiring button press to power on/off)

### New Sensors
- **AC Power Detection:** Optocoupler on GPIO7 detects when inverter AC power is ON
- **12V Snow/Rain Sensor:** Support for additional precipitation sensor (digital + RS485)

### Power Supply Improvements
- Larger input capacitors for better stability
- Upsized fuse for improved safety
- Different 5V regulator for enhanced reliability

## Software Changes

### Core Functionality
1. **Updated all pin definitions** to match ESP32-S3 GPIO assignments
2. **Added third relay control (K3)** for inverter soft-power button
3. **AC power state monitoring** via optocoupler
4. **Inverter power state tracking** (relay state + AC power state)

### New Functions (roof_controller.cpp)
- `toggleInverterPower()` - Toggle K1 inverter power relay
- `sendInverterButtonPress()` - Send K3 soft-power button press
- `getInverterRelayState()` - Get state of K1 relay
- `getInverterACPowerState()` - Get state of AC power
- `updateInverterPowerStatus()` - Monitor and update AC power state

### Web Interface Enhancements
- **New Inverter Control Card** displaying:
  - Power Relay (K1) state
  - AC Power state
- **Manual Control Buttons:**
  - Toggle Power Relay (K1)
  - Press Soft-Power Button (K3)
- Real-time status indicators with color coding

### MQTT Integration
New MQTT topics published in status messages:
- `inverter_relay_state` - Boolean (true/false)
- `inverter_ac_power_state` - Boolean (true/false)

### API Endpoints
New web API endpoints:
- `POST /inverter_toggle` - Toggle inverter power relay
- `POST /inverter_button` - Send soft-power button press
- `GET /inverter_status` - Get inverter power states (JSON)

## Compilation Settings

### ESP32-S3 Arduino IDE Settings
- **Board:** ESP32-S3 Dev Module
- **USB CDC On Boot:** Enabled
- **Flash Size:** 4MB (or larger)
- **Partition Scheme:** Default 4MB with SPIFFS
- **Upload Speed:** 921600
- **USB Mode:** Hardware CDC and JTAG

## Migration from v2 to v3

### Pin Remapping
If you have existing v2 hardware, note the following changes:
- All GPIO pins have changed due to ESP32-S3 architecture
- Update any external connections according to new schematic

### New Features
- Test the inverter soft-power button (K3) functionality
- Verify AC power detection via optocoupler
- Configure snow sensor if using additional precipitation detection

### Configuration Preservation
- All existing settings (WiFi, MQTT, etc.) are preserved in preferences
- Pin configurations may need to be reviewed in setup page

## Version Information
- **Hardware Version:** v3 (2026-01-07)
- **Firmware Version:** 3.0.0
- **Board:** ESP32-S3 44-pin
- **Compatible with:** ASCOM Alpaca protocol
