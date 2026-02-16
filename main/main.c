#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_indicator.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

// WiFi callback functions
void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "=== WiFi Connected Callback ===");
    led_set_system_status(LED_SYSTEM_CONNECTED);
    led_set_ap_mode(false);
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
        ESP_LOGI(TAG, "WiFi credentials found, connecting to WiFi...");
        
        // Try to connect to saved WiFi
        if (wifi_manager_start_sta() == ESP_OK) {
            ESP_LOGI(TAG, "Successfully connected to WiFi!");
            // LED will be set via callback
        } else {
            ESP_LOGE(TAG, "Failed to connect, starting AP mode...");
            wifi_manager_start_ap();
            led_set_ap_mode(true);
            led_set_system_status(LED_SYSTEM_OFF);
        }
    } else {
        ESP_LOGI(TAG, "No WiFi credentials, starting AP mode...");
        wifi_manager_start_ap();
        led_set_ap_mode(true);
        led_set_system_status(LED_SYSTEM_OFF);
    }
    
    // Main loop
    while (1) {
        wifi_state_t state = wifi_manager_get_state();
        
        switch (state) {
            case WIFI_STATE_AP_STARTED:
                ESP_LOGI(TAG, "State: AP Mode - Connect to '%s' and go to %s", 
                         WIFI_AP_SSID, WIFI_AP_IP);
                break;
                
            case WIFI_STATE_STA_CONNECTED:
                ESP_LOGI(TAG, "State: WiFi Connected");
                break;
                
            case WIFI_STATE_STA_DISCONNECTED:
                ESP_LOGI(TAG, "State: WiFi Disconnected (retrying...)");
                break;
                
            case WIFI_STATE_STA_FAILED:
                ESP_LOGE(TAG, "State: WiFi Failed - Switching to AP mode");
                // Could switch to AP mode here as fallback
                break;
                
            default:
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Log every 10 seconds
    }
}