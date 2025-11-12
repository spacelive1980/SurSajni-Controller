# Quick Start Guide - Sursajni ESP32 Controller

Get your Sursajni Pump Controller running in 10 minutes!

## What You Need

### Minimum Setup (Testing)
- ESP32 board
- USB cable
- Computer with Arduino IDE

### Full Setup (Production)
- ESP32 board
- DS3231 RTC module
- SSD1306 OLED display (128x64)
- LoRa SX1278 module
- 5V relay module
- Float switches (5x)
- Sensors (turbidity, flow)
- Buzzer
- Power supply (5V 2A)

## Step 1: Install Arduino IDE (5 minutes)

1. Download Arduino IDE from https://www.arduino.cc/en/software
2. Install for your operating system
3. Add ESP32 board support:
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Click OK
   - Go to Tools → Board → Boards Manager
   - Search "esp32"
   - Click "Install" on "ESP32 by Espressif Systems"

## Step 2: Install Required Libraries (3 minutes)

Go to Sketch → Include Library → Manage Libraries

Install these libraries:
1. **RTClib** (by Adafruit)
2. **LoRa** (by Sandeep Mistry)
3. **Adafruit SSD1306**
4. **Adafruit GFX Library**

Search each name, click Install, wait for completion.

## Step 3: Configure & Upload (2 minutes)

1. Connect ESP32 to computer via USB
2. Open `SurSajni_ESP32.ino` in Arduino IDE
3. Select board: Tools → Board → ESP32 Arduino → ESP32 Dev Module
4. Select port: Tools → Port → (your ESP32 COM port)
5. Click Upload button (→)
6. Wait for "Done uploading" message

**If upload fails:** Hold BOOT button on ESP32 while clicking upload

## Step 4: Connect to Device

### Option A: Access Point Mode (Default)
1. Open WiFi settings on your phone/computer
2. Connect to: **Sursajni-Controller**
3. Password: **sursajni123**
4. Open browser to: http://192.168.4.1
5. Login: admin / admin

### Option B: Connect to Your WiFi
1. First connect via AP mode (Option A)
2. In web interface, go to Settings
3. Enter your WiFi SSID and password
4. Save settings
5. Device will connect to your network
6. Find device IP in router or serial monitor
7. Access via: http://[device-ip]

## Step 5: Basic Configuration

### Change Password (IMPORTANT!)
1. Go to Settings → Security
2. Change password from default "admin"
3. Click Save

### Set Pump Mode
1. Go to Settings → Pump Settings
2. Select mode:
   - **Auto:** Tank level control (recommended for testing)
   - **Manual:** User control
   - **Schedule:** Time-based
   - **Water Sensing:** Flow sensor based

### Configure Tank Levels (Auto Mode)
1. Set "Start Level" (when pump starts)
   - Example: Empty (0%)
2. Set "End Level" (when pump stops)
   - Example: Full (100%)

### Test Pump
1. Go to Status page
2. Click "Start Pump" button
3. Verify relay clicks
4. Click "Stop Pump" button

## Hardware Connection (Quick Test Setup)

### Minimum Test Wiring
```
ESP32 GPIO26 → Relay IN
ESP32 GND    → Relay GND, Common GND
ESP32 5V     → Relay VCC (if 5V relay)
```

### Add OLED Display (Optional)
```
ESP32 GPIO21 → OLED SDA
ESP32 GPIO22 → OLED SCL
ESP32 3.3V   → OLED VCC
ESP32 GND    → OLED GND
```

### Add RTC (Optional)
```
ESP32 GPIO21 → RTC SDA (shared with OLED)
ESP32 GPIO22 → RTC SCL (shared with OLED)
ESP32 3.3V   → RTC VCC
ESP32 GND    → RTC GND
```

**See WIRING.md for complete wiring diagram**

## Troubleshooting

### Upload Failed
- Hold BOOT button during upload
- Check USB cable (must support data)
- Try lower upload speed: Tools → Upload Speed → 115200

### Can't Find WiFi Network
- Wait 30 seconds after upload
- Check serial monitor: Tools → Serial Monitor (115200 baud)
- Reset ESP32 (press EN button)

### Can't Login to Web Interface
- Verify password: admin / admin
- Clear browser cache
- Try different browser

### Pump Not Starting
- Check relay wiring
- Look for errors in Status page
- Reset dry run: Status → Reset Dry Run
- Check pump mode setting

### No Display on OLED
- Verify I2C wiring (SDA/SCL)
- Check power to OLED
- Not critical - system works without display

## Serial Monitor Debug

