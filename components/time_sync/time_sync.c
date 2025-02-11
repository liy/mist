#include "esp_sntp.h"
#include "esp_log.h"
#include "time_sync.h"

static const char *TAG = "time_sync";

/* Initialize SNTP client */
void time_sync(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time synchronization
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    // Print time information
    // Get current time
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Current time info:");
    ESP_LOGI(TAG, "Unix timestamp: %lld", (long long)now);
    ESP_LOGI(TAG, "UTC time:       %s", asctime(&timeinfo));
    ESP_LOGI(TAG, "Local time:     %s", ctime(&now));
}