# Main component CMakeLists.txt

idf_component_register(
    SRCS 
        "main.c"
        "pump_control.c"
        "sensor_manager.c"
        "web_server.c"
        "wifi_manager.c"
        "settings_manager.c"
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash
        esp_http_server
        esp_wifi
        driver
        esp_timer
        esp_netif
        lwip
        json
        spiffs
        fatfs
)
