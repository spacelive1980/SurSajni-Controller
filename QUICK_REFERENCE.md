# Quick Reference - Sursajni ESP-IDF Firmware

## Essential Commands

```bash
# Environment Setup (run once per terminal session)
. $HOME/esp/esp-idf/export.sh

# Build Workflow
idf.py set-target esp32          # Configure for ESP32
idf.py menuconfig                # Optional: configure project
idf.py build                     # Compile firmware
idf.py -p PORT flash             # Flash to device
idf.py -p PORT monitor           # View serial output
idf.py -p PORT flash monitor     # Flash and monitor

# Maintenance
idf.py fullclean                 # Clean all build files
idf.py erase-flash               # Erase device flash
idf.py size                      # Show binary size breakdown
```

## Pin Quick Reference

| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| LoRa CS | 5 | Out | SPI chip select |
| LoRa RST | 14 | Out | Reset pin |
| LoRa DIO0 | 2 | In | IRQ |
| SPI CLK | 18 | Out | SPI clock |
| SPI MISO | 19 | In | SPI MISO |
| SPI MOSI | 23 | Out | SPI MOSI |
| I2C SDA | 21 | I/O | I2C data |
| I2C SCL | 22 | Out | I2C clock |
| RX LED | 13 | Out | Status LED |
| Heartbeat | 27 | Out | Status LED |
| Battery | 33 | Out | Status LED |
| Pump Relay | 16 | Out | Pump control |
| Valve Relay | 17 | Out | Valve control |
| Button SET | 4 | In | Pull-up |
| Button UP | 0 | In | Pull-up |
| Button DOWN | 3 | In | Pull-up |
| Buzzer | 26 | PWM | LEDC |
| Turbidity | 34 | ADC | Analog in |

## I2C Addresses

| Device | Address | Notes |
|--------|---------|-------|
| DS3231 RTC | 0x68 | 7-bit address |
| SSD1306 OLED | 0x3C | 7-bit address |

## File Locations

```
main/main.cpp           # Main application code
main/logo.h             # OLED logo graphics

components/lora/        # LoRa SX1276/SX1278 driver
components/u8g2/        # U8g2 OLED library
components/ds3231/      # DS3231 RTC library

sdkconfig.defaults      # Default ESP32 configuration
partitions.csv          # Flash partition table
```

## Common Tasks

### Add New Source File
1. Create file in `main/` or component directory
2. No CMakeLists.txt update needed (auto-discovered)

### Add New Component
1. Create `components/mycomponent/`
2. Add source files to `components/mycomponent/src/`
3. Add headers to `components/mycomponent/include/`
4. Create `components/mycomponent/CMakeLists.txt`:
   ```cmake
   idf_component_register(SRCS "src/myfile.cpp"
                          INCLUDE_DIRS "include"
                          REQUIRES driver)
   ```

### Change Log Level
In code:
```cpp
esp_log_level_set("TAG", ESP_LOG_DEBUG);
```

Via menuconfig:
```
Component config → Log output → Default log verbosity
```

### NVS (Settings Storage)
```cpp
// Open
nvs_handle_t handle;
nvs_open("storage", NVS_READWRITE, &handle);

// Write
nvs_set_u8(handle, "key", value);
nvs_commit(handle);

// Read
uint8_t value;
nvs_get_u8(handle, "key", &value);

// Close
nvs_close(handle);
```

### GPIO Operations
```cpp
// Configure
gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);

// Write
gpio_set_level(GPIO_NUM_16, 1);  // HIGH
gpio_set_level(GPIO_NUM_16, 0);  // LOW

// Read
int level = gpio_get_level(GPIO_NUM_4);
```

### FreeRTOS Task
```cpp
void my_task(void *pvParameters) {
    while(1) {
        // Task code here
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second delay
    }
}

// Create task
xTaskCreatePinnedToCore(
    my_task,           // Function
    "MyTask",          // Name
    4096,              // Stack size
    NULL,              // Parameters
    5,                 // Priority
    NULL,              // Handle
    0                  // Core (0 or 1)
);
```

