void init_mqtt();

esp_err_t mqtt_publish(const char *topic, const char *data);
