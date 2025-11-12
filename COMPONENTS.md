# ESP-IDF Component Dependencies

This document outlines the component dependencies needed for the Sursajni Controller ESP-IDF port.

## Required ESP-IDF Components

### Core Components (Built-in)
- `freertos` - FreeRTOS RTOS kernel
- `esp_system` - System initialization and utilities
- `esp_timer` - High-resolution timer API
- `esp_event` - Event loop library
- `nvs_flash` - Non-volatile storage
- `driver` - Peripheral drivers
- `spi_flash` - SPI flash access
- `esp_wifi` - WiFi driver
- `esp_netif` - Network interface abstraction
- `lwip` - Lightweight IP stack
- `esp_http_server` - HTTP server
- `esp_https_ota` - OTA updates
- `mdns` - mDNS service
- `esp_sntp` - SNTP time synchronization
- `wpa_supplicant` - WiFi authentication
- `esp_phy` - PHY layer

### GPIO & Peripheral Drivers
- `esp_driver_gpio` - GPIO driver
- `esp_driver_spi` - SPI driver
- `esp_driver_i2c` - I2C driver
- `esp_driver_ledc` - LED Controller (PWM)
- `esp_adc` - ADC driver

## Third-Party Libraries Needed

### 1. LoRa (SX1276/SX1278)
**Options:**
a) **arduino-LoRa** (by Sandeep Mistry) - Can be adapted for ESP-IDF
   - Repository: https://github.com/sandeepmistry/arduino-LoRa
   - Needs adaptation: Replace Arduino SPI calls with ESP-IDF SPI master

b) **esp32-lora-library** (Native ESP-IDF)
   - Repository: https://github.com/Inteform/esp32-lora-library
   - Already ESP-IDF compatible

**Recommended:** Create ESP-IDF component wrapper for arduino-LoRa

### 2. U8g2 OLED Library
**Source:** https://github.com/olikraus/u8g2
- U8g2 has ESP-IDF support built-in
- Create component in `components/u8g2`
- Include U8x8 HAL for ESP-IDF I2C

**Setup:**
```cmake
idf_component_register(SRCS "u8g2_esp32_hal.c"
                             "csrc/u8g2_bitmap.c"
                             "csrc/u8g2_box.c"
                             ...
                       INCLUDE_DIRS "csrc" "."
                       REQUIRES driver)
```

### 3. RTClib (DS3231)
**Options:**
a) **Adafruit RTClib** - Can be ported
   - Needs I2C adaptation

b) **Create native ESP-IDF DS3231 driver**
   - Implement I2C communication using ESP-IDF I2C driver
   - DateTime class can be kept as C++ class

**Recommended:** Create minimal DS3231 driver in components/ds3231

### 4. ArduinoJson
**Source:** https://arduinojson.org/
- ArduinoJson v6+ works with ESP-IDF
- Add as component or use ESP-IDF's cJSON instead

**Alternative:** Use ESP-IDF's `cJSON` library (built-in)

## Component Directory Structure

```
components/
├── lora/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── LoRa.h
│   └── LoRa.cpp
├── u8g2/
│   ├── CMakeLists.txt
│   ├── u8g2_esp32_hal.c
│   └── csrc/           # U8g2 source files
├── ds3231/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── RTClib.h
│   └── RTClib.cpp
└── web_server/
    ├── CMakeLists.txt
    ├── include/
    │   └── web_server.h
    └── web_server.cpp
```

## Creating Components

### Example: LoRa Component CMakeLists.txt

```cmake
idf_component_register(SRCS "LoRa.cpp"
                      INCLUDE_DIRS "include"
                      REQUIRES driver esp_driver_spi esp_driver_gpio)
```

### Example: DS3231 Component CMakeLists.txt

```cmake
idf_component_register(SRCS "RTClib.cpp"
                      INCLUDE_DIRS "include"
                      REQUIRES driver esp_driver_i2c)
```

### Example: U8g2 Component CMakeLists.txt

```cmake
set(U8G2_SRCS
    "csrc/u8g2_bitmap.c"
    "csrc/u8g2_box.c"
    "csrc/u8g2_buffer.c"
    # ... add all required u8g2 sources
    "u8g2_esp32_hal.c"
)

idf_component_register(SRCS ${U8G2_SRCS}
                      INCLUDE_DIRS "csrc" "include"
                      REQUIRES driver esp_driver_i2c)
```

## Implementation Steps

1. **Create Component Directories**
   ```bash
   mkdir -p components/{lora,u8g2,ds3231,web_server}
   ```

2. **Download/Create Library Sources**
   - Clone u8g2 repository into components/u8g2
   - Create LoRa component from arduino-LoRa
   - Create DS3231 component

3. **Create HAL Layers**
   - ESP-IDF I2C HAL for U8g2
   - ESP-IDF SPI HAL for LoRa
   - ESP-IDF I2C implementation for DS3231

4. **Update Main CMakeLists.txt**
   ```cmake
   set(EXTRA_COMPONENT_DIRS "components")
   ```

## Key Conversion Notes

### EEPROM → NVS
Replace all EEPROM calls:
```cpp
// Arduino
EEPROM.begin(size);
EEPROM.write(addr, value);
value = EEPROM.read(addr);
EEPROM.commit();

// ESP-IDF NVS
nvs_handle_t nvs_handle;
nvs_open("storage", NVS_READWRITE, &nvs_handle);
nvs_set_u8(nvs_handle, "key", value);
nvs_get_u8(nvs_handle, "key", &value);
nvs_commit(nvs_handle);
nvs_close(nvs_handle);
```

### WebServer → esp_http_server
```cpp
// Arduino
WebServer server(80);
server.on("/", handleRoot);
server.begin();

// ESP-IDF
httpd_handle_t server = NULL;
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
httpd_start(&server, &config);
httpd_register_uri_handler(server, &root_uri);
```

### Task Creation
```cpp
// Create loop task
xTaskCreatePinnedToCore(
    loop_task,        // Task function
    "loop",           // Name
    8192,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    NULL,             // Handle
    1);               // Core ID
```

## Testing Checklist

- [ ] LoRa communication working
- [ ] OLED display showing content
- [ ] RTC keeping time
- [ ] WiFi AP mode functional
- [ ] WiFi STA mode connecting
- [ ] Web server responding
- [ ] All GPIO pins functioning
- [ ] Buzzer PWM working
- [ ] Turbidity sensor reading ADC
- [ ] NVS saving/loading settings
- [ ] OTA updates working
- [ ] All FreeRTOS tasks running
