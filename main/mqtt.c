#include <esp_err.h>
#include <stddef.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <mqtt_client.h>

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;

static nvs_handle_t s_nvs_handle;

esp_err_t read_nvs_value(const char *key, char *value, size_t *length) {
    esp_err_t err;

    // Read value from NVS
    ESP_LOGI(TAG, "Reading %s...", key);
    err = nvs_get_str(s_nvs_handle, key, value, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read %s: %s", key, esp_err_to_name(err));
        nvs_close(s_nvs_handle);
        return err;
    }

    return ESP_OK;
}

esp_err_t read_credentials(char *broker_uri, size_t broker_uri_len, char *username, size_t username_len, char *password, size_t password_len, char *ca_cert, size_t ca_cert_len) {
    esp_err_t err;

    // Open NVS handle
    err = nvs_open_from_partition("nvs", "storage", NVS_READONLY, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Read MQTT broker URI
    err = read_nvs_value("MQTT_BROKER_URI", broker_uri, &broker_uri_len);
    if (err != ESP_OK) return err;

    // Read MQTT username
    err = read_nvs_value("MQTT_USERNAME", username, &username_len);
    if (err != ESP_OK) return err;

    // Read MQTT password
    err = read_nvs_value("MQTT_PASSWORD", password, &password_len);
    if (err != ESP_OK) return err;

    // Read MQTT CA certificate
    err = read_nvs_value("MQTT_CA_CERT", ca_cert, &ca_cert_len);
    if (err != ESP_OK) return err;

    // Close NVS handle
    nvs_close(s_nvs_handle);

    return ESP_OK;
}

static void event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGW(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGW(TAG, "Unknown event id:%d", event->event_id);
        break;
    }
}

esp_err_t init_mqtt() {
    ESP_LOGI(TAG, "Initializing MQTT s_client");

    size_t broker_uri_len = 128;
    size_t username_len = 64;
    size_t password_len = 128;
    size_t ca_cert_len = 2048;

    // Buffers to hold the credentials
    char *broker_uri = malloc(broker_uri_len);
    char *username = malloc(username_len);
    char *password = malloc(password_len);
    char *ca_cert = malloc(ca_cert_len); 

    // Read credentials from NVS
    esp_err_t err = read_credentials(broker_uri, broker_uri_len, username, username_len, password, password_len, ca_cert, ca_cert_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT credentials");
        return err;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .broker.verification.certificate = ca_cert,
        .network.timeout_ms = 10000,
        .network.reconnect_timeout_ms = 5000,
        .session.keepalive = 60,
        // Add TLS/SSL configuration
        .broker.verification.skip_cert_common_name_check = false,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_mqtt_client_start(s_client);
    
    ESP_LOGI(TAG, "MQTT client initialized");

    return ESP_OK;
}

esp_err_t mqtt_publish(const char *topic, const char *data) {
    int msg_id = esp_mqtt_client_publish(s_client, topic, data, 0, 0, 0);
    ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
    return ESP_OK;
}