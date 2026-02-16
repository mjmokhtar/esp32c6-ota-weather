#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * Initialize and start web server
 * Handles all HTTP requests for:
 * - WiFi provisioning
 * - OTA updates
 * - Status APIs
 * - Time sync info
 */
esp_err_t web_server_start(void);

/**
 * Stop web server
 */
esp_err_t web_server_stop(void);

/**
 * Check if web server is running
 */
bool web_server_is_running(void);

#endif // WEB_SERVER_H