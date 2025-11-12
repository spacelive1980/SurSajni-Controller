/**
 * @file wifi_manager.c
 */

#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WIFI";

esp_err_t wifi_manager_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create AP and STA netif
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "Sursajni-Controller",
            .password = "sursajni123",
            .ssid_len = strlen("Sursajni-Controller"),
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started: SSID=%s", ap_config.ap.ssid);
    ESP_LOGI(TAG, "WiFi initialized");
    
    return ESP_OK;
}

bool wifi_is_connected(void) {
    return true; // AP is always "connected"
}
