# Wiring Diagram and Hardware Setup

## Component Connections for ESP32

### Power Supply
- **ESP32 VIN:** 5V from power adapter or USB
- **ESP32 GND:** Common ground for all components
- **ESP32 3.3V:** Powers 3.3V components (OLED, RTC, sensors)

⚠️ **Important:** ESP32 GPIO pins are 3.3V tolerant. Do NOT connect 5V signals directly!

---

## I2C Devices (Shared Bus)

### RTC Module (DS3231)
```
DS3231 VCC  →  ESP32 3.3V
DS3231 GND  →  ESP32 GND
DS3231 SDA  →  ESP32 GPIO21 (OLED_SDA_PIN)
DS3231 SCL  →  ESP32 GPIO22 (OLED_SCL_PIN)
```

### OLED Display (SSD1306 128x64)
```
OLED VCC  →  ESP32 3.3V
OLED GND  →  ESP32 GND
OLED SDA  →  ESP32 GPIO21 (OLED_SDA_PIN)
OLED SCL  →  ESP32 GPIO22 (OLED_SCL_PIN)
```

**Note:** Both RTC and OLED share the same I2C bus (SDA/SCL pins)

---

## SPI Device (LoRa Module)

### LoRa SX1278/RFM95 Module
```
LoRa VCC   →  ESP32 3.3V
LoRa GND   →  ESP32 GND
LoRa SCK   →  ESP32 GPIO18 (LORA_SCK_PIN)
LoRa MISO  →  ESP32 GPIO19 (LORA_MISO_PIN)
LoRa MOSI  →  ESP32 GPIO23 (LORA_MOSI_PIN)
LoRa NSS   →  ESP32 GPIO5  (LORA_SS_PIN)
LoRa RST   →  ESP32 GPIO4  (LORA_RST_PIN)
LoRa DIO0  →  ESP32 GPIO2  (LORA_DIO0_PIN)
LoRa ANT   →  Connect appropriate antenna (433MHz or 868MHz)
```

⚠️ **Important:** Always use an antenna with LoRa module to avoid damage!

---

## Digital Outputs

### Pump Relay Module
```
Relay VCC   →  ESP32 5V (if 5V relay) or 3.3V (if 3.3V relay)
Relay GND   →  ESP32 GND
Relay IN    →  ESP32 GPIO26 (PUMP_RELAY_PIN)
Relay COM   →  AC/DC Power Line
Relay NO/NC →  Connect to pump motor
```

**Relay Connection for AC Pump:**
```
AC Mains HOT  →  Relay COM
Relay NO      →  Pump Motor HOT
Pump Motor    →  AC Mains NEUTRAL
```

⚠️ **Safety:** Use appropriate relay rated for your pump voltage/current!

### Buzzer
```
Buzzer +    →  ESP32 GPIO25 (BUZZER_PIN)
Buzzer -    →  ESP32 GND
```

For louder buzzer, use transistor driver:
```
GPIO25 → 1kΩ → Transistor Base (NPN)
Transistor Emitter → GND
Transistor Collector → Buzzer -
Buzzer + → 3.3V or 5V
```

---

## Digital Inputs (Tank Level Sensors)

### Float Switch Connections (Active LOW)
```
Float Switch Common   →  ESP32 3.3V
Float Switch NO       →  ESP32 GPIO + 10kΩ pulldown to GND

Level 0 (Empty)  →  ESP32 GPIO32 (TANK_LEVEL_0_PIN)
Level 1 (25%)    →  ESP32 GPIO33 (TANK_LEVEL_1_PIN)
Level 2 (50%)    →  ESP32 GPIO27 (TANK_LEVEL_2_PIN)
Level 3 (75%)    →  ESP32 GPIO14 (TANK_LEVEL_3_PIN)
Level 4 (Full)   →  ESP32 GPIO12 (TANK_LEVEL_4_PIN)
```

**Float Switch Installation in Tank:**
```
    ┌─────────────────┐
    │                 │  ← Full (Level 4)
    │   ◯             │
    │                 │  ← 75% (Level 3)
    │   ◯             │
    │                 │  ← 50% (Level 2)
    │   ◯             │
    │                 │  ← 25% (Level 1)
    │   ◯             │
    │                 │  ← Empty (Level 0)
    │   ◯             │
    └─────────────────┘
```

---

## Analog Inputs (ADC1 Pins Only)

⚠️ **Important:** Use only ADC1 pins (GPIO32-39) when WiFi is active!

### Turbidity Sensor
```
Turbidity VCC   →  ESP32 3.3V or 5V (check sensor spec)
Turbidity GND   →  ESP32 GND
Turbidity OUT   →  ESP32 GPIO34 (TURBIDITY_PIN)
```

If sensor outputs 5V, use voltage divider:
```
Sensor OUT → 10kΩ → GPIO34
          ↓
        6.8kΩ → GND
(Creates 3.3V max from 5V input)
```

### Flow Sensor (Analog Output)
```
Flow Sensor VCC  →  ESP32 3.3V or 5V
Flow Sensor GND  →  ESP32 GND
Flow Sensor OUT  →  ESP32 GPIO35 (FLOW_SENSOR_PIN)
```

### Battery Voltage Monitor
```
Battery + → 100kΩ → GPIO36 (BATTERY_PIN)
                 ↓
              22kΩ → GND
```

This voltage divider scales 0-16.5V to 0-3.3V
- Adjust resistor values based on your battery voltage
- For 12V battery: 100kΩ + 33kΩ divider
- For LiPo (3.7-4.2V): Direct connection or 10kΩ + 10kΩ

---

## Complete Wiring Summary

### ESP32 Pin Usage Table

