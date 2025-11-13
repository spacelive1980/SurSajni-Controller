# Arduino to ESP32 Conversion Guide

This document explains the key differences between the Arduino and ESP32 versions of the Sursajni Controller firmware.

## Overview

The ESP32 firmware maintains 100% feature compatibility with the original Arduino version while adding enhancements specific to the ESP32 platform.

## Main Differences

### 1. Microcontroller Capabilities

| Feature | Arduino (Uno/Mega) | ESP32 |
|---------|-------------------|-------|
| CPU Speed | 16 MHz | 240 MHz (dual-core) |
| RAM | 2-8 KB | 520 KB |
| Flash | 32-256 KB | 4 MB (typical) |
| GPIO Pins | 14-54 digital | 34 GPIO |
| ADC Channels | 6-16 | 18 (12-bit) |
| WiFi | External module | Built-in |
| Bluetooth | No | Built-in |
| Operating Voltage | 5V | 3.3V |

### 2. Code Changes Required

#### Include Headers
**Arduino:**
```cpp
#include <Ethernet.h>
#include <EEPROM.h>
```

**ESP32:**
```cpp
#include <WiFi.h>
#include <Preferences.h>  // Instead of EEPROM
```

#### WiFi Setup
**Arduino (with Ethernet shield):**
```cpp
#include <Ethernet.h>
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
Ethernet.begin(mac);
```

**ESP32:**
```cpp
#include <WiFi.h>
WiFi.softAP("SSID", "password");  // AP mode
WiFi.begin(ssid, password);        // Station mode
```

#### Storage
**Arduino:**
```cpp
#include <EEPROM.h>
EEPROM.begin();
EEPROM.write(addr, value);
EEPROM.commit();
```

**ESP32:**
```cpp
#include <Preferences.h>
Preferences preferences;
preferences.begin("namespace", false);
preferences.putUChar("key", value);
preferences.end();
```

#### Web Server
**Arduino (with Ethernet shield):**
```cpp
#include <Ethernet.h>
EthernetServer server(80);
```

**ESP32:**
```cpp
#include <WebServer.h>
WebServer server(80);
server.on("/", handleRoot);
server.begin();
```

### 3. Pin Configuration Differences

#### Voltage Levels
- **Arduino:** 5V logic (most pins are 5V tolerant)
- **ESP32:** 3.3V logic (pins are NOT 5V tolerant!)

⚠️ **Important:** Never connect 5V signals directly to ESP32 GPIO pins!

#### Analog Inputs
**Arduino:**
- 10-bit ADC (0-1023)
- Reference voltage: 5V
- Simple: `analogRead(A0)`

**ESP32:**
- 12-bit ADC (0-4095)
- Reference voltage: 3.3V
- Two ADC banks: ADC1 (GPIO32-39), ADC2 (GPIO0-27)
- **ADC2 cannot be used when WiFi is active!**
- Use ADC1 pins for all analog sensors

#### PWM
**Arduino:**
```cpp
analogWrite(pin, value);  // 8-bit (0-255)
```

**ESP32:**
```cpp
// Use LEDC peripheral
ledcSetup(channel, freq, resolution);
ledcAttachPin(pin, channel);
ledcWrite(channel, dutyCycle);
```

For simple tone/buzzer:
```cpp
tone(pin, frequency, duration);  // Still works!
```

### 4. Pin Mapping

#### Example Arduino Uno Pin Mapping
```cpp
// Arduino Uno
#define PUMP_RELAY_PIN      7
#define BUZZER_PIN          8
#define TANK_LEVEL_0_PIN    2
#define TANK_LEVEL_1_PIN    3
#define TANK_LEVEL_2_PIN    4
#define TANK_LEVEL_3_PIN    5
#define TANK_LEVEL_4_PIN    6
#define TURBIDITY_PIN       A0
#define FLOW_SENSOR_PIN     A1
#define BATTERY_PIN         A2
```

#### ESP32 Pin Mapping (This Firmware)
```cpp
// ESP32
#define PUMP_RELAY_PIN      26
#define BUZZER_PIN          25
#define TANK_LEVEL_0_PIN    32
#define TANK_LEVEL_1_PIN    33
#define TANK_LEVEL_2_PIN    27
#define TANK_LEVEL_3_PIN    14
#define TANK_LEVEL_4_PIN    12
#define TURBIDITY_PIN       34   // ADC1
#define FLOW_SENSOR_PIN     35   // ADC1
#define BATTERY_PIN         36   // ADC1
```

