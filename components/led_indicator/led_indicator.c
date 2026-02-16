#include "led_indicator.h"
#include "esp_log.h"

static const char *TAG = "LED_INDICATOR";

// Current LED states
static led_system_status_t current_system_status = LED_SYSTEM_OFF;
static bool weather_fetch_active = false;
static bool ap_mode_active = false;

// Blink task handle
static TaskHandle_t blink_task_handle = NULL;

/**
 * Initialize all LED GPIO pins
 */
void led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_SYSTEM_STATUS) | 
                        (1ULL << LED_WEATHER_FETCH) | 
                        (1ULL << LED_AP_MODE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Turn off all LEDs initially
    gpio_set_level(LED_SYSTEM_STATUS, 0);
    gpio_set_level(LED_WEATHER_FETCH, 0);
    gpio_set_level(LED_AP_MODE, 0);

    ESP_LOGI(TAG, "LED initialized - GPIO4: System, GPIO5: Weather, GPIO6: AP");
}

/**
 * Set system status LED behavior
 */
void led_set_system_status(led_system_status_t status) {
    current_system_status = status;
    ESP_LOGI(TAG, "System status changed to: %d", status);
}

/**
 * Set weather fetch LED
 */
void led_set_weather_fetch(bool active) {
    weather_fetch_active = active;
    
    if (active) {
        ESP_LOGI(TAG, "Weather fetch started - LED blinking");
    } else {
        // Turn ON for 2 seconds after fetch complete
        gpio_set_level(LED_WEATHER_FETCH, 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
        gpio_set_level(LED_WEATHER_FETCH, 0);
        ESP_LOGI(TAG, "Weather fetch completed");
    }
}

/**
 * Set AP mode LED
 */
void led_set_ap_mode(bool active) {
    ap_mode_active = active;
    gpio_set_level(LED_AP_MODE, active ? 1 : 0);
    ESP_LOGI(TAG, "AP mode %s", active ? "ACTIVE" : "INACTIVE");
}

/**
 * LED blink task - handles all blinking patterns
 */
static void led_blink_task(void *pvParameters) {
    uint32_t blink_interval_ms = 500;
    bool led_state = false;
    uint32_t weather_blink_counter = 0;

    while (1) {
        // Handle System Status LED
        switch (current_system_status) {
            case LED_SYSTEM_OFF:
                gpio_set_level(LED_SYSTEM_STATUS, 0);
                break;

            case LED_SYSTEM_CONNECTED:
                gpio_set_level(LED_SYSTEM_STATUS, 1);
                break;

            case LED_SYSTEM_OTA_UPDATING:
                // Blink fast (200ms interval)
                blink_interval_ms = 200;
                led_state = !led_state;
                gpio_set_level(LED_SYSTEM_STATUS, led_state);
                break;

            case LED_SYSTEM_RECOVERY:
                // Blink slow (1000ms interval)
                blink_interval_ms = 1000;
                led_state = !led_state;
                gpio_set_level(LED_SYSTEM_STATUS, led_state);
                break;
        }

        // Handle Weather Fetch LED (blink while active)
        if (weather_fetch_active) {
            weather_blink_counter++;
            if (weather_blink_counter % 2 == 0) {
                gpio_set_level(LED_WEATHER_FETCH, 1);
            } else {
                gpio_set_level(LED_WEATHER_FETCH, 0);
            }
        } else {
            weather_blink_counter = 0;
        }

        // Delay based on current blink interval
        vTaskDelay(pdMS_TO_TICKS(blink_interval_ms));
        
        // Reset to default interval if not blinking
        if (current_system_status == LED_SYSTEM_OFF || 
            current_system_status == LED_SYSTEM_CONNECTED) {
            blink_interval_ms = 500;
        }
    }
}

/**
 * Start the LED blink task
 */
void led_start_blink_task(void) {
    if (blink_task_handle == NULL) {
        xTaskCreate(led_blink_task, "led_blink_task", 2048, NULL, 5, &blink_task_handle);
        ESP_LOGI(TAG, "LED blink task started");
    }
}