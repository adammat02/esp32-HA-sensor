#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi in station mode and connect to the configured AP.
 *
 * SSID and password are taken from Kconfig (CONFIG_WIFI_SSID /
 * CONFIG_WIFI_PASSWORD). Blocks until the connection succeeds or all
 * CONFIG_WIFI_MAXIMUM_RETRY attempts have failed.
 *
 * @return ESP_OK on successful connection, ESP_FAIL otherwise.
 */
esp_err_t wifi_manager_connect(void);

/**
 * @brief Query whether the station is currently connected with an IP address.
 *
 * @return true if connected, false otherwise.
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
