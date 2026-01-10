# ESP32-S3 Roll-Off Roof Controller (v3)

An ASCOM Alpaca compatible roll-off roof controller for remote observatory automation, built on ESP32-S3 hardware with enhanced inverter control and monitoring capabilities.

![Version](https://img.shields.io/badge/version-3.0.0-blue)
![Hardware](https://img.shields.io/badge/hardware-ESP32--S3-green)
![License](https://img.shields.io/badge/license-MIT-orange)

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware](#hardware)
- [Pin Assignments](#pin-assignments)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [MQTT Integration](#mqtt-integration)
- [API Reference](#api-reference)
- [PCB Files](#pcb-files)
- [Changelog](#changelog)
- [License](#license)

## 🔭 Overview

This project provides a complete solution for controlling a roll-off observatory roof using the ASCOM Alpaca protocol. It features:

- **ASCOM Alpaca API** - Full compatibility with ASCOM dome interface
- **Web Interface** - Browser-based control and monitoring
- **MQTT Integration** - Home Assistant and automation platform support
- **Safety Interlocks** - Telescope park sensor with bypass capability
- **UDP Park Sensors** - Network-based telescope position detection
- **Enhanced Inverter Control** - Dual relay system with AC power monitoring
- **OTA Updates** - Over-the-air firmware updates via ElegantOTA

## ✨ Features

### Core Functionality
- **Roof Control**: Open/close operations with limit switch detection
- **Safety System**: Telescope park sensor prevents roof movement when unsafe
- **Status Monitoring**: Real-time roof position and system status
- **Remote Access**: Control via web interface, ASCOM, or MQTT
- **Network Discovery**: Automatic ASCOM Alpaca device discovery

### v3 Hardware Enhancements
- **Triple Relay System**:
  - K1: Inverter 12V power relay
  - K2: Roof opener button relay
  - K3: Inverter soft-power button relay (NEW)
- **AC Power Detection**: Real-time monitoring via optocoupler
- **Dual Power States**: Track both relay state and actual AC power
- **Manual Controls**: Web interface buttons for direct inverter control
- **Enhanced Reliability**: Improved power supply with larger capacitors

### Sensor Support
- **Limit Switches**: Open and closed position detection
- **Physical Park Sensor**: Local telescope position detection
- **UDP Park Sensors**: Network-based telescope position from multiple sources
- **Rain Sensor**: RG9 rain sensor input (GPIO37)
- **Snow Sensor**: 12V digital sensor with RS485 support (NEW in v3)

## 🔧 Hardware

### v3 Specifications (2026-01-07)

**Microcontroller**: ESP32-S3 (44-pin variant)
- Dual-core Xtensa LX7 processor
- 320 KB SRAM, 4-8 MB Flash
- Built-in WiFi 802.11 b/g/n
- USB CDC for programming and debugging

**Power Supply**:
- Input: 12V DC (via JST connector)
- 5V regulator: Improved design with larger input capacitors
- Fuse: Upsized for better protection

**Relays**: 3x HFD4/3 12V relays
- K1: Inverter power (12V DC control)
- K2: Roof opener button
- K3: Inverter soft-power button

**Sensors**:
- 2x Limit switches (open/closed positions)
- 1x Telescope park sensor input
- 1x Rain sensor input (RG9)
- 1x Snow sensor input (digital + RS485)
- 1x AC power detection (optocoupler)

**Connectors**:
- USB-C: Programming and power
- 4-pin: Roof opener connection
- 4-pin: I2C expansion
- 2-pin: Park sensor
- 2-pin: Rain sensor
- 3-pin: Roof limit switches (4-pin connector)
- 4-pin: Inverter AC output

## 📍 Pin Assignments

### ESP32-S3 GPIO Configuration

#### Control Outputs (Relays)
| Pin | Function | Description |
|-----|----------|-------------|
| GPIO4 | K1 - Inverter Power | Main 12V power relay for inverter |
| GPIO5 | K2 - Roof Control | Roof opener button relay |
| GPIO6 | K3 - Inverter Button | Inverter soft-power button relay (v3) |

#### Sensor Inputs
| Pin | Function | Description |
|-----|----------|-------------|
| GPIO7 | AC Power Detection | Optocoupler input for AC power state (v3) |
| GPIO35 | Open Limit Switch | Roof fully open position |
| GPIO36 | Closed Limit Switch | Roof fully closed position |
| GPIO37 | Rain Sensor (RG9) | Precipitation detection |
| GPIO38 | Snow Sensor (Digital) | 12V snow/rain sensor (v3) |
| GPIO39 | Snow Sensor (RS485 RE/DE) | RS485 transceiver control (v3) |
| GPIO40 | Snow Sensor (RS485 DI) | RS485 driver input (v3) |
| GPIO41 | Snow Sensor (RS485 RO) | RS485 receiver output (v3) |
| GPIO42 | Park Sensor | Telescope parked position |

#### Communication
| Pin | Function | Description |
|-----|----------|-------------|
| USB | Programming/Debug | USB CDC via ESP32-S3 internal USB |
| GPIO21 | I2C SDA | I2C expansion port |
| GPIO20 | I2C SCL | I2C expansion port |

## 💻 Installation

### Prerequisites

- **Arduino IDE** 2.0+ or PlatformIO
- **ESP32 Board Support** (ESP32-S3 variant)
- Required Libraries:
  - WiFi (built-in)
  - ESPmDNS (built-in)
  - WebServer (built-in)
  - ArduinoJson (6.x)
  - PubSubClient (MQTT)
  - ElegantOTA

### Arduino IDE Setup

1. **Install ESP32 Board Support**:
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to "Additional Boards Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search for "esp32" and install "esp32 by Espressif Systems"

2. **Configure Board Settings**:
   - Board: **ESP32-S3 Dev Module**
   - USB CDC On Boot: **Enabled**
   - Flash Size: **4MB** (or larger)
   - Partition Scheme: **Default 4MB with SPIFFS**
   - Upload Speed: **921600**
   - USB Mode: **Hardware CDC and JTAG**

3. **Install Required Libraries**:
   - Go to Tools → Manage Libraries
   - Install: ArduinoJson, PubSubClient, ElegantOTA

4. **Upload Firmware**:
   - Open `main/main.ino`
   - Select correct COM port
   - Click Upload

### First Boot Configuration

1. **Access Point Mode**:
   - On first boot, device creates WiFi AP: `RoofController`
   - Password: `RoofController`
   - Connect to this network

2. **Configure WiFi**:
   - Open browser to `http://192.168.4.1`
   - Go to WiFi Configuration
   - Enter your WiFi credentials
   - Device will restart and connect to your network

3. **Find Device IP**:
   - Check your router's DHCP client list
   - Look for "ESP32-RoofController"
   - Or use network scanner to find device

## ⚙️ Configuration

### Web Interface

Access the web interface at `http://<device-ip>/`

**Main Status Page**:
- Current roof status (Open/Closed/Opening/Closing/Error)
- Telescope park status
- Limit switch states
- Inverter power states (relay + AC power)
- MQTT connection status

**Setup Page** (`/setup`):
- WiFi credentials
- MQTT broker settings
- Pin configuration
- Trigger state configuration
- Park sensor type selection
- Bypass park sensor toggle

### Inverter Control (v3)

The web interface includes a dedicated **Inverter Control** card:

**Status Display**:
- Power Relay (K1) state: ON/OFF
- AC Power state: ON/OFF

**Manual Controls**:
- **Toggle Power Relay** - Manually switch K1 on/off
- **Press Soft-Power Button** - Trigger K3 for 500ms

Use these controls to manually power the inverter or toggle its soft-power button without moving the roof.

### Pin Settings

**Limit Switch Configuration**:
- Open Switch Pin (default: GPIO35)
- Closed Switch Pin (default: GPIO36)
- Trigger State: HIGH or LOW
- Swap Switches: Reverse open/close logic

**Park Sensor Types**:
- **Physical Only**: Use local GPIO input
- **UDP Only**: Use network-based sensors
- **Both (AND)**: Require both sensors to indicate parked

## 🚀 Usage

### ASCOM Alpaca Control

1. **Discovery**:
   - Device broadcasts on UDP port 32227
   - Responds to "alpacadiscovery1" packets
   - ASCOM applications auto-discover device

2. **API Endpoint**:
   - Base URL: `http://<device-ip>:11111/api/v1/dome/0/`
   - Device Type: Dome (RollOffRoof)
   - ASCOM API Version: 1

3. **Supported Methods**:
   - `connected` - Get/set connection state
   - `openshutter` - Start opening roof
   - `closeshutter` - Start closing roof
   - `abortslew` - Stop roof movement
   - `shutterstatus` - Get current roof state (0=Open, 1=Closed, 2=Opening, 3=Closing, 4=Error)
   - `athome` - Check if roof is closed
   - `atpark` - Check if roof is open

### Web Interface Control

Access `http://<device-ip>/` for browser-based control:

- View real-time status
- Monitor telescope park state
- Check limit switches
- View inverter power states
- Control inverter manually (v3)
- Configure settings
- Update firmware (OTA)

### MQTT Control

**Command Topic**: `observatory/roof/command`

**Commands**:
```json
{"command": "open"}
{"command": "close"}
{"command": "stop"}
```

**Status Topic**: `observatory/roof/status`

Publishes status every 30 seconds and on state changes.

## 📡 MQTT Integration

### Configuration

Configure MQTT settings via web interface:
- **MQTT Broker**: IP address or hostname
- **Port**: Default 1883
- **Username**: MQTT credentials
- **Password**: MQTT credentials
- **Topic Prefix**: Default `observatory/roof`

### Topics

**Status Topic**: `<prefix>/status`
```json
{
  "status": "Closed",
  "slaved": false,
  "telescope_parked": true,
  "limit_open": false,
  "limit_closed": true,
  "bypass_enabled": false,
  "inverter_relay_state": false,
  "inverter_ac_power_state": false,
  "device_id": "ESP32-XXXXXX",
  "ip_address": "192.168.1.100",
  "version": "3.0.0"
}
```

**Command Topic**: `<prefix>/command`

**Availability Topic**: `<prefix>/availability`
- Payload: `online` or `offline`
- LWT (Last Will Testament) enabled

### Home Assistant Integration

Example configuration for Home Assistant:

```yaml
mqtt:
  cover:
    - name: "Observatory Roof"
      command_topic: "observatory/roof/command"
      state_topic: "observatory/roof/status"
      state_open: "Open"
      state_closed: "Closed"
      payload_open: '{"command":"open"}'
      payload_close: '{"command":"close"}'
      payload_stop: '{"command":"stop"}'
      value_template: "{{ value_json.status }}"
      availability_topic: "observatory/roof/availability"

  binary_sensor:
    - name: "Telescope Parked"
      state_topic: "observatory/roof/status"
      value_template: "{{ value_json.telescope_parked }}"
      payload_on: true
      payload_off: false

    - name: "Inverter Power"
      state_topic: "observatory/roof/status"
      value_template: "{{ value_json.inverter_ac_power_state }}"
      payload_on: true
      payload_off: false
```

## 🔌 API Reference

### Web API Endpoints

#### Status and Control
- `GET /` - Main status page (HTML)
- `GET /setup` - Configuration page (HTML)
- `POST /setup` - Save configuration

#### Alpaca API
- `GET /api/v1/dome/0/connected` - Connection status
- `PUT /api/v1/dome/0/openshutter` - Open roof
- `PUT /api/v1/dome/0/closeshutter` - Close roof
- `PUT /api/v1/dome/0/abortslew` - Stop movement
- `GET /api/v1/dome/0/shutterstatus` - Get roof status

#### Inverter Control (v3)
- `POST /inverter_toggle` - Toggle K1 power relay
- `POST /inverter_button` - Send K3 button press
- `GET /inverter_status` - Get inverter states (JSON)

#### Configuration
- `POST /set_pins` - Update pin configuration
- `POST /toggle_bypass` - Toggle park sensor bypass
- `POST /park_sensor_type` - Set park sensor type
- `POST /restart` - Restart device

#### OTA Updates
- `/update` - ElegantOTA web interface

## 📦 PCB Files

The `PCB Files/` directory contains all manufacturing files for v2.4 hardware:

| File | Description |
|------|-------------|
| `Schematic_v2.4.pdf` | Complete schematic diagram |
| `PCB_v2.4.pdf` | PCB layout and dimensions |
| `3D_PCB_v2.4.png` | 3D rendering of assembled board |
| `3D_PCB_v2.4.step` | 3D CAD model (STEP format) |
| `Gerber_PCB_v2.4.zip` | Gerber files for PCB manufacturing |
| `BOM_Board_v2.4_Schematic_v2.4.xlsx` | Bill of Materials |
| `PickAndPlace_PCB_v2.4.xlsx` | Pick and place coordinates for assembly |
| `FlyingProbeTesting_PCB_v2_4.zip` | Flying probe test files |

### Manufacturing

**PCB Specifications**:
- Layers: 2
- Board Size: Custom (see PCB_v2.4.pdf)
- PCB Thickness: 1.6mm
- Copper Weight: 1oz
- Silkscreen: Both sides
- Solder Mask: Green (standard)

**Recommended Manufacturers**:
- JLCPCB
- PCBWay
- OSH Park

Simply upload the Gerber zip file and BOM/Pick&Place for assembly services.

## 📝 Changelog

### v3.0.0 (2026-01-07)

**Hardware**:
- Migrated from ESP32 to ESP32-S3 (44-pin)
- Added K3 relay for inverter soft-power button
- Added AC power detection via optocoupler (GPIO7)
- Added support for 12V snow sensor (digital + RS485)
- Improved power supply design
- Updated all GPIO pin assignments

**Software**:
- Implemented dual inverter power state monitoring
- Added manual inverter control functions
- Enhanced web interface with inverter control card
- Added MQTT topics for inverter states
- Added API endpoints for inverter control
- Updated firmware to v3.0.0

**Features**:
- `toggleInverterPower()` - Manual K1 relay control
- `sendInverterButtonPress()` - K3 button press control
- Real-time AC power monitoring
- Web UI buttons for manual inverter control

### v2.1.0 (Previous)
- Initial public release
- ESP32 hardware support
- Basic roof control functionality
- ASCOM Alpaca implementation
- MQTT integration
- UDP park sensors

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👤 Author

**Corey Smart**

## 🤝 Contributing

Contributions, issues, and feature requests are welcome!

## ⭐ Support

If you find this project useful, please consider giving it a star on GitHub!

## 📮 Contact

For questions, issues, or suggestions, please open an issue on GitHub.

---

**Note**: This is specialized hardware for controlling observatory equipment. Always test thoroughly in a safe environment before deploying to a production observatory. Ensure proper safety interlocks and redundancy for critical systems.
