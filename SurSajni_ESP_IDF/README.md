# Sursajni Controller - ESP-IDF Version

**Native ESP-IDF implementation with full flash encryption and secure boot support**

This is a complete rewrite of the Sursajni Pump Controller using native ESP-IDF framework, specifically designed to support advanced security features like flash encryption and secure boot that are not fully available in Arduino framework.

## Why ESP-IDF Version?

The Arduino framework has limitations when it comes to security features:

❌ **Arduino Limitations:**
- Flash encryption not fully supported for compiled binaries
- Secure boot v2 requires manual configuration
- Limited control over partition tables
- NVS encryption harder to implement
- Less control over bootloader security

✅ **ESP-IDF Advantages:**
- **Full flash encryption support** - Native API, automatic encryption
- **Secure boot v2** - Complete implementation with signing keys
- **NVS encryption** - Built-in encrypted storage
- **Custom bootloader** - Full control over boot process
- **Production-ready security** - Tested and validated by Espressif

## Features

All features from the Arduino version, plus:

- ✅ **Flash Encryption Ready** - Works with both development and production modes
- ✅ **Secure Boot Compatible** - Sign firmware with your private key
- ✅ **NVS Encryption** - Settings stored encrypted in NVS
- ✅ **Optimized Performance** - Native IDF performance benefits
- ✅ **Advanced Debugging** - Full IDF debugging tools
- ✅ **OTA Updates** - Encrypted OTA firmware updates support
- ✅ **Modular Architecture** - Clean separation of concerns

## Project Structure

```
SurSajni_ESP_IDF/
├── CMakeLists.txt              # Main project configuration
├── sdkconfig.defaults          # Default configuration with security enabled
├── partitions.csv              # Custom partition table
├── README.md                   # This file
├── main/
│   ├── CMakeLists.txt          # Main component configuration
│   ├── main.c                  # Application entry point
│   ├── common.h                # Common definitions and types
│   ├── settings_manager.c/h   # NVS-based settings management
│   ├── pump_control.c/h        # Pump control logic
│   ├── sensor_manager.c/h     # Sensor reading and management
│   ├── wifi_manager.c/h        # WiFi AP/STA management
│   └── web_server.c/h          # HTTP server with REST API
└── components/                 # Future: Custom components
```

## Prerequisites

### 1. Install ESP-IDF

```bash
# Linux/macOS
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32

# Set up environment (add to ~/.bashrc or ~/.zshrc)
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

For Windows, follow: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html

### 2. Verify Installation

```bash
get_idf
idf.py --version
# Should show: ESP-IDF v4.4 or later
```

## Quick Start

### 1. Clone and Build

```bash
cd /path/to/SurSajni-Controller/SurSajni_ESP_IDF

# Set up IDF environment
get_idf

# Configure project (optional - defaults are set)
idf.py menuconfig

# Build
idf.py build
```

### 2. Flash Without Encryption (Development/Testing)

```bash
# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Or just flash
idf.py -p /dev/ttyUSB0 flash
```

### 3. Access the Device

- **WiFi AP:** Connect to "Sursajni-Controller" (password: sursajni123)
- **Web Interface:** http://192.168.4.1
- **API:** http://192.168.4.1/get_settings

## Enabling Flash Encryption

⚠️ **WARNING:** Flash encryption is a ONE-TIME operation. Test thoroughly first!

### Development Mode (Reflashable)

For testing, use development mode which allows reflashing:

```bash
# 1. Edit sdkconfig.defaults or use menuconfig
idf.py menuconfig

# Navigate to: Security features → Flash encryption mode
# Select: Development (NOT recommended for production)

# 2. Build with encryption enabled
idf.py build

# 3. Flash (first time only)
idf.py -p /dev/ttyUSB0 flash

# 4. Device will generate and burn encryption key on first boot
# Monitor the serial output to confirm
idf.py -p /dev/ttyUSB0 monitor
```

### Production Mode (Permanent, Most Secure)

For production devices:

```bash
# 1. Generate encryption key (SAVE THIS SECURELY!)
espsecure.py generate_flash_encryption_key my_flash_encryption_key.bin

# 2. Configure for production
idf.py menuconfig
# Security features → Flash encryption mode → Release

# 3. Build
idf.py build

# 4. Burn key to device (ONE-TIME, IRREVERSIBLE!)
espefuse.py --port /dev/ttyUSB0 burn_key \
    BLOCK_KEY0 my_flash_encryption_key.bin \
    XTS_AES_256_KEY

