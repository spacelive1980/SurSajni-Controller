/**
 * @file main.c
 * @brief Sursajni Wireless Pump Controller - ESP-IDF Version
 * 
 * This is the ESP-IDF native implementation with full flash encryption support.
 * 
 * Features:
 * - Flash encryption compatible
 * - Secure boot ready
 * - NVS encryption support
 * - Optimized for ESP-IDF v4.4+
 * 
 * @copyright Sursajni Automations © 2025
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_flash_encrypt.h"
#include "nvs_flash.h"

#include "pump_control.h"
#include "sensor_manager.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "settings_manager.h"

static const char *TAG = "MAIN";

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Sursajni Controller ESP-IDF v1.0.0  ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    // Check if flash encryption is enabled
    if (esp_flash_encryption_enabled()) {
        ESP_LOGI(TAG, "Flash encryption: ENABLED");
    } else {
        ESP_LOGW(TAG, "Flash encryption: DISABLED");
        ESP_LOGW(TAG, "For production, enable flash encryption!");
    }

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize settings manager (loads from NVS)
    ret = settings_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize settings manager");
        return;
    }
    ESP_LOGI(TAG, "Settings manager initialized");

    // Initialize sensor manager
    ret = sensor_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor manager");
    }
    ESP_LOGI(TAG, "Sensor manager initialized");

    // Initialize pump control
    ret = pump_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pump control");
    }
    ESP_LOGI(TAG, "Pump control initialized");

    // Initialize WiFi
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    }
    ESP_LOGI(TAG, "WiFi manager initialized");

    // Initialize web server (after WiFi)
    ret = web_server_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize web server");
    }
    ESP_LOGI(TAG, "Web server initialized");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "System initialization complete!");
    ESP_LOGI(TAG, "Firmware: v1.0.0-ESP-IDF");
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "");

    // Main loop - monitor system health
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10 seconds
        
        // Log system health
        ESP_LOGD(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
        ESP_LOGD(TAG, "Min free heap: %d bytes", esp_get_minimum_free_heap_size());
    }
}
