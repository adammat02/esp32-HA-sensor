#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "wifi_manager.h"

static const char *TAG = "main";

void app_main(void)
{
    /* Wi-Fi stores calibration data in NVS, so it must be initialized first. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* System-wide networking bootstrap: the TCP/IP stack and the shared default
       event loop. These are not Wi-Fi specific, so they live here rather than in
       the wifi_manager component. */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (wifi_manager_connect() == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi up and running");
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed");
    }
}
