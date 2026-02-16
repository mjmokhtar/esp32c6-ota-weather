#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include <stdbool.h>
#include <time.h>

// Task configuration
#define SNTP_SYNC_TASK_STACK_SIZE   4096
#define SNTP_SYNC_TASK_PRIORITY     5
#define SNTP_SYNC_TASK_CORE_ID      0

/**
 * Initialize and start SNTP sync task
 */
void sntp_sync_init(void);

/**
 * Get current time as string (format: DD.MM.YYYY HH:MM:SS)
 * @return pointer to static time buffer
 */
char* sntp_sync_get_time_str(void);

/**
 * Get current time as struct tm
 * @param timeinfo pointer to tm structure to fill
 * @return true if time is valid (synced), false otherwise
 */
bool sntp_sync_get_time(struct tm *timeinfo);

/**
 * Check if time is synchronized
 * @return true if synced, false otherwise
 */
bool sntp_sync_is_synced(void);

/**
 * Get epoch timestamp
 * @return current epoch time
 */
time_t sntp_sync_get_epoch(void);

#endif // SNTP_SYNC_H