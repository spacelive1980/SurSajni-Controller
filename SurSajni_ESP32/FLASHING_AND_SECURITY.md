# Flashing and Security Guide

This guide covers downloading the firmware, flashing to ESP32, and protecting your firmware from cloning.

## Part 1: Downloading and Flashing the Firmware

### Option A: Download from GitHub (Easiest)

1. **Download the Repository:**
   - Go to: https://github.com/spacelive1980/SurSajni-Controller
   - Click the green "Code" button
   - Select "Download ZIP"
   - Extract the ZIP file to your computer

2. **Locate the Firmware:**
   ```
   SurSajni-Controller/
   └── SurSajni_ESP32/
       ├── SurSajni_ESP32.ino
       └── WebServer.ino
   ```

3. **Flash Using Arduino IDE:**
   - Open `SurSajni_ESP32.ino` in Arduino IDE
   - Install required libraries (see QUICKSTART.md)
   - Select your ESP32 board: Tools → Board → ESP32 Dev Module
   - Select COM port: Tools → Port → (your ESP32)
   - Click Upload button (→)

### Option B: Using Git Clone

```bash
# Clone the repository
git clone https://github.com/spacelive1980/SurSajni-Controller.git
cd SurSajni-Controller/SurSajni_ESP32

# Open in Arduino IDE
arduino SurSajni_ESP32.ino
```

### Option C: Using PlatformIO

```bash
# Clone and open
git clone https://github.com/spacelive1980/SurSajni-Controller.git
cd SurSajni-Controller/SurSajni_ESP32

# Upload using PlatformIO
pio run -t upload
```

### Flashing Steps (Detailed)

**1. Install Arduino IDE** (if not already installed)
   - Download from: https://www.arduino.cc/en/software
   - Install for your operating system

**2. Add ESP32 Board Support**
   ```
   File → Preferences → Additional Board Manager URLs:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   
   Tools → Board → Boards Manager → Search "esp32" → Install
   ```

**3. Install Required Libraries**
   ```
   Sketch → Include Library → Manage Libraries
   
   Install:
   - RTClib (by Adafruit)
   - LoRa (by Sandeep Mistry)
   - Adafruit SSD1306
   - Adafruit GFX Library
   ```

**4. Connect ESP32**
   - Connect ESP32 to computer via USB cable
   - Wait for drivers to install (Windows may require CH340 drivers)

**5. Configure Arduino IDE**
   ```
   Tools → Board → ESP32 Arduino → ESP32 Dev Module
   Tools → Port → (select your ESP32 COM port)
   Tools → Upload Speed → 921600
   Tools → Flash Size → 4MB
   Tools → Partition Scheme → Default 4MB with spiffs
   ```

**6. Upload Firmware**
   - Click Upload button (→) or press Ctrl+U
   - If upload fails, hold the BOOT button on ESP32 while clicking upload
   - Wait for "Done uploading" message

**7. Verify Upload**
   - Open Serial Monitor: Tools → Serial Monitor (115200 baud)
   - Press EN button on ESP32
   - You should see:
   ```
   === Sursajni Controller ESP32 ===
   Firmware Version: 1.0.0-ESP32
   ...
   Setup complete!
   ```

---

## Part 2: Firmware Encryption (Anti-Cloning Protection)

ESP32 supports multiple security features to protect your firmware from being copied or reverse-engineered.

### Level 1: Flash Encryption (Recommended)

Flash encryption prevents reading the firmware from the ESP32's flash memory.

**Using Arduino IDE with esptool:**

1. **Backup Your Firmware First!** Once encrypted, you cannot read it back.

2. **Generate Encryption Key:**
   ```bash
   # Install esptool
   pip install esptool
   
   # Generate a random 256-bit key
   esptool.py generate_flash_encryption_key my_flash_encryption_key.bin
   ```
   
   **⚠️ CRITICAL:** Store this key in a safe place! Without it, you cannot update the device.

3. **Burn Encryption Key to ESP32:**
   ```bash
   # For ESP32 (not S2/S3/C3)
   esptool.py --port COM3 burn_key flash_encryption my_flash_encryption_key.bin
   
   # This is irreversible! The key is burned into eFuses.
   ```

4. **Upload Encrypted Firmware:**
   ```bash
   # First, compile your sketch to get the binary
   # In Arduino IDE: Sketch → Export Compiled Binary
   
   # Then encrypt and upload
   esptool.py --chip esp32 --port COM3 \
     --baud 921600 \
     --before default_reset --after hard_reset \
     write_flash --encrypt \
     0x10000 SurSajni_ESP32.ino.bin
   ```

