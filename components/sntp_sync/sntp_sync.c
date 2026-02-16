#include "sntp_sync.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "SNTP_SYNC";

// SNTP operating mode set status
static bool sntp_initialized = false;
static bool time_synced = false;

/**
 * SNTP sync notification callback
 */
static void sntp_sync_time_cb(struct timeval *tv)
{
    time_synced = true;
    ESP_LOGI(TAG, "Time synchronized with NTP server");
}

/**
 * Initialize SNTP service
 */
static void sntp_sync_init_sntp(void)
{
    if (sntp_initialized) {
        ESP_LOGW(TAG, "SNTP already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing SNTP service");
    
    // Set timezone to WIB (GMT+7)
    setenv("TZ", "WIB-7", 1);
    tzset();
    
    // Set SNTP operating mode
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // Set NTP server
    esp_sntp_setservername(0, "pool.ntp.org");
    
    // Set sync mode to update time immediately when received
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    
    // Set notification callback
    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    
    // Initialize SNTP
    esp_sntp_init();
    
    sntp_initialized = true;
    
    ESP_LOGI(TAG, "SNTP service initialized - waiting for time sync...");
}

/**
 * Check and obtain time
 */
static void sntp_sync_obtain_time(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Check if we need to initialize SNTP
    if (!sntp_initialized || timeinfo.tm_year < (2016 - 1900)) {
        sntp_sync_init_sntp();
    }
    
    // Update sync status
    if (timeinfo.tm_year >= (2016 - 1900)) {
        time_synced = true;
    }
}

/**
 * SNTP sync task
 */
static void sntp_sync_task(void *pvParam)
{
    ESP_LOGI(TAG, "SNTP sync task started");
    
    while (1) {
        sntp_sync_obtain_time();
        
        if (time_synced) {
            // Log time every 1 hour after synced
            vTaskDelay(pdMS_TO_TICKS(3600000));
        } else {
            // Check every 10 seconds if not synced
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    
    vTaskDelete(NULL);
}

/**
 * Get current time as string
 */
char* sntp_sync_get_time_str(void)
{
    static char time_buffer[100] = {0};
    time_t now = 0;
    struct tm timeinfo = {0};
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year < (2016 - 1900)) {
        snprintf(time_buffer, sizeof(time_buffer), "Time not synchronized");
        ESP_LOGW(TAG, "Time is not set yet");
    } else {
        strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
        ESP_LOGD(TAG, "Current time: %s", time_buffer);
    }
    
    return time_buffer;
}

/**
 * Get current time as struct tm
 */
bool sntp_sync_get_time(struct tm *timeinfo)
{
    if (!timeinfo) {
        return false;
    }
    
    time_t now = 0;
    time(&now);
    localtime_r(&now, timeinfo);
    
    // Check if time is valid
    if (timeinfo->tm_year < (2016 - 1900)) {
        return false;
    }
    
    return true;
}

/**
 * Check if time is synchronized
 */
bool sntp_sync_is_synced(void)
{
    return time_synced;
}

/**
 * Get epoch timestamp
 */
time_t sntp_sync_get_epoch(void)
{
    return time(NULL);
}

/**
 * Initialize and start SNTP sync
 */
void sntp_sync_init(void)
{
    ESP_LOGI(TAG, "Starting SNTP time synchronization");
    
    // Create SNTP sync task
    xTaskCreatePinnedToCore(
        &sntp_sync_task,
        "sntp_sync_task",
        SNTP_SYNC_TASK_STACK_SIZE,
        NULL,
        SNTP_SYNC_TASK_PRIORITY,
        NULL,
        SNTP_SYNC_TASK_CORE_ID
    );
}