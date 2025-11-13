/**
 * @file common.h
 * @brief Common definitions and types for Sursajni Controller
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

// GPIO Pin definitions
#define GPIO_PUMP_RELAY     26
#define GPIO_BUZZER         25
#define GPIO_TURBIDITY      34  // ADC1
#define GPIO_FLOW_SENSOR    35  // ADC1
#define GPIO_BATTERY        36  // ADC1
#define GPIO_TANK_LEVEL_0   32  // Empty
#define GPIO_TANK_LEVEL_1   33  // 25%
#define GPIO_TANK_LEVEL_2   27  // 50%
#define GPIO_TANK_LEVEL_3   14  // 75%
#define GPIO_TANK_LEVEL_4   12  // Full

// I2C pins
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_FREQ_HZ         100000

// SPI pins for LoRa
#define SPI_MOSI_PIN        23
#define SPI_MISO_PIN        19
#define SPI_SCK_PIN         18
#define LORA_CS_PIN         5
#define LORA_RST_PIN        4
#define LORA_DIO0_PIN       2

// ADC constants
#define ADC_MAX_VALUE       4095
#define ADC_VREF_MV         3300
#define BATTERY_DIVIDER_RATIO 5.0f

// Pump modes
typedef enum {
    PUMP_MODE_SCHEDULE = 0,
    PUMP_MODE_AUTO = 1,
    PUMP_MODE_MANUAL = 2,
    PUMP_MODE_WATER_SENSING = 3
} pump_mode_t;

// Repeat types for schedules
typedef enum {
    REPEAT_EVERY_DAY = 0,
    REPEAT_ODD_DAY = 1,
    REPEAT_EVEN_DAY = 2,
    REPEAT_NO_REPEAT = 3,
    REPEAT_SPECIFIC_DATE = 4
} repeat_type_t;

// Beep styles
typedef enum {
    BEEP_SILENT = 0,
    BEEP_ALERT = 1,
    BEEP_WARNING = 2,
    BEEP_PULSE = 3,
    BEEP_LONG = 4,
    BEEP_SPARROW = 5
} beep_style_t;

// Sensor errors
typedef enum {
    SENSOR_ERROR_NONE = 0,
    SENSOR_ERROR_TANK_LEVEL = 1,
    SENSOR_ERROR_FLOW = 2,
    SENSOR_ERROR_TURBIDITY = 3,
    SENSOR_ERROR_LORA = 4
} sensor_error_t;

// System profile
typedef enum {
    PROFILE_LEVEL_INDICATOR = 0,
    PROFILE_MONOBLOCK = 1,
    PROFILE_SUBMERSIBLE = 2,
    PROFILE_FULLY_AUTOMATIC = 3
} system_profile_t;

// Schedule structure
typedef struct {
    uint8_t hour;
    uint8_t minute;
    repeat_type_t repeat_type;
    uint16_t year;
    uint8_t month;
    uint8_t day;
} schedule_t;

#define MAX_SCHEDULES 10

#endif // COMMON_H
