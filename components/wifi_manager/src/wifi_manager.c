#include "wifi_manager.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi_manager";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool s_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry_num < CONFIG_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying to connect to the AP (%d/%d)",
                     s_retry_num, CONFIG_WIFI_MAXIMUM_RETRY);
        } else {
            ESP_LOGE(TAG, "Connection limit exceeded");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Log and return on failure instead of aborting, so a missing prerequisite
   (esp_netif_init / esp_event_loop_create_default, expected to be done by the
   application) surfaces as an error rather than a panic. */
#define WIFI_RETURN_ON_ERR(expr, msg)                                     \
    do {                                                                  \
        esp_err_t err_ = (expr);                                          \
        if (err_ != ESP_OK) {                                             \
            ESP_LOGE(TAG, msg ": %s", esp_err_to_name(err_));             \
            return err_;                                                  \
        }                                                                 \
    } while (0)

esp_err_t wifi_manager_connect(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    /* Requires esp_netif_init() to have run first; returns NULL otherwise. */
    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi STA netif "
                      "(did the app call esp_netif_init() first?)");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    WIFI_RETURN_ON_ERR(esp_wifi_init(&cfg), "esp_wifi_init failed");

    /* Requires the default event loop; returns ESP_ERR_INVALID_STATE if the app
       did not call esp_event_loop_create_default() first. */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    WIFI_RETURN_ON_ERR(esp_event_handler_instance_register(WIFI_EVENT,
                                                           ESP_EVENT_ANY_ID,
                                                           &wifi_event_handler,
                                                           NULL,
                                                           &instance_any_id),
                       "Failed to register WIFI_EVENT handler "
                       "(did the app call esp_event_loop_create_default() first?)");
    WIFI_RETURN_ON_ERR(esp_event_handler_instance_register(IP_EVENT,
                                                           IP_EVENT_STA_GOT_IP,
                                                           &wifi_event_handler,
                                                           NULL,
                                                           &instance_got_ip),
                       "Failed to register IP_EVENT handler");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };

    WIFI_RETURN_ON_ERR(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode failed");
    WIFI_RETURN_ON_ERR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), "esp_wifi_set_config failed");
    WIFI_RETURN_ON_ERR(esp_wifi_start(), "esp_wifi_start failed");

    ESP_LOGI(TAG, "Wi-Fi connecting: %s", CONFIG_WIFI_SSID);

    /* Block until either the connection succeeds or all retries fail. */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID: %s", CONFIG_WIFI_SSID);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to AP SSID: %s", CONFIG_WIFI_SSID);
    return ESP_FAIL;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}
