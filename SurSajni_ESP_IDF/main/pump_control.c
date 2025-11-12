/**
 * @file pump_control.c
 * @brief Pump control implementation
 */

#include "pump_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PUMP";
static bool pump_running = false;
static bool pump_priming = false;
static uint64_t pump_start_time = 0;
static uint64_t pump_priming_start_time = 0;

esp_err_t pump_control_init(void) {
    // Configure pump relay pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_PUMP_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_PUMP_RELAY, 0);
    
    ESP_LOGI(TAG, "Pump control initialized");
    return ESP_OK;
}

esp_err_t pump_start(void) {
    if (pump_running) {
        ESP_LOGW(TAG, "Pump already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting pump");
    gpio_set_level(GPIO_PUMP_RELAY, 1);
    pump_running = true;
    pump_start_time = esp_timer_get_time() / 1000; // Convert to ms
    
    return ESP_OK;
}

esp_err_t pump_stop(void) {
    if (!pump_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping pump");
    gpio_set_level(GPIO_PUMP_RELAY, 0);
    pump_running = false;
    pump_priming = false;
    
    return ESP_OK;
}

bool pump_is_running(void) {
    return pump_running;
}
