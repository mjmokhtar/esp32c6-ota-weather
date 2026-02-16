#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <string.h>
#include "esp_http_server.h"
#include "sntp_sync.h"
#include "cJSON.h"

static const char *TAG = "WIFI_MANAGER";

// Global state
static wifi_state_t current_state = WIFI_STATE_IDLE;
static wifi_credentials_t stored_credentials = {0};
static int retry_count = 0;

// Callbacks
static wifi_connected_cb_t connected_callback = NULL;
static wifi_disconnected_cb_t disconnected_callback = NULL;

// HTTP server handle
static httpd_handle_t server = NULL;

// Event group
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// HTML page for WiFi provisioning
// HTML page for WiFi provisioning with connection status
static const char *html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>ESP32-C6 WiFi Setup</title>"
"<style>"
"body{font-family:Arial,sans-serif;background:#f0f0f0;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}"
".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:400px;width:100%}"
"h1{color:#333;text-align:center;margin-bottom:10px}"
".subtitle{color:#666;text-align:center;margin-bottom:20px;font-size:14px}"
".info-card{background:#e8f5e9;border-left:4px solid #4CAF50;padding:15px;margin-bottom:20px;border-radius:5px}"
".info-card.disconnected{background:#fff3cd;border-left-color:#ff9800}"
".info-title{font-weight:bold;color:#2e7d32;margin-bottom:8px;font-size:14px}"
".info-card.disconnected .info-title{color:#e65100}"
".info-row{display:flex;justify-content:space-between;margin:5px 0;font-size:13px}"
".info-label{color:#555;font-weight:500}"
".info-value{color:#333;font-family:monospace;font-weight:bold}"
".form-group{margin-bottom:20px}"
"label{display:block;margin-bottom:5px;color:#555;font-weight:bold}"
"input{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:14px}"
"input:focus{outline:none;border-color:#4CAF50}"
"button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;font-weight:bold}"
"button:hover{background:#45a049}"
"button:active{background:#3d8b40}"
".status{margin-top:20px;padding:10px;border-radius:5px;text-align:center;display:none}"
".status.success{background:#d4edda;color:#155724;display:block}"
".status.error{background:#f8d7da;color:#721c24;display:block}"
".refresh-btn{background:#2196F3;margin-top:10px;padding:8px;font-size:14px}"
".refresh-btn:hover{background:#1976D2}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>ESP32-C6 Setup</h1>"
"<p class='subtitle'>WiFi Configuration & Status</p>"
"<div id='connectionStatus' class='info-card'>"
"<div class='info-title'>Loading status...</div>"
"</div>"
"<form id='wifiForm'>"
"<div class='form-group'>"
"<label for='ssid'>WiFi SSID</label>"
"<input type='text' id='ssid' name='ssid' placeholder='Enter WiFi Name' required>"
"</div>"
"<div class='form-group'>"
"<label for='password'>WiFi Password</label>"
"<input type='password' id='password' name='password' placeholder='Enter WiFi Password'>"
"</div>"
"<button type='submit'>Connect</button>"
"</form>"
"<button class='refresh-btn' onclick='loadStatus()'>Refresh Status</button>"
"<div id='status' class='status'></div>"
"</div>"
"<script>"
"function loadStatus(){"
"fetch('/status')"
".then(response=>response.json())"
".then(data=>{"
"var statusDiv=document.getElementById('connectionStatus');"
"if(data.connected){"
"statusDiv.className='info-card';"
"statusDiv.innerHTML="
"'<div class=\"info-title\">✓ Connected to WiFi</div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">SSID:</span><span class=\"info-value\">'+data.ssid+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">IP Address:</span><span class=\"info-value\">'+data.ip+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">Subnet:</span><span class=\"info-value\">'+data.subnet+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">Gateway:</span><span class=\"info-value\">'+data.gateway+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">AP IP:</span><span class=\"info-value\">'+data.ap_ip+'</span></div>';"
"}else{"
"statusDiv.className='info-card disconnected';"
"statusDiv.innerHTML="
"'<div class=\"info-title\">⚠ Not Connected</div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">Mode:</span><span class=\"info-value\">AP Only</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">AP IP:</span><span class=\"info-value\">'+data.ap_ip+'</span></div>'"
"+'<div style=\"margin-top:10px;color:#666;font-size:12px\">Configure WiFi below to connect</div>';"
"}"
"})"
".catch(error=>{"
"console.error('Error loading status:',error);"
"});"
"}"
"loadStatus();"
"document.getElementById('wifiForm').addEventListener('submit',function(e){"
"e.preventDefault();"
"var ssid=document.getElementById('ssid').value;"
"var password=document.getElementById('password').value;"
"var data={ssid:ssid,password:password};"
"var statusDiv=document.getElementById('status');"
"statusDiv.className='status';"
"statusDiv.textContent='Connecting...';"
"statusDiv.style.display='block';"
"fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(response=>response.json())"
".then(data=>{"
"if(data.success){"
"statusDiv.className='status success';"
"statusDiv.textContent='WiFi configured! Device will restart in 3 seconds...';"
"setTimeout(function(){window.location.reload();},3000);"
"}else{"
"statusDiv.className='status error';"
"statusDiv.textContent='Error: '+data.message;"
"}"
"})"
".catch(error=>{"
"statusDiv.className='status error';"
"statusDiv.textContent='Connection failed: '+error;"
"});"
"});"
"</script>"
"</body>"
"</html>";

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
 * Start WiFi in APSTA mode (AP + STA simultaneously)
 */
