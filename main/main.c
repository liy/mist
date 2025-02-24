#include <time.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <string.h>
#include <esp_mac.h>

#include "pb_encode.h"
#include "pb_decode.h"
#include "messages.pb.h"
#include "wireless.h"
#include "time_sync.h"
#include <esp_now.h>
#include "comm.h"
#include "led.h"
#include "mqtt.h"

#define BROKER_URL "mqtt://192.168.3.105:1883"  // Replace with your broker URL

static const char *TAG = "Mist";

// Task handle to notify when slave has sent over its the address
static TaskHandle_t s_handshake_notify = NULL;

static uint8_t s_mac[6];

void nvs_init() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void handle_sensor_data(const SensorData* sensor_data) {
    esp_err_t err;
    
    switch (sensor_data->sensor_type) {
        case SensorType_AIR_SENSOR:

            ESP_LOGI(TAG, "Received AIR_SENSOR sensor_data, timestamp: %lld, humidity: %f, temperature: %f, voc_index: %d", 
                     sensor_data->body.air_sensor.timestamp, sensor_data->body.air_sensor.humidity, sensor_data->body.air_sensor.temperature, sensor_data->body.air_sensor.voc_index);

            led_action();

            
            // Publish JSON data to MQTT topic
            char* temperature_str = malloc(30 * sizeof(char));
            snprintf(temperature_str, 30, "%.2f", sensor_data->body.air_sensor.temperature);
            err = mqtt_publish("/esp32/air/temperature", temperature_str);
            char* humidity_str = malloc(30 * sizeof(char));
            snprintf(humidity_str, 30, "%.2f", sensor_data->body.air_sensor.humidity);
            err = mqtt_publish("/esp32/air/humidity", humidity_str);
            char* voc_index_str = malloc(30 * sizeof(char));
            snprintf(voc_index_str, 30, "%.2f", sensor_data->body.air_sensor.humidity);
            err = mqtt_publish("/esp32/air/voc_index", voc_index_str);

            if(err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to publish message");
                led_fail();
            }

            free(temperature_str);
            free(humidity_str);
            free(voc_index_str);

            break;
        case SensorType_SOIL_SENSOR:

            ESP_LOGI(TAG, "Received SOIL_SENSOR sensor_data, timestamp: %lld, moisture: %f", 
                     sensor_data->body.soil_sensor.timestamp, sensor_data->body.soil_sensor.moisture);

            led_action();
            
            char* moisture_str = malloc(30 * sizeof(char));
            snprintf(moisture_str, 30, "%.2f", sensor_data->body.soil_sensor.moisture);
            err = mqtt_publish("/esp32/soil/moisture", moisture_str);
            free(moisture_str);

            if(err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to publish message");
                led_fail();
            }

            break;
        case SensorType_MIST_SENSOR:
            ESP_LOGI(TAG, "Received MIST_SENSOR sensor_data, timestamp: %lld, humidity: %f, temperature: %f", 
                     sensor_data->body.mist_sensor.timestamp, sensor_data->body.mist_sensor.humidity, sensor_data->body.mist_sensor.temperature);
            break;
        case SensorType_LIGHT_SENSOR:
            ESP_LOGI(TAG, "Received LIGHT_SENSOR sensor_data, timestamp: %lld, light intensity: %f", 
                     sensor_data->body.light_sensor.timestamp, sensor_data->body.light_sensor.intensity);
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown sensor type: %d", sensor_data->sensor_type);
            break;
    }
}

static void handle_sensor_command(const SensorCommand* cmd) {
    ESP_LOGI(TAG, "Received command with ID: %lld", cmd->id);
    
    switch (cmd->which_body) {
        case SensorCommand_sleep_cycle_tag:
            ESP_LOGI(TAG, "Sleep cycle command with duration: %lld", 
                     cmd->body.sleep_cycle.sleep_time);
            break;
        case SensorCommand_sample_rate_tag:
            ESP_LOGI(TAG, "Sample rate command with rate: %lld", 
                     cmd->body.sample_rate.rate);
            break;
        default:
            ESP_LOGE(TAG, "Unknown command body type");
            break;
    }
}

