#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Home Assistant device the sensors are attached to. */
typedef struct {
    const char *device_id;
    const char *name;
    const char *model;
    const char *manufacturer;
} mqtt_device_t;

/** Single Home Assistant sensor (MQTT discovery descriptor). */
typedef struct {
    const char *name;
    const char *unique_id;
    const char *state_topic;
    const char *unit_of_measurement;
    const char *device_class;
    const char *state_class;
    mqtt_device_t dev;
} mqtt_sensor_t;

/** One measurement: a sensor and its current value. */
typedef struct {
    const mqtt_sensor_t *sensor;
    float value;
} mqtt_measurement_t;

/**
 * @brief Create the MQTT client and connect to the configured broker.
 *
 * Broker host/port and credentials are taken from Kconfig (CONFIG_MQTT_HOST,
 * CONFIG_MQTT_PORT, CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD). The client id
 * is taken from @p dev->device_id. Blocks until the connection is established.
 *
 * @param dev Device the published sensors belong to (must stay valid; only the
 *            pointers it holds are copied).
 * @return ESP_OK once connected, an error code otherwise.
 */
esp_err_t mqtt_manager_init(const mqtt_device_t *dev);

/**
 * @brief Publish a retained Home Assistant discovery config for a sensor.
 *
 * Topic: homeassistant/sensor/<unique_id>/config
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t mqtt_manager_publish_config(const mqtt_sensor_t *sensor);

/**
 * @brief Publish sensor values as JSON to the first measurement's state topic.
 *
 * Payload is a JSON object keyed by each sensor's device_class, e.g.
 * {"temperature":23.5,"humidity":50}.
 *
 * @param measurements Array of measurements (must not be empty).
 * @param count        Number of measurements.
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t mqtt_manager_publish_data(const mqtt_measurement_t *measurements, size_t count);

/**
 * @brief Query whether the client is currently connected to the broker.
 *
 * @return true if connected, false otherwise.
 */
bool mqtt_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
