static const char *TAG = "SURSAJNI";

#include "driver/spi_master.h"
#include <LoRa.h>
#include "driver/i2c.h"
#include <RTClib.h>
#include <U8g2lib.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_task_wdt.h"
#include "logo.h" // Assuming logo.h contains the sursajni_logo XBM data
#include "driver/ledc.h" // Explicitly include for LEDC functions

// NEW: WiFi and WebServer Libraries
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
// ArduinoJson can still be used with ESP-IDF
#include <ArduinoJson.h> // NEW: Include ArduinoJson library for efficient JSON handling
#include "mdns.h" // NEW: For OTA hostname resolution
// #include <Hash.h> // <<< REMOVED: This include was causing errors
#include "esp_ota_ops.h"
#include "esp_https_ota.h" // NEW: For Over-the-Air updates


// NEW: FreeRTOS headers for tasks, queues, and mutexes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// NEW: NTP Client Libraries for time synchronization
#include "lwip/sockets.h"
#include "esp_sntp.h"

// --- NEW: LoRa Packet Structure (Synchronized with Transmitter) ---
#pragma pack(push, 1) // Ensures the compiler doesn't add padding bytes
struct LoRaPacket {
    uint8_t groupID;
    uint16_t serialID;
    uint8_t switchStates[5]; // [Flow, 25%, 50%, 75%, 100%]
    uint16_t batteryVoltage_mV;
    uint8_t txPower;
    uint8_t ackAttempts;
};
#pragma pack(pop)


// === Pin Definitions for ESP32 WROOM 38 ===
// Note: These pins have been selected to be general-purpose GPIOs on a typical ESP32 WROOM 38 board.
// They have been changed from the previous ESP32-S3 pinout.
#define LORA_CS_PIN       5
#define LORA_RESET_PIN    14 
#define LORA_DIO0_PIN     2 
#define LORA_SCK          18
#define LORA_MISO         19
#define LORA_MOSI         23

#define RX_LED_PIN        13  // RX LED (MOVED from 25 to 13 for testing)
#define BUZZER_PIN        26 // Buzzer pin
#define HEARTBEAT_LED_PIN 27
#define BATTERY_LED_PIN   33 // Not actively used for blinking in this sketch
#define RELAY_PIN         16
#define VALVE_RELAY_PIN   17 // New low-triggered relay for valve
#define BUTTON_SET        4
#define BUTTON_UP         0
#define BUTTON_DOWN       3 // UPDATED: Changed from pin 34 to 3. GPIO3 is a general-purpose GPIO with internal pull-up.

// NEW: Turbidity Sensor Pin
#define TURBIDITY_SENSOR_PIN 34 // An analog-capable pin

// === RTC & Display Objects (Global Declarations) ===
RTC_DS3231 rtc;
// Changed U8g2 constructor for 1.3-inch OLED, commonly uses SH1106 driver
// UPDATED PINS for ESP32 standard I2C (SCL=22, SDA=21)
// MODIFIED: Changed driver to SSD1306 for 0.96" OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 22, 21); 

// NEW: WebServer object
WebServer server(80); // Web server on port 80

// NEW: NTP Client for time synchronization
WiFiUDP ntpUDP;
// India is UTC +5:30, which is 5.5 * 3600 = 19800 seconds
const long utcOffsetInSeconds = 19800; 
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
// OLD: bool ntpSyncCompleted = false; // Flag to ensure we sync only once per connection
// NEW: Variables for periodic NTP sync
// REMOVED: No longer needed here, the task will manage its own timing
// unsigned long lastNtpSyncTime = 0; // Timestamp of the last sync ATTEMPT
const unsigned long NTP_SYNC_INTERVAL_MS = 3600000UL; // 1 hour
bool ntpSyncCompleted = false; // Still used to track if we've synced at least once for the display icon

// NEW: Handle for the background NTP sync task
TaskHandle_t NtpSyncTaskHandle = NULL;
static uint8_t lastCheckDay = 0; // NEW: For robust daily reset

// NEW: Handles and mutex for LoRa task and shared data
TaskHandle_t LoraTaskHandle = NULL;
QueueHandle_t loraPacketQueue = NULL;
SemaphoreHandle_t sharedDataMutex = NULL;


// === WiFi Credentials (for AP mode) and Web UI Credentials ===
char ap_ssid[32] = "Sursajni"; // NEW: Make this a mutable char array for dynamic naming
// OLD: const char* ap_password = "sirftumhareliye"; // Used for AP mode password
// NEW: Make passwords mutable char arrays to allow changing them
char ap_password[64] = ""; // Set to empty for an open network by default
char http_username[32] = "admin"; // Username is now a char array, though not editable in this version
char http_password[64] = "sirftumhareliye";

// === Icons (Global Declarations) ===
const uint8_t waterDrop[] U8X8_PROGMEM = {0x18,0x3C,0x7E,0xFF,0x7E,0x3C,0x18,0x00};
const uint8_t checkMark[] U8X8_PROGMEM = {0x00,0x10,0x18,0x1C,0x0C,0x06,0x02,0x00};
const uint8_t hourglass[] U8X8_PROGMEM = {0xFF,0x81,0x42,0x24,0x24,0x42,0x81,0xFF};
const uint8_t clockIcon[] U8X8_PROGMEM = {0x3C,0x42,0xA5,0xA9,0x91,0x89,0x42,0x3C};
const uint8_t signalBar[] U8X8_PROGMEM = {0x00,0x10,0x18,0x1C,0x1E,0x1F,0x1F,0x1F};
const uint8_t signalCut[] U8X8_PROGMEM = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

// NEW: Custom WiFi Icons (8x8 XBM format)
const uint8_t wifiConnectedIcon[] U8X8_PROGMEM = {
  0x00, // 00000000
  0x00, // 00000000
  0x08, // 00001000  (top dot)
  0x14, // 00010100  (small arc)
  0x22, // 00100010  (medium arc)
  0x41, // 01000001  (large arc)
  0x00, // 00000000
  0x00  // 00000000
};

const uint8_t wifiDisconnectedIcon[] U8X8_PROGMEM = {
  0x81, // 10000001
  0x42, // 01000010
  0x24, // 00100100
  0x10, // 00010000
  0x24, // 00100100
  0x42, // 01000010
  0x81, // 10000001
  0x00  // 00000000
};

// NEW: NTP sync icon (8x8 XBM format)
const uint8_t ntpIcon[] U8X8_PROGMEM = {
  0x00, 0x81, 0xC1, 0xA1, 0x91, 0x89, 0x81, 0x00
};

// NEW: Turbidity icon (8x8 XBM format)
const uint8_t turbidityIcon[] U8X8_PROGMEM = {
  0x0E, // 00001110
  0x17, // 00010111
  0x2B, // 00101011
  0x4A, // 01001010
  0x84, // 10000100
  0x4A, // 01001010
  0x2B, // 00101011
  0x17  // 00010111
};


// New: Tank Level and Sensor Error related enums and variables (MOVED TO TOP)
// UPDATED: Tank levels to match the new transmitter's switches (25, 50, 75, 100)
enum TankLevel {
    TANK_EMPTY = 0,
    TANK_25 = 1,
    TANK_50 = 2,
    TANK_75 = 3,
    TANK_FULL = 4,
    TANK_INVALID = 99 // Indicates an invalid sensor combination or initial state
};
TankLevel currentTankLevel = TANK_INVALID; // Start as invalid/unknown
TankLevel lastObservedValidTankLevel = TANK_INVALID; // Last level that was valid and in sequence
TankLevel previousTankLevel = TANK_INVALID; // To detect tank level changes for buzzer (NOW DECLARED AFTER ENUM)

// Changed sensorError to an enum for more specific error types
enum SensorErrorType {
    SENSOR_ERROR_NONE = 0,
    SENSOR_ERROR_INVALID_COMBINATION = 1,
    SENSOR_ERROR_SEQUENCE_UP = 2,
    SENSOR_ERROR_SEQUENCE_DOWN = 3,
    SENSOR_ERROR_DIRTY_WATER = 4
};
SensorErrorType sensorError = SENSOR_ERROR_NONE; // Initialize to no error
char sensorErrorDetailMessage[30] = ""; // New: To hold specific sensor error details (using char array)

// === System State Variables (Global Declarations) ===
bool inSettingsMenu = false; // Global flag for settings menu state
bool firstHeartbeatReceived = false;
bool isRtcWorking = false; // NEW: Flag to track if the RTC module is found and working
float batteryVoltage = 0.0; // NEW: Global variable for battery voltage
bool lowBattery = false; // Flag derived from batteryVoltage
bool prevSwitchState[5] = {0}; // UPDATED: Stores last received 5 switch states
bool waitingForFlow = false; // True if pump is on and waiting for flow sensor to detect water
bool flowOk = false;      // True if flow is detected
bool dryRunError = false;    // True if dry run attempts exhausted
bool pumpRunning = false;    // True if relay is 1
bool retryInProgress = false; // True if pump off, waiting for retry interval
uint8_t remainingDryRunAttempts = 0;
// REMOVED: bool rxLedState = false; // No longer needed
unsigned long rxLedOnTime = 0; // NEW: Timer for RX LED blink duration
unsigned long pumpStartTime = 0;         // Timestamp when pump was last turned ON
unsigned long lastScreenSwitch = 0;      // Timestamp for display rotation
unsigned long buttonHoldStart = 0;      // Timestamp for dry run reset button hold
unsigned long retryStartTime = 0;        // Timestamp when retry interval started
bool waitingForPumpRelay = false; // True when valve relay is on, waiting to start pump relay
unsigned long valveRelayStartTime = 0; // Timestamp for valve relay start
uint8_t screenState = 0; // Current state for rotating display content
DateTime lastHeartbeatTimeRTC;          // RTC time of last heartbeat from remote unit
bool pumpBlockedMessageActive = false;
unsigned long pumpBlockedMessageStartTime = 0;
bool schedulesClearedMessageActive = false;
unsigned long schedulesClearedMessageStartTime = 0;
// NEW: Flags for schedule validation error message
bool scheduleInvalidMessageActive = false;
unsigned long scheduleInvalidMessageStartTime = 0;
// NEW (FIX): Declare the missing variables for the single schedule cleared message
bool singleScheduleClearedMessageActive = false;
unsigned long singleScheduleClearedMessageStartTime = 0;
// NEW: Flags for error reset messages
bool dryRunResetMessageActive = false;
unsigned long dryRunResetMessageStartTime = 0;
bool sensorErrorResetMessageActive = false;
unsigned long sensorErrorResetMessageStartTime = 0;
// NEW: Flags for factory reset confirmation screen
bool factoryResetConfirmActive = false;
unsigned long factoryResetConfirmStartTime = 0;
// NEW FIX: Timestamp to ignore button presses after a long-press exit
unsigned long ignoreButtonsUntil = 0; 
// NEW: Flags for NTP sync display messages
bool isNtpSyncingMessageActive = false;
bool isNtpSyncedMessageActive = false;
unsigned long ntpMessageStartTime = 0;
const unsigned long NTP_MESSAGE_DURATION_MS = 2000; // 2 seconds for the "Synced!" message
// NEW: Add flags for turbidity limit set confirmation message
bool limitSetMessageActive = false;
unsigned long limitSetMessageStartTime = 0;
// bool wifiActive = false; // OLD: Flag to indicate if WiFi AP is active (now handled by wifiMode and staConnected)
// NEW: OTA Update Flag
bool otaUpdateInProgress = false;

// NEW: To store the RSSI of the last received packet
int lastPacketRssi = 0; 

// NEW: Water Sensing setting
uint16_t waterPresenceThreshold = 1850; // Analog value (0-4095) ABOVE which water is considered present
// NEW: Configurable delays for Water Sensing mode
uint16_t waterSensingStartDelaySec = 5; // Default 5 seconds
uint16_t waterSensingStopDelaySec = 5;  // Default 5 seconds
unsigned long waterPresentStartTime = 0; // NEW: Timer for water presence start delay
unsigned long waterAbsentStartTime = 0; // NEW: Timer for water absence stop delay

// NEW: Start/End sensor level settings
TankLevel startSensorLevel = TANK_EMPTY;
TankLevel endSensorLevel = TANK_FULL;

// NEW: Turbidity sensor variables
uint16_t turbidityValue = 0; // Raw analog value from sensor
uint16_t turbidityLimit = 100; // NEW: User configurable turbidity limit (0-4095)
bool isWaterDirty = false;
bool isDirtyWaterError = false; // A persistent flag for the dirty water error state
unsigned long dirtyWaterDetectedTime = 0; // Timestamp when dirty water was first detected
// REMOVED: const unsigned long TURBIDITY_TIMEOUT_MS = 3000; // 3 seconds timeout

// NEW: Globals for non-blocking turbidity reading
uint32_t turbiditySampleSum = 0;
uint16_t turbiditySampleCount = 0;
unsigned long lastTurbiditySampleTime = 0;
const unsigned long TURBIDITY_SAMPLE_INTERVAL_MS = 5; // 5ms between samples

// NEW: Calibration state management
enum CalibrationState {
    CAL_IDLE,
    CAL_CLEAN_WATER_SAMPLING,
    CAL_DIRTY_WATER_SAMPLING,
    CAL_DONE
};
CalibrationState currentCalibrationState = CAL_IDLE;
uint16_t calibratedCleanValue = 0; // Stores the reading for clean water
uint16_t calibratedDirtyValue = 4095; // Stores the reading for dirty water
unsigned long calibrationStartTime = 0;
const unsigned long CALIBRATION_SAMPLE_DURATION_MS = 5000; // 5 seconds for sampling
uint32_t calibrationSampleSum = 0;
uint16_t calibrationSampleCount = 0;

// NEW: Flag to indicate if a web server request is being handled
volatile bool isWebRequestActive = false;

// NEW: Function prototype for our new Web UI task
void webUiTask(void *pvParameters);

// NEW: Function prototypes for LoRa task and queue processing
void loraTask(void *pvParameters);
void processLoRaQueue();

// NEW: Add function prototype for isAuthenticated to resolve declaration error
bool isAuthenticated();

// NEW: WiFi Mode and STA connection status
enum WifiMode { WIFI_AP_MODE = 0, WIFI_STA_MODE = 1 };
WifiMode wifiMode = WIFI_AP_MODE; // Default to AP mode
char staSsid[32] = "YourHomeSSID"; // Default STA SSID
char staPassword[64] = "YourHomePassword"; // Default STA Password
bool staConnected = false; // True if connected to STA network
unsigned long lastWifiConnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 30000; // Try to reconnect every 30 seconds

// NEW: Flag to track if flow was detected during the current pump run
bool currentPumpRunHadFlow = false;

// === Buzzer Control Variables ===
bool buzzerEnabled = true;
uint8_t buzzerVolume = 128; // 0-255, for PWM
// Replaced multiple flags with a single enum for cleaner state management
enum BuzzerState { BUZZER_OFF, BUZZER_BRIEF, BUZZER_CONTINUOUS, BUZZER_SEQUENCE };
BuzzerState currentBuzzerState = BUZZER_OFF;
unsigned long buzzerActiveUntil = 0;
unsigned long buzzerToggleTime = 0;
uint8_t currentBeepCount = 0;
unsigned long beepOnDuration = 0;
unsigned long beepOffDuration = 0;
bool isBuzzerCurrentlyOn = false;

// NEW: Flags to prevent continuous re-triggering of event beeps
bool isDryRunErrorBeepActive = false;
bool isSensorErrorBeepActive = false;
bool isLowBatteryBeepActive = false;
bool isHeartbeatExpiredBeepActive = false;
bool isTankEmptyBeepActive = false;
bool isTankFullBeepActive = false;
bool isDirtyWaterBeepActive = false; // NEW
unsigned long lastLowBatteryBeepTime = 0; // NEW: Timer for hourly low battery beep

// Define LEDC channel for buzzer PWM
#define BUZZER_LEDC_CHANNEL LEDC_CHANNEL_0 // Use LEDC_CHANNEL_0 enum
#define BUZZER_LEDC_RESOLUTION LEDC_TIMER_8_BIT // 8-bit resolution (0-255)
#define BUZZER_LEDC_FREQ 4000 // MODIFIED: Changed from 2000Hz to 4000Hz for potentially higher volume
#define BUZZER_LEDC_TIMER LEDC_TIMER_0 // Use LEDC_TIMER_0 enum

// Button state struct definition
struct ButtonState {
    bool lastStableState = 1;
    bool lastReadState = 1;
    unsigned long lastChangeTime = 0;
    bool isPressed = false; // True if button is currently held down (debounced)
    unsigned long pressStartTime = 0; // Timestamp when button was initially pressed
};
ButtonState setBtn, upBtn, downBtn; // Declare button state variables globally

// === Constants ===
const uint8_t allowedGroupID = 0x01;
const uint16_t allowedSerialID = 0x01AF;
#define HEARTBEAT_TIMEOUT 10800000UL // 3 hours in milliseconds
#define BATTERY_BLINK_MS      500 // Not currently used
#define DRY_RUN_DISPLAY_ROTATION_MS 3000
#define BUTTON_HOLD_TIME_MS 5000 // Time for long press to reset dry run error
#define PUMP_BLOCKED_DISPLAY_DURATION_MS 3000 // Duration for "PUMP BLOCKED" message
#define SCHEDULE_CLEARED_DISPLAY_DURATION_MS 2000 // Duration for "Schedule cleared" message
#define SCHEDULE_INVALID_DISPLAY_DURATION_MS 3000 // NEW: Duration for "Check Date OR Time" message
#define ERROR_RESET_MESSAGE_DURATION_MS 2000 // NEW: Duration for error reset messages
// #define VALVE_OPEN_TIME_MS 2000 // REMOVED: Replaced by primingTimeMs setting

// Constants for button debouncing and auto-repeat
#define DEBOUNCE_INTERVAL_MS 10   // Time for button debounce
#define BUTTON_REPEAT_DELAY_MS 500 // Initial delay before repeating value change
#define BUTTON_REPEAT_RATE_MS      100 // Interval for repeating value change
#define BUTTON_LONG_PRESS_THRESHOLD_MS 1000 // Time to consider it a long press for manual pump toggle

// MODIFIED: Increased number of schedules from 3 to 10
#define MAX_SCHEDULES 10

// NEW: RSSI thresholds for power optimization
#define RSSI_TOO_STRONG -60
#define RSSI_WEAK -85

// === Schedule Struct ===
// MODIFIED: Added SPECIFIC_DATE and updated struct for date fields
enum RepeatType { EVERY_DAY = 0, ODD_DAY = 1, EVEN_DAY = 2, NO_REPEAT = 3, SPECIFIC_DATE = 4 };
struct Schedule {
    uint8_t hour;
    uint8_t minute;
    RepeatType repeatType;
    uint16_t year;  // New: for SPECIFIC_DATE
    uint8_t month; // New: for SPECIFIC_DATE
    uint8_t day;   // New: for SPECIFIC_DATE
};

// MODIFIED: Initialize 10 schedules with default times and date fields set to 0
Schedule schedules[MAX_SCHEDULES] = {
    {6,0, EVERY_DAY, 0, 0, 0}, {12,0, EVERY_DAY, 0, 0, 0}, {18,0, EVERY_DAY, 0, 0, 0},
    {0,0, EVERY_DAY, 0, 0, 0}, {0,0, EVERY_DAY, 0, 0, 0}, {0,0, EVERY_DAY, 0, 0, 0},
    {0,0, EVERY_DAY, 0, 0, 0}, {0,0, EVERY_DAY, 0, 0, 0}, {0,0, EVERY_DAY, 0, 0, 0},
    {0,0, EVERY_DAY, 0, 0, 0}
};
// MODIFIED: Initialize trigger flags for 10 schedules
bool scheduleTriggeredToday[MAX_SCHEDULES] = {false, false, false, false, false, false, false, false, false, false};

// === Menu Settings ===
// Renamed PumpMode: AUTO -> SCHEDULE, MANUAL -> AUTO, added MANUAL_OVERRIDE
// NEW: Added WATER_SENSING mode
enum PumpMode { SCHEDULE = 0, AUTO = 1, MANUAL = 2, WATER_SENSING = 3 }; // Renamed MANUAL_OVERRIDE to MANUAL

// NEW: System Profile Enum
enum SystemProfile {
    PROFILE_LEVEL_INDICATOR = 0,
    PROFILE_MONOBLOCK = 1,
    PROFILE_SUBMERSIBLE = 2,
    PROFILE_FULLY_AUTOMATIC = 3
};
SystemProfile currentProfile = PROFILE_FULLY_AUTOMATIC; // Default to fully automatic

PumpMode pumpMode = SCHEDULE; // Default to SCHEDULE mode
uint8_t dryRunDelayMin = 1; // Minimum 1 minute
uint8_t dryRunAttempts = 1; // Minimum 1 attempt
uint8_t retryIntervalMin = 1; // Minimum 1 minute
uint8_t scheduleDurationMin = 10; // Minimum 0 minutes (0 means indefinite run until tank full)
uint16_t primingTimeMs = 2000; // NEW: Priming time setting (100-10000ms), default 2s
bool forceScheduledRun = false;
bool sensorErrorBypassEnabled = false; // New: Sensor Error Bypass setting
uint8_t currentFontIndex = 0; // New: Index for selected display font
bool manualModeSafetyLogicEnabled = false; // NEW: Enable safety logic in manual mode
bool retryLogicEnabled = true; // NEW: Enable/disable dry run retry logic
bool dryRunLogicEnabled = true; // NEW: Enable/disable dry run logic
bool turbidityBypassEnabled = false; // NEW: Enable/disable turbidity sensor logic
uint8_t turbidityTimeoutSec = 3; // NEW: User configurable dirty water timeout (1-60s)
bool autoResetDryRunOnWater = false; // NEW: Auto-reset dry run error when water is detected
bool autoResetTurbidityOnError = false; // NEW: Auto-reset turbidity error on new water cycle
bool loraPowerOptimizationEnabled = false; // NEW: LoRa Power Optimization setting

// NEW: State tracking for auto-resetting turbidity error
bool turbidityErrorWaterAbsent = false;

// NEW: OLED Layout Settings
bool oledShowTime = true;
bool oledShowDate = false;
bool oledShowDay = false;
bool oledShowBattery = true;

// NEW: Web UI Refresh Interval
uint16_t webUiRefreshIntervalSec = 10; // Default 10 seconds refresh interval for Web UI

// NEW: Theme enum for OLED
enum OledTheme { DARK = 0, LIGHT = 1 };
OledTheme currentOledTheme = DARK;

// NEW: Theme enum for Web UI
uint8_t currentWebTheme = 0; // 0: Dark, 1: Light, 2: Oceanic, 3: Sunset, 4: Forest

// Array of available fonts for the main display area
const uint8_t* displayFonts[] = {
    u8g2_font_helvB14_tr, // Default large font
    u8g2_font_7x13_tf,    // Medium font
    u8g2_font_6x12_tr,    // Small font
    u8g2_font_ncenB08_tr, // Another small, bold font
    u8g2_font_ncenR10_tr  // A slightly larger, regular font
};
const char* displayFontNames[] = {
    "HelvB14",
    "7x13",
    "6x12",
    "ncenB08",
    "ncenR10"
};
const uint8_t numDisplayFonts = sizeof(displayFonts) / sizeof(displayFonts[0]);

// NEW: Buzzer beep styles for configuration
enum BeepStyle {
    BEEP_STYLE_SILENT = 0,
    BEEP_STYLE_ALERT = 1,       // Three rapid beeps for urgent alerts
    BEEP_STYLE_WARNING = 2,     // Three slow beeps for warnings
    BEEP_STYLE_PULSE = 3,       // A pulsing sound for attention
    BEEP_STYLE_LONG = 4,        // A continuous long beep
    BEEP_STYLE_SPARROW = 5,     // A rapid, chirping sound
};

// NEW: Variables to store the chosen beep style for each event
BeepStyle dryRunBeepStyle = BEEP_STYLE_ALERT;
BeepStyle sensorErrorBeepStyle = BEEP_STYLE_WARNING;
BeepStyle lowBatteryBeepStyle = BEEP_STYLE_PULSE;
BeepStyle heartbeatExpiredBeepStyle = BEEP_STYLE_PULSE;
BeepStyle tankEmptyBeepStyle = BEEP_STYLE_LONG;
BeepStyle tankFullBeepStyle = BEEP_STYLE_LONG;
BeepStyle dirtyWaterBeepStyle = BEEP_STYLE_WARNING; // NEW

const char* beepStyleNames[] = {
    "SILENT", "ALERT", "WARNING", "PULSE", "LONG", "SPARROW"
};
const uint8_t numBeepStyles = sizeof(beepStyleNames) / sizeof(beepStyleNames[0]);


// === EEPROM Addresses (UPDATED for 10 schedules) ===
// The schedule block now takes up MAX_SCHEDULES * 4 bytes.
// All subsequent addresses are automatically recalculated based on this.
#define EEPROM_MAGIC_ADDR      0
#define EEPROM_MAGIC_VALUE     0xA5
#define ADDR_PUMP_MODE         1
#define ADDR_DRY_DELAY         2
#define ADDR_DRY_ATTEMPTS      3
#define ADDR_RETRY_INTERVAL    4
#define ADDR_SCHED_DURATION    5
#define ADDR_SCHEDULE_START    10
// MODIFIED: Recalculated all subsequent EEPROM addresses due to smaller Schedule struct size
#define ADDR_FORCE_SCHEDULED_RUN (ADDR_SCHEDULE_START + MAX_SCHEDULES * 7)
#define ADDR_CLEAR_SCHEDULES_OPTION (ADDR_FORCE_SCHEDULED_RUN + 1)
#define ADDR_SENSOR_ERROR_BYPASS (ADDR_CLEAR_SCHEDULES_OPTION + 1)
#define ADDR_DISPLAY_FONT_INDEX (ADDR_SENSOR_ERROR_BYPASS + 1)
#define ADDR_BUZZER_ENABLED    (ADDR_DISPLAY_FONT_INDEX + 1)
#define ADDR_MANUAL_MODE_SAFETY_LOGIC (ADDR_BUZZER_ENABLED + 1)
#define ADDR_PRIMING_TIME      (ADDR_MANUAL_MODE_SAFETY_LOGIC + 1)
#define ADDR_WIFI_MODE         (ADDR_PRIMING_TIME + 2)
#define ADDR_STA_SSID          (ADDR_WIFI_MODE + 1)
#define ADDR_STA_PASSWORD      (ADDR_STA_SSID + 32)
#define ADDR_OLED_THEME        (ADDR_STA_PASSWORD + 64)
#define ADDR_WEB_THEME         (ADDR_OLED_THEME + 1)
#define ADDR_DRY_RUN_LOGIC_ENABLED (ADDR_WEB_THEME + 1)
#define ADDR_RETRY_LOGIC_ENABLED (ADDR_DRY_RUN_LOGIC_ENABLED + 1)
#define ADDR_BUZZER_VOLUME     (ADDR_RETRY_LOGIC_ENABLED + 1) // NEW: Address for buzzer volume
// NEW: EEPROM addresses for buzzer styles
#define ADDR_DRY_RUN_BEEP      (ADDR_BUZZER_VOLUME + 1) // SHIFTED
#define ADDR_SENSOR_BEEP       (ADDR_DRY_RUN_BEEP + 1)
#define ADDR_LOW_BATTERY_BEEP  (ADDR_SENSOR_BEEP + 1)
#define ADDR_HEARTBEAT_BEEP    (ADDR_LOW_BATTERY_BEEP + 1)
#define ADDR_TANK_EMPTY_BEEP   (ADDR_HEARTBEAT_BEEP + 1)
#define ADDR_TANK_FULL_BEEP    (ADDR_TANK_EMPTY_BEEP + 1)
#define ADDR_DIRTY_WATER_BEEP  (ADDR_TANK_FULL_BEEP + 1) // NEW
#define ADDR_TURBIDITY_LIMIT   (ADDR_DIRTY_WATER_BEEP + 1) // NEW
#define ADDR_TURBIDITY_BYPASS  (ADDR_TURBIDITY_LIMIT + 2) // NEW: Address for turbidity bypass
#define ADDR_TURBIDITY_TIMEOUT (ADDR_TURBIDITY_BYPASS + 1) // NEW: Address for turbidity timeout
// NEW: EEPROM addresses for calibration values
#define ADDR_CALIBRATED_CLEAN  (ADDR_TURBIDITY_TIMEOUT + 1)
#define ADDR_CALIBRATED_DIRTY  (ADDR_CALIBRATED_CLEAN + 2)
// NEW: EEPROM addresses for new passwords
#define ADDR_AP_PASSWORD       (ADDR_CALIBRATED_DIRTY + 2) // NOTE: This address is no longer used but kept for struct integrity
#define ADDR_HTTP_PASSWORD     (ADDR_AP_PASSWORD + 64)
#define ADDR_SYSTEM_PROFILE    (ADDR_HTTP_PASSWORD + 64) // NEW: Address for System Profile
// NEW: EEPROM addresses for OLED layout
#define ADDR_OLED_SHOW_TIME    (ADDR_SYSTEM_PROFILE + 1)
#define ADDR_OLED_SHOW_DATE    (ADDR_OLED_SHOW_TIME + 1)
#define ADDR_OLED_SHOW_DAY     (ADDR_OLED_SHOW_DATE + 1)
#define ADDR_OLED_SHOW_BATTERY (ADDR_OLED_SHOW_DAY + 1)
// NEW: Address for Water Sensing Threshold
#define ADDR_WATER_PRESENCE_THRESHOLD (ADDR_OLED_SHOW_BATTERY + 1)
#define ADDR_START_SENSOR_LEVEL (ADDR_WATER_PRESENCE_THRESHOLD + 2) // NEW
#define ADDR_END_SENSOR_LEVEL   (ADDR_START_SENSOR_LEVEL + 1)     // NEW
// NEW: Addresses for Water Sensing delays
#define ADDR_WATER_SENSING_START_DELAY (ADDR_END_SENSOR_LEVEL + 1)
#define ADDR_WATER_SENSING_STOP_DELAY  (ADDR_WATER_SENSING_START_DELAY + 2)
#define ADDR_WEB_UI_REFRESH_INTERVAL   (ADDR_WATER_SENSING_STOP_DELAY + 2) // NEW: Address for Web UI refresh interval
#define ADDR_AUTO_RESET_DRY_RUN        (ADDR_WEB_UI_REFRESH_INTERVAL + 2) // NEW
#define ADDR_AUTO_RESET_TURBIDITY      (ADDR_AUTO_RESET_DRY_RUN + 1) // NEW
#define ADDR_TRANSMITTER_TX_POWER      (ADDR_AUTO_RESET_TURBIDITY + 1) // NEW: Address for transmitter LoRa power
#define ADDR_TRANSMITTER_ACK_ATTEMPTS  (ADDR_TRANSMITTER_TX_POWER + 1) // NEW: Address for transmitter ACK attempts
#define ADDR_LORA_POWER_OPTIMIZATION   (ADDR_TRANSMITTER_ACK_ATTEMPTS + 1) // NEW
#define EEPROM_SIZE                    (ADDR_LORA_POWER_OPTIMIZATION + 1)

// === Menu State ===
// Define menu item types
enum MenuItemType {
    MENU_ITEM_SUBMENU,
    MENU_ITEM_SETTING,
    MENU_ITEM_ACTION // For actions like "Clear Schedules"
};

// Structure for a menu item
struct MenuItem {
    const char* label;
    MenuItemType type;
    uint8_t targetPage; // Only for MENU_ITEM_SUBMENU, indicates the page to go to
    uint8_t settingIndex; // For MENU_ITEM_SETTING, refers to an index within a settings group
};

// Define menu pages
enum MenuPage {
    PAGE_MAIN_MENU = 0,
    PAGE_SYSTEM_PROFILE,  // NEW: System Profile Page
    PAGE_PUMP_SETTINGS,
    PAGE_SAFETY_SENSORS,  // NEW: For safety and sensor settings
    PAGE_SCHEDULES_MENU,
    PAGE_SCHEDULE_EDIT,   // NEW: Page for editing a single schedule
    PAGE_RTC_SETTINGS,
    PAGE_DISPLAY_SETTINGS,
    PAGE_OLED_LAYOUT,     // NEW: For customizing OLED screen elements
    PAGE_BUZZER_SETTINGS, // NEW
    PAGE_WIFI_SETTINGS,   // NEW
    PAGE_LORA_SETTINGS,   // NEW: For LoRa specific settings
    PAGE_CALIBRATION,     // NEW: Calibration page
    PAGE_SYSTEM_INFO,
    PAGE_SECURITY,        // NEW: Security settings page
    NUM_MENU_PAGES // Keep track of total pages
};

// Menu definitions for each page
const MenuItem mainMenu[] = {
    {"System Profile", MENU_ITEM_SUBMENU, PAGE_SYSTEM_PROFILE, 0},   // NEW
    {"Pump Settings", MENU_ITEM_SUBMENU, PAGE_PUMP_SETTINGS, 0},
    {"Safety & Sensors", MENU_ITEM_SUBMENU, PAGE_SAFETY_SENSORS, 0}, // NEW
    {"Schedules", MENU_ITEM_SUBMENU, PAGE_SCHEDULES_MENU, 0},
    {"RTC Settings", MENU_ITEM_SUBMENU, PAGE_RTC_SETTINGS, 0},
    {"Display Settings", MENU_ITEM_SUBMENU, PAGE_DISPLAY_SETTINGS, 0},
    {"Buzzer Settings", MENU_ITEM_SUBMENU, PAGE_BUZZER_SETTINGS, 0}, // NEW
    {"WiFi Settings", MENU_ITEM_SUBMENU, PAGE_WIFI_SETTINGS, 0},     // NEW
    {"LoRa Settings", MENU_ITEM_SUBMENU, PAGE_LORA_SETTINGS, 0},     // NEW
    {"Calibration", MENU_ITEM_SUBMENU, PAGE_CALIBRATION, 0},         // NEW
    {"System Info", MENU_ITEM_SUBMENU, PAGE_SYSTEM_INFO, 0},
    {"Security", MENU_ITEM_SUBMENU, PAGE_SECURITY, 0},               // NEW
    {"Factory Reset", MENU_ITEM_ACTION, 0, 100},                     // NEW
    {"Exit", MENU_ITEM_ACTION, 0, 99} // New Exit option
};
const uint8_t MAIN_MENU_ITEMS = sizeof(mainMenu) / sizeof(mainMenu[0]);

// NEW: Menu for System Profile selection
const MenuItem systemProfileMenu[] = {
    {"Profile", MENU_ITEM_SETTING, 0, 0},
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t SYSTEM_PROFILE_MENU_ITEMS = sizeof(systemProfileMenu) / sizeof(systemProfileMenu[0]);


const MenuItem pumpSettingsMenu[] = {
    {"Pump Mode", MENU_ITEM_SETTING, 0, 0},
    {"Start Snsr Lvl", MENU_ITEM_SETTING, 0, 1},
    {"End Snsr Level", MENU_ITEM_SETTING, 0, 2},
    {"Run Time", MENU_ITEM_SETTING, 0, 3},
    {"Priming Time", MENU_ITEM_SETTING, 0, 4},
    {"ForcRnSchdl", MENU_ITEM_SETTING, 0, 5},
    {"Manual Safety", MENU_ITEM_SETTING, 0, 6},
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t PUMP_SETTINGS_MENU_ITEMS = sizeof(pumpSettingsMenu) / sizeof(pumpSettingsMenu[0]);

// NEW: Menu for Safety and Sensor settings
const MenuItem safetySensorsMenu[] = {
    {"Dry Run", MENU_ITEM_SETTING, 0, 0},
    {"Dry Run Delay", MENU_ITEM_SETTING, 0, 1},
    {"Retry Logic", MENU_ITEM_SETTING, 0, 2},
    {"Retry Attempts", MENU_ITEM_SETTING, 0, 3},
    {"Atmpt Intrval", MENU_ITEM_SETTING, 0, 4},
    {"Snsr Err Byps", MENU_ITEM_SETTING, 0, 5},
    {"Turbidity Byp", MENU_ITEM_SETTING, 0, 6},
    {"Turbidity T/O", MENU_ITEM_SETTING, 0, 7},
    {"Water Threshold", MENU_ITEM_SETTING, 0, 8},
    {"Water Start Dly", MENU_ITEM_SETTING, 0, 9},
    {"Water Stop Dly", MENU_ITEM_SETTING, 0, 10},
    {"Auto DR Reset", MENU_ITEM_SETTING, 0, 11}, // NEW
    {"Auto TB Reset", MENU_ITEM_SETTING, 0, 12}, // NEW
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t SAFETY_SENSORS_MENU_ITEMS = sizeof(safetySensorsMenu) / sizeof(safetySensorsMenu[0]);

// MODIFIED: This menu now navigates to a single unified edit page.
const MenuItem schedulesMenu[] = {
    {"Edit Schedule", MENU_ITEM_SUBMENU, PAGE_SCHEDULE_EDIT, 0},
    {"Clear Schedules", MENU_ITEM_ACTION, 0, 100}, // Use a unique index for the action
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t SCHEDULES_MENU_ITEMS = sizeof(schedulesMenu) / sizeof(schedulesMenu[0]);

// NEW: A dedicated menu page for editing a single schedule's details.
// MODIFIED: Added Schedule Selector and Date fields.
const MenuItem scheduleEditMenu[] = {
    {"Schedule #", MENU_ITEM_SETTING, 0, 10}, // Selector for which schedule to edit
    {"Hour", MENU_ITEM_SETTING, 0, 0},
    {"Minute", MENU_ITEM_SETTING, 0, 1},
    {"Repeat", MENU_ITEM_SETTING, 0, 2},
    {"Year", MENU_ITEM_SETTING, 0, 3},
    {"Month", MENU_ITEM_SETTING, 0, 4},
    {"Day", MENU_ITEM_SETTING, 0, 5},
    {"Clear This Sch.", MENU_ITEM_ACTION, 0, 101}, // NEW: Action to clear the current schedule
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t SCHEDULE_EDIT_MENU_ITEMS = sizeof(scheduleEditMenu) / sizeof(scheduleEditMenu[0]);


const MenuItem rtcSettingsMenu[] = {
    {"RTC Year", MENU_ITEM_SETTING, 0, 0},
    {"RTC Month", MENU_ITEM_SETTING, 0, 1},
    {"RTC Day", MENU_ITEM_SETTING, 0, 2},
    {"RTC Hour", MENU_ITEM_SETTING, 0, 3},
    {"RTC Minute", MENU_ITEM_SETTING, 0, 4},
    {"RTC Second", MENU_ITEM_SETTING, 0, 5},
    {"Back", MENU_ITEM_ACTION, 0, 98} // New Back option
};
const uint8_t RTC_SETTINGS_MENU_ITEMS = sizeof(rtcSettingsMenu) / sizeof(rtcSettingsMenu[0]);

const MenuItem displaySettingsMenu[] = {
    {"Display Font", MENU_ITEM_SETTING, 0, 0},
    {"OLED Theme", MENU_ITEM_SETTING, 0, 1}, // NEW: Theme setting for OLED
    {"OLED Layout", MENU_ITEM_SUBMENU, PAGE_OLED_LAYOUT, 0}, // NEW: Link to layout submenu
    {"Back", MENU_ITEM_ACTION, 0, 98} // New Back option
};
const uint8_t DISPLAY_SETTINGS_MENU_ITEMS = sizeof(displaySettingsMenu) / sizeof(displaySettingsMenu[0]);

// NEW: Menu for OLED Layout Settings
const MenuItem oledLayoutMenu[] = {
    {"Show Time", MENU_ITEM_SETTING, 0, 0},
    {"Show Date", MENU_ITEM_SETTING, 0, 1},
    {"Show Day", MENU_ITEM_SETTING, 0, 2},
    {"Show Battery", MENU_ITEM_SETTING, 0, 3},
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t OLED_LAYOUT_MENU_ITEMS = sizeof(oledLayoutMenu) / sizeof(oledLayoutMenu[0]);

// NEW: Buzzer Settings Menu with new items
const MenuItem buzzerSettingsMenu[] = {
    {"Buzzer ON/OFF", MENU_ITEM_SETTING, 0, 0},
    {"Buzzer Volume", MENU_ITEM_SETTING, 0, 8}, // NEW: Buzzer Volume control
    {"Dry Run", MENU_ITEM_SETTING, 0, 1},
    {"Sensor Error", MENU_ITEM_SETTING, 0, 2},
    {"Low Battery", MENU_ITEM_SETTING, 0, 3},
    {"Heartbeat", MENU_ITEM_SETTING, 0, 4},
    {"Tank Empty", MENU_ITEM_SETTING, 0, 5},
    {"Tank Full", MENU_ITEM_SETTING, 0, 6},
    {"Dirty Water", MENU_ITEM_SETTING, 0, 7}, // NEW
    {"Back", MENU_ITEM_ACTION, 0, 98} // New Back option
};
const uint8_t BUZZER_SETTINGS_MENU_ITEMS = sizeof(buzzerSettingsMenu) / sizeof(buzzerSettingsMenu[0]);

const MenuItem wifiSettingsMenu[] = {
    {"WiFi Mode", MENU_ITEM_SETTING, 0, 0},
    {"STA SSID", MENU_ITEM_SETTING, 0, 1},
    {"STA Pass", MENU_ITEM_SETTING, 0, 2},
    {"Back", MENU_ITEM_ACTION, 0, 98} // New Back option
};
const uint8_t WIFI_SETTINGS_MENU_ITEMS = sizeof(wifiSettingsMenu) / sizeof(wifiSettingsMenu[0]);

// NEW: LoRa Settings Menu
const MenuItem loraSettingsMenu[] = {
    {"Pwr Optimize", MENU_ITEM_SETTING, 0, 2},
    {"TX Power", MENU_ITEM_SETTING, 0, 0},
    {"ACK Attempts", MENU_ITEM_SETTING, 0, 1}, // NEW
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t LORA_SETTINGS_MENU_ITEMS = sizeof(loraSettingsMenu) / sizeof(loraSettingsMenu[0]);

// NEW: Calibration Menu (UPDATED)
const MenuItem calibrationMenu[] = {
    {"Set Clean Lvl", MENU_ITEM_ACTION, 0, 0},   // Action to start clean water calibration
    {"Set Dirty Lvl", MENU_ITEM_ACTION, 0, 1},   // Action to start dirty water calibration
    {"Manual Limit", MENU_ITEM_SETTING, 0, 2},  // Manually set the limit
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t CALIBRATION_MENU_ITEMS = sizeof(calibrationMenu) / sizeof(calibrationMenu[0]);

// NEW: Security Menu (display only on OLED, edit in WebUI)
const MenuItem securityMenu[] = {
    {"Edit In WebUI", MENU_ITEM_SETTING, 0, 0},
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t SECURITY_MENU_ITEMS = sizeof(securityMenu) / sizeof(securityMenu[0]);

// EXPANDED: System Info menu with more details
const MenuItem systemInfoMenu[] = {
    {"Version", MENU_ITEM_SETTING, 0, 0},
    {"Device ID", MENU_ITEM_SETTING, 0, 1},
    {"MAC Addr.", MENU_ITEM_SETTING, 0, 2},
    {"Chip Info", MENU_ITEM_SETTING, 0, 3},
    {"Free Heap", MENU_ITEM_SETTING, 0, 4},
    {"Uptime", MENU_ITEM_SETTING, 0, 5},
    {"Back", MENU_ITEM_ACTION, 0, 98}
};
const uint8_t SYSTEM_INFO_MENU_ITEMS = sizeof(systemInfoMenu) / sizeof(systemInfoMenu[0]);

// Array of pointers to menu arrays and their sizes
const MenuItem* menuPages[] = {
    mainMenu,
    systemProfileMenu,    // NEW
    pumpSettingsMenu,
    safetySensorsMenu,    // NEW
    schedulesMenu,
    scheduleEditMenu,   // NEW
    rtcSettingsMenu,
    displaySettingsMenu,
    oledLayoutMenu,       // NEW
    buzzerSettingsMenu,
    wifiSettingsMenu,
    loraSettingsMenu,     // NEW
    calibrationMenu,
    systemInfoMenu,
    securityMenu
};

const uint8_t menuPageSizes[] = {
    MAIN_MENU_ITEMS,
    SYSTEM_PROFILE_MENU_ITEMS, // NEW
    PUMP_SETTINGS_MENU_ITEMS,
    SAFETY_SENSORS_MENU_ITEMS, // NEW
    SCHEDULES_MENU_ITEMS,
    SCHEDULE_EDIT_MENU_ITEMS, // NEW
    RTC_SETTINGS_MENU_ITEMS,
    DISPLAY_SETTINGS_MENU_ITEMS,
    OLED_LAYOUT_MENU_ITEMS,    // NEW
    BUZZER_SETTINGS_MENU_ITEMS,
    WIFI_SETTINGS_MENU_ITEMS,
    LORA_SETTINGS_MENU_ITEMS,      // NEW
    CALIBRATION_MENU_ITEMS,
    SYSTEM_INFO_MENU_ITEMS,
    SECURITY_MENU_ITEMS
};

// NEW: Struct to hold both page and index for menu history
struct MenuHistoryItem {
    MenuPage page;
    uint8_t index;
};

MenuPage currentMenuPage = PAGE_MAIN_MENU; // Start on the main menu
uint8_t menuIndex = 0; // Index within the current page

// Stack for menu history to allow "back" navigation
#define MENU_HISTORY_DEPTH 5
MenuHistoryItem menuHistory[MENU_HISTORY_DEPTH]; // MODIFIED: Use the new struct
uint8_t menuHistoryPointer = 0;

// New: Flag to indicate if a setting is currently being edited
bool editingSetting = false;
uint8_t editingScheduleIndex = 0; // NEW: To track which schedule is being edited in the new sub-menu

// NEW: LoRa Settings
uint8_t transmitterTxPower = 17; // LoRa TX Power for the remote transmitter unit (2-20)
uint8_t transmitterAckAttempts = 3; // NEW: LoRa ACK attempts for the remote transmitter (1-10)

// NEW: Variables to store the confirmed settings from the transmitter
uint8_t confirmedTxPower = 0;
uint8_t confirmedAckAttempts = 0;

// =================================================================
// === HELPER FUNCTION DEFINITIONS (Moved to appear before setup() and loop()) ===
// =================================================================

// NEW: Function prototype for sendAcknowledgment
void sendAcknowledgment(uint16_t senderID);

// NEW: Function prototype for setupOTA
void setupOTA();

// NEW: Function to continuously sample turbidity sensor regardless of pump state.
// This keeps the global 'turbidityValue' updated for all other functions to use.
void updateLiveTurbidityValue() {
    // This is a simplified version of the sampling logic inside readTurbiditySensor, made global.
    // Non-blocking sampling logic, runs all the time
    if ((uint32_t)(esp_timer_get_time() / 1000) - lastTurbiditySampleTime >= TURBIDITY_SAMPLE_INTERVAL_MS) {
        lastTurbiditySampleTime = (uint32_t)(esp_timer_get_time() / 1000);
        turbiditySampleSum += adc1_get_raw(TURBIDITY_SENSOR_PIN);
        turbiditySampleCount++;

        // Take an average over 50 samples (~250ms) for a stable reading
        if (turbiditySampleCount >= 50) { 
            turbidityValue = turbiditySampleSum / turbiditySampleCount;
            
            // Reset for next average calculation
            turbiditySampleSum = 0;
            turbiditySampleCount = 0;
        }
    }
}


// NEW: Function to check if a schedule is set for a past date/time
bool isScheduleValid(const Schedule& s) {
    // This validation applies only to one-time schedules that depend on a specific date.
    if (s.repeatType != NO_REPEAT && s.repeatType != SPECIFIC_DATE) {
        return true; // Repeating schedules (Daily, Odd, Even) are always considered valid as they will trigger on a future day.
    }

    if (!isRtcWorking) {
        return true; // Cannot validate without a working RTC, so we allow it.
    }

    DateTime now = rtc.now();
    
    // Construct a DateTime object for the schedule. Seconds are ignored (set to 0).
    // For NO_REPEAT, the date part is assumed to be today.
    uint16_t schedYear = (s.repeatType == SPECIFIC_DATE) ? s.year : now.year();
    uint8_t schedMonth = (s.repeatType == SPECIFIC_DATE) ? s.month : now.month();
    uint8_t schedDay = (s.repeatType == SPECIFIC_DATE) ? s.day : now.day();

    DateTime scheduleTime(schedYear, schedMonth, schedDay, s.hour, s.minute, 0);

    // To check if a schedule is in the past, we compare it to the current time,
    // also with seconds set to 0. This means a schedule for 10:30 is valid
    // as long as the current time is still within the 10:30 minute.
    DateTime nowAtStartOfMinute(now.year(), now.month(), now.day(), now.hour(), now.minute(), 0);

    return scheduleTime.unixtime() >= nowAtStartOfMinute.unixtime();
}


// NEW: Function to find the next chronologically upcoming schedule
bool findNextSchedule(Schedule& nextSched) {
    uint32_t soonestTime = 0xFFFFFFFF; // Represents the largest possible timestamp
    bool found = false;
    if (!isRtcWorking) return false;

    DateTime now = rtc.now();
    uint32_t now_ts = now.unixtime();

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        // Skip only "blank" schedules (all zeros)
        if (schedules[i].hour == 0 && schedules[i].minute == 0 && schedules[i].year == 0) {
            continue;
        }

        uint32_t next_ts = 0;

        switch (schedules[i].repeatType) {
            case SPECIFIC_DATE:
            case NO_REPEAT: {
                uint16_t schedYear = (schedules[i].repeatType == SPECIFIC_DATE) ? schedules[i].year : now.year();
                uint8_t schedMonth = (schedules[i].repeatType == SPECIFIC_DATE) ? schedules[i].month : now.month();
                uint8_t schedDay = (schedules[i].repeatType == SPECIFIC_DATE) ? schedules[i].day : now.day();
                DateTime schedTime(schedYear, schedMonth, schedDay, schedules[i].hour, schedules[i].minute, 0);
                
                // Only consider it if it's in the future
                if (schedTime.unixtime() > now_ts) {
                    next_ts = schedTime.unixtime();
                }
                break;
            }
            case EVERY_DAY: {
                DateTime todaySched(now.year(), now.month(), now.day(), schedules[i].hour, schedules[i].minute, 0);
                if (todaySched.unixtime() > now_ts) {
                    // It's scheduled for later today
                    next_ts = todaySched.unixtime();
                } else { 
                    // It has already passed today, so schedule it for tomorrow
                    DateTime tomorrow = now + TimeSpan(1, 0, 0, 0);
                    DateTime tomorrowSched(tomorrow.year(), tomorrow.month(), tomorrow.day(), schedules[i].hour, schedules[i].minute, 0);
                    next_ts = tomorrowSched.unixtime();
                }
                break;
            }
            case ODD_DAY:
            case EVEN_DAY: {
                bool isSchedOdd = (schedules[i].repeatType == ODD_DAY);
                
                // First, check if it could trigger today
                DateTime todaySched(now.year(), now.month(), now.day(), schedules[i].hour, schedules[i].minute, 0);
                bool isTodayOdd = (now.day() % 2 != 0);
                
                if (isTodayOdd == isSchedOdd && todaySched.unixtime() > now_ts) {
                    // It's the right day type and it's later today
                    next_ts = todaySched.unixtime();
                } else {
                    // It's either the wrong day type, or the time has already passed today.
                    // We must find the *next* valid day.
                    for (int d = 1; d <= 2; ++d) { // Check tomorrow (d=1) and the day after (d=2)
                        DateTime futureDay = now + TimeSpan(d, 0, 0, 0);
                        DateTime futureSched(futureDay.year(), futureDay.month(), futureDay.day(), schedules[i].hour, schedules[i].minute, 0);
                        bool isFutureDayOdd = (futureDay.day() % 2 != 0);

                        if (isFutureDayOdd == isSchedOdd) {
                            // This is the next valid day for this schedule
                            next_ts = futureSched.unixtime();
                            break; // Found the soonest valid day
                        }
                    }
                }
                break;
            }
        }
        
        // Check if this schedule's next time is the soonest we've found so far
        if (next_ts > 0 && next_ts < soonestTime) {
            soonestTime = next_ts;
            nextSched = schedules[i];
            found = true;
        }
    }
    
    return found;
}


// NEW: Background task function for non-blocking NTP synchronization
void ntpSyncTask(void *pvParameters) {
  ESP_LOGI(TAG, "%s
", String("NTP Sync Task started and pinned to Core 0.");
  
  // Give the system a moment to stabilize after boot
  vTaskDelay(5000 / portTICK_PERIOD_MS);

  timeClient.begin();
  ESP_LOGI(TAG, "%s
", String("NTP Client started in background task.");

  for (;;) { // Infinite loop for the task
    // This task should only run when in STA mode and connected to WiFi
    if (wifiMode == WIFI_STA_MODE && WiFi.status() == WL_CONNECTED) {
      
      ESP_LOGI(TAG, "%s
", String("NTP Task: Attempting NTP time synchronization...");

      // Set a flag for the main loop to display "Syncing..." message on the OLED
      isNtpSyncingMessageActive = true;
      ntpMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);

      // This is the potentially blocking call. It's now safely in its own task.
      // We'll use forceUpdate() to ensure it actively tries to sync.
      bool success = timeClient.forceUpdate();

      // The sync attempt is over, so the "Syncing..." message can be cleared.
      isNtpSyncingMessageActive = false;

      if (success) {
        unsigned long epochTime = timeClient.getEpochTime();
        
        // Only adjust the RTC if the time difference is significant (e.g., > 10 seconds)
        // This prevents small, frequent adjustments and unnecessary writes.
        if (isRtcWorking && abs(long(rtc.now().unixtime() - epochTime)) > 10) {
          rtc.adjust(DateTime(epochTime));
          ESP_LOGI(TAG, "NTP Task: RTC time synchronized. New time: %s\n", timeClient.getFormattedTime().c_str());
        } else if (isRtcWorking) {
          ESP_LOGI(TAG, "%s
", String("NTP Task: RTC time is already accurate. No sync adjustment needed.");
        }
        
        // Set flags for the main loop to display the "Synced!" message and the NTP icon.
        isNtpSyncedMessageActive = true;
        ntpMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
        ntpSyncCompleted = true;

      } else {
        ESP_LOGI(TAG, "%s
", String("NTP Task: NTP update failed. Will retry later.");
      }
      
      // After an attempt (successful or not), sleep for the long interval.
      ESP_LOGI(TAG, "NTP Task: Sleeping for %lu ms.\n", NTP_SYNC_INTERVAL_MS);
      vTaskDelay(NTP_SYNC_INTERVAL_MS / portTICK_PERIOD_MS);

    } else {
      // If not connected, just wait for a shorter period before checking the connection status again.
      vTaskDelay(10000 / portTICK_PERIOD_MS); // Check for connection every 10 seconds
    }
  }
}


// REFACTORED: This function is now ONLY for "Dirty Water" detection when the pump is running.
// It uses the globally updated 'turbidityValue'.
void readTurbiditySensor() {
    // Only care about dirty water when the pump is actually running.
    if (!pumpRunning) {
        // Reset ONLY the transient detection flag when pump is off.
        // The persistent isDirtyWaterError flag must remain until manually reset.
        isWaterDirty = false;
        dirtyWaterDetectedTime = 0;
        // turbiditySampleSum = 0; // No longer needed here as it's handled by updateLiveTurbidityValue
        // turbiditySampleCount = 0;
        
        // --- BUG FIX ---
        // The block below was incorrectly clearing the persistent turbidity error.
        // The error state must be maintained even when the pump is off,
        // until it is manually reset by the user.
        /*
        if (sensorError == SENSOR_ERROR_DIRTY_WATER) {
            sensorError = SENSOR_ERROR_NONE;
            isDirtyWaterError = false;
        }
        */
        // --- END BUG FIX ---
        
        return;
    }
    
    // If bypass is enabled, do nothing and reset dirty water flags.
    if (turbidityBypassEnabled) {
        isWaterDirty = false;
        if (sensorError == SENSOR_ERROR_DIRTY_WATER) {
            sensorError = SENSOR_ERROR_NONE;
            isDirtyWaterError = false;
        }
        return;
    }

    // --- Original logic now using the globally updated 'turbidityValue' ---
    if (turbidityValue > turbidityLimit) {
        if (!isWaterDirty) {
            // First time we've detected dirty water in this pump cycle. Start the timer.
            isWaterDirty = true;
            dirtyWaterDetectedTime = (uint32_t)(esp_timer_get_time() / 1000);
            ESP_LOGI(TAG, "High turbidity detected (%d > %d). Starting timeout.\n", turbidityValue, turbidityLimit);
        } else {
            // Water is already known to be dirty, check if the timeout has expired.
            if ((uint32_t)(esp_timer_get_time() / 1000) - dirtyWaterDetectedTime > (turbidityTimeoutSec * 1000UL)) {
                // Timeout expired. This check ensures we only process the error once per event.
                if (!isDirtyWaterError && !retryInProgress) {
                    stopPump(); // Stop the pump immediately.
                    ESP_LOGI(TAG, "%s
", String("Dirty water timeout detected.");

                    // Now, decide whether to retry or set a permanent error.
                    if (retryLogicEnabled) {
                        if (remainingDryRunAttempts > 0) {
                            retryInProgress = true; // Start the retry process.
                            retryStartTime = (uint32_t)(esp_timer_get_time() / 1000); // Record when the retry interval begins.
                            remainingDryRunAttempts--; // Use one attempt.
                            ESP_LOGI(TAG, "Retrying due to turbidity... Attempts left: %d\n", remainingDryRunAttempts);
                            // Trigger the alert beep for the first attempt.
                            if (buzzerEnabled && !isDirtyWaterBeepActive) {
                                beepForDirtyWater();
                            }
                        } else {
                            // All retry attempts have been exhausted. Set the final, persistent error state.
                            isDirtyWaterError = true;
                            sensorError = SENSOR_ERROR_DIRTY_WATER;
                            retryInProgress = false; // No longer retrying.
                            snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "Turbidity: %d", turbidityValue);
                            ESP_LOGI(TAG, "%s
", String("Turbidity error: All retry attempts exhausted.");
                            if (buzzerEnabled && !isDirtyWaterBeepActive) {
                                beepForDirtyWater();
                            }
                        }
                    } else {
                        // If retry logic is disabled, it's an immediate, persistent error.
                        isDirtyWaterError = true;
                        sensorError = SENSOR_ERROR_DIRTY_WATER;
                        snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "Turbidity: %d", turbidityValue);
                        ESP_LOGI(TAG, "%s
", String("Turbidity error: Retry logic is disabled.");
                        if (buzzerEnabled && !isDirtyWaterBeepActive) {
                            beepForDirtyWater();
                        }
                    }
                }
            }
        }
    } else {
        // If turbidity level goes back to normal, reset the dirty water detection flag for the current cycle.
        isWaterDirty = false;
    }
}


// NEW: Function to handle the Water Sensing pump mode logic.
void handleWaterSensingMode() {
    // This logic only runs in WATER_SENSING mode.
    if (pumpMode != WATER_SENSING) {
        // Always reset timers if not in this mode to prevent stale states
        if (waterPresentStartTime != 0) waterPresentStartTime = 0;
        if (waterAbsentStartTime != 0) waterAbsentStartTime = 0;
        return;
    }
    
    // Determine water presence based on the threshold
    bool waterIsPresent = (turbidityValue > waterPresenceThreshold);
    
    // --- PUMP START LOGIC ---
    // Conditions to START: Pump must be off, NO retry in progress, no blocking errors (except turbidity error, which this mode can reset),
    // AND tank level must be at or below the start level.
    // MODIFIED: Added !retryInProgress check
    if (!pumpRunning && !waitingForPumpRelay && !dryRunError && !retryInProgress && 
        (sensorError == SENSOR_ERROR_NONE || sensorError == SENSOR_ERROR_DIRTY_WATER || sensorErrorBypassEnabled) && 
        currentTankLevel <= startSensorLevel) {
        
        if (waterIsPresent) {
            // Water is present, start the timer to confirm it's not a temporary spike.
            if (waterPresentStartTime == 0) {
                waterPresentStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                ESP_LOGI(TAG, "Water detected! Turbidity (%d) > threshold (%d). Starting %ds start timer.\n", turbidityValue, waterPresenceThreshold, waterSensingStartDelaySec);
            } else if ((uint32_t)(esp_timer_get_time() / 1000) - waterPresentStartTime >= (waterSensingStartDelaySec * 1000UL)) {
                // Timer expired, it's safe to start the pump.
                ESP_LOGI(TAG, "%ds start delay passed. Initiating pump start due to water presence.\n", waterSensingStartDelaySec);

                // --- NEW FIX (Combines Problem 1 & 2) ---
                // This new water detection event clears any previous *dry run* errors and resets attempts for the new cycle.
                // Turbidity errors are handled *only* by the handleAutoTurbidityReset() function.
                if (dryRunError) {
                    dryRunError = false;
                    isDryRunErrorBeepActive = false;
                    dryRunResetMessageActive = true; // Show dry run reset message
                    dryRunResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                    ESP_LOGI(TAG, "%s
", String("Water Sensing start is due. Automatically resetting DRY RUN error.");
                }
                
                // The auto-reset for turbidity error is now *exclusively* handled by handleAutoTurbidityReset(),
                // which waits for water to disappear and then reappear. This block no longer interferes.
                
                // ALWAYS reset attempts for a new water-sensing run.
                remainingDryRunAttempts = dryRunAttempts; // Restore all retry attempts.
                // --- END NEW FIX ---

                initiatePumpStart(false);
                waterPresentStartTime = 0; // Reset timer after use.
            }
        } else {
            // Water is not present, so reset any pending start timer.
            if (waterPresentStartTime != 0) waterPresentStartTime = 0;
        }
    } else if (retryInProgress) {
        // NEW: If a retry *is* in progress, ensure any water sensing start timer is reset.
        // This prevents the water sensing timer from potentially interfering later.
        if (waterPresentStartTime != 0) waterPresentStartTime = 0;
    }

    // --- PUMP STOP LOGIC (specifically for this mode) ---
    // Condition to STOP: Pump must be running and water must have disappeared.
    // Other stop conditions (tank full, dirty water error) are handled by other functions.
    if (pumpRunning) { // This logic should only apply when the pump is actually on
        if (!waterIsPresent) {
            // Water is gone, start the timer to confirm it's not a temporary dip.
            if (waterAbsentStartTime == 0) {
                waterAbsentStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                ESP_LOGI(TAG, "Water NOT detected! Turbidity (%d) <= threshold (%d). Starting %ds stop timer.\n", turbidityValue, waterPresenceThreshold, waterSensingStopDelaySec);
            } else if ((uint32_t)(esp_timer_get_time() / 1000) - waterAbsentStartTime >= (waterSensingStopDelaySec * 1000UL)) {
                // Timer expired, it's safe to stop the pump.
                ESP_LOGI(TAG, "%ds stop delay passed. Stopping pump due to water absence.\n", waterSensingStopDelaySec);
                stopPump();
                // Reset dry run attempts if we had flow, as this is a normal stop condition.
                if (currentPumpRunHadFlow) {
                    remainingDryRunAttempts = dryRunAttempts;
                }
                waterAbsentStartTime = 0; // Reset timer after use.
            }
        } else {
            // Water is present, so reset any pending stop timer.
            if (waterAbsentStartTime != 0) waterAbsentStartTime = 0;
        }
    }
}

// NEW: Function prototype for handling calibration process
void processCalibration();

// NEW: Function prototypes for starting calibration steps
void startCleanWaterCalibration();
void startDirtyWaterCalibration();

// NEW: Function prototype for updating the theme from the web UI
void handleUpdateThemeWrapped();

// NEW: Function prototypes for missing web handlers
void handleGetLiveDataWrapped();
void handleResetDryRunWrapped();
void handleApSetupSaveWrapped();

// NEW: Function prototypes for WiFi string conversion
const char* getWifiModeString(WifiMode mode);
void connectToWiFiSTA();

// NEW: Function prototype for handling start/end sensor logic
void handleStartEndSensorLogic();

// NEW: Function prototype for auto-populating schedules
void autoPopulateBlankSchedule(uint8_t scheduleIndex);

// NEW: Function prototype for auto dry run reset
void handleAutoDryRunReset();

// NEW: Function prototype for auto turbidity reset
void handleAutoTurbidityReset();

void pushMenuPage(MenuPage page) {
    if (menuHistoryPointer < MENU_HISTORY_DEPTH) {
        menuHistory[menuHistoryPointer].page = currentMenuPage;
        menuHistory[menuHistoryPointer].index = menuIndex;
        menuHistoryPointer++;
    }
    currentMenuPage = page;
    menuIndex = 0; // Reset index when changing page
    editingSetting = false; // Always exit editing mode when entering a new menu page
}

void popMenuPage() {
    if (menuHistoryPointer > 0) {
        if (currentMenuPage == PAGE_CALIBRATION) { // NEW: Reset calibration state when leaving page
            currentCalibrationState = CAL_IDLE;
        }
        // NEW (FIX): Reset the schedule editing index when leaving the schedule edit page.
        // This prevents a stale index from being used if you re-enter the menu.
        if (currentMenuPage == PAGE_SCHEDULE_EDIT) {
            editingScheduleIndex = 0;
        }
        menuHistoryPointer--; // Decrement first
        currentMenuPage = menuHistory[menuHistoryPointer].page;
        menuIndex = menuHistory[menuHistoryPointer].index; // Restore the saved index
    } else {
        // If no history, exit settings completely
        inSettingsMenu = false;
    }
    editingSetting = false; // Always exit editing mode when changing menu page or exiting
}

// Debounce function (updated to manage new ButtonState fields)
bool debounceButton(uint8_t pin, ButtonState& btn) {
    bool isNewlyPressed = false;
    bool currentState = gpio_get_level(pin);

    // Check for a state change and update the timer
    if (currentState != btn.lastReadState) {
        btn.lastChangeTime = (uint32_t)(esp_timer_get_time() / 1000);
        btn.lastReadState = currentState;
    }

    // After the debounce interval, update the stable state if it has changed
    if (((uint32_t)(esp_timer_get_time() / 1000) - btn.lastChangeTime) > DEBOUNCE_INTERVAL_MS) {
        if (btn.lastStableState != currentState) {
            btn.lastStableState = currentState;
            // If the new stable state is pressed (0), it's a new press event
            if (btn.lastStableState == 0) {
                isNewlyPressed = true;
                btn.pressStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            }
        }
    }

    // The 'isPressed' flag should always reflect the current debounced state
    btn.isPressed = (btn.lastStableState == 0);

    // Return true only on the initial press event
    return isNewlyPressed;
}

// === Buzzer Control Helper Functions (UPDATED) ===
// Function to play a specified beep style
void playBeepStyle(BeepStyle style) {
    if (!buzzerEnabled) return;

    switch (style) {
        case BEEP_STYLE_SILENT:
            stopBuzzer();
            break;
        case BEEP_STYLE_ALERT:
            startBuzzerSequence(3, 100, 100); // Three rapid beeps
            break;
        case BEEP_STYLE_WARNING:
            startBuzzerSequence(3, 500, 500); // Three slow beeps
            break;
        case BEEP_STYLE_PULSE:
            // MODIFIED: Capped the pulse duration to be around 10 seconds.
            startBuzzerSequence(4, 500, 2000); // 4 beeps over ~10 seconds
            break;
        case BEEP_STYLE_LONG:
            // MODIFIED: Changed the continuous beep duration to 10 seconds.
            startBuzzerContinuous(10000); // Continuous beep for 10 seconds
            break;
        case BEEP_STYLE_SPARROW:
            startBuzzerSequence(50, 50, 100); // Rapid, chirping sound
            break;
    }
}

// Function to start a continuous beep for a duration
void startBuzzerContinuous(unsigned long duration_ms) {
    if (!buzzerEnabled) return;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, buzzerVolume);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
    buzzerActiveUntil = (uint32_t)(esp_timer_get_time() / 1000) + duration_ms;
    currentBuzzerState = BUZZER_CONTINUOUS;
    isBuzzerCurrentlyOn = true;
    ESP_LOGI(TAG, "Continuous buzzer started for %lu ms.\n", duration_ms);
}

// Function to start a sequence of beeps
void startBuzzerSequence(uint8_t count, unsigned long on_duration_ms, unsigned long off_duration_ms) {
    if (!buzzerEnabled) return;
    // Don't override a more important continuous beep
    if (currentBuzzerState == BUZZER_CONTINUOUS) return;
    currentBeepCount = count * 2;
    beepOnDuration = on_duration_ms;
    beepOffDuration = off_duration_ms;
    buzzerToggleTime = (uint32_t)(esp_timer_get_time() / 1000);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, buzzerVolume);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
    isBuzzerCurrentlyOn = true;
    currentBuzzerState = BUZZER_SEQUENCE;
    ESP_LOGI(TAG, "Buzzer sequence started: %d beeps, %lu on, %lu off.\n", count, on_duration_ms, off_duration_ms);
}

// Function to start a very brief beep (for button presses)
void startBriefBeep() {
    if (!buzzerEnabled) return;
    // Don't override a continuous or sequence beep with a brief beep
    if (currentBuzzerState == BUZZER_CONTINUOUS || currentBuzzerState == BUZZER_SEQUENCE) return;
    if (currentBuzzerState == BUZZER_BRIEF && (uint32_t)(esp_timer_get_time() / 1000) < buzzerActiveUntil) return; // Already beeping briefly

    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, buzzerVolume);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
    buzzerActiveUntil = (uint32_t)(esp_timer_get_time() / 1000) + 50; // 50ms brief beep
    currentBuzzerState = BUZZER_BRIEF;
    isBuzzerCurrentlyOn = true;
    ESP_LOGI(TAG, "%s
", String("Brief beep started.");
}

// Function to stop the buzzer immediately
void stopBuzzer() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
    currentBuzzerState = BUZZER_OFF;
    buzzerActiveUntil = 0;
    buzzerToggleTime = 0;
    currentBeepCount = 0;
    isBuzzerCurrentlyOn = false;
    ESP_LOGI(TAG, "%s
", String("Buzzer stopped.");
}

// === NEW: Event-specific beep functions (UPDATED to use configurable styles) ===
void beepForDryRunError() {
    playBeepStyle(dryRunBeepStyle);
    isDryRunErrorBeepActive = true;
}

void beepForSensorError() {
    playBeepStyle(sensorErrorBeepStyle);
    isSensorErrorBeepActive = true;
}

void beepForLowBattery() {
    unsigned long currentTime = (uint32_t)(esp_timer_get_time() / 1000);
    // Check if at least an hour (3,600,000 ms) has passed since the last low battery beep
    if (lastLowBatteryBeepTime == 0 || currentTime - lastLowBatteryBeepTime > 3600000UL) {
        playBeepStyle(lowBatteryBeepStyle);
        lastLowBatteryBeepTime = currentTime; // Update the timestamp
        isLowBatteryBeepActive = true;
    }
}

void beepForHeartbeatExpired() {
    playBeepStyle(heartbeatExpiredBeepStyle);
    isHeartbeatExpiredBeepActive = true;
}

void beepForTankEmpty() {
    playBeepStyle(tankEmptyBeepStyle);
    isTankEmptyBeepActive = true;
}

void beepForTankFull() {
    playBeepStyle(tankFullBeepStyle);
    isTankFullBeepActive = true;
}

// NEW: Buzzer function for dirty water
void beepForDirtyWater() {
    playBeepStyle(dirtyWaterBeepStyle);
    isDirtyWaterBeepActive = true;
}

// Function to update buzzer state (called in loop)
void updateBuzzer() {
    if (!buzzerEnabled) {
        if (isBuzzerCurrentlyOn) stopBuzzer();
        return;
    }

    switch(currentBuzzerState) {
        case BUZZER_OFF:
            // Do nothing
            break;

        case BUZZER_BRIEF:
            if ((uint32_t)(esp_timer_get_time() / 1000) >= buzzerActiveUntil) {
                stopBuzzer();
            }
            break;

        case BUZZER_CONTINUOUS:
            if ((uint32_t)(esp_timer_get_time() / 1000) >= buzzerActiveUntil) {
                stopBuzzer();
            }
            break;

        case BUZZER_SEQUENCE:
            if (currentBeepCount > 0) {
                if (isBuzzerCurrentlyOn) {
                    if ((uint32_t)(esp_timer_get_time() / 1000) - buzzerToggleTime >= beepOnDuration) {
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, 0);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
                        isBuzzerCurrentlyOn = false;
                        buzzerToggleTime = (uint32_t)(esp_timer_get_time() / 1000);
                        currentBeepCount--;
                    }
                } else { // Buzzer is currently OFF, waiting for next ON cycle
                    if ((uint32_t)(esp_timer_get_time() / 1000) - buzzerToggleTime >= beepOffDuration) {
                        ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, buzzerVolume);
                        ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
                        isBuzzerCurrentlyOn = true;
                        buzzerToggleTime = (uint32_t)(esp_timer_get_time() / 1000);
                        currentBeepCount--;
                    }
                }
            } else {
                stopBuzzer(); // Sequence finished
            }
            break;
    }
}


// Function to save current settings to EEPROM
void saveSettingsToEEPROM() {
    EEPROM.write(ADDR_PUMP_MODE, (uint8_t)pumpMode);
    EEPROM.write(ADDR_DRY_DELAY, dryRunDelayMin);
    EEPROM.write(ADDR_DRY_ATTEMPTS, dryRunAttempts);
    EEPROM.write(ADDR_RETRY_INTERVAL, retryIntervalMin);
    EEPROM.write(ADDR_SCHED_DURATION, scheduleDurationMin);

    // NEW: Save priming time (as two bytes)
    EEPROM.write(ADDR_PRIMING_TIME, primingTimeMs >> 8); // High byte
    EEPROM.write(ADDR_PRIMING_TIME + 1, primingTimeMs & 0xFF); // Low byte
    
    // NEW: Save turbidity limit (as two bytes)
    EEPROM.write(ADDR_TURBIDITY_LIMIT, turbidityLimit >> 8); // High byte
    EEPROM.write(ADDR_TURBIDITY_LIMIT + 1, turbidityLimit & 0xFF); // Low byte

    for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 0, schedules[i].hour);
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 1, schedules[i].minute);
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 2, (uint8_t)schedules[i].repeatType);
      // NEW: Save date fields
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 3, schedules[i].year >> 8);
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 4, schedules[i].year & 0xFF);
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 5, schedules[i].month);
      EEPROM.write(ADDR_SCHEDULE_START + i*7 + 6, schedules[i].day);
    }
    EEPROM.write(ADDR_FORCE_SCHEDULED_RUN, forceScheduledRun ? 1 : 0); // Use new calculated address
    EEPROM.write(ADDR_SENSOR_ERROR_BYPASS, sensorErrorBypassEnabled ? 1 : 0); // Save new bypass setting
    EEPROM.write(ADDR_DISPLAY_FONT_INDEX, currentFontIndex); // Save new font index
    EEPROM.write(ADDR_BUZZER_ENABLED, buzzerEnabled ? 1 : 0); // NEW: Save buzzer enabled state
    EEPROM.write(ADDR_MANUAL_MODE_SAFETY_LOGIC, manualModeSafetyLogicEnabled ? 1 : 0); // NEW: Save manual mode safety logic state
    // FIX: This block was redundant and has been removed. The value was already written above.
    // EEPROM.write(ADDR_PRIMING_TIME, (primingTimeMs >> 8) & 0xFF); // Save high byte
    // EEPROM.write(ADDR_PRIMING_TIME + 1, primingTimeMs & 0xFF); // Save low byte
    EEPROM.write(ADDR_WIFI_MODE, (uint8_t)wifiMode);
    EEPROM.put(ADDR_STA_SSID, staSsid);
    EEPROM.put(ADDR_STA_PASSWORD, staPassword);

    EEPROM.write(ADDR_OLED_THEME, (uint8_t)currentOledTheme); // NEW: Save OLED theme
    EEPROM.write(ADDR_WEB_THEME, currentWebTheme);           // NEW: Save Web UI theme
    EEPROM.write(ADDR_DRY_RUN_LOGIC_ENABLED, dryRunLogicEnabled ? 1 : 0); // NEW: Save dry run logic enabled flag
    EEPROM.write(ADDR_RETRY_LOGIC_ENABLED, retryLogicEnabled ? 1 : 0); // NEW: Save retry logic enabled flag
    EEPROM.write(ADDR_BUZZER_VOLUME, buzzerVolume); // NEW: Save buzzer volume
    
    // NEW: Save buzzer style settings
    EEPROM.write(ADDR_DRY_RUN_BEEP, (uint8_t)dryRunBeepStyle);
    EEPROM.write(ADDR_SENSOR_BEEP, (uint8_t)sensorErrorBeepStyle);
    EEPROM.write(ADDR_LOW_BATTERY_BEEP, (uint8_t)lowBatteryBeepStyle);
    EEPROM.write(ADDR_HEARTBEAT_BEEP, (uint8_t)heartbeatExpiredBeepStyle);
    EEPROM.write(ADDR_TANK_EMPTY_BEEP, (uint8_t)tankEmptyBeepStyle);
    EEPROM.write(ADDR_TANK_FULL_BEEP, (uint8_t)tankFullBeepStyle);
    EEPROM.write(ADDR_DIRTY_WATER_BEEP, (uint8_t)dirtyWaterBeepStyle); // NEW
    EEPROM.write(ADDR_TURBIDITY_BYPASS, turbidityBypassEnabled ? 1 : 0); // NEW: Save turbidity bypass
    EEPROM.write(ADDR_TURBIDITY_TIMEOUT, turbidityTimeoutSec); // NEW: Save turbidity timeout
    
    // NEW: Save calibration values
    EEPROM.write(ADDR_CALIBRATED_CLEAN, calibratedCleanValue >> 8);
    EEPROM.write(ADDR_CALIBRATED_CLEAN + 1, calibratedCleanValue & 0xFF);
    EEPROM.write(ADDR_CALIBRATED_DIRTY, calibratedDirtyValue >> 8);
    EEPROM.write(ADDR_CALIBRATED_DIRTY + 1, calibratedDirtyValue & 0xFF);

    // NEW: Save passwords
    EEPROM.put(ADDR_HTTP_PASSWORD, http_password); // MODIFIED: Added missing line to save HTTP password

    // NEW: Save System Profile
    EEPROM.write(ADDR_SYSTEM_PROFILE, (uint8_t)currentProfile);

    // NEW: Save OLED Layout settings
    EEPROM.write(ADDR_OLED_SHOW_TIME, oledShowTime ? 1 : 0);
    EEPROM.write(ADDR_OLED_SHOW_DATE, oledShowDate ? 1 : 0);
    EEPROM.write(ADDR_OLED_SHOW_DAY, oledShowDay ? 1 : 0);
    EEPROM.write(ADDR_OLED_SHOW_BATTERY, oledShowBattery ? 1 : 0);

    // NEW: Save water presence threshold (as two bytes)
    EEPROM.write(ADDR_WATER_PRESENCE_THRESHOLD, waterPresenceThreshold >> 8); // High byte
    EEPROM.write(ADDR_WATER_PRESENCE_THRESHOLD + 1, waterPresenceThreshold & 0xFF); // Low byte

    // NEW: Save Start/End Sensor Levels
    EEPROM.write(ADDR_START_SENSOR_LEVEL, (uint8_t)startSensorLevel);
    EEPROM.write(ADDR_END_SENSOR_LEVEL, (uint8_t)endSensorLevel);

    // NEW: Save Water Sensing delays
    EEPROM.write(ADDR_WATER_SENSING_START_DELAY, waterSensingStartDelaySec >> 8);
    EEPROM.write(ADDR_WATER_SENSING_START_DELAY + 1, waterSensingStartDelaySec & 0xFF);
    EEPROM.write(ADDR_WATER_SENSING_STOP_DELAY, waterSensingStopDelaySec >> 8);
    EEPROM.write(ADDR_WATER_SENSING_STOP_DELAY + 1, waterSensingStopDelaySec & 0xFF);

    // NEW: Save Web UI refresh interval
    EEPROM.write(ADDR_WEB_UI_REFRESH_INTERVAL, webUiRefreshIntervalSec >> 8);
    EEPROM.write(ADDR_WEB_UI_REFRESH_INTERVAL + 1, webUiRefreshIntervalSec & 0xFF);

    EEPROM.write(ADDR_AUTO_RESET_DRY_RUN, autoResetDryRunOnWater ? 1 : 0); // NEW
    EEPROM.write(ADDR_AUTO_RESET_TURBIDITY, autoResetTurbidityOnError ? 1 : 0); // NEW
    EEPROM.write(ADDR_TRANSMITTER_TX_POWER, transmitterTxPower); // NEW
    EEPROM.write(ADDR_TRANSMITTER_ACK_ATTEMPTS, transmitterAckAttempts); // NEW
    EEPROM.write(ADDR_LORA_POWER_OPTIMIZATION, loraPowerOptimizationEnabled ? 1 : 0); // NEW

    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    esp_task_wdt_reset(); // Reset WDT before commit
    EEPROM.commit(); // Commit changes to EEPROM
    esp_task_wdt_reset(); // Reset WDT after commit
}

// Function to load settings from EEPROM
void loadSettingsFromEEPROM() {
    // Check if EEPROM has been initialized with magic value
    if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) { 
      ESP_LOGI(TAG, "%s
", String("EEPROM not initialized, using default settings.");
      // If EEPROM is not initialized, set defaults for the new buzzer styles.
      dryRunBeepStyle = BEEP_STYLE_ALERT;
      sensorErrorBeepStyle = BEEP_STYLE_WARNING;
      lowBatteryBeepStyle = BEEP_STYLE_PULSE;
      heartbeatExpiredBeepStyle = BEEP_STYLE_PULSE;
      tankEmptyBeepStyle = BEEP_STYLE_LONG;
      tankFullBeepStyle = BEEP_STYLE_LONG;
      dirtyWaterBeepStyle = BEEP_STYLE_WARNING; // NEW
      // Also initialize default STA credentials
      strncpy(staSsid, "YourHomeSSID", sizeof(staSsid));
      strncpy(staPassword, "YourHomePassword", sizeof(staPassword));
      // NEW: Initialize passwords
      strncpy(ap_password, "", sizeof(ap_password)); // AP password is now empty
      strncpy(http_password, "sirftumhareliye", sizeof(http_password));
      currentProfile = PROFILE_FULLY_AUTOMATIC; // Default profile
      return; // If not initialized, use default values defined in global variables
    }

    currentProfile = (SystemProfile)EEPROM.read(ADDR_SYSTEM_PROFILE); // NEW: Load System Profile
    if (currentProfile > PROFILE_FULLY_AUTOMATIC) currentProfile = PROFILE_FULLY_AUTOMATIC; // Validate

    pumpMode = (PumpMode)EEPROM.read(ADDR_PUMP_MODE);
    dryRunDelayMin = EEPROM.read(ADDR_DRY_DELAY);
    dryRunAttempts = EEPROM.read(ADDR_DRY_ATTEMPTS);
    retryIntervalMin = EEPROM.read(ADDR_RETRY_INTERVAL);
    scheduleDurationMin = EEPROM.read(ADDR_SCHED_DURATION);

    // NEW: Load priming time (from two bytes)
    uint8_t highByte = EEPROM.read(ADDR_PRIMING_TIME);
    uint8_t lowByte = EEPROM.read(ADDR_PRIMING_TIME + 1);
    primingTimeMs = (highByte << 8) | lowByte;
    // Validate priming time
    if (primingTimeMs < 100 || primingTimeMs > 10000) {
        primingTimeMs = 2000; // Reset to default if value is out of range
    }
    
    // UPDATED: Correctly READ schedules from EEPROM instead of writing to it.
    for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
      schedules[i].hour = EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 0);
      schedules[i].minute = EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 1);
      
      uint8_t repeatVal = EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 2);
      // Validate the read value before casting to prevent invalid states
      if (repeatVal < 5) { // There are 5 valid repeat types (0-4)
          schedules[i].repeatType = (RepeatType)repeatVal;
      } else {
          schedules[i].repeatType = EVERY_DAY; // Default to EVERY_DAY if EEPROM data is invalid
      }
      // NEW: Load date fields
      schedules[i].year = (EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 3) << 8) | EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 4);
      schedules[i].month = EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 5);
      schedules[i].day = EEPROM.read(ADDR_SCHEDULE_START + i * 7 + 6);
    }
    
    forceScheduledRun = EEPROM.read(ADDR_FORCE_SCHEDULED_RUN); // Use new calculated address
    sensorErrorBypassEnabled = EEPROM.read(ADDR_SENSOR_ERROR_BYPASS); // Load new bypass setting
    currentFontIndex = EEPROM.read(ADDR_DISPLAY_FONT_INDEX); // Load new font index
    if (currentFontIndex >= numDisplayFonts) { // Validate loaded index
        currentFontIndex = 0;
    }
    buzzerEnabled = EEPROM.read(ADDR_BUZZER_ENABLED); // NEW: Load buzzer enabled state
    buzzerVolume = EEPROM.read(ADDR_BUZZER_VOLUME); // NEW: Load buzzer volume
    
    // NEW (BUG FIX): Add validation to prevent a silent buzzer if it's enabled.
    // This handles cases where EEPROM might have been corrupted with a volume of 0.
    if (buzzerEnabled && buzzerVolume == 0) {
        buzzerVolume = 128; // Reset to a default audible volume
    }

    manualModeSafetyLogicEnabled = EEPROM.read(ADDR_MANUAL_MODE_SAFETY_LOGIC); // NEW: Load manual mode safety logic state
    dryRunLogicEnabled = EEPROM.read(ADDR_DRY_RUN_LOGIC_ENABLED); // NEW: Load dry run logic enabled flag
    retryLogicEnabled = EEPROM.read(ADDR_RETRY_LOGIC_ENABLED); // NEW: Load retry logic enabled flag
    turbidityBypassEnabled = EEPROM.read(ADDR_TURBIDITY_BYPASS); // NEW: Load turbidity bypass
    turbidityTimeoutSec = EEPROM.read(ADDR_TURBIDITY_TIMEOUT); // NEW: Load turbidity timeout
    if (turbidityTimeoutSec < 1 || turbidityTimeoutSec > 60) turbidityTimeoutSec = 3; // NEW: Validate timeout

    // NEW: Load calibration values
    highByte = EEPROM.read(ADDR_CALIBRATED_CLEAN);
    lowByte = EEPROM.read(ADDR_CALIBRATED_CLEAN + 1);
    calibratedCleanValue = (highByte << 8) | lowByte;
    if (calibratedCleanValue > 4095) calibratedCleanValue = 0;

    highByte = EEPROM.read(ADDR_CALIBRATED_DIRTY);
    lowByte = EEPROM.read(ADDR_CALIBRATED_DIRTY + 1);
    calibratedDirtyValue = (highByte << 8) | lowByte;
    if (calibratedDirtyValue > 4095) calibratedDirtyValue = 4095;

    // NEW: Load WiFi settings
    wifiMode = (WifiMode)EEPROM.read(ADDR_WIFI_MODE);
    EEPROM.get(ADDR_STA_SSID, staSsid);
    EEPROM.get(ADDR_STA_PASSWORD, staPassword);
    // Ensure null termination for strings loaded from EEPROM
    staSsid[31] = '\0';
    staPassword[63] = '\0';

    // NEW: Load passwords
    // EEPROM.get(ADDR_AP_PASSWORD, ap_password); // AP Password is no longer loaded
    strncpy(ap_password, "", sizeof(ap_password)); // Always ensure AP password is empty on load
    EEPROM.get(ADDR_HTTP_PASSWORD, http_password);
    // ap_password[63] = '\0'; // MODIFIED: Removed typo, this line was incorrect
    http_password[63] = '\0';

    // NEW: Add a failsafe for an empty AP password
    // If the loaded password is empty, revert to the default.
    // This check is no longer needed as we want an empty password
    /*
    if (ap_password[0] == '\0') {
        ESP_LOGI(TAG, "%s
", String("WARN: Loaded AP password was empty. Reverting to default.");
        strncpy(ap_password, "sirftumhareliye", sizeof(ap_password) - 1);
        ap_password[sizeof(ap_password) - 1] = '\0'; // Ensure null termination
    }
    */
    
    // MODIFIED: Add a failsafe for an empty or corrupted HTTP password
    if (http_password[0] == '\0' || http_password[0] == (char)0xFF) {
        ESP_LOGI(TAG, "%s
", String("WARN: Loaded HTTP password was invalid. Reverting to default.");
        strncpy(http_password, "sirftumhareliye", sizeof(http_password) - 1);
        http_password[sizeof(http_password) - 1] = '\0'; // Ensure null termination
    }


    currentOledTheme = (OledTheme)EEPROM.read(ADDR_OLED_THEME); // NEW: Load OLED theme
    currentWebTheme = EEPROM.read(ADDR_WEB_THEME);              // NEW: Load Web UI theme
    
    // NEW: Load buzzer style settings
    // Validate the EEPROM value before casting to prevent out-of-bounds array access.
    uint8_t val = EEPROM.read(ADDR_DRY_RUN_BEEP); dryRunBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_ALERT;
    val = EEPROM.read(ADDR_SENSOR_BEEP); sensorErrorBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_WARNING;
    val = EEPROM.read(ADDR_LOW_BATTERY_BEEP); lowBatteryBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_PULSE;
    val = EEPROM.read(ADDR_HEARTBEAT_BEEP); heartbeatExpiredBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_PULSE;
    val = EEPROM.read(ADDR_TANK_EMPTY_BEEP); tankEmptyBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_LONG;
    val = EEPROM.read(ADDR_TANK_FULL_BEEP); tankFullBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_LONG;
    val = EEPROM.read(ADDR_DIRTY_WATER_BEEP); dirtyWaterBeepStyle = (val < numBeepStyles) ? (BeepStyle)val : BEEP_STYLE_WARNING; // NEW

    // NEW: Load OLED Layout settings, with a check for uninitialized EEPROM
    if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
        oledShowTime = true;
        oledShowDate = false;
        oledShowDay = false;
        oledShowBattery = true;
    } else {
        oledShowTime = EEPROM.read(ADDR_OLED_SHOW_TIME);
        oledShowDate = EEPROM.read(ADDR_OLED_SHOW_DATE);
        oledShowDay = EEPROM.read(ADDR_OLED_SHOW_DAY);
        oledShowBattery = EEPROM.read(ADDR_OLED_SHOW_BATTERY);
    }

    // NEW: Load water presence threshold (from two bytes)
    highByte = EEPROM.read(ADDR_WATER_PRESENCE_THRESHOLD);
    lowByte = EEPROM.read(ADDR_WATER_PRESENCE_THRESHOLD + 1);
    waterPresenceThreshold = (highByte << 8) | lowByte;
    // Validate the threshold
    if (waterPresenceThreshold > 4095) {
        waterPresenceThreshold = 3500; // Reset to default if value is out of range
    }

    // NEW: Load Start/End Sensor Levels
    startSensorLevel = (TankLevel)EEPROM.read(ADDR_START_SENSOR_LEVEL);
    if (startSensorLevel > TANK_75) startSensorLevel = TANK_EMPTY; // Validate: cannot start higher than 75%

    endSensorLevel = (TankLevel)EEPROM.read(ADDR_END_SENSOR_LEVEL);
    if (endSensorLevel < TANK_25 || endSensorLevel > TANK_FULL) endSensorLevel = TANK_FULL; // Validate: must end at 25% or higher

    // NEW: Final validation to ensure start is less than end
    if (startSensorLevel >= endSensorLevel) {
        startSensorLevel = TANK_EMPTY;
        endSensorLevel = TANK_FULL;
    }

    // NEW: Load Water Sensing delays
    highByte = EEPROM.read(ADDR_WATER_SENSING_START_DELAY);
    lowByte = EEPROM.read(ADDR_WATER_SENSING_START_DELAY + 1);
    waterSensingStartDelaySec = (highByte << 8) | lowByte;
    if (waterSensingStartDelaySec < 5 || waterSensingStartDelaySec > 1800) waterSensingStartDelaySec = 5; // Validate 5s to 30min

    highByte = EEPROM.read(ADDR_WATER_SENSING_STOP_DELAY);
    lowByte = EEPROM.read(ADDR_WATER_SENSING_STOP_DELAY + 1);
    waterSensingStopDelaySec = (highByte << 8) | lowByte;
    if (waterSensingStopDelaySec < 5 || waterSensingStopDelaySec > 1800) waterSensingStopDelaySec = 5; // Validate 5s to 30min

    // NEW: Load Web UI refresh interval
    highByte = EEPROM.read(ADDR_WEB_UI_REFRESH_INTERVAL);
    lowByte = EEPROM.read(ADDR_WEB_UI_REFRESH_INTERVAL + 1);
    webUiRefreshIntervalSec = (highByte << 8) | lowByte;
    // Validate: 10 seconds to 300 seconds (5 minutes)
    if (webUiRefreshIntervalSec < 10 || webUiRefreshIntervalSec > 300) {
        webUiRefreshIntervalSec = 10; // Reset to default if out of range
    }

    autoResetDryRunOnWater = EEPROM.read(ADDR_AUTO_RESET_DRY_RUN); // NEW
    autoResetTurbidityOnError = EEPROM.read(ADDR_AUTO_RESET_TURBIDITY); // NEW

    // NEW: Load Transmitter TX Power
    transmitterTxPower = EEPROM.read(ADDR_TRANSMITTER_TX_POWER);
    if (transmitterTxPower < 2 || transmitterTxPower > 20) {
        transmitterTxPower = 17; // Validate and reset to default if needed
    }

    // NEW: Load Transmitter ACK Attempts
    transmitterAckAttempts = EEPROM.read(ADDR_TRANSMITTER_ACK_ATTEMPTS);
    if (transmitterAckAttempts < 1 || transmitterAckAttempts > 10) {
        transmitterAckAttempts = 3; // Validate and reset to default
    }
    loraPowerOptimizationEnabled = EEPROM.read(ADDR_LORA_POWER_OPTIMIZATION); // NEW


    ESP_LOGI(TAG, "%s
", String("Settings loaded from EEPROM.");
}


// === Pump Control Helper Functions ===
bool initiatePumpStart(bool isScheduledRun) {
    // NEW: Block pump actions if in Level Indicator profile
    if (currentProfile == PROFILE_LEVEL_INDICATOR) {
        ESP_LOGI(TAG, "%s
", String("Pump start blocked: Level Indicator profile is active.");
        pumpBlockedMessageActive = true;
        pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
        return false;
    }

    // Don't start if a sequence is already in progress or pump is already running.
    // Also, don't start if there's a sensor error AND bypass is NOT enabled,
    // UNLESS it's a scheduled run (which has its own checks before calling this).
    // UPDATED: Added a check for dryRunError to block any new start.
    // MODIFIED: The dryRunError check is now BYPASSED if it's a scheduled run (isScheduledRun == true),
    // because the schedule logic in loop() is now responsible for resetting the error *before* calling this function.
    if ( (dryRunError && !isScheduledRun) || waitingForPumpRelay || pumpRunning || (!isScheduledRun && sensorError != SENSOR_ERROR_NONE && !sensorErrorBypassEnabled)) {
      if (dryRunError && !isScheduledRun) { // MODIFIED: Only log/block if it's NOT a scheduled run.
          ESP_LOGI(TAG, "%s
", String("Pump start blocked: Dry Run Error is active. Please reset.");
          // Optionally trigger the pump blocked message on the display
          pumpBlockedMessageActive = true;
          pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
      } else {
          ESP_LOGI(TAG, "%s
", String("Pump start blocked due to an existing run or an un-bypassed sensor error.");
      }
      return false;
    }

    // NEW: Check if dry run logic is enabled before initiating the pump start
    if (dryRunLogicEnabled) {
        // For Monoblock, start pump relay directly without priming.
        if (currentProfile == PROFILE_MONOBLOCK) {
            ESP_LOGI(TAG, "%s
", String("Monoblock Profile: Starting pump directly (no priming).");
            gpio_set_level(RELAY_PIN, 1); // Turn ON pump relay directly
            pumpRunning = true;
            pumpStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            waitingForPumpRelay = false; // Not waiting for priming sequence
            if (pumpMode != MANUAL || manualModeSafetyLogicEnabled) {
                waitingForFlow = true;
            }
            flowOk = false;
        } else { // For Submersible and Fully Automatic, use priming sequence
            ESP_LOGI(TAG, "Initiating pump start sequence (%dms valve delay).\n", primingTimeMs);
            ESP_LOGI(TAG, "%s
", String("-> Valve relay (pin 17) setting to 1 (ON).");
            gpio_set_level(VALVE_RELAY_PIN, 1); // Turn ON valve relay (high-triggered)
            valveRelayStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            waitingForPumpRelay = true;
        }
        
        // Reset dryRunError and retryInProgress for a new run, but NOT remainingDryRunAttempts
    dryRunError = false;
    retryInProgress = false;
    // NEW: Reset all error beep flags
    // REMOVED: isDryRunErrorBeepActive = false; // BUGFIX: Do not reset this here, it causes re-beeping on retries.
    isSensorErrorBeepActive = false;
    isHeartbeatExpiredBeepActive = false;
    // remainingDryRunAttempts is NOT reset here. It's reset on successful flow or manual reset.
        currentPumpRunHadFlow = false; // NEW: Reset flow flag for new pump run
    } else {
        // If dry run logic is disabled, directly start the pump without the priming sequence
        ESP_LOGI(TAG, "%s
", String("Dry run logic disabled. Starting pump directly.");
        ESP_LOGI(TAG, "%s
", String("-> Pump relay (pin 16) setting to 1 (ON).");
        gpio_set_level(RELAY_PIN, 1);
        pumpRunning = true;
        pumpStartTime = (uint32_t)(esp_timer_get_time() / 1000);
    }
    return true;
}

void stopPump() {
    // NEW: In Level Indicator mode, this function does nothing.
    if (currentProfile == PROFILE_LEVEL_INDICATOR) return;

    ESP_LOGI(TAG, "%s
", String("Pump stopping...");
    gpio_set_level(RELAY_PIN, 0);
    gpio_set_level(VALVE_RELAY_PIN, 0); // Ensure valve relay is OFF
    pumpRunning = false;
    waitingForPumpRelay = false; // Cancel any pending start sequence
    waitingForFlow = false;
    flowOk = false;
    retryInProgress = false;
    ESP_LOGI(TAG, "%s
", String("Pump stopped.");
    ESP_LOGI(TAG, "%s
", String("-> Pump relay (pin 16) set to 0 (OFF).");
    ESP_LOGI(TAG, "%s
", String("-> Valve relay (pin 17) set to 0 (OFF).");
}

// Function to get tank level from the 4 new switch states
TankLevel getTankLevelFromSwitches(bool s_25, bool s_50, bool s_75, bool s_100) {
    if (s_100) {
        // If 100% is true, all lower levels must also be true.
        if (!s_75 || !s_50 || !s_25) return TANK_INVALID;
        return TANK_FULL;
    } else if (s_75) {
        // If 75% is true, 50% and 25% must also be true.
        if (!s_50 || !s_25) return TANK_INVALID;
        return TANK_75;
    } else if (s_50) {
        // If 50% is true, 25% must also be true.
        if (!s_25) return TANK_INVALID;
        return TANK_50;
    } else if (s_25) {
        // If only 25% is true (all others are false)
        return TANK_25;
    } else {
        // All switches are false, tank is empty
        return TANK_EMPTY;
    }
}

// NEW: Function to get tank level string (moved to global scope for wider use)
const char* getTankLevelString(TankLevel level) {
    switch (level) {
        case TANK_EMPTY: return "EMPTY";
        case TANK_25: return "25%";
        case TANK_50: return "HALF";
        case TANK_75: return "75%";
        case TANK_FULL: return "FULL";
        case TANK_INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

// NEW: Function to auto-populate a blank schedule with the current date and time
void autoPopulateBlankSchedule(uint8_t scheduleIndex) {
    if (scheduleIndex >= MAX_SCHEDULES) return; // Boundary check
    
    Schedule& selectedSched = schedules[scheduleIndex];
    // A "blank" schedule is identified by all-zero time and date components
    if (selectedSched.hour == 0 && selectedSched.minute == 0 && selectedSched.year == 0) {
        if (isRtcWorking) {
            DateTime now = rtc.now();
            selectedSched.hour = now.hour();
            selectedSched.minute = now.minute();
            selectedSched.repeatType = SPECIFIC_DATE; // Default to specific date to make all fields relevant
            selectedSched.year = now.year();
            selectedSched.month = now.month();
            selectedSched.day = now.day();
        }
    }
}

// REWRITTEN: This entire function is replaced with a more robust and readable version.
void handleMenuButtons() {
    // Call debounce for all buttons first to update their states
    bool setPressed = debounceButton(BUTTON_SET, setBtn);
    bool upPressed = debounceButton(BUTTON_UP, upBtn);
    bool downPressed = debounceButton(BUTTON_DOWN, downBtn);
    static unsigned long lastRepeatTime = 0;

    // --- NEW: High Priority State: Factory Reset Confirmation ---
    if (factoryResetConfirmActive) {
        if ((uint32_t)(esp_timer_get_time() / 1000) - factoryResetConfirmStartTime > 10000) { // 10 second timeout
            factoryResetConfirmActive = false;
            return;
        }
        if (setPressed) {
            factoryResetConfirmActive = false; // Acknowledge press
            
            // Invalidate EEPROM and restart
            EEPROM.write(EEPROM_MAGIC_ADDR, 0x00);
            EEPROM.commit();
            
            // Display restarting message
            display.clearBuffer();
            display.setFont(u8g2_font_7x13_tf);
            display.drawStr((128 - display.getStrWidth("Resetting...")) / 2, 28, "Resetting...");
            display.drawStr((128 - display.getStrWidth("Rebooting...")) / 2, 48, "Rebooting...");
            display.sendBuffer();
            
            vTaskDelay(pdMS_TO_TICKS(2000);
            ESP.restart();
        } else if (upPressed || downPressed) {
            factoryResetConfirmActive = false; // Cancel on any other button press
        }
        return; // Consume all button presses while in this state
    }

    // --- High Priority Actions (checked every loop cycle) ---

    // 1. Long Press SET to exit settings menu (highest priority inside menu)
    if (inSettingsMenu && setBtn.isPressed && !setPressed && ((uint32_t)(esp_timer_get_time() / 1000) - setBtn.pressStartTime >= BUTTON_LONG_PRESS_THRESHOLD_MS)) {
        inSettingsMenu = false;
        editingSetting = false;
        menuHistoryPointer = 0;
        currentCalibrationState = CAL_IDLE;
        editingScheduleIndex = 0;
        ESP_LOGI(TAG, "%s
", String("Long SET press: Exiting settings menu to home screen.");
        saveSettingsToEEPROM();
        ignoreButtonsUntil = (uint32_t)(esp_timer_get_time() / 1000) + 500; // Ignore for 500ms to prevent re-entry
        return; // Action complete
    }

    // 2. Stop buzzer on any SET press (high priority)
    if (setPressed && isBuzzerCurrentlyOn) {
        stopBuzzer();
        return; // Consume this press to only silence the alarm.
    }

    // --- Logic when NOT in settings menu (Main Screen) ---
    if (!inSettingsMenu) {
        if (setPressed) {
            inSettingsMenu = true;
            currentMenuPage = PAGE_MAIN_MENU;
            menuIndex = 0;
            editingSetting = false;
            menuHistoryPointer = 0;
            ESP_LOGI(TAG, "%s
", String("Entering settings menu.");
            startBriefBeep();
        } else if (upPressed || downPressed) {
            startBriefBeep();
            if (currentProfile == PROFILE_LEVEL_INDICATOR) {
                pumpBlockedMessageActive = true;
                pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                ESP_LOGI(TAG, "%s
", String("PUMP BLOCKED: Manual toggle attempted in Level Indicator profile.");
                return;
            }
            if (sensorError != SENSOR_ERROR_NONE && !sensorErrorBypassEnabled) {
                pumpBlockedMessageActive = true;
                pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                ESP_LOGI(TAG, "%s
", String("PUMP BLOCKED: Sensor Error active and bypass disabled.");
                return;
            }
            if (pumpMode == MANUAL) {
                if (upPressed && !pumpRunning && !waitingForPumpRelay) { initiatePumpStart(false); }
                else if (downPressed && (pumpRunning || waitingForPumpRelay)) { stopPump(); if (currentPumpRunHadFlow) remainingDryRunAttempts = dryRunAttempts; }
            } else if (pumpMode == AUTO || pumpMode == WATER_SENSING) { // FIX: Allow manual override in Water Sensing mode
                if (downPressed) {
                    if (!pumpRunning && !waitingForPumpRelay) { initiatePumpStart(false); }
                    else { stopPump(); if (currentPumpRunHadFlow) remainingDryRunAttempts = dryRunAttempts; }
                }
            } else { // SCHEDULE mode
                // NEW: Allow manual STOP override in SCHEDULE mode
                if (downPressed && (pumpRunning || waitingForPumpRelay)) {
                    stopPump();
                    if (currentPumpRunHadFlow) remainingDryRunAttempts = dryRunAttempts;
                } else if (upPressed || (downPressed && !pumpRunning && !waitingForPumpRelay)) { // Block manual START
                    pumpBlockedMessageActive = true;
                    pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                    ESP_LOGI(TAG, "%s
", String("PUMP BLOCKED: Manual START not allowed in SCHEDULE mode.");
                }
            }
        }
        return; // Nothing more to do if not in settings menu
    }

    // --- Logic when IN settings menu ---
    // This is the beginning of the new, simplified structure.
    
    // Beep for any new button press inside the menu.
    if (setPressed || upPressed || downPressed) {
        startBriefBeep();
    }

    const MenuItem& currentItem = menuPages[currentMenuPage][menuIndex];

    if (editingSetting) {
        // === VALUE EDITING LOGIC ===
        // If we are in edit mode, the SET button confirms the change.
        if (setPressed) {
            // RE-ENABLED: Validate the schedule before saving.
            if (currentMenuPage == PAGE_SCHEDULE_EDIT && !isScheduleValid(schedules[editingScheduleIndex])) {
                scheduleInvalidMessageActive = true; scheduleInvalidMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                return; // Stay in editing mode if schedule is invalid
            }
            editingSetting = false;
            saveSettingsToEEPROM();
            return; // Exit handled
        }

        // Check if a value change should be applied (new press or auto-repeat)
        bool applyChange = false;
        if (upPressed || downPressed) {
            applyChange = true;
            lastRepeatTime = (uint32_t)(esp_timer_get_time() / 1000);
        } else if ((upBtn.isPressed || downBtn.isPressed) && ((uint32_t)(esp_timer_get_time() / 1000) - (upBtn.isPressed ? upBtn.pressStartTime : downBtn.pressStartTime) > BUTTON_REPEAT_DELAY_MS)) {
            if ((uint32_t)(esp_timer_get_time() / 1000) - lastRepeatTime > BUTTON_REPEAT_RATE_MS) {
                applyChange = true;
                lastRepeatTime = (uint32_t)(esp_timer_get_time() / 1000);
            }
        }

        if (applyChange) {
            // Determine direction: 1 for UP, -1 for DOWN.
            // This correctly handles both initial press and holding for auto-repeat.
            int direction = (upPressed || upBtn.isPressed) ? 1 : -1;

            switch (currentMenuPage) {
                case PAGE_SYSTEM_PROFILE:
                    if (currentItem.settingIndex == 0) currentProfile = (SystemProfile)((currentProfile + direction + 4) % 4);
                    break;
                case PAGE_PUMP_SETTINGS:
                    switch (currentItem.settingIndex) {
                        case 0: pumpMode = (PumpMode)((pumpMode + direction + 4) % 4); break;
                        case 1: // Start Sensor Lvl
                            if (direction > 0) { // UP
                                if (startSensorLevel < TANK_75) startSensorLevel = (TankLevel)(startSensorLevel + 1);
                            } else { // DOWN
                                if (startSensorLevel > TANK_EMPTY) startSensorLevel = (TankLevel)(startSensorLevel - 1);
                            }
                            if (startSensorLevel >= endSensorLevel) {
                                endSensorLevel = (TankLevel)min((int)startSensorLevel + 1, (int)TANK_FULL);
                            }
                            break;
                        case 2: // End Sensor Lvl
                            if (direction > 0) { // UP
                                if (endSensorLevel < TANK_FULL) endSensorLevel = (TankLevel)(endSensorLevel + 1);
                            } else { // DOWN
                                if (endSensorLevel > TANK_25) endSensorLevel = (TankLevel)(endSensorLevel - 1);
                            }
                            if (endSensorLevel <= startSensorLevel) {
                                startSensorLevel = (TankLevel)max((int)endSensorLevel - 1, (int)TANK_EMPTY);
                            }
                            break;
                        case 3: if (direction > 0) { if (scheduleDurationMin < 60) scheduleDurationMin++; else scheduleDurationMin = 0; } else { if (scheduleDurationMin > 0) scheduleDurationMin--; else scheduleDurationMin = 60; } break;
                        case 4: if (direction > 0) { if (primingTimeMs < 10000) primingTimeMs += 100; } else { if (primingTimeMs > 100) primingTimeMs -= 100; } break;
                        case 5: forceScheduledRun = !forceScheduledRun; break;
                        case 6: manualModeSafetyLogicEnabled = !manualModeSafetyLogicEnabled; break;
                    }
                    break;
                case PAGE_SAFETY_SENSORS:
                    switch (currentItem.settingIndex) {
                        case 0: dryRunLogicEnabled = !dryRunLogicEnabled; break;
                        case 1: if (direction > 0) { if (dryRunDelayMin < 10) dryRunDelayMin++; else dryRunDelayMin = 1; } else { if (dryRunDelayMin > 1) dryRunDelayMin--; else dryRunDelayMin = 10; } break;
                        case 2: retryLogicEnabled = !retryLogicEnabled; break;
                        case 3: if (direction > 0) { if (dryRunAttempts < 10) dryRunAttempts++; else dryRunAttempts = 1; } else { if (dryRunAttempts > 1) dryRunAttempts--; else dryRunAttempts = 10; } remainingDryRunAttempts = dryRunAttempts; break;
                        case 4: if (direction > 0) { if (retryIntervalMin < 59) retryIntervalMin++; else retryIntervalMin = 1; } else { if (retryIntervalMin > 1) retryIntervalMin--; else retryIntervalMin = 59; } break;
                        case 5: sensorErrorBypassEnabled = !sensorErrorBypassEnabled; break;
                        case 6: turbidityBypassEnabled = !turbidityBypassEnabled; break;
                        case 7: if (direction > 0) { if (turbidityTimeoutSec < 60) turbidityTimeoutSec++; else turbidityTimeoutSec = 1; } else { if (turbidityTimeoutSec > 1) turbidityTimeoutSec--; else turbidityTimeoutSec = 60; } break;
                        case 8: if (direction > 0) { if (waterPresenceThreshold < 4085) waterPresenceThreshold += 10; else waterPresenceThreshold = 4095; } else { if (waterPresenceThreshold >= 10) waterPresenceThreshold -= 10; else waterPresenceThreshold = 0; } break;
                        case 9: // Water Start Delay
                            {
                                int step = (waterSensingStartDelaySec >= 60) ? 10 : 1; // Step by 1s below 1min, 10s above
                                if (direction > 0) { if (waterSensingStartDelaySec <= 1800 - step) waterSensingStartDelaySec += step; else waterSensingStartDelaySec = 1800; }
                                else { if (waterSensingStartDelaySec >= 5 + step) waterSensingStartDelaySec -= step; else waterSensingStartDelaySec = 5; }
                            }
                            break;
                        case 10: // Water Stop Delay
                            {
                                int step = (waterSensingStopDelaySec >= 60) ? 10 : 1;
                                if (direction > 0) { if (waterSensingStopDelaySec <= 1800 - step) waterSensingStopDelaySec += step; else waterSensingStopDelaySec = 1800; }
                                else { if (waterSensingStopDelaySec >= 5 + step) waterSensingStopDelaySec -= step; else waterSensingStopDelaySec = 5; }
                            }
                            break;
                        case 11: autoResetDryRunOnWater = !autoResetDryRunOnWater; break; // NEW
                        case 12: autoResetTurbidityOnError = !autoResetTurbidityOnError; break; // NEW
                    }
                    break;
                case PAGE_SCHEDULE_EDIT: {
                    uint8_t sched = editingScheduleIndex;
                    switch (currentItem.settingIndex) {
                        case 0: schedules[sched].hour = (schedules[sched].hour + direction + 24) % 24; break;
                        case 1: schedules[sched].minute = (schedules[sched].minute + direction + 60) % 60; break;
                        case 2: schedules[sched].repeatType = (RepeatType)((schedules[sched].repeatType + direction + 5) % 5); break;
                        case 3: if (schedules[sched].repeatType == SPECIFIC_DATE) { if (direction > 0) { if(schedules[sched].year < 2099) schedules[sched].year++; else schedules[sched].year = 2024; } else { if(schedules[sched].year > 2024) schedules[sched].year--; else schedules[sched].year = 2099; } } break;
                        case 4: if (schedules[sched].repeatType == SPECIFIC_DATE) { if (direction > 0) { if(schedules[sched].month < 12) schedules[sched].month++; else schedules[sched].month = 1; } else { if(schedules[sched].month > 1) schedules[sched].month--; else schedules[sched].month = 12; } } break;
                        case 5: if (schedules[sched].repeatType == SPECIFIC_DATE) { if (direction > 0) { if(schedules[sched].day < 31) schedules[sched].day++; else schedules[sched].day = 1; } else { if(schedules[sched].day > 1) schedules[sched].day--; else schedules[sched].day = 31; } } break;
                    }
                    break;
                }
                case PAGE_RTC_SETTINGS:
                    if (isRtcWorking) {
                        DateTime currentRTC = rtc.now();
                        int16_t year = currentRTC.year(); uint8_t month = currentRTC.month(); uint8_t day = currentRTC.day();
                        uint8_t hour = currentRTC.hour(); uint8_t minute = currentRTC.minute(); uint8_t second = currentRTC.second();
                        switch (currentItem.settingIndex) {
                            case 0: year += direction; if(year < 2000) year = 2099; if(year > 2099) year = 2000; break;
                            case 1: month += direction; if(month < 1) month = 12; if(month > 12) month = 1; break;
                            case 2: day += direction; if(day < 1) day = 31; if(day > 31) day = 1; break;
                            case 3: hour = (hour + direction + 24) % 24; break;
                            case 4: minute = (minute + direction + 60) % 60; break;
                            case 5: second = (second + direction + 60) % 60; break;
                        }
                        rtc.adjust(DateTime(year, month, day, hour, minute, second));
                    }
                    break;
                case PAGE_DISPLAY_SETTINGS:
                    switch (currentItem.settingIndex) {
                        case 0: currentFontIndex = (currentFontIndex + direction + numDisplayFonts) % numDisplayFonts; break;
                        case 1: currentOledTheme = (OledTheme)((currentOledTheme + direction + 2) % 2); break;
                    }
                    break;
                case PAGE_OLED_LAYOUT:
                    switch (currentItem.settingIndex) {
                        case 0: oledShowTime = !oledShowTime; break;
                        case 1: oledShowDate = !oledShowDate; break;
                        case 2: oledShowDay = !oledShowDay; break;
                        case 3: oledShowBattery = !oledShowBattery; break;
                    }
                    break;
                case PAGE_BUZZER_SETTINGS:
                    switch (currentItem.settingIndex) {
                        case 0: buzzerEnabled = !buzzerEnabled; break;
                        case 1: dryRunBeepStyle = (BeepStyle)((dryRunBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 2: sensorErrorBeepStyle = (BeepStyle)((sensorErrorBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 3: lowBatteryBeepStyle = (BeepStyle)((lowBatteryBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 4: heartbeatExpiredBeepStyle = (BeepStyle)((heartbeatExpiredBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 5: tankEmptyBeepStyle = (BeepStyle)((tankEmptyBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 6: tankFullBeepStyle = (BeepStyle)((tankFullBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 7: dirtyWaterBeepStyle = (BeepStyle)((dirtyWaterBeepStyle + direction + numBeepStyles) % numBeepStyles); break;
                        case 8: // Buzzer Volume
                            {
                                int newVolume = buzzerVolume + (direction * 5); // Step by 5 for faster change
                                if (newVolume < 0) newVolume = 0;
                                if (newVolume > 255) newVolume = 255;
                                buzzerVolume = newVolume;
                            }
                            break;
                    }
                    break;
                case PAGE_CALIBRATION:
                     if (currentItem.settingIndex == 2) { // Manual Limit
                        int newLimit = turbidityLimit + (direction * 10);
                        if (newLimit < 0) newLimit = 0;
                        if (newLimit > 4095) newLimit = 4095;
                        turbidityLimit = newLimit;
                    }
                    break;
                case PAGE_WIFI_SETTINGS:
                    if (currentItem.settingIndex == 0) { wifiMode = (WifiMode)((wifiMode + direction + 2) % 2); if (wifiMode == WIFI_AP_MODE) { WiFi.mode(WIFI_AP); WiFi.softAP(ap_ssid, ap_password); } else { WiFi.mode(WIFI_STA); connectToWiFiSTA(); } }
                    break;
                case PAGE_LORA_SETTINGS: // NEW
                    if (currentItem.settingIndex == 0) { // TX Power
                        // NEW: Only allow manual adjustment if optimization is OFF
                        if (!loraPowerOptimizationEnabled) {
                            if (direction > 0) { if (transmitterTxPower < 20) transmitterTxPower++; }
                            else { if (transmitterTxPower > 2) transmitterTxPower--; }
                        }
                    } else if (currentItem.settingIndex == 1) { // NEW: ACK Attempts
                        if (direction > 0) { if (transmitterAckAttempts < 10) transmitterAckAttempts++; }
                        else { if (transmitterAckAttempts > 1) transmitterAckAttempts--; }
                    } else if (currentItem.settingIndex == 2) {
                        loraPowerOptimizationEnabled = !loraPowerOptimizationEnabled;
                    }
                    break;
                default:
                    break;
            }
        }
    } else {
        // === NAVIGATION LOGIC ===
        // Special case for schedule selection on the edit page (which is a form of navigation)
        if (currentMenuPage == PAGE_SCHEDULE_EDIT && currentItem.settingIndex == 10) {
            if (upPressed) {
                editingScheduleIndex = (editingScheduleIndex == 0) ? MAX_SCHEDULES - 1 : editingScheduleIndex - 1;
                autoPopulateBlankSchedule(editingScheduleIndex);
            } else if (downPressed) {
                editingScheduleIndex = (editingScheduleIndex + 1) % MAX_SCHEDULES;
                autoPopulateBlankSchedule(editingScheduleIndex);
            } else if (setPressed) {
                // When SET is pressed on "Schedule #", just move to the next item ("Hour").
                menuIndex = (menuIndex + 1) % menuPageSizes[currentMenuPage];
            }
        } else { // Standard navigation for all other menu items
            if (upPressed) {
                menuIndex = (menuIndex == 0) ? (menuPageSizes[currentMenuPage] - 1) : (menuIndex - 1);
            } else if (downPressed) {
                menuIndex = (menuIndex + 1) % menuPageSizes[currentMenuPage];
            } else if (setPressed) {
                // NEW: 1 PRIORITY check for calibration completion. Overrides standard menu actions.
                if (currentMenuPage == PAGE_CALIBRATION && currentCalibrationState == CAL_DONE) {
                    saveSettingsToEEPROM();
                    currentCalibrationState = CAL_IDLE;
                    limitSetMessageActive = true; // Show confirmation message
                    limitSetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                    return; // Action complete, exit function
                }

                if (currentItem.type == MENU_ITEM_SUBMENU) {
                    if (currentItem.targetPage == PAGE_SCHEDULE_EDIT) autoPopulateBlankSchedule(editingScheduleIndex);
                    pushMenuPage((MenuPage)currentItem.targetPage);
                } else if (currentItem.type == MENU_ITEM_SETTING) {
                    // MODIFIED: Prevent entering edit mode for read-only info pages
                    if (currentMenuPage != PAGE_SYSTEM_INFO && currentMenuPage != PAGE_SECURITY) {
                        editingSetting = true;
                    }
                } else if (currentItem.type == MENU_ITEM_ACTION) {
                    if (currentMenuPage == PAGE_SCHEDULES_MENU && currentItem.settingIndex == 100) {
                        for (uint8_t i = 0; i < MAX_SCHEDULES; i++) schedules[i] = {0, 0, EVERY_DAY, 0, 0, 0};
                        schedulesClearedMessageActive = true; schedulesClearedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                    } else if (currentMenuPage == PAGE_SCHEDULE_EDIT && currentItem.settingIndex == 101) {
                        uint8_t sched = editingScheduleIndex; schedules[sched] = {0, 0, EVERY_DAY, 0, 0, 0};
                        singleScheduleClearedMessageActive = true; singleScheduleClearedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                        autoPopulateBlankSchedule(sched);
                    } else if (currentMenuPage == PAGE_CALIBRATION && currentItem.settingIndex == 0) { startCleanWaterCalibration();
                    } else if (currentMenuPage == PAGE_CALIBRATION && currentItem.settingIndex == 1) { startDirtyWaterCalibration();
                    } else if (currentItem.settingIndex == 98) { popMenuPage(); saveSettingsToEEPROM();
                    } else if (currentItem.settingIndex == 99) { inSettingsMenu = false; menuHistoryPointer = 0; saveSettingsToEEPROM();
                    } else if (currentItem.settingIndex == 100) { factoryResetConfirmActive = true; factoryResetConfirmStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                    }
                }
            }
        }
    }
}


// Function to update the OLED display
void updateDisplay() {
    display.clearBuffer(); // Clear the display buffer

    // NEW: Apply OLED Theme
    if (currentOledTheme == LIGHT) {
        display.setColorIndex(1); // Set ink to white
        display.drawBox(0, 0, 128, 64); // Fill screen with white (paper=1)
        display.setColorIndex(0); // Set ink to black for text
    } else {
        display.setColorIndex(1); // Set ink to white for text (paper=0)
    }

    // NEW: Handle Factory Reset confirmation screen
    if (factoryResetConfirmActive) {
        display.setFont(u8g2_font_7x13_tf);
        const char* msg1 = "Confirm Reset?";
        const char* msg2 = "SET=Yes Oth=No";
        int16_t x1 = (128 - display.getStrWidth(msg1)) / 2;
        int16_t x2 = (128 - display.getStrWidth(msg2)) / 2;
        display.drawStr(x1, 28, msg1);
        display.drawStr(x2, 48, msg2);
        display.sendBuffer();
        return; // Don't draw anything else
    }

    // NEW: Handle NTP Syncing/Synced messages
    if (isNtpSyncingMessageActive) {
        display.setFont(u8g2_font_7x13_tf);
        const char* msg = "Syncing Time...";
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg);
        display.sendBuffer();
        return; // Don't draw anything else
    }
    // NEW: Handle "Check Date OR Time" message
    if (scheduleInvalidMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - scheduleInvalidMessageStartTime < SCHEDULE_INVALID_DISPLAY_DURATION_MS) {
        display.setFont(u8g2_font_7x13_tf);
        const char* msg = "Check Date OR Time";
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg);
        display.sendBuffer();
        return;
      } else {
        scheduleInvalidMessageActive = false;
      }
    }

    // Check for Sensor Error message and blink logic
    // The message will be displayed for 5 seconds and then the normal screen will be displayed for 5 seconds
    if (sensorError != SENSOR_ERROR_NONE) {
        if (((uint32_t)(esp_timer_get_time() / 1000) / 5000) % 2 == 0) { // On for 5 seconds
            display.setFont(u8g2_font_helvB14_tr);
            const char* msg1 = "Sensor Error!";
            const char* msg2 = "";
            switch (sensorError) {
                case SENSOR_ERROR_INVALID_COMBINATION: msg2 = "Invalid Comb."; break;
                case SENSOR_ERROR_SEQUENCE_UP:         msg2 = "Seq. Up!"; break;
                case SENSOR_ERROR_SEQUENCE_DOWN:       msg2 = "Seq. Down!"; break;
                case SENSOR_ERROR_DIRTY_WATER:         msg2 = "Dirty Water!"; break; // NEW
                default:                               msg2 = "Unknown!"; break;
            }
            int16_t x1 = (128 - display.getStrWidth(msg1)) / 2;
            int16_t x2 = (128 - display.getStrWidth(msg2)) / 2;
            display.drawStr(x1, 16, msg1); // Adjusted Y for first line
            display.drawStr(x2, 38, msg2); // Adjusted Y for second line
            // Display the detailed message on the third line with a smaller font
            display.setFont(u8g2_font_6x12_tr); // Smaller font for detail
            int16_t x3 = (128 - display.getStrWidth(sensorErrorDetailMessage)) / 2;
            display.drawStr(x3, 58, sensorErrorDetailMessage);
            display.sendBuffer();
            return; // Don't draw anything else in this cycle
        }
    }


    // Check for PUMP BLOCKED message (next highest priority)
    if (pumpBlockedMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - pumpBlockedMessageStartTime < PUMP_BLOCKED_DISPLAY_DURATION_MS) {
        display.setFont(u8g2_font_helvB14_tr); // Larger font for "PUMP" and "BLOCKED"
        const char* msg1 = "PUMP";
        const char* msg2 = "BLOCKED";
        // Calculate positions to center both lines
        // Adjusted Y positions for better centering on a 64-pixel high display
        // Line 1: 64/2 - font_height/2 - spacing/2 = 32 - 7 - 6 = 19 (approx)
        // Line 2: 64/2 + font_height/2 + spacing/2 = 32 + 7 + 6 = 45 (approx)
        int16_t x1 = (128 - display.getStrWidth(msg1)) / 2;
        int16_t x2 = (128 - display.getStrWidth(msg2)) / 2;
        display.drawStr(x1, 24, msg1); // Top line
        display.drawStr(x2, 48, msg2); // Bottom line
        display.sendBuffer();
        return; // Don't draw anything else if pump blocked message is active
      } else {
        pumpBlockedMessageActive = false; // Message display time is over
      }
    }

    // NEW: Check for "Dry Run Reset" message
    if (dryRunResetMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - dryRunResetMessageStartTime < ERROR_RESET_MESSAGE_DURATION_MS) {
        display.setFont(u8g2_font_7x13_tf);
        const char* msg = "Dry Run Reset!";
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg);
        display.sendBuffer();
        return;
      } else {
        dryRunResetMessageActive = false;
      }
    }

    // NEW: Check for "Sensor Error Reset" message
    if (sensorErrorResetMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - sensorErrorResetMessageStartTime < ERROR_RESET_MESSAGE_DURATION_MS) {
        display.setFont(u8g2_font_7x13_tf);
        const char* msg = "Sensor Error Reset!";
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg);
        display.sendBuffer();
        return;
      } else {
        sensorErrorResetMessageActive = false;
      }
    }

    // NEW: Check for "Single Schedule Cleared" message
    if (singleScheduleClearedMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - singleScheduleClearedMessageStartTime < SCHEDULE_CLEARED_DISPLAY_DURATION_MS) {
        display.setFont(u8g2_font_6x12_tr);
        char msg[20];
        snprintf(msg, sizeof(msg), "Schedule %d Cleared", editingScheduleIndex + 1);
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg);
        display.sendBuffer();
        return;
      } else {
        singleScheduleClearedMessageActive = false;
      }
    }

    // Check for "Schedule cleared" message (lower priority)
    if (schedulesClearedMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - schedulesClearedMessageStartTime < SCHEDULE_CLEARED_DISPLAY_DURATION_MS) {
        display.setFont(u8g2_font_6x12_tr); // Decreased font size for "Schedule cleared"
        const char* msg = "Schedule cleared"; // Changed message
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg); // Centered vertically
        display.sendBuffer();
        return; // Don't draw anything else if this message is active
      } else {
        schedulesClearedMessageActive = false; // Message display time is over
      }
    }

    // NEW: Check for "Limit Set" message
    if (limitSetMessageActive) {
      if ((uint32_t)(esp_timer_get_time() / 1000) - limitSetMessageStartTime < SCHEDULE_CLEARED_DISPLAY_DURATION_MS) { // reuse duration
        display.setFont(u8g2_font_6x12_tr);
        char msg[20];
        snprintf(msg, sizeof(msg), "Limit Set: %d", turbidityLimit);
        int16_t x = (128 - display.getStrWidth(msg)) / 2;
        display.drawStr(x, 38, msg); // Centered vertically
        display.sendBuffer();
        return; // Don't draw anything else if this message is active
      } else {
        limitSetMessageActive = false; // Message display time is over
      }
    }


    DateTime now = rtc.now();
    char dynamicStr[30]; // For dynamic strings like countdowns, tank status, etc.
    char dateStr[20];    // For date and day

    // Determine if a next schedule is shown to control clock icon visibility
    bool nextScheduleWillBeShown = false;
    if (pumpMode == SCHEDULE && currentProfile != PROFILE_LEVEL_INDICATOR) { // Only show next schedule if in SCHEDULE mode and not Level Indicator profile
      for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
        if (!((schedules[i].hour == 0 && schedules[i].minute == 0 && schedules[i].year == 0) ||
              scheduleTriggeredToday[i])) {
          nextScheduleWillBeShown = true;
          break;
        }
      }
    }

    // Always draw top section (Time, Battery, Signal)
    display.setFont(u8g2_font_6x12_tr); // Smaller font for top section

    // NEW: WiFi icon (top left)
    if (wifiMode == WIFI_AP_MODE) {
        display.drawXBMP(0, 0, 8, 8, wifiConnectedIcon); // AP mode is always "connected"
    } else { // WIFI_STA_MODE
        if (staConnected) {
            display.drawXBMP(0, 0, 8, 8, wifiConnectedIcon); // STA connected
        } else {
            display.drawXBMP(0, 0, 8, 8, wifiDisconnectedIcon); // STA disconnected
        }
    }

    int16_t nextIconX = 16; // Start position for the next icon
    // NEW: Draw NTP icon if time has been synced
    if (ntpSyncCompleted) {
        display.drawXBMP(nextIconX, 0, 8, 8, ntpIcon);
        nextIconX += 16; // Increment position for the next icon
    }

    // Draw clock icon only if a next schedule will be shown, shifted to the right of the WiFi icon
    if (nextScheduleWillBeShown) {
      display.drawXBMP(nextIconX, 0, 8, 8, clockIcon);
    }
    
    // --- NEW: Display RSSI value and adjust signal icon position ---
    char rssiStr[12];
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", lastPacketRssi);
    // Right-align the text. The signal icon will be to the left of this.
    int16_t rssi_x = 128 - display.getStrWidth(rssiStr);
    display.drawStr(rssi_x, 8, rssiStr); // Draw the RSSI text

    // Draw the signal icon to the left of the RSSI text
    int16_t signal_icon_x = rssi_x - 10; // Position icon 10px to the left
    if (isRtcWorking && firstHeartbeatReceived && (rtc.now().unixtime() - lastHeartbeatTimeRTC.unixtime()) <= (HEARTBEAT_TIMEOUT / 1000))
      display.drawXBMP(signal_icon_x, 0, 8, 8, signalBar);
    else
      display.drawXBMP(signal_icon_x, 0, 8, 8, signalCut);
    

    // NEW: Dynamic Top Section Layout
    int16_t top_y = 16; // Initial Y position for the first line of text
    const int16_t line_height = 12; // Height for the small font

    if (oledShowTime) {
        sprintf(dynamicStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        int16_t x = (128 - display.getStrWidth(dynamicStr)) / 2;
        display.drawStr(x, top_y, dynamicStr);
        top_y += line_height;
    }

    if (oledShowDate || oledShowDay) {
        static const char* const daysOfTheWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        char top_line[25] = "";

        if (oledShowDate) {
            sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());
            strcat(top_line, dateStr);
        }
        if (oledShowDate && oledShowDay) {
            strcat(top_line, " ");
        }
        if (oledShowDay) {
            if (isRtcWorking) strcat(top_line, daysOfTheWeek[now.dayOfTheWeek()]);
        }

        int16_t x = (128 - display.getStrWidth(top_line)) / 2;
        display.drawStr(x, top_y, top_line);
        top_y += line_height;
    }

    if (oledShowBattery) {
        char batteryStr[20];
        snprintf(batteryStr, sizeof(batteryStr), "Batt: %.2fV", batteryVoltage);
        int16_t x = (128 - display.getStrWidth(batteryStr)) / 2;
        display.drawStr(x, top_y, batteryStr);
        top_y += line_height;
    }

    // --- Rotating Content for Middle (Icon) and Bottom (Message) Sections ---
    // Use the selected display font for the main content area
    display.setFont(displayFonts[currentFontIndex]);
    const char* msg = "";
    const uint8_t* icon = nullptr; // Icon for pump status
    int16_t x; // Variable for centering text
    
    // NEW: Conditionally hide icons if the top section is fully populated
    bool showIcons = !(oledShowTime && oledShowDate && oledShowDay && oledShowBattery);

    // NEW: Dynamically adjust vertical position of main content based on top section height
    const int16_t remaining_space_start = top_y > 16 ? top_y - 4 : 28; // Start below the info block, or at a fixed pos
    const int16_t icon_y = remaining_space_start + ((64 - remaining_space_start - 8) / 2);
    const int16_t msg_y_single_line = 62;
    const int16_t msg_y_line1_two_lines = 50; // FIX: Changed assignment to a const declaration
    const int16_t msg_y_line2_two_lines = 62;


    if (waitingForPumpRelay) {
      msg = "Priming...";
      icon = hourglass;
      x = (128 - display.getStrWidth(msg)) / 2;
      display.drawStr(x, msg_y_single_line, msg);
      if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
    } else if (pumpRunning) {
      switch (screenState) {
        case 1: // PUMP ON status
          msg = "PUMP ON";
          icon = waterDrop;
          x = (128 - display.getStrWidth(msg)) / 2;
          display.drawStr(x, msg_y_single_line, msg);
          if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          break;

        case 2: // Flow Status (only if not MANUAL or if manualModeSafetyLogicEnabled)
          // NEW: Only show flow status if dry run logic is enabled
          if (!dryRunLogicEnabled || (pumpMode == MANUAL && !manualModeSafetyLogicEnabled)) {
            screenState = 3; // Skip to next state
            msg = "PUMP ON"; icon = waterDrop;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          } else {
            if (waitingForFlow) { msg = "Waiting Flow"; icon = hourglass; }
            else if (flowOk) { msg = "Flow OK"; icon = checkMark; }
            else { msg = "PUMP ON"; icon = waterDrop; } // Fallback
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;

        case 3: // Dry run countdown (only if not MANUAL or if manualModeSafetyLogicEnabled)
          // NEW: Only show countdown if dry run logic is enabled
          if (!dryRunLogicEnabled || (pumpMode == MANUAL && !manualModeSafetyLogicEnabled)) {
            screenState = 4; // Skip to next state
            msg = "PUMP ON"; icon = waterDrop;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          } else {
            if (waitingForFlow) {
              uint32_t remaining = (dryRunDelayMin * 60) - (((uint32_t)(esp_timer_get_time() / 1000) - pumpStartTime) / 1000);
              if (remaining < 0) remaining = 0;
              snprintf(dynamicStr, sizeof(dynamicStr), "Dry: %luS", remaining);
              msg = dynamicStr;
              icon = hourglass;
            } else { msg = "PUMP ON"; icon = waterDrop; } // Fallback
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;

        case 4: // Tank Status (when pump is running)
          {
            const char* tankStatusStr;
            // UPDATED: Tank level display strings
            switch (currentTankLevel) {
                case TANK_FULL: tankStatusStr = "FULL"; break;
                case TANK_75: tankStatusStr = "75%";  break;
                case TANK_50: tankStatusStr = "HALF";  break;
                case TANK_25: tankStatusStr = "25%";  break;
                case TANK_EMPTY: tankStatusStr = "EMPTY"; break;
                case TANK_INVALID: tankStatusStr = "INVALID"; break;
                default: tankStatusStr = "UNKNOWN"; break;
            }
            snprintf(dynamicStr, sizeof(dynamicStr), "Tank: %s", tankStatusStr);
            msg = dynamicStr;
            icon = waterDrop; // Generic water icon
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;
        case 5: // NEW: Turbidity value display
          {
            snprintf(dynamicStr, sizeof(dynamicStr), "Turbidity: %d", turbidityValue);
            msg = dynamicStr;
            icon = turbidityIcon;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;
        default: // Fallback if screenState is out of expected range for pumpRunning
          msg = "PUMP ON";
          icon = waterDrop;
          x = (128 - display.getStrWidth(msg)) / 2;
          display.drawStr(x, msg_y_single_line, msg);
          if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          break;
      }
    } else { // Pump not running (idle, dry error, or retry)
      switch (screenState) {
        case 1: // Error/Retry message OR Tank Status (Idle)
          if (dryRunError) {
            msg = "DryRunError!";
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_line1_two_lines, msg); // First line for error message
            display.setFont(u8g2_font_6x12_tr); // Temporarily switch to smaller font
            msg = "Hold UP to reset"; // Second line for error message
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_line2_two_lines, msg);
            display.setFont(displayFonts[currentFontIndex]); // Switch back to selected font
            icon = signalCut; // Error icon
          } else if (isDirtyWaterError) { // <--- NEW BLOCK ADDED
            msg = "TRBDT Error!";
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_line1_two_lines, msg); // First line for error message
            display.setFont(u8g2_font_6x12_tr); // Temporarily switch to smaller font
            msg = "Hold DOWN to reset"; // Second line for error message
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_line2_two_lines, msg);
            display.setFont(displayFonts[currentFontIndex]); // Switch back to selected font
            icon = signalCut; // Error icon
          } else if (retryInProgress) {
            unsigned long elapsed = (uint32_t)(esp_timer_get_time() / 1000) - retryStartTime;
            unsigned long remaining = (retryIntervalMin * 60000UL - elapsed) / 1000;
            if (remaining < 0) remaining = 0;
            snprintf(dynamicStr, sizeof(dynamicStr), "Retry in: %luS", remaining);
            msg = dynamicStr;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg); // Only one line for retry in
            icon = hourglass; // Retry icon
          } else { // Idle: Tank Status
            const char* tankStatusStr;
            // UPDATED: Tank level display strings
            switch (currentTankLevel) {
                case TANK_FULL: tankStatusStr = "FULL"; break;
                case TANK_75: tankStatusStr = "75%";  break;
                case TANK_50: tankStatusStr = "HALF";  break;
                case TANK_25: tankStatusStr = "25%";  break;
                case TANK_EMPTY: tankStatusStr = "EMPTY"; break;
                case TANK_INVALID: tankStatusStr = "INVALID"; break;
                default: tankStatusStr = "UNKNOWN"; break;
            }
            snprintf(dynamicStr, sizeof(dynamicStr), "Tank: %s", tankStatusStr);
            msg = dynamicStr;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            icon = waterDrop; // Generic water icon
          }
          if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          break;

        case 2: // Attempts Left OR Next Schedule (Idle)
          if (dryRunError || retryInProgress) { // Attempts Left
            snprintf(dynamicStr, sizeof(dynamicStr), "Atmpt Lft: %d", remainingDryRunAttempts); // Shortened
            msg = dynamicStr;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            icon = hourglass; // Icon for attempts
          } else { // Idle: Next Schedule (only if in SCHEDULE mode)
            bool nextShown = false;
            if (pumpMode == SCHEDULE && currentProfile != PROFILE_LEVEL_INDICATOR) {
              // NEW: Use findNextSchedule to get the chronologically first schedule
              Schedule nextSchedule;
              if (findNextSchedule(nextSchedule)) {
                snprintf(dynamicStr, sizeof(dynamicStr), "Next: %02d:%02d", nextSchedule.hour, nextSchedule.minute);
                msg = dynamicStr;
                nextShown = true;
              }
            }
            if (!nextShown) { msg = "No Schedules"; } // Updated message if no schedules or not in SCHEDULE mode
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            icon = clockIcon; // Icon for next schedule
          }
          if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          break;

        case 3: // Tank Status (when error/retry active)
          {
            const char* tankStatusStr;
            // UPDATED: Tank level display strings
            switch (currentTankLevel) {
                case TANK_FULL: tankStatusStr = "FULL"; break;
                case TANK_75: tankStatusStr = "75%";  break;
                case TANK_50: tankStatusStr = "HALF";  break;
                case TANK_25: tankStatusStr = "25%";  break;
                case TANK_EMPTY: tankStatusStr = "EMPTY"; break;
                case TANK_INVALID: tankStatusStr = "INVALID"; break;
                default: tankStatusStr = "UNKNOWN"; break;
            }
            snprintf(dynamicStr, sizeof(dynamicStr), "Tank: %s", tankStatusStr);
            msg = dynamicStr;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            icon = waterDrop; // Generic water icon
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;

        // FIX: This entire section has been restructured to fix compilation errors
        // caused by a misplaced brace and duplicate case labels.
        case 5: // NEW: Turbidity value display
          {
            snprintf(dynamicStr, sizeof(dynamicStr), "Turbidity: %d", turbidityValue);
            msg = dynamicStr;
            icon = turbidityIcon;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;
        case 6: // NEW: Water Presence Status (when idle)
          {
            bool waterIsPresent = (turbidityValue > waterPresenceThreshold);
            if (waterIsPresent) {
              msg = "Water Dtctd";
              icon = waterDrop;
            } else {
              msg = "No Water";
              icon = signalCut;
            }
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;
        case 7: // NEW: Confirmed Transmitter Settings
          {
            snprintf(dynamicStr, sizeof(dynamicStr), "TxPw:%d ACK:%d", confirmedTxPower, confirmedAckAttempts);
            msg = dynamicStr;
            icon = checkMark; // Use checkmark for "confirmed"
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;
        default: // Fallback for unexpected screenState when not pumpRunning
          {
            // Default to showing tank status if nothing specific is active
            const char* tankStatusStr;
            switch (currentTankLevel) {
                case TANK_FULL: tankStatusStr = "FULL"; break;
                case TANK_75: tankStatusStr = "75%";  break;
                case TANK_50: tankStatusStr = "HALF";  break;
                case TANK_25: tankStatusStr = "25%";  break;
                case TANK_EMPTY: tankStatusStr = "EMPTY"; break;
                case TANK_INVALID: tankStatusStr = "INVALID"; break;
                default: tankStatusStr = "UNKNOWN"; break;
            }
            snprintf(dynamicStr, sizeof(dynamicStr), "Tank: %s", tankStatusStr);
            msg = dynamicStr;
            x = (128 - display.getStrWidth(msg)) / 2;
            display.drawStr(x, msg_y_single_line, msg);
            icon = waterDrop; // Generic water icon
            if (icon && showIcons) display.drawXBMP((128 - 8) / 2, icon_y, 8, 8, icon);
          }
          break;
      }
    }

    display.sendBuffer(); // Send buffer to display
}

// Function to display the settings menu on the OLED
void showSettingsMenu() {
    display.clearBuffer();

    // NEW: Apply OLED Theme
    if (currentOledTheme == LIGHT) {
        display.setColorIndex(1); // Set ink to white
        display.drawBox(0, 0, 128, 64); // Fill screen with white (paper=1)
        display.setColorIndex(0); // Set ink to black for text
    } else {
        display.setColorIndex(1); // Set ink to white for text (paper=0)
    }

    // SPECIAL CASE: Handle dynamic calibration screen
    if (currentMenuPage == PAGE_CALIBRATION && currentCalibrationState != CAL_IDLE) {
        display.setFont(u8g2_font_7x13_tf);
        switch (currentCalibrationState) {
            case CAL_CLEAN_WATER_SAMPLING: {
                display.drawStr((128 - display.getStrWidth("Calibrating Clean"))/2, 12, "Calibrating Clean");
                display.drawStr((128 - display.getStrWidth("Dip sensor in"))/2, 26, "Dip sensor in");
                display.drawStr((128 - display.getStrWidth("clean water"))/2, 40, "clean water");
                unsigned long progress = (((uint32_t)(esp_timer_get_time() / 1000) - calibrationStartTime) * 100) / CALIBRATION_SAMPLE_DURATION_MS;
                if (progress > 100) progress = 100;
                display.drawFrame(14, 50, 100, 10);
                display.drawBox(14, 50, progress, 10);
                break;
            }
            case CAL_DIRTY_WATER_SAMPLING: {
                display.drawStr((128 - display.getStrWidth("Calibrating Dirty"))/2, 12, "Calibrating Dirty");
                display.drawStr((128 - display.getStrWidth("Dip sensor in"))/2, 26, "Dip sensor in");
                display.drawStr((128 - display.getStrWidth("dirty water"))/2, 40, "dirty water");
                unsigned long progress = (((uint32_t)(esp_timer_get_time() / 1000) - calibrationStartTime) * 100) / CALIBRATION_SAMPLE_DURATION_MS;
                if (progress > 100) progress = 100;
                display.drawFrame(14, 50, 100, 10);
                display.drawBox(14, 50, progress, 10);
                break;
            }
            case CAL_DONE: {
                char buffer[25];
                display.drawStr((128 - display.getStrWidth("Calibration Done!"))/2, 12, "Calibration Done!");
                snprintf(buffer, sizeof(buffer), "Clean: %d Dirty: %d", calibratedCleanValue, calibratedDirtyValue);
                display.drawStr((128 - display.getStrWidth(buffer))/2, 28, buffer);
                snprintf(buffer, sizeof(buffer), "New Limit: %d", turbidityLimit);
                display.drawStr((128 - display.getStrWidth(buffer))/2, 44, buffer);
                display.setFont(u8g2_font_6x12_tr);
                display.drawStr((128 - display.getStrWidth("Press SET to Save"))/2, 60, "Press SET to Save");
                break;
            }
            default: break; // Should not happen
        }
        display.sendBuffer();
        return; // Skip drawing the standard menu
    }

    // Set font for menu title to a smaller, bold font
    display.setFont(u8g2_font_ncenB08_tr); // Changed to a smaller bold font
    // Determine title based on current page
    const char* title = "SETTINGS";
    switch (currentMenuPage) {
        case PAGE_MAIN_MENU: title = "SETTINGS"; break;
        case PAGE_SYSTEM_PROFILE: title = "SYSTEM PROFILE"; break;
        case PAGE_PUMP_SETTINGS: title = "PUMP SETTINGS"; break;
        case PAGE_SAFETY_SENSORS: title = "SAFETY & SENSORS"; break; // NEW
        case PAGE_SCHEDULES_MENU: title = "SCHEDULES"; break;
        case PAGE_SCHEDULE_EDIT: // NEW: Dynamic title for schedule edit page
            {
                static char schedTitle[20]; // Use a static char array for the title
                snprintf(schedTitle, sizeof(schedTitle), "EDIT SCHEDULE"); // Title is now generic
                title = schedTitle;
            }
            break;
        case PAGE_RTC_SETTINGS: title = "RTC SETTINGS"; break;
        case PAGE_DISPLAY_SETTINGS: title = "DISPLAY"; break;
        case PAGE_OLED_LAYOUT: title = "OLED LAYOUT"; break;         // NEW
        case PAGE_BUZZER_SETTINGS: title = "BUZZER SETTINGS"; break; // NEW
        case PAGE_WIFI_SETTINGS: title = "WIFI SETTINGS"; break;     // NEW
        case PAGE_LORA_SETTINGS: title = "LORA SETTINGS"; break;     // NEW
        case PAGE_CALIBRATION: title = "CALIBRATION"; break;         // NEW
        case PAGE_SYSTEM_INFO: title = "SYSTEM INFO"; break;
        case PAGE_SECURITY: title = "SECURITY"; break;               // NEW
        default: title = "MENU"; break;
    }

    // Center align the title
    int16_t title_x = (128 - display.getStrWidth(title)) / 2;
    display.drawStr(title_x, 10, title); // Menu title

    const uint8_t itemsPerPage = 4; // Number of menu items to display per page
    uint8_t pageStart = (menuIndex / itemsPerPage) * itemsPerPage; // Calculate start index for current page
    // Starting Y position for the first menu item, leaving space for the title
    int initial_item_y = 22; // Adjusted for title and some padding

    for (uint8_t i = 0; i < itemsPerPage; i++) {
      uint8_t itemActualIndex = pageStart + i;
      if (itemActualIndex >= menuPageSizes[currentMenuPage]) break; // Stop if no more items on this page
      // Changed line height from 14px to 13px
      int y_pos = initial_item_y + (i * 13); // Calculate Y position for each item, with 13px line height
      const MenuItem& currentItem = menuPages[currentMenuPage][itemActualIndex];
      char line[40]; // Use a fixed-size char array for menu lines

      // Append current value to the label based on item type and index
      if (currentItem.type == MENU_ITEM_SETTING) {
          switch (currentMenuPage) {
              case PAGE_SYSTEM_PROFILE: // NEW: Display System Profile
                  switch (currentItem.settingIndex) {
                      case 0: {
                          const char* profileName = "Unknown";
                          switch(currentProfile) {
                              case PROFILE_LEVEL_INDICATOR: profileName = "Level Indic."; break;
                              case PROFILE_MONOBLOCK:       profileName = "Monoblock"; break;
                              case PROFILE_SUBMERSIBLE:     profileName = "Submersible"; break;
                              case PROFILE_FULLY_AUTOMATIC: profileName = "Full Auto"; break;
                          }
                          snprintf(line, sizeof(line), "%s: %s", currentItem.label, profileName);
                          break;
                      }
                  }
                  break;
              case PAGE_PUMP_SETTINGS:
                  switch (currentItem.settingIndex) {
                      case 0: // Pump Mode
                          {
                            const char* modeStr = "N/A";
                            switch(pumpMode) {
                                case SCHEDULE: modeStr = "SCHEDULE"; break;
                                case AUTO: modeStr = "AUTO"; break;
                                case MANUAL: modeStr = "MANUAL"; break;
                                case WATER_SENSING: modeStr = "WATER SENSE"; break; // NEW
                            }
                            snprintf(line, sizeof(line), "%s: %s", currentItem.label, modeStr);
                          }
                          break;
                      case 1: snprintf(line, sizeof(line), "%s: %s", currentItem.label, getTankLevelString(startSensorLevel)); break; // NEW
                      case 2: snprintf(line, sizeof(line), "%s: %s", currentItem.label, getTankLevelString(endSensorLevel)); break;   // NEW
                      case 3: 
                          if (scheduleDurationMin == 0) snprintf(line, sizeof(line), "%s: UNLIMITED", currentItem.label);
                          else snprintf(line, sizeof(line), "%s: %d min", currentItem.label, scheduleDurationMin);
                          break;
                      case 4: snprintf(line, sizeof(line), "%s: %dms", currentItem.label, primingTimeMs); break;
                      case 5: snprintf(line, sizeof(line), "%s: %s", currentItem.label, forceScheduledRun ? "YES" : "NO"); break;
                      case 6: snprintf(line, sizeof(line), "%s: %s", currentItem.label, manualModeSafetyLogicEnabled ? "ON" : "OFF"); break; // NEW: Toggle manual mode safety logic
                  }
                  break;
              case PAGE_SAFETY_SENSORS: // NEW
                  switch (currentItem.settingIndex) {
                      case 0: snprintf(line, sizeof(line), "%s: %s", currentItem.label, dryRunLogicEnabled ? "ON" : "OFF"); break;
                      case 1: snprintf(line, sizeof(line), "%s: %d min", currentItem.label, dryRunDelayMin); break;
                      case 2: snprintf(line, sizeof(line), "%s: %s", currentItem.label, retryLogicEnabled ? "ON" : "OFF"); break;
                      case 3: snprintf(line, sizeof(line), "%s: %d", currentItem.label, dryRunAttempts); break;
                      case 4: snprintf(line, sizeof(line), "%s: %d min", currentItem.label, retryIntervalMin); break;
                      case 5: snprintf(line, sizeof(line), "%s: %s", currentItem.label, sensorErrorBypassEnabled ? "ON" : "OFF"); break;
                      case 6: snprintf(line, sizeof(line), "%s: %s", currentItem.label, turbidityBypassEnabled ? "ON" : "OFF"); break;
                      case 7: snprintf(line, sizeof(line), "%s: %ds", currentItem.label, turbidityTimeoutSec); break;
                      case 8: snprintf(line, sizeof(line), "%s: %d", currentItem.label, waterPresenceThreshold); break;
                      case 9: // Water Start Delay
                          {
                              uint16_t val = waterSensingStartDelaySec;
                              if (val < 60) snprintf(line, sizeof(line), "%s: %d s", currentItem.label, val);
                              else snprintf(line, sizeof(line), "%s: %dm %ds", currentItem.label, val / 60, val % 60);
                          }
                          break;
                      // FIX: Added missing case for Water Stop Delay
                      case 10: // Water Stop Delay
                          {
                              uint16_t val = waterSensingStopDelaySec;
                              if (val < 60) snprintf(line, sizeof(line), "%s: %d s", currentItem.label, val);
                              else snprintf(line, sizeof(line), "%s: %dm %ds", currentItem.label, val / 60, val % 60);
                          }
                          break;
                      case 11: snprintf(line, sizeof(line), "%s: %s", currentItem.label, autoResetDryRunOnWater ? "ON" : "OFF"); break; // NEW
                      case 12: snprintf(line, sizeof(line), "%s: %s", currentItem.label, autoResetTurbidityOnError ? "ON" : "OFF"); break; // NEW
                  }
                  break;
              case PAGE_SCHEDULE_EDIT: { // MODIFIED: Logic to display the editable schedule
                  uint8_t sched = editingScheduleIndex;
                  // NEW: Ensure blank schedules are always populated with the most current time before display.
                  if (schedules[sched].hour == 0 && schedules[sched].minute == 0 && schedules[sched].year == 0) {
                      autoPopulateBlankSchedule(sched);
                  }
                  switch (currentItem.settingIndex) {
                      case 10: { // Schedule #
                          // A "blank" schedule is identified by all-zero time and date components
                          if (schedules[sched].hour == 0 && schedules[sched].minute == 0 && schedules[sched].year == 0) {
                              snprintf(line, sizeof(line), "Schedule %d SET:None", sched + 1);
                          } else {
                              snprintf(line, sizeof(line), "Schedule %d SET:%02d:%02d", sched + 1, schedules[sched].hour, schedules[sched].minute);
                          }
                          break;
                      }
                      case 0: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, schedules[sched].hour); break;
                      case 1: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, schedules[sched].minute); break;
                      case 2: {
                          const char* rpt = "N/A";
                          if (schedules[sched].repeatType < 5) {
                            const char* repeatTypes[] = {"Daily", "Odd Day", "Even Day", "Once", "Date"};
                            rpt = repeatTypes[schedules[sched].repeatType];
                          }
                          snprintf(line, sizeof(line), "%s: %s", currentItem.label, rpt);
                          break;
                      }
                      case 3: // Year
                          if (schedules[sched].repeatType == SPECIFIC_DATE) snprintf(line, sizeof(line), "%s: %d", currentItem.label, schedules[sched].year);
                          else snprintf(line, sizeof(line), "%s: N/A", currentItem.label);
                          break;
                      case 4: // Month
                          if (schedules[sched].repeatType == SPECIFIC_DATE) snprintf(line, sizeof(line), "%s: %d", currentItem.label, schedules[sched].month);
                          else snprintf(line, sizeof(line), "%s: N/A", currentItem.label);
                          break;
                      case 5: // Day
                          if (schedules[sched].repeatType == SPECIFIC_DATE) snprintf(line, sizeof(line), "%s: %d", currentItem.label, schedules[sched].day);
                          else snprintf(line, sizeof(line), "%s: N/A", currentItem.label);
                          break;
                  }
                  break;
              }
              // --- START OF FIX: ADDED MISSING RTC SETTINGS DISPLAY LOGIC ---
              case PAGE_RTC_SETTINGS: {
                  if (isRtcWorking) {
                      DateTime now = rtc.now();
                      switch (currentItem.settingIndex) {
                          case 0: snprintf(line, sizeof(line), "%s: %d", currentItem.label, now.year()); break;
                          case 1: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, now.month()); break;
                          case 2: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, now.day()); break;
                          case 3: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, now.hour()); break;
                          case 4: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, now.minute()); break;
                          case 5: snprintf(line, sizeof(line), "%s: %02d", currentItem.label, now.second()); break;
                      }
                  } else {
                      snprintf(line, sizeof(line), "%s: Not Found", currentItem.label);
                  }
                  break;
              }
              // --- END OF FIX ---
              case PAGE_DISPLAY_SETTINGS:
                  switch (currentItem.settingIndex) {
                      case 0: snprintf(line, sizeof(line), "%s: %s", currentItem.label, displayFontNames[currentFontIndex]); break;
                      case 1: snprintf(line, sizeof(line), "%s: %s", currentItem.label, currentOledTheme == DARK ? "DARK" : "LIGHT"); break;
                  }
                  break;
              case PAGE_OLED_LAYOUT: // NEW
                  switch (currentItem.settingIndex) {
                      case 0: snprintf(line, sizeof(line), "%s: %s", currentItem.label, oledShowTime ? "ON" : "OFF"); break;
                      case 1: snprintf(line, sizeof(line), "%s: %s", currentItem.label, oledShowDate ? "ON" : "OFF"); break;
                      case 2: snprintf(line, sizeof(line), "%s: %s", currentItem.label, oledShowDay ? "ON" : "OFF"); break;
                      case 3: snprintf(line, sizeof(line), "%s: %s", currentItem.label, oledShowBattery ? "ON" : "OFF"); break;
                  }
                  break;
              case PAGE_CALIBRATION:
                  switch(currentItem.settingIndex) {
                      case 2: // Manual Limit
                          snprintf(line, sizeof(line), "%s: %d", currentItem.label, turbidityLimit);
                          break;
                  }
                  break;
              case PAGE_BUZZER_SETTINGS: // NEW: Display buzzer settings
                  switch (currentItem.settingIndex) {
                      case 0: snprintf(line, sizeof(line), "%s: %s", currentItem.label, buzzerEnabled ? "ON" : "OFF"); break;
                      case 1: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[dryRunBeepStyle]); break;
                      case 2: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[sensorErrorBeepStyle]); break;
                      case 3: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[lowBatteryBeepStyle]); break;
                      case 4: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[heartbeatExpiredBeepStyle]); break;
                      case 5: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[tankEmptyBeepStyle]); break;
                      case 6: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[tankFullBeepStyle]); break;
                      case 7: snprintf(line, sizeof(line), "%s: %s", currentItem.label, beepStyleNames[dirtyWaterBeepStyle]); break; // NEW
                      case 8: snprintf(line, sizeof(line), "%s: %d", currentItem.label, buzzerVolume); break; // NEW: Display buzzer volume
                  }
                  break;
              case PAGE_WIFI_SETTINGS: // NEW: WiFi settings
                  switch (currentItem.settingIndex) {
                      case 0: { // Show WiFi Mode and IP Address
                        char mode_ip_line[40]; // Allocate a larger buffer for this specific line
                        if (wifiMode == WIFI_STA_MODE && staConnected) {
                            IPAddress ip = WiFi.localIP();
                            snprintf(mode_ip_line, sizeof(mode_ip_line), "%s: %d.%d.%d.%d", getWifiModeString(wifiMode), ip[0], ip[1], ip[2], ip[3]);
                        } else {
                            snprintf(mode_ip_line, sizeof(mode_ip_line), "%s: %s", currentItem.label, getWifiModeString(wifiMode));
                        }
                        snprintf(line, sizeof(line), "%s", mode_ip_line);
                      }
                      break;
                      case 1: snprintf(line, sizeof(line), "%s: %s", currentItem.label, staSsid); break;
                      case 2: snprintf(line, sizeof(line), "%s: %s", currentItem.label, staPassword); break; // Display password (for dev, not secure)
                  }
                  break;
              case PAGE_LORA_SETTINGS: // NEW
                  if (currentItem.settingIndex == 0) {
                      // NEW: Add an indicator if the value is auto-managed
                      if (loraPowerOptimizationEnabled) {
                          snprintf(line, sizeof(line), "%s: %d (Auto)", currentItem.label, transmitterTxPower);
                      } else {
                          snprintf(line, sizeof(line), "%s: %d", currentItem.label, transmitterTxPower);
                      }
                  } else if (currentItem.settingIndex == 1) { // NEW
                      snprintf(line, sizeof(line), "%s: %d", currentItem.label, transmitterAckAttempts);
                  } else if (currentItem.settingIndex == 2) {
                      snprintf(line, sizeof(line), "%s: %s", currentItem.label, loraPowerOptimizationEnabled ? "ON" : "OFF");
                  }
                  break;
              case PAGE_SECURITY: // NEW
                  snprintf(line, sizeof(line), "%s", currentItem.label);
                  break;
              case PAGE_SYSTEM_INFO: {
                  switch (currentItem.settingIndex) {
                      case 0: // Version
                          snprintf(line, sizeof(line), "Version: SS-WLPC v6.0");
                          break;
                      case 1: // ID
                          snprintf(line, sizeof(line), "ID: %02X-%04X", allowedGroupID, allowedSerialID);
                          break;
                      case 2: // MAC Address
                      {
                          String mac = WiFi.macAddress();
                          snprintf(line, sizeof(line), "MAC: %s", mac.c_str());
                          break;
                      }
                      case 3: // Chip Info
                      {
                          char chipInfoStr[30];
                          snprintf(chipInfoStr, sizeof(chipInfoStr), "%s C%u R%u", ESP.getChipModel(), ESP.getChipCores(), ESP.getChipRevision());
                          snprintf(line, sizeof(line), "Chip: %s", chipInfoStr);
                          break;
                      }
                      case 4: // Free Heap
                          snprintf(line, sizeof(line), "Heap: %lu B", ESP.getFreeHeap());
                          break;
                      case 5: // Uptime
                      {
                          unsigned long seconds = (uint32_t)(esp_timer_get_time() / 1000) / 1000;
                          unsigned long days = seconds / 86400;
                          unsigned long hours = (seconds % 86400) / 3600;
                          unsigned long minutes = (seconds % 3600) / 60;
                          snprintf(line, sizeof(line), "Up: %lud %02luh %02lum", days, hours, minutes);
                          break;
                      }
                  }
                  break;
              }
          }
      } else {
          switch (currentMenuPage) {
              case PAGE_CALIBRATION:
                  // The first two items are actions, just show their label
                  snprintf(line, sizeof(line), "%s", currentItem.label);
                  break;
              default:
                  snprintf(line, sizeof(line), "%s", currentItem.label);
                  break;
          }
      }

      if (itemActualIndex == menuIndex) {
        // Highlight selected item with inverse text, but keep the same font size
        display.setFont(u8g2_font_6x12_tr); // Use regular font for selected item
        // Adjusted the y-position and height of the highlight box for better fit
        
        // NEW: Theme-aware highlight logic
        if (currentOledTheme == LIGHT) {
            // Light theme: highlight is black box, white text
            display.setColorIndex(0); // Ink = black
            display.drawBox(-2, y_pos - display.getFontAscent() - 1, 132, display.getFontAscent() - display.getFontDescent() + 2);
            display.setColorIndex(1); // Ink = white
        } else {
            // Dark theme: highlight is white box, black text
            display.setColorIndex(1); // Ink = white
            display.drawBox(-2, y_pos - display.getFontAscent() - 1, 132, display.getFontAscent() - display.getFontDescent() + 2);
            display.setColorIndex(0); // Ink = black
        }
        
        display.drawStr(5, y_pos, line); // Draw text
        
        // Reset color for next items
        if (currentOledTheme == LIGHT) {
            display.setColorIndex(0); // Back to black ink
        } else {
            display.setColorIndex(1); // Back to white ink
        }
      } else {
        // Color is already set by the theme block at the top
        display.setFont(u8g2_font_6x12_tr); // Regular font for unselected items
        display.drawStr(5, y_pos, line); // Draw text
      }
    }
    // Add display for current calibration values in idle state
    if (currentMenuPage == PAGE_CALIBRATION) {
        display.setFont(u8g2_font_5x8_tr); // Use a very small font
        char buffer[30];
        snprintf(buffer, sizeof(buffer), "C:%d D:%d L:%d", calibratedCleanValue, calibratedDirtyValue, turbidityLimit);
        display.drawStr(0, 63, buffer);
    }
    display.sendBuffer(); // Send buffer to display
}

// NEW: WiFi Mode String conversion (returns char array)
const char* getWifiModeString(WifiMode mode) {
    switch (mode) {
        case WIFI_AP_MODE: return "AP";
        case WIFI_STA_MODE: return "STA";
        default: return "UNKNOWN";
    }
}

// NEW: WiFi Mode Enum conversion
WifiMode getWifiModeEnum(const String& modeStr) {
    if (modeStr == "AP") return WIFI_AP_MODE;
    if (modeStr == "STA") return WIFI_STA_MODE;
    return WIFI_AP_MODE; // Default
}

// NEW: Get WiFi STA connection status string (returns char array)
const char* getWifiStatusString() {
    if (wifiMode == WIFI_AP_MODE) return "AP Active";
    if (staConnected) return "Connected";
    return "Disconnected";
}

// NEW: Function to connect to WiFi in STA mode
void connectToWiFiSTA() {
    ESP_LOGI(TAG, "Connecting to WiFi '%s'...\n", staSsid);
    WiFi.begin(staSsid, staPassword);
    lastWifiConnectAttempt = (uint32_t)(esp_timer_get_time() / 1000);
    // Non-blocking connection, status will be checked in loop()
}


// === NEW: Web Server Handler Implementations ===

// NEW: Handler for fetching live data for calibration UI
void handleGetLiveDataWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }

    // Use a small JSON document for just the live value
    DynamicJsonDocument doc(128);
    xSemaphoreTake(sharedDataMutex, portMAX_DELAY);
    doc["turbidityValue"] = turbidityValue;
    xSemaphoreGive(sharedDataMutex);

    String jsonString;
    serializeJson(doc, jsonString);
    server.send(200, "application/json", jsonString);
    isWebRequestActive = false;
}

// NEW: Handler for saving settings from the simple AP mode setup page
void handleApSetupSaveWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    
    // This endpoint is for initial setup, so no authentication is required.
    if (server.hasArg("staSsid") && server.hasArg("staPassword")) {
        xSemaphoreTake(sharedDataMutex, portMAX_DELAY);
        strncpy(staSsid, server.arg("staSsid").c_str(), sizeof(staSsid) - 1);
        staSsid[sizeof(staSsid) - 1] = '\0';
        
        strncpy(staPassword, server.arg("staPassword").c_str(), sizeof(staPassword) - 1);
        staPassword[sizeof(staPassword) - 1] = '\0';
        
        wifiMode = WIFI_STA_MODE; // Switch mode to Station
        xSemaphoreGive(sharedDataMutex);
        
        saveSettingsToEEPROM();
        
        server.send(200, "text/plain", "Settings saved. Restarting to connect to your WiFi...");
        esp_task_wdt_reset(); // Reset after send
        
        vTaskDelay(pdMS_TO_TICKS(1000); // Give the browser time to receive the response
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Bad Request: Missing SSID or password parameters.");
    }

    isWebRequestActive = false;
}

// NEW: Handler for resetting the dry run error state
void handleResetDryRunWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }
    xSemaphoreTake(sharedDataMutex, portMAX_DELAY);
    dryRunError = false;
    isDryRunErrorBeepActive = false; // Reset beep flag
    remainingDryRunAttempts = dryRunAttempts; // Reset attempts
    dryRunResetMessageActive = true; // NEW: Trigger display message
    dryRunResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000); // NEW: Set message start time
    xSemaphoreGive(sharedDataMutex);
    ESP_LOGI(TAG, "%s
", String("Dry run error manually reset via Web UI.");
    server.send(200, "text/plain", "Dry Run Error Reset!");
    isWebRequestActive = false;
}

// NEW: Handler for updating the Web UI theme
void handleUpdateThemeWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }
    
    if (server.hasArg("theme")) {
        currentWebTheme = server.arg("theme").toInt();
        saveSettingsToEEPROM();
        server.send(200, "text/plain", "Theme updated.");
    } else {
        server.send(400, "text/plain", "Bad Request: Missing 'theme' parameter.");
    }
    esp_task_wdt_reset(); // Reset after send

    isWebRequestActive = false;
}

// UPDATED: Function to get tank level string for web UI
// REMOVED: This function is now defined in the global scope earlier in the file.
/*
const char* getTankLevelString(TankLevel level) {
    switch (level) {
        case TANK_EMPTY: return "EMPTY";
        case TANK_25: return "25%";
        case TANK_50: return "HALF";
        case TANK_75: return "75%";
        case TANK_FULL: return "FULL";
        case TANK_INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}
*/

// FIX: Removed the entire duplicate definition of 'getTankLevelString' which was here.

const char* getPumpModeString(PumpMode mode) {
    switch (mode) {
        case SCHEDULE: return "SCHEDULE";
        case AUTO: return "AUTO";
        case MANUAL: return "MANUAL";
        case WATER_SENSING: return "WATER_SENSING"; // NEW
        default: return "UNKNOWN";
    }
}

const char* getRepeatTypeString(RepeatType type) {
    switch (type) {
        case EVERY_DAY: return "EVERY_DAY";
        case ODD_DAY: return "ODD_DAY";
        case EVEN_DAY: return "EVEN_DAY";
        case NO_REPEAT: return "NO_REPEAT";
        case SPECIFIC_DATE: return "SPECIFIC_DATE"; // NEW
        default: return "UNKNOWN";
    }
}

RepeatType getRepeatTypeEnum(const String& typeStr) {
    if (typeStr == "EVERY_DAY") return EVERY_DAY;
    if (typeStr == "ODD_DAY") return ODD_DAY;
    if (typeStr == "EVEN_DAY") return EVEN_DAY;
    if (typeStr == "NO_REPEAT") return NO_REPEAT;
    if (typeStr == "SPECIFIC_DATE") return SPECIFIC_DATE; // NEW
    return EVERY_DAY; // Default
}

const char* getBeepStyleString(BeepStyle style) {
    switch(style) {
        case BEEP_STYLE_SILENT: return "SILENT";
        case BEEP_STYLE_ALERT: return "ALERT";
        case BEEP_STYLE_WARNING: return "WARNING";
        case BEEP_STYLE_PULSE: return "PULSE";
        case BEEP_STYLE_LONG: return "LONG";
        case BEEP_STYLE_SPARROW: return "SPARROW";
        default: return "UNKNOWN";
    }
}

BeepStyle getBeepStyleEnum(const String& styleStr) {
    if (styleStr == "SILENT") return BEEP_STYLE_SILENT;
    if (styleStr == "ALERT") return BEEP_STYLE_ALERT;
    if (styleStr == "WARNING") return BEEP_STYLE_WARNING;
    if (styleStr == "PULSE") return BEEP_STYLE_PULSE;
    if (styleStr == "LONG") return BEEP_STYLE_LONG;
    if (styleStr == "SPARROW") return BEEP_STYLE_SPARROW;
    return BEEP_STYLE_ALERT; // Default to ALERT
}


// Helper function to check authentication before processing any request
bool isAuthenticated() {
    return server.authenticate(http_username, http_password);
}

// Full Web UI HTML (served in STA mode)
#define FULL_WEB_UI R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sursajni Pump Controller</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap" rel="stylesheet">
    <!-- Font Awesome for Icons -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.2/css/all.min.css" xintegrity="sha512-SnH5WK+bZxgPHs44uWIX+LLJAJ9/2PkPKZ5QiAj6Ta86w+fsb2I5VOHgL/A9H87T8e52t6d35kQ/G6uR4K8yA==" crossorigin="anonymous" referrerpolicy="no-referrer" />
    <style>
        body { font-family: 'Roboto', sans-serif; }
        .theme-dark {
            --bg-main: #111827; --bg-content-rgb: 31, 41, 55; --bg-card-rgb: 55, 65, 81; --bg-base-opacity: 0.6;
            --bg-content: rgba(var(--bg-content-rgb), var(--bg-base-opacity)); --bg-card: rgba(var(--bg-card-rgb), calc(var(--bg-base-opacity) - 0.05));
            --text-primary: #f9fafb; --text-secondary: #d1d5db; --text-accent: #93c5fd;
            --border-color: #4b5563; --accent-color: #3b82f6; --accent-hover: #60a5fa;
            --btn-green: #16a34a; --btn-red: #dc2626; --btn-blue: #2563eb;
            --status-ok: #22c55e; --status-warn: #f59e0b; --status-error: #ef4444;
            /* NEW: Enhanced text colors */
            --header-accent: #60a5fa; --text-label: #9ca3af; --text-help: #6b7280;
        }
        .theme-light {
            --bg-main: #f3f4f6; --bg-content-rgb: 255, 255, 255; --bg-card-rgb: 249, 250, 251; --bg-base-opacity: 0.6;
            --bg-content: rgba(var(--bg-content-rgb), var(--bg-base-opacity)); --bg-card: rgba(var(--bg-card-rgb), calc(var(--bg-base-opacity) - 0.05));
            --text-primary: #111827; --text-secondary: #4b5563; --text-accent: #1d4ed8;
            --border-color: #e5e7eb; --accent-color: #2563eb; --accent-hover: #3b82f6;
            --btn-green: #16a34a; --btn-red: #dc2626; --btn-blue: #2563eb;
            --status-ok: #16a34a; --status-warn: #d97706; --status-error: #b91c1c;
            /* NEW: Enhanced text colors */
            --header-accent: #1e40af; --text-label: #6b7280; --text-help: #9ca3af;
        }
        .theme-oceanic {
            --bg-main: #0c1a2e; --bg-content-rgb: 26, 42, 64; --bg-card-rgb: 40, 56, 80; --bg-base-opacity: 0.6;
            --bg-content: rgba(var(--bg-content-rgb), var(--bg-base-opacity)); --bg-card: rgba(var(--bg-card-rgb), calc(var(--bg-base-opacity) - 0.05));
            --text-primary: #e0f2fe; --text-secondary: #94a3b8; --text-accent: #7dd3fc;
            --border-color: #334155; --accent-color: #0ea5e9; --accent-hover: #38bdf8;
            --btn-green: #10b981; --btn-red: #f43f5e; --btn-blue: #3b82f6;
            --status-ok: #34d399; --status-warn: #facc15; --status-error: #fb7185;
            /* NEW: Enhanced text colors */
            --header-accent: #38bdf8; --text-label: #94a3b8; --text-help: #64748b;
        }
        .theme-sunset {
            --bg-main: #2d192b; --bg-content-rgb: 76, 42, 70; --bg-card-rgb: 94, 58, 90; --bg-base-opacity: 0.6;
            --bg-content: rgba(var(--bg-content-rgb), var(--bg-base-opacity)); --bg-card: rgba(var(--bg-card-rgb), calc(var(--bg-base-opacity) - 0.05));
            --text-primary: #fee2e2; --text-secondary: #fbcfe8; --text-accent: #f9a8d4;
            --border-color: #834c7d; --accent-color: #e11d48; --accent-hover: #f43f5e;
            --btn-green: #f59e0b; --btn-red: #e11d48; --btn-blue: #c026d3;
            --status-ok: #fbbf24; --status-warn: #f97316; --status-error: #f43f5e;
            /* NEW: Enhanced text colors */
            --header-accent: #f472b6; --text-label: #e9d5ff; --text-help: #a8a29e;
        }
        .theme-forest {
            --bg-main: #1a2e1d; --bg-content-rgb: 42, 64, 45; --bg-card-rgb: 56, 80, 59; --bg-base-opacity: 0.6;
            --bg-content: rgba(var(--bg-content-rgb), var(--bg-base-opacity)); --bg-card: rgba(var(--bg-card-rgb), calc(var(--bg-base-opacity) - 0.05));
            --text-primary: #dcfce7; --text-secondary: #bbf7d0; --text-accent: #86efac;
            --border-color: #4d6850; --accent-color: #22c55e; --accent-hover: #4ade80;
            --btn-green: #22c55e; --btn-red: #f97316; --btn-blue: #0d9488;
            --status-ok: #4ade80; --status-warn: #facc15; --status-error: #fb923c;
            /* NEW: Enhanced text colors */
            --header-accent: #4ade80; --text-label: #a7f3d0; --text-help: #88a18b;
        }
        body {
            background-image: url('https://i.ibb.co/SXXkq0WX/Gemini-Generated-Image-x2vg82x2vg82x2vg.png?updatedAt=1759915229580');
            background-size: cover;
            background-position: center;
            background-repeat: no-repeat;
            background-attachment: fixed;
            background-color: var(--bg-main); /* Fallback color */
            color: var(--text-primary);
            transition: background-color 0.5s, color 0.5s;
        }
        .bg-content { background-color: var(--bg-content); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); }
        .bg-card { background-color: var(--bg-card); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.05); }
        .text-primary { color: var(--text-primary); }
        .text-secondary { color: var(--text-secondary); }
        .text-accent { color: var(--text-accent); }
        /* NEW: Utility classes for enhanced text colors */
        .text-header-accent { color: var(--header-accent); }
        .text-label { color: var(--text-label); }
        .text-help { color: var(--text-help); }
        .border-color { border-color: var(--border-color); }
        .accent-color { background-color: var(--accent-color); }
        .accent-color-text { color: var(--accent-color); }
        .accent-hover:hover { background-color: var(--accent-hover); }
        .btn-green { background-color: var(--btn-green); }
        .btn-red { background-color: var(--btn-red); }
        .btn-blue { background-color: var(--btn-blue); }
        .status-ok { color: var(--status-ok); }
        .status-warn { color: var(--status-warn); }
        .status-error { color: var(--status-error); }
        .page { display: none; }
        .page.active { display: block; animation: fadeIn 0.5s; }
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
        .form-input, input[type=range] {
            background-color: var(--bg-main); border: 1px solid var(--border-color);
            color: var(--text-primary);
        }
        .form-input:focus, input[type=range]:focus {
            outline: none; border-color: var(--accent-color);
            box-shadow: 0 0 0 2px var(--accent-color);
        }
        .accordion-content { max-height: 0; overflow: hidden; transition: max-height 0.3s ease-out, background-color 0.3s ease-out; }

        /* NEW: Styles for active accordion */
        button.active-header {
            background-color: var(--accent-color) !important; /* Solid accent color for header */
        }
        /* Make the header's text and icon white when active */
        button.active-header span, button.active-header i {
            color: white !important; 
        }
        
        /* Highlight for the content area when open */
        /* In dark themes, make it slightly lighter */
        .theme-dark .accordion-content.open,
        .theme-oceanic .accordion-content.open,
        .theme-sunset .accordion-content.open,
        .theme-forest .accordion-content.open {
             background-color: rgba(255, 255, 255, 0.03); /* Very subtle white tint */
        }
        
        /* In light theme, make it slightly darker */
        .theme-light .accordion-content.open {
            background-color: rgba(0, 0, 0, 0.02); /* Very subtle black tint */
        }
        /* NEW: Hover effect for non-active accordion buttons */
        button[onclick*="toggleAccordion"]:not(.active-header):hover {
            background-color: rgba(255, 255, 255, 0.05);
        }
        .theme-light button[onclick*="toggleAccordion"]:not(.active-header):hover {
            background-color: rgba(0, 0, 0, 0.03);
        }
    </style>
</head>
<body class="theme-dark">
    <div class="bg-content min-h-screen flex flex-col">
        <!-- Header -->
        <header class="sticky top-0 z-10 bg-card p-4 shadow-xl flex justify-between items-center">
            <div>
                    <h1 class="text-2xl md:text-3xl font-bold text-primary">Sursajni Controller</h1>
                    <p class="text-sm text-secondary">Remote Pump & Level Monitoring</p>
            </div>
            
            <!-- REMOVED: Background Opacity Slider -->

            <!-- Increased logo size to w-16 h-16 -->
            <img src="https://i.ibb.co/NgmbHyzF/Whats-App-Image-2025-07-23-at-12-36-18-90bdfff3.png" alt="Logo" class="w-16 h-16 rounded-full">
        </header>

        <!-- Scrollable Main Content -->
        <main class="flex-grow p-4 md:p-8 overflow-y-auto">
            <div class="max-w-4xl mx-auto">
                <!-- Navigation with Icons -->
                <!-- MOVED TO BOTTOM -->
                <!--
                <nav class="flex space-x-2 md:space-x-4 border-b border-color mb-6">
                    <button id="nav-status" class="nav-btn active" onclick="showPage('status')"><i class="fas fa-chart-bar mr-2"></i>Status</button>
                    <button id="nav-schedules" class="nav-btn" onclick="showPage('schedules')"><i class="fas fa-calendar-alt mr-2"></i>Schedules</button>
                    <button id="nav-settings" class="nav-btn" onclick="showPage('settings')"><i class="fas fa-cog mr-2"></i>Settings</button>
                </nav>
                -->

                <!-- Pages Container -->
                <div>
                    <!-- Status Page -->
                    <div id="page-status" class="page active">
                        <!-- Changed from lg:grid-cols-2 to grid-cols-1 -->
                        <div class="grid grid-cols-1 gap-8">
                            <!-- Main Status Column -->
                            <div class="lg:col-span-1 space-y-8">
                                <!-- Status Grid -->
                                <h2 class="text-xl font-semibold mb-4 text-header-accent">Controls</h2>
                                <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                                    <button id="togglePumpBtn" onclick="togglePump()" class="w-full py-3 rounded-lg font-bold text-white transition-all duration-300 btn-blue">Loading...</button>
                                    <button onclick="resetDryRunError()" class="w-full py-3 rounded-lg font-bold text-white transition-all duration-300 btn-blue">Reset Dry Run</button>
                                    <button onclick="resetSensorError()" class="w-full py-3 rounded-lg font-bold text-white transition-all duration-300 btn-blue">Reset Sensor Err</button>
                                </div>
                                <!-- NEW: Status Grid (Restored) -->
                                <h2 class="text-xl font-semibold mt-8 mb-4 text-header-accent">Live Status</h2>
                                <div id="status-grid" class="grid grid-cols-2 md:grid-cols-3 gap-6">
                                    <!-- Status items will be injected here by JavaScript -->
                                </div>
                            </div>
                            <!-- NEW: System Information Card -->
                            <div class="bg-card p-4 rounded-xl shadow-xl">
                                <h2 class="text-xl font-semibold mb-4 text-header-accent">System Information</h2>
                                <div id="system-info-grid" class="grid grid-cols-2 md:grid-cols-3 gap-4 text-sm">
                                    <!-- System info items will be injected here -->
                                </div>
                            </div>
                        </div>
                        <!-- NEW: Tank Visualization Column (REMOVED) -->
                    </div>
                </div>

                <!-- Schedules Page -->
                <div id="page-schedules" class="page">
                    <div class="bg-card p-4 rounded-xl shadow-xl">
                        <h2 class="text-xl font-semibold mb-4 text-header-accent">Edit Schedules</h2>
                        <div class="space-y-4">
                            <!-- Schedule Selector -->
                            <div class="grid grid-cols-1 md:grid-cols-3 items-center gap-2">
                                <label for="scheduleSelector" class="text-label col-span-1">Select Schedule</label>
                                <div class="col-span-2">
                                    <select id="scheduleSelector" class="w-full p-2 rounded form-input"></select>
                                </div>
                            </div>
                            <!-- Time Inputs -->
                            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                                <div>
                                    <label for="schedHour" class="text-label text-sm">Hour</label>
                                    <input type="number" id="schedHour" min="0" max="23" class="w-full p-2 rounded form-input">
                                </div>
                                <div>
                                    <label for="schedMinute" class="text-label text-sm">Minute</label>
                                    <input type="number" id="schedMinute" min="0" max="59" class="w-full p-2 rounded form-input">
                                </div>
                            </div>
                            <!-- Repeat Type -->
                            <div class="grid grid-cols-1 md:grid-cols-3 items-center gap-2">
                                <label for="schedRepeatType" class="text-label col-span-1">Repeat</label>
                                <div class="col-span-2">
                                    <select id="schedRepeatType" class="w-full p-2 rounded form-input"></select>
                                </div>
                            </div>
                            <!-- Date Fields (conditionally visible) -->
                            <div id="dateFieldsContainer" class="grid grid-cols-1 md:grid-cols-3 gap-4" style="display: none;">
                                <div>
                                    <label for="schedYear" class="text-label text-sm">Year</label>
                                    <input type="number" id="schedYear" min="2024" max="2099" class="w-full p-2 rounded form-input">
                                </div>
                                <div>
                                    <label for="schedMonth" class="text-label text-sm">Month</label>
                                    <input type="number" id="schedMonth" min="1" max="12" class="w-full p-2 rounded form-input">
                                </div>
                                <div>
                                    <label for="schedDay" class="text-label text-sm">Day</label>
                                    <input type="number" id="schedDay" min="1" max="31" class="w-full p-2 rounded form-input">
                                </div>
                            </div>
                        </div>
                    </div>
                    <!-- Buttons Container -->
                    <div class="mt-8 flex flex-col space-y-4">
                        <!-- First row for clear buttons -->
                        <div class="flex justify-between items-center">
                            <button onclick="clearCurrentSchedule()" class="px-6 py-2 rounded-lg btn-red text-white font-semibold">Clear Schd</button>
                            <button onclick="clearAllSchedules()" class="px-6 py-2 rounded-lg btn-red text-white font-semibold">Clear All</button>
                        </div>
                        <!-- Second row for save button -->
                        <div class="flex justify-center">
                            <button onclick="saveCurrentSchedule()" class="px-6 py-2 rounded-lg accent-color text-white font-semibold accent-hover transition-colors">Save Schd</button>
                        </div>
                    </div>
                </div>

                <!-- Settings Page -->
                <div id="page-settings" class="page">
                    <!-- NEW: Standalone System Profile Card -->
                    <div class="bg-card p-4 rounded-xl shadow-xl mb-6">
                        <h2 class="text-xl font-semibold mb-4 text-header-accent">System Profile</h2>
                        <div id="system-profile-container">
                            <!-- System Profile row will be injected here by JS -->
                        </div>
                        <div class="flex justify-end pt-4">
                           <button onclick="saveIndividualSettings('system-profile-container')" class="px-4 py-2 text-sm rounded-lg accent-color text-white font-semibold accent-hover transition-colors">Save Setting</button>
                        </div>
                    </div>
                    
                    <div class="space-y-6">
                        <!-- Accordion Items will be injected here -->
                    </div>
                </div>
                
            </div>
        </main>
        
        <!-- NEW: Footer Message -->
        <footer class="text-center p-2 text-xs text-secondary bg-card border-t border-color">
            Developed by: Sursajni Automations © 2025 All rights reserved
        </footer>

        <!-- NEW: Bottom Navigation Bar -->
        <nav class="sticky bottom-0 z-10 bg-card shadow-inner grid grid-cols-3 gap-1 p-1 border-t border-color">
            <button id="nav-status" class="nav-btn active" onclick="showPage('status')">
                <i class="fas fa-chart-bar"></i>
                <span>Status</span>
            </button>
            <button id="nav-schedules" class="nav-btn" onclick="showPage('schedules')">
                <i class="fas fa-calendar-alt"></i>
                <span>Schedules</span>
            </button>
            <button id="nav-settings" class="nav-btn" onclick="showPage('settings')">
                <i class="fas fa-cog"></i>
                <span>Settings</span>
            </button>
        </nav>

    </div> <!-- End of .bg-content -->

    <div id="messageBox" class="fixed bottom-20 right-5 p-3 rounded-lg text-white shadow-lg" style="display:none; z-index: 50;"></div>
    
    <!-- NEW: Confirmation Modal -->
    <div id="confirmModal" class="hidden fixed inset-0 bg-black bg-opacity-70 flex items-center justify-center z-50 p-4">
            <div class="bg-card p-6 rounded-xl shadow-2xl max-w-sm w-full">
                <h3 id="confirmModalTitle" class="text-xl font-bold text-red-400 mb-4">Confirm Action</h3>
                <p id="confirmModalMessage" class="text-secondary mb-6">Are you sure?</p>
                <div class="flex justify-end space-x-4">
                    <button onclick="closeConfirmModal()" class="px-4 py-2 rounded-lg bg-gray-500 hover:bg-gray-600 text-white font-semibold">Cancel</button>
                    <button id="confirmModalButton" class="px-4 py-2 rounded-lg bg-red-600 hover:bg-red-700 text-white font-semibold">Confirm</button>
                </div>
            </div>
        </div>

    </div>

    <!-- REMOVED: Floating Save Button for Settings Page -->

<script>
    let liveDataInterval;
    let mainDataTimeout = null; // NEW: Replaces mainDataInterval
    let controllerData = {}; // Global variable to hold all settings data
    const themeClasses = ['theme-dark', 'theme-light', 'theme-oceanic', 'theme-sunset', 'theme-forest'];
    const pumpModeMap = ["SCHEDULE", "AUTO", "MANUAL", "WATER_SENSING"];
    const repeatTypeMap = ["EVERY_DAY", "ODD_DAY", "EVEN_DAY", "NO_REPEAT", "SPECIFIC_DATE"];
    const beepStyleMap = ["SILENT", "ALERT", "WARNING", "PULSE", "LONG", "SPARROW"];
    const profileMap = ["Level Indicator", "Monoblock", "Submersible", "Fully Automatic"]; // NEW
    const tankLevelMap = {0: "Empty", 1: "25%", 2: "Half", 3: "75%", 4: "Full"}; // NEW
    
    // Mapping for status icons
    const statusIcons = {
        'Time': 'fas fa-clock',
        'Pump Mode': 'fas fa-cogs',
        'Tank Level': 'fas fa-tint',
        'Signal Strength': 'fas fa-signal', // NEW: Icon for Signal Strength
        'Pump': 'fas fa-power-off', // Changed icon
        'Flow': 'fas fa-water',
        'Dry Run': 'fas fa-exclamation-triangle',
        'Sensor Error': 'fas fa-satellite-dish', // Changed icon
        'Battery': 'fas fa-battery-half', // Changed icon
        'Heartbeat': 'fas fa-heartbeat',
        'Rem. Attempts': 'fas fa-redo-alt',
        'WiFi Mode': 'fas fa-broadcast-tower',
        'WiFi Status': 'fas fa-wifi',
        'Serial ID': 'fas fa-id-card',
        'Turbidity': 'fas fa-smog',
        'Next Schedule': 'fas fa-stopwatch', // NEW: Icon for Next Schedule
        // NEW: Confirmed Transmitter Settings Icons
        'TX Cfg Power': 'fas fa-check-circle',
        'TX Cfg ACKs': 'fas fa-check-double',
        // NEW: Icons for System Information
        'Firmware': 'fas fa-code-branch',
        'Device ID': 'fas fa-microchip',
        'MAC Address': 'fas fa-ethernet',
        'Chip Info': 'fas fa-memory',
        'Free Heap': 'fas fa-server',
        'Uptime': 'fas fa-hourglass-start'
    };

    // --- NEW: UTILITY & UI-BUILDING FUNCTIONS ---

    function showPage(pageName) {
        // Hide all pages
        document.querySelectorAll('.page').forEach(page => {
            page.classList.remove('active');
        });
        // Deactivate all nav buttons
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.classList.remove('active');
        });
        
        // Show the selected page
        document.getElementById(`page-${pageName}`).classList.add('active');
        // Activate the selected nav button
        document.getElementById(`nav-${pageName}`).classList.add('active');

        // REMOVED: Logic for floating save button
    }

    function showMessage(message, type = 'success') {
        const box = document.getElementById('messageBox');
        box.textContent = message;
        box.style.display = 'block';
        if (type === 'error') {
            box.style.backgroundColor = 'var(--btn-red)';
        } else {
            box.style.backgroundColor = 'var(--btn-green)';
        }
        setTimeout(() => { box.style.display = 'none'; }, 3000);
    }

    function applyTheme(themeIndex) {
        document.body.classList.remove(...themeClasses);
        document.body.classList.add(themeClasses[themeIndex]);
        // Also update the theme selector in settings if it exists
        const themeSelector = document.getElementById('webTheme');
        if (themeSelector) {
            themeSelector.value = themeIndex;
        }
    }
    
    // NEW: Function to update the tank visualization (REMOVED)

    // REMOVED: Function to update background opacity
    /*
    function updateBgOpacity(value) {
        const opacity = value / 100;
        document.documentElement.style.setProperty('--bg-base-opacity', opacity);
        // Update the label
        const label = document.getElementById('bgOpacityLabel');
        if (label) {
            label.textContent = `${value}%`;
        }
        // Store the value in localStorage to persist it
        localStorage.setItem('bgOpacity', value);
    }
    */

    function createStatusItem(label, value, id, valueClass = '') {
        const iconClass = statusIcons[label] || 'fas fa-question-circle';
        return `
            <div class="flex items-start space-x-3">
                <i class="${iconClass} text-accent mt-1"></i>
                <div>
                    <div class="text-secondary">${label}</div>
                    <div id="${id}" class="font-bold ${valueClass}">${value}</div>
                </div>
            </div>
        `;
    }

    function createAccordionItem(id, title, icon, content, isOpen = false) {
        return `
            <div class="border border-color rounded-xl shadow-xl">
                <button class="w-full flex justify-between items-center p-4 text-left transition-colors" onclick="toggleAccordion('${id}')">
                    <div class="flex items-center space-x-3">
                        <i class="${icon} text-accent text-lg"></i>
                        <span class="font-semibold text-header-accent">${title}</span>
                    </div>
                    <i id="icon-${id}" class="fas fa-chevron-down transform transition-transform ${isOpen ? 'rotate-180' : ''}"></i>
                </button>
                <div id="content-${id}" class="accordion-content ${isOpen ? 'open' : ''}" ${isOpen ? 'style="max-height: 1000px;"' : ''}>
                    <div class="p-4 border-t border-color space-y-4">
                        ${content}
                        <div class="flex justify-end pt-2">
                           <button onclick="saveIndividualSettings('content-${id}')" class="px-4 py-2 text-sm rounded-lg accent-color text-white font-semibold accent-hover transition-colors">Save Setting</button>
                        </div>
                    </div>
                </div>
            </div>`;
    }

    function toggleAccordion(id) {
        const content = document.getElementById(`content-${id}`);
        const icon = document.getElementById(`icon-${id}`);
        const button = icon.closest('button'); // Get the button
        
        // Close all other accordions for a cleaner UI
        document.querySelectorAll('.accordion-content.open').forEach(openContent => {
            if (openContent.id !== `content-${id}`) {
                openContent.style.maxHeight = null;
                openContent.classList.remove('open');
                const otherIcon = document.getElementById(`icon-${openContent.id.replace('content-', '')}`);
                if (otherIcon) {
                    otherIcon.classList.remove('rotate-180');
                    otherIcon.closest('button').classList.remove('active-header'); // Remove active class from other button
                }
            }
        });

        if (content.style.maxHeight) {
            // It's open, so close it
            content.style.maxHeight = null;
            content.classList.remove('open');
            icon.classList.remove('rotate-180');
            button.classList.remove('active-header'); // Remove active class from this button
        } else {
            // It's closed, so open it
            content.style.maxHeight = content.scrollHeight + "px";
            content.classList.add('open');
            icon.classList.add('rotate-180');
            button.classList.add('active-header'); // Add active class to this button
        }
    }

    function createFormRow(label, inputHtml, helpText = '') {
        return `
            <div class="grid grid-cols-1 md:grid-cols-3 items-center gap-2">
                <label class="text-label col-span-1">${label}</label>
                <div class="col-span-2">
                    ${inputHtml}
                    ${helpText ? `<p class="text-xs text-help mt-1">${helpText}</p>` : ''}
                </div>
            </div>`;
    }

    function createToggleSwitch(id, isChecked) {
        return `
            <label for="${id}" class="relative inline-flex items-center cursor-pointer">
                <input type="checkbox" id="${id}" class="sr-only peer" ${isChecked ? 'checked' : ''}>
                <div class="w-11 h-6 bg-gray-600 rounded-full peer peer-focus:ring-2 peer-focus:ring-[var(--accent-color)] dark:peer-focus:ring-[var(--accent-color)] peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-0.5 after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all dark:border-gray-600 peer-checked:bg-[var(--accent-color)]"></div>
            </label>
        `;
    }
    
    // NEW: Function to toggle password visibility
    function togglePasswordVisibility(inputId, iconId) {
        const input = document.getElementById(inputId);
        const icon = document.getElementById(iconId);
        if (!input || !icon) return;

        if (input.type === 'password') {
            input.type = 'text';
            icon.classList.remove('fa-eye');
            icon.classList.add('fa-eye-slash');
        } else {
            input.type = 'password';
            icon.classList.remove('fa-eye-slash');
            icon.classList.add('fa-eye');
        }
    }
    
    // --- NEW: MAIN UI GENERATION FUNCTION ---
    
    function createSettingsForms(data) {
        const settingsContainer = document.querySelector('#page-settings .space-y-6');
        const profileContainer = document.getElementById('system-profile-container');
        if (!settingsContainer || !profileContainer) return;

        controllerData = data; // Store data globally

        // Helper to create a select element string
        const createSelect = (id, options, selectedVal) => {
            let opts = options.map((opt, i) => `<option value="${i}" ${i == selectedVal ? 'selected' : ''}>${opt.replace('_', ' ')}</option>`).join('');
            return `<select id="${id}" class="w-full p-2 rounded form-input">${opts}</select>`;
        };

        const createBeepSelect = (id, selectedVal) => {
            let opts = beepStyleMap.map(opt => `<option value="${opt}" ${opt === selectedVal ? 'selected' : ''}>${opt}</option>`).join('');
            return `<select id="${id}" class="w-full p-2 rounded form-input">${opts}</select>`;
        };

        // NEW: Create and inject System Profile setting separately
        const systemProfileHtml = createFormRow('Profile', createSelect('systemProfile', profileMap, data.systemProfile), 'Select the primary function of the device.');
        profileContainer.innerHTML = systemProfileHtml;

        const startSensorOptions = [0, 1, 2, 3].map(i => `<option value="${i}" ${i == data.startSensorLevel ? 'selected' : ''}>${tankLevelMap[i]}</option>`).join('');
        const endSensorOptions = [1, 2, 3, 4].map(i => `<option value="${i}" ${i == data.endSensorLevel ? 'selected' : ''}>${tankLevelMap[i]}</option>`).join('');

        const pumpContent = `
            ${createFormRow('Pump Mode', createSelect('pumpMode', pumpModeMap, data.pumpMode))}
            ${createFormRow('Start Snsr Lvl', `<select id="startSensorLevel" class="w-full p-2 rounded form-input">${startSensorOptions}</select>`, 'Pump starts when level drops to this.')}
            ${createFormRow('End Snsr Level', `<select id="endSensorLevel" class="w-full p-2 rounded form-input">${endSensorOptions}</select>`, 'Pump stops when level reaches this.')}
            ${createFormRow('Max Run Time (min)', `<input id="scheduleDurationMin" type="number" value="${data.scheduleDurationMin}" class="w-full p-2 rounded form-input">`, '0 for indefinite run.')}
            ${createFormRow('Priming Time (ms)', `<input id="primingTimeMs" type="number" value="${data.primingTimeMs}" class="w-full p-2 rounded form-input">`)}
            ${createFormRow('Force Schd Run', createToggleSwitch('forceScheduledRun', data.forceScheduledRun), 'Force run even if tank is not empty.')}
            ${createFormRow('ManualMode Safety', createToggleSwitch('manualModeSafetyLogicEnabled', data.manualModeSafetyLogicEnabled), 'Applies tank/flow safety in Manual mode.')}
        `;
        
        const safetyContent = `
            ${createFormRow('Dry Run Logic', createToggleSwitch('dryRunLogicEnabled', data.dryRunLogicEnabled), 'Enables/disables the entire dry run detection system.')}
            ${createFormRow('Dry Run Delay (min)', `<input id="dryRunDelayMin" type="number" value="${data.dryRunDelayMin}" class="w-full p-2 rounded form-input">`, 'Time to wait for flow before triggering a dry run.')}
            ${createFormRow('Retry Logic', createToggleSwitch('retryLogicEnabled', data.retryLogicEnabled), 'If disabled, a dry run is a final error.')}
            ${createFormRow('Retry Attempts', `<input id="dryRunAttempts" type="number" value="${data.dryRunAttempts}" class="w-full p-2 rounded form-input">`, 'Number of times to retry after a dry run.')}
            ${createFormRow('Attempt Interval (min)', `<input id="retryIntervalMin" type="number" value="${data.retryIntervalMin}" class="w-full p-2 rounded form-input">`, 'Time to wait between retry attempts.')}
            ${createFormRow('Sensor Error Bypass', createToggleSwitch('sensorErrorBypassEnabled', data.sensorErrorBypassEnabled), 'Allows pump to run despite sensor errors.')}
            ${createFormRow('Auto Reset Dry Run', createToggleSwitch('autoResetDryRunOnWater', data.autoResetDryRunOnWater), 'Automatically resets a dry run error if water is detected at the source.')}
            ${createFormRow('Auto Reset Turbidity', createToggleSwitch('autoResetTurbidityOnError', data.autoResetTurbidityOnError), 'Automatically resets a turbidity error after water disappears and then reappears.')}
            ${createFormRow('Turbidity Bypass', createToggleSwitch('turbidityBypassEnabled', data.turbidityBypassEnabled), 'Ignore turbidity readings for pump control.')}
            ${createFormRow('Turbidity Timeout (s)', `<input id="turbidityTimeoutSec" type="number" value="${data.turbidityTimeoutSec}" class="w-full p-2 rounded form-input">`, 'Time dirty water must be detected before stopping pump.')}
            <div class="mt-4 border-t border-color pt-4">
                <h3 class="text-md font-semibold text-header-accent mb-2">Water Sensing Mode</h3>
                ${createFormRow('Water Threshold', `<input id="waterPresenceThreshold" type="number" value="${data.waterPresenceThreshold}" class="w-full p-2 rounded form-input">`, 'Turbidity value above which water is considered present.')}
                ${createFormRow('Water Start Delay (s)', `<input id="waterSensingStartDelaySec" type="number" value="${data.waterSensingStartDelaySec}" class="w-full p-2 rounded form-input">`, 'Delay after detecting water before starting the pump.')}
                ${createFormRow('Water Stop Delay (s)', `<input id="waterSensingStopDelaySec" type="number" value="${data.waterSensingStopDelaySec}" class="w-full p-2 rounded form-input">`, 'Delay after water disappears before stopping the pump.')}
            </div>
        `;
        
        const buzzerContent = `
            ${createFormRow('Buzzer', createToggleSwitch('buzzerEnabled', data.buzzerEnabled))}
            ${createFormRow('Buzzer Volume', `<div class="flex items-center space-x-2"><input id="buzzerVolume" type="range" min="0" max="255" value="${data.buzzerVolume}" class="w-full" oninput="document.getElementById('buzzerVolumeLabel').textContent=this.value"><span id="buzzerVolumeLabel" class="w-8 text-center">${data.buzzerVolume}</span></div>`)}
            ${createFormRow('Dry Run Beep', createBeepSelect('dryRunBeepStyle', data.dryRunBeepStyle))}
            ${createFormRow('Sensor Error Beep', createBeepSelect('sensorErrorBeepStyle', data.sensorErrorBeepStyle))}
            ${createFormRow('Low Battery Beep', createBeepSelect('lowBatteryBeepStyle', data.lowBatteryBeepStyle))}
            ${createFormRow('Heartbeat Beep', createBeepSelect('heartbeatExpiredBeepStyle', data.heartbeatExpiredBeepStyle))}
            ${createFormRow('Tank Empty Beep', createBeepSelect('tankEmptyBeepStyle', data.tankEmptyBeepStyle))}
            ${createFormRow('Tank Full Beep', createBeepSelect('tankFullBeepStyle', data.tankFullBeepStyle))}
            ${createFormRow('Dirty Water Beep', createBeepSelect('dirtyWaterBeepStyle', data.dirtyWaterBeepStyle))}
        `;

        const displayContent = `
            ${createFormRow('OLED Theme', createSelect('oledTheme', ['Dark', 'Light'], data.oledTheme))}
            ${createFormRow('Web UI Theme', createSelect('webTheme', themeClasses.map(t => t.replace('theme-','').charAt(0).toUpperCase() + t.slice(7)), data.webTheme))}
            ${createFormRow('Web Refresh (sec)', `<div class="flex items-center space-x-2"><input id="webUiRefreshIntervalSec" type="range" min="10" max="300" step="10" value="${data.webUiRefreshIntervalSec}" class="w-full" oninput="document.getElementById('refreshIntervalLabel').textContent=this.value + 's'"><span id="refreshIntervalLabel" class="w-12 text-center">${data.webUiRefreshIntervalSec}s</span></div>`, 'How often the web dashboard updates.')}
            ${createFormRow('Display Font', createSelect('displayFontIndex', ['HelvB14', '7x13', '6x12', 'ncenB08', 'ncenR10'], data.currentFontIndex))}
            <div class="mt-4 border-t border-color pt-4">
                <h3 class="text-md font-semibold text-header-accent mb-2">OLED Layout</h3>
                ${createFormRow('Show Time', createToggleSwitch('oledShowTime', data.oledShowTime))}
                ${createFormRow('Show Date', createToggleSwitch('oledShowDate', data.oledShowDate))}
                ${createFormRow('Show Day', createToggleSwitch('oledShowDay', data.oledShowDay))}
                ${createFormRow('Show Battery', createToggleSwitch('oledShowBattery', data.oledShowBattery))}
            </div>
        `;
        
        const wifiContent = `
            ${createFormRow('WiFi Mode', createSelect('wifiMode', ['Access Point', 'Station (Client)'], data.wifiMode))}
            ${createFormRow('STA SSID', `<input id="staSsid" type="text" value="${data.staSsid}" class="w-full p-2 rounded form-input">`)}
            ${createFormRow('STA Password', `<div class="relative w-full"><input id="staPassword" type="password" placeholder="New password (optional)" class="w-full p-2 rounded form-input pr-10"><button type="button" onclick="togglePasswordVisibility('staPassword', 'staPassIcon')" class="absolute inset-y-0 right-0 px-3 flex items-center text-secondary focus:outline-none"><i id="staPassIcon" class="fas fa-eye"></i></button></div>`)}
        `;

        const loraContent = `
            ${createFormRow('Power Optimization', createToggleSwitch('loraPowerOptimizationEnabled', data.loraPowerOptimizationEnabled), 'Automatically adjust transmitter power based on signal strength.')}
            ${createFormRow('Transmitter TX Power', `<div class="flex items-center space-x-2"><input id="transmitterTxPower" type="range" min="2" max="20" value="${data.transmitterTxPower}" class="w-full" oninput="document.getElementById('txPowerLabel').textContent=this.value" ${data.loraPowerOptimizationEnabled ? 'disabled' : ''}><span id="txPowerLabel" class="w-8 text-center">${data.transmitterTxPower}</span></div>`, data.loraPowerOptimizationEnabled ? 'Power is managed automatically.' : 'Sets the transmit power (2-20) for the remote sensor unit.')}
            ${createFormRow('ACK Attempts', `<div class="flex items-center space-x-2"><input id="transmitterAckAttempts" type="range" min="1" max="10" value="${data.transmitterAckAttempts}" class="w-full" oninput="document.getElementById('ackAttemptsLabel').textContent=this.value"><span id="ackAttemptsLabel" class="w-8 text-center">${data.transmitterAckAttempts}</span></div>`, 'Number of times the transmitter will try to get an ACK from this receiver.')}
        `;

        const securityContent = `
            ${createFormRow('Web UI Password', `<div class="relative w-full"><input id="httpPassword" type="password" placeholder="New password (optional)" class="w-full p-2 rounded form-input pr-10"><button type="button" onclick="togglePasswordVisibility('httpPassword', 'httpPassIcon')" class="absolute inset-y-0 right-0 px-3 flex items-center text-secondary focus:outline-none"><i id="httpPassIcon" class="fas fa-eye"></i></button></div>`)}
        `;

        const rtcContent = `
            <div class="grid grid-cols-3 gap-2">
                ${createFormRow('Year', `<input id="rtcYear" type="number" value="${data.rtc.year}" class="w-full p-2 rounded form-input">`)}
                ${createFormRow('Month', `<input id="rtcMonth" type="number" value="${data.rtc.month}" class="w-full p-2 rounded form-input">`)}
                ${createFormRow('Day', `<input id="rtcDay" type="number" value="${data.rtc.day}" class="w-full p-2 rounded form-input">`)}
                ${createFormRow('Hour', `<input id="rtcHour" type="number" value="${data.rtc.hour}" class="w-full p-2 rounded form-input">`)}
                ${createFormRow('Minute', `<input id="rtcMinute" type="number" value="${data.rtc.minute}" class="w-full p-2 rounded form-input">`)}
                ${createFormRow('Second', `<input id="rtcSecond" type="number" value="${data.rtc.second}" class="w-full p-2 rounded form-input">`)}
            </div>
            <div class="text-center text-secondary text-sm mt-4">Current Time: ${data.currentTime}</div>
        `;

        const calibrationContent = `
            ${createFormRow('Turbidity Limit', `<input id="turbidityLimit" type="number" value="${data.turbidityLimit}" class="w-full p-2 rounded form-input">`)}
            <div class="mt-4 border-t border-color pt-4">
                 <div class="text-lg font-bold text-center text-header-accent mb-2">Live Calibration</div>
                 <div class="text-center mb-4">Live Reading: <span id="liveTurbidityValue" class="font-bold text-xl">...</span></div>
                 <div class="grid grid-cols-2 gap-4">
                    <button onclick="setTurbidityLimitFromLive()" class="w-full py-2 rounded-lg btn-blue text-white">Set Limit to Live</button>
                    <button onclick="startCalibration()" class="w-full py-2 rounded-lg btn-green text-white">Start Auto-Calibrate</button>
                 </div>
                 <div class="text-sm text-secondary mt-2 text-center">Auto-calibration will guide you to set clean and dirty water levels.</div>
                 <div class="grid grid-cols-2 gap-4 mt-2 text-center text-sm">
                    <div>Clean Level: <span id="calibratedCleanValueUI">${data.calibratedCleanValue}</span></div>
                    <div>Dirty Level: <span id="calibratedDirtyValueUI">${data.calibratedDirtyValue}</span></div>
                 </div>
            </div>
        `;

        const systemContent = `
            <div class="p-4 bg-red-900 bg-opacity-50 rounded-lg">
                <h3 class="text-lg font-semibold text-red-300">Danger Zone</h3>
                <p class="text-sm text-red-200 mt-1 mb-4">These actions are destructive and cannot be undone.</p>
                <button onclick="factoryReset()" class="w-full py-2 rounded-lg btn-red text-white font-bold">Factory Reset</button>
            </div>
        `;


        settingsContainer.innerHTML =
            createAccordionItem('pump', 'Pump Settings', 'fas fa-sliders-h', pumpContent, true) +
            createAccordionItem('safety', 'Safety & Sensors', 'fas fa-shield-alt', safetyContent) +
            createAccordionItem('buzzer', 'Buzzer & Alerts', 'fas fa-bell', buzzerContent) +
            createAccordionItem('turbidity', 'Turbidity Sensor', 'fas fa-smog', calibrationContent) +
            createAccordionItem('display', 'Display & Theme', 'fas fa-desktop', displayContent) +
            createAccordionItem('wifi', 'WiFi & Network', 'fas fa-wifi', wifiContent) +
            createAccordionItem('lora', 'LoRa Settings', 'fas fa-broadcast-tower', loraContent) + // NEW LoRa Accordion
            createAccordionItem('security', 'Security', 'fas fa-shield-alt', securityContent) +
            createAccordionItem('rtc', 'Time & Date (RTC)', 'fas fa-clock', rtcContent) +
            createAccordionItem('system', 'System Actions', 'fas fa-power-off', systemContent);
            
        // Add event listener for Web Theme selector to apply theme instantly
        document.getElementById('webTheme').addEventListener('change', (e) => applyTheme(e.target.value));

        // NEW: Add event listener to enable/disable LoRa power slider
        document.getElementById('loraPowerOptimizationEnabled').addEventListener('change', (e) => {
            const powerSlider = document.getElementById('transmitterTxPower');
            const powerHelpText = powerSlider.closest('.grid').querySelector('.text-help');
            if (e.target.checked) {
                powerSlider.disabled = true;
                if (powerHelpText) powerHelpText.textContent = 'Power is managed automatically.';
            } else {
                powerSlider.disabled = false;
                if (powerHelpText) powerHelpText.textContent = 'Sets the transmit power (2-20) for the remote sensor unit.';
            }
        });

        // NEW: Add event listeners for start/end sensor validation
        document.getElementById('startSensorLevel').addEventListener('change', (e) => {
            const startVal = parseInt(e.target.value);
            const endSelect = document.getElementById('endSensorLevel');
            const endVal = parseInt(endSelect.value);
            if (startVal >= endVal) {
                endSelect.value = Math.min(startVal + 1, 4); // Clamp value to max 4 (Full)
            }
        });
        document.getElementById('endSensorLevel').addEventListener('change', (e) => {
            const endVal = parseInt(e.target.value);
            const startSelect = document.getElementById('startSensorLevel');
            const startVal = parseInt(startSelect.value);
            if (endVal <= startVal) {
                startSelect.value = Math.max(endVal - 1, 0); // Clamp value to min 0 (Empty)
            }
        });
        
        // NEW: Add event listener for System Profile to update UI visibility
        document.getElementById('systemProfile').addEventListener('change', (e) => updateUiForProfile(e.target.value));
        updateUiForProfile(data.systemProfile); // Initial call to set UI state


        // Start polling for live data now that the calibration UI exists
        if (!liveDataInterval) {
            liveDataInterval = setInterval(fetchLiveData, 2000);
        }
    }

    async function fetchLiveData() {
        try {
            const response = await fetch('/get_live_data');
            const data = await response.json();
            const liveValueEl = document.getElementById('liveTurbidityValue');
            if (liveValueEl) {
                liveValueEl.textContent = data.turbidityValue;
            }
        } catch (error) {
            console.error('Error fetching live data:', error);
        }
    }

    // --- NEW: FUNCTION TO UPDATE UI BASED ON PROFILE ---
    function updateUiForProfile(profileIndex) {
        const profile = parseInt(profileIndex);
        
        // Get the entire accordion content divs
        const pumpSettingsContent = document.getElementById('content-pump');
        const schedulesPage = document.getElementById('page-schedules');
        const turbidityContent = document.getElementById('content-turbidity');
        const pumpControls = document.getElementById('togglePumpBtn').parentElement.parentElement;

        // Hide/show entire sections
        if (profile === 0) { // Level Indicator
            if (pumpSettingsContent) pumpSettingsContent.parentElement.style.display = 'none';
            if (schedulesPage) document.getElementById('nav-schedules').style.display = 'none';
            if (turbidityContent) turbidityContent.parentElement.style.display = 'none';
            if (pumpControls) pumpControls.style.display = 'none';
        } else {
            if (pumpSettingsContent) pumpSettingsContent.parentElement.style.display = 'block';
            if (schedulesPage) document.getElementById('nav-schedules').style.display = 'flex'; // Use flex for the nav button
             if (turbidityContent) turbidityContent.parentElement.style.display = 'block';
            if (pumpControls) pumpControls.style.display = 'block';

            // Fine-tune visibility within the Pump Settings section
            const primingTimeRow = document.getElementById('primingTimeMs')?.closest('.grid');
            if(primingTimeRow) {
                if (profile === 1) { // Monoblock
                    primingTimeRow.style.display = 'none';
                } else { // Submersible or Full Auto
                    primingTimeRow.style.display = 'grid';
                }
            }
        }
    }


    // --- NEW: SCHEDULES PAGE FUNCTIONS ---
    
    function initializeSchedulesPage(data) {
        const selector = document.getElementById('scheduleSelector');
        const repeatTypeSelect = document.getElementById('schedRepeatType');
        
        // Store current selection to restore it later, preventing UI jumps
        const currentIndex = selector.value || '0';

        // Repopulate selector every time to reflect current schedule times
        selector.innerHTML = '';
        for (let i = 0; i < 10; i++) {
            const schedule = data.schedules[i];
            const isBlankSchedule = schedule.hour === 0 && schedule.minute === 0 && schedule.year === 0;
            let label;
            if (isBlankSchedule) {
                label = `Schedule ${i + 1} (SET: None)`;
            } else {
                const hour = String(schedule.hour).padStart(2, '0');
                const minute = String(schedule.minute).padStart(2, '0');
                label = `Schedule ${i + 1} (SET: ${hour}:${minute})`;
            }
            selector.add(new Option(label, i));
        }
        
        // Restore previous selection
        selector.value = currentIndex;
        
        if (repeatTypeSelect.options.length === 0) { // Only populate this and add listeners once
            repeatTypeMap.forEach(type => {
                repeatTypeSelect.add(new Option(type.replace('_', ' '), type));
            });
            selector.addEventListener('change', () => updateScheduleForm(selector.value));
            repeatTypeSelect.addEventListener('change', () => {
                document.getElementById('dateFieldsContainer').style.display = (repeatTypeSelect.value === 'SPECIFIC_DATE') ? 'grid' : 'none';
            });
        }
        
        // Update form with data for the selected (or default) schedule
        updateScheduleForm(selector.value);
    }

    function updateScheduleForm(index) {
        const schedule = controllerData.schedules[index];
        const rtc = controllerData.rtc;

        // A "blank" schedule is one that has default (zero) values.
        const isBlankSchedule = schedule.hour === 0 && schedule.minute === 0 && schedule.year === 0;

        if (isBlankSchedule && rtc) {
            // Auto-populate with current RTC time for new/blank schedules.
            document.getElementById('schedHour').value = rtc.hour;
            document.getElementById('schedMinute').value = rtc.minute;
            // Default to SPECIFIC_DATE to make the date fields visible and pre-filled.
            document.getElementById('schedRepeatType').value = 'SPECIFIC_DATE';
            document.getElementById('schedYear').value = rtc.year;
            document.getElementById('schedMonth').value = rtc.month;
            document.getElementById('schedDay').value = rtc.day;
        } else {
            // Load existing schedule data.
            document.getElementById('schedHour').value = schedule.hour;
            document.getElementById('schedMinute').value = schedule.minute;
            document.getElementById('schedRepeatType').value = schedule.repeatType;
            document.getElementById('schedYear').value = schedule.year;
            document.getElementById('schedMonth').value = schedule.month;
            document.getElementById('schedDay').value = schedule.day;
        }
        
        // Show/hide date fields based on the currently selected repeat type in the dropdown.
        const currentRepeatType = document.getElementById('schedRepeatType').value;
        document.getElementById('dateFieldsContainer').style.display = (currentRepeatType === 'SPECIFIC_DATE') ? 'grid' : 'none';
    }

    async function saveCurrentSchedule() {
        const index = document.getElementById('scheduleSelector').value;
        const formData = new FormData();
        
        formData.append(`sched${index}Hour`, document.getElementById('schedHour').value);
        formData.append(`sched${index}Minute`, document.getElementById('schedMinute').value);
        formData.append(`sched${index}RepeatType`, document.getElementById('schedRepeatType').value);
        formData.append(`sched${index}Year`, document.getElementById('schedYear').value);
        formData.append(`sched${index}Month`, document.getElementById('schedMonth').value);
        formData.append(`sched${index}Day`, document.getElementById('schedDay').value);

        try {
            const response = await fetch('/update_all_settings', { method: 'POST', body: formData });
            if (response.ok) {
                showMessage(`Schedule ${parseInt(index) + 1} saved successfully!`);
                fetchAndUpdate(); // Refresh all data to ensure consistency
            } else {
                showMessage('Failed to save schedule.', 'error');
            }
        } catch (error) {
            console.error('Error saving schedule:', error);
            showMessage('Error saving schedule.', 'error');
        }
    }


    // --- MAIN DATA FETCH & ACTION FUNCTIONS ---

    async function fetchAndUpdate() {
        // Clear any pending timeout to prevent race conditions if called manually
        if (mainDataTimeout) {
            clearTimeout(mainDataTimeout);
        }
        try {
            const response = await fetch('/get_settings');
            if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
            const data = await response.json();
            controllerData = data; // Update global data cache

            applyTheme(data.webTheme);

            const statusGrid = document.getElementById('status-grid');
            if (statusGrid) {
                // --- BUG FIX START ---
                // The logic for setting the color of the signal strength and assigning
                // the HTML to the status grid was syntactically incorrect, causing a
                // JavaScript error that broke all UI interactivity.
                // This has been corrected by separating the logic and fixing the assignment.

                // 1. Determine the color class for the RSSI value first.
                let rssiClass = 'status-ok';
                if (data.signalStrength <= -90) { // More severe signal loss
                    rssiClass = 'status-error';
                } else if (data.signalStrength <= -80) {
                    rssiClass = 'status-warn';
                }
 
                // 2. Build and assign the complete HTML for the status grid.
                // REINSTATED: Populate the detailed status grid
                statusGrid.innerHTML =
                    createStatusItem('Time', data.currentTime, 'statusTime') +
                    createStatusItem('Pump Mode', pumpModeMap[data.pumpMode], 'statusPumpMode') +
                    createStatusItem('Tank Level', data.currentTankLevel, 'statusTankLevel') +
                    createStatusItem('Signal Strength', `${data.signalStrength} dBm`, 'statusSignalStrength', rssiClass) +
                    createStatusItem('Pump', data.pumpRunning ? 'ON' : 'OFF', 'statusPumpStatus', data.pumpRunning ? 'status-ok' : '') +
                    createStatusItem('Flow', data.flowOk ? 'OK' : 'NO', 'statusFlowOk', data.flowOk ? 'status-ok' : 'status-warn') +
                    createStatusItem('Dry Run', data.dryRunError ? 'ERROR' : 'OK', 'statusDryRunError', data.dryRunError ? 'status-error' : 'status-ok') +
                    createStatusItem('Sensor Error', data.sensorError.replace('SENSOR_ERROR_',''), 'statusSensorError', data.sensorError !== 'SENSOR_ERROR_NONE' ? 'status-error' : 'status-ok') +
                    createStatusItem('Turbidity', data.turbidityValue, 'statusTurbidity', data.isWaterDirty ? 'status-warn' : 'status-ok') +
                    createStatusItem('Battery', `${data.batteryVoltage.toFixed(2)} V`, 'statusBattery', data.batteryVoltage < 3.3 ? 'status-warn' : 'status-ok') +
                    createStatusItem('Heartbeat', data.heartbeatExpired ? 'EXPIRED' : 'OK', 'statusHeartbeat', data.heartbeatExpired ? 'status-error' : 'status-ok') +
                    // NEW: Add Confirmed Transmitter Settings
                    createStatusItem('TX Cfg Power', data.confirmedTxPower, 'statusConfirmedTxPower') +
                    createStatusItem('TX Cfg ACKs', data.confirmedAckAttempts, 'statusConfirmedAckAttempts') +
                    createStatusItem('Next Schedule', data.nextSchedule ? data.nextSchedule : 'None', 'statusNextSchedule');

                // --- BUG FIX END ---
            }
            
            // NEW: Update tank visualization (REMOVED)

            // NEW: Populate System Information Page
            const systemInfoGrid = document.getElementById('system-info-grid');
            if (systemInfoGrid) {
                systemInfoGrid.innerHTML = 
                    createStatusItem('Firmware', data.firmwareVersion, 'sysinfoFirmware') +
                    createStatusItem('Device ID', data.deviceId, 'sysinfoDeviceId') +
                    createStatusItem('MAC Address', data.macAddress, 'sysinfoMac') +
                    createStatusItem('Chip Info', data.chipInfo, 'sysinfoChip') +
                    createStatusItem('Free Heap', data.freeHeap, 'sysinfoHeap') +
                    createStatusItem('Uptime', data.uptime, 'sysinfoUptime');
            }

            const toggleBtn = document.getElementById('togglePumpBtn');
            toggleBtn.textContent = data.pumpRunning ? 'Stop Pump' : 'Start Pump';
            toggleBtn.className = `w-full py-3 rounded-lg font-bold text-white transition-all duration-300 ${data.pumpRunning ? 'btn-red' : 'btn-green'}`;
            // Disable button if not in Manual, Auto, or Water Sensing mode
            toggleBtn.disabled = !(data.pumpMode === 2 || data.pumpMode === 1 || data.pumpMode === 3);

            // Populate Settings Forms (if not already populated)
            if (!document.getElementById('pumpMode')) {
                 createSettingsForms(data);
            }
            
            // Initialize schedules page
            initializeSchedulesPage(data);

            // Update all setting values
            Object.keys(data).forEach(key => {
                const el = document.getElementById(key);
                if (el) {
                    if (el.type === 'checkbox') {
                        // This logic is correct. data[key] is a boolean (true/false)
                        // which directly sets the checkbox state.
                        el.checked = data[key];
                    } else {
                        // This will handle regular inputs and selects
                        el.value = data[key];
                    }
                }
            });

            // Special handling for elements that need updates
            if(document.getElementById('buzzerVolumeLabel')) document.getElementById('buzzerVolumeLabel').textContent = data.buzzerVolume;
            if(document.getElementById('calibratedCleanValueUI')) document.getElementById('calibratedCleanValueUI').textContent = data.calibratedCleanValue;
            if(document.getElementById('calibratedDirtyValueUI')) document.getElementById('calibratedDirtyValueUI').textContent = data.calibratedDirtyValue;
            if(document.getElementById('txPowerLabel')) document.getElementById('txPowerLabel').textContent = data.transmitterTxPower; // NEW: Update LoRa TX power label
            if(document.getElementById('ackAttemptsLabel')) document.getElementById('ackAttemptsLabel').textContent = data.transmitterAckAttempts; // NEW: Update ACK attempts label
            const powerSlider = document.getElementById('transmitterTxPower'); // NEW
            if (powerSlider) {
                powerSlider.disabled = data.loraPowerOptimizationEnabled;
                const powerHelpText = powerSlider.closest('.grid').querySelector('.text-help');
                if (powerHelpText) powerHelpText.textContent = data.loraPowerOptimizationEnabled ? 'Power is managed automatically.' : 'Sets the transmit power (2-20) for the remote sensor unit.';
            }
            if(document.getElementById('refreshIntervalLabel')) document.getElementById('refreshIntervalLabel').textContent = data.webUiRefreshIntervalSec + 's'; // Update refresh label

            
            // Schedule the next update
            const refreshMs = (data.webUiRefreshIntervalSec || 10) * 1000;
            mainDataTimeout = setTimeout(fetchAndUpdate, refreshMs);
             
        } catch (error) {
            console.error('Error fetching settings:', error);
            showMessage('Failed to fetch settings.', 'error');
            // In case of error, retry after a default interval (e.g., 15 seconds)
            mainDataTimeout = setTimeout(fetchAndUpdate, 15000);
        }
    }

    async function saveAllSettings() {
        const formData = new FormData();
        
        document.querySelectorAll('#page-settings input, #page-settings select, #page-security input').forEach(element => {
            formData.append(element.id, element.value);
        });

        try {
            const response = await fetch('/update_all_settings', { method: 'POST', body: formData });
            if (response.ok) {
                showMessage('All settings saved successfully!');
                fetchAndUpdate();
            } else {
                showMessage('Failed to save settings.', 'error');
            }
        } catch (error) {
            console.error('Error saving settings:', error);
            showMessage('Error saving settings.', 'error');
        }
    }

    async function saveIndividualSettings(accordionId) {
        const formData = new FormData();
        const accordionElement = document.getElementById(accordionId);
        
        accordionElement.querySelectorAll('input, select').forEach(element => {
            if (element.type === 'checkbox') {
                formData.append(element.id, element.checked ? '1' : '0');
            } else {
                formData.append(element.id, element.value);
            }
        });
        
        // --- MODIFICATION START ---
        // Convert FormData to URLSearchParams for correct encoding, matching saveAllSettingsFromFloat
        const urlEncodedData = new URLSearchParams(formData);

        try {
            // Send data as 'application/x-www-form-urlencoded'
            const response = await fetch('/update_all_settings', {
                 method: 'POST',
                 headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                 },
                 body: urlEncodedData // Send the URL-encoded data
            });
            // --- MODIFICATION END ---
            
            if (response.ok) {
                // Get the title from the button that opens the accordion
                const title = accordionElement.previousElementSibling?.querySelector('span')?.textContent || 'Settings';
                showMessage(`${title} saved successfully!`);
                fetchAndUpdate();
            } else {
                 const errorText = await response.text();
                 showMessage(`Failed to save ${accordionElement.previousElementSibling?.querySelector('span')?.textContent || 'settings'}: ${errorText}`, 'error');
            }
        } catch (error) {
            console.error('Error saving settings:', error);
            showMessage('Error saving settings.', 'error');
        }
    }
    
    // This function is no longer needed as each section is saved individually, or via the new schedule save button.
    /*
    async function saveAllSchedules() {
        const formData = new FormData();
        for (let i = 0; i < 10; i++) { // MODIFIED: Loop through all 10 schedules
            formData.append(`sched${i}Hour`, document.getElementById(`sched${i}Hour`).value);
            formData.append(`sched${i}Minute`, document.getElementById(`sched${i}Minute`).value);
            formData.append(`sched${i}Second`, document.getElementById(`sched${i}Second`).value);
            formData.append(`sched${i}RepeatType`, document.getElementById(`sched${i}RepeatType`).value);
            // NEW: Add date fields to the form data
            formData.append(`sched${i}Year`, document.getElementById(`sched${i}Year`).value);
            formData.append(`sched${i}Month`, document.getElementById(`sched${i}Month`).value);
            formData.append(`sched${i}Day`, document.getElementById(`sched${i}Day`).value);
        }
        try {
            const response = await fetch('/update_all_settings', { method: 'POST', body: new URLSearchParams(formData) });
            if (response.ok) {
                showMessage(`Schedule ${parseInt(index) + 1} saved successfully!`);
                fetchAndUpdate(); // Refresh all data to ensure consistency
    async function saveAllSettings() {
        const formData = new FormData();
        
        document.querySelectorAll('#page-settings input, #page-settings select, #page-security input').forEach(element => {
            formData.append(element.id, element.value);
        });

        try {
            const response = await fetch('/update_all_settings', { method: 'POST', body: new URLSearchParams(formData) });
            if (response.ok) {
                showMessage('All settings saved successfully!');
                fetchAndUpdate();
            } else {
                showMessage('Failed to save schedules.', 'error');
            }
        } catch (error) {
            console.error('Error saving schedules:', error);
            showMessage('Error saving schedules.', 'error');
        }
    }
    */

    // NEW: Functions to handle a custom confirmation modal, replacing confirm()
    function showConfirmModal(title, message, onConfirmCallback) {
        document.getElementById('confirmModalTitle').textContent = title;
        document.getElementById('confirmModalMessage').textContent = message;
        
        const confirmBtn = document.getElementById('confirmModalButton');
        const newConfirmBtn = confirmBtn.cloneNode(true);
        confirmBtn.parentNode.replaceChild(newConfirmBtn, confirmBtn);
        
        newConfirmBtn.onclick = onConfirmCallback;
        
        document.getElementById('confirmModal').classList.remove('hidden');
    }

    function closeConfirmModal() {
        document.getElementById('confirmModal').classList.add('hidden');
    }

    async function clearCurrentSchedule() {
        const index = document.getElementById('scheduleSelector').value;
        showConfirmModal(
            `Clear Schedule ${parseInt(index) + 1}?`,
            'This will reset this schedule. This action cannot be undone.',
            async () => {
                closeConfirmModal();
                try {
                    const response = await fetch('/update_all_settings', { method: 'POST', body: new URLSearchParams({ action: 'clear_schedule', index: index }) });
                    if (response.ok) {
                        showMessage(`Schedule ${parseInt(index) + 1} cleared!`);
                        fetchAndUpdate();
                    } else { showMessage('Failed to clear schedule.', 'error'); }
                } catch (e) { showMessage('Error clearing schedule.', 'error'); }
            }
        );
    }

    async function clearAllSchedules() {
        showConfirmModal(
            'Clear All Schedules?',
            'This will reset all 10 schedules to 00:00:00. This action cannot be undone.',
            async () => {
                closeConfirmModal();
                try {
                    const response = await fetch('/update_all_settings', { method: 'POST', body: new URLSearchParams({ action: 'clear_schedules' }) });
                    if (response.ok) {
                        showMessage('All schedules cleared!');
                        fetchAndUpdate();
                    } else { showMessage('Failed to clear schedules.', 'error'); }
                } catch (e) { showMessage('Error clearing schedules.', 'error'); }
            }
        );
    }
    
    async function factoryReset() {
        showConfirmModal(
            'Confirm Factory Reset',
            'This will erase all custom settings, including schedules, pump configurations, and WiFi credentials. The device will revert to its original state and restart.',
            async () => {
                closeConfirmModal();
                try {
                    const response = await fetch('/factory_reset', { method: 'POST' });
                    if (response.ok) {
                        showMessage('Factory reset successful. Device is restarting.', 'success');
                        document.querySelectorAll('button, input, select').forEach(el => el.disabled = true);
                    } else { 
                        showMessage('Failed to perform factory reset.', 'error'); 
                    }
                } catch (e) { 
                    showMessage('Error performing factory reset.', 'error'); 
                }
            }
        );
    }

    async function sendAction(endpoint) {
        try {
            const response = await fetch(endpoint, { method: 'POST' });
            if (response.ok) {
                showMessage(await response.text(), 'success');
                fetchAndUpdate();
            } else { showMessage(await response.text(), 'error'); }
        } catch (e) { showMessage(`Error performing action.`, 'error'); }
    }

    function togglePump() { sendAction('/toggle_pump'); }
    function resetDryRunError() { sendAction('/reset_dry_run'); }
    function resetSensorError() { sendAction('/reset_sensor_error'); }

    // NEW: Function to set the turbidity limit input to the current live value
    function setTurbidityLimitFromLive() {
        const liveValueEl = document.getElementById('liveTurbidityValue');
        const limitInputEl = document.getElementById('turbidityLimit');
        
        if (liveValueEl && limitInputEl) {
            const liveValue = liveValueEl.textContent.trim();
            // Check if the value is a valid number (not '...')
            if (liveValue && !isNaN(liveValue) && liveValue !== '...') {
                limitInputEl.value = liveValue;
                showMessage('Limit field updated to live value. Click "Save Setting" to confirm.', 'success');
            } else {
                showMessage('Waiting for a valid live reading...', 'error');
            }
        }
    }

    // NEW: Stub function for the auto-calibration button
    function startCalibration() {
        showMessage('Auto-calibration must be performed using the OLED menu on the device itself.', 'success');
    }

    document.addEventListener('DOMContentLoaded', () => {
        // createSchedulesForm(); // OLD logic removed
        
        // REMOVED: Opacity slider logic
        /*
        // NEW: Load and apply saved background opacity
        const savedOpacityValue = localStorage.getItem('bgOpacity') || '90';
        const slider = document.getElementById('bgOpacitySlider');
        if (slider) {
            slider.value = savedOpacityValue;
        }
        updateBgOpacity(savedOpacityValue);
        */

        fetchAndUpdate(); // Initial call, the function will now manage its own timer
        // FIX: Reduced the data refresh interval from 50 seconds to 5 seconds for a much more responsive UI.
        // setInterval(fetchAndUpdate, 5000); // Main data update every 5 seconds for responsiveness -- OLD METHOD
    });
</script>
<style>
/* NEW: Styles for the bottom navigation bar */
.nav-btn {
    display: flex;
    flex-direction: column; /* Stack icon and text */
    align-items: center;
    justify-content: center;
    padding: 8px 4px; /* Smaller padding */
    border-top: 3px solid transparent; /* Top border for active state */
    color: var(--text-secondary);
    transition: all 0.3s;
    font-size: 0.75rem; /* text-xs */
    line-height: 1rem; /* leading-4 */
    border-radius: 0.25rem; /* rounded-md */
}
.nav-btn.active {
    color: var(--text-accent);
    border-top-color: var(--accent-color); /* Use top border */
    background-color: var(--bg-main); /* Slight bg change for active */
}
.nav-btn:not(.active):hover {
    color: var(--text-primary);
    background-color: var(--bg-main);
}
.nav-btn i {
    font-size: 1.125rem; /* text-lg */
    margin-bottom: 2px; /* Space between icon and text */
}
</style>
</body>
</html>
)rawliteral"


// A new, simpler UI specifically for AP mode to allow configuration.
#define AP_WEB_UI R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sursajni Controller Setup</title>
    <!-- NEW: Added Font Awesome for the eye icon -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.2/css/all.min.css">
    <style>
        body { 
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: #111827; 
            color: #f9fafb;
            display: flex; 
            justify-content: center; 
            align-items: center; 
            min-height: 100vh;
            margin: 0;
            padding: 1rem;
            box-sizing: border-box;
        }
        .container {
            background-color: #1f2937;
            padding: 2rem;
            border-radius: 0.75rem;
            box-shadow: 0 10px 15px -3px rgba(0,0,0,0.1), 0 4px 6px -2px rgba(0,0,0,0.05);
            width: 100%;
            max-width: 400px;
            text-align: center;
        }
        h1 {
            font-size: 1.5rem;
            font-weight: bold;
            margin-bottom: 1rem;
        }
        p {
            color: #d1d5db;
            margin-bottom: 1.5rem;
        }
        .form-group {
            margin-bottom: 1rem;
            text-align: left;
        }
        label {
            display: block;
            color: #d1d5db;
            margin-bottom: 0.25rem;
            font-size: 0.875rem;
        }
        .form-input {
            width: 100%;
            padding: 0.75rem;
            border-radius: 0.5rem;
            border: 1px solid #4b5563;
            background-color: #374151;
            color: #f9fafb;
            box-sizing: border-box;
        }
        .form-input:focus {
            outline: none;
            border-color: #3b82f6;
            box-shadow: 0 0 0 2px #3b82f6;
        }
        /* NEW: Styles for password toggle icon */
        .relative {
            position: relative;
        }
        .password-toggle-btn {
            position: absolute;
            top: 0;
            right: 0;
            bottom: 0;
            background: none;
            border: none;
            color: #9ca3af; /* gray-400 */
            cursor: pointer;
            padding: 0 0.75rem;
            display: flex;
            align-items: center;
        }
        .pr-10 {
             padding-right: 2.5rem; /* 40px */
        }
        .btn {
            width: 100%;
            padding: 0.75rem;
            border-radius: 0.5rem;
            background-color: #3b82f6;
            color: white;
            font-weight: 600;
            border: none;
            cursor: pointer;
            transition: background-color 0.2s;
        }
        .btn:hover {
            background-color: #2563eb;
        }
        .footer-text {
            font-size: 0.75rem;
            color: #6b7280;
            margin-top: 1.5rem;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Sursajni Controller Setup</h1>
        <p>Connect to your home network to access the full control panel.</p>
        <form id="wifi-form">
            <div class="form-group">
                <label for="ssid">Wi-Fi Network (SSID)</label>
                <input type="text" id="ssid" name="staSsid" class="form-input" required>
            </div>
            <div class="form-group">
                <label for="password">Wi-Fi Password</label>
                <!-- NEW: Added wrapper and toggle button for password visibility -->
                <div class="relative">
                    <input type="password" id="password" name="staPassword" class="form-input pr-10">
                    <button type="button" class="password-toggle-btn" onclick="togglePasswordVisibility('password', 'toggleIcon')">
                        <i id="toggleIcon" class="fas fa-eye"></i>
                    </button>
                </div>
            </div>
            <button type="submit" class="btn">Save & Connect</button>
        </form>
        <p class="footer-text">The device will restart. Check the OLED screen for the new IP address to access the main dashboard.</p>
    </div>
    <script>
        document.getElementById('wifi-form').addEventListener('submit', async (e) => {
            e.preventDefault();
            const btn = e.target.querySelector('button');
            btn.textContent = 'Saving...';
            btn.disabled = true;
            
            const formData = new FormData(e.target);
            
            try {
                const response = await fetch('/save_ap_settings', {
                    method: 'POST',
                    body: new URLSearchParams(formData)
                });
                
                if (response.ok) {
                    document.querySelector('.container').innerHTML = '<h1>Success!</h1><p>Settings saved. The device is now restarting to connect to your Wi-Fi. Please check the OLED display for the new IP address.</p>';
                } else {
                    alert('Failed to save settings. Please try again.');
                    btn.textContent = 'Save & Connect';
                    btn.disabled = false;
                }
            } catch (error) {
                console.error('Error:', error);
                alert('An error occurred. Check the serial monitor for details.');
                btn.textContent = 'Save & Connect';
                btn.disabled = false;
            }
        });

        // NEW: Function to toggle password visibility
        function togglePasswordVisibility(inputId, iconId) {
            const input = document.getElementById(inputId);
            const icon = document.getElementById(iconId); //.querySelector('i'); // Get the i element inside the button - CORRECTED
            if (!input || !icon) return;

            if (input.type === 'password') {
                input.type = 'text';
                icon.classList.remove('fa-eye');
                icon.classList.add('fa-eye-slash');
            } else {
                input.type = 'password';
                icon.classList.remove('fa-eye-slash');
                icon.classList.add('fa-eye');
            }
        }
    </script>
</body>
</html>
)rawliteral"

// Wrapped Web Server Handlers to manage the isWebRequestActive flag
void handleRootWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    
    // If in AP mode, serve the simple setup page without authentication.
    if (wifiMode == WIFI_AP_MODE) {
      server.send_P(200, "text/html", AP_WEB_UI); // Serve the new AP setup UI
    } 
    // If in STA mode, require authentication and then serve the full UI.
    else { 
      if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
      }
      server.send_P(200, "text/html", FULL_WEB_UI); // Serve the full UI in STA mode
    }
    esp_task_wdt_reset(); // Reset after send

    isWebRequestActive = false;
}

// Handler function for getting settings from the ESP32
void handleGetSettingsWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }

    // Allocate a DynamicJsonDocument. Increased capacity to 4096 bytes for more headroom.
    // FIX: Made the JSON document static to prevent stack overflow, and clearing it before use.
    const size_t capacity = 4096;
    static DynamicJsonDocument doc(capacity);
    doc.clear();
    esp_task_wdt_reset();

    // NEW: Lock mutex before accessing shared data
    xSemaphoreTake(sharedDataMutex, portMAX_DELAY);

    DateTime now = rtc.now();

    char timeStr[10];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    doc["currentTime"] = timeStr;

    // NEW: Find and add the next chronological schedule to the JSON response
    Schedule nextSched;
    if (pumpMode == SCHEDULE && findNextSchedule(nextSched)) {
        char nextSchedStr[25];
        if (nextSched.repeatType == SPECIFIC_DATE) {
            snprintf(nextSchedStr, sizeof(nextSchedStr), "%02d/%02d %02d:%02d", nextSched.day, nextSched.month, nextSched.hour, nextSched.minute);
        } else {
            snprintf(nextSchedStr, sizeof(nextSchedStr), "%02d:%02d", nextSched.hour, nextSched.minute);
        }
        doc["nextSchedule"] = nextSchedStr;
    } else {
        doc["nextSchedule"] = "None";
    }

    // NEW: Add System Information to the JSON response
    doc["firmwareVersion"] = "SS-WLPC v6.0";

    char deviceIdStr[20];
    snprintf(deviceIdStr, sizeof(deviceIdStr), "%02X-%04X", allowedGroupID, allowedSerialID);
    doc["deviceId"] = deviceIdStr;

    doc["macAddress"] = WiFi.macAddress();

    char chipInfoStr[50];
    snprintf(chipInfoStr, sizeof(chipInfoStr), "%s C%u R%u", ESP.getChipModel(), ESP.getChipCores(), ESP.getChipRevision());
    doc["chipInfo"] = chipInfoStr;

    char heapStr[20];
    snprintf(heapStr, sizeof(heapStr), "%lu bytes", ESP.getFreeHeap());
    doc["freeHeap"] = heapStr;

    unsigned long seconds = (uint32_t)(esp_timer_get_time() / 1000) / 1000;
    unsigned long days = seconds / 86400;
    unsigned long hours = (seconds % 86400) / 3600;
    unsigned long minutes = (seconds % 3600) / 60;
    char uptimeStr[30];
    snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02luh %02lum", days, hours, minutes);
    doc["uptime"] = uptimeStr;


    doc["pumpMode"] = pumpMode;
    doc["systemProfile"] = currentProfile; // NEW: Add system profile
    // UPDATED: Use the new tank level string function
    doc["currentTankLevel"] = getTankLevelString(currentTankLevel);
    doc["pumpRunning"] = pumpRunning;
    doc["flowOk"] = flowOk;
    doc["dryRunError"] = dryRunError;

    char sensorErrorStr[30];
    switch(sensorError) {
        case SENSOR_ERROR_NONE: snprintf(sensorErrorStr, sizeof(sensorErrorStr), "SENSOR_ERROR_NONE"); break;
        case SENSOR_ERROR_INVALID_COMBINATION: snprintf(sensorErrorStr, sizeof(sensorErrorStr), "SENSOR_ERROR_INVALID_COMBINATION"); break;
        case SENSOR_ERROR_SEQUENCE_UP: snprintf(sensorErrorStr, sizeof(sensorErrorStr), "SENSOR_ERROR_SEQUENCE_UP"); break;
        case SENSOR_ERROR_SEQUENCE_DOWN: snprintf(sensorErrorStr, sizeof(sensorErrorStr), "SENSOR_ERROR_SEQUENCE_DOWN"); break;
        case SENSOR_ERROR_DIRTY_WATER: snprintf(sensorErrorStr, sizeof(sensorErrorStr), "SENSOR_ERROR_DIRTY_WATER"); break; // NEW
        default: snprintf(sensorErrorStr, sizeof(sensorErrorStr), "UNKNOWN_ERROR"); break;
    }
    doc["sensorError"] = sensorErrorStr;
    doc["sensorErrorDetail"] = sensorErrorDetailMessage;
    doc["isWaterDirty"] = isWaterDirty; // NEW: Expose the isWaterDirty status
    doc["turbidityValue"] = turbidityValue; // NEW: Expose the turbidity value
    doc["turbidityBypassEnabled"] = turbidityBypassEnabled; // NEW: Expose turbidity bypass status
    doc["calibratedCleanValue"] = calibratedCleanValue; // NEW: Expose clean calibration value
    doc["calibratedDirtyValue"] = calibratedDirtyValue; // NEW: Expose dirty calibration value
    doc["transmitterTxPower"] = transmitterTxPower; // NEW: Expose transmitter power setting
    doc["transmitterAckAttempts"] = transmitterAckAttempts; // NEW: Expose transmitter ACK attempts setting
    doc["loraPowerOptimizationEnabled"] = loraPowerOptimizationEnabled; // NEW
    esp_task_wdt_reset();

    doc["batteryVoltage"] = batteryVoltage; // UPDATED: Send actual voltage
    doc["lowBattery"] = lowBattery;        // Keep the boolean flag for other logic if needed
    doc["heartbeatExpired"] = (!firstHeartbeatReceived || (rtc.now().unixtime() - lastHeartbeatTimeRTC.unixtime()) > (HEARTBEAT_TIMEOUT / 1000));
    doc["remainingDryRunAttempts"] = remainingDryRunAttempts;

    // NEW: Add confirmed transmitter settings to JSON
    doc["confirmedTxPower"] = confirmedTxPower;
    doc["confirmedAckAttempts"] = confirmedAckAttempts;

    doc["dryRunDelayMin"] = dryRunDelayMin;
    doc["dryRunAttempts"] = dryRunAttempts;
    doc["retryIntervalMin"] = retryIntervalMin;
    doc["scheduleDurationMin"] = scheduleDurationMin;
    doc["primingTimeMs"] = primingTimeMs;
    doc["signalStrength"] = lastPacketRssi; // NEW: Add RSSI to JSON response
    doc["turbidityLimit"] = turbidityLimit; // NEW
    doc["waterPresenceThreshold"] = waterPresenceThreshold; // NEW
    doc["startSensorLevel"] = startSensorLevel; // NEW
    doc["endSensorLevel"] = endSensorLevel; // NEW
    doc["waterSensingStartDelaySec"] = waterSensingStartDelaySec;
    doc["waterSensingStopDelaySec"] = waterSensingStopDelaySec;

    doc["webUiRefreshIntervalSec"] = webUiRefreshIntervalSec; // NEW
    doc["autoResetDryRunOnWater"] = autoResetDryRunOnWater; // NEW
    doc["autoResetTurbidityOnError"] = autoResetTurbidityOnError; // NEW

    // --- MODIFICATION START: Add missing boolean flags ---
    doc["dryRunLogicEnabled"] = dryRunLogicEnabled;
    doc["retryLogicEnabled"] = retryLogicEnabled;
    doc["sensorErrorBypassEnabled"] = sensorErrorBypassEnabled;
    doc["forceScheduledRun"] = forceScheduledRun; // <<< ADDED THIS
    doc["manualModeSafetyLogicEnabled"] = manualModeSafetyLogicEnabled; // <<< ADDED THIS
    // --- MODIFICATION END ---

    JsonArray schedulesArray = doc.createNestedArray("schedules");
    for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
        JsonObject scheduleObj = schedulesArray.createNestedObject();
        scheduleObj["hour"] = schedules[i].hour;
        scheduleObj["minute"] = schedules[i].minute;
        scheduleObj["repeatType"] = getRepeatTypeString(schedules[i].repeatType);
        // NEW: Add date fields to JSON response
        scheduleObj["year"] = schedules[i].year;
        scheduleObj["month"] = schedules[i].month;
        scheduleObj["day"] = schedules[i].day;
    }

    JsonObject rtcObj = doc.createNestedObject("rtc");
    rtcObj["year"] = now.year();
    rtcObj["month"] = now.month();
    rtcObj["day"] = now.day();
    rtcObj["hour"] = now.hour();
    rtcObj["minute"] = now.minute();
    rtcObj["second"] = now.second();
    esp_task_wdt_reset();

    doc["currentFontIndex"] = currentFontIndex;
    doc["oledTheme"] = currentOledTheme;
    doc["buzzerEnabled"] = buzzerEnabled;
    doc["buzzerVolume"] = buzzerVolume; // NEW: Add buzzer volume
    // NEW: Add OLED layout settings to JSON
    doc["oledShowTime"] = oledShowTime;
    doc["oledShowDate"] = oledShowDate;
    doc["oledShowDay"] = oledShowDay;
    doc["oledShowBattery"] = oledShowBattery;
    // Removed buzzer volume from JSON response
    doc["webTheme"] = currentWebTheme; // NEW: Add web theme to JSON response
    doc["turbidityTimeoutSec"] = turbidityTimeoutSec; // NEW: Add turbidity timeout

    // NEW: WiFi Settings to JSON
    doc["wifiMode"] = wifiMode;
    doc["staSsid"] = staSsid;
    // doc["staPassword"] = staPassword; // Don't send passwords back to client
    // NEW: Add passwords to JSON, but consider security implications
    // doc["apPassword"] = ap_password; // AP Password is no longer configurable
    doc["httpPassword"] = http_password;

    doc["wifiStatus"] = getWifiStatusString();

    char localIPStr[16];
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(localIPStr, sizeof(localIPStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    } else {
        snprintf(localIPStr, sizeof(localIPStr), "N/A");
    }
    doc["localIP"] = localIPStr;

    // Add serial ID to JSON
    char serialIdStr[15];
    snprintf(serialIdStr, sizeof(serialIdStr), "0x%02X-0x%04X", allowedGroupID, allowedSerialID);
    doc["serialId"] = serialIdStr;

    // NEW: Add buzzer settings to JSON
    doc["dryRunBeepStyle"] = getBeepStyleString(dryRunBeepStyle);
    doc["sensorErrorBeepStyle"] = getBeepStyleString(sensorErrorBeepStyle);
    doc["lowBatteryBeepStyle"] = getBeepStyleString(lowBatteryBeepStyle);
    doc["heartbeatExpiredBeepStyle"] = getBeepStyleString(heartbeatExpiredBeepStyle);
    doc["tankEmptyBeepStyle"] = getBeepStyleString(tankEmptyBeepStyle);
    doc["tankFullBeepStyle"] = getBeepStyleString(tankFullBeepStyle);
    doc["dirtyWaterBeepStyle"] = getBeepStyleString(dirtyWaterBeepStyle); // NEW

    esp_task_wdt_reset();

    // NEW: Release mutex after all shared data has been read
    xSemaphoreGive(sharedDataMutex);

    // FIX: Serialize to a string first, then send. This avoids blocking on slow clients.
    // NEW: Make jsonString static to avoid large stack allocation
    static String jsonString;
    jsonString = ""; // Clear the static string
    serializeJson(doc, jsonString);
    esp_task_wdt_reset(); // Reset after serialization
    server.send(200, "application/json", jsonString);
    esp_task_wdt_reset(); // Reset after send

    isWebRequestActive = false;
}

// NEW: Handler for updating all settings at once
void handleUpdateAllSettingsWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();

    // In STA mode, authentication is always required to update settings
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }

    // NEW: Flag to track validation status
    bool validationError = false;

    // NEW: Take mutex before modifying shared state
    xSemaphoreTake(sharedDataMutex, portMAX_DELAY);

    // Handle special actions first
    if (server.hasArg("action")) {
        String action = server.arg("action");
        if (action == "clear_schedules") {
            for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
                schedules[i] = {0, 0, EVERY_DAY, 0, 0, 0};
            }
            ESP_LOGI(TAG, "%s
", String("All schedules cleared via Web UI.");
            // REMOVED: saveSettingsToEEPROM();
            // REMOVED: server.send(200, "text/plain", "All schedules cleared.");
            // REMOVED: isWebRequestActive = false;
            // REMOVED: return; // Action handled, exit function
            // Let it fall through to the main save at the end
        }
        if (action == "clear_schedule" && server.hasArg("index")) {
            uint8_t index = server.arg("index").toInt();
            if (index < MAX_SCHEDULES) {
                schedules[index] = {0, 0, EVERY_DAY, 0, 0, 0};
                ESP_LOGI(TAG, "Schedule #%d cleared via Web UI.\n", index + 1);
                // No need to give mutex here, as saveSettings will be called below
                // and the function will exit. Let's keep it simple.
            }
        }
    }


    // If no special action, proceed with updating all settings from form data
    // Update System Profile First
    if (server.hasArg("systemProfile")) currentProfile = (SystemProfile)server.arg("systemProfile").toInt();

    // Update Pump Settings
    if (server.hasArg("pumpMode")) pumpMode = (PumpMode)server.arg("pumpMode").toInt();
    if (server.hasArg("startSensorLevel") && server.hasArg("endSensorLevel")) { // NEW: Handle Start/End sensor levels
        TankLevel newStart = (TankLevel)server.arg("startSensorLevel").toInt();
        TankLevel newEnd = (TankLevel)server.arg("endSensorLevel").toInt();
        // Validation
        if (newStart < newEnd && newStart <= TANK_75 && newEnd >= TANK_25) {
            startSensorLevel = newStart;
            endSensorLevel = newEnd;
        } else {
            ESP_LOGI(TAG, "%s
", String("WebUI sent invalid start/end sensor levels. Ignoring.");
        }
    }
    if (server.hasArg("dryRunLogicEnabled")) dryRunLogicEnabled = (server.arg("dryRunLogicEnabled") == "1");
    if (server.hasArg("dryRunDelayMin")) dryRunDelayMin = server.arg("dryRunDelayMin").toInt();
        if (server.hasArg("retryLogicEnabled")) retryLogicEnabled = (server.arg("retryLogicEnabled") == "1");
    if (server.hasArg("dryRunAttempts")) { dryRunAttempts = server.arg("dryRunAttempts").toInt(); remainingDryRunAttempts = dryRunAttempts; }
    if (server.hasArg("retryIntervalMin")) retryIntervalMin = server.arg("retryIntervalMin").toInt();
    if (server.hasArg("scheduleDurationMin")) scheduleDurationMin = server.arg("scheduleDurationMin").toInt();
    if (server.hasArg("primingTimeMs")) primingTimeMs = server.arg("primingTimeMs").toInt();
        if (server.hasArg("turbidityLimit")) turbidityLimit = server.arg("turbidityLimit").toInt(); // NEW
    if (server.hasArg("waterPresenceThreshold")) waterPresenceThreshold = server.arg("waterPresenceThreshold").toInt(); // NEW
    if (server.hasArg("turbidityBypassEnabled")) turbidityBypassEnabled = (server.arg("turbidityBypassEnabled") == "1"); // NEW
    if (server.hasArg("forceScheduledRun")) forceScheduledRun = (server.arg("forceScheduledRun") == "1");
    if (server.hasArg("sensorErrorBypassEnabled")) sensorErrorBypassEnabled = (server.arg("sensorErrorBypassEnabled") == "1");
    if (server.hasArg("manualModeSafetyLogicEnabled")) manualModeSafetyLogicEnabled = (server.arg("manualModeSafetyLogicEnabled") == "1");
    if (server.hasArg("waterSensingStartDelaySec")) waterSensingStartDelaySec = server.arg("waterSensingStartDelaySec").toInt();
    if (server.hasArg("waterSensingStopDelaySec")) waterSensingStopDelaySec = server.arg("waterSensingStopDelaySec").toInt();
    if (server.hasArg("webUiRefreshIntervalSec")) webUiRefreshIntervalSec = server.arg("webUiRefreshIntervalSec").toInt(); // NEW
    if (server.hasArg("autoResetDryRunOnWater")) autoResetDryRunOnWater = (server.arg("autoResetDryRunOnWater") == "1"); // NEW
    if (server.hasArg("autoResetTurbidityOnError")) autoResetTurbidityOnError = (server.arg("autoResetTurbidityOnError") == "1"); // NEW
    if (server.hasArg("transmitterTxPower")) { // NEW
        uint8_t newPower = server.arg("transmitterTxPower").toInt();
        if (newPower >= 2 && newPower <= 20) {
            transmitterTxPower = newPower;
        }
    }
    if (server.hasArg("transmitterAckAttempts")) { // NEW
        uint8_t newAttempts = server.arg("transmitterAckAttempts").toInt();
        if (newAttempts >= 1 && newAttempts <= 10) {
            transmitterAckAttempts = newAttempts;
        }
    }
    if (server.hasArg("loraPowerOptimizationEnabled")) loraPowerOptimizationEnabled = (server.arg("loraPowerOptimizationEnabled") == "1"); // NEW


        // Add a watchdog reset after this large block of parsing
    esp_task_wdt_reset();

    // Update Schedules
    for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
        // Check if data for this schedule index exists in the request
        String hourArg = "sched" + String(i) + "Hour";
        if (server.hasArg(hourArg)) {
            // A schedule is being updated, create a temporary struct to validate it
            Schedule tempSched;
            tempSched.hour = server.arg(hourArg).toInt();
            tempSched.minute = server.arg("sched" + String(i) + "Minute").toInt();
            tempSched.repeatType = getRepeatTypeEnum(server.arg("sched" + String(i) + "RepeatType"));
            tempSched.year = server.arg("sched" + String(i) + "Year").toInt();
            tempSched.month = server.arg("sched" + String(i) + "Month").toInt();
            tempSched.day = server.arg("sched" + String(i) + "Day").toInt();

            // RE-ENABLED: Validate the schedule before saving from the Web UI.
            if (!isScheduleValid(tempSched)) {
                // server.send(400, "text/plain", "Check Date OR Time");
                // isWebRequestActive = false;
                // return; // Stop processing and send error
                validationError = true; // NEW: Set flag
                break; // NEW: Stop processing schedules
            }

            // If valid, apply changes to the global schedules array
            schedules[i] = tempSched;
        }
    }

    // Update System Settings
    if (!validationError) { // NEW: Don't process other settings if validation failed
        if (server.hasArg("displayFontIndex")) currentFontIndex = server.arg("displayFontIndex").toInt();
        if (server.hasArg("oledTheme")) currentOledTheme = (OledTheme)server.arg("oledTheme").toInt();
        if (server.hasArg("webTheme")) currentWebTheme = server.arg("webTheme").toInt(); // <<< FIX: Added this line to save the Web UI theme
    if (server.hasArg("turbidityTimeoutSec")) turbidityTimeoutSec = server.arg("turbidityTimeoutSec").toInt(); // NEW: Update turbidity timeout
    // NEW: Update OLED layout settings from web request
    if (server.hasArg("oledShowTime")) oledShowTime = (server.arg("oledShowTime") == "1");
    if (server.hasArg("oledShowDate")) oledShowDate = (server.arg("oledShowDate") == "1");
    if (server.hasArg("oledShowDay")) oledShowDay = (server.arg("oledShowDay") == "1");
    if (server.hasArg("oledShowBattery")) oledShowBattery = (server.arg("oledShowBattery") == "1");
    // Removed buzzer volume from setting update logic
    
        // --- MODIFICATION START: Add missing buzzerEnabled toggle ---
        if (server.hasArg("buzzerEnabled")) buzzerEnabled = (server.arg("buzzerEnabled") == "1");
        // --- MODIFICATION END ---

        // NEW: Update Security Settings
    // AP Password is now fixed as open (empty string) and cannot be changed from the UI.
    /*
    bool apPasswordChanged = false;
        if (server.hasArg("apPassword")) {
        const char* newPass = server.arg("apPassword").c_str();
        if (strcmp(ap_password, newPass) != 0) {
            strncpy(ap_password, newPass, sizeof(ap_password) - 1);
                ap_password[sizeof(ap_password) - 1] = '\0';
            apPasswordChanged = true;
        }
    }
    */
    if (server.hasArg("httpPassword")) {
        const char* newPass = server.arg("httpPassword").c_str();
        if (strcmp(http_password, newPass) != 0) {
             strncpy(http_password, newPass, sizeof(http_password) - 1);
                 http_password[sizeof(http_password) - 1] = '\0';
        }
    }

    // Update WiFi Settings
        WifiMode oldWifiMode = wifiMode;
    if (server.hasArg("wifiMode")) wifiMode = (WifiMode)server.arg("wifiMode").toInt();
    if (server.hasArg("staSsid")) strncpy(staSsid, server.arg("staSsid").c_str(), sizeof(staSsid) - 1);
    if (server.hasArg("staPassword")) strncpy(staPassword, server.arg("staPassword").c_str(), sizeof(staPassword) - 1);
    
    // Re-init WiFi if mode changed
        if (wifiMode != oldWifiMode) {
         if (wifiMode == WIFI_AP_MODE) { WiFi.softAP(ap_ssid, ap_password); } else { connectToWiFiSTA(); }
    }

    // Update RTC
        DateTime currentRTC = rtc.now();
    int16_t year = server.hasArg("rtcYear") ? server.arg("rtcYear").toInt() : currentRTC.year();
    uint8_t month = server.hasArg("rtcMonth") ? server.arg("rtcMonth").toInt() : currentRTC.month();
    uint8_t day = server.hasArg("rtcDay") ? server.arg("rtcDay").toInt() : currentRTC.day();
        uint8_t hour = server.hasArg("rtcHour") ? server.arg("rtcHour").toInt() : currentRTC.hour();
    uint8_t minute = server.hasArg("rtcMinute") ? server.arg("rtcMinute").toInt() : currentRTC.minute();
    uint8_t second = server.hasArg("rtcSecond") ? server.arg("rtcSecond").toInt() : currentRTC.second();
    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    // NEW: Update buzzer settings from web request
        if (server.hasArg("dryRunBeepStyle")) dryRunBeepStyle = getBeepStyleEnum(server.arg("dryRunBeepStyle"));
    if (server.hasArg("sensorErrorBeepStyle")) sensorErrorBeepStyle = getBeepStyleEnum(server.arg("sensorErrorBeepStyle"));
    if (server.hasArg("lowBatteryBeepStyle")) lowBatteryBeepStyle = getBeepStyleEnum(server.arg("lowBatteryBeepStyle"));
    if (server.hasArg("heartbeatExpiredBeepStyle")) heartbeatExpiredBeepStyle = getBeepStyleEnum(server.arg("heartbeatExpiredBeepStyle"));
    if (server.hasArg("tankEmptyBeepStyle")) tankEmptyBeepStyle = getBeepStyleEnum(server.arg("tankEmptyBeepStyle"));
    if (server.hasArg("tankFullBeepStyle")) tankFullBeepStyle = getBeepStyleEnum(server.arg("tankFullBeepStyle"));
    if (server.hasArg("dirtyWaterBeepStyle")) dirtyWaterBeepStyle = getBeepStyleEnum(server.arg("dirtyWaterBeepStyle")); // NEW

    } // NEW: Added closing brace for 'if (!validationError)'

    // NEW: Release mutex after all modifications are done in RAM
    xSemaphoreGive(sharedDataMutex);

    // --- NEW: Perform slow operations (save, send) outside of the mutex ---

    if (validationError) {
        server.send(400, "text/plain", "Check Date OR Time");
        isWebRequestActive = false;
        return; // Stop processing and send error
    }

    saveSettingsToEEPROM(); // This is now outside the mutex lock
    esp_task_wdt_reset(); // Reset after EEPROM save
    server.send(200, "text/plain", "Settings updated.");
    esp_task_wdt_reset(); // Reset after send
    isWebRequestActive = false;
} // <-- DELETED: This brace was in the wrong place. // void handleUpdateAllSettingsWrapped()

// NEW: Handler for Factory Reset
void handleFactoryResetWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }
    
    // Invalidate EEPROM
    EEPROM.write(EEPROM_MAGIC_ADDR, 0x00); // Write a non-magic value
    EEPROM.commit();
    esp_task_wdt_reset(); // Reset after commit
    
    server.send(200, "text/plain", "Factory reset successful. Device is restarting.");
    esp_task_wdt_reset(); // Reset after send
    
    // Delay to allow the server response to be sent before restarting
    vTaskDelay(pdMS_TO_TICKS(1000);
    ESP.restart();
}


// Handler function for resetting sensor error
void handleResetSensorErrorWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }
    xSemaphoreTake(sharedDataMutex, portMAX_DELAY);
    sensorError = SENSOR_ERROR_NONE;
    isSensorErrorBeepActive = false; // Reset the beep flag
    isDirtyWaterError = false; // NEW: Also clear the persistent dirty water error
    isDirtyWaterBeepActive = false; // NEW: And its beep flag
    snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), ""); // Clear char array
    sensorErrorResetMessageActive = true; // NEW: Trigger display message
    sensorErrorResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000); // NEW: Set message start time
    ESP_LOGI(TAG, "%s
", String("Sensor error manually reset.");
    lastObservedValidTankLevel = currentTankLevel;
    xSemaphoreGive(sharedDataMutex);
    server.send(200, "text/plain", "Sensor Error Reset!");
    isWebRequestActive = false;
}

// Handler function for toggling the pump
void handleTogglePumpWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    if (!isAuthenticated()) {
        isWebRequestActive = false;
        server.requestAuthentication();
        return;
    }

    // NEW: Take mutex to safely check and modify pump state
    xSemaphoreTake(sharedDataMutex, portMAX_DELAY);

    if (sensorError != SENSOR_ERROR_NONE && !sensorErrorBypassEnabled) {
        server.send(403, "text/plain", "PUMP BLOCKED: Sensor Error active and bypass disabled.");
        pumpBlockedMessageActive = true;
        pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
        ESP_LOGI(TAG, "%s
", String("PUMP BLOCKED: Manual toggle attempted via Web UI due to sensor error.");
        xSemaphoreGive(sharedDataMutex); // Release mutex before returning
        isWebRequestActive = false;
        return;
    }

    if (pumpMode == MANUAL || pumpMode == AUTO || pumpMode == WATER_SENSING) {
        if (pumpRunning || waitingForPumpRelay) {
            stopPump();
            if (currentPumpRunHadFlow) remainingDryRunAttempts = dryRunAttempts;
            server.send(200, "text/plain", "Pump Stopped!");
        } else {
            initiatePumpStart(false);
            server.send(200, "text/plain", "Pump Started!");
        }
    } else { // SCHEDULE mode
        // NEW: Allow stopping the pump in schedule mode, but not starting it.
        if (pumpRunning || waitingForPumpRelay) {
            stopPump();
            if (currentPumpRunHadFlow) remainingDryRunAttempts = dryRunAttempts;
            server.send(200, "text/plain", "Pump Stopped!");
        } else {
            pumpBlockedMessageActive = true;
            pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            server.send(403, "text/plain", "Manual START not allowed in SCHEDULE mode.");
        }
    }

    // NEW: Release the mutex
    xSemaphoreGive(sharedDataMutex);

    esp_task_wdt_reset(); // Reset after send
    isWebRequestActive = false;
}

// Handler for not found pages
void handleNotFoundWrapped() {
    isWebRequestActive = true;
    esp_task_wdt_reset();
    server.send(404, "text/plain", "Not Found");
    isWebRequestActive = false;
}

// Function to handle rotation of display screens
void handleScreenRotation() {
    // Don't rotate if a valve sequence is active
    if (waitingForPumpRelay) return;

    // Only rotate if enough time has passed
    if ((uint32_t)(esp_timer_get_time() / 1000) - lastScreenSwitch >= DRY_RUN_DISPLAY_ROTATION_MS) {
      uint8_t maxState = 0; // Will determine the max index for the current rotation mode
      if (pumpRunning) {
        // If pump is running in MANUAL, and safety logic is disabled, only show PUMP ON and Tank Status
        if (pumpMode == MANUAL && !manualModeSafetyLogicEnabled) {
          maxState = 2; // PUMP ON, Tank Status
          screenState++;
          if (screenState > maxState) screenState = 1;
          // Map 1->PUMP ON, 2->Tank Status
          if (screenState == 1) { /* PUMP ON */ }
          else if (screenState == 2) { screenState = 4; } // Map to Tank Status display
        } else { // SCHEDULE or AUTO mode, or MANUAL mode with safety logic enabled
          // NEW: If dry run logic is disabled, don't show states 2 and 3
          maxState = dryRunLogicEnabled ? 6 : 4; 
          screenState++;
          if (screenState > maxState) screenState = 1;
          // Special handling for Dry Countdown: only show if relevant
          if (dryRunLogicEnabled && screenState == 3 && !waitingForFlow) {
            // If Dry Countdown is not relevant, skip to next state
            screenState++;
            if (screenState > maxState) screenState = 1;
          }
        }
      } else { // Pump not running (idle, dry error, or retry)
        // --- State Rotation Logic for IDLE/ERROR states ---
        
        // 1. If there's a Dry Run Error or a Retry is in progress, cycle through all info screens
        if (dryRunLogicEnabled && (dryRunError || retryInProgress)) {
          screenState++;
          // Cycle through all 7 states (1-7)
          if (screenState > 7 || screenState == 0) screenState = 1; 
        } 
        // 2. Specific, shorter rotation for WATER_SENSING mode when idle
        else if (pumpMode == WATER_SENSING) {
            // Cycle through: Water Presence (6), Tank Status (1), and Turbidity (5)
            if (screenState == 6) screenState = 1;
            else if (screenState == 1) screenState = 5;
            else screenState = 6; // From Turbidity (5) or any other state, default to Water Presence
        }
        // 3. Normal Idle state for SCHEDULE, AUTO, or MANUAL modes
        else { 
          // Cycle through: Tank(1), Schedule(2), Turbidity(5), Water Presence(6), TX Confirmed(7)
          if (screenState == 1) screenState = 2;
          else if (screenState == 2) screenState = 5;
          else if (screenState == 5) screenState = 6;
          else if (screenState == 6) screenState = 7;
          else screenState = 1; // From TX Confirmed(7) or other states, go back to Tank
        }
      }
      lastScreenSwitch = (uint32_t)(esp_timer_get_time() / 1000);
    }
}

// NEW: Consolidated function to manage LoRa and the RX LED to prevent timing issues.
    // This function replaces the old readLoRaPacket().
// DELETED: This function is being removed. Its logic is moved to processLoRaQueue().
/*
void handleLoRaAndRxLed() {
    // ... entire function body ...
}
*/

// Function to handle LED indicators
void handleLEDs() {
    // RX LED logic has been moved to handleLoRaAndRxLed() for better timing control.

    // Heartbeat LED: Solid ON if heartbeat is recent, blinking if expired
    bool heartbeatExpired = !isRtcWorking || !firstHeartbeatReceived ||
      (rtc.now().unixtime() - lastHeartbeatTimeRTC.unixtime() > HEARTBEAT_TIMEOUT / 1000);
    gpio_set_level(HEARTBEAT_LED_PIN, heartbeatExpired ? ((uint32_t)(esp_timer_get_time() / 1000) / 500) % 2 : 1); // Blinks if expired, solid ON if active
    // UPDATED: Only beep for expired heartbeat if one has been received before, to prevent beeping on startup
    if (heartbeatExpired && firstHeartbeatReceived && buzzerEnabled && !isHeartbeatExpiredBeepActive) {
      beepForHeartbeatExpired();
    } else if (!heartbeatExpired) {
      isHeartbeatExpiredBeepActive = false;
    }

    // UPDATED: Battery LED blinks if low battery is detected
    gpio_set_level(BATTERY_LED_PIN, lowBattery ? ((uint32_t)(esp_timer_get_time() / 1000) / 500) % 2 : 0);
}

// Function to check for dry run timeout
void checkDryRunTimeout() {
    // NEW: Check if dry run logic is enabled before running any of the logic
    if (!dryRunLogicEnabled) return;

    // Dry run logic only applies if NOT in MANUAL mode, or if in MANUAL mode and safety logic is enabled
    if (pumpMode == MANUAL && !manualModeSafetyLogicEnabled) return;

    if (pumpRunning && waitingForFlow) {
      // If pump has been running longer than dry run delay and still waiting for flow
      if ((uint32_t)(esp_timer_get_time() / 1000) - pumpStartTime > dryRunDelayMin * 60000UL) {
        stopPump(); // Stop both relays
        ESP_LOGI(TAG, "%s
", String("Dry run timeout detected.");
        // NEW: Check if retry logic is enabled before starting the retry process
        if (retryLogicEnabled) {
          if (remainingDryRunAttempts > 0) {
            retryInProgress = true; // Start retry process
            retryStartTime = (uint32_t)(esp_timer_get_time() / 1000); // Record retry start time
            remainingDryRunAttempts--; // Decrement remaining attempts
            ESP_LOGI(TAG, "%s", String("Retrying... Attempts left: "); ESP_LOGI(TAG, "%s
", String(remainingDryRunAttempts);
          } else {
            dryRunError = true; // All retry attempts exhausted, set dry run error
            retryInProgress = false; // No longer retrying
            ESP_LOGI(TAG, "%s
", String("Dry run error: All attempts exhausted.");
            if (buzzerEnabled && !isDryRunErrorBeepActive) { // Trigger beep only once
              beepForDryRunError();
            }
          }
        } else { // Retry logic is disabled
          dryRunError = true;
          ESP_LOGI(TAG, "%s
", String("Dry run error: Retry logic is disabled.");
          // NEW (Bug Fix): Trigger the beep even if retry logic is disabled.
          if (buzzerEnabled && !isDryRunErrorBeepActive) {
              beepForDryRunError();
          }
        }
      }
    }
}

// Function to check if it's time to retry after a dry run attempt
void checkRetryTrigger() {
    // NEW: Check if dry run and retry logic is enabled before running the logic
    if (!dryRunLogicEnabled || !retryLogicEnabled) return;
    
    // Retry logic only applies if NOT in MANUAL mode, or if in MANUAL mode and safety logic is enabled
    if (pumpMode == MANUAL && !manualModeSafetyLogicEnabled) return;

    // NEW: Also check if retry logic is enabled
    if (retryInProgress && !pumpRunning) { // If a retry is pending and pump is currently off
      if ((uint32_t)(esp_timer_get_time() / 1000) - retryStartTime > retryIntervalMin * 60000UL) {
        retryInProgress = false; // Stop retrying, we are initiating a start
        initiatePumpStart(false); // Pass false for retry trigger
      }
    }
}

// Function to allow manual reset of dry run error and sensor error by holding buttons
void checkDryRunReset() {
    // Dry run reset logic
    if (dryRunError) {
        // Apply this only if not in MANUAL mode OR if in MANUAL mode and safety logic is enabled
        if (pumpMode != MANUAL || manualModeSafetyLogicEnabled) {
            // Check if UP button is being held down
            if (upBtn.isPressed && ((uint32_t)(esp_timer_get_time() / 1000) - upBtn.pressStartTime >= BUTTON_HOLD_TIME_MS)) {
                dryRunError = false; // Clear dry run error
                isDryRunErrorBeepActive = false; // Reset the beep flag
                remainingDryRunAttempts = dryRunAttempts; // Reset attempts
                dryRunResetMessageActive = true; // NEW: Trigger display message
                dryRunResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000); // NEW: Set message start time
                ESP_LOGI(TAG, "%s
", String("Dry run error manually reset.");
                // If tank is empty and in AUTO mode, restart pump after reset
                bool tankEmpty = (currentTankLevel == TANK_EMPTY);
                if (tankEmpty && pumpMode == AUTO) { // This part remains AUTO-specific
                    initiatePumpStart(false); // Pass false for manual reset trigger
                }
            }
        }
    }

    // Sensor Error Reset (this part remains unconditional, as it's a critical safety override)
    if (sensorError != SENSOR_ERROR_NONE) {
        // Check if DOWN button is being held down
        if (downBtn.isPressed && ((uint32_t)(esp_timer_get_time() / 1000) - downBtn.pressStartTime >= BUTTON_HOLD_TIME_MS)) {
            sensorError = SENSOR_ERROR_NONE; // Clear sensor error
            isDirtyWaterError = false; // <--- ADDED THIS LINE
            isSensorErrorBeepActive = false; // Reset the beep flag
            snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), ""); // Clear char array
            sensorErrorResetMessageActive = true; // NEW: Trigger display message
            sensorErrorResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000); // NEW: Set message start time
            ESP_LOGI(TAG, "%s
", String("Sensor error manually reset.");
            // Reset lastObservedValidTankLevel to current for a fresh start
            lastObservedValidTankLevel = currentTankLevel;
        }
    }
}


// === NEW: Acknowledgment Function (UPDATED to include RTC time) ===
void sendAcknowledgment(uint16_t senderID) {
  // Get the current time from the RTC as a Unix timestamp (a 32-bit integer)
  DateTime now = rtc.now();
  uint32_t unixTime = now.unixtime();

  // The acknowledgment packet is now 8 bytes:
  // 2 bytes for the sender ID + 4 bytes for Unix timestamp + 1 byte for TX power + 1 byte for ACK attempts
  uint8_t ackPacket[8];

  // Pack the sender ID into the first two bytes
  ackPacket[0] = (senderID >> 8) & 0xFF;  // High byte of the serial ID
  ackPacket[1] = senderID & 0xFF;         // Low byte of the serial ID
  
  // Pack the 32-bit Unix timestamp into the next four bytes
  ackPacket[2] = (unixTime >> 24) & 0xFF;
  ackPacket[3] = (unixTime >> 16) & 0xFF;
  ackPacket[4] = (unixTime >> 8) & 0xFF;
  ackPacket[5] = unixTime & 0xFF;
  
  // Pack the transmitter power setting
  ackPacket[6] = transmitterTxPower;
  
  // NEW: Pack the transmitter ACK attempts setting
  ackPacket[7] = transmitterAckAttempts;
  
  // --- NEW: Add detailed logging for debugging ---
  ESP_LOGI(TAG, "%s
", String("------------------------------------");
  ESP_LOGI(TAG, "%s
", String("Preparing to send ACK to transmitter...");
  ESP_LOGI(TAG, "  -> Target Sender ID: 0x%04X\n", senderID);
  ESP_LOGI(TAG, "  -> Sending DESIRED TX Power: %d\n", transmitterTxPower);
  ESP_LOGI(TAG, "  -> Sending DESIRED ACK Attempts: %d\n", transmitterAckAttempts);
  ESP_LOGI(TAG, "  -> Sending RTC Unix Time: %lu\n", unixTime);
  ESP_LOGI(TAG, "%s
", String("------------------------------------");
  // --- END of new logging ---
  
  // Switch to transmit mode
  LoRa.idle();

  // Send the acknowledgment packet
  LoRa.beginPacket();
  LoRa.write(ackPacket, 8); // Send the new 8-byte packet
  LoRa.endPacket(true); // 'true' forces a blocking send, ensuring transmission completes

  // After sending, immediately go back to receive mode
  LoRa.receive();
  ESP_LOGI(TAG, "%s", String("ACK sent with RTC time (Unix: ");
  ESP_LOGI(TAG, "%s", String(unixTime);
  ESP_LOGI(TAG, "%s", String(") and TX Power (");
  ESP_LOGI(TAG, "%s", String(transmitterTxPower);
  ESP_LOGI(TAG, "%s", String(") and ACK Attempts ("); // NEW
  ESP_LOGI(TAG, "%s", String(transmitterAckAttempts); // NEW
  ESP_LOGI(TAG, "%s", String(") for ID: 0x");
  ESP_LOGI(TAG, "%s
", String(senderID, HEX);
}


// =================================================================
// === MAIN SETUP & LOOP ===
// =================================================================

// NEW: Task function to handle Web UI on Core 0
void webUiTask(void *pvParameters) {
  ESP_LOGI(TAG, "%s
", String("Web UI Task started on Core 0.");

  // Add this task to the watchdog timer
  esp_task_wdt_add(NULL);

  for (;;) { // Infinite loop for this task
    // Handle incoming web server clients
    server.handleClient();

    // Reset the watchdog for this task
    esp_task_wdt_reset();
    
    // A small delay to prevent this task from hogging the core and to allow other system tasks to run.
    vTaskDelay(2 / portTICK_PERIOD_MS); 
  }
}

extern "C" void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Original setup() code:
    {
    esp_log_level_set("*", ESP_LOG_INFO); // Serial.begin(115200);

    // Initialize LED and Relay pins
    gpio_set_direction(RX_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(HEARTBEAT_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BATTERY_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(VALVE_RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0); // Important: Keep pump off on startup
    // For a high-triggered relay, 0 is OFF.
    // For a low-triggered relay, 1 is OFF.
    gpio_set_level(VALVE_RELAY_PIN, 0); // Set relay to OFF on startup
    ESP_LOGI(TAG, "%s
", String("Valve relay pin configured as high-triggered.");

    // --- NEW: RX LED Hardware Test ---
    ESP_LOGI(TAG, "%s
", String("--- Starting RX LED Hardware Test ---");
    for (int i = 0; i < 5; i++) {
        gpio_set_level(RX_LED_PIN, 1);
        ESP_LOGI(TAG, "%s
", String("RX LED -> ON");
        vTaskDelay(pdMS_TO_TICKS(200);
        gpio_set_level(RX_LED_PIN, 0);
        ESP_LOGI(TAG, "%s
", String("RX LED -> OFF");
        vTaskDelay(pdMS_TO_TICKS(200);
    }
    ESP_LOGI(TAG, "%s
", String("--- RX LED Hardware Test Finished ---");
    // The LED will be left OFF after the test.

    // Initialize Buzzer pin and LEDC (PWM)
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE, // Use low speed mode
        .duty_resolution  = BUZZER_LEDC_RESOLUTION,
        .timer_num        = BUZZER_LEDC_TIMER,
        .freq_hz          = BUZZER_LEDC_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .gpio_num         = BUZZER_PIN,
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .channel          = BUZZER_LEDC_CHANNEL,
        .intr_type        = LEDC_INTR_DISABLE,
        .timer_sel        = LEDC_TIMER_0,
        .duty             = 0, // Start with buzzer off
        .hpoint           = 0,
    };
    ledc_channel_config(&ledc_channel);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, 0); // Corrected arguments
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL); // Corrected arguments

    // Initialize Button pins with internal pull-up resistors for BUTTON_SET and BUTTON_UP
    gpio_set_direction(BUTTON_SET, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_UP, GPIO_MODE_INPUT);
    // UPDATED: Now using GPIO3, which can use the internal pull-up resistor.
    gpio_set_direction(BUTTON_DOWN, GPIO_MODE_INPUT);

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE); // Allocate enough bytes for EEPROM storage

    // Initialize I2C for RTC and OLED
    // UPDATED I2C PINS TO ESP32 STANDARD GPIO21/22
    Wire.begin(21, 22); // SCL, SDA pins for ESP32

    // Initialize RTC
    if (!rtc.begin()) {
      ESP_LOGI(TAG, "%s
", String("Couldn't find RTC");
      isRtcWorking = false;
    } else {
      isRtcWorking = true;
      // Set RTC time if it's not running or time is way off (e.g., before 2020)
      // This is a placeholder, usually you'd set it via user input or NTP
      if (rtc.lostPower() || rtc.now().year() < 2020) {
        ESP_LOGI(TAG, "%s
", String("RTC lost power, setting time to compile time.");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }
    }
    lastHeartbeatTimeRTC = rtc.now(); // Initialize with current RTC time

    // Initialize OLED Display
    display.begin();
    display.setFont(u8g2_font_6x12_tr); // Set a small font for initial messages
    
    // === START of Splash Screens ===
    // 1. "AMIT RAJPUT Presents"
    display.clearBuffer();
    display.setFont(u8g2_font_helvB14_tr);
    const char* present_text = "AMIT RAJPUT";
    const char* presents_text = "Presents";
    int16_t x_present = (128 - display.getStrWidth(present_text)) / 2;
    int16_t x_presents = (128 - display.getStrWidth(presents_text)) / 2;
    display.drawStr(x_present, 28, present_text);
    display.drawStr(x_presents, 48, presents_text);
    display.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(3000); // Display for 3 seconds

    // 2. SURSAJNI AUTOMATIONS logo
    display.clearBuffer();
    display.drawXBMP(32, 0, 64, 64, sursajni_logo);
    display.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(3000); // Display for 3 seconds

    // 3. SURSAJNI AUTOMATIONS text screen
    display.clearBuffer();
    const char* line1 = "SURSAJNI";
    const char* line2 = "AUTOMATIONS";
    display.setFont(u8g2_font_helvB14_tr);
    int16_t line1_width = display.getStrWidth(line1);
    int16_t font_height_line1 = display.getFontAscent() - display.getFontDescent();
    display.setFont(u8g2_font_7x13_tf);
    int16_t line2_width = display.getStrWidth(line2);
    int16_t font_height_line2 = display.getFontAscent() - display.getFontDescent();
    int16_t line_spacing_splash3 = 4;
    int16_t total_text_height = font_height_line1 + line_spacing_splash3 + font_height_line2;
    int16_t y_offset = (64 - total_text_height) / 2;
    int16_t y1 = y_offset + display.getFontAscent();
    int16_t y2 = y1 + font_height_line1 + line_spacing_splash3 + display.getFontAscent();
    display.setFont(u8g2_font_helvB14_tr);
    int16_t x1 = (128 - line1_width) / 2;
    display.drawStr(x1, y1, line1);
    display.setFont(u8g2_font_7x13_tf);
    int16_t x2 = (128 - line2_width) / 2;
    display.drawStr(x2, y2, line2);
    display.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(3000);

    // 4. "WIRELESS Pump Controller & Level Indicator"
    display.clearBuffer();
    const char* line_wireless_text = "WIRELESS";
    const char* line_pump_controller_text = "Pump Controller";
    const char* line_and_text = "&";
    const char* line_level_indicator_text = "Level Indicator";
    
    // Get font metrics for each line
    display.setFont(u8g2_font_helvB14_tr);
    int16_t font_height_wireless = display.getFontAscent() - display.getFontDescent();
    
    display.setFont(u8g2_font_7x13_tf);
    int16_t font_height_pump_controller = display.getFontAscent() - display.getFontDescent();
    
    display.setFont(u8g2_font_6x12_tr);
    int16_t font_height_and = display.getFontAscent() - display.getFontDescent();
    
    int16_t line_spacing = 2; // Consistent spacing between lines
    
    // Calculate total height of the text block to center it vertically
    int16_t total_text_block_height = 
        font_height_wireless + line_spacing +
        font_height_pump_controller + line_spacing +
        font_height_and + line_spacing +
        font_height_pump_controller; // The font for "Level Indicator" is the same as "Pump Controller"

    int16_t y_start_of_block = (64 - total_text_block_height) / 2;

    int16_t current_y_baseline;

    // Draw "WIRELESS"
    display.setFont(u8g2_font_helvB14_tr);
    int16_t x_wireless = (128 - display.getStrWidth(line_wireless_text)) / 2;
    current_y_baseline = y_start_of_block + display.getFontAscent();
    display.drawStr(x_wireless, current_y_baseline, line_wireless_text);

    // Draw "Pump Controller"
    display.setFont(u8g2_font_7x13_tf);
    int16_t x_pump_controller = (128 - display.getStrWidth(line_pump_controller_text)) / 2;
    current_y_baseline += font_height_wireless + line_spacing;
    display.drawStr(x_pump_controller, current_y_baseline, line_pump_controller_text);

    // Draw "&"
    display.setFont(u8g2_font_6x12_tr);
    int16_t x_and = (128 - display.getStrWidth(line_and_text)) / 2;
    current_y_baseline += font_height_pump_controller + line_spacing;
    display.drawStr(x_and, current_y_baseline, line_and_text);
    
    // Draw "Level Indicator"
    display.setFont(u8g2_font_7x13_tf);
    int16_t x_level_indicator = (128 - display.getStrWidth(line_level_indicator_text)) / 2;
    current_y_baseline += font_height_and + line_spacing;
    display.drawStr(x_level_indicator, current_y_baseline, line_level_indicator_text);

    display.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(4000); // Display for 4 seconds
    // === END of Splash Screens ===


    // Initialize LoRa module
    // UPDATED SPI PINS
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS_PIN);
    LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(433E6)) { // Initialize LoRa at 433 MHz
      display.clearBuffer();
      display.drawStr(0, 20, "LoRa Init Failed!");
      display.sendBuffer();
      ESP_LOGI(TAG, "%s
", String("LoRa initialization failed!");
      while (true) { // Halt and blink RX_LED if LoRa fails
        gpio_set_level(RX_LED_PIN, !gpio_get_level(RX_LED_PIN));
        vTaskDelay(pdMS_TO_TICKS(300);
      }
    }
    LoRa.enableCrc(); // Enable CRC for packet integrity checking
    LoRa.setTxPower(5); // Set transmit power (0-20, higher is more power)
    ESP_LOGI(TAG, "%s
", String("LoRa initialized successfully.");

    // Initialize ESP32 Watchdog Timer
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 50000, // Watchdog timeout in milliseconds (50 seconds)
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // Apply to all cores
      .trigger_panic = true // Trigger a panic if watchdog times out
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL); // Add the current task to the watchdog

    // Load settings from EEPROM after all initializations
    loadSettingsFromEEPROM();
    remainingDryRunAttempts = dryRunAttempts; // Initialize remaining attempts

    // NEW: Create the mutex for protecting shared data between main loop and web server
    sharedDataMutex = xSemaphoreCreateMutex();
    
    // NEW: Create the queue for LoRa packets
    loraPacketQueue = xQueueCreate(5, sizeof(LoRaPacket)); // Queue can hold 5 packets

    // NEW: Dynamically create the AP SSID
    snprintf(ap_ssid, sizeof(ap_ssid), "Sursajni_%02X_%04X", allowedGroupID, allowedSerialID);

    // NEW: WiFi Setup based on saved mode
    if (wifiMode == WIFI_AP_MODE) {
        ESP_LOGI(TAG, "Setting up WiFi AP '%s'...\n", ap_ssid);
        WiFi.softAP(ap_ssid, ap_password);
        IPAddress IP = WiFi.softAPIP();
        ESP_LOGI(TAG, "%s", String("AP IP address: ");
        ESP_LOGI(TAG, "%s
", String(IP);
    } else { // WIFI_STA_MODE
        ESP_LOGI(TAG, "Setting up WiFi STA, connecting to '%s'...\n", staSsid);
        WiFi.mode(WIFI_STA);
        connectToWiFiSTA(); // Initial connection attempt
    }

    // NEW: Web Server Route Handling
    server.on("/", HTTP_GET, handleRootWrapped);
    server.on("/get_settings", HTTP_GET, handleGetSettingsWrapped);
    server.on("/get_live_data", HTTP_GET, handleGetLiveDataWrapped); // NEW: Route for live data
    server.on("/update_all_settings", HTTP_POST, handleUpdateAllSettingsWrapped); // NEW: Single endpoint for saving
    server.on("/update_theme", HTTP_POST, handleUpdateThemeWrapped); // NEW: Endpoint for theme
    server.on("/reset_dry_run", HTTP_POST, handleResetDryRunWrapped);
    server.on("/reset_sensor_error", HTTP_POST, handleResetSensorErrorWrapped);
    server.on("/toggle_pump", HTTP_POST, handleTogglePumpWrapped);
    server.on("/factory_reset", HTTP_POST, handleFactoryResetWrapped); // NEW: Factory reset route
    server.onNotFound(handleNotFoundWrapped); // Handle unknown routes

    // NEW: Add the dedicated handler for AP setup
    server.on("/save_ap_settings", HTTP_POST, handleApSetupSaveWrapped);

    server.begin();
    ESP_LOGI(TAG, "%s
", String("HTTP server started.");
    
    // NEW: Stop the buzzer at the end of setup to ensure it's silent on boot
    stopBuzzer();

    // NEW: Create the background task for NTP synchronization on Core 0
    xTaskCreatePinnedToCore(
      ntpSyncTask,          /* Task function. */
      "NTP Sync Task",      /* name of task. */
      4096,                 /* Stack size of task (increased for network operations) */
      NULL,                 /* parameter of the task */
      1,                    /* priority of the task (low priority) */
      &NtpSyncTaskHandle,   /* Task handle to keep track of created task */
      0);                   /* pin task to core 0 */

    // NEW: Create the LoRa handling task and pin it to Core 0
    xTaskCreatePinnedToCore(
      loraTask,             /* Task function. */
      "LoRa Task",          /* name of task. */
      4096,                 /* Stack size of task */
      NULL,                 /* parameter of the task */
      2,                    /* priority of the task (higher than web/ntp) */
      &LoraTaskHandle,      /* Task handle to keep track of created task */
      0);                   /* pin task to core 0 */

    // NEW: Create the Web UI task and pin it to Core 0
    xTaskCreatePinnedToCore(
      webUiTask,         /* Task function. */
      "Web UI Task",     /* name of task. */
      8192,              /* Stack size of task (increased for web server + JSON) */
      NULL,              /* parameter of the task */
      1,                 /* priority of the task (low priority) */
      NULL,              /* Task handle (not needed) */
      0);                /* pin task to core 0 */
}

static uint8_t lastMinuteChecked = 99; // New: To ensure schedule check runs once per minute

}

void loop_task(void *pvParameters) {
    while(1) {
    // NEW: Handle OTA updates first. This should be called in every loop.
    if (staConnected) { // Only handle OTA if connected to WiFi
      ArduinoOTA.handle();
    }
    
    // If an OTA update is in progress, skip the rest of the loop.
    if (otaUpdateInProgress) {
      return; // The OTA handler has its own timing, just return to yield control.
    }
    
    // REMOVED: The web server is now handled by its own task on Core 0
    // server.handleClient();

    // NEW: Process any LoRa packets waiting in the queue.
    processLoRaQueue();

      // If no web request is active, run the main control logic.
    // This ensures the web server is responsive and does not get starved by other tasks.
    if (!isWebRequestActive) {
      // NEW: Lock the mutex to protect shared data during main loop processing
      xSemaphoreTake(sharedDataMutex, portMAX_DELAY);

      // --- Core functions that must always run, regardless of menu state ---
      // NEW FIX: Add a check to see if we should be ignoring buttons
      if ((uint32_t)(esp_timer_get_time() / 1000) > ignoreButtonsUntil) {
          handleMenuButtons();
      }
      updateBuzzer();
      handleAutoDryRunReset(); // NEW: Check for auto-reset condition
      handleAutoTurbidityReset(); // NEW: Check for auto-reset condition for turbidity
      // handleLoRaAndRxLed(); // DELETED: This is now handled by the LoRa task and queue processing
      handleLEDs();
      updateLiveTurbidityValue(); // NEW: Continuously update turbidity reading
      
      // NEW: Call the dedicated logic for start/end sensor level control.
      // This is called early to ensure stop conditions are checked frequently.
      handleStartEndSensorLogic();

      // If in STA mode and not connected, try to reconnect periodically. Also handle NTP sync.
      if (wifiMode == WIFI_STA_MODE) {
          if (WiFi.status() != WL_CONNECTED) {
              if (staConnected) { // This block runs only on the moment of disconnection
                staConnected = false;
                ntpSyncCompleted = false; // Reset the sync status on disconnect
                ESP_LOGI(TAG, "%s
", String("WiFi connection lost.");
              }
              if ((uint32_t)(esp_timer_get_time() / 1000) - lastWifiConnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
                  ESP_LOGI(TAG, "%s
", String("WiFi STA not connected, attempting reconnect...");
                  connectToWiFiSTA();
              }
          } else { // WiFi is connected
              if (!staConnected) { // This block runs only on the first moment of connection
                  staConnected = true;
                  ESP_LOGI(TAG, "%s
", String("WiFi Connected.");
                  
                  // NEW: Setup OTA now that we are connected
                  setupOTA();
              }
              // REMOVED: The NTP sync logic has been moved to the ntpSyncTask background process
              // to prevent it from blocking the main loop.
          }
      } else { // In AP mode
          staConnected = false;
          ntpSyncCompleted = false;
      }

      // Check if the valve relay delay is over to start the main pump
      if (waitingForPumpRelay && !pumpRunning && ((uint32_t)(esp_timer_get_time() / 1000) - valveRelayStartTime >= primingTimeMs)) {
        ESP_LOGI(TAG, "%s
", String("Priming time elapsed. Starting pump.");
        gpio_set_level(VALVE_RELAY_PIN, 0); // Turn valve relay OFF
        gpio_set_level(RELAY_PIN, 1);       // Turn pump relay ON
        pumpRunning = true;
        pumpStartTime = (uint32_t)(esp_timer_get_time() / 1000);
        if (pumpMode != MANUAL || manualModeSafetyLogicEnabled) { waitingForFlow = true; }
        flowOk = false;
        waitingForPumpRelay = false;
        ESP_LOGI(TAG, "%s
", String("Main pump relay activated.");
      }

      // Get current time from RTC
      DateTime now = rtc.now();

      // MODIFIED: Robustly reset schedule triggered flags at midnight by checking for day change
      if (isRtcWorking) {
        if (lastCheckDay == 0) { // First run initialization
          lastCheckDay = now.day();
        } else if (now.day() != lastCheckDay) {
          ESP_LOGI(TAG, "%s
", String("New day detected. Resetting daily schedule triggers.");
          for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
            // Reset trigger for all repeating schedules. One-time schedules are cleared upon execution.
            if (schedules[i].repeatType != NO_REPEAT && schedules[i].repeatType != SPECIFIC_DATE) {
              scheduleTriggeredToday[i] = false;
            }
          }
          lastCheckDay = now.day();
        }
      }
      
      // If in settings menu, display the menu and handle its specific logic
      if (inSettingsMenu) {
        if (currentMenuPage == PAGE_CALIBRATION) {
            processCalibration();
        }
        showSettingsMenu();
      } else {
        // --- Normal Operation (Not in Settings Menu) ---

        // MODIFIED: Check for scheduled pump runs with a more robust time check
        if (isRtcWorking && pumpMode == SCHEDULE && currentProfile != PROFILE_LEVEL_INDICATOR && now.minute() != lastMinuteChecked) {
          lastMinuteChecked = now.minute(); // Update the minute tracker

          for (uint8_t i = 0; i < MAX_SCHEDULES; i++) {
            // Check if schedule is active (not blank), not already triggered today, and the time (hour and minute) matches.
            if (!scheduleTriggeredToday[i] &&
                !(schedules[i].hour == 0 && schedules[i].minute == 0 && schedules[i].year == 0) &&
                now.hour() == schedules[i].hour &&
                now.minute() == schedules[i].minute) {
              
              bool shouldTrigger = false;
              // Daily schedule check
              if (schedules[i].repeatType == EVERY_DAY) {
                  shouldTrigger = true;
              }
              // Odd/Even day schedule check
              else if (schedules[i].repeatType == ODD_DAY && (now.day() % 2 != 0)) {
                  shouldTrigger = true;
              } else if (schedules[i].repeatType == EVEN_DAY && (now.day() % 2 == 0)) {
                  shouldTrigger = true;
              }
              // Specific date or No Repeat schedule check (No Repeat defaults to today)
              else if (schedules[i].repeatType == SPECIFIC_DATE || schedules[i].repeatType == NO_REPEAT) {
                  uint16_t schedYear = (schedules[i].repeatType == SPECIFIC_DATE) ? schedules[i].year : now.year();
                  uint8_t schedMonth = (schedules[i].repeatType == SPECIFIC_DATE) ? schedules[i].month : now.month();
                  uint8_t schedDay = (schedules[i].repeatType == SPECIFIC_DATE) ? schedules[i].day : now.day();
                  if (now.year() == schedYear && now.month() == schedMonth && now.day() == schedDay) {
                      shouldTrigger = true;
                  }
              }

              if (shouldTrigger) {
                // --- MODIFIED: Automatic dry run reset on scheduled time (UNCONDITIONAL) ---
                // If a scheduled run is due, automatically reset any existing dry run error.
                if (dryRunError) {
                    ESP_LOGI(TAG, "%s
", String("Scheduled run is due. Automatically resetting Dry Run Error.");
                    dryRunError = false;
                    isDryRunErrorBeepActive = false; // Stop any ongoing error beeps.
                    remainingDryRunAttempts = dryRunAttempts; // Restore all retry attempts.
                    dryRunResetMessageActive = true; // Show a confirmation message on the display.
                    dryRunResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                }
                // --- End of modified block ---

                // --- NEW: Automatic TURBIDITY error reset on scheduled time ---
                if (isDirtyWaterError) {
                    ESP_LOGI(TAG, "%s
", String("Scheduled run is due. Automatically resetting Turbidity Error.");
                    isDirtyWaterError = false;
                    // Only clear the main sensorError flag if it was set to DIRTY_WATER
                    if (sensorError == SENSOR_ERROR_DIRTY_WATER) {
                        sensorError = SENSOR_ERROR_NONE; 
                    }
                    isDirtyWaterBeepActive = false; // Stop any ongoing error beeps
                    snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), ""); // Clear details
                    sensorErrorResetMessageActive = true; // Show sensor reset message
                    sensorErrorResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                    
                    // Also reset the auto-reset state machine
                    turbidityErrorWaterAbsent = false;

                    // --- BUG FIX ---
                    // Reset the retry counter, just like we do for a dry run reset.
                    remainingDryRunAttempts = dryRunAttempts; 
                    ESP_LOGI(TAG, "%s
", String("Retry attempts reset for new scheduled run.");
                    // --- END BUG FIX ---
                }
                // --- End of NEW block ---

                bool pumpWasInitiated = false;
                
                // Safety check for critical top sensor faults
                bool topSensorFault = false;
                // ... (topSensorFault checking logic remains the same) ...
                
                if (topSensorFault) {
                    ESP_LOGI(TAG, "%s
", String("Scheduled run blocked: Critical top sensor fault.");
                    pumpBlockedMessageActive = true;
                    pumpBlockedMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
                } else {
                    // initiatePumpStart already contains the check for dryRunError
                    if (currentTankLevel != TANK_FULL || forceScheduledRun) {
                        pumpWasInitiated = initiatePumpStart(true);
                    }
                }

                // Only mark schedule as triggered if the pump actually started.
                if (pumpWasInitiated) {
                  scheduleTriggeredToday[i] = true;
                  
                  // For one-time schedules, clear them after they have triggered.
                  if (schedules[i].repeatType == SPECIFIC_DATE || schedules[i].repeatType == NO_REPEAT) {
                      ESP_LOGI(TAG, "One-time schedule #%d triggered. Clearing it.\n", i + 1);
                      schedules[i] = {0, 0, EVERY_DAY, 0, 0, 0};
                      saveSettingsToEEPROM();
                  }
                }
              }
            }
          }
        }

        // ... (rest of the loop function remains the same) ...
        handleWaterSensingMode(); // NEW: Handle the water sensing mode
        handleScreenRotation();
        readTurbiditySensor();

        if (dryRunLogicEnabled) {
          checkDryRunTimeout();
          checkRetryTrigger();
        }
          
        if (pumpRunning && scheduleDurationMin > 0) {
          if ((uint32_t)(esp_timer_get_time() / 1000) - pumpStartTime >= scheduleDurationMin * 60000UL) {
            ESP_LOGI(TAG, "%s
", String("Pump turning OFF due to scheduled duration.");
            stopPump();
            if (currentPumpRunHadFlow) remainingDryRunAttempts = dryRunAttempts;
          }
        }

        checkDryRunReset();
        updateDisplay();
      }
      // NEW: Release the mutex after processing is done
      xSemaphoreGive(sharedDataMutex);
    } 

    esp_task_wdt_reset(); 
    vTaskDelay(pdMS_TO_TICKS(2);
}


// NEW: Function to set up OTA updates
void setupOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp32-[MAC]
  ArduinoOTA.setHostname("sursajni-pump-controller");

  // No authentication by default
  ArduinoOTA.setPassword("sursajni_ota");

  ArduinoOTA
    .onStart([]() {
      otaUpdateInProgress = true; // Set flag to pause main loop logic
      esp_task_wdt_reset(); // FIX: Reset watchdog at the start of OTA
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS, ensure all filesystem objects are closed
      ESP_LOGI(TAG, "%s
", String("Start updating " + type);
      
      // Display update message on OLED
      display.clearBuffer();
      display.setFont(u8g2_font_7x13_tf);
      display.drawStr((128 - display.getStrWidth("OTA Update...")) / 2, 20, "OTA Update...");
      display.sendBuffer();
    })
    .onEnd([]() {
      esp_task_wdt_reset(); // FIX: Reset watchdog before rebooting
      ESP_LOGI(TAG, "%s
", String("\nEnd");
      display.clearBuffer();
      display.setFont(u8g2_font_7x13_tf);
      display.drawStr((128 - display.getStrWidth("Update Done!")) / 2, 20, "Update Done!");
      display.drawStr((128 - display.getStrWidth("Rebooting...")) / 2, 40, "Rebooting...");
      display.sendBuffer();
      vTaskDelay(pdMS_TO_TICKS(2000);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      esp_task_wdt_reset(); // FIX: Reset watchdog during the update progress
      int percentage = (progress / (total / 100));
      ESP_LOGI(TAG, "Progress: %u%%\r", percentage);

      // Update OLED with progress bar
      display.clearBuffer();
      display.setFont(u8g2_font_7x13_tf);
      display.drawStr((128 - display.getStrWidth("Updating...")) / 2, 20, "Updating...");
      
      char progressText[10];
      sprintf(progressText, "%d%%", percentage);
      display.drawStr((128 - display.getStrWidth(progressText)) / 2, 55, progressText);
      
      // Draw progress bar
      display.drawFrame(10, 30, 108, 12);
      display.drawBox(12, 32, percentage, 8);
      
      display.sendBuffer();
    })
    .onError([](ota_error_t error) {
      otaUpdateInProgress = false; // Reset flag on error
      esp_task_wdt_reset(); // FIX: Reset watchdog on error
      ESP_LOGI(TAG, "Error[%u]: ", error);
      const char* errorMsg = "Unknown Error";
      if (error == OTA_AUTH_ERROR) errorMsg = "Auth Failed";
      else if (error == OTA_BEGIN_ERROR) errorMsg = "Begin Failed";
      else if (error == OTA_CONNECT_ERROR) errorMsg = "Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) errorMsg = "Receive Failed";
      else if (error == OTA_END_ERROR) errorMsg = "End Failed";
      
      ESP_LOGI(TAG, "%s
", String(errorMsg);

      display.clearBuffer();
      display.setFont(u8g2_font_7x13_tf);
      display.drawStr((128 - display.getStrWidth("Update Error!")) / 2, 20, "Update Error!");
      display.drawStr((128 - display.getStrWidth(errorMsg)) / 2, 40, errorMsg);
      display.sendBuffer();
      vTaskDelay(pdMS_TO_TICKS(3000);
    });

  ArduinoOTA.begin();
  ESP_LOGI(TAG, "%s
", String("OTA Ready");
  ESP_LOGI(TAG, "%s", String("IP address: ");
  ESP_LOGI(TAG, "%s
", String(WiFi.localIP());
}

// NEW: Function to automatically reset a dry run error if water is detected at the source.
void handleAutoDryRunReset() {
    // This feature is only active if it's enabled AND there's a dry run error to clear.
    if (!autoResetDryRunOnWater || !dryRunError) {
        return;
    }

    // Use the globally updated turbidity value to check for water presence.
    bool waterIsPresent = (turbidityValue > waterPresenceThreshold);
    static unsigned long autoResetWaterPresentStartTime = 0;

    if (waterIsPresent) {
        // Water has been detected. Start a confirmation timer to ensure it's not a fluke.
        if (autoResetWaterPresentStartTime == 0) {
            autoResetWaterPresentStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            ESP_LOGI(TAG, "%s
", String("Dry Run Error active & Water Detected. Starting auto-reset confirmation timer.");
        } else if ((uint32_t)(esp_timer_get_time() / 1000) - autoResetWaterPresentStartTime >= (waterSensingStartDelaySec * 1000UL)) {
            // The confirmation time has passed, and water is still present. Reset the error.
            ESP_LOGI(TAG, "%s
", String("Auto-reset timer expired. Resetting Dry Run Error now.");
            dryRunError = false;
            isDryRunErrorBeepActive = false; // Stop any ongoing error beeps.
            remainingDryRunAttempts = dryRunAttempts; // Restore all retry attempts.
            dryRunResetMessageActive = true; // Show a confirmation message on the display.
            dryRunResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            autoResetWaterPresentStartTime = 0; // Reset the timer.
        }
    } else {
        // If water is not present, reset the confirmation timer.
        if (autoResetWaterPresentStartTime != 0) {
             autoResetWaterPresentStartTime = 0;
        }
    }
}

// NEW: Function to automatically reset a turbidity error on a new water cycle.
void handleAutoTurbidityReset() {
    // Feature must be enabled and a dirty water error must be active.
    if (!autoResetTurbidityOnError || !isDirtyWaterError) {
        // If the feature is disabled or there's no error, reset the tracking flag.
        if (turbidityErrorWaterAbsent) turbidityErrorWaterAbsent = false;
        return;
    }

    // Use the globally updated turbidity value to check for water presence.
    bool waterIsPresent = (turbidityValue > waterPresenceThreshold);
    
    // --- State Machine for Reset ---

    // State 1: Error is active, waiting for water to disappear.
    if (!turbidityErrorWaterAbsent) {
        if (!waterIsPresent) {
            // Water has disappeared. Set the flag to move to the next state.
            turbidityErrorWaterAbsent = true;
            ESP_LOGI(TAG, "%s
", String("Turbidity Error active & Water Disappeared. Waiting for water to return to reset.");
        }
    } 
    // State 2: Error is active, water has disappeared, now waiting for it to reappear.
    else {
        if (waterIsPresent) {
            // Water has returned. This completes the cycle. Reset the error.
            ESP_LOGI(TAG, "%s
", String("Water has returned. Auto-resetting Turbidity Error.");
            
            // Use the same reset logic as the manual reset
            isDirtyWaterError = false; 
            sensorError = SENSOR_ERROR_NONE;
            isDirtyWaterBeepActive = false;
            snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), ""); // Clear details
            sensorErrorResetMessageActive = true; // Show sensor reset message
            sensorErrorResetMessageStartTime = (uint32_t)(esp_timer_get_time() / 1000);
            
            // Reset the tracking flag for the next cycle.
            turbidityErrorWaterAbsent = false;
        }
    }
}


// NEW: Functions to handle calibration process
void startCleanWaterCalibration() {
    currentCalibrationState = CAL_CLEAN_WATER_SAMPLING;
    calibrationStartTime = (uint32_t)(esp_timer_get_time() / 1000);
    calibrationSampleSum = 0;
    calibrationSampleCount = 0;
    ESP_LOGI(TAG, "%s
", String("Starting clean water calibration...");
}

void startDirtyWaterCalibration() {
    currentCalibrationState = CAL_DIRTY_WATER_SAMPLING;
    calibrationStartTime = (uint32_t)(esp_timer_get_time() / 1000);
    calibrationSampleSum = 0;
    calibrationSampleCount = 0;

    ESP_LOGI(TAG, "%s
", String("Starting dirty water calibration...");
}

void processCalibration() {
    // This function is called only when on the calibration page
    if (currentCalibrationState == CAL_CLEAN_WATER_SAMPLING || currentCalibrationState == CAL_DIRTY_WATER_SAMPLING) {
        if ((uint32_t)(esp_timer_get_time() / 1000) - calibrationStartTime < CALIBRATION_SAMPLE_DURATION_MS) {
            // Still sampling
            calibrationSampleSum += adc1_get_raw(TURBIDITY_SENSOR_PIN);
            calibrationSampleCount++;
        } else {
            // Sampling finished
            if (calibrationSampleCount > 0) {
                uint16_t avgValue = calibrationSampleSum / calibrationSampleCount;
                if (currentCalibrationState == CAL_CLEAN_WATER_SAMPLING) {
                    calibratedCleanValue = avgValue;
                    ESP_LOGI(TAG, "Clean water calibration finished. Avg value: %d\n", calibratedCleanValue);
                } else { // CAL_DIRTY_WATER_SAMPLING
                    calibratedDirtyValue = avgValue;
                    ESP_LOGI(TAG, "Dirty water calibration finished. Avg value: %d\n", calibratedDirtyValue);
                }
                // Automatically set the new limit to be 75% of the way between clean and dirty
                if (calibratedDirtyValue > calibratedCleanValue) {
                   turbidityLimit = calibratedCleanValue + (uint16_t)((calibratedDirtyValue - calibratedCleanValue) * 0.75);
                } else {
                    // If dirty is cleaner than clean for some reason, just use the dirty value as limit.
                    turbidityLimit = calibratedDirtyValue;
                }
                ESP_LOGI(TAG, "New turbidity limit automatically set to: %d\n", turbidityLimit);
            }
            currentCalibrationState = CAL_DONE; // Go to done state to show results
        }
    }
}

// NEW: Function to handle the pump logic based on user-defined start and end sensor levels.
void handleStartEndSensorLogic() {
    // This function provides the primary level-based pump control.
    // It's designed to work across different modes.

    // --- STOP condition ---
    // This is a critical safety feature to prevent overflow.
    // It triggers if the pump is running and the tank reaches the designated 'end' level.
    // In MANUAL mode, it only triggers if the safety logic is enabled.
    if ((pumpRunning || waitingForPumpRelay) && (pumpMode != MANUAL || manualModeSafetyLogicEnabled)) {
        if (currentTankLevel >= endSensorLevel && currentTankLevel != TANK_INVALID) { // TANK_INVALID check is a failsafe
            ESP_LOGI(TAG, "Pump turning OFF: Reached End Sensor Level (%s).\n", getTankLevelString(endSensorLevel));
            stopPump();
            // Reset dry run attempts if the pump stopped successfully due to being full.
            if (currentPumpRunHadFlow) {
                remainingDryRunAttempts = dryRunAttempts;
            }
            return; // Stop condition met, no further action needed.
        }
    }


    // --- START condition ---
    // This condition is primarily for AUTO mode.
    // It starts the pump if it's idle and the tank drops to the designated 'start' level.
    // It won't override a dry run error or an active sensor error (unless bypassed).
    if (!pumpRunning && !waitingForPumpRelay && !dryRunError && (sensorError == SENSOR_ERROR_NONE || sensorErrorBypassEnabled)) {
        // In AUTO mode, this is the main trigger.
        if (pumpMode == AUTO && currentTankLevel <= startSensorLevel) {
             ESP_LOGI(TAG, "Pump turning ON: Reached Start Sensor Level (%s).\n", getTankLevelString(startSensorLevel));
             initiatePumpStart(false); // Not a scheduled run.
        }
    }
}

// === NEW: LoRa Task and Queue Processing Functions ===

// NEW: Task to run on Core 0, solely dedicated to receiving LoRa packets.
void loraTask(void *pvParameters) {
  ESP_LOGI(TAG, "%s
", String("LoRa Task started and pinned to Core 0.");
  esp_task_wdt_add(NULL); // Add this task to the watchdog timer

  unsigned long rxLedOnTime_task = 0; // Local timer for the LED

  for (;;) { // Infinite loop for this task
    esp_task_wdt_reset(); // Pet the watchdog

    // --- RX LED OFF Logic ---
    // Handle turning the LED OFF if its blink timer has expired
    if (rxLedOnTime_task > 0 && (uint32_t)(esp_timer_get_time() / 1000) - rxLedOnTime_task > 100) {
        gpio_set_level(RX_LED_PIN, 0);
        rxLedOnTime_task = 0; // Reset the timer
    }

    // --- Packet Reception Logic ---
    int packetSize = LoRa.parsePacket();
    if (packetSize == sizeof(LoRaPacket)) {
      // A correctly sized packet was received.
      gpio_set_level(RX_LED_PIN, 1); 
      rxLedOnTime_task = (uint32_t)(esp_timer_get_time() / 1000); 
      
      LoRaPacket receivedPacket;
      LoRa.readBytes((uint8_t*)&receivedPacket, sizeof(LoRaPacket));

      // Send the received packet to the queue for processing by the main loop on Core 1
      if (xQueueSend(loraPacketQueue, &receivedPacket, (TickType_t)10) != pdPASS) {
        ESP_LOGI(TAG, "%s
", String("LoRa packet queue is full. Packet dropped.");
      }
    } else if (packetSize > 0) {
      // A packet of the wrong size was received. Flush it.
      ESP_LOGI(TAG, "LoRa packet size mismatch. Expected %d, got %d. Flushing.\n", sizeof(LoRaPacket), packetSize);
      while (LoRa.available()) {
        LoRa.read();
      }
    }

    vTaskDelay(5 / portTICK_PERIOD_MS); // Small delay to yield to other Core 0 tasks
  }
}

// NEW: Function to be called from the main loop to process packets from the LoRa queue.
// This contains the logic from the old handleLoRaAndRxLed() function.
void processLoRaQueue() {
  LoRaPacket receivedPacket;

  // Check if a packet is available in the queue (non-blocking)
  if (xQueueReceive(loraPacketQueue, &receivedPacket, (TickType_t)0) == pdPASS) {
    // A packet was successfully received from the queue. Process it now.
    ESP_LOGI(TAG, "%s
", String("Processing LoRa packet from queue...");

    // Basic packet validation: check group ID and serial ID
    if (receivedPacket.groupID != allowedGroupID || receivedPacket.serialID != allowedSerialID) {
      ESP_LOGI(TAG, "%s
", String("LoRa packet ID mismatch.");
      return; // Invalid Group ID or Serial ID
    }

    // A valid packet was received, so send an acknowledgment now
    sendAcknowledgment(receivedPacket.serialID);

    // Update heartbeat time and last packet process time
    if (isRtcWorking) {
        lastHeartbeatTimeRTC = rtc.now();
    }
    firstHeartbeatReceived = true;
    lastPacketRssi = LoRa.packetRssi(); // Update RSSI here as it's context-sensitive

    // --- NEW: LoRa Power Optimization Logic ---
    if (loraPowerOptimizationEnabled) {
        if (lastPacketRssi > RSSI_TOO_STRONG) {
            // Signal is very strong, tell transmitter to decrease power
            if (transmitterTxPower > 2) { // Don't go below the minimum power
                transmitterTxPower--;
                ESP_LOGI(TAG, "RSSI is strong (%d dBm). Requesting TX power decrease to %d.\n", lastPacketRssi, transmitterTxPower);
                saveSettingsToEEPROM(); // Persist the new auto-adjusted power level
            }
        } else if (lastPacketRssi < RSSI_WEAK) {
            // Signal is weak, tell transmitter to increase power
            if (transmitterTxPower < 20) { // Don't go above the maximum power
                transmitterTxPower++;
                ESP_LOGI(TAG, "RSSI is weak (%d dBm). Requesting TX power increase to %d.\n", lastPacketRssi, transmitterTxPower);
                saveSettingsToEEPROM(); // Persist the new auto-adjusted power level
            }
        }
    }

    // Extract and update all 5 switch states from the struct
    bool newSwitchState[5];
    for (uint8_t i = 0; i < 5; i++) {
      newSwitchState[i] = (receivedPacket.switchStates[i] == 0x01);
      prevSwitchState[i] = newSwitchState[i]; // Store all 5 states
    }

    // Extract battery voltage from the packet (already in millivolts)
    batteryVoltage = receivedPacket.batteryVoltage_mV / 1000.0f;
    
    // Set lowBattery flag based on a voltage threshold
    bool prevLowBattery = lowBattery;
    lowBattery = (batteryVoltage < 3.3);
    if (lowBattery && !prevLowBattery && buzzerEnabled && !isLowBatteryBeepActive) {
      beepForLowBattery();
    } else if (!lowBattery) {
      isLowBatteryBeepActive = false; // Reset the beep flag
    }

    // Extract confirmed settings from the transmitter
    confirmedTxPower = receivedPacket.txPower;
    confirmedAckAttempts = receivedPacket.ackAttempts;
    ESP_LOGI(TAG, "Confirmed settings from transmitter -> Power: %d, ACK Attempts: %d\n", confirmedTxPower, confirmedAckAttempts);

    // Determine current tank level from switch states
    // Pass the four level switches to the function

    // Determine current tank level from switch states
    // UPDATED: Pass the four level switches to the function
    TankLevel incomingTankLevel = getTankLevelFromSwitches(newSwitchState[1], newSwitchState[2], newSwitchState[3], newSwitchState[4]);
    
    // Check for status changes and trigger beeps
    // BUG FIX: The condition was incorrectly preventing tank status beeps in MANUAL mode
    // when safety logic was disabled. Tank alerts should be independent of pump mode.
    if (buzzerEnabled) { // REMOVED: && (pumpMode != MANUAL || manualModeSafetyLogicEnabled)
      // REFACTORED (BUG FIX): This block is rewritten for clarity and robustness.
      // It now uses two separate if/else if blocks to handle EMPTY and FULL states independently,
      // preventing any unintended interactions between the logic checks.

      // Handle EMPTY state beeps and flag reset
      if (incomingTankLevel == TANK_EMPTY && previousTankLevel != TANK_EMPTY) {
          if (!isTankEmptyBeepActive) {
              beepForTankEmpty();
          }
      } else if (incomingTankLevel != TANK_EMPTY) {
          isTankEmptyBeepActive = false;
      }

      // Handle FULL state beeps and flag reset
      if (incomingTankLevel == TANK_FULL && previousTankLevel != TANK_FULL) {
          if (!isTankFullBeepActive) {
              beepForTankFull();
          }
      } else if (incomingTankLevel != TANK_FULL) {
          isTankFullBeepActive = false;
      }
    }
    previousTankLevel = incomingTankLevel;


    // === NEW: INITIAL PAIRING/HEARTBEAT HANDLING ===
    if (lastObservedValidTankLevel == TANK_INVALID) {
      lastObservedValidTankLevel = incomingTankLevel;
      sensorError = SENSOR_ERROR_NONE; // Clear any error from initial state
      snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "");
      ESP_LOGI(TAG, "%s
", String("Initial heartbeat received. Sequence check bypassed.");
    } else {
    // We have a physically valid combination, now check the sequence against the last valid state.
    // Ignore sequence check on the very first packet or if the level hasn't changed.
    if (incomingTankLevel != lastObservedValidTankLevel) {
        // If the combination of switches is physically impossible
        if (incomingTankLevel == TANK_INVALID) {
            sensorError = SENSOR_ERROR_INVALID_COMBINATION;
            // UPDATED: Detailed error messages for the new switch logic
            if (newSwitchState[4] && (!newSwitchState[3] || !newSwitchState[2] || !newSwitchState[1])) {
                snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "100%% on, 75/50/25 off");
            } else if (newSwitchState[3] && (!newSwitchState[2] || !newSwitchState[1])) {
                snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "75%% on, 50/25 off");
            } else if (newSwitchState[2] && !newSwitchState[1]) {
                snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "50%% on, 25 off");
            } else {
                snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "Unknown Comb.");
            }
            ESP_LOGI(TAG, "%s", String("Sensor Error: Invalid switch combination. Detail: ");
            ESP_LOGI(TAG, "%s
", String(sensorErrorDetailMessage);

            if (buzzerEnabled && !isSensorErrorBeepActive) {
                beepForSensorError();
            }
        } else {
            // Check for filling (level is increasing)
            if (incomingTankLevel > lastObservedValidTankLevel) {
                // A valid "up" change should only be one step at a time
                if (incomingTankLevel - lastObservedValidTankLevel > 1) {
                    sensorError = SENSOR_ERROR_SEQUENCE_UP;
                    snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "Jump Up: %s->%s", getTankLevelString(lastObservedValidTankLevel), getTankLevelString(incomingTankLevel));
                }
            }
            // Check for emptying (level is decreasing)
            else if (incomingTankLevel < lastObservedValidTankLevel) {
                // A valid "down" change should only be one step at a time
                if (lastObservedValidTankLevel - incomingTankLevel > 1) {
                    sensorError = SENSOR_ERROR_SEQUENCE_DOWN;
                    snprintf(sensorErrorDetailMessage, sizeof(sensorErrorDetailMessage), "Jump Down: %s->%s", getTankLevelString(lastObservedValidTankLevel), getTankLevelString(incomingTankLevel));
                }
            }
        }
    }
    }


    // Trigger error beep if a new sensor error is detected
    if (sensorError != SENSOR_ERROR_NONE && buzzerEnabled && !isSensorErrorBeepActive) {
      beepForSensorError();
    } else if (sensorError == SENSOR_ERROR_NONE) {
        isSensorErrorBeepActive = false;
    }

    // NEW: Stop the pump immediately if a sensor error occurs, unless bypassed.
    // This is a critical safety check that applies in all modes.
    if (sensorError != SENSOR_ERROR_NONE && !sensorErrorBypassEnabled) {
        if (pumpRunning || waitingForPumpRelay) {
            ESP_LOGI(TAG, "%s
", String("CRITICAL: Stopping pump due to un-bypassed sensor error.");
            stopPump();
            // Since this is an error stop, we don't reset dry run attempts.
        }
    }

    // MODIFIED: Only bypass pump control logic for a dry run error.
    // A sensor error will no longer prevent pump control logic from running.
    bool shouldBypassPumpControl = dryRunError;
    if (shouldBypassPumpControl && !(pumpMode == MANUAL && !manualModeSafetyLogicEnabled)) {
      return;
    }

    // Update currentTankLevel and lastObservedValidTankLevel
    currentTankLevel = incomingTankLevel;
    if (sensorError == SENSOR_ERROR_NONE || sensorErrorBypassEnabled) {
        lastObservedValidTankLevel = incomingTankLevel;
    }

    // --- Pump control logic based on tank levels and flow sensor ---

    // REMOVED: The logic for stopping the pump when full and starting in AUTO mode
    // has been moved to the new handleStartEndSensorLogic() function for better control.

    // Flow Sensor Logic (Switch 0)
    // UPDATED: Check the new flow switch (newSwitchState[0])
    if (pumpRunning && (pumpMode != MANUAL || manualModeSafetyLogicEnabled)) {
      // NEW: Only run flow sensor logic if dry run logic is enabled
      if (dryRunLogicEnabled) {
        if (waitingForFlow && newSwitchState[0]) {
          flowOk = true;
          waitingForFlow = false;
          ESP_LOGI(TAG, "%s
", String("Flow detected. Dry run averted.");
          currentPumpRunHadFlow = true; // NEW: Mark that flow was detected during this run
          isDryRunErrorBeepActive = false; // NEW: Reset the beep flag on successful flow detection.
        } else if (flowOk && !newSwitchState[0]) {
          waitingForFlow = true;
          pumpStartTime = (uint32_t)(esp_timer_get_time() / 1000);
          ESP_LOGI(TAG, "%s
", String("Flow lost. Re-entering waiting for flow state.");
        }
      }
    }
  }
}