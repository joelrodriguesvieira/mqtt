#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "driver/gpio.h"
#include "mqtt_client.h"
#include "esp_timer.h"

#include "esp_mac.h"

#define TAG "MQTT_LED"

#define LED_GPIO     23
#define BUTTON_GPIO  22

#define BROKER_URI "mqtt://192.168.2.215:1883"

static bool led_on = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static bool button_pending_on = false;
static int64_t button_timestamp = 0;



static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT CONNECTED");
            esp_mqtt_client_subscribe(event->client, "esp32/tp1", 0);

            break;

        case MQTT_EVENT_DATA:

        ESP_LOGI(TAG, "MQTT DATA");

            if (strncmp(event->topic, "esp32/tp1", event->topic_len) == 0) {

                if (strncmp(event->data, "on", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 0);
                }

                else if (strncmp(event->data, "off", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 1);
                }
            }
            break;

        default:
            ESP_LOGD(TAG, "MQTT event id: %d", event->event_id);

        break;
    }

    return ESP_OK;
}

static void mqtt_event_handler(void *args, esp_event_base_t base,
    int32_t id, void *data) {

    mqtt_event_handler_cb(data);
}

static void button_task(void *arg) {
    int level_before = -1;

    while (1) {
        int level_now = gpio_get_level(BUTTON_GPIO);

        if(level_now != level_before) {
            level_before = level_now;
            esp_mqtt_client_publish(mqtt_client, "esp32/tp2", level_now ? "on" : "off", 0,1,0);

        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);



    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = BROKER_URI
    };

    mqtt_client = esp_mqtt_client_init(&cfg);

    esp_mqtt_client_register_event(mqtt_client,
        ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_mqtt_client_start(mqtt_client);
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}