#include <time.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <string.h>
#include <esp_mac.h>
#include <mqtt_client.h>

#include "pb_encode.h"
#include "pb_decode.h"
#include "messages.pb.h"
#include "wireless.h"
#include "time_sync.h"
#include <esp_now.h>
#include "comm.h"

#define BROKER_URL "mqtt://192.168.3.105:1883"  // Replace with your broker URL

static const char *TAG = "Mist";

// Task handle to notify when slave has sent over its the address
static TaskHandle_t s_handshake_notify = NULL;

static esp_mqtt_client_handle_t client;

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
            ESP_LOGI(TAG, "Received MIST_SENSOR query, timestamp: %lld, humidity: %f, temperature: %f", 
                     query->body.mist_sensor.timestamp, query->body.mist_sensor.humidity, query->body.mist_sensor.temperature);

            char *sensor_data_str = malloc(200 * sizeof(char));
            snprintf(sensor_data_str, 200, "timestamp=%lld,humidity=%.2f,temperature=%.2f", 
                     query->body.mist_sensor.timestamp, 
                     query->body.mist_sensor.humidity, 
                     query->body.mist_sensor.temperature);
            
            // Publish JSON data to MQTT topic
            int msg_id = esp_mqtt_client_publish(client, "/esp32/sensor", sensor_data_str, 0, 0, 0);
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);

            char* temperature_str = malloc(30 * sizeof(char));
            snprintf(temperature_str, 30, "%.2f", query->body.mist_sensor.temperature);
            msg_id = esp_mqtt_client_publish(client, "/esp32/temperature", temperature_str, 0, 0, 0);
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);

            char* humidity_str = malloc(30 * sizeof(char));
            snprintf(humidity_str, 30, "%.2f", query->body.mist_sensor.humidity);
            msg_id = esp_mqtt_client_publish(client, "/esp32/humidity", humidity_str, 0, 0, 0);
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);


            free(sensor_data_str);
            free(temperature_str);
            free(humidity_str);

            break;
            
        case SensorType_AIR_SENSOR:
            ESP_LOGI(TAG, "Received AIR_SENSOR query, timestamp: %lld, humidity: %f, temperature: %f", 
                     query->body.air_sensor.timestamp, query->body.air_sensor.humidity, query->body.air_sensor.temperature);
            break;
            
        case SensorType_LIGHT_SENSOR:
            ESP_LOGI(TAG, "Received LIGHT_SENSOR query, timestamp: %lld, light intensity: %f", 
                     query->body.light_sensor.timestamp, query->body.light_sensor.intensity);
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown sensor type: %d", query->sensor_type);
            break;
    }
}

static void handle_command(const Command* cmd) {
    ESP_LOGI(TAG, "Received command with ID: %lld", cmd->id);
    
    switch (cmd->which_body) {
        case Command_sleep_cycle_tag:
            ESP_LOGI(TAG, "Sleep cycle command with duration: %lld", 
                     cmd->body.sleep_cycle.sleep_time);
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
        case MessageType_SENSOR_QUERY: {
            SensorQuery query = SensorQuery_init_default;
            if (pb_decode(&stream, SensorQuery_fields, &query)) {
                handle_sensor_query(&query);
            } else {
                ESP_LOGE(TAG, "Failed to decode SensorQuery: %s", PB_GET_ERROR(&stream));
                return ESP_FAIL;
            }
            break;
        }
        case MessageType_COMMAND: {
            Command cmd = Command_init_default;
            if (pb_decode(&stream, &Command_msg, &cmd)) {
                handle_command(&cmd);
            } else {
                ESP_LOGE(TAG, "Failed to decode command: %s", PB_GET_ERROR(&stream));
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

void start_slavery_handshake() {
    // Store the handle of the current handshake task
    s_handshake_notify = xTaskGetCurrentTaskHandle();

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

    for(uint i = 0; i < 60; i++) {
        if(ulTaskNotifyTake(pdTRUE, 1000 / portTICK_PERIOD_MS) == 1) {
            ESP_LOGI(TAG, "Exiting loop as signaled");
            break;
        }
        ESP_LOGI(TAG, "Broadcasting slavery handshake with master MAC address...");
        comm_broadcast(buffer, buffer_size);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
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
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGW(TAG, "Unknown event id:%d", event->event_id);
        break;
    }
}

void init_mtqq() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // Connect to the broker
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    // Initialize NVS for wifi station mode
    nvs_init();

    wl_wifi_init();

    // Sync time
    time_sync();
    // Initialize ESPNOW communication and add broadcast peer
    comm_init();
    comm_add_peer(COMM_BROADCAST_MAC_ADDR, false);
    comm_register_recv_msg_cb(recv_msg_cb);
    
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

    // Init MTQQ client to be ready to publish sensor data 
    init_mtqq();

    // Start broadcasting master MAC address and wait for slave to send its address to complete the handshake.
    start_slavery_handshake();

    // Dummy main loop
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}