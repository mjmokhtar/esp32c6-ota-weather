#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <stdbool.h>
#include <time.h>

// Weather data structure
typedef struct {
    float temperature;      // Temperature in Celsius
    int humidity;          // Relative humidity in %
    time_t last_update;    // Timestamp of last update
    bool is_valid;         // Data validity flag
} weather_data_t;

// Configuration
#define WEATHER_FETCH_INTERVAL_MS   (3600000)  // 1 hour in milliseconds
#define WEATHER_RETRY_INTERVAL_MS   (60000)    // 1 minute retry on failure

// Jakarta coordinates
#define WEATHER_LATITUDE    "-6.1818"
#define WEATHER_LONGITUDE   "106.8223"

// API URL
#define WEATHER_API_URL     "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LATITUDE \
                            "&longitude=" WEATHER_LONGITUDE \
                            "&current=temperature_2m,relative_humidity_2m&forecast_days=1"

/**
 * Initialize weather client
 */
void weather_client_init(void);

/**
 * Start weather fetch task
 */
void weather_client_start(void);

/**
 * Stop weather fetch task
 */
void weather_client_stop(void);

/**
 * Get latest weather data
 * @param data Pointer to weather_data_t structure to fill
 * @return true if data is valid, false otherwise
 */
bool weather_client_get_data(weather_data_t *data);

/**
 * Force immediate weather fetch
 */
void weather_client_fetch_now(void);

/**
 * Check if weather client is running
 */
bool weather_client_is_running(void);

#endif // WEATHER_CLIENT_H