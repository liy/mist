esp_err_t read_mqtt_credentials(char *username, size_t username_len, char *password, size_t password_len, char *ca_cert, size_t ca_cert_len);

esp_err_t write_mqtt_credentials(const char *username, const char *password, const char *ca_cert);
