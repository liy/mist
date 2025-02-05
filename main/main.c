#include <time.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "pb_encode.h"
#include "pb_decode.h"
#include "messages.pb.h"
#include "wireless.h"
#include "time_sync.h"

#include <esp_now.h>
#include "comm.h"

#define WIFI_SSID      "lijilin_2.4G"
#define WIFI_PASS      "lijilinlijilin"

static const char *TAG = "Mist";

void nvs_init() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    // Initialize NVS for wifi station mode
    nvs_init();
    // Init WiFi to station mode in order to sync time
    wifi_sta_init(WIFI_SSID, WIFI_PASS);
    // Sync time
    time_sync();
    // Deinit WiFi
    wifi_shutdown();
    // Swith WiFi to ESPNOW mode
    wifi_espnow_init();

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
    mist_sensor.temperature = 25.5;
    mist_sensor.humidity = 60.0;
    uint8_t mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    memcpy(mist_sensor.mac_addr, mac, sizeof(mac));

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


    // Initialize ESPNOW
    comm_init();
}