# 5. Flash encrypted firmware
esptool.py --port /dev/ttyUSB0 \
    --before default_reset --after no_reset \
    write_flash --encrypt \
    0x1000 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/sursajni_controller.bin

# 6. Burn encryption settings (ONE-TIME, IRREVERSIBLE!)
espefuse.py --port /dev/ttyUSB0 burn_efuse FLASH_CRYPT_CNT
espefuse.py --port /dev/ttyUSB0 burn_efuse FLASH_CRYPT_CONFIG 0xF

# 7. Reset device
esptool.py --port /dev/ttyUSB0 --after hard_reset read_mac
```

## Enabling Secure Boot

Secure Boot ensures only your signed firmware runs on the device.

```bash
# 1. Generate signing key (SAVE THIS SECURELY!)
espsecure.py generate_signing_key secure_boot_signing_key.pem

# 2. Configure secure boot
idf.py menuconfig
# Security features → Enable secure boot in bootloader → Yes

# 3. Build
idf.py build

# 4. Sign bootloader
espsecure.py sign_data --version 2 \
    --keyfile secure_boot_signing_key.pem \
    build/bootloader/bootloader.bin

# 5. Flash signed bootloader
esptool.py --port /dev/ttyUSB0 write_flash 0x1000 bootloader.bin.signed

# 6. Burn secure boot key (ONE-TIME, IRREVERSIBLE!)
espefuse.py --port /dev/ttyUSB0 burn_key \
    secure_boot_v2 secure_boot_signing_key_public.pem

# 7. Enable secure boot (ONE-TIME, IRREVERSIBLE!)
espefuse.py --port /dev/ttyUSB0 burn_efuse ABS_DONE_1
```

## Production Deployment Workflow

For maximum security (Flash Encryption + Secure Boot):

```bash
# 1. Generate keys (DO THIS ONCE, BACKUP SECURELY!)
espsecure.py generate_flash_encryption_key flash_key.bin
espsecure.py generate_signing_key --version 2 secure_boot_key.pem

# 2. Configure for production
idf.py menuconfig
# Enable both flash encryption (Release mode) and secure boot v2

# 3. Build
idf.py build

# 4. Sign firmware
espsecure.py sign_data --version 2 \
    --keyfile secure_boot_key.pem \
    build/sursajni_controller.bin

# 5. For each device:
#    a. Burn keys (ONE-TIME per device)
espefuse.py --port /dev/ttyUSB0 burn_key BLOCK_KEY0 flash_key.bin XTS_AES_256_KEY
espefuse.py --port /dev/ttyUSB0 burn_key secure_boot_v2 secure_boot_key_public.pem

#    b. Flash encrypted firmware
esptool.py --port /dev/ttyUSB0 write_flash --encrypt \
    0x1000 build/bootloader/bootloader.bin \
    0x10000 build/sursajni_controller.bin.signed

#    c. Enable security features (ONE-TIME per device)
espefuse.py --port /dev/ttyUSB0 burn_efuse FLASH_CRYPT_CNT
espefuse.py --port /dev/ttyUSB0 burn_efuse ABS_DONE_1

#    d. Write-protect security eFuses
espefuse.py --port /dev/ttyUSB0 burn_efuse WR_DIS_FLASH_CRYPT_CNT
espefuse.py --port /dev/ttyUSB0 burn_efuse WR_DIS_BLK0
```

## Updating Encrypted Firmware

### OTA Updates (Recommended for Production)

```c
// Include in your code
#include "esp_https_ota.h"

esp_http_client_config_t config = {
    .url = "https://yourserver.com/firmware.bin",
    .cert_pem = server_cert_pem_start,
};
esp_https_ota(&config);
```

### Manual Updates (Development)

With development mode flash encryption:

```bash
idf.py -p /dev/ttyUSB0 encrypted-app-flash monitor
```

With production mode:

```bash
# Must use encryption key
esptool.py --port /dev/ttyUSB0 write_flash --encrypt \
    0x10000 build/sursajni_controller.bin
```

## Configuration Options

### Via menuconfig

```bash
idf.py menuconfig
```

Key configuration sections:
- **Security features** - Flash encryption, secure boot, etc.
- **Component config → ESP32-specific** - WiFi, Bluetooth settings
- **Component config → HTTP Server** - Web server configuration
- **Component config → NVS** - Storage settings

### Via sdkconfig.defaults

Edit `sdkconfig.defaults` to set project defaults:

```ini
# Flash encryption (development mode for testing)
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y

