#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

// WiFi Configuration
#define WIFI_AP_SSID            "ESP32-C6-Setup"
#define WIFI_AP_PASSWORD        "12345678"
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP              "192.168.4.1"

#define WIFI_STA_MAXIMUM_RETRY  5

// NVS Keys for WiFi credentials
#define NVS_NAMESPACE           "wifi_config"
#define NVS_KEY_SSID            "ssid"
#define NVS_KEY_PASSWORD        "password"

// WiFi Manager States
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_AP_STARTED,
    WIFI_STATE_STA_CONNECTING,
    WIFI_STATE_STA_CONNECTED,
    WIFI_STATE_STA_DISCONNECTED,
    WIFI_STATE_STA_FAILED
} wifi_state_t;

// WiFi credentials structure
typedef struct {
    char ssid[32];
    char password[64];
} wifi_credentials_t;

// Callback function types
typedef void (*wifi_connected_cb_t)(void);
typedef void (*wifi_disconnected_cb_t)(void);

// Function Prototypes
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_ap(void);
esp_err_t wifi_manager_start_apsta_auto(void);
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);
esp_err_t wifi_manager_load_credentials(wifi_credentials_t *creds);
bool wifi_manager_has_credentials(void);
wifi_state_t wifi_manager_get_state(void);
void wifi_manager_set_connected_callback(wifi_connected_cb_t callback);
void wifi_manager_set_disconnected_callback(wifi_disconnected_cb_t callback);

// Get network info
esp_netif_t* wifi_manager_get_sta_netif(void);
esp_netif_t* wifi_manager_get_ap_netif(void);

#endif // WIFI_MANAGER_H