#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <esp_random.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_crc.h>
#include "comm.h"

#define ESPNOW_MAXDELAY 512

static const char *TAG = "Mist";

// Queue for sending and receiving data
static QueueHandle_t s_espnow_queue = NULL;


// Discovery mode flag
static bool discovery_mode = true;

// Register a new message handler
static message_handler_t s_message_handler;

void register_message_handler(message_handler_t handler) {
    s_message_handler = handler;
}

void deregister_message_handler(void) {
    s_message_handler = NULL;
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
    // peer->ifidx = ESPNOW_WIFI_IF;
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
    while (xQueueReceive(s_espnow_queue, &task, portMAX_DELAY) == pdTRUE) {
        if (s_message_handler != NULL && !s_message_handler(task)) {
            ESP_LOGE(TAG, "Message handler failed");
        }
        free(task->buffer);
        free(task);
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
    // peer->ifidx = ESPNOW_WIFI_IF;
    // Data encryption configuration
    peer->encrypt = false;
    memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    // Start the queue pulling loop
    xTaskCreate(task_loop, "task_loop", 2048, NULL, 4, NULL);

    return ESP_OK;
}

esp_err_t send(const uint8_t* buffer, const int64_t buffer_size, uint8_t des_mac[ESP_NOW_ETH_ALEN]) {
    task_t* task = create_task(buffer, buffer_size, des_mac, true);
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

esp_err_t broadcast(const uint8_t* buffer, const int64_t buffer_size) {
    return send(buffer, buffer_size, broadcast_mac);
}

task_t* create_task(const uint8_t* buffer, const int64_t buffer_size, uint8_t mac_addr[ESP_NOW_ETH_ALEN], bool is_outbound) {
    task_t* task = malloc(sizeof(task_t));
    if (task == NULL) {
        ESP_LOGE(TAG, "Malloc task fail");
        return NULL;
    }

    task->is_outbound = is_outbound;
    task->buffer = malloc(buffer_size);
    memcpy(task->buffer, buffer, buffer_size);
    task->buffer_size = buffer_size;
    memcpy(task->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

    return task;
}

esp_err_t comm_init() {
    if(init_queue() != ESP_OK) {
        return ESP_FAIL;
    }
    
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
        // peer->ifidx = ESPNOW_WIFI_IF;
        // Data encryption configuration
        peer->encrypt = false;
        memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
        free(peer);
        discovery_mode = true;
    } else {
        discovery_mode = false;
        remove_peer(broadcast_mac);
    }
}

bool get_discovery_mode(void) {
    return discovery_mode;
}