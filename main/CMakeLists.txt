idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer esp_http_server nvs_flash esp_wifi esp_netif 
                             lwip driver spi_flash esp_adc mbedtls esp_eth esp_event
                             freertos esp_system esp_hw_support esp_driver_gpio esp_driver_spi
                             esp_driver_i2c esp_driver_ledc wpa_supplicant esp_phy
                             lora u8g2 ds3231)
