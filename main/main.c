#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "led_indicator.h"
#include "wifi_manager.h"
#include "sntp_sync.h"

static const char *TAG = "MAIN";

// WiFi callback functions
void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "=== WiFi Connected Callback ===");
    led_set_system_status(LED_SYSTEM_CONNECTED);
    led_set_ap_mode(false);
    // Initialize SNTP time sync
    sntp_sync_init();
    
    // Stop provisioning server
    wifi_manager_stop_webserver();
    
    // Start OTA server (nanti kita buat)
    // ota_server_init();  // <-- Next step
    
    // Sementara start web server biasa dulu untuk test
    wifi_manager_start_webserver();
}

void on_wifi_disconnected(void)
{
    ESP_LOGI(TAG, "=== WiFi Disconnected Callback ===");
    led_set_system_status(LED_SYSTEM_OFF);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C6 OTA Weather Project ===");
    
    // Initialize LED
    led_init();
    led_start_blink_task();
    
    // Initialize WiFi Manager
    wifi_manager_init();
    
    // Set callbacks
    wifi_manager_set_connected_callback(on_wifi_connected);
    wifi_manager_set_disconnected_callback(on_wifi_disconnected);
    
    // Check if WiFi credentials exist
    if (wifi_manager_has_credentials()) {
        ESP_LOGI(TAG, "WiFi credentials found, starting APSTA mode...");
        
        // Start APSTA mode (AP + STA)
        wifi_manager_start_apsta_auto();
        
        // Start web server for provisioning
        wifi_manager_start_webserver();
        
        // LED: AP always ON in APSTA mode
        led_set_ap_mode(true);
        
    } else {
        ESP_LOGI(TAG, "No WiFi credentials, starting AP mode only...");
        wifi_manager_start_ap();
        wifi_manager_start_webserver();
        led_set_ap_mode(true);
        led_set_system_status(LED_SYSTEM_OFF);
    }
    
    // Main loop
    while (1) {
        wifi_state_t state = wifi_manager_get_state();
        
        if (state == WIFI_STATE_STA_CONNECTED) {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif) {
                esp_netif_ip_info_t ip_info;
                esp_netif_get_ip_info(netif, &ip_info);
                ESP_LOGI(TAG, "APSTA Mode - STA IP: " IPSTR " | AP IP: %s", 
                         IP2STR(&ip_info.ip), WIFI_AP_IP);
            }
        } else {
            ESP_LOGI(TAG, "AP Mode Active: %s", WIFI_AP_IP);
        }
        
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}