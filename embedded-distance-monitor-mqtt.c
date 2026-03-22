#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "mqtt_client.h"

#define TRIG_PIN GPIO_NUM_4 
#define ECHO_PIN GPIO_NUM_5 
#define SOUND_SPEED_CM_US 0.0343f
#define MAX_DISTANCE_CM   400
#define QUEUE_SIZE        10

static const char *TAG = "SENSOR_DATA";
static QueueHandle_t distanta_queue;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectat la Brokerul MQTT!");
            mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Deconectat de la Broker!");
            mqtt_connected = false;
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_event_handler_cb(event_data);
}

static void ultrasonic_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << ECHO_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_set_level(TRIG_PIN, 0);
}

static float ultrasonic_measure(void) {
    gpio_set_level(TRIG_PIN, 0);
    ets_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    ets_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    int64_t start = esp_timer_get_time();
    while(gpio_get_level(ECHO_PIN) == 0) {
        if((esp_timer_get_time() - start) > 100000) return -1.0f; 
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1) {
        if((esp_timer_get_time() - echo_start) > 100000) return -1.0f;
    }
    int64_t echo_end = esp_timer_get_time();

    float duration_us = (float)(echo_end - echo_start);
    float distance_cm = (duration_us * SOUND_SPEED_CM_US) / 2.0f;

    return (distance_cm > MAX_DISTANCE_CM) ? -1.0f : distance_cm;
}

void sensor_task(void *pvParameters) {
    float current_dist;
    while(1) {
        current_dist = ultrasonic_measure();
        if (current_dist > 0) {
            ESP_LOGI(TAG, "Distanta: %.2f cm", current_dist);
            xQueueSend(distanta_queue, &current_dist, 0);
        } else {
            ESP_LOGW(TAG, "Eroare citire senzor!");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void mqtt_publisher_task(void *pvParameters) {
    float dist_from_queue;
    char msg[16];
    while(1) {
        if (xQueueReceive(distanta_queue, &dist_from_queue, portMAX_DELAY)) {
            if (mqtt_connected && mqtt_client != NULL) {
                snprintf(msg, sizeof(msg), "%.2f", dist_from_queue);
                esp_mqtt_client_publish(mqtt_client, "andrei/distanta", msg, 0, 1, 0);
            }
        }
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    distanta_queue = xQueueCreate(QUEUE_SIZE, sizeof(float));

    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://192.168.137.143", 
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);

    ultrasonic_init();

    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, NULL);
    xTaskCreate(mqtt_publisher_task, "mqtt_task", 4096, NULL, 4, NULL);
}