# Sursajni Controller - ESP-IDF Firmware

This is the ESP-IDF port of the Sursajni wireless pump controller and level indicator firmware.

## Overview

The Sursajni Controller is a comprehensive water pump automation system with the following features:
- LoRa wireless communication for remote tank level monitoring
- 0.96" OLED display (SSD1306) for local status display
- RTC (DS3231) for schedule-based pump control
- Web interface for configuration and monitoring
- Multiple pump operation modes (Schedule, Auto, Manual, Water Sensing)
- Safety features (dry run protection, sensor error detection, turbidity monitoring)
- WiFi AP and STA modes
- OTA firmware updates

## Hardware Requirements

### ESP32 WROOM 38 Module

### Pin Configuration

**LoRa Module (SX1276/78):**
- CS: GPIO5
- RESET: GPIO14
- DIO0: GPIO2
- SCK: GPIO18
- MISO: GPIO19
- MOSI: GPIO23

**I2C Devices (RTC & OLED):**
- SDA: GPIO21
- SCL: GPIO22

**LEDs:**
- RX LED: GPIO13
- Heartbeat LED: GPIO27
- Battery LED: GPIO33

**Relays:**
- Pump Relay: GPIO16
- Valve Relay: GPIO17

**Buttons:**
- SET: GPIO4
- UP: GPIO0
- DOWN: GPIO3

**Buzzer:**
- GPIO26 (PWM controlled)

**Sensors:**
- Turbidity Sensor: GPIO34 (ADC)

## Project Structure

```
sursajni-controller/
├── CMakeLists.txt              # Root build configuration
├── sdkconfig.defaults          # Default configuration
├── partitions.csv              # Partition table
├── main/
│   ├── CMakeLists.txt          # Main component build config
│   ├── main.cpp                # Main application code
│   └── logo.h                  # Logo graphics for OLED
├── components/                 # External components (if any)
└── README.md                   # This file
```

## Building the Firmware

### Prerequisites

1. Install ESP-IDF v5.0 or later
2. Set up ESP-IDF environment:
```bash
. $HOME/esp/esp-idf/export.sh
```

### Build Commands

```bash
# Configure the project
idf.py set-target esp32

# Build the project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

## Features Converted from Arduino

### Core Libraries Mapping

| Arduino Library | ESP-IDF Equivalent |
|----------------|-------------------|
| WiFi.h | esp_wifi.h |
| WebServer.h | esp_http_server.h |
| EEPROM.h | nvs_flash.h (NVS) |
| Wire.h | driver/i2c.h |
| SPI.h | driver/spi_master.h |
| esp_task_wdt.h | esp_task_wdt.h (native) |
| ArduinoOTA.h | esp_ota_ops.h |
| NTPClient.h | esp_sntp.h |

### Pin Functions Preserved

All pin functions from the original Arduino firmware have been preserved:
- GPIO configurations
- PWM (LEDC) for buzzer
- ADC for turbidity sensor
- SPI for LoRa
- I2C for RTC and OLED
- All relay and LED controls

### FreeRTOS Tasks

The firmware uses multiple FreeRTOS tasks:
1. **Main Loop Task** (Core 1) - Handles pump control logic, display, buttons
2. **LoRa Task** (Core 0) - Handles LoRa packet reception  
3. **Web UI Task** (Core 0) - Handles HTTP server requests
4. **NTP Sync Task** (Core 0) - Handles periodic time synchronization

## Configuration

The system can be configured via:
1. **Web Interface** - Access via WiFi AP (default SSID: "Sursajni")
2. **Button Menu** - Use SET/UP/DOWN buttons on device
3. **NVS Storage** - Settings are persisted in non-volatile storage

### Default Settings

- **WiFi AP Mode**: SSID "Sursajni" (no password by default)
- **Web Interface**: http://192.168.4.1
- **Default Credentials**: admin / sirftumhareliye

## API Endpoints

The web server provides these RESTful API endpoints:
- `GET /` - Web interface HTML
- `GET /get_settings` - Get all system settings
- `GET /get_live_data` - Get live sensor data
- `POST /update_all_settings` - Update system settings
- `POST /update_theme` - Update UI theme
- `POST /reset_dry_run` - Reset dry run error
- `POST /reset_sensor_error` - Reset sensor error
- `POST /toggle_pump` - Manual pump control
- `POST /factory_reset` - Reset to factory defaults

## Development Notes

### Key Differences from Arduino

1. **Setup/Loop Pattern**: Arduino's `setup()` and `loop()` converted to ESP-IDF's `app_main()` and FreeRTOS tasks
2. **EEPROM**: Replaced with NVS (Non-Volatile Storage) API
3. **Delays**: `delay()` replaced with `vTaskDelay()` for non-blocking delays
4. **PWM**: Arduino's `analogWrite()` replaced with LEDC API
5. **Serial**: `Serial.print()` replaced with `ESP_LOGI()` and `printf()`

### Memory Management

- Uses FreeRTOS heap allocation
- NVS for persistent storage
- Proper task stack sizes allocated
- Watchdog timer configured with 50-second timeout

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure ESP-IDF is properly set up and environment variables are exported
2. **Flash Errors**: Check USB cable and port permissions
3. **WiFi Connection**: Reset device or clear NVS partition

### Serial Monitor

Use the serial monitor to view debug output:
```bash
idf.py -p /dev/ttyUSB0 monitor
```

Press `Ctrl+]` to exit the monitor.

## License

Copyright © 2025 Sursajni Automations. All rights reserved.

## Author

Developed by Amit Rajput for Sursajni Automations
