#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

// Components
#include "led_indicator.h"
#include "wifi_manager.h"
#include "sntp_sync.h"
#include "ota_manager.h"
#include "web_server.h"
#include "weather_client.h"

static const char *TAG = "MAIN";

// ============================================================================
// WiFi Callbacks
// ============================================================================

void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "╔════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   WiFi Connected Successfully!     ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════╝");
    
    // Update LED status
    led_set_system_status(LED_SYSTEM_CONNECTED);
    
    // Initialize SNTP time sync
    ESP_LOGI(TAG, "Starting SNTP time synchronization...");
    sntp_sync_init();

    // Initialize weather client
    ESP_LOGI(TAG, "Initializing weather client...");
    weather_client_init();
    weather_client_start();
    
    // Start web server (if not already running)
    if (!web_server_is_running()) {
        ESP_LOGI(TAG, "Starting web server...");
        web_server_start();
    }
    
    ESP_LOGI(TAG, "All services initialized successfully!");
}

void on_wifi_disconnected(void)
{
    ESP_LOGW(TAG, "WiFi disconnected!");
    led_set_system_status(LED_SYSTEM_OFF);
}

// ============================================================================
// Main Application
// ============================================================================

void app_main(void)
{
    // Print startup banner
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                                           ║");
    ESP_LOGI(TAG, "║     ESP32-C6 OTA Weather Station          ║");
    ESP_LOGI(TAG, "║                                           ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Firmware Version: %s", ota_manager_get_version());
    ESP_LOGI(TAG, "Running Partition: %s", ota_manager_get_partition());
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Initializing system components...");
    ESP_LOGI(TAG, "");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "✓ NVS initialized");
    
    // Initialize LED indicators
    led_init();
    led_start_blink_task();
    ESP_LOGI(TAG, "✓ LED indicators initialized (GPIO 4, 5, 6)");
    
    // Initialize OTA manager
    ota_manager_init();
    ESP_LOGI(TAG, "✓ OTA manager initialized");
    
    // Initialize WiFi Manager
    wifi_manager_init();
    ESP_LOGI(TAG, "✓ WiFi manager initialized");
    
    // Set WiFi callbacks
    wifi_manager_set_connected_callback(on_wifi_connected);
    wifi_manager_set_disconnected_callback(on_wifi_disconnected);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Checking WiFi configuration...");
    
    // Check if WiFi credentials exist
    if (wifi_manager_has_credentials()) {
        wifi_credentials_t creds;
        wifi_manager_load_credentials(&creds);
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔════════════════════════════════════╗");
        ESP_LOGI(TAG, "║  WiFi Credentials Found!           ║");
        ESP_LOGI(TAG, "╠════════════════════════════════════╣");
        ESP_LOGI(TAG, "║  SSID: %-28s║", creds.ssid);
        ESP_LOGI(TAG, "╚════════════════════════════════════╝");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Starting APSTA mode (AP + STA)...");
        
        // Start APSTA mode (AP + STA)
        wifi_manager_start_apsta_auto();
        
        // LED: AP always ON in APSTA mode
        led_set_ap_mode(true);
        
        // Start web server
        ESP_LOGI(TAG, "Starting web server...");
        web_server_start();
        
    } else {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔════════════════════════════════════╗");
        ESP_LOGI(TAG, "║  No WiFi Credentials Found         ║");
        ESP_LOGI(TAG, "╠════════════════════════════════════╣");
        ESP_LOGI(TAG, "║  Starting AP mode for setup...     ║");
        ESP_LOGI(TAG, "╚════════════════════════════════════╝");
        ESP_LOGI(TAG, "");
        
        // Start AP mode only
        wifi_manager_start_ap();
        led_set_ap_mode(true);
        led_set_system_status(LED_SYSTEM_OFF);
        
        // Start web server
        ESP_LOGI(TAG, "Starting web server...");
        web_server_start();
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "System initialization complete!");
    ESP_LOGI(TAG, "");
    
    // Main loop - status reporting
    // Main loop - status reporting
    while (1) {
        wifi_state_t state = wifi_manager_get_state();
        
        if (state == WIFI_STATE_STA_CONNECTED) {
            esp_netif_t *netif = wifi_manager_get_sta_netif();
            if (netif) {
                esp_netif_ip_info_t ip_info;
                esp_netif_get_ip_info(netif, &ip_info);
                
                // Convert IP to string SAFELY
                char ip_str[16];
                char gw_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
                snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&ip_info.gw));
                
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
                ESP_LOGI(TAG, "║          System Status Report             ║");
                ESP_LOGI(TAG, "╠═══════════════════════════════════════════╣");
                ESP_LOGI(TAG, "║  WiFi Status:  CONNECTED ✓                ║");
                ESP_LOGI(TAG, "║  STA IP:       %-26s ║", ip_str);
                ESP_LOGI(TAG, "║  AP IP:        %-26s ║", WIFI_AP_IP);
                ESP_LOGI(TAG, "║  Gateway:      %-26s ║", gw_str);
                ESP_LOGI(TAG, "║  Time:         %-26s ║", sntp_sync_get_time_str());
                ESP_LOGI(TAG, "╠═══════════════════════════════════════════╣");
                ESP_LOGI(TAG, "║          Web Interfaces                   ║");
                ESP_LOGI(TAG, "╠═══════════════════════════════════════════╣");
                
                char url[64];
                snprintf(url, sizeof(url), "http://%s/", ip_str);
                ESP_LOGI(TAG, "║  WiFi Setup:   %-26s ║", url);
                
                snprintf(url, sizeof(url), "http://%s/ota", ip_str);
                ESP_LOGI(TAG, "║  OTA Update:   %-26s ║", url);
                
                snprintf(url, sizeof(url), "http://%s/api/status", ip_str);
                ESP_LOGI(TAG, "║  Status API:   %-26s ║", url);
                
                ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
                ESP_LOGI(TAG, "");
            }
        } else {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
            ESP_LOGI(TAG, "║          System Status Report             ║");
            ESP_LOGI(TAG, "╠═══════════════════════════════════════════╣");
            ESP_LOGI(TAG, "║  WiFi Status:  AP MODE ONLY               ║");
            ESP_LOGI(TAG, "║  AP SSID:      %-26s ║", WIFI_AP_SSID);
            ESP_LOGI(TAG, "║  AP IP:        %-26s ║", WIFI_AP_IP);
            ESP_LOGI(TAG, "║  AP Password:  %-26s ║", WIFI_AP_PASSWORD);
            ESP_LOGI(TAG, "╠═══════════════════════════════════════════╣");
            ESP_LOGI(TAG, "║          Setup Instructions               ║");
            ESP_LOGI(TAG, "╠═══════════════════════════════════════════╣");
            ESP_LOGI(TAG, "║  1. Connect to WiFi: %s      ║", WIFI_AP_SSID);
            ESP_LOGI(TAG, "║  2. Open browser: http://%-15s ║", WIFI_AP_IP);
            ESP_LOGI(TAG, "║  3. Configure your WiFi credentials       ║");
            ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
            ESP_LOGI(TAG, "");
        }
        
        // Wait 30 seconds before next status report
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}