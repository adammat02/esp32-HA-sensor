#include "mqtt_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_log.h"

#define MQTT_CONNECTED_BIT BIT0

static const char *TAG = "mqtt_manager";

static esp_mqtt_client_handle_t s_client;
static mqtt_device_t s_device;
static EventGroupHandle_t s_mqtt_event_group;
static bool s_connected = false;

/* Log and return on failure instead of aborting, mirroring wifi_manager. */
#define MQTT_RETURN_ON_ERR(expr, msg)                                     \
    do {                                                                  \
        esp_err_t err_ = (expr);                                          \
        if (err_ != ESP_OK) {                                             \
            ESP_LOGE(TAG, msg ": %s", esp_err_to_name(err_));             \
            return err_;                                                  \
        }                                                                 \
    } while (0)

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker");
        s_connected = true;
        xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        s_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error event");
        break;
    default:
        break;
    }
}

/* Build the Home Assistant discovery config JSON for a sensor. Caller owns the
   returned cJSON object and must cJSON_Delete() it. */
static cJSON *struct_to_json(const mqtt_sensor_t *sensor)
{
    cJSON *doc = cJSON_CreateObject();
    if (doc == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(doc, "name", sensor->name);
    cJSON_AddStringToObject(doc, "unique_id", sensor->unique_id);
    cJSON_AddStringToObject(doc, "state_topic", sensor->state_topic);

    char value_template[64];
    snprintf(value_template, sizeof(value_template),
             "{{ value_json.%s }}", sensor->device_class);
    cJSON_AddStringToObject(doc, "value_template", value_template);

    cJSON_AddStringToObject(doc, "unit_of_measurement", sensor->unit_of_measurement);
    cJSON_AddStringToObject(doc, "device_class", sensor->device_class);
    cJSON_AddStringToObject(doc, "state_class", sensor->state_class);

    cJSON *device = cJSON_AddObjectToObject(doc, "device");
    cJSON_AddStringToObject(device, "name", sensor->dev.name);
    cJSON_AddStringToObject(device, "model", sensor->dev.model);
    cJSON_AddStringToObject(device, "manufacturer", sensor->dev.manufacturer);

    cJSON *identifiers = cJSON_AddArrayToObject(device, "identifiers");
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(sensor->dev.device_id));

    return doc;
}

esp_err_t mqtt_manager_init(const mqtt_device_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_device = *dev;

    s_mqtt_event_group = xEventGroupCreate();
    if (s_mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = CONFIG_MQTT_HOST,
        .broker.address.port = CONFIG_MQTT_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .credentials.client_id = s_device.device_id,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    MQTT_RETURN_ON_ERR(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                                      mqtt_event_handler, NULL),
                       "Failed to register MQTT event handler");

    ESP_LOGI(TAG, "Connecting to MQTT broker %s:%d", CONFIG_MQTT_HOST, CONFIG_MQTT_PORT);
    MQTT_RETURN_ON_ERR(esp_mqtt_client_start(s_client), "esp_mqtt_client_start failed");

    /* Block until the client reports a successful connection. */
    xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t mqtt_manager_publish_config(const mqtt_sensor_t *sensor)
{
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *doc = struct_to_json(sensor);
    if (doc == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char *payload = cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/config", sensor->unique_id);

    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, true);
    ESP_LOGI(TAG, "Published config to %s: %s", topic, payload);
    cJSON_free(payload);

    return msg_id < 0 ? ESP_FAIL : ESP_OK;
}

esp_err_t mqtt_manager_publish_data(const mqtt_measurement_t *measurements, size_t count)
{
    if (measurements == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *doc = cJSON_CreateObject();
    if (doc == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < count; i++) {
        double rounded = round(measurements[i].value * 100.0) / 100.0;
        cJSON_AddNumberToObject(doc, measurements[i].sensor->device_class,
                                rounded);
    }

    char *payload = cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const char *topic = measurements[0].sensor->state_topic;
    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 0, false);
    ESP_LOGI(TAG, "Published data to %s: %s", topic, payload);
    cJSON_free(payload);

    return msg_id < 0 ? ESP_FAIL : ESP_OK;
}

bool mqtt_manager_is_connected(void)
{
    return s_connected;
}
