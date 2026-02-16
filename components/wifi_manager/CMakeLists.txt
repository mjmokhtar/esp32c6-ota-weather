idf_component_register(
    SRCS "wifi_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_netif lwip esp_http_server json led_indicator
)