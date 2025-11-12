idf_component_register(
    SRCS "src/U8g2lib.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c
)
