idf_component_register(
    SRCS "led_indicator.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
)