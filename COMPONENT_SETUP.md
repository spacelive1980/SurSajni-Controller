# Component Setup Guide

This guide explains how to set up the external library components for the ESP-IDF build.

## Quick Start - Option 1: Manual Component Setup (Recommended)

### 1. U8g2 OLED Library

```bash
cd components/u8g2
git clone https://github.com/olikraus/u8g2.git temp
cp -r temp/csrc/* ./
rm -rf temp
```

Create `components/u8g2/CMakeLists.txt`:
```cmake
file(GLOB U8G2_SRCS "u8g2_*.c" "u8x8_*.c")

idf_component_register(
    SRCS ${U8G2_SRCS} "u8g2_esp32_hal.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_i2c
)
```

Create `components/u8g2/u8g2_esp32_hal.c` - ESP-IDF I2C HAL implementation for U8g2.

### 2. LoRa Library

Option A - Use arduino-LoRa with modifications:
```bash
cd components/lora
wget https://github.com/sandeepmistry/arduino-LoRa/archive/refs/heads/master.zip
unzip master.zip
cp -r arduino-LoRa-master/src/* ./
# Modify LoRa.cpp to use ESP-IDF SPI instead of Arduino SPI
```

Option B - Use esp32-lora-library (already ESP-IDF compatible):
```bash
cd components/lora
git clone https://github.com/Inteform/esp32-lora-library.git
```

Create `components/lora/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "LoRa.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_spi esp_driver_gpio
)
```

### 3. DS3231 RTC Library

Create minimal ESP-IDF implementation:

`components/ds3231/include/RTClib.h`:
```cpp
#ifndef RTCLIB_H
#define RTCLIB_H

#include <stdint.h>
#include "driver/i2c.h"

class DateTime {
public:
    DateTime(uint16_t y, uint8_t m, uint8_t d, uint8_t hh, uint8_t mm, uint8_t ss);
    DateTime(uint32_t t = 0);
    DateTime(const char* date, const char* time);
    
    uint16_t year() const { return yOff + 2000; }
    uint8_t month() const { return m; }
    uint8_t day() const { return d; }
    uint8_t hour() const { return hh; }
    uint8_t minute() const { return mm; }
    uint8_t second() const { return ss; }
    uint32_t unixtime() const;
    
    DateTime operator+(const class TimeSpan& span);
    
private:
    uint8_t yOff, m, d, hh, mm, ss;
};

class TimeSpan {
public:
    TimeSpan(int32_t seconds = 0);
    TimeSpan(int16_t days, int8_t hours, int8_t minutes, int8_t seconds);
    int32_t totalseconds() const { return _seconds; }
private:
    int32_t _seconds;
};

class RTC_DS3231 {
public:
    bool begin();
    void adjust(const DateTime& dt);
    DateTime now();
    bool lostPower();
private:
    i2c_port_t i2c_num;
};

#endif
```

`components/ds3231/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "RTClib.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c
)
```

## Quick Start - Option 2: Component Registry (Future)

When components are published to ESP Component Registry:
```bash
idf.py add-dependency "u8g2^2.35.0"
idf.py add-dependency "lora^1.0.0"
```

## Verification

After setting up components, verify:
```bash
cd /path/to/sursajni-controller
idf.py reconfigure
```

Should show all components being discovered.

## Alternative: Pre-built Component Package

Download pre-configured components:
```bash
wget https://github.com/yourrepo/sursajni-components/archive/main.zip
unzip main.zip -d components/
```

## Common Issues

### Issue: Component not found
**Solution**: Ensure `set(EXTRA_COMPONENT_DIRS "components")` in root CMakeLists.txt

### Issue: Header not found
**Solution**: Check INCLUDE_DIRS in component's CMakeLists.txt

### Issue: Undefined references
**Solution**: Add missing REQUIRES in component's CMakeLists.txt

## Component Dependencies Graph

```
main
├── lora
│   ├── driver (SPI)
│   └── esp_driver_gpio
├── u8g2
│   └── esp_driver_i2c
├── ds3231
│   └── esp_driver_i2c
├── esp_http_server
├── nvs_flash
├── esp_wifi
└── ... (other ESP-IDF components)
```

## Testing Components

### Test LoRa:
```cpp
#include "LoRa.h"
void app_main() {
    if (!LoRa.begin(433E6)) {
        ESP_LOGE("LoRa", "Init failed");
    }
}
```

### Test U8g2:
```cpp
#include "u8g2.h"
void app_main() {
    u8g2_t u8g2;
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_sw_i2c, u8x8_gpio_and_delay_esp32);
    u8g2_InitDisplay(&u8g2);
}
```

### Test DS3231:
```cpp
#include "RTClib.h"
void app_main() {
    RTC_DS3231 rtc;
    if (!rtc.begin()) {
        ESP_LOGE("RTC", "Init failed");
    }
}
```

## Build Process

1. Configure: `idf.py set-target esp32`
2. Build: `idf.py build`
3. Flash: `idf.py -p /dev/ttyUSB0 flash monitor`

## Next Steps

After components are set up:
1. Complete main.cpp implementation
2. Test individual peripherals
3. Integrate full application logic
4. Hardware validation