5. **Enable Flash Encryption (One-Time):**
   ```bash
   # This burns the FLASH_CRYPT_CNT eFuse
   esptool.py --port COM3 burn_efuse FLASH_CRYPT_CNT
   ```

**Important Notes:**
- Once flash encryption is enabled, normal upload won't work
- You must always use `--encrypt` flag when uploading
- OTA updates require special handling
- Cannot read back firmware from device

### Level 2: Secure Boot

Secure Boot ensures only your signed firmware can run on the device.

**Setup (Advanced):**

1. **Generate Signing Key:**
   ```bash
   espsecure.py generate_signing_key secure_boot_signing_key.pem
   ```

2. **Enable Secure Boot:**
   ```bash
   espsecure.py sign_data --keyfile secure_boot_signing_key.pem \
     -o signed_bootloader.bin bootloader.bin
   
   esptool.py --port COM3 burn_efuse SECURE_BOOT_EN
   ```

**⚠️ WARNING:** Secure Boot is a one-way operation. Test thoroughly before enabling!

### Level 3: Combined Protection (Maximum Security)

For best protection, use both Flash Encryption + Secure Boot:

```bash
# 1. Generate keys
esptool.py generate_flash_encryption_key flash_key.bin
espsecure.py generate_signing_key secure_boot_key.pem

# 2. Burn keys
esptool.py --port COM3 burn_key flash_encryption flash_key.bin
esptool.py --port COM3 burn_key secure_boot secure_boot_key.pem

# 3. Sign and encrypt firmware
espsecure.py sign_data --keyfile secure_boot_key.pem \
  -o signed_firmware.bin firmware.bin

esptool.py --port COM3 write_flash --encrypt 0x10000 signed_firmware.bin

# 4. Enable protections
esptool.py --port COM3 burn_efuse FLASH_CRYPT_CNT
esptool.py --port COM3 burn_efuse SECURE_BOOT_EN
```

### Level 4: Additional Protection Measures

**A. Disable JTAG Debugging:**
```bash
esptool.py --port COM3 burn_efuse JTAG_DISABLE
```

**B. Disable ROM Download Mode:**
```bash
esptool.py --port COM3 burn_efuse DOWNLOAD_DIS
```

**C. Write-Protect Boot Sectors:**
```bash
esptool.py --port COM3 burn_efuse WR_DIS_FLASH_CRYPT_CNT
```

### Simplified Encryption Method (Using PlatformIO)

**platformio.ini:**
```ini
[env:esp32_encrypted]
platform = espressif32
board = esp32dev
framework = arduino

; Enable flash encryption
board_build.flash_mode = dio
board_build.partitions = default.csv
board_upload.flash_size = 4MB

; Encryption settings
build_flags = 
    -DCONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE

; Upload with encryption
upload_flags = 
    --encrypt
```

Then simply: `pio run -t upload`

---

## Part 3: Practical Security Recommendations

### For Commercial Products

1. **Use Flash Encryption** - Prevents reading firmware
2. **Use Secure Boot** - Prevents unauthorized firmware
3. **Disable JTAG** - Prevents hardware debugging
4. **Store keys securely** - Use hardware security modules (HSM)
5. **Version control** - Track which devices have which firmware

### For DIY/Hobbyist Use

1. **Flash Encryption is sufficient** for most needs
2. **Keep your encryption key backup safe**
3. **Document your encryption process**
4. **Test encryption on a spare ESP32 first**

### What Each Protection Prevents

| Protection | Prevents Firmware Dump | Prevents Clone | Prevents Tampering |
|------------|----------------------|----------------|-------------------|
| None | ❌ No | ❌ No | ❌ No |
| Flash Encryption | ✅ Yes | ⚠️ Partial | ❌ No |
| Secure Boot | ❌ No | ❌ No | ✅ Yes |
| Both | ✅ Yes | ✅ Yes | ✅ Yes |

### Important Warnings

⚠️ **Flash Encryption is PERMANENT**
- Cannot be disabled once enabled
- Must keep encryption key safe
- Test on development board first

⚠️ **Secure Boot is PERMANENT**
- Cannot be disabled once enabled
- Must keep signing key safe
- Makes future updates more complex

⚠️ **eFuse burns are ONE-TIME**
- Cannot be reversed
- Cannot be changed
- Plan carefully before burning

⚠️ **Back Up Everything**
- Encryption keys
- Signing keys
- Original firmware
- Configuration files

