#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_indicator.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C6 LED Indicator Test ===");
    
    // Initialize LED
    led_init();
    
    // Start blink task
    led_start_blink_task();
    
    ESP_LOGI(TAG, "Starting LED test sequence...");
    
    // Test 1: AP Mode ON
    ESP_LOGI(TAG, "TEST 1: AP Mode ON (LED 6 should be solid ON)");
    led_set_ap_mode(true);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Test 2: WiFi Connected
    ESP_LOGI(TAG, "TEST 2: WiFi Connected (LED 4 solid ON, LED 6 OFF)");
    led_set_ap_mode(false);
    led_set_system_status(LED_SYSTEM_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Test 3: Weather Fetch (simulate)
    ESP_LOGI(TAG, "TEST 3: Weather Fetch (LED 5 blinking)");
    led_set_weather_fetch(true);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Simulate API call
    led_set_weather_fetch(false); // Will turn ON for 2 seconds then OFF
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 4: OTA Update
    ESP_LOGI(TAG, "TEST 4: OTA Update (LED 4 blink fast 200ms)");
    led_set_system_status(LED_SYSTEM_OTA_UPDATING);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Test 5: Recovery Mode
    ESP_LOGI(TAG, "TEST 5: Recovery Mode (LED 4 blink slow 1000ms)");
    led_set_system_status(LED_SYSTEM_RECOVERY);
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Test 6: Back to Connected
    ESP_LOGI(TAG, "TEST 6: Back to WiFi Connected");
    led_set_system_status(LED_SYSTEM_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 7: WiFi Disconnected
    ESP_LOGI(TAG, "TEST 7: WiFi Disconnected (All LEDs OFF)");
    led_set_system_status(LED_SYSTEM_OFF);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Loop: Simulate normal operation
    ESP_LOGI(TAG, "Entering normal operation loop...");
    led_set_system_status(LED_SYSTEM_CONNECTED);
    
    while (1) {
        // Simulate weather fetch every 10 seconds (instead of 1 hour for demo)
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        ESP_LOGI(TAG, "Fetching weather...");
        led_set_weather_fetch(true);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Simulate API call
        led_set_weather_fetch(false);
        
        ESP_LOGI(TAG, "Weather fetched. Temperature: 29.5°C, Humidity: 73%%");
    }
}