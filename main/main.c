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

static void handle_sensor_query(const SensorQuery* query) {
    switch (query->sensor_type) {
        case SensorType_MIST_SENSOR:
            ESP_LOGI(TAG, "Received MIST_SENSOR query, timestamp: %lld", 
                     query->body.mist_sensor.timestamp);
            break;
            
        case SensorType_AIR_SENSOR:
            ESP_LOGI(TAG, "Received AIR_SENSOR query, timestamp: %lld, humidity: %f, temperature: %f", 
                     query->body.air_sensor.timestamp, query->body.air_sensor.humidity, query->body.air_sensor.temperature);
            break;
            
        case SensorType_LIGHT_SENSOR:
            ESP_LOGI(TAG, "Received LIGHT_SENSOR query, timestamp: %lld", 
                     query->body.light_sensor.timestamp);
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown sensor type: %d", query->sensor_type);
            break;
    }
}

static void handle_command(const Command* cmd) {
    ESP_LOGI(TAG, "Received command with ID: %lld", cmd->id);
    
    switch (cmd->which_body) {
        case Command_time_sync_tag:
            if (cmd->body.time_sync.has_timestamp) {
                ESP_LOGI(TAG, "Time sync command with timestamp: %lld", 
                         cmd->body.time_sync.timestamp);
            }
            break;
            
        case Command_sleep_cycle_tag:
            ESP_LOGI(TAG, "Sleep cycle command with duration: %lld", 
                     cmd->body.sleep_cycle.sleep_time);
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown command body type");
            break;
    }
}

bool task_handler(const task_t* task) {
    pb_istream_t stream = pb_istream_from_buffer(task->buffer, task->buffer_size);
    
    MessageType message_type;
    if (!pb_decode_varint(&stream, (uint64_t*)&message_type)) {
        ESP_LOGE(TAG, "Failed to decode message type");
        return true;
    }

    switch (message_type) {
        case MessageType_SENSOR_QUERY: {
            SensorQuery query = SensorQuery_init_zero;
            if (pb_decode(&stream, &SensorQuery_msg, &query)) {
                handle_sensor_query(&query);
            } else {
                ESP_LOGE(TAG, "Failed to decode sensor query");
            }
            break;
        }
        
        case MessageType_COMMAND: {
            Command cmd = Command_init_zero;
            if (pb_decode(&stream, &Command_msg, &cmd)) {
                handle_command(&cmd);
            } else {
                ESP_LOGE(TAG, "Failed to decode command");
            }
            break;
        }
        
        default:
            ESP_LOGE(TAG, "Unknown message type: %d", message_type);
            break;
    }

    return true;
}