# For production, change to:
# CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
# CONFIG_SECURE_BOOT_V2_ENABLED=y
```

## Partition Table

The project uses a custom partition table (`partitions.csv`):

| Name | Type | SubType | Offset | Size | Purpose |
|------|------|---------|--------|------|---------|
| nvs | data | nvs | 0x9000 | 24K | Settings storage (encrypted) |
| phy_init | data | phy | 0xf000 | 4K | RF calibration data |
| factory | app | factory | 0x10000 | 1M | Main firmware |
| storage | data | spiffs | 0x110000 | 3M | Web files, logs |

Total: 4MB flash

## API Endpoints

The web server provides RESTful API:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main page with status |
| `/get_settings` | GET | Get all settings and status (JSON) |
| `/toggle_pump` | POST | Toggle pump on/off |
| `/update_settings` | POST | Update settings |
| `/factory_reset` | POST | Reset to factory defaults |

## Troubleshooting

### Build Errors

**"Cannot find esp-idf"**
```bash
get_idf  # Make sure IDF environment is loaded
```

**"Python packages not found"**
```bash
cd $IDF_PATH
./install.sh esp32
```

### Flash Encryption Issues

**"Flash encryption is in an inconsistent state"**
- Flash encryption was partially enabled
- Solution: Completely erase chip and start over
```bash
esptool.py --port /dev/ttyUSB0 erase_flash
```

**"Cannot flash new firmware"**
- Production mode flash encryption is enabled
- Must use `--encrypt` flag or OTA updates

### Secure Boot Issues

**"Signature verification failed"**
- Firmware not signed with correct key
- Re-sign with proper signing key

**"Cannot disable secure boot"**
- Secure boot is permanent once enabled
- Cannot be reversed

### Common Errors

**"espefuse.py command not found"**
```bash
pip install esptool
```

**"Permission denied /dev/ttyUSB0"**
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in
```

## Differences from Arduino Version

| Feature | Arduino | ESP-IDF |
|---------|---------|---------|
| Flash Encryption | Limited | Full support |
| Secure Boot | Manual | Native |
| NVS Encryption | Complex | Built-in |
| Build System | Arduino IDE | CMake |
| Code Structure | Sketch | Modular C |
| Debugging | Limited | Full GDB |
| OTA Updates | Basic | Advanced |
| Performance | Good | Better |
| Learning Curve | Easy | Moderate |

## Migration from Arduino Version

The ESP-IDF version maintains API compatibility:
- Same pin assignments
- Same settings structure
- Same REST API endpoints
- Compatible web interface

To migrate:
1. Use this ESP-IDF version for new devices
2. Existing Arduino devices can continue running
3. Web interface works with both versions

## Development Tips

### Logging

```c
#include "esp_log.h"

static const char *TAG = "MYMODULE";

ESP_LOGI(TAG, "Info message");
ESP_LOGW(TAG, "Warning: %d", value);
ESP_LOGE(TAG, "Error occurred!");
ESP_LOGD(TAG, "Debug info (only if verbosity enabled)");
```

### Debugging

```bash
# With hardware debugger (JTAG)
idf.py openocd gdb

# Monitor with filters
idf.py monitor --port /dev/ttyUSB0

# Core dump analysis
idf.py coredump-info
```

### Performance Profiling

```bash
# Enable in menuconfig: Component config → App Trace
idf.py app-trace
```

## Security Best Practices

1. ✅ **Always test on development board first**
2. ✅ **Keep encryption and signing keys in secure location**
3. ✅ **Use hardware security module (HSM) for key storage in production**
4. ✅ **Enable both flash encryption AND secure boot for maximum security**
5. ✅ **Disable JTAG in production**: `espefuse.py burn_efuse JTAG_DISABLE`
6. ✅ **Disable ROM download mode**: `espefuse.py burn_efuse DOWNLOAD_DIS`
7. ✅ **Use encrypted OTA updates**
8. ✅ **Implement firmware version checking**
9. ✅ **Regular security audits**
10. ✅ **Document your security configuration**

## Resources

- **ESP-IDF Documentation:** https://docs.espressif.com/projects/esp-idf/en/latest/
- **Flash Encryption:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html
- **Secure Boot:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html
- **ESP-IDF Examples:** https://github.com/espressif/esp-idf/tree/master/examples
- **ESP32 Forum:** https://esp32.com/

## Contributing

This ESP-IDF version is part of the Sursajni Controller project. Contributions welcome!

## License

© Sursajni Automations 2025. All rights reserved.

---

**Need Help?**
- Open an issue on GitHub
- Check ESP-IDF documentation
- Ask on ESP32 forums

**Remember:** Flash encryption and secure boot are PERMANENT. Test thoroughly first!
