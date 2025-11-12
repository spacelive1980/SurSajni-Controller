# Sursajni Wireless Pump Controller

A comprehensive pump automation system with web-based control, multiple operating modes, and advanced safety features.

## 🌟 Features

- **Multiple Control Modes:** Schedule, Auto, Manual, and Water Sensing
- **Advanced Safety:** Dry run protection, retry logic, sensor error handling
- **Web Interface:** Responsive PWA for configuration and monitoring
- **Wireless Control:** WiFi-based control and monitoring
- **Sensor Integration:** Tank level, turbidity, flow, and battery monitoring
- **Remote Communication:** LoRa support for wireless sensors
- **Real-time Clock:** Accurate scheduling with DS3231 RTC
- **OLED Display:** Local status display
- **Customizable Alerts:** Multiple buzzer tones for different events

## 📁 Repository Structure

```
SurSajni-Controller/
├── index.html                  # Web interface (PWA)
├── SurSajni_ESP32/            # Arduino-ESP32 firmware
│   ├── SurSajni_ESP32.ino     # Main firmware file
│   ├── WebServer.ino          # Web server handlers
│   ├── README.md              # Detailed documentation
│   ├── QUICKSTART.md          # Quick start guide
│   ├── WIRING.md              # Hardware wiring guide
│   ├── ARDUINO_TO_ESP32.md    # Arduino conversion guide
│   └── platformio.ini         # PlatformIO configuration
└── SurSajni_ESP_IDF/          # ESP-IDF native firmware (NEW!)
    ├── CMakeLists.txt         # Build configuration
    ├── sdkconfig.defaults     # Default settings
    ├── partitions.csv         # Partition table
    ├── README.md              # ESP-IDF guide
    └── main/                  # Source files
        ├── main.c             # Entry point
        ├── pump_control.c/h   # Pump control
        ├── sensor_manager.c/h # Sensors
        ├── wifi_manager.c/h   # WiFi
        └── web_server.c/h     # Web API
```

## 🚀 Quick Start

### For ESP32 with Arduino Framework

1. **Hardware:** Get an ESP32 development board
2. **Software:** Install Arduino IDE and required libraries
3. **Upload:** Flash the firmware from `SurSajni_ESP32/`
4. **Connect:** Join WiFi network "Sursajni-Controller" (password: sursajni123)
5. **Configure:** Open http://192.168.4.1 in browser (login: admin/admin)

See [SurSajni_ESP32/QUICKSTART.md](SurSajni_ESP32/QUICKSTART.md) for detailed instructions.

### For ESP32 with ESP-IDF (Recommended for Production/Security)

**Use ESP-IDF version when you need:**
- ✅ **Flash encryption** (prevent firmware cloning)
- ✅ **Secure boot** (prevent unauthorized firmware)
- ✅ **Production security** (NVS encryption, bootloader protection)

1. **Install ESP-IDF:** Follow instructions at https://docs.espressif.com/projects/esp-idf/
2. **Build:** `cd SurSajni_ESP_IDF && idf.py build`
3. **Flash:** `idf.py -p /dev/ttyUSB0 flash monitor`
4. **Connect:** Same as Arduino version (192.168.4.1)

See [SurSajni_ESP_IDF/README.md](SurSajni_ESP_IDF/README.md) for complete ESP-IDF guide including flash encryption setup.

### For Arduino (If you have Arduino .ino file)

If you have existing Arduino firmware:
1. Follow the conversion guide in [SurSajni_ESP32/ARDUINO_TO_ESP32.md](SurSajni_ESP32/ARDUINO_TO_ESP32.md)
2. The ESP32 version is fully compatible and provides additional features

## 📱 Web Interface

The included `index.html` provides a feature-rich Progressive Web App (PWA) with:

- **Status Page:** Real-time monitoring of pump, tank, and sensors
- **Schedules Page:** Manage up to 10 schedules with various repeat options
- **Settings Page:** Configure all system parameters
- **Responsive Design:** Works on desktop, tablet, and mobile
- **Multiple Themes:** Dark, Light, Oceanic, Sunset, Forest
- **Offline Support:** Can be installed as PWA