| Pin | Function | Component | Type |
|-----|----------|-----------|------|
| GPIO2 | LoRa DIO0 | LoRa Module | Input |
| GPIO4 | LoRa RST | LoRa Module | Output |
| GPIO5 | LoRa SS | LoRa Module | Output |
| GPIO12 | Tank Level 4 | Float Switch | Input |
| GPIO14 | Tank Level 3 | Float Switch | Input |
| GPIO18 | LoRa SCK | LoRa Module | SPI |
| GPIO19 | LoRa MISO | LoRa Module | SPI |
| GPIO21 | I2C SDA | RTC + OLED | I2C |
| GPIO22 | I2C SCL | RTC + OLED | I2C |
| GPIO23 | LoRa MOSI | LoRa Module | SPI |
| GPIO25 | Buzzer | Buzzer | Output |
| GPIO26 | Pump Relay | Relay Module | Output |
| GPIO27 | Tank Level 2 | Float Switch | Input |
| GPIO32 | Tank Level 0 | Float Switch | Input |
| GPIO33 | Tank Level 1 | Float Switch | Input |
| GPIO34 | Turbidity | Sensor | ADC |
| GPIO35 | Flow Sensor | Sensor | ADC |
| GPIO36 (VP) | Battery | Voltage Divider | ADC |

---

## Power Requirements

### Component Power Consumption
- **ESP32:** 80mA (WiFi active), 20mA (WiFi sleep)
- **LoRa Module:** 120mA (transmitting), 12mA (receiving), 1µA (sleep)
- **OLED Display:** 20mA (typical)
- **RTC Module:** 200µA (typical)
- **Relay Module:** 70mA (coil energized)
- **Buzzer:** 30mA (typical)

**Total Peak Current:** ~350mA (all active)
**Recommended Power Supply:** 5V 2A adapter

### Battery Backup Option
For battery operation:
- Use 12V lead-acid or 3S LiPo battery
- Add buck converter (12V → 5V)
- ESP32 has built-in 3.3V regulator

---

## PCB Layout Recommendations

1. **Keep I2C lines short** - reduces interference
2. **Separate power traces** - use thicker traces for relay
3. **Add decoupling capacitors:**
   - 10µF on ESP32 VIN
   - 100nF near each component VCC
4. **Ground plane** - use solid ground plane on bottom layer
5. **Antenna clearance** - keep 5mm clearance around LoRa antenna

---

## Testing Procedure

### 1. Power Test
- [ ] Connect power supply
- [ ] Verify 5V on VIN
- [ ] Verify 3.3V on 3.3V pin
- [ ] Check for overheating

### 2. I2C Devices
- [ ] Upload test sketch
- [ ] Scan I2C bus (should find 0x3C for OLED, 0x68 for RTC)
- [ ] Display test pattern on OLED
- [ ] Read time from RTC

### 3. LoRa Module
- [ ] Check LoRa initialization in serial monitor
- [ ] Send test packet
- [ ] Verify antenna connection (SWR meter optional)

### 4. Sensors
- [ ] Read analog values from sensors
- [ ] Test each float switch manually
- [ ] Verify voltage dividers

### 5. Relay
- [ ] Test relay activation (without pump connected)
- [ ] Verify relay clicks on/off
- [ ] Check NO/NC contacts with multimeter

### 6. Integration
- [ ] Connect all components
- [ ] Upload full firmware
- [ ] Access web interface
- [ ] Test all functions

---

## Troubleshooting Hardware

### ESP32 Won't Boot
- Check power supply voltage
- Verify GPIO0 is not pulled LOW
- Check for short circuits

### I2C Devices Not Found
- Verify SDA/SCL connections
- Check pull-up resistors (internal are usually sufficient)
- Try I2C scanner sketch

### LoRa Not Working
- Verify SPI connections
- Check antenna connection
- Ensure correct frequency in code

### Sensors Reading Wrong Values
- Check voltage dividers
- Verify sensor power supply
- Calibrate using web interface

---

## Safety Warnings

⚠️ **ELECTRICAL SAFETY:**
- Never work on AC connections while powered
- Use appropriate wire gauge for pump current
- Install circuit breaker or fuse
- Enclose all high-voltage connections
- Follow local electrical codes

⚠️ **WATER SAFETY:**
- Waterproof all electrical enclosures
- Use IP65 or higher rated boxes
- Keep ESP32 away from water
- Use submersible sensors if needed

⚠️ **FIRE SAFETY:**
- Don't exceed component ratings
- Use thermal protection
- Avoid blocking ventilation
- Have fire extinguisher nearby during testing

---

## Enclosure Recommendations

### Main Controller Box
- IP65 rated enclosure (dust/water resistant)
- Minimum size: 200mm x 150mm x 75mm
- Cable glands for all entry points
- Mount relay and power supply separately
- Add ventilation for heat dissipation

### Sensor Connections
- Use waterproof connectors
- Apply heat shrink tubing
- Use shielded cables for analog sensors
- Separate power and signal cables

---

## Bill of Materials (BOM)

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 DevKit | 1 | 38-pin version recommended |
| DS3231 RTC Module | 1 | With battery backup |
| OLED 128x64 I2C | 1 | SSD1306 driver |
| LoRa SX1278 Module | 1 | 433MHz or 868MHz |
| 5V Relay Module | 1 | 10A rating minimum |
| Active Buzzer | 1 | 3.3V or 5V |
| Float Switches | 5 | For tank levels |
| Turbidity Sensor | 1 | 3.3V compatible |
| Flow Sensor | 1 | Analog output |
| Resistors | Various | For voltage dividers |
| Capacitors | Various | Decoupling |
| Power Supply | 1 | 5V 2A adapter |
| Enclosure | 1 | IP65 rated |
| Wires & Terminals | - | As needed |

---

**Document Version:** 1.0
**Last Updated:** 2025
