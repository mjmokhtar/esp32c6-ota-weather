#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "led_indicator.h"
#include <string.h>

static const char *TAG = "OTA_MANAGER";

// OTA handle and state
static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = NULL;
static size_t total_written = 0;
static size_t total_size = 0;
static bool ota_in_progress = false;

// Progress callback
static ota_progress_cb_t progress_callback = NULL;

/**
 * Initialize OTA manager
 */
esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "OTA Manager initialized");
    ESP_LOGI(TAG, "Firmware Version: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Running Partition: %s", ota_manager_get_partition());
    return ESP_OK;
}

/**
 * Begin OTA update
 */
esp_err_t ota_manager_begin(size_t file_size)
{
    if (ota_in_progress) {
        ESP_LOGE(TAG, "OTA already in progress");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting OTA update, size: %zu bytes", file_size);
    
    // Get next update partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition '%s' at offset 0x%lx", 
             update_partition->label, update_partition->address);
    
    // Begin OTA
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        return err;
    }
    
    total_written = 0;
    total_size = file_size;
    ota_in_progress = true;
    
    // Set LED to OTA mode
    led_set_system_status(LED_SYSTEM_OTA_UPDATING);
    
    ESP_LOGI(TAG, "OTA begin successful");
    return ESP_OK;
}

/**
 * Write data chunk
 */
esp_err_t ota_manager_write(const void *data, size_t size)
{
    if (!ota_in_progress) {
        ESP_LOGE(TAG, "OTA not started");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_ota_write(ota_handle, data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
        return err;
    }
    
    total_written += size;
    
    // Call progress callback
    if (progress_callback) {
        progress_callback(total_written, total_size);
    }
    
    // Log progress every 10%
    if (total_size > 0) {
        int progress = (total_written * 100) / total_size;
        static int last_progress = 0;
        if (progress >= last_progress + 10) {
            ESP_LOGI(TAG, "OTA progress: %d%%", progress);
            last_progress = progress;
        }
    }
    
    return ESP_OK;
}

/**
 * End OTA update
 */
esp_err_t ota_manager_end(void)
{
    if (!ota_in_progress) {
        ESP_LOGE(TAG, "OTA not in progress");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Finalizing OTA update, total written: %zu bytes", total_written);
    
    // End OTA
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
        led_set_system_status(LED_SYSTEM_RECOVERY);
        ota_in_progress = false;
        return err;
    }
    
    // Set boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(err));
        led_set_system_status(LED_SYSTEM_RECOVERY);
        ota_in_progress = false;
        return err;
    }
    
    ota_in_progress = false;
    
    ESP_LOGI(TAG, "OTA update successful!");
    ESP_LOGI(TAG, "New partition: %s", update_partition->label);
    
    return ESP_OK;
}

/**
 * Abort OTA
 */
void ota_manager_abort(void)
{
    if (ota_in_progress) {
        ESP_LOGW(TAG, "Aborting OTA update");
        esp_ota_abort(ota_handle);
        ota_in_progress = false;
        led_set_system_status(LED_SYSTEM_RECOVERY);
    }
}

/**
 * Get firmware version
 */
const char* ota_manager_get_version(void)
{
    return FIRMWARE_VERSION;
}

/**
 * Get current partition
 */
const char* ota_manager_get_partition(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running->label;
}

/**
 * Get update partition
 */
const esp_partition_t* ota_manager_get_update_partition(void)
{
    return esp_ota_get_next_update_partition(NULL);
}

/**
 * Set progress callback
 */
void ota_manager_set_progress_callback(ota_progress_cb_t callback)
{
    progress_callback = callback;
}