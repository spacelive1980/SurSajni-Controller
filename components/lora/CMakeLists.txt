idf_component_register(
    SRCS "src/LoRa.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_spi esp_driver_gpio esp_timer
)
