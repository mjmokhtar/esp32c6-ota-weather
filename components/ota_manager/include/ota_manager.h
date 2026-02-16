#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include "esp_partition.h"
#include <stddef.h>

// Firmware version
#define FIRMWARE_VERSION "1.0.0"

/**
 * OTA status callback
 */
typedef void (*ota_progress_cb_t)(size_t current, size_t total);

/**
 * Initialize OTA manager
 */
esp_err_t ota_manager_init(void);

/**
 * Start OTA update process
 * @param file_size Total file size
 * @return ESP_OK on success
 */
esp_err_t ota_manager_begin(size_t file_size);

/**
 * Write chunk of data to OTA partition
 * @param data Data buffer
 * @param size Data size
 * @return ESP_OK on success
 */
esp_err_t ota_manager_write(const void *data, size_t size);

/**
 * Finalize OTA update and set boot partition
 * @return ESP_OK on success
 */
esp_err_t ota_manager_end(void);

/**
 * Abort OTA update
 */
void ota_manager_abort(void);

/**
 * Get current firmware version
 */
const char* ota_manager_get_version(void);

/**
 * Get current running partition
 */
const char* ota_manager_get_partition(void);

/**
 * Get next update partition info
 */
const esp_partition_t* ota_manager_get_update_partition(void);

/**
 * Set progress callback
 */
void ota_manager_set_progress_callback(ota_progress_cb_t callback);

#endif // OTA_MANAGER_H