/*
  LED + botão + MQTT + lógica de 1 segundo
  - esp32/button: recebe led_on / led_off
  - esp32/led: único que acende/apaga o LED
*/

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

#define BROKER_URI "mqtt://192.168.2.164:1883"

static bool led_on = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static bool button_pending_on = false;
static int64_t button_timestamp = 0;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT CONNECTED");
            esp_mqtt_client_subscribe(event->client, "esp32/led", 0);
            esp_mqtt_client_subscribe(event->client, "esp32/button", 0);
            break;

        case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT DATA");
            if (strncmp(event->topic, "esp32/led", event->topic_len) == 0) {
                if (strncmp(event->data, "on", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 1);
                    led_on = true;
                } else if (strncmp(event->data, "off", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 0);
                    led_on = false;
                }
            }

            if (strncmp(event->topic, "esp32/button", event->topic_len) == 0) {

                if (strncmp(event->data, "led_on", event->data_len) == 0) {
                    button_pending_on = true;
                    button_timestamp = esp_timer_get_time();
                }

                else if (strncmp(event->data, "led_off", event->data_len) == 0) {
                    button_pending_on = false;

                    esp_mqtt_client_publish(mqtt_client,
                        "esp32/led", "off", 0, 1, 0);
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
    bool pressed = false;

    while (1) {
        int level = gpio_get_level(BUTTON_GPIO);

        if (level == 0 && !pressed) {
            pressed = true;

            esp_mqtt_client_publish(mqtt_client,
                "esp32/button", "led_on", 0, 1, 0);
        }

        if (level == 1 && pressed) {
            pressed = false;

            esp_mqtt_client_publish(mqtt_client,
                "esp32/button", "led_off", 0, 1, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void button_interpreter_task(void *arg) {
    while (1) {
        if (button_pending_on) {
            int64_t dt = esp_timer_get_time() - button_timestamp;

            if (dt >= 0) {
                esp_mqtt_client_publish(mqtt_client,
                    "esp32/led", "on", 0, 1, 0);

                button_pending_on = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
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
    xTaskCreate(button_interpreter_task, "button_interpreter_task",
                4096, NULL, 5, NULL);
}
