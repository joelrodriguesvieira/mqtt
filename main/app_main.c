/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "driver/gpio.h" 
#include "mqtt_client.h"

#define TAG "MQTT_LED"

#define BROKER_URI "mqtt://10.220.0.65:1883"
 
#define LED_GPIO     23
#define BUTTON_GPIO  22

#define BUTTON_POLL_MS     20
#define BUTTON_DEBOUNCE_MS 200

static bool led_on = false;

static esp_mqtt_client_handle_t mqtt_client = NULL;

static bool topic_equals(const char *topic, int topic_len, const char *expected) {
    size_t expected_len = strlen(expected);
    return (topic_len == expected_len && memcmp(topic, expected, expected_len) == 0);
}

static bool data_equals(const char *data, int data_len, const char *expected) {
    size_t expected_len = strlen(expected);
    return (data_len == expected_len && memcmp(data, expected, expected_len) == 0);
}

static void mqtt_event_handler(void *handler_args,
    esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT CONNECTED");

            esp_mqtt_client_subscribe(client, "esp32/led", 0);
            ESP_LOGI(TAG, "Subscribed to esp32/led");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT EVENT DATA");

            printf("TOPIC: %.*s\n", event->topic_len, event->topic);
            printf("DATA : %.*s\n", event->data_len, event->data);

            if (topic_equals(event->topic, event->topic_len, "esp32/led")) {

                if (data_equals(event->data, event->data_len, "on")) {
                    gpio_set_level(LED_GPIO, 1);
                    led_on = true;
                    ESP_LOGI(TAG, "LED ON (MQTT)");
                }

                else if (data_equals(event->data, event->data_len, "off")) {
                    gpio_set_level(LED_GPIO, 0);
                    led_on = false;
                    ESP_LOGI(TAG, "LED OFF (MQTT)");
                }
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT DISCONNECTED");
            break;

        default:
            break;
    }
}

static void button_task(void *pv) {
    int last_level = gpio_get_level(BUTTON_GPIO);

    for (;;) {
        int level = gpio_get_level(BUTTON_GPIO);

        if (last_level == 1 && level == 0) {

            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));

            if (gpio_get_level(BUTTON_GPIO) == 0) {
                if (led_on) {
                    led_on = false;
                    gpio_set_level(LED_GPIO, 0);
                    ESP_LOGI(TAG, "Botão -> LED OFF");

                    if (mqtt_client) {
                        esp_mqtt_client_publish(mqtt_client,
                        "esp32/button",
                        "led_off",
                        0, 1, 0);
                    }
                }
                else {
                    led_on = true;
                    gpio_set_level(LED_GPIO, 1);
                    ESP_LOGI(TAG, "Botão -> LED ON");

                    if (mqtt_client) {
                        esp_mqtt_client_publish(mqtt_client,
                        "esp32/button",
                        "led_on",
                        0, 1, 0);
                    }
                }
            }
        }

        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}


static void mqtt_app_start(void) {
    esp_mqtt_client_config_t client_config = {
        .broker.address.uri = BROKER_URI,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,
    };

    mqtt_client = esp_mqtt_client_init(&client_config);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_mqtt_client_start(mqtt_client);
}


void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    gpio_config_t io_conf_led = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };

    gpio_config(&io_conf_led);
    gpio_set_level(LED_GPIO, 0);
    led_on = false;

    gpio_config_t io_conf_btn = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    gpio_config(&io_conf_btn);

    mqtt_app_start();

    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}