esp_err_t wifi_manager_start_apsta(void)
{
    if (!wifi_manager_has_credentials()) {
        ESP_LOGE(TAG, "No WiFi credentials found");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting WiFi in APSTA mode (AP + STA)");
    
    // Stop WiFi first if already running
    esp_wifi_stop();
    
    // Get or create netif for AP
    esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif_ap == NULL) {
        ESP_LOGI(TAG, "Creating AP netif");
        netif_ap = esp_netif_create_default_wifi_ap();
    } else {
        ESP_LOGI(TAG, "Reusing existing AP netif");
    }
    
    // Get or create netif for STA
    esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif_sta == NULL) {
        ESP_LOGI(TAG, "Creating STA netif");
        netif_sta = esp_netif_create_default_wifi_sta();
    } else {
        ESP_LOGI(TAG, "Reusing existing STA netif");
    }
    
    // Configure AP IP
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    
    esp_netif_dhcps_stop(netif_ap);
    esp_netif_set_ip_info(netif_ap, &ip_info);
    esp_netif_dhcps_start(netif_ap);
    
    // WiFi configuration for APSTA mode
    wifi_config_t wifi_config_ap = {
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
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    wifi_config_t wifi_config_sta = {0};
    strncpy((char *)wifi_config_sta.sta.ssid, stored_credentials.ssid, sizeof(wifi_config_sta.sta.ssid) - 1);
    strncpy((char *)wifi_config_sta.sta.password, stored_credentials.password, sizeof(wifi_config_sta.sta.password) - 1);
    wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config_sta.sta.pmf_cfg.capable = true;
    wifi_config_sta.sta.pmf_cfg.required = false;
    
    // Set mode to APSTA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    current_state = WIFI_STATE_AP_STARTED; // Will change to CONNECTED when STA connects
    
    ESP_LOGI(TAG, "WiFi APSTA started");
    ESP_LOGI(TAG, "  AP - SSID: %s, IP: %s", WIFI_AP_SSID, WIFI_AP_IP);
    ESP_LOGI(TAG, "  STA - Connecting to: %s", stored_credentials.ssid);
    
    // Wait for STA connection
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "STA connected to SSID: %s (AP still running)", stored_credentials.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "STA failed to connect to SSID: %s (AP still running)", stored_credentials.ssid);
        return ESP_FAIL;
    }
    
    return ESP_FAIL;
}

/**
 * HTTP GET handler - serve HTML page
 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

/**
 * HTTP POST handler - save WiFi credentials
 */
