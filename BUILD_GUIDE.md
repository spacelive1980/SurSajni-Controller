# Build and Flash Guide

## Prerequisites

1. **ESP-IDF v5.0 or later** installed
2. ESP32 WROOM 38 module or development board
3. USB-to-Serial adapter (if not built into your board)

## Environment Setup

### Linux/macOS
```bash
# Install ESP-IDF (if not already installed)
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32

# Activate ESP-IDF environment
. $HOME/esp/esp-idf/export.sh
```

### Windows
```cmd
# Install ESP-IDF using the installer from:
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html

# Activate ESP-IDF environment
%userprofile%\esp\esp-idf\export.bat
```

## Building the Project

### 1. Clone the Repository
```bash
git clone https://github.com/spacelive1980/SurSajni-Controller.git
cd SurSajni-Controller
```

### 2. Set Target to ESP32
```bash
idf.py set-target esp32
```

### 3. Configure (Optional)
```bash
idf.py menuconfig
```

Navigate to configure:
- WiFi settings
- Serial flasher config  
- Component config

### 4. Build
```bash
idf.py build
```

This will:
- Compile all source files
- Link the binary
- Generate flash images

Expected output:
```
Project build complete. To flash, run:
idf.py -p (PORT) flash
```

## Flashing to Device

### 1. Connect ESP32
Connect your ESP32 to the computer via USB.

### 2. Identify the Port

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# Usually /dev/ttyUSB0
```

**macOS:**
```bash
ls /dev/cu.*
# Usually /dev/cu.usbserial-*
```

**Windows:**
- Check Device Manager for COM port
- Usually COM3, COM4, etc.

### 3. Flash the Firmware
```bash
# Linux/macOS
idf.py -p /dev/ttyUSB0 flash

# Windows
idf.py -p COM3 flash
```

### 4. Monitor Serial Output
```bash
# Linux/macOS
idf.py -p /dev/ttyUSB0 monitor

# Windows
idf.py -p COM3 monitor
```

Press `Ctrl+]` to exit the monitor.

### Combined Flash and Monitor
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Troubleshooting

### Build Errors

#### Error: Component not found
```
Solution: Ensure all components are in components/ directory:
- components/lora
- components/u8g2
- components/ds3231
```

#### Error: Header not found
```
Solution: Check INCLUDE_DIRS in component CMakeLists.txt
```

#### Error: Undefined reference
```
Solution: Add missing REQUIRES in component CMakeLists.txt
```

### Flash Errors

#### Error: Failed to connect
```
Solution:
1. Put ESP32 in download mode:
   - Hold BOOT button
   - Press and release RESET button
   - Release BOOT button
2. Check USB cable (use data cable, not charge-only)
3. Try different USB port
4. Check permissions: sudo usermod -a -G dialout $USER
```

#### Error: Chip ID mismatch
```
Solution: Ensure target is set correctly:
idf.py set-target esp32
```

#### Error: Flash size mismatch
```
Solution: Check flash size in menuconfig:
Serial flasher config → Flash size
Set to 4 MB for most ESP32 modules
```

### Runtime Errors

#### Device reboots continuously
```
Check serial monitor for:
- Panic messages
- Stack overflow
- Watchdog timeout

Solutions:
- Increase task stack sizes in sdkconfig
- Check for infinite loops
- Disable watchdog temporarily for debugging
```

#### WiFi not connecting
```
Solutions:
- Check WiFi credentials in code/web interface
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check router settings
```

#### Display not working
```
Solutions:
- Check I2C connections (SDA=21, SCL=22)
- Verify OLED address (usually 0x3C)
- Test with I2C scanner
```

#### LoRa not communicating
```
Solutions:
- Check SPI connections
- Verify LoRa frequency (433MHz/868MHz/915MHz)
- Check antenna connection
- Implement full LoRa driver (current is stub)
```

## Development Workflow

### 1. Edit Code
Make changes to `main/main.cpp` or component files.

### 2. Build
```bash
idf.py build
```

### 3. Flash and Monitor
```bash
idf.py flash monitor
```

### 4. Debug
- Use ESP_LOGI/ESP_LOGD/ESP_LOGE for logging
- Monitor serial output for errors
- Use JTAG debugger if available

## Advanced Operations

### Erase Flash (Factory Reset)
```bash
idf.py -p /dev/ttyUSB0 erase-flash
```

### Flash Only Bootloader
```bash
idf.py -p /dev/ttyUSB0 bootloader-flash
```

### Flash Only Partition Table
```bash
idf.py -p /dev/ttyUSB0 partition-table-flash
```

### Create Binary for External Flashing
```bash
idf.py build
# Binary is in build/sursajni-controller.bin
# Can be flashed with esptool.py or other tools
```

### OTA Update
Once the device is flashed and connected to WiFi:
1. Access web interface at http://[device-ip]
2. Upload new firmware bin file
3. Device will update and reboot

## Monitoring and Debugging

### Enable Debug Logging
In `sdkconfig`:
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
```

Or via menuconfig:
```
Component config → Log output → Default log verbosity → Debug
```

### View Task Statistics
Add in code:
```cpp
vTaskList(buffer);
ESP_LOGI(TAG, "Task list:\n%s", buffer);
```

### Monitor Heap Usage
```cpp
ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
```

## Performance Optimization

### Reduce Binary Size
- Disable unused components in menuconfig
- Use -Os optimization
- Enable LTO (Link Time Optimization)

### Increase Speed
- Use -O2 or -O3 optimization
- Pin performance-critical tasks to specific cores
- Increase CPU frequency to 240MHz

## Next Steps

After successful flash:
1. Connect to device WiFi AP (default: "Sursajni")
2. Access web interface at http://192.168.4.1
3. Configure settings
4. Connect tank level sensors
5. Test pump control

## Support

For issues:
1. Check CONVERSION_STATUS.md for known limitations
2. Review COMPONENTS.md for library status
3. Check ESP-IDF documentation
4. Open issue on GitHub

## Useful Commands Reference

```bash
# Full workflow
idf.py set-target esp32 && idf.py build && idf.py -p /dev/ttyUSB0 flash monitor

# Clean build
idf.py fullclean && idf.py build

# Show build size
idf.py size

# Show partition info
idf.py partition-table

# Monitor only (after flash)
idf.py -p /dev/ttyUSB0 monitor

# Monitor with specific baud rate
idf.py -p /dev/ttyUSB0 -b 921600 monitor
```
