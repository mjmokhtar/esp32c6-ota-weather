#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

// Global state
static wifi_state_t current_state = WIFI_STATE_IDLE;
static wifi_credentials_t stored_credentials = {0};
static int retry_count = 0;

// Callbacks
static wifi_connected_cb_t connected_callback = NULL;
static wifi_disconnected_cb_t disconnected_callback = NULL;

// Event group
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/**
 * WiFi event handler
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED:
                {
                    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                    ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                             MAC2STR(event->mac), event->aid);
                }
                break;

            case WIFI_EVENT_AP_STADISCONNECTED:
                {
                    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                    ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                             MAC2STR(event->mac), event->aid);
                }
                break;

            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started, connecting...");
                esp_wifi_connect();
                current_state = WIFI_STATE_STA_CONNECTING;
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected");
                current_state = WIFI_STATE_STA_DISCONNECTED;
                
                if (retry_count < WIFI_STA_MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    retry_count++;
                    ESP_LOGI(TAG, "Retry connecting to WiFi (%d/%d)", retry_count, WIFI_STA_MAXIMUM_RETRY);
                } else {
                    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                    current_state = WIFI_STATE_STA_FAILED;
                    ESP_LOGE(TAG, "Failed to connect to WiFi after %d retries", WIFI_STA_MAXIMUM_RETRY);
                    
                    if (disconnected_callback) {
                        disconnected_callback();
                    }
                }
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        current_state = WIFI_STATE_STA_CONNECTED;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (connected_callback) {
            connected_callback();
        }
    }
}

/**
 * Initialize WiFi manager
 */
esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create event group
    wifi_event_group = xEventGroupCreate();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    ESP_LOGI(TAG, "WiFi Manager initialized");
    return ESP_OK;
}

/**
 * Start WiFi in AP mode (Access Point)
 */
esp_err_t wifi_manager_start_ap(void)
{
    ESP_LOGI(TAG, "Starting WiFi in AP mode");
    
    // Create default AP netif
    esp_netif_t *netif_ap = esp_netif_create_default_wifi_ap();
    
    // Configure AP IP
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    
    esp_netif_dhcps_stop(netif_ap);
    esp_netif_set_ip_info(netif_ap, &ip_info);
    esp_netif_dhcps_start(netif_ap);
    
    // WiFi AP configuration
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    current_state = WIFI_STATE_AP_STARTED;
    
    ESP_LOGI(TAG, "WiFi AP started - SSID: %s, Password: %s, IP: %s", 
             WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_IP);
    
    return ESP_OK;
}

/**
 * Start WiFi in STA mode (Station)
 */
esp_err_t wifi_manager_start_sta(void)
{
    if (!wifi_manager_has_credentials()) {
        ESP_LOGE(TAG, "No WiFi credentials found");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting WiFi in STA mode");
    ESP_LOGI(TAG, "Connecting to SSID: %s", stored_credentials.ssid);
    
    // Create default STA netif
    esp_netif_create_default_wifi_sta();
    
    // WiFi STA configuration
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, stored_credentials.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, stored_credentials.password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", stored_credentials.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", stored_credentials.ssid);
        return ESP_FAIL;
    }
    
    return ESP_FAIL;
}

/**
 * Save WiFi credentials to NVS
 */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // Save SSID
    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    // Save password
    err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, password ? password : "");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    // Commit
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    
    // Update stored credentials
    strncpy(stored_credentials.ssid, ssid, sizeof(stored_credentials.ssid) - 1);
    strncpy(stored_credentials.password, password ? password : "", sizeof(stored_credentials.password) - 1);
    
    ESP_LOGI(TAG, "WiFi credentials saved - SSID: %s", ssid);
    return ESP_OK;
}

/**
 * Load WiFi credentials from NVS
 */
esp_err_t wifi_manager_load_credentials(wifi_credentials_t *creds)
{
    if (!creds) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No WiFi credentials found in NVS");
        return err;
    }
    
    size_t ssid_len = sizeof(creds->ssid);
    size_t password_len = sizeof(creds->password);
    
    // Load SSID
    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, creds->ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error loading SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    // Load password
    err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, creds->password, &password_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error loading password: %s", esp_err_to_name(err));
        creds->password[0] = '\0'; // Empty password
    }
    
    nvs_close(nvs_handle);
    
    // Update stored credentials
    memcpy(&stored_credentials, creds, sizeof(wifi_credentials_t));
    
    ESP_LOGI(TAG, "WiFi credentials loaded - SSID: %s", creds->ssid);
    return ESP_OK;
}

/**
 * Check if WiFi credentials exist
 */
bool wifi_manager_has_credentials(void)
{
    wifi_credentials_t creds = {0};
    esp_err_t err = wifi_manager_load_credentials(&creds);
    return (err == ESP_OK && strlen(creds.ssid) > 0);
}

/**
 * Get current WiFi state
 */
wifi_state_t wifi_manager_get_state(void)
{
    return current_state;
}

/**
 * Set connected callback
 */
void wifi_manager_set_connected_callback(wifi_connected_cb_t callback)
{
    connected_callback = callback;
}

/**
 * Set disconnected callback
 */
void wifi_manager_set_disconnected_callback(wifi_disconnected_cb_t callback)
{
    disconnected_callback = callback;
}