static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[200];
    int ret, remaining = req->content_len;
    
    if (remaining > sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }
    
    // Read POST data
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Received WiFi config: %s", buf);
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(root, "password");
    
    if (!cJSON_IsString(ssid_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
        return ESP_FAIL;
    }
    
    const char *ssid = ssid_json->valuestring;
    const char *password = cJSON_IsString(password_json) ? password_json->valuestring : "";
    
    ESP_LOGI(TAG, "Saving WiFi - SSID: %s", ssid);
    
    // Save credentials
    esp_err_t err = wifi_manager_save_credentials(ssid, password);
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    
    if (err == ESP_OK) {
        const char *resp = "{\"success\":true,\"message\":\"WiFi credentials saved\"}";
        httpd_resp_send(req, resp, strlen(resp));
        
        // Schedule restart after 3 seconds
        ESP_LOGI(TAG, "WiFi credentials saved, restarting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        const char *resp = "{\"success\":false,\"message\":\"Failed to save credentials\"}";
        httpd_resp_send(req, resp, strlen(resp));
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * HTTP GET handler - return WiFi connection status as JSON
 */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Check if STA is connected
    wifi_state_t state = wifi_manager_get_state();
    bool is_connected = (state == WIFI_STATE_STA_CONNECTED);
    
    cJSON_AddBoolToObject(root, "connected", is_connected);
    cJSON_AddStringToObject(root, "ap_ip", WIFI_AP_IP);
    
    if (is_connected) {
        // Get STA network info
        esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif_sta) {
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(netif_sta, &ip_info);
            
            char ip_str[16], subnet_str[16], gw_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            snprintf(subnet_str, sizeof(subnet_str), IPSTR, IP2STR(&ip_info.netmask));
            snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&ip_info.gw));
            
            cJSON_AddStringToObject(root, "ip", ip_str);
            cJSON_AddStringToObject(root, "subnet", subnet_str);
            cJSON_AddStringToObject(root, "gateway", gw_str);
            cJSON_AddStringToObject(root, "ssid", stored_credentials.ssid);
        }
    }
    
    // Convert to JSON string
    char *json_str = cJSON_Print(root);
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    // Cleanup
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/**
 * HTTP GET handler - return current time as JSON
 */
static esp_err_t time_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Get time info
    struct tm timeinfo;
    bool time_valid = sntp_sync_get_time(&timeinfo);
    
    cJSON_AddBoolToObject(root, "synced", sntp_sync_is_synced());
    
    if (time_valid) {
        // Format time string
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%d.%m.%Y %H:%M:%S", &timeinfo);
        cJSON_AddStringToObject(root, "time", time_str);
        
        // Add individual components
        cJSON_AddNumberToObject(root, "year", timeinfo.tm_year + 1900);
        cJSON_AddNumberToObject(root, "month", timeinfo.tm_mon + 1);
        cJSON_AddNumberToObject(root, "day", timeinfo.tm_mday);
        cJSON_AddNumberToObject(root, "hour", timeinfo.tm_hour);
        cJSON_AddNumberToObject(root, "minute", timeinfo.tm_min);
        cJSON_AddNumberToObject(root, "second", timeinfo.tm_sec);
        
        // Add epoch
        cJSON_AddNumberToObject(root, "epoch", (double)sntp_sync_get_epoch());
    } else {
        cJSON_AddStringToObject(root, "time", "Not synchronized");
        cJSON_AddStringToObject(root, "message", "Waiting for NTP sync...");
    }
    
    // Convert to JSON string
    char *json_str = cJSON_Print(root);
    
    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    // Cleanup
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/**
 * Start WiFi in APSTA mode automatically using saved credentials
 */
esp_err_t wifi_manager_start_apsta_auto(void)
{
    if (!wifi_manager_has_credentials()) {
        ESP_LOGE(TAG, "No WiFi credentials found");
        return ESP_FAIL;
    }
    
    wifi_credentials_t creds;
    wifi_manager_load_credentials(&creds);
    
    ESP_LOGI(TAG, "Starting WiFi APSTA (AP + STA)");
    
    // Stop any existing WiFi
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create both netif (check if exists first)
    esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!netif_ap) {
        netif_ap = esp_netif_create_default_wifi_ap();
    }
    
    esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif_sta) {
        netif_sta = esp_netif_create_default_wifi_sta();
    }
    
    // Configure AP IP
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(netif_ap);
    esp_netif_set_ip_info(netif_ap, &ip_info);
    esp_netif_dhcps_start(netif_ap);
    
    // WiFi config
    wifi_config_t wifi_config_ap = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.required = false},
        },
    };
    
    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    wifi_config_t wifi_config_sta = {0};
    strncpy((char *)wifi_config_sta.sta.ssid, creds.ssid, sizeof(wifi_config_sta.sta.ssid) - 1);
    strncpy((char *)wifi_config_sta.sta.password, creds.password, sizeof(wifi_config_sta.sta.password) - 1);
    wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config_sta.sta.pmf_cfg.capable = true;
    wifi_config_sta.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "APSTA mode started - AP: %s, STA connecting to: %s", WIFI_AP_SSID, creds.ssid);
    
    return ESP_OK;
}

/**
 * Start HTTP web server for provisioning
 */
esp_err_t wifi_manager_start_webserver(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t save_uri = {
            .uri       = "/save",
            .method    = HTTP_POST,
            .handler   = save_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &save_uri);

        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t time_uri = {
            .uri       = "/time",
            .method    = HTTP_GET,
            .handler   = time_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &time_uri);
        
        ESP_LOGI(TAG, "Web server started - Access at http://%s", WIFI_AP_IP);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

/**
 * Stop HTTP web server
 */
esp_err_t wifi_manager_stop_webserver(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
    return ESP_OK;
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

