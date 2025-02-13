#pragma once

esp_err_t init_mqtt();

esp_err_t mqtt_publish(const char *topic, const char *data);

typedef esp_err_t (*mqtt_recv_msg_handler_t)(const char *topic, const uint8_t *data, int data_len);

void mqtt_register_recv_msg_handler(mqtt_recv_msg_handler_t handler);

void mqtt_deregister_recv_msg_handler(void);