#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "sht31.h"

static const char *TAG = "main";

#define PUBLISH_INTERVAL_MS 10000

/* Defined as a macro so the same literal can initialize both the standalone
   `dev` and each sensor's `.dev` field — in C a static `const` variable is not a
   constant expression, so `.dev = dev` would not compile. */
#define DEVICE_DEF {                  \
    .device_id = "esp32_02",          \
    .name = "ESP32_02",               \
    .model = "ESP32-WROOM",           \
    .manufacturer = "Espressif",      \
}

static const mqtt_device_t dev = DEVICE_DEF;

static const mqtt_sensor_t temp_sensor = {
    .name = "Temperatura",
    .unique_id = "esp32_02_temp",
    .state_topic = "home/esp32_02/state",
    .unit_of_measurement = "°C",
    .device_class = "temperature",
    .state_class = "measurement",
    .dev = DEVICE_DEF,
};

static const mqtt_sensor_t hum_sensor = {
    .name = "Wilgotność",
    .unique_id = "esp32_02_hum",
    .state_topic = "home/esp32_02/state",
    .unit_of_measurement = "%",
    .device_class = "humidity",
    .state_class = "measurement",
    .dev = DEVICE_DEF,
};

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

    if (wifi_manager_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi up and running");

    if (mqtt_manager_init(&dev) != ESP_OK) {
        ESP_LOGE(TAG, "MQTT connection failed");
        return;
    }

    sht31_init();

    /* Publish Home Assistant discovery config once at startup. */
    mqtt_manager_publish_config(&temp_sensor);
    mqtt_manager_publish_config(&hum_sensor);

    float temp;
    float hum;
    while (true) {
        /* TODO: odczyt z DHT11 — na razie hardkodowane wartości. */

        sht31_measure(&temp, &hum);
        mqtt_measurement_t measurements[] = {
            { &temp_sensor, temp },
            { &hum_sensor, hum },
        };
        mqtt_manager_publish_data(measurements, 2);

        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
    }
}
