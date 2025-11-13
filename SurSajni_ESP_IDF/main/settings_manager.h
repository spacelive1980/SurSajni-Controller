/**
 * @file settings_manager.h
 * @brief Settings management with NVS encryption support
 */

#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include "esp_err.h"
#include "common.h"

// Settings structure
typedef struct {
    // System
    system_profile_t system_profile;
    
    // Pump
    pump_mode_t pump_mode;
    uint8_t start_sensor_level;
    uint8_t end_sensor_level;
    uint16_t schedule_duration_min;
    uint16_t priming_time_ms;
    bool force_scheduled_run;
    bool manual_mode_safety_enabled;
    
    // Safety
    bool dry_run_logic_enabled;
    uint16_t dry_run_delay_min;
    bool retry_logic_enabled;
    uint8_t dry_run_attempts;
    uint16_t retry_interval_min;
    bool sensor_error_bypass_enabled;
    bool auto_reset_dry_run_on_water;
    uint16_t water_presence_threshold;
    uint16_t water_sensing_start_delay_sec;
    uint16_t water_sensing_stop_delay_sec;
    
    // Buzzer
    bool buzzer_enabled;
    uint8_t buzzer_volume;
    beep_style_t dry_run_beep_style;
    beep_style_t sensor_error_beep_style;
    beep_style_t low_battery_beep_style;
    beep_style_t heartbeat_expired_beep_style;
    beep_style_t tank_empty_beep_style;
    beep_style_t tank_full_beep_style;
    beep_style_t dirty_water_beep_style;
    
    // Turbidity
    bool turbidity_bypass_enabled;
    uint16_t turbidity_timeout_sec;
    uint16_t turbidity_limit;
    uint16_t calibrated_clean_value;
    uint16_t calibrated_dirty_value;
    
    // Display
    uint8_t oled_theme;
    uint8_t web_theme;
    uint16_t web_ui_refresh_interval_sec;
    uint8_t current_font_index;
    bool oled_show_time;
    bool oled_show_date;
    bool oled_show_day;
    bool oled_show_battery;
    
    // LoRa
    bool lora_power_optimization_enabled;
    uint8_t transmitter_tx_power;
    uint8_t transmitter_ack_attempts;
    
    // Security
    char http_username[32];
    char http_password[32];
    
    // WiFi
    char wifi_ssid[32];
    char wifi_password[64];
    
    // Schedules
    schedule_t schedules[MAX_SCHEDULES];
    
} settings_t;

/**
 * @brief Initialize settings manager
 * @return ESP_OK on success
 */
esp_err_t settings_manager_init(void);

/**
 * @brief Get current settings
 * @return Pointer to settings structure
 */
const settings_t* settings_get(void);

/**
 * @brief Update settings
 * @param settings New settings to save
 * @return ESP_OK on success
 */
esp_err_t settings_save(const settings_t *settings);

/**
 * @brief Reset to factory defaults
 * @return ESP_OK on success
 */
esp_err_t settings_factory_reset(void);

#endif // SETTINGS_MANAGER_H