### Features
- Tank level visualization
- Live sensor readings
- Schedule management (daily, odd/even days, specific dates)
- Turbidity sensor calibration
- System controls (start/stop pump, reset errors)
- Comprehensive settings (pump, safety, buzzer, display, LoRa)

## 🔧 Hardware Requirements

### Essential Components
- ESP32 Development Board
- DS3231 RTC Module
- 5V Relay Module (10A minimum)
- Float Switches (5x for tank levels)
- Power Supply (5V 2A)

### Optional Components
- SSD1306 OLED Display (128x64)
- LoRa SX1278 Module (433/868MHz)
- Turbidity Sensor
- Flow Sensor
- Buzzer

See [SurSajni_ESP32/WIRING.md](SurSajni_ESP32/WIRING.md) for complete wiring diagrams.

## 📖 Documentation

### Arduino-ESP32 Version
Comprehensive documentation is available in the `SurSajni_ESP32/` folder:

- **[README.md](SurSajni_ESP32/README.md)** - Complete firmware documentation
- **[QUICKSTART.md](SurSajni_ESP32/QUICKSTART.md)** - Get started in 10 minutes
- **[WIRING.md](SurSajni_ESP32/WIRING.md)** - Detailed hardware connections
- **[ARDUINO_TO_ESP32.md](SurSajni_ESP32/ARDUINO_TO_ESP32.md)** - Arduino conversion guide
- **[FLASHING_AND_SECURITY.md](SurSajni_ESP32/FLASHING_AND_SECURITY.md)** - Flash firmware & encryption guide

### ESP-IDF Version (Production/Security)
Native ESP-IDF documentation in the `SurSajni_ESP_IDF/` folder:

- **[README.md](SurSajni_ESP_IDF/README.md)** - Complete ESP-IDF guide with flash encryption & secure boot
  - Flash encryption setup (development & production modes)
  - Secure boot v2 implementation
  - NVS encryption
  - OTA encrypted updates
  - Production deployment workflow

## 🎯 Use Cases

### Residential
- Automatic sump pump control
- Overhead tank filling
- Garden irrigation
- Pool pump automation

### Agricultural
- Field irrigation systems
- Livestock water management
- Greenhouse watering
- Drip irrigation control

### Industrial
- Process water management
- Cooling system control
- Waste water handling
- Chemical dosing

### Commercial
- Building water supply
- Car wash systems
- Laundry services
- Restaurant water systems

## 🛡️ Safety Features

- **Dry Run Protection:** Prevents pump damage when no water is available
- **Flow Monitoring:** Verifies pump is actually moving water
- **Retry Logic:** Automatic retry with configurable attempts
- **Sensor Error Handling:** Bypass or stop operation on sensor failure
- **Maximum Run Time:** Prevents pump from running indefinitely
- **Tank Overflow Prevention:** Stops pump when tank is full
- **Battery Monitoring:** Low battery alerts
- **Turbidity Detection:** Stops pump if water is too dirty

## 🔌 API Endpoints

