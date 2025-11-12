/*
 * Sursajni Wireless Pump Controller - ESP32 Version
 * 
 * This firmware is designed for ESP32 boards and provides:
 * - WiFi connectivity and web server
 * - Multiple pump control modes (Schedule, Auto, Manual, Water Sensing)
 * - Tank level monitoring
 * - Turbidity sensor integration
 * - Dry run protection
 * - LoRa communication for remote sensors
 * - Real-time clock (RTC) support
 * - Buzzer alerts
 * - Web-based configuration interface
 * 
 * Compatible with: ESP32, ESP32-S2, ESP32-S3, ESP32-C3
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <RTClib.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>

// ===== ADC CONSTANTS =====
#define ADC_MAX_VALUE       4095.0    // 12-bit ADC
#define ADC_REFERENCE_V     3.3       // ESP32 ADC reference voltage
#define BATTERY_DIVIDER_R   5.0       // Voltage divider ratio (adjust based on your circuit)

// ===== PIN DEFINITIONS (Adjust based on your hardware) =====
#define PUMP_RELAY_PIN      26    // Pump control relay
#define BUZZER_PIN          25    // Buzzer for alerts
#define TURBIDITY_PIN       34    // Analog input for turbidity sensor (ADC1)
#define FLOW_SENSOR_PIN     35    // Analog input for flow sensor (ADC1)
#define BATTERY_PIN         36    // Analog input for battery voltage (VP/ADC1_0)

// Tank level sensor pins (adjust as needed)
#define TANK_LEVEL_0_PIN    32    // Empty sensor
#define TANK_LEVEL_1_PIN    33    // 25% sensor
#define TANK_LEVEL_2_PIN    27    // 50% sensor
#define TANK_LEVEL_3_PIN    14    // 75% sensor
#define TANK_LEVEL_4_PIN    12    // Full sensor

// LoRa pins for ESP32 (using VSPI)
#define LORA_SCK_PIN        18
#define LORA_MISO_PIN       19
#define LORA_MOSI_PIN       23
#define LORA_SS_PIN         5
#define LORA_RST_PIN        4
#define LORA_DIO0_PIN       2

// OLED Display pins (I2C)
#define OLED_SDA_PIN        21
#define OLED_SCL_PIN        22
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_ADDR           0x3C

// ===== ENUMS =====
enum PumpMode {
  PUMP_MODE_SCHEDULE = 0,
  PUMP_MODE_AUTO = 1,
  PUMP_MODE_MANUAL = 2,
  PUMP_MODE_WATER_SENSING = 3
};

enum RepeatType {
  REPEAT_EVERY_DAY = 0,
  REPEAT_ODD_DAY = 1,
  REPEAT_EVEN_DAY = 2,
  REPEAT_NO_REPEAT = 3,
  REPEAT_SPECIFIC_DATE = 4
};

enum BeepStyle {
  BEEP_SILENT = 0,
  BEEP_ALERT = 1,
  BEEP_WARNING = 2,
  BEEP_PULSE = 3,
  BEEP_LONG = 4,
  BEEP_SPARROW = 5
};

enum SensorError {
  SENSOR_ERROR_NONE = 0,
  SENSOR_ERROR_TANK_LEVEL = 1,
  SENSOR_ERROR_FLOW = 2,
  SENSOR_ERROR_TURBIDITY = 3,
  SENSOR_ERROR_LORA = 4
};

enum SystemProfile {
  PROFILE_LEVEL_INDICATOR = 0,
  PROFILE_MONOBLOCK = 1,
  PROFILE_SUBMERSIBLE = 2,
  PROFILE_FULLY_AUTOMATIC = 3
};

// ===== STRUCTURES =====
struct Schedule {
  uint8_t hour;
  uint8_t minute;
  uint8_t repeatType;
  uint16_t year;
  uint8_t month;
  uint8_t day;
};

struct Settings {
  // System Profile
  uint8_t systemProfile;
  
  // Pump Settings
  uint8_t pumpMode;
  uint8_t startSensorLevel;
  uint8_t endSensorLevel;
  uint16_t scheduleDurationMin;
  uint16_t primingTimeMs;
  bool forceScheduledRun;
  bool manualModeSafetyLogicEnabled;
  
  // Safety Settings
  bool dryRunLogicEnabled;
  uint16_t dryRunDelayMin;
  bool retryLogicEnabled;
  uint8_t dryRunAttempts;
  uint16_t retryIntervalMin;
  bool sensorErrorBypassEnabled;
  bool autoResetDryRunOnWater;
  uint16_t waterPresenceThreshold;
  uint16_t waterSensingStartDelaySec;
  uint16_t waterSensingStopDelaySec;
  
  // Buzzer Settings
  bool buzzerEnabled;
  uint8_t buzzerVolume;
  uint8_t dryRunBeepStyle;
  uint8_t sensorErrorBeepStyle;
  uint8_t lowBatteryBeepStyle;
  uint8_t heartbeatExpiredBeepStyle;
  uint8_t tankEmptyBeepStyle;
  uint8_t tankFullBeepStyle;
  uint8_t dirtyWaterBeepStyle;
  
  // Turbidity Settings
  bool turbidityBypassEnabled;
  uint16_t turbidityTimeoutSec;
  uint16_t turbidityLimit;
  uint16_t calibratedCleanValue;
  uint16_t calibratedDirtyValue;
  
  // Display Settings
  uint8_t oledTheme;
  uint8_t webTheme;
  uint16_t webUiRefreshIntervalSec;
  uint8_t currentFontIndex;
  bool oledShowTime;
  bool oledShowDate;
  bool oledShowDay;
  bool oledShowBattery;
  
  // LoRa Settings
  bool loraPowerOptimizationEnabled;
  uint8_t transmitterTxPower;
  uint8_t transmitterAckAttempts;
  
  // Security
  char httpUsername[32];
  char httpPassword[32];
  
  // WiFi
  char wifiSSID[32];
  char wifiPassword[64];
  
  // Schedules
  Schedule schedules[10];
};

// ===== GLOBAL VARIABLES =====
WebServer server(80);
Preferences preferences;
RTC_DS3231 rtc;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

Settings settings;
bool pumpRunning = false;
bool dryRunError = false;
bool heartbeatExpired = false;
uint8_t currentTankLevel = 0;
uint16_t turbidityValue = 0;
float batteryVoltage = 0.0;
bool flowOk = false;
SensorError sensorError = SENSOR_ERROR_NONE;
uint8_t remainingAttempts = 0;
int8_t signalStrength = -100;
String nextSchedule = "None";
String deviceId = "";
String macAddress = "";
unsigned long pumpStartTime = 0;
unsigned long pumpPrimingStartTime = 0;
bool pumpPriming = false;
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastLoRaCheck = 0;

// ===== FUNCTION PROTOTYPES =====
void setupWiFi();
void setupWebServer();
void setupPins();
void setupRTC();
void setupLoRa();
void setupDisplay();
void loadSettings();
void saveSettings();
void factoryReset();
void readSensors();
uint8_t readTankLevel();
void controlPump();
void checkSchedules();
void handleDryRun();
void handleRetryLogic();
void updateDisplay();
void playBeep(uint8_t beepStyle);
void sendLoRaMessage();
void handleLoRaMessage();

// Web server handlers
void handleRoot();
void handleGetSettings();
void handleUpdateSettings();
void handleTogglePump();
void handleResetDryRun();
void handleResetSensorError();
void handleFactoryReset();
void handleGetLiveData();
void handleNotFound();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Sursajni Controller ESP32 ===");
  Serial.println("Firmware Version: 1.0.0-ESP32");
  
  // Initialize SPIFFS for web files
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed - will serve basic HTML");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }
  
  // Initialize preferences
  preferences.begin("sursajni", false);
  
  // Setup hardware
  setupPins();
  setupRTC();
  setupDisplay();
  
  // Load settings from NVS
  loadSettings();
  
  // Setup WiFi and web server
  setupWiFi();
  setupWebServer();
  
  // Setup LoRa
  setupLoRa();
  
  // Get device info
  macAddress = WiFi.macAddress();
  deviceId = "ESP32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  
  remainingAttempts = settings.dryRunAttempts;
  
  Serial.println("Setup complete!");
  Serial.printf("MAC Address: %s\n", macAddress.c_str());
  Serial.printf("Device ID: %s\n", deviceId.c_str());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
}

// ===== MAIN LOOP =====
void loop() {
  server.handleClient();
  
  // Read sensors periodically
  if (millis() - lastSensorRead > 1000) {
    readSensors();
    lastSensorRead = millis();
  }
  
  // Control pump based on mode
  controlPump();
  
  // Check schedules
  if (settings.pumpMode == PUMP_MODE_SCHEDULE) {
    checkSchedules();
  }
  
  // Handle dry run protection
  if (settings.dryRunLogicEnabled) {
    handleDryRun();
  }
  
  // Handle retry logic
  if (settings.retryLogicEnabled) {
    handleRetryLogic();
  }
  
  // Update display
  if (millis() - lastDisplayUpdate > 500) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Check LoRa messages
  if (millis() - lastLoRaCheck > 100) {
    handleLoRaMessage();
    lastLoRaCheck = millis();
  }
  
  // Update WiFi signal strength
  if (WiFi.status() == WL_CONNECTED) {
    signalStrength = WiFi.RSSI();
  }
}

// ===== PIN SETUP =====
void setupPins() {
  // Output pins
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Input pins for tank level sensors
  pinMode(TANK_LEVEL_0_PIN, INPUT_PULLUP);
  pinMode(TANK_LEVEL_1_PIN, INPUT_PULLUP);
  pinMode(TANK_LEVEL_2_PIN, INPUT_PULLUP);
  pinMode(TANK_LEVEL_3_PIN, INPUT_PULLUP);
  pinMode(TANK_LEVEL_4_PIN, INPUT_PULLUP);
  
  Serial.println("Pins configured");
}

// ===== WIFI SETUP =====
void setupWiFi() {
  WiFi.mode(WIFI_MODE_APSTA);  // Both AP and Station mode
  
  // Start AP mode
  WiFi.softAP("Sursajni-Controller", "sursajni123");
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("AP Mode: SSID=Sursajni-Controller, IP=%s\n", apIP.toString().c_str());
  
  // Connect to WiFi if configured
  if (strlen(settings.wifiSSID) > 0) {
    Serial.printf("Connecting to WiFi: %s\n", settings.wifiSSID);
    WiFi.begin(settings.wifiSSID, settings.wifiPassword);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("\nFailed to connect to WiFi");
    }
  }
}

// ===== RTC SETUP =====
void setupRTC() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    sensorError = SENSOR_ERROR_LORA;  // Reuse for general I2C error
  } else {
    Serial.println("RTC initialized");
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting default time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
}

// ===== DISPLAY SETUP =====
void setupDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found!");
  } else {
    Serial.println("OLED initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Sursajni");
    display.println("Controller");
    display.println("ESP32");
    display.println("Initializing...");
    display.display();
  }
}

// ===== LORA SETUP =====
void setupLoRa() {
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_SS_PIN);
  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  
  if (!LoRa.begin(433E6)) {  // 433 MHz frequency
    Serial.println("LoRa initialization failed!");
    sensorError = SENSOR_ERROR_LORA;
  } else {
    Serial.println("LoRa initialized");
    LoRa.setTxPower(settings.transmitterTxPower);
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
  }
}

// ===== SETTINGS MANAGEMENT =====
void loadSettings() {
  // Load settings from NVS with defaults
  settings.systemProfile = preferences.getUChar("sysProfile", PROFILE_FULLY_AUTOMATIC);
  
  // Pump settings
  settings.pumpMode = preferences.getUChar("pumpMode", PUMP_MODE_AUTO);
  settings.startSensorLevel = preferences.getUChar("startLevel", 0);
  settings.endSensorLevel = preferences.getUChar("endLevel", 4);
  settings.scheduleDurationMin = preferences.getUShort("schedDuration", 0);
  settings.primingTimeMs = preferences.getUShort("primingTime", 2000);
  settings.forceScheduledRun = preferences.getBool("forceSchRun", false);
  settings.manualModeSafetyLogicEnabled = preferences.getBool("manualSafety", true);
  
  // Safety settings
  settings.dryRunLogicEnabled = preferences.getBool("dryRunEn", true);
  settings.dryRunDelayMin = preferences.getUShort("dryRunDelay", 5);
  settings.retryLogicEnabled = preferences.getBool("retryEn", true);
  settings.dryRunAttempts = preferences.getUChar("dryRunAttempt", 3);
  settings.retryIntervalMin = preferences.getUShort("retryInterval", 10);
  settings.sensorErrorBypassEnabled = preferences.getBool("sensorBypass", false);
  settings.autoResetDryRunOnWater = preferences.getBool("autoResetDR", true);
  settings.waterPresenceThreshold = preferences.getUShort("waterThresh", 500);
  settings.waterSensingStartDelaySec = preferences.getUShort("waterStartDly", 3);
  settings.waterSensingStopDelaySec = preferences.getUShort("waterStopDly", 5);
  
  // Buzzer settings
  settings.buzzerEnabled = preferences.getBool("buzzerEn", true);
  settings.buzzerVolume = preferences.getUChar("buzzerVol", 128);
  settings.dryRunBeepStyle = preferences.getUChar("dryRunBeep", BEEP_ALERT);
  settings.sensorErrorBeepStyle = preferences.getUChar("sensorBeep", BEEP_WARNING);
  settings.lowBatteryBeepStyle = preferences.getUChar("lowBattBeep", BEEP_PULSE);
  settings.heartbeatExpiredBeepStyle = preferences.getUChar("hbBeep", BEEP_WARNING);
  settings.tankEmptyBeepStyle = preferences.getUChar("emptyBeep", BEEP_ALERT);
  settings.tankFullBeepStyle = preferences.getUChar("fullBeep", BEEP_SPARROW);
  settings.dirtyWaterBeepStyle = preferences.getUChar("dirtyBeep", BEEP_WARNING);
  
  // Turbidity settings
  settings.turbidityBypassEnabled = preferences.getBool("turbBypass", false);
  settings.turbidityTimeoutSec = preferences.getUShort("turbTimeout", 30);
  settings.turbidityLimit = preferences.getUShort("turbLimit", 1500);
  settings.calibratedCleanValue = preferences.getUShort("turbClean", 0);
  settings.calibratedDirtyValue = preferences.getUShort("turbDirty", 4095);
  
  // Display settings
  settings.oledTheme = preferences.getUChar("oledTheme", 0);
  settings.webTheme = preferences.getUChar("webTheme", 0);
  settings.webUiRefreshIntervalSec = preferences.getUShort("webRefresh", 10);
  settings.currentFontIndex = preferences.getUChar("fontIndex", 0);
  settings.oledShowTime = preferences.getBool("showTime", true);
  settings.oledShowDate = preferences.getBool("showDate", true);
  settings.oledShowDay = preferences.getBool("showDay", true);
  settings.oledShowBattery = preferences.getBool("showBatt", true);
  
  // LoRa settings
  settings.loraPowerOptimizationEnabled = preferences.getBool("loraPwrOpt", false);
  settings.transmitterTxPower = preferences.getUChar("loraTxPwr", 17);
  settings.transmitterAckAttempts = preferences.getUChar("loraAckAtt", 3);
  
  // Security
  preferences.getString("httpUser", settings.httpUsername, sizeof(settings.httpUsername));
  preferences.getString("httpPass", settings.httpPassword, sizeof(settings.httpPassword));
  if (strlen(settings.httpUsername) == 0) {
    strcpy(settings.httpUsername, "admin");
  }
  if (strlen(settings.httpPassword) == 0) {
    strcpy(settings.httpPassword, "admin");
  }
  
  // WiFi
  preferences.getString("wifiSSID", settings.wifiSSID, sizeof(settings.wifiSSID));
  preferences.getString("wifiPass", settings.wifiPassword, sizeof(settings.wifiPassword));
  
  // Schedules - load from preferences
  for (int i = 0; i < 10; i++) {
    String prefix = "sched" + String(i);
    settings.schedules[i].hour = preferences.getUChar((prefix + "h").c_str(), 0);
    settings.schedules[i].minute = preferences.getUChar((prefix + "m").c_str(), 0);
    settings.schedules[i].repeatType = preferences.getUChar((prefix + "r").c_str(), REPEAT_NO_REPEAT);
    settings.schedules[i].year = preferences.getUShort((prefix + "y").c_str(), 0);
    settings.schedules[i].month = preferences.getUChar((prefix + "mo").c_str(), 0);
    settings.schedules[i].day = preferences.getUChar((prefix + "d").c_str(), 0);
  }
  
  Serial.println("Settings loaded from NVS");
}

void saveSettings() {
  // Save all settings to NVS
  preferences.putUChar("sysProfile", settings.systemProfile);
  preferences.putUChar("pumpMode", settings.pumpMode);
  preferences.putUChar("startLevel", settings.startSensorLevel);
  preferences.putUChar("endLevel", settings.endSensorLevel);
  preferences.putUShort("schedDuration", settings.scheduleDurationMin);
  preferences.putUShort("primingTime", settings.primingTimeMs);
  preferences.putBool("forceSchRun", settings.forceScheduledRun);
  preferences.putBool("manualSafety", settings.manualModeSafetyLogicEnabled);
  
  preferences.putBool("dryRunEn", settings.dryRunLogicEnabled);
  preferences.putUShort("dryRunDelay", settings.dryRunDelayMin);
  preferences.putBool("retryEn", settings.retryLogicEnabled);
  preferences.putUChar("dryRunAttempt", settings.dryRunAttempts);
  preferences.putUShort("retryInterval", settings.retryIntervalMin);
  preferences.putBool("sensorBypass", settings.sensorErrorBypassEnabled);
  preferences.putBool("autoResetDR", settings.autoResetDryRunOnWater);
  preferences.putUShort("waterThresh", settings.waterPresenceThreshold);
  preferences.putUShort("waterStartDly", settings.waterSensingStartDelaySec);
  preferences.putUShort("waterStopDly", settings.waterSensingStopDelaySec);
  
  preferences.putBool("buzzerEn", settings.buzzerEnabled);
  preferences.putUChar("buzzerVol", settings.buzzerVolume);
  preferences.putUChar("dryRunBeep", settings.dryRunBeepStyle);
  preferences.putUChar("sensorBeep", settings.sensorErrorBeepStyle);
  preferences.putUChar("lowBattBeep", settings.lowBatteryBeepStyle);
  preferences.putUChar("hbBeep", settings.heartbeatExpiredBeepStyle);
  preferences.putUChar("emptyBeep", settings.tankEmptyBeepStyle);
  preferences.putUChar("fullBeep", settings.tankFullBeepStyle);
  preferences.putUChar("dirtyBeep", settings.dirtyWaterBeepStyle);
  
  preferences.putBool("turbBypass", settings.turbidityBypassEnabled);
  preferences.putUShort("turbTimeout", settings.turbidityTimeoutSec);
  preferences.putUShort("turbLimit", settings.turbidityLimit);
  preferences.putUShort("turbClean", settings.calibratedCleanValue);
  preferences.putUShort("turbDirty", settings.calibratedDirtyValue);
  
  preferences.putUChar("oledTheme", settings.oledTheme);
  preferences.putUChar("webTheme", settings.webTheme);
  preferences.putUShort("webRefresh", settings.webUiRefreshIntervalSec);
  preferences.putUChar("fontIndex", settings.currentFontIndex);
  preferences.putBool("showTime", settings.oledShowTime);
  preferences.putBool("showDate", settings.oledShowDate);
  preferences.putBool("showDay", settings.oledShowDay);
  preferences.putBool("showBatt", settings.oledShowBattery);
  
  preferences.putBool("loraPwrOpt", settings.loraPowerOptimizationEnabled);
  preferences.putUChar("loraTxPwr", settings.transmitterTxPower);
  preferences.putUChar("loraAckAtt", settings.transmitterAckAttempts);
  
  preferences.putString("httpUser", settings.httpUsername);
  preferences.putString("httpPass", settings.httpPassword);
  preferences.putString("wifiSSID", settings.wifiSSID);
  preferences.putString("wifiPass", settings.wifiPassword);
  
  // Save schedules
  for (int i = 0; i < 10; i++) {
    String prefix = "sched" + String(i);
    preferences.putUChar((prefix + "h").c_str(), settings.schedules[i].hour);
    preferences.putUChar((prefix + "m").c_str(), settings.schedules[i].minute);
    preferences.putUChar((prefix + "r").c_str(), settings.schedules[i].repeatType);
    preferences.putUShort((prefix + "y").c_str(), settings.schedules[i].year);
    preferences.putUChar((prefix + "mo").c_str(), settings.schedules[i].month);
    preferences.putUChar((prefix + "d").c_str(), settings.schedules[i].day);
  }
  
  Serial.println("Settings saved to NVS");
}

void factoryReset() {
  Serial.println("Factory reset initiated");
  preferences.clear();
  delay(100);
  ESP.restart();
}

// ===== SENSOR READING =====
void readSensors() {
  // Read tank level
  currentTankLevel = readTankLevel();
  
  // Read turbidity sensor
  turbidityValue = analogRead(TURBIDITY_PIN);
  
  // Read battery voltage (voltage divider scales higher voltage to ADC range)
  // Adjust BATTERY_DIVIDER_R constant based on your voltage divider circuit
  // Example: For 0-16.5V scaled to 0-3.3V, use 5.0 (5:1 ratio)
  int batteryRaw = analogRead(BATTERY_PIN);
  batteryVoltage = (batteryRaw / ADC_MAX_VALUE) * ADC_REFERENCE_V * BATTERY_DIVIDER_R;
  
  // Read flow sensor (simplified - adjust based on your sensor)
  int flowRaw = analogRead(FLOW_SENSOR_PIN);
  flowOk = (flowRaw > settings.waterPresenceThreshold);
  
  // Check turbidity
  if (!settings.turbidityBypassEnabled) {
    if (pumpRunning && turbidityValue > settings.turbidityLimit) {
      playBeep(settings.dirtyWaterBeepStyle);
    }
  }
}

uint8_t readTankLevel() {
  // Read tank level sensors (inverted logic - sensors are active LOW)
  bool level0 = !digitalRead(TANK_LEVEL_0_PIN);  // Empty
  bool level1 = !digitalRead(TANK_LEVEL_1_PIN);  // 25%
  bool level2 = !digitalRead(TANK_LEVEL_2_PIN);  // 50%
  bool level3 = !digitalRead(TANK_LEVEL_3_PIN);  // 75%
  bool level4 = !digitalRead(TANK_LEVEL_4_PIN);  // Full
  
  // Determine tank level
  if (level4) return 4;
  if (level3) return 3;
  if (level2) return 2;
  if (level1) return 1;
  if (level0) return 0;
  
  // If no sensor is active, there's an error
  sensorError = SENSOR_ERROR_TANK_LEVEL;
  return 0;
}

// ===== PUMP CONTROL =====
void controlPump() {
  bool shouldRun = false;
  
  switch (settings.pumpMode) {
    case PUMP_MODE_SCHEDULE:
      // Pump control handled by checkSchedules()
      break;
      
    case PUMP_MODE_AUTO:
      // Start if tank at or below start level
      if (currentTankLevel <= settings.startSensorLevel) {
        shouldRun = true;
      }
      // Stop if tank at or above end level
      if (currentTankLevel >= settings.endSensorLevel) {
        shouldRun = false;
      }
      break;
      
    case PUMP_MODE_MANUAL:
      // Manual mode - pump state is controlled by user
      // Safety logic can override if enabled
      if (settings.manualModeSafetyLogicEnabled) {
        if (currentTankLevel >= settings.endSensorLevel) {
          shouldRun = false;
        }
      }
      break;
      
    case PUMP_MODE_WATER_SENSING:
      // Start pump if water is detected
      if (flowOk) {
        shouldRun = true;
      }
      break;
  }
  
  // Apply dry run error override
  if (dryRunError) {
    shouldRun = false;
  }
  
  // Apply sensor error override
  if (sensorError != SENSOR_ERROR_NONE && !settings.sensorErrorBypassEnabled) {
    shouldRun = false;
  }
  
  // Handle priming phase (non-blocking)
  if (pumpPriming) {
    if (millis() - pumpPrimingStartTime >= settings.primingTimeMs) {
      // Priming complete, turn on pump
      digitalWrite(PUMP_RELAY_PIN, HIGH);
      pumpRunning = true;
      pumpPriming = false;
      pumpStartTime = millis();
      Serial.println("Pump started (priming complete)");
    }
    return; // Don't process other pump logic during priming
  }
  
  // Update pump state
  if (shouldRun && !pumpRunning && !pumpPriming) {
    startPump();
  } else if (!shouldRun && pumpRunning) {
    stopPump();
  }
  
  // Check max run time
  if (pumpRunning && settings.scheduleDurationMin > 0) {
    if ((millis() - pumpStartTime) > (settings.scheduleDurationMin * 60000UL)) {
      Serial.println("Max run time reached");
      stopPump();
    }
  }
}

void startPump() {
  if (settings.primingTimeMs > 0) {
    // Start priming phase (non-blocking)
    Serial.printf("Starting priming phase for %d ms\n", settings.primingTimeMs);
    pumpPriming = true;
    pumpPrimingStartTime = millis();
    // Pump relay stays LOW during priming
  } else {
    // No priming, start immediately
    Serial.println("Starting pump");
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    pumpRunning = true;
    pumpStartTime = millis();
  }
}

void stopPump() {
  Serial.println("Stopping pump");
  digitalWrite(PUMP_RELAY_PIN, LOW);
  pumpRunning = false;
  pumpPriming = false;  // Cancel priming if active
}

// ===== SCHEDULE MANAGEMENT =====
void checkSchedules() {
  DateTime now = rtc.now();
  nextSchedule = "None";
  
  for (int i = 0; i < 10; i++) {
    Schedule &sched = settings.schedules[i];
    
    // Skip empty schedules
    if (sched.hour == 0 && sched.minute == 0 && sched.year == 0) {
      continue;
    }
    
    bool shouldTrigger = false;
    
    switch (sched.repeatType) {
      case REPEAT_EVERY_DAY:
        if (now.hour() == sched.hour && now.minute() == sched.minute) {
          shouldTrigger = true;
        }
        break;
        
      case REPEAT_ODD_DAY:
        if ((now.day() % 2 == 1) && now.hour() == sched.hour && now.minute() == sched.minute) {
          shouldTrigger = true;
        }
        break;
        
      case REPEAT_EVEN_DAY:
        if ((now.day() % 2 == 0) && now.hour() == sched.hour && now.minute() == sched.minute) {
          shouldTrigger = true;
        }
        break;
        
      case REPEAT_SPECIFIC_DATE:
        if (now.year() == sched.year && now.month() == sched.month && 
            now.day() == sched.day && now.hour() == sched.hour && 
            now.minute() == sched.minute) {
          shouldTrigger = true;
        }
        break;
    }
    
    if (shouldTrigger) {
      if (settings.forceScheduledRun || currentTankLevel <= settings.startSensorLevel) {
        if (!pumpRunning) {
          Serial.printf("Schedule %d triggered\n", i);
          startPump();
        }
      }
    }
    
    // Calculate next schedule time
    if (nextSchedule == "None") {
      nextSchedule = String(sched.hour) + ":" + String(sched.minute);
    }
  }
}

// ===== DRY RUN PROTECTION =====
void handleDryRun() {
  if (!pumpRunning || dryRunError) return;
  
  // Check if pump has been running for dry run delay period
  unsigned long runTime = (millis() - pumpStartTime) / 60000UL;  // Convert to minutes
  
  if (runTime >= settings.dryRunDelayMin) {
    // Check flow
    if (!flowOk) {
      Serial.println("DRY RUN ERROR DETECTED!");
      dryRunError = true;
      stopPump();
      playBeep(settings.dryRunBeepStyle);
      remainingAttempts--;
    }
  }
}

void handleRetryLogic() {
  static unsigned long lastRetryTime = 0;
  
  if (!dryRunError || remainingAttempts == 0) return;
  
  unsigned long elapsedMin = (millis() - lastRetryTime) / 60000UL;
  
  if (elapsedMin >= settings.retryIntervalMin) {
    Serial.printf("Retry attempt %d\n", settings.dryRunAttempts - remainingAttempts + 1);
    
    // Check if water is present
    if (settings.autoResetDryRunOnWater && flowOk) {
      Serial.println("Water detected, resetting dry run error");
      dryRunError = false;
      remainingAttempts = settings.dryRunAttempts;
      return;
    }
    
    // Retry starting pump
    dryRunError = false;
    lastRetryTime = millis();
  }
}

// ===== BUZZER CONTROL =====
void playBeep(uint8_t beepStyle) {
  if (!settings.buzzerEnabled) return;
  
  int duration = 200;
  int frequency = 2000;
  
  switch (beepStyle) {
    case BEEP_SILENT:
      return;
    case BEEP_ALERT:
      tone(BUZZER_PIN, 2000, 200);
      break;
    case BEEP_WARNING:
      tone(BUZZER_PIN, 1000, 500);
      break;
    case BEEP_PULSE:
      for (int i = 0; i < 3; i++) {
        tone(BUZZER_PIN, 2500, 100);
        delay(150);
      }
      break;
    case BEEP_LONG:
      tone(BUZZER_PIN, 1500, 1000);
      break;
    case BEEP_SPARROW:
      for (int i = 0; i < 5; i++) {
        tone(BUZZER_PIN, 2000 + i * 200, 50);
        delay(70);
      }
      break;
  }
}

// ===== DISPLAY UPDATE =====
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  DateTime now = rtc.now();
  
  // Show time if enabled
  if (settings.oledShowTime) {
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    display.println(timeStr);
  }
  
  // Show date if enabled
  if (settings.oledShowDate) {
    char dateStr[11];
    sprintf(dateStr, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    display.println(dateStr);
  }
  
  // Show pump status
  display.print("Pump: ");
  display.println(pumpRunning ? "ON" : "OFF");
  
  // Show tank level
  display.print("Tank: ");
  display.print(currentTankLevel * 25);
  display.println("%");
  
  // Show errors
  if (dryRunError) {
    display.println("DRY RUN ERROR!");
  }
  if (sensorError != SENSOR_ERROR_NONE) {
    display.println("SENSOR ERROR!");
  }
  
  // Show battery if enabled
  if (settings.oledShowBattery) {
    display.print("Batt: ");
    display.print(batteryVoltage, 1);
    display.println("V");
  }
  
  display.display();
}

// ===== LORA COMMUNICATION =====
void sendLoRaMessage() {
  // Send status update via LoRa
  LoRa.beginPacket();
  LoRa.print(deviceId);
  LoRa.print(",");
  LoRa.print(pumpRunning ? "1" : "0");
  LoRa.print(",");
  LoRa.print(currentTankLevel);
  LoRa.print(",");
  LoRa.print(turbidityValue);
  LoRa.endPacket();
}

void handleLoRaMessage() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String message = "";
    while (LoRa.available()) {
      message += (char)LoRa.read();
    }
    Serial.printf("LoRa received: %s (RSSI: %d)\n", message.c_str(), LoRa.packetRssi());
    
    // Parse and handle LoRa commands here
    // Format: "CMD:VALUE"
    int colonIndex = message.indexOf(':');
    if (colonIndex > 0) {
      String cmd = message.substring(0, colonIndex);
      String value = message.substring(colonIndex + 1);
      
      if (cmd == "PUMP") {
        if (value == "ON" && !pumpRunning) {
          startPump();
        } else if (value == "OFF" && pumpRunning) {
          stopPump();
        }
      }
    }
  }
}

// Web server handlers are in WebServer.ino
