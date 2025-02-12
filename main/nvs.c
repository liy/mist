#include <esp_err.h>
#include <stddef.h>
#include <nvs_flash.h>
#include <esp_log.h>

static const char *TAG = "NVS";

esp_err_t read_mqtt_credentials(char *username, size_t username_len, char *password, size_t password_len, char *ca_cert, size_t ca_cert_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    // err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    err = nvs_open_from_partition("nvs", "storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Read MQTT username
    ESP_LOGI(TAG, "Reading MQTT username...");
    err = nvs_get_str(nvs_handle, "MQTT_USERNAME", username, &username_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT username: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Read MQTT password
    ESP_LOGI(TAG, "Reading MQTT password...");
    err = nvs_get_str(nvs_handle, "MQTT_PASSWORD", password, &password_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Read MQTT CA certificate
    ESP_LOGI(TAG, "Reading MQTT CA certificate...");
    err = nvs_get_str(nvs_handle, "MQTT_CA_CERT", ca_cert, &ca_cert_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT CA certificate: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t write_mqtt_credentials(const char *username, const char *password, const char *ca_cert) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Write MQTT username
    ESP_LOGI(TAG, "Writing MQTT username...");
    err = nvs_set_str(nvs_handle, "MQTT_USERNAME", username);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MQTT username: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Write MQTT password
    ESP_LOGI(TAG, "Writing MQTT password...");
    err = nvs_set_str(nvs_handle, "MQTT_PASSWORD", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MQTT password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Write MQTT CA certificate
    ESP_LOGI(TAG, "Writing MQTT CA certificate...");
    err = nvs_set_str(nvs_handle, "MQTT_CA_CERT", ca_cert);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MQTT CA certificate: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Commit written value
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return ESP_OK;
}