The firmware exposes RESTful API for programmatic control:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/get_settings` | GET | Get all settings and current status |
| `/update_all_settings` | POST | Update system settings |
| `/toggle_pump` | POST | Start/stop pump |
| `/reset_dry_run` | POST | Reset dry run error |
| `/reset_sensor_error` | POST | Reset sensor error |
| `/get_live_data` | GET | Get real-time sensor data |
| `/factory_reset` | POST | Factory reset device |

All endpoints require HTTP Basic Authentication.

## 🛠️ Development

### Arduino IDE
1. Install ESP32 board support
2. Install required libraries (RTClib, LoRa, Adafruit_SSD1306, etc.)
3. Open `SurSajni_ESP32.ino`
4. Select board and port
5. Upload

### PlatformIO
1. Install PlatformIO
2. Open project folder
3. Run `pio run -t upload`

The `platformio.ini` file includes configurations for ESP32, ESP32-S2, ESP32-S3, and ESP32-C3.

## 📊 System Specifications

### Performance
- CPU: ESP32 240MHz dual-core
- Memory: 520KB SRAM, 4MB Flash
- WiFi: 802.11 b/g/n (2.4GHz)
- Range: 100m+ (open space)
- Update Rate: Configurable (5-60 seconds)

### Electrical
- Operating Voltage: 5V (USB/adapter)
- Logic Level: 3.3V
- Max Current: ~350mA (all peripherals active)
- Relay Rating: 10A @ 250VAC / 30VDC

### Environmental
- Operating Temperature: -10°C to +50°C
- Storage Temperature: -40°C to +85°C
- Humidity: 0-95% RH (non-condensing)
- Enclosure: IP65 recommended

## 🔐 Security

- HTTP Basic Authentication (default: admin/admin)
- **Change default password immediately!**
- Optional: Enable HTTPS (future feature)
- Isolated AP mode for setup
- WPA2 encryption for station mode

## 🤝 Contributing

Contributions are welcome! Areas for improvement:

- [ ] HTTPS support
- [ ] MQTT integration
- [ ] Home Assistant integration
- [ ] Mobile app (native)
- [ ] OTA firmware updates
- [ ] Data logging to SD card
- [ ] SMS notifications (GSM module)
- [ ] Additional sensor support

## 📄 License

This project is provided as-is for the Sursajni Pump Controller.

## 🐛 Troubleshooting

### Common Issues

**Pump won't start:**
- Check relay connections
- Verify pump mode setting
- Look for dry run or sensor errors
- Check power supply

**Can't connect to WiFi:**
- Verify SSID "Sursajni-Controller"
- Password: sursajni123
- Wait 30 seconds after power-on
- Check serial monitor for IP address

**Sensors not reading:**
- Verify 3.3V compatibility
- Check wiring
- Use voltage dividers for 5V sensors
- Ensure ADC1 pins (GPIO32-39)

**Web interface not loading:**
- Clear browser cache
- Try different browser
- Check authentication (admin/admin)
- Verify device IP address

See documentation for more troubleshooting tips.

## 📞 Support

- 📖 Read the documentation in `SurSajni_ESP32/`
- 🐛 Open an issue for bugs
- 💡 Open an issue for feature requests
- 📧 Contact the development team

## 🏆 Credits

Developed by **Sursajni Automations** © 2025

Built with:
- ESP32 by Espressif
- Arduino framework
- Adafruit libraries
- LoRa library by Sandeep Mistry

## ⚠️ Important Notes

1. **Voltage:** ESP32 is 3.3V - never connect 5V signals directly!
2. **Testing:** Always test thoroughly before connecting to real pumps
3. **Safety:** Follow electrical codes and safety guidelines
4. **Water:** Use waterproof enclosures near water
5. **Power:** Use quality 5V 2A power supply
6. **Updates:** Check for firmware updates regularly

## 🎓 Learning Resources

New to ESP32? Check these resources:
- [ESP32 Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Arduino-ESP32 Guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [Random Nerd Tutorials](https://randomnerdtutorials.com/projects-esp32/)

## 📈 Roadmap

### v1.0 (Current)
- ✅ ESP32 firmware with all features
- ✅ Web interface (PWA)
- ✅ Complete documentation
- ✅ Multiple control modes
- ✅ Safety features

### v1.1 (Planned)
- [ ] OTA firmware updates
- [ ] MQTT support
- [ ] Enhanced data logging
- [ ] Mobile app improvements

### v2.0 (Future)
- [ ] Home Assistant integration
- [ ] Voice control (Alexa/Google)
- [ ] Cloud dashboard
- [ ] Advanced analytics
- [ ] Multi-pump support

## 🌟 Why Sursajni Controller?

✅ **Complete Solution** - Hardware + Software + Documentation
✅ **Easy to Use** - Web interface, no app required
✅ **Flexible** - Multiple modes and configurations
✅ **Safe** - Comprehensive safety features
✅ **Affordable** - ESP32 is cost-effective ($5-15)
✅ **Expandable** - Add sensors and features easily
✅ **Open** - Full source code and documentation
✅ **Reliable** - Tested and proven design

---

**Get Started:** See [QUICKSTART.md](SurSajni_ESP32/QUICKSTART.md)

**Questions?** Open an issue!

**Happy Automating! 💧🔌**
