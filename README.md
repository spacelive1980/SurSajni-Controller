# Sursajni Wireless Pump Controller & Level Indicator

<div align="center">

![Version](https://img.shields.io/badge/version-2.0--espidf-blue)
![Platform](https://img.shields.io/badge/platform-ESP32-green)
![Framework](https://img.shields.io/badge/framework-ESP--IDF-orange)
![License](https://img.shields.io/badge/license-Proprietary-red)

**Advanced water pump automation system with wireless tank monitoring**

[Features](#features) • [Hardware](#hardware) • [Documentation](#documentation) • [Build](#quick-start) • [Status](#project-status)

</div>

## Overview

The Sursajni Controller is a comprehensive wireless water pump automation system designed for residential and commercial applications. It provides intelligent pump control with multiple operating modes, safety features, and a modern web interface for monitoring and configuration.

### Key Features

- 🎯 **Multiple Pump Modes**: Schedule, Auto, Manual, and Water Sensing
- 📡 **LoRa Wireless**: Long-range communication (433MHz) for remote tank monitoring
- 💧 **Tank Level Monitoring**: Real-time 5-level sensing (Empty, 25%, 50%, 75%, Full)
- 🛡️ **Safety Features**: Dry run protection, sensor error detection, turbidity monitoring
- 🌐 **Web Interface**: Modern responsive UI for configuration and monitoring
- 📅 **Scheduling**: Support for up to 10 time-based schedules
- 🔔 **Alert System**: Configurable buzzer patterns for different events
- 📊 **Live Monitoring**: Real-time sensor data and system status
- 🔄 **OTA Updates**: Over-the-air firmware updates via WiFi
- ⏰ **RTC Integration**: Accurate timekeeping with DS3231 module

## Hardware

### Main Controller (Receiver)
- **MCU**: ESP32 WROOM 38
- **Display**: 0.96" OLED (SSD1306, 128x64, I2C)
- **RTC**: DS3231 (I2C)
- **Wireless**: LoRa SX1276/SX1278 (433MHz, SPI)
- **Power**: 5V DC input

### Interfaces
- **Relays**: 2x (Pump + Valve)
- **LEDs**: 3x status indicators
- **Buzzer**: PWM controlled for alert tones
- **Buttons**: 3x (SET, UP, DOWN) for local configuration
- **Sensors**: Turbidity sensor (analog)

### Remote Transmitter
- Tank level sensors (5 float switches)
- Flow sensor
- Battery voltage monitoring
- LoRa communication

## Project Status

### Current Version: ESP-IDF Conversion

This repository contains the **ESP-IDF port** of the original Arduino firmware. The conversion is structurally complete with all components in place, but full hardware functionality requires completion of component implementations.

#### ✅ Complete (Ready to Build)
- ESP-IDF project structure
- All pin definitions and configurations
- Component stubs for external libraries
- Build system and documentation
- Converted main application code

#### ⏳ In Progress (Needs Implementation)
- Full LoRa driver (currently stub)
- Complete U8g2 OLED library integration
- DS3231 RTC I2C communication
- Web server conversion (Arduino → ESP-IDF)
- EEPROM → NVS migration

#### 📊 Progress: ~75% Structural, ~20% Functional

See [CONVERSION_STATUS.md](CONVERSION_STATUS.md) for detailed progress.

## Documentation

Comprehensive documentation is provided in multiple files:

| Document | Description |
|----------|-------------|
| [ESP-IDF-README.md](ESP-IDF-README.md) | Project overview and architecture |
| [BUILD_GUIDE.md](BUILD_GUIDE.md) | Complete build and flash instructions |
| [COMPONENTS.md](COMPONENTS.md) | Component dependencies and mappings |
| [COMPONENT_SETUP.md](COMPONENT_SETUP.md) | Guide to replace stub components |
| [CONVERSION_STATUS.md](CONVERSION_STATUS.md) | Detailed conversion progress |
| [CONVERSION_SUMMARY.md](CONVERSION_SUMMARY.md) | High-level conversion overview |

## Quick Start

### Prerequisites
- ESP-IDF v5.0 or later
- ESP32 WROOM 38 development board
- USB cable for programming

### Build

```bash
# Setup ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Clone repository
git clone https://github.com/spacelive1980/SurSajni-Controller.git
cd SurSajni-Controller

# Configure for ESP32
idf.py set-target esp32

# Build
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

See [BUILD_GUIDE.md](BUILD_GUIDE.md) for detailed instructions and troubleshooting.

## Pin Configuration

### ESP32 WROOM 38 Pin Assignments

| Peripheral | Pins | Notes |
|------------|------|-------|
| **LoRa Module** | CS: 5, RST: 14, DIO0: 2<br>SPI: SCK=18, MISO=19, MOSI=23 | SX1276/SX1278 |
| **I2C Bus** | SDA: 21, SCL: 22 | RTC + OLED |
| **OLED Display** | SDA: 21, SCL: 22 | SSD1306 128x64 |
| **RTC Module** | SDA: 21, SCL: 22 | DS3231 |
| **LEDs** | RX: 13, Heartbeat: 27, Battery: 33 | Status indicators |
| **Relays** | Pump: 16, Valve: 17 | High-triggered |
| **Buttons** | SET: 4, UP: 0, DOWN: 3 | Internal pull-ups |
| **Buzzer** | GPIO: 26 | PWM via LEDC |
| **Turbidity** | GPIO: 34 | ADC input |

## Features in Detail

### Pump Operating Modes

1. **Schedule Mode**: Time-based pump activation with up to 10 configurable schedules
2. **Auto Mode**: Level-based control (starts at empty, stops at full)
3. **Manual Mode**: Direct pump control via web interface or buttons
4. **Water Sensing Mode**: Turbidity-based auto start/stop

### Safety Features

- **Dry Run Protection**: Detects no-flow condition and stops pump
- **Retry Logic**: Configurable retry attempts with intervals
- **Sensor Error Detection**: Validates tank level sensor combinations
- **Turbidity Monitoring**: Detects dirty water and prevents pump damage
- **Overflow Prevention**: Stops pump at configured end level
- **Auto Recovery**: Optional automatic error reset on water detection

### Web Interface

Access at `http://192.168.4.1` (AP mode) or device IP (STA mode)

- Real-time system status and sensor data
- Live tank level visualization
- Schedule management
- Settings configuration
- Manual pump control
- Theme customization
- Responsive design for mobile

## Architecture

### Task Structure (FreeRTOS)
- **Main Loop Task** (Core 1): Pump control, display, buttons
- **LoRa Task** (Core 0): Packet reception and processing
- **Web UI Task** (Core 0): HTTP server request handling
- **NTP Sync Task** (Core 0): Time synchronization

### Communication Protocol
- **LoRa Packets**: Structured data with CRC validation
- **Acknowledgments**: Bidirectional with settings updates
- **REST API**: JSON-based configuration interface

## Development

### Adding New Features

1. Review architecture in [ESP-IDF-README.md](ESP-IDF-README.md)
2. Modify source in `main/main.cpp` or create new components
3. Update settings in NVS if needed
4. Add web API endpoints if required
5. Test thoroughly with hardware

### Contributing

This is proprietary software. For collaboration inquiries, contact Sursajni Automations.

## Support

### Troubleshooting

Common issues and solutions are documented in [BUILD_GUIDE.md](BUILD_GUIDE.md#troubleshooting).

For additional support:
1. Check documentation files
2. Review serial monitor output
3. Verify hardware connections
4. Check ESP-IDF version compatibility

## License

Copyright © 2025 Sursajni Automations. All rights reserved.

Proprietary software - Unauthorized copying, modification, or distribution is prohibited.

## Credits

**Developed by**: Amit Rajput  
**Company**: Sursajni Automations  
**Version**: 2.0 (ESP-IDF)  
**Year**: 2025

---

<div align="center">

**Made with ❤️ for smarter water management**

[Report Bug](https://github.com/spacelive1980/SurSajni-Controller/issues) • [Request Feature](https://github.com/spacelive1980/SurSajni-Controller/issues)

</div>
