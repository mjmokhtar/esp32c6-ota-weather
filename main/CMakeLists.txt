idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        led_indicator 
        wifi_manager 
        sntp_sync 
        ota_manager 
        web_server
        weather_client
)