static esp_err_t recv_msg_cb(const CommTask_t* task) {
    pb_istream_t stream = pb_istream_from_buffer(task->buffer, task->buffer_size);
    
    // In Protocol Buffers, each field is prefixed with a tag that contains two pieces of information:
    //      1. The field number from your .proto file
    //      2. The wire type (encoding type)
    // It is encoded as the first byte, the tag: (field_number << 3) | wire_type
    uint32_t tag;
    if (!pb_decode_varint32(&stream, &tag)) {
        ESP_LOGE(TAG, "Failed to decode message type");
        return ESP_FAIL; 
    }

    // Decode the actual message type
    MessageType message_type;
    if (!pb_decode_varint32(&stream, (uint32_t*)&message_type)) {
        return ESP_FAIL; 
    }

    ESP_LOGI(TAG, "Received message type: %d", message_type);

    // In order to decode the message using the correct message type, we need to reset the stream. 
    // Reset stream to beginning, by setting the bytes_left to the total buffer size 
    stream.bytes_left = task->buffer_size;
    // and setting pointer to the beginning of the buffer
    stream.state = (void*)task->buffer;

    switch (message_type) {
        case MessageType_SENSOR_DATA: {
            SensorData sensor_data = SensorData_init_default;
            if (pb_decode(&stream, SensorData_fields, &sensor_data)) {
                handle_sensor_data(&sensor_data);
            } else {
                ESP_LOGE(TAG, "Failed to decode SensorData: %s", PB_GET_ERROR(&stream));
                return ESP_FAIL;
            }
            break;
        }
        case MessageType_SLAVERY_HANDSHAKE: {
            SlaveryHandshake handshake = SlaveryHandshake_init_default;
            if (pb_decode(&stream, SlaveryHandshake_fields, &handshake)) {                
                ESP_LOGI(TAG, "Received slave MAC address: "MACSTR"", MAC2STR(handshake.slave_mac_addr));
                comm_add_peer(handshake.slave_mac_addr, false);
                
                // Notify that slave has sent the address, and handshake is complete
                xTaskNotifyGive(s_handshake_notify);
            } else {
                ESP_LOGE(TAG, "Failed to decode SlaveryHandshake: %s", PB_GET_ERROR(&stream));
                return ESP_FAIL;
            }
            break;
        }
        case MessageType_SYNC_TIME: {
            SyncTime sync_time = SyncTime_init_default;
            if (pb_decode(&stream, SyncTime_fields, &sync_time)) {
                ESP_LOGI(TAG, "Received SyncTime message from slave, "MACSTR"", MAC2STR(task->mac_addr));

                time_t now;
                time(&now);
                sync_time.has_master_timestamp = true;
                sync_time.master_timestamp = now;

                // Encode the SyncTime message
                size_t buffer_size = 0;
                if (!pb_get_encoded_size(&buffer_size, SyncTime_fields, &sync_time)) {
                    ESP_LOGE(TAG, "Failed to get SyncTime encoded size");
                    return ESP_FAIL;
                }
                uint8_t buffer[buffer_size];
                pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);
                if(!pb_encode(&stream, SyncTime_fields, &sync_time)) {
                    ESP_LOGE(TAG, "Encoding SyncTime failed: %s", PB_GET_ERROR(&stream));
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "Sending SyncTime message, timestamp: %lld", sync_time.master_timestamp);
                // Send back the SyncTime message
                comm_send(buffer, buffer_size, task->mac_addr);

                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "Failed to decode SyncTime: %s", PB_GET_ERROR(&stream));
                return ESP_FAIL;
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "Unknown message type: %d", message_type);
            return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t mqtt_recv_msg_handler(const char *topic, const uint8_t *buffer, int buffer_size) {
    pb_istream_t stream = pb_istream_from_buffer(buffer, buffer_size);
    
    // In Protocol Buffers, each field is prefixed with a tag that contains two pieces of information:
    //      1. The field number from your .proto file
    //      2. The wire type (encoding type)
    // It is encoded as the first byte, the tag: (field_number << 3) | wire_type
    uint32_t tag;
    if (!pb_decode_varint32(&stream, &tag)) {
        ESP_LOGE(TAG, "Failed to decode message type");
        return ESP_FAIL; 
    }

    // Decode the actual message type
    MessageType message_type;
    if (!pb_decode_varint32(&stream, (uint32_t*)&message_type)) {
        return ESP_FAIL; 
    }

    ESP_LOGI(TAG, "Received message type: %d", message_type);

    // In order to decode the message using the correct message type, we need to reset the stream. 
    // Reset stream to beginning, by setting the bytes_left to the total buffer size 
    stream.bytes_left = buffer_size;
    // and setting pointer to the beginning of the buffer
    stream.state = (void*)buffer;

    switch (message_type) {
        case MessageType_SENSOR_COMMAND: {
            SensorCommand cmd = SensorCommand_init_default;
            if (pb_decode(&stream, &SensorCommand_msg, &cmd)) {
                if ((cmd.has_master_mac_addr  && memcmp(cmd.master_mac_addr, s_mac, ESP_NOW_ETH_ALEN) == 0) || COMM_IS_BROADCAST_ADDR(cmd.master_mac_addr)) {
                    switch (cmd.which_body) {
                        case SensorCommand_sample_rate_tag:
                            ESP_LOGI(TAG, "Forward received sample rate command with rate: %lld", cmd.body.sample_rate.rate);  
                            comm_send(buffer, buffer_size, cmd.sensor_mac_addr);
                            break;
                        default:
                            ESP_LOGE(TAG, "Unknown command body type");
                            break;
                    }
                }

            } else {
                ESP_LOGE(TAG, "Failed to decode command: %s", PB_GET_ERROR(&stream));
                return ESP_FAIL;
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "Unknown message type: %d", message_type);
            return ESP_FAIL;
    }

    return ESP_OK;
}

void start_slavery_handshake() {
    // Store the handle of the current handshake task
    s_handshake_notify = xTaskGetCurrentTaskHandle();
    ESP_LOGI(TAG, "Starting Handshake");

    // Get master's mac address
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address");
        return;
    }
    // Broadcast SlaveryHandshake message
    SlaveryHandshake handshake = SlaveryHandshake_init_default;
    handshake.message_type = MessageType_SLAVERY_HANDSHAKE;
    handshake.has_master_mac_addr = true;
    memcpy(handshake.master_mac_addr, mac, ESP_NOW_ETH_ALEN);
    ESP_LOGI(TAG, "Master MAC address: "MACSTR"", MAC2STR(handshake.master_mac_addr));
    size_t buffer_size = 0;
    if (!pb_get_encoded_size(&buffer_size, SlaveryHandshake_fields, &handshake)) {
        ESP_LOGE(TAG, "Failed to get encoded size");
        return;
    }
    uint8_t buffer[buffer_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);
    bool status = pb_encode(&stream, SlaveryHandshake_fields, &handshake);
    if (!status) {
        ESP_LOGE(TAG, "Encoding failed: %s", PB_GET_ERROR(&stream));
        return;
    }

    for(uint i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Broadcasting slavery handshake with master MAC address...");
        comm_broadcast(buffer, buffer_size);

        if(ulTaskNotifyTake(pdTRUE, 1000 / portTICK_PERIOD_MS) == 1) {
            ESP_LOGI(TAG, "Main: Exiting loop as signaled");
            break;
        }
    }
}

void app_main(void)
{
    led_blink();
    // Initialize NVS for wifi station mode
    nvs_init();

    led_wait();
    wl_wifi_init();

    // Get device MAC address
    esp_err_t ret = esp_wifi_get_mac(ESP_IF_WIFI_STA, s_mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address, error: %s", esp_err_to_name(ret));
        return;
    }

    led_blink();
    // Sync time
    time_sync();

    // Initialize ESPNOW communication and add broadcast peer
    comm_init();
    comm_add_peer(COMM_BROADCAST_MAC_ADDR, false);
    comm_register_recv_msg_cb(recv_msg_cb);
    
    // Init MTQQ client to be ready to publish sensor data
    init_mqtt();
    mqtt_register_recv_msg_handler(mqtt_recv_msg_handler);

    // Start broadcasting master MAC address and wait for slave to send its address to complete the handshake.
    start_slavery_handshake();

    led_off();

    // Dummy main loop
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}