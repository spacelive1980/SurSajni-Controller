# Sursajni Wireless Pump Controller - ESP32 Firmware

This is the ESP32-compatible firmware for the Sursajni Wireless Pump Controller. This firmware provides comprehensive pump automation with multiple control modes, safety features, and web-based configuration.

## Features

- **Multiple Pump Control Modes:**
  - Schedule Mode: Time-based automatic control
  - Auto Mode: Tank level-based control
  - Manual Mode: User-controlled operation
  - Water Sensing Mode: Flow sensor-based control

- **Safety Features:**
  - Dry run protection
  - Flow monitoring
  - Retry logic with configurable attempts
  - Sensor error detection and bypass
  - Maximum run time protection

- **Sensor Integration:**
  - 5-level tank sensor (Empty, 25%, 50%, 75%, Full)
  - Turbidity sensor for water quality monitoring
  - Flow sensor for pump operation verification
  - Battery voltage monitoring
  - LoRa wireless communication for remote sensors

- **Web Interface:**
  - WiFi Access Point mode (default: Sursajni-Controller)
  - Station mode for home network connectivity
  - RESTful API for configuration and control
  - Real-time status monitoring
  - Compatible with the included index.html web interface

- **Display & Alerts:**
  - OLED display (128x64 SSD1306)
  - Buzzer with multiple beep styles
  - Customizable alert tones for different events

- **Schedule Management:**
  - Up to 10 configurable schedules
  - Multiple repeat types (daily, odd/even days, specific date)
  - RTC-based time keeping (DS3231)

## Hardware Requirements

### ESP32 Board
Compatible with:
- ESP32 DevKit
- ESP32-S2
- ESP32-S3
- ESP32-C3

### Required Components
- **RTC Module:** DS3231 (I2C)
- **OLED Display:** 128x64 SSD1306 (I2C)
- **LoRa Module:** SX1278/SX1276 (433MHz or 868MHz)
- **Relay Module:** 5V relay for pump control
- **Buzzer:** Active or passive buzzer
- **Sensors:**
  - 5x Float switches for tank level
  - Turbidity sensor (analog)
  - Flow sensor (analog)
  - Battery voltage divider circuit

### Pin Configuration

Default pin assignments (can be modified in the code):

```cpp
// Pump and Buzzer
#define PUMP_RELAY_PIN      26    // Pump control relay
#define BUZZER_PIN          25    // Buzzer for alerts

// Analog Sensors (ADC1)
#define TURBIDITY_PIN       34    // Turbidity sensor
#define FLOW_SENSOR_PIN     35    // Flow sensor
#define BATTERY_PIN         36    // Battery voltage (VP)

// Tank Level Sensors
#define TANK_LEVEL_0_PIN    32    // Empty sensor
#define TANK_LEVEL_1_PIN    33    // 25% sensor
#define TANK_LEVEL_2_PIN    27    // 50% sensor
#define TANK_LEVEL_3_PIN    14    // 75% sensor
#define TANK_LEVEL_4_PIN    12    // Full sensor

// LoRa Module (SPI)
#define LORA_SCK_PIN        18
#define LORA_MISO_PIN       19
#define LORA_MOSI_PIN       23
#define LORA_SS_PIN         5
#define LORA_RST_PIN        4
#define LORA_DIO0_PIN       2

// OLED Display (I2C)
#define OLED_SDA_PIN        21
#define OLED_SCL_PIN        22
```

## Required Libraries

Install these libraries via Arduino IDE Library Manager:

1. **WiFi** (built-in with ESP32)
2. **WebServer** (built-in with ESP32)
3. **Preferences** (built-in with ESP32)
4. **RTClib** by Adafruit - for DS3231 RTC
5. **Wire** (built-in) - for I2C communication
6. **SPI** (built-in) - for LoRa communication
7. **LoRa** by Sandeep Mistry - for LoRa radio
8. **Adafruit_SSD1306** by Adafruit - for OLED display
9. **Adafruit_GFX** by Adafruit - dependency for OLED

### Installation Commands

In Arduino IDE:
```
Sketch → Include Library → Manage Libraries
```

Search and install:
- RTClib
- LoRa
- Adafruit SSD1306
- Adafruit GFX Library

## Installation

### 1. Arduino IDE Setup

1. Install Arduino IDE (1.8.19 or later, or Arduino IDE 2.x)
2. Add ESP32 board support:
   - Go to File → Preferences
   - Add this URL to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search for "esp32" and install "ESP32 by Espressif Systems"

### 2. Upload the Firmware

1. Open `SurSajni_ESP32.ino` in Arduino IDE
2. Select your ESP32 board:
   - Tools → Board → ESP32 Arduino → (your board model)
3. Select the correct COM port:
   - Tools → Port → (your ESP32 port)
4. Configure board settings (for standard ESP32):
   - Upload Speed: 921600
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB
   - Partition Scheme: Default 4MB with spiffs
5. Click Upload button

### 3. Upload Web Interface (Optional)

To use the web interface without loading it from the repository:

1. Install the ESP32 Sketch Data Upload plugin
2. Create a `data` folder in your sketch folder
3. Copy `index.html` to the `data` folder
4. Use Tools → ESP32 Sketch Data Upload

## Configuration

### First Time Setup

1. **Connect to WiFi AP:**
   - SSID: `Sursajni-Controller`
   - Password: `sursajni123`
   - Navigate to: `http://192.168.4.1`

2. **Login:**
   - Default username: `admin`
   - Default password: `admin`
   - **Important:** Change these credentials immediately!

3. **Configure WiFi (Optional):**
   - Connect to your home network for remote access
   - The device will maintain both AP and Station modes

### Web Interface

Access the web interface at:
- AP Mode: `http://192.168.4.1`
- Station Mode: `http://<device-ip-address>`

The web interface provides:
- Real-time system status
- Pump control (start/stop)
- Schedule management
- Settings configuration
- Error reset functions
- Live sensor data

### API Endpoints

The firmware exposes these RESTful API endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main web interface |
| `/get_settings` | GET | Get all settings and status as JSON |
| `/update_all_settings` | POST | Update system settings |
| `/toggle_pump` | POST | Toggle pump on/off |
| `/reset_dry_run` | POST | Reset dry run error |
| `/reset_sensor_error` | POST | Reset sensor error |
| `/factory_reset` | POST | Factory reset (erases all settings) |
| `/get_live_data` | GET | Get live sensor readings |

All endpoints require HTTP Basic Authentication.

## Differences from Arduino Version

This ESP32 version includes enhancements over a standard Arduino implementation:

### Improvements
- **NVS Storage:** Uses ESP32's NVS (Non-Volatile Storage) instead of EEPROM
- **Dual WiFi Mode:** Supports both AP and Station modes simultaneously
- **Better Performance:** 240MHz CPU, more memory (520KB SRAM)
- **More ADC Pins:** 18 ADC channels vs 6-8 on Arduino
- **Native WiFi:** Built-in WiFi with better range and stability
- **OTA Updates:** Can be extended to support Over-The-Air firmware updates

### Pin Differences
- ESP32 uses different pin numbers than Arduino
- Some ESP32 pins are input-only (34-39)
- ADC2 pins cannot be used when WiFi is active (use ADC1)

### Voltage Levels
- **ESP32 is 3.3V:** Do not connect 5V signals directly to GPIO pins
- Use level shifters for 5V sensors if needed
- Most sensors work fine with 3.3V logic

## Troubleshooting

### Cannot Upload Firmware
- Hold the BOOT button while uploading
- Check if the correct COM port is selected
- Reduce upload speed to 115200
- Check USB cable (some cables are charge-only)

### WiFi Not Connecting
- Check SSID and password in settings
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check WiFi signal strength
- Restart the device

### Sensors Not Reading
- Verify pin connections
- Check voltage levels (ESP32 is 3.3V)
- Use multimeter to test sensors
- Check serial monitor for error messages

### Pump Not Starting
- Check relay connections
- Verify pump mode setting
- Check for dry run or sensor errors
- Review tank level readings

### OLED Display Not Working
- Verify I2C connections (SDA, SCL)
- Check I2C address (default 0x3C)
- Ensure proper power supply to OLED

### LoRa Not Working
- Check SPI connections
- Verify antenna connection
- Ensure correct frequency (433MHz or 868MHz)
- Check LoRa module power supply

## Customization

### Changing Pin Assignments

Edit the pin definitions at the top of `SurSajni_ESP32.ino`:

```cpp
#define PUMP_RELAY_PIN      26    // Change to your pin
#define BUZZER_PIN          25    // Change to your pin
// ... etc
```

### Changing Default WiFi Credentials

In the `setupWiFi()` function:

```cpp
WiFi.softAP("Your-SSID", "Your-Password");
```

### Changing LoRa Frequency

In the `setupLoRa()` function:

```cpp
LoRa.begin(433E6);  // Change to 868E6 for 868MHz
```

### Adjusting Sensor Thresholds

Modify the settings or use the web interface to adjust:
- Turbidity limit
- Water presence threshold
- Tank level trigger points
- Dry run delay

## Security Considerations

1. **Change Default Password:** Always change the default admin password
2. **Network Security:** Use WPA2 encryption for your WiFi
3. **Firmware Updates:** Keep the firmware updated
4. **Physical Access:** Secure the device to prevent unauthorized access

## License

This firmware is provided as-is for the Sursajni Pump Controller project.

## Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check the troubleshooting section
- Review the serial monitor output for debugging

## Version History

### v1.0.0-ESP32 (Initial Release)
- Full ESP32 port from Arduino version
- Web server with RESTful API
- NVS-based settings storage
- All original features implemented
- Enhanced with dual WiFi mode

## Credits

Developed for the Sursajni Wireless Pump Controller project.

---

**Note:** Always test the firmware in a safe environment before deploying to control actual pumps or water systems.
