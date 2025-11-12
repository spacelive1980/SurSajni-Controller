/**
 * @file web_server.c
 */

#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "settings_manager.h"
#include "sensor_manager.h"
#include "pump_control.h"
#include "cJSON.h"

static const char *TAG = "WEB";
static httpd_handle_t server = NULL;

// Root handler
static esp_err_t root_handler(httpd_req_t *req) {
    const char *html = "<html><body><h1>Sursajni Controller ESP-IDF</h1>"
                      "<p>Firmware: v1.0.0-ESP-IDF</p>"
                      "<p>Flash Encryption: Supported</p>"
                      "<p><a href='/get_settings'>View Settings</a></p>"
                      "</body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Get settings handler
static esp_err_t get_settings_handler(httpd_req_t *req) {
    const settings_t *settings = settings_get();
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmwareVersion", "1.0.0-ESP-IDF");
    cJSON_AddNumberToObject(root, "pumpMode", settings->pump_mode);
    cJSON_AddBoolToObject(root, "pumpRunning", pump_is_running());
    cJSON_AddNumberToObject(root, "tankLevel", sensor_get_tank_level());
    cJSON_AddNumberToObject(root, "turbidity", sensor_get_turbidity());
    cJSON_AddNumberToObject(root, "batteryVoltage", sensor_get_battery_voltage());
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Toggle pump handler
static esp_err_t toggle_pump_handler(httpd_req_t *req) {
    if (pump_is_running()) {
        pump_stop();
        httpd_resp_send(req, "Pump stopped", HTTPD_RESP_USE_STRLEN);
    } else {
        pump_start();
        httpd_resp_send(req, "Pump started", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

esp_err_t web_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    // Register handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    httpd_register_uri_handler(server, &root_uri);
    
    httpd_uri_t settings_uri = {
        .uri = "/get_settings",
        .method = HTTP_GET,
        .handler = get_settings_handler,
    };
    httpd_register_uri_handler(server, &settings_uri);
    
    httpd_uri_t toggle_uri = {
        .uri = "/toggle_pump",
        .method = HTTP_POST,
        .handler = toggle_pump_handler,
    };
    httpd_register_uri_handler(server, &toggle_uri);
    
    ESP_LOGI(TAG, "Web server started on port %d", config.server_port);
    return ESP_OK;
}
