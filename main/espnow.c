#include "espnow.h"

#define ESPNOW_MAXDELAY 512

static const char *TAG = "Mist";

// Queue for sending and receiving data
static QueueHandle_t s_espnow_queue = NULL;

// Broadcast MAC address
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };


// Discovery mode flag
static bool discovery_mode = true;

/* WiFi should start before using ESPNOW */
static void init_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Send callback, data to "MACSTR", status: %d", MAC2STR(mac_addr), status);
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    uint8_t *mac_addr = recv_info->src_addr;
    // uint8_t *des_addr = recv_info->des_addr;

    ESP_LOGI(TAG, "Receive data from "MACSTR", len: %d", MAC2STR(mac_addr), data_len);
}


esp_err_t add_peer(const uint8_t *peer_addr) {
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);
    return ESP_OK;
}

/**
* @brief Clean up ESPNOW queue data.
*/ 
static void espnow_deinit(task_t *task)
{
    free(task->buffer);
    free(task);
    vSemaphoreDelete(s_espnow_queue);
    esp_now_deinit();
}

// Function to remove a peer
esp_err_t remove_peer(const uint8_t *peer_addr) {
    esp_err_t err = esp_now_del_peer(peer_addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove peer: %s", esp_err_to_name(err));
    }
    return err;
}

void task_loop() {
    task_t* task;
    while (true) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        if(xQueueReceive(s_espnow_queue, &task, portMAX_DELAY) == pdTRUE) {

            switch(task->type) {
                case MSG_TYPE_SENSOR:
                    sensor_msg_t *msg = malloc(sizeof(sensor_msg_t));
                    if(msg == NULL) {
                        ESP_LOGE(TAG, "Malloc sensor message fail");
                        free(task->buffer);
                        free(task);
                        continue;
                    }
                    
                    if(!deserialize_sensor_message(msg, task->buffer)) {
                        ESP_LOGE(TAG, "Deserialize sensor message fail");
                        free(msg);
                        free(task->buffer);
                        free(task);
                        continue;
                    }

                    // TODO: add sensor data to database
                    ESP_LOGI(TAG, "Temperature: %d, Humidity: %d", msg->temperature, msg->humidity);
                    break;
                case MSG_TYPE_QUERY:
                case MSG_TYPE_INSTRUCTION:
                    if (esp_now_send(task->mac_addr, task->buffer, task->buffer_size) != ESP_OK) {
                        ESP_LOGE(TAG, "Query send error");
                        espnow_deinit(task);
                        vTaskDelete(NULL);
                    }
                    ESP_LOGI(TAG, "Send message to "MACSTR"", MAC2STR(task->mac_addr));
                    break;
                default:
                    ESP_LOGE(TAG, "Unknown message type");
                    espnow_deinit(task);
                    vTaskDelete(NULL);
                    continue;
            }
        }
    }
    vQueueDelete(s_espnow_queue);
    vTaskDelete(NULL);
}

esp_err_t init_queue() {
    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(esp_now_peer_info_t));
    if (s_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Error creating the ESPNOW queue");
    } else {
        ESP_LOGI(TAG, "ESPNOW queue created");
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    // Data encryption configuration
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    // Start the queue pulling loop
    xTaskCreate(task_loop, "task_loop", 2048, NULL, 4, NULL);

    return ESP_OK;
}

esp_err_t send(msg_header_t* msg, uint8_t des_mac[ESP_NOW_ETH_ALEN]) {
    task_t* task = create_task(msg, des_mac);
    if (task == NULL) {
        ESP_LOGE(TAG, "Create task fail");
        return ESP_FAIL;
    }
    
    // Send the structure to the queue, the structure will be cloned.
    // The receiver will be responsible for freeing the data, so it is safe to send pointer into the queue
    if (xQueueSend(s_espnow_queue, &task, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t broadcast(msg_header_t* msg) {
    return send(msg, s_broadcast_mac);
}

task_t* create_task(msg_header_t *msg, uint8_t mac_addr[ESP_NOW_ETH_ALEN]) {
    task_t* task = malloc(sizeof(task_t));
    if (task == NULL) {
        ESP_LOGE(TAG, "Malloc task fail");
        return NULL;
    }
    
    task->type = msg->type;
    switch(msg->type) {
        case MSG_TYPE_SENSOR:
            task->buffer_size = sizeof(sensor_msg_t);
            break;
        case MSG_TYPE_QUERY:
            task->buffer_size = sizeof(query_msg_t);
            break;
        case MSG_TYPE_INSTRUCTION:
            task->buffer_size = sizeof(instruction_msg_t);
            break;
        default:
            ESP_LOGE(TAG, "Unknown message type");
            free(task);
            return NULL;
    }

    memcpy(task->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    task->buffer = malloc(task->buffer_size);
    memcpy(task->buffer, msg, task->buffer_size);

    return task;
}

esp_err_t espnow_init() {
    // Initialize NVS, this is required by wifi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    init_wifi();

    if(init_queue() != ESP_OK) {
        return ESP_FAIL;
    }

    broadcast((msg_header_t*)create_sensor_message(37, 58));
    
    return ESP_OK;
}

void set_discovery_mode(bool enabled) {
    if (enabled) {
        // Add broadcast peer information to peer list
        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
        if (peer == NULL) {
            ESP_LOGE(TAG, "Malloc peer information fail");
            return;
        }
        memset(peer, 0, sizeof(esp_now_peer_info_t));
        peer->channel = CONFIG_ESPNOW_CHANNEL;
        peer->ifidx = ESPNOW_WIFI_IF;
        // Data encryption configuration
        peer->encrypt = false;
        memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
        free(peer);
        discovery_mode = true;
    } else {
        discovery_mode = false;
        remove_peer(s_broadcast_mac);
    }
}

bool get_discovery_mode(void) {
    return discovery_mode;
}