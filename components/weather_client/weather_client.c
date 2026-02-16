#include "weather_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_indicator.h"
#include <string.h>
#include <time.h>

static const char *TAG = "WEATHER_CLIENT";

// Task handle
static TaskHandle_t weather_task_handle = NULL;
static bool is_running = false;

// Current weather data
static weather_data_t current_weather = {
    .temperature = 0.0,
    .humidity = 0,
    .last_update = 0,
    .is_valid = false
};

// HTTP response buffer
#define HTTP_BUFFER_SIZE 2048
static char http_buffer[HTTP_BUFFER_SIZE];
static int http_buffer_index = 0;

/**
 * HTTP event handler
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Append data to buffer
            if (http_buffer_index + evt->data_len < HTTP_BUFFER_SIZE - 1) {
                memcpy(http_buffer + http_buffer_index, evt->data, evt->data_len);
                http_buffer_index += evt->data_len;
                http_buffer[http_buffer_index] = '\0';
            } else {
                ESP_LOGW(TAG, "HTTP buffer overflow");
            }
            break;
            
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP error");
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

/**
 * Parse JSON response from API
 */
static bool parse_weather_json(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }
    
    // Get "current" object
    cJSON *current = cJSON_GetObjectItem(root, "current");
    if (!cJSON_IsObject(current)) {
        ESP_LOGE(TAG, "No 'current' object in JSON");
        cJSON_Delete(root);
        return false;
    }
    
    // Get temperature
    cJSON *temp = cJSON_GetObjectItem(current, "temperature_2m");
    if (!cJSON_IsNumber(temp)) {
        ESP_LOGE(TAG, "No temperature data");
        cJSON_Delete(root);
        return false;
    }
    
    // Get humidity
    cJSON *humidity = cJSON_GetObjectItem(current, "relative_humidity_2m");
    if (!cJSON_IsNumber(humidity)) {
        ESP_LOGE(TAG, "No humidity data");
        cJSON_Delete(root);
        return false;
    }
    
    // Update current weather data
    current_weather.temperature = (float)temp->valuedouble;
    current_weather.humidity = humidity->valueint;
    current_weather.last_update = time(NULL);
    current_weather.is_valid = true;
    
    ESP_LOGI(TAG, "Weather updated: %.1f°C, %d%% humidity", 
             current_weather.temperature, current_weather.humidity);
    
    cJSON_Delete(root);
    return true;
}

/**
 * Fetch weather data from API
 */
static bool fetch_weather_data(void)
{
    ESP_LOGI(TAG, "Fetching weather data from API...");
    
    // Turn on weather fetch LED
    led_set_weather_fetch(true);
    
    // Reset buffer
    http_buffer_index = 0;
    memset(http_buffer, 0, HTTP_BUFFER_SIZE);
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = WEATHER_API_URL,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = 512,
        .crt_bundle_attach = esp_crt_bundle_attach,  // <-- WAJIB untuk HTTPS
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        led_set_weather_fetch(false);
        return false;
    }
    
    // Perform HTTP GET request
    esp_err_t err = esp_http_client_perform(client);
    
    bool success = false;
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        
        if (status_code == 200) {
            ESP_LOGI(TAG, "HTTP GET successful, parsing data...");
            success = parse_weather_json(http_buffer);
        } else {
            ESP_LOGE(TAG, "HTTP GET failed, status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET error: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    
    // Turn off weather fetch LED
    led_set_weather_fetch(false);
    
    return success;
}

/**
 * Weather fetch task
 */
static void weather_fetch_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Weather fetch task started");
    ESP_LOGI(TAG, "Location: Jakarta (Lat: %s, Lon: %s)", WEATHER_LATITUDE, WEATHER_LONGITUDE);
    ESP_LOGI(TAG, "Fetch interval: %d seconds", WEATHER_FETCH_INTERVAL_MS / 1000);
    
    // Initial delay to let WiFi stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Fetch weather immediately on start
    fetch_weather_data();
    
    while (is_running) {
        // Wait for next fetch interval
        vTaskDelay(pdMS_TO_TICKS(WEATHER_FETCH_INTERVAL_MS));
        
        // Fetch weather data
        if (!fetch_weather_data()) {
            ESP_LOGW(TAG, "Weather fetch failed, will retry in %d seconds", 
                     WEATHER_RETRY_INTERVAL_MS / 1000);
            
            // Retry after shorter interval
            vTaskDelay(pdMS_TO_TICKS(WEATHER_RETRY_INTERVAL_MS));
        }
    }
    
    ESP_LOGI(TAG, "Weather fetch task stopped");
    weather_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * Initialize weather client
 */
void weather_client_init(void)
{
    ESP_LOGI(TAG, "Weather client initialized");
}

/**
 * Start weather fetch task
 */
void weather_client_start(void)
{
    if (is_running) {
        ESP_LOGW(TAG, "Weather client already running");
        return;
    }
    
    is_running = true;
    
    xTaskCreate(
        weather_fetch_task,
        "weather_fetch",
        4096,
        NULL,
        5,
        &weather_task_handle
    );
    
    ESP_LOGI(TAG, "Weather client started");
}

/**
 * Stop weather fetch task
 */
void weather_client_stop(void)
{
    if (!is_running) {
        return;
    }
    
    is_running = false;
    
    if (weather_task_handle) {
        vTaskDelete(weather_task_handle);
        weather_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Weather client stopped");
}

/**
 * Get latest weather data
 */
bool weather_client_get_data(weather_data_t *data)
{
    if (!data) {
        return false;
    }
    
    *data = current_weather;
    return current_weather.is_valid;
}

/**
 * Force immediate fetch
 */
void weather_client_fetch_now(void)
{
    if (is_running) {
        // Trigger immediate fetch by resuming task
        if (weather_task_handle) {
            vTaskResume(weather_task_handle);
        }
    }
}

/**
 * Check if running
 */
bool weather_client_is_running(void)
{
    return is_running;
}