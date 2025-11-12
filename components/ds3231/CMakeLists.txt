idf_component_register(
    SRCS "src/RTClib.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c
)