## Debugging Tips

### Enable Debug Output
```cpp
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

static const char* TAG = "MyComponent";
ESP_LOGD(TAG, "Debug message: %d", value);
ESP_LOGI(TAG, "Info message");
ESP_LOGW(TAG, "Warning");
ESP_LOGE(TAG, "Error");
```

### Monitor Heap
```cpp
ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
ESP_LOGI(TAG, "Min free heap: %d", esp_get_minimum_free_heap_size());
```

### Backtrace on Crash
Serial monitor automatically decodes crash backtrace.
Use `addr2line` for manual decoding:
```bash
xtensa-esp32-elf-addr2line -pfiaC -e build/app.elf ADDRESS
```

## Web Interface

### Default Access
- **AP Mode**: http://192.168.4.1
- **STA Mode**: http://[device-ip]

### Default Credentials
- **Username**: admin
- **Password**: sirftumhareliye

### API Endpoints
```
GET  /                      # Main UI
GET  /get_settings          # All settings JSON
GET  /get_live_data         # Live sensor data
POST /update_all_settings   # Save settings
POST /toggle_pump           # Manual pump control
POST /reset_dry_run         # Reset dry run error
POST /reset_sensor_error    # Reset sensor error
POST /factory_reset         # Factory reset
```

## Troubleshooting

### Won't Flash
1. Hold BOOT, press RESET, release BOOT
2. Check USB cable (must support data)
3. Verify port permissions: `sudo chmod 666 /dev/ttyUSB0`

### Build Errors
```bash
# Clean and rebuild
idf.py fullclean
idf.py build

# Check ESP-IDF version
idf.py --version
# Should be v5.0 or later
```

### Runtime Crashes
1. Check serial monitor for panic message
2. Note the backtrace addresses
3. Decode with addr2line
4. Check task stack sizes
5. Look for buffer overflows

### LoRa Not Working
- Current implementation is stub
- See COMPONENT_SETUP.md for full implementation

### OLED Blank
- Current implementation is stub
- Check I2C connections
- Verify I2C address with scanner

## Performance Notes

- **CPU Frequency**: 240MHz (default)
- **Flash Speed**: 40MHz (default)
- **RAM**: 320KB internal SRAM
- **Flash**: 4MB (configured)

## Component Status

| Component | Status | Action |
|-----------|--------|--------|
| LoRa | ⚠️ Stub | Implement SPI driver |
| U8g2 | ⚠️ Stub | Add full library |
| DS3231 | ⚠️ Partial | Implement I2C |
| WiFi | ✅ Ready | Core ESP-IDF |
| NVS | ✅ Ready | Core ESP-IDF |
| HTTP Server | ⏳ Todo | Port from Arduino |

## Documentation Index

| File | Purpose |
|------|---------|
| README.md | Main project overview |
| BUILD_GUIDE.md | Build instructions |
| ESP-IDF-README.md | Technical details |
| COMPONENTS.md | Component info |
| COMPONENT_SETUP.md | Setup guide |
| CONVERSION_STATUS.md | Progress tracker |
| CONVERSION_SUMMARY.md | High-level summary |
| **QUICK_REFERENCE.md** | **This file** |

## Support Checklist

Before asking for help:
- [ ] Checked documentation
- [ ] Reviewed serial monitor output
- [ ] Verified hardware connections
- [ ] Tested with known-good cable
- [ ] Checked ESP-IDF version
- [ ] Tried clean rebuild
- [ ] Searched ESP-IDF docs

## Project Links

- Repository: https://github.com/spacelive1980/SurSajni-Controller
- ESP-IDF Docs: https://docs.espressif.com/projects/esp-idf/
- U8g2 Library: https://github.com/olikraus/u8g2
- Arduino-LoRa: https://github.com/sandeepmistry/arduino-LoRa

---

**Quick Tip**: Keep this file open in a side terminal for easy reference while developing!
