/**
 * @file sensor_manager.c
 */

#include "sensor_manager.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "SENSOR";

esp_err_t sensor_manager_init(void) {
    // Configure tank level sensor pins
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << GPIO_TANK_LEVEL_0) | 
                        (1ULL << GPIO_TANK_LEVEL_1) |
                        (1ULL << GPIO_TANK_LEVEL_2) |
                        (1ULL << GPIO_TANK_LEVEL_3) |
                        (1ULL << GPIO_TANK_LEVEL_4)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Configure ADC for analog sensors
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // GPIO34
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); // GPIO35
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11); // GPIO36
    
    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

uint8_t sensor_get_tank_level(void) {
    if (!gpio_get_level(GPIO_TANK_LEVEL_4)) return 4;
    if (!gpio_get_level(GPIO_TANK_LEVEL_3)) return 3;
    if (!gpio_get_level(GPIO_TANK_LEVEL_2)) return 2;
    if (!gpio_get_level(GPIO_TANK_LEVEL_1)) return 1;
    if (!gpio_get_level(GPIO_TANK_LEVEL_0)) return 0;
    return 0;
}

uint16_t sensor_get_turbidity(void) {
    return adc1_get_raw(ADC1_CHANNEL_6); // GPIO34
}

float sensor_get_battery_voltage(void) {
    int raw = adc1_get_raw(ADC1_CHANNEL_0); // GPIO36
    float voltage = (raw / (float)ADC_MAX_VALUE) * (ADC_VREF_MV / 1000.0f) * BATTERY_DIVIDER_RATIO;
    return voltage;
}

bool sensor_get_flow_ok(void) {
    int raw = adc1_get_raw(ADC1_CHANNEL_7); // GPIO35
    return (raw > 500); // Threshold
}
