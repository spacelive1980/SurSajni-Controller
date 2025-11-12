/**
 * @file settings_manager.c
 * @brief Settings management implementation
 */

#include "settings_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SETTINGS";
static const char *NVS_NAMESPACE = "sursajni";
static settings_t g_settings;

// Default settings
static void load_defaults(settings_t *settings) {
    memset(settings, 0, sizeof(settings_t));
    
    // System
    settings->system_profile = PROFILE_FULLY_AUTOMATIC;
    
    // Pump
    settings->pump_mode = PUMP_MODE_AUTO;
    settings->start_sensor_level = 0;
    settings->end_sensor_level = 4;
    settings->schedule_duration_min = 0;
    settings->priming_time_ms = 2000;
    settings->force_scheduled_run = false;
    settings->manual_mode_safety_enabled = true;
    
    // Safety
    settings->dry_run_logic_enabled = true;
    settings->dry_run_delay_min = 5;
    settings->retry_logic_enabled = true;
    settings->dry_run_attempts = 3;
    settings->retry_interval_min = 10;
    settings->sensor_error_bypass_enabled = false;
    settings->auto_reset_dry_run_on_water = true;
    settings->water_presence_threshold = 500;
    settings->water_sensing_start_delay_sec = 3;
    settings->water_sensing_stop_delay_sec = 5;
    
    // Buzzer
    settings->buzzer_enabled = true;
    settings->buzzer_volume = 128;
    settings->dry_run_beep_style = BEEP_ALERT;
    settings->sensor_error_beep_style = BEEP_WARNING;
    settings->low_battery_beep_style = BEEP_PULSE;
    settings->heartbeat_expired_beep_style = BEEP_WARNING;
    settings->tank_empty_beep_style = BEEP_ALERT;
    settings->tank_full_beep_style = BEEP_SPARROW;
    settings->dirty_water_beep_style = BEEP_WARNING;
    
    // Turbidity
    settings->turbidity_bypass_enabled = false;
    settings->turbidity_timeout_sec = 30;
    settings->turbidity_limit = 1500;
    settings->calibrated_clean_value = 0;
    settings->calibrated_dirty_value = 4095;
    
    // Display
    settings->oled_theme = 0;
    settings->web_theme = 0;
    settings->web_ui_refresh_interval_sec = 10;
    settings->current_font_index = 0;
    settings->oled_show_time = true;
    settings->oled_show_date = true;
    settings->oled_show_day = true;
    settings->oled_show_battery = true;
    
    // LoRa
    settings->lora_power_optimization_enabled = false;
    settings->transmitter_tx_power = 17;
    settings->transmitter_ack_attempts = 3;
    
    // Security
    strcpy(settings->http_username, "admin");
    strcpy(settings->http_password, "admin");
    
    // WiFi
    strcpy(settings->wifi_ssid, "");
    strcpy(settings->wifi_password, "");
}

esp_err_t settings_manager_init(void) {
    esp_err_t err;
    nvs_handle_t handle;
    
    // Load defaults first
    load_defaults(&g_settings);
    
    // Open NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved settings found, using defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // Load settings from NVS
    size_t required_size = sizeof(settings_t);
    err = nvs_get_blob(handle, "settings", &g_settings, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved settings blob, using defaults");
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error reading settings: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Settings loaded from NVS");
    }
    
    nvs_close(handle);
    return ESP_OK;
}

const settings_t* settings_get(void) {
    return &g_settings;
}

esp_err_t settings_save(const settings_t *settings) {
    esp_err_t err;
    nvs_handle_t handle;
    
    // Update global settings
    memcpy(&g_settings, settings, sizeof(settings_t));
    
    // Open NVS for writing
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS for write: %s", esp_err_to_name(err));
        return err;
    }
    
    // Save settings blob
    err = nvs_set_blob(handle, "settings", settings, sizeof(settings_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing settings: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }
    
    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Settings saved successfully");
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t settings_factory_reset(void) {
    ESP_LOGW(TAG, "Performing factory reset");
    
    // Load defaults
    load_defaults(&g_settings);
    
    // Save defaults to NVS
    return settings_save(&g_settings);
}