bool test(const uint8_t* buffer, const uint64_t buffer_size) {
    pb_istream_t stream = pb_istream_from_buffer(buffer, buffer_size);
    uint64_t tag;
    MessageType message_type;
    
    if (!pb_decode_varint(&stream, (uint64_t*)&tag)) {
        return _MessageType_MIN; // or handle error
    }

    // Read message type value
    if (!pb_decode_varint(&stream, (uint64_t*)&message_type)) {
        return _MessageType_MIN; // or handle error
    }

    switch (message_type) {
        case MessageType_SENSOR_QUERY: {
            pb_istream_t stream = pb_istream_from_buffer(buffer, buffer_size);
            SensorQuery query = SensorQuery_init_zero;
            if (!pb_decode(&stream, SensorQuery_fields, &query)) {
                ESP_LOGE(TAG, "Failed to decode SensorQuery: %s", PB_GET_ERROR(&stream));
                return false;
            }

            // Log common fields
            ESP_LOGI(TAG, "Decoded SensorQuery:");
            ESP_LOGI(TAG, "  Message type: %d", query.message_type);
            ESP_LOGI(TAG, "  Sensor type: %d", query.sensor_type);

            // Handle oneof body field
            switch (query.which_body) {
                case SensorQuery_mist_sensor_tag:
                    ESP_LOGI(TAG, "  MistSensor data:");
                    ESP_LOGI(TAG, "    Temperature: %.2f", query.body.mist_sensor.temperature);
                    ESP_LOGI(TAG, "    Humidity: %.2f", query.body.mist_sensor.humidity);
                    ESP_LOGI(TAG, "    Timestamp: %lld", query.body.mist_sensor.timestamp);
                    break;

                case SensorQuery_air_sensor_tag:
                    ESP_LOGI(TAG, "  AirSensor data:");
                    ESP_LOGI(TAG, "    Temperature: %.2f", query.body.air_sensor.temperature);
                    ESP_LOGI(TAG, "    Humidity: %.2f", query.body.air_sensor.humidity);
                    ESP_LOGI(TAG, "    Pressure: %.2f", query.body.air_sensor.pressure);
                    ESP_LOGI(TAG, "    Timestamp: %lld", query.body.air_sensor.timestamp);
                    break;

                case SensorQuery_light_sensor_tag:
                    ESP_LOGI(TAG, "  LightSensor data:");
                    ESP_LOGI(TAG, "    Intensity: %llu", query.body.light_sensor.intensity);
                    ESP_LOGI(TAG, "    Timestamp: %lld", query.body.light_sensor.timestamp);
                    break;

                default:
                    ESP_LOGE(TAG, "Unknown sensor type in oneof");
                    return false;
            }

            break;
        }
        
        case MessageType_COMMAND: {
            Command cmd = Command_init_zero;
            if (pb_decode(&stream, &Command_msg, &cmd)) {
                handle_command(&cmd);
            } else {
                ESP_LOGE(TAG, "Failed to decode command");
            }
            break;
        }
        
        default:
            ESP_LOGE(TAG, "Unknown message type: %d", message_type);
            break;
    }

    return true;
}

void app_main(void)
{
    // Initialize NVS for wifi station mode
    nvs_init();
    // Init WiFi to station mode in order to sync time
    wifi_sta_init(WIFI_SSID, WIFI_PASS);
    // Sync time
    // time_sync();
    // Deinit WiFi
    wifi_shutdown();
    // Swith WiFi to ESPNOW mode
    wifi_espnow_init();

    // Initialize ESPNOW
    comm_init();
    register_message_handler(task_handler);

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

    // Create a sensor query message
    SensorQuery query = SensorQuery_init_zero;
    query.message_type = MessageType_SENSOR_QUERY;
    query.sensor_type = SensorType_AIR_SENSOR;
    // If you need to set the body (optional)
    query.which_body = SensorQuery_air_sensor_tag;
    query.body.air_sensor.sensor_type = SensorType_AIR_SENSOR;
    query.body.air_sensor.timestamp = time(NULL);
    query.body.air_sensor.humidity = 0.5;
    query.body.air_sensor.temperature = 20;
    query.body.air_sensor.pressure = 1013.25;
    memcpy(query.body.air_sensor.mac_addr, broadcast_mac, ESP_NOW_ETH_ALEN);

    // Log the sensor query message
    ESP_LOGI(TAG, "Sensor query message created:");
    ESP_LOGI(TAG, "  Message type: %d", query.message_type);
    ESP_LOGI(TAG, "  Sensor type:  %d", query.sensor_type);


    // Create output buffer for encoding
    size_t buffer_size = 0;
    if (!pb_get_encoded_size(&buffer_size, SensorQuery_fields, &query)) {
        ESP_LOGE(TAG, "Failed to get encoded size");
        return;
    }
    uint8_t buffer[buffer_size];

    // Create stream for encoding
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);
    // Encode the message
    bool status = pb_encode(&stream, SensorQuery_fields, &query);
    if (!status) {
        ESP_LOGE(TAG, "Encoding failed: %s", PB_GET_ERROR(&stream));
        return;
    }

    // After encoding, add debug logging
    // ESP_LOGI(TAG, "Encoded buffer size: %d", buffer_size);
    // for(size_t i = 0; i < buffer_size; i++) {
    //     ESP_LOGI(TAG, "buffer[%d] = 0x%02x", i, buffer[i]);
    // }

    test(buffer, buffer_size);
    // Broadcast the encoded message
    // esp_err_t result = broadcast(buffer, buffer_size);
    // if (result != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to broadcast message");
    //     return;
    // }
}