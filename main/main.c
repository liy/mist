#include "espnow.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "messages.pb.h"
#include <time.h>
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"

static const char *TAG = "Mist";

// Configure your WiFi credentials here
#define WIFI_SSID      "lijilin_2.4G"
#define WIFI_PASS      "lijilinlijilin"
#define MAX_RETRIES    5

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static int retry_count = 0;

/* Event handler for WiFi events */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                             int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < MAX_RETRIES) {
            esp_wifi_connect();
            retry_count++;
            ESP_LOGI(TAG, "Retrying WiFi connection (%d/%d)", retry_count, MAX_RETRIES);
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi");
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Initialize WiFi station */
static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished");
}

/* Initialize SNTP client */
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    // Wait for time synchronization
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // Initialize ESPNOW
    // espnow_init();


    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Connect to WiFi
    wifi_init_sta();

    // Initialize SNTP
    initialize_sntp();

    
    // Get current time
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Print time information
    ESP_LOGI(TAG, "Current time info:");
    ESP_LOGI(TAG, "  Unix timestamp: %lld", (long long)now);
    ESP_LOGI(TAG, "  UTC time:       %s", asctime(&timeinfo));
    ESP_LOGI(TAG, "  Local time:     %s", ctime(&now));

    // Create and initialize a MistSensor message
    MistSensor mist_sensor = MistSensor_init_default;
    mist_sensor.timestamp = now;
    mist_sensor.has_temperature = true;
    mist_sensor.temperature = 25.5;
    mist_sensor.has_humidity = true;
    mist_sensor.humidity = 60.0;

    // Encode the MistSensor message
    uint8_t buffer[128];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&ostream, MistSensor_fields, &mist_sensor)) {
        const char *error = PB_GET_ERROR(&ostream);
        printf("pb_encode error: %s\n", error);
        return;
    }
    size_t total_bytes_encoded = ostream.bytes_written;
    printf("Encoded size: %zu\n", total_bytes_encoded);

    // Decode the MistSensor message
    MistSensor decoded_mist_sensor = MistSensor_init_default;
    pb_istream_t istream = pb_istream_from_buffer(buffer, total_bytes_encoded);
    if (!pb_decode(&istream, MistSensor_fields, &decoded_mist_sensor)) {
        const char *error = PB_GET_ERROR(&istream);
        printf("pb_decode error: %s\n", error);
        return;
    }

    // Print the decoded MistSensor message
    printf("Decoded MistSensor message:\n");
    printf("Timestamp: %lld\n", decoded_mist_sensor.timestamp);
    if (decoded_mist_sensor.has_temperature) {
        printf("Temperature: %.2f\n", decoded_mist_sensor.temperature);
    }
    if (decoded_mist_sensor.has_humidity) {
        printf("Humidity: %.2f\n", decoded_mist_sensor.humidity);
    }
}