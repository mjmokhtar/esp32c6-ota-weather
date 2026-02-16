#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// GPIO Pin Definitions
#define LED_SYSTEM_STATUS    GPIO_NUM_4   // System/OTA/WiFi/Recovery
#define LED_WEATHER_FETCH    GPIO_NUM_5   // Weather API activity
#define LED_AP_MODE          GPIO_NUM_6   // AP mode indicator

// System Status LED States
typedef enum {
    LED_SYSTEM_OFF = 0,           // WiFi disconnected
    LED_SYSTEM_CONNECTED,         // WiFi connected (solid ON)
    LED_SYSTEM_OTA_UPDATING,      // OTA in progress (blink 200ms)
    LED_SYSTEM_RECOVERY           // Recovery mode (blink 1000ms)
} led_system_status_t;

// Function Prototypes
void led_init(void);
void led_set_system_status(led_system_status_t status);
void led_set_weather_fetch(bool active);
void led_set_ap_mode(bool active);
void led_start_blink_task(void);

#endif // LED_INDICATOR_H