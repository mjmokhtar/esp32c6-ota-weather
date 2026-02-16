idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json wifi_manager ota_manager sntp_sync led_indicator weather_client
)