### 5. Libraries

#### Arduino Libraries
```
- Ethernet.h (for W5100/W5500)
- EEPROM.h
- Wire.h
- SPI.h
- RTClib.h
- Adafruit_SSD1306.h
- LoRa.h
```

#### ESP32 Libraries (Same + Built-in)
```
- WiFi.h (built-in)
- WebServer.h (built-in)
- Preferences.h (built-in, replaces EEPROM)
- Wire.h (built-in)
- SPI.h (built-in)
- RTClib.h (same)
- Adafruit_SSD1306.h (same)
- LoRa.h (same)
```

### 6. Features Added in ESP32 Version

#### 1. Dual WiFi Mode
```cpp
WiFi.mode(WIFI_MODE_APSTA);  // Both AP and Station
```
- Run as Access Point for direct connection
- Connect to home WiFi simultaneously
- No Ethernet shield required

#### 2. NVS Storage
- More reliable than EEPROM
- Key-value storage
- Namespace support
- Automatic wear leveling

#### 3. Better Performance
- Faster processing
- More memory for features
- Smoother operation
- Can handle more schedules

#### 4. Enhanced Security
- Built-in hardware encryption
- Secure boot (can be enabled)
- Flash encryption support

### 7. Code Structure Comparison

#### Arduino Sketch Structure
```
Arduino_Sketch/
├── Arduino_Sketch.ino       (Main file, all code)
└── config.h                 (Optional configuration)
```

#### ESP32 Sketch Structure
```
SurSajni_ESP32/
├── SurSajni_ESP32.ino       (Main file, core logic)
├── WebServer.ino            (Web server handlers)
├── README.md                (Documentation)
├── WIRING.md                (Wiring guide)
├── QUICKSTART.md            (Quick start)
└── platformio.ini           (PlatformIO config)
```

### 8. Migration Checklist

If converting existing Arduino code to ESP32:

#### Hardware
- [ ] Verify all components are 3.3V compatible
- [ ] Add voltage dividers for 5V sensors
- [ ] Update pin connections (no 5V pins!)
- [ ] Replace Ethernet shield with WiFi
- [ ] Check power supply (ESP32 uses more current)

#### Software
- [ ] Change include headers
- [ ] Update pin definitions
- [ ] Convert EEPROM to Preferences
- [ ] Update web server code
- [ ] Adjust analog read values (1023 → 4095)
- [ ] Use ADC1 pins only for analog sensors
- [ ] Update board selection in Arduino IDE

#### Testing
- [ ] Test without connecting to pump
- [ ] Verify all sensors read correctly
- [ ] Check voltage levels with multimeter
- [ ] Test WiFi connection
- [ ] Test web interface
- [ ] Test all safety features
- [ ] Load test for 24 hours

### 9. Performance Improvements

#### Memory Usage
**Arduino Mega:**
- SRAM: 8 KB
- Program storage: 256 KB
- EEPROM: 4 KB

**ESP32:**
- SRAM: 520 KB (65x more!)
- Program storage: 4 MB (16x more!)
- NVS: Typically 20+ KB

#### Network Performance
**Arduino with Ethernet:**
- Wired only
- No AP mode
- Limited to local network
- Speed: 10/100 Mbps

**ESP32 WiFi:**
- Wireless
- AP and Station modes
- Can connect from anywhere (with port forward)
- Speed: 802.11 b/g/n (up to 150 Mbps)

### 10. Power Consumption

**Arduino Uno:**
- Active: ~50 mA
- Sleep: ~15 mA (with external modules)

**ESP32:**
- Active (WiFi on): ~80-160 mA
- Active (WiFi off): ~20 mA
- Light sleep: ~0.8 mA
- Deep sleep: ~10 µA

**For battery operation:** ESP32 deep sleep is much better!

### 11. Common Pitfalls When Converting

#### 1. Voltage Levels
❌ **Wrong:**
```cpp
// Connecting 5V sensor directly to ESP32
5V Sensor OUT → ESP32 GPIO34
```

✅ **Correct:**
```cpp
// Using voltage divider
5V Sensor OUT → 10kΩ → ESP32 GPIO34
                     ↓
                   6.8kΩ → GND
```

#### 2. ADC2 with WiFi
❌ **Wrong:**
```cpp
#define SENSOR_PIN 0  // GPIO0 is ADC2
analogRead(SENSOR_PIN);  // Fails when WiFi active!
```

✅ **Correct:**
```cpp
#define SENSOR_PIN 34  // GPIO34 is ADC1
analogRead(SENSOR_PIN);  // Works with WiFi!
```

#### 3. EEPROM Size
❌ **Wrong:**
```cpp
EEPROM.write(500, value);  // EEPROM size not defined
```

✅ **Correct:**
```cpp
preferences.putUChar("key", value);  // No size limit
```

#### 4. Pin Numbers
❌ **Wrong:**
```cpp
#define LED_PIN 13  // Arduino built-in LED
```

✅ **Correct:**
```cpp
#define LED_PIN 2   // ESP32 built-in LED (varies by board)
```

### 12. Advantages of ESP32 Version

#### Why Choose ESP32?

1. **No Extra Modules Needed**
   - WiFi built-in (no shield required)
   - More GPIO pins
   - More memory

2. **Better Performance**
   - Faster CPU
   - Dual core
   - Better multitasking

3. **Lower Total Cost**
   - No Ethernet/WiFi shield ($10-30 saved)
   - No level shifters needed (with 3.3V sensors)
   - Fewer components

4. **More Features**
   - Bluetooth (for future expansion)
   - Touch sensors
   - Hall effect sensor
   - Ultra-low power modes

5. **Easier Development**
   - Built-in USB-to-Serial
   - No external programmer needed
   - Better debugging with more memory

### 13. Disadvantages of ESP32 Version

#### Why You Might Stick with Arduino?

1. **5V Logic**
   - Many sensors are 5V
   - No voltage dividers needed
   - Simpler wiring

2. **Established Ecosystem**
   - More shields available
   - Longer track record
   - More tutorials

3. **Simpler Power**
   - Standard 5V supply
   - Many USB chargers work
   - Easier to interface with 5V relays

4. **Industrial Environment**
   - Some prefer wired (Ethernet) over WiFi
   - More EMI resistant
   - Better isolation

### 14. Best Practices for ESP32

1. **Always use 3.3V logic or level shifters**
2. **Use ADC1 pins for analog sensors**
3. **Add decoupling capacitors on power rails**
4. **Use quality power supply (2A minimum)**
5. **Test thoroughly before production**
6. **Keep antenna clear of metal**
7. **Add ESD protection on exposed pins**
8. **Use proper grounding**

### 15. Future Expansion Possibilities

With ESP32, you can easily add:

1. **Bluetooth Control**
   - Control via mobile app
   - No WiFi needed

2. **OTA Updates**
   - Update firmware over WiFi
   - No USB cable needed

3. **SD Card Logging**
   - Log all events
   - Store historical data

4. **Touch Buttons**
   - Use built-in touch sensors
   - No mechanical buttons

5. **Camera Integration**
   - ESP32-CAM variant
   - Visual monitoring

6. **Cloud Integration**
   - Send data to cloud
   - Remote monitoring

7. **Voice Control**
   - Integration with Alexa/Google Home
   - Voice commands

### 16. Compatibility Matrix

| Feature | Arduino Uno | Arduino Mega | ESP32 |
|---------|-------------|--------------|-------|
| Pump Control | ✓ | ✓ | ✓ |
| Tank Sensors | ✓ | ✓ | ✓ |
| Schedules | 5 max | 10 | 10+ |
| WiFi | Shield | Shield | Built-in |
| Ethernet | Shield | Shield | Optional |
| Bluetooth | No | No | ✓ |
| OTA Updates | No | No | ✓ |
| Cost | $25 | $45 | $5-15 |

## Conclusion

The ESP32 version provides all the functionality of the Arduino version with these key improvements:

✅ **Built-in WiFi** - No shield required
✅ **More memory** - More features possible
✅ **Faster CPU** - Better performance
✅ **Lower cost** - $5-15 vs $25+ for Arduino + shields
✅ **Future-proof** - Room for expansion

The main consideration is ensuring all sensors and modules are 3.3V compatible or have proper level shifting.

For new projects, ESP32 is highly recommended. For existing Arduino projects, migration is straightforward with the guidelines in this document.

---

**Document Version:** 1.0
**Last Updated:** 2025