Open Tools → Serial Monitor (115200 baud) to see:
- Boot messages
- WiFi connection status
- IP addresses
- Sensor readings
- Error messages
- Pump events

Example output:
```
=== Sursajni Controller ESP32 ===
Firmware Version: 1.0.0-ESP32
Pins configured
RTC initialized
OLED initialized
AP Mode: SSID=Sursajni-Controller, IP=192.168.4.1
Web server started
Setup complete!
```

## Next Steps

### For Testing
1. ✓ Upload firmware
2. ✓ Connect to web interface
3. ✓ Test pump control
4. Configure schedules
5. Set up safety features
6. Calibrate sensors

### For Production
1. Wire all sensors (see WIRING.md)
2. Mount in weatherproof enclosure
3. Connect to pump
4. Test all safety features:
   - Dry run protection
   - Tank level sensors
   - Flow sensor
5. Configure schedules
6. Monitor for 24 hours
7. Deploy

## Safety Checklist

Before connecting real pump:
- [ ] All wiring is secure
- [ ] No exposed high-voltage connections
- [ ] Relay is properly rated for pump
- [ ] Emergency stop is accessible
- [ ] Dry run protection is configured
- [ ] Tank sensors are working
- [ ] Device is in waterproof enclosure
- [ ] Circuit breaker is installed
- [ ] All connections are insulated
- [ ] System has been tested without load

## Getting Help

### Documentation
- README.md - Full documentation
- WIRING.md - Complete wiring guide
- Code comments - In-line documentation

### Debug Mode
Enable verbose serial output for troubleshooting:
- Check serial monitor at 115200 baud
- Watch for error messages
- Note sensor values

### Common Issues & Solutions

**Issue:** ESP32 keeps resetting
- **Solution:** Power supply insufficient, use 5V 2A adapter

**Issue:** WiFi drops frequently
- **Solution:** Poor signal, move router closer or use external antenna

**Issue:** Sensors showing wrong values
- **Solution:** Check wiring, verify voltage levels (3.3V)

**Issue:** Pump starts but won't stop
- **Solution:** Check end level sensor, verify wiring

**Issue:** Dry run error keeps triggering
- **Solution:** Adjust dry run delay, check flow sensor

## Web Interface Overview

### Status Page
- Real-time pump status
- Tank level visual
- Sensor readings
- System information
- Control buttons

### Schedules Page
- Add/edit up to 10 schedules
- Multiple repeat options
- Easy time selection

### Settings Page
- System profile
- Pump configuration
- Safety settings
- Buzzer alerts
- Turbidity sensor
- Display options
- LoRa settings
- Time & date

## Performance Tips

1. **WiFi Range:** Use ESP32 with external antenna connector for better range
2. **Power Stability:** Use quality 5V 2A power supply
3. **Sensor Accuracy:** Calibrate turbidity sensor using web interface
4. **Response Time:** Decrease web refresh interval for faster updates
5. **Battery Life:** Enable LoRa power optimization

## Maintenance

### Weekly
- Check pump operation
- Verify tank sensors
- Review error logs in serial monitor

### Monthly
- Test all safety features
- Clean turbidity sensor
- Check relay contacts
- Verify all connections

### Annually
- Replace RTC battery
- Check capacitors
- Update firmware if available

## Advanced Features

### OTA Updates (Future)
Can be added to update firmware over WiFi without USB cable

### Remote Monitoring
Access from anywhere if port forwarding is configured on router

### Data Logging
Add SD card module for logging pump events and sensor data

### SMS Alerts
Add GSM module for SMS notifications

### Mobile App
Use the web interface as a PWA (Progressive Web App)

## Quick Reference

### Default Credentials
```
WiFi SSID: Sursajni-Controller
WiFi Pass: sursajni123
Web User:  admin
Web Pass:  admin
```

### Default IPs
```
AP Mode:      http://192.168.4.1
Station Mode: Check router or serial monitor
```

### Pin Quick Reference
```
Pump Relay: GPIO26
Buzzer:     GPIO25
I2C SDA:    GPIO21
I2C SCL:    GPIO22
Tank 0-4:   GPIO32,33,27,14,12
Turbidity:  GPIO34
Flow:       GPIO35
Battery:    GPIO36
```

## Success!

Your Sursajni Pump Controller is now running!

**Remember:**
- Change default password
- Test thoroughly before production use
- Follow safety guidelines
- Monitor system regularly

**Need more help?** Check the full README.md and WIRING.md documentation.

---

*Document Version: 1.0*
*Compatible with Firmware: v1.0.0-ESP32*