---

## Part 4: Alternative Protection Methods

If permanent encryption is too risky, consider these alternatives:

### Software-Only Protection

1. **Obfuscate Code:**
   - Remove comments
   - Rename variables
   - Use compiler optimization
   ```
   Tools → Core Debug Level → None
   Tools → Optimize → Smallest Size (-Os)
   ```

2. **Custom Bootloader:**
   - Add password check at startup
   - Implement software license key
   - Time-limited operation

3. **Remote Authentication:**
   - Require online activation
   - Check license server
   - Cloud-based features

### Hardware Protection

1. **Potting/Encapsulation:**
   - Encase PCB in epoxy resin
   - Makes physical access difficult
   - Protects from moisture too

2. **Tamper-Evident Seals:**
   - Labels that show if opened
   - Warranty void stickers
   - Security screws

3. **Custom PCB:**
   - Integrate ESP32 module
   - Remove programming headers
   - Hide pin connections

---

## Part 5: Workflow for Encrypted Production

### Initial Setup (One-Time)

```bash
# 1. Generate keys (SAVE THESE!)
esptool.py generate_flash_encryption_key production_key.bin

# 2. Test on development board first
esptool.py --port COM3 burn_key flash_encryption production_key.bin
# Verify it works before mass production

# 3. Document the process
# Create a secure backup of all keys
```

### For Each Device

```bash
# 1. Compile firmware
arduino-cli compile --fqbn esp32:esp32:esp32 SurSajni_ESP32

# 2. Burn encryption key
esptool.py --port COM3 burn_key flash_encryption production_key.bin

# 3. Upload encrypted firmware
esptool.py --port COM3 write_flash --encrypt 0x10000 firmware.bin

# 4. Test device
# 5. Seal device
```

### Firmware Updates (Encrypted Devices)

```bash
# Must use encryption key for updates
esptool.py --port COM3 write_flash --encrypt 0x10000 new_firmware.bin
```

Or implement OTA with encryption:
- Store encrypted firmware on server
- ESP32 downloads and writes to flash
- Encryption key stays on device

---

## Part 6: Troubleshooting

### Cannot Upload After Encryption

**Problem:** "A fatal error occurred: MD5 of file does not match"

**Solution:** Use `--encrypt` flag:
```bash
esptool.py --port COM3 write_flash --encrypt 0x10000 firmware.bin
```

### Lost Encryption Key

**Problem:** Cannot update device, key is lost

**Solution:** Unfortunately, there is NO solution. Device must be:
- Completely erased (loses all data)
- Re-flashed from scratch
- New encryption key generated

**Prevention:** Always keep multiple backups of keys in secure locations

### Want to Remove Encryption

**Problem:** Device is encrypted, want to disable

**Solution:** Cannot disable. Must:
1. Completely erase chip: `esptool.py erase_flash`
2. Upload new unencrypted firmware
3. Note: eFuse values cannot be changed

---

## Part 7: Quick Reference Commands

### Check ESP32 Security Status
```bash
espefuse.py --port COM3 summary
```

### Get Chip Info
```bash
esptool.py --port COM3 chip_id
esptool.py --port COM3 flash_id
```

### Read Flash (if not encrypted)
```bash
esptool.py --port COM3 read_flash 0 0x400000 flash_backup.bin
```

### Erase Flash Completely
```bash
esptool.py --port COM3 erase_flash
```

### Upload with Encryption
```bash
esptool.py --port COM3 write_flash --encrypt 0x10000 firmware.bin
```

---

## Summary

**For Simple Protection (Recommended for Most Users):**
1. Use Flash Encryption
2. Keep encryption key backed up
3. Test on spare ESP32 first

**For Maximum Protection (Commercial Products):**
1. Use Flash Encryption + Secure Boot
2. Disable JTAG and Download Mode
3. Store keys in HSM
4. Document everything

**Remember:**
- ✅ Always test on development board first
- ✅ Keep multiple backups of all keys
- ✅ Document your process
- ⚠️ Encryption is permanent
- ⚠️ eFuses cannot be reversed

---

## Additional Resources

- ESP32 Security Features: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/security.html
- Flash Encryption: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html
- Secure Boot: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html
- esptool Documentation: https://docs.espressif.com/projects/esptool/en/latest/

**Need Help?** Open an issue on the GitHub repository with specific questions.

---

*Document Version: 1.0*
*Compatible with: ESP32, ESP32-S2, ESP32-S3, ESP32-C3*
*Last Updated: 2025*
