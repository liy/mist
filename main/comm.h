/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#define ESPNOW_QUEUE_SIZE           6


// Broadcast MAC address
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef struct {
    bool is_inbound;
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *buffer;
    size_t buffer_size;
} task_t;

task_t* create_task(const uint8_t* buffer, const int64_t buffer_size, uint8_t mac_addr[ESP_NOW_ETH_ALEN], bool is_outbound);

esp_err_t comm_init(void);

// Send data to a specific MAC address
esp_err_t send(const uint8_t* buffer, const int64_t buffer_size, uint8_t des_mac[ESP_NOW_ETH_ALEN]);

// Broadcast data to all peers
esp_err_t broadcast(const uint8_t* buffer, const int64_t buffer_size);

/**
 * @brief Enables or disables the discovery mode.
 *
 * This function sets the discovery mode to either enabled or disabled.
 * When discovery mode is enabled, the device will be listening for public broadcasts.
 *
 * @param enabled A boolean value indicating whether to enable (true) or disable (false) discovery mode.
 */
void set_discovery_mode(bool enabled);

bool get_discovery_mode(void);


/**
 * @typedef message_handler_t
 * @brief A function pointer type for handling messages.
 *
 * This type defines a function pointer that takes a constant pointer to a task_t
 * structure and returns a boolean value indicating the success or failure of the
 * message handling operation.
 *
 * @param task A constant pointer to a task_t structure representing the task to be handled.
 * @return A boolean value indicating the success (true) or failure (false) of the message handling.
 */
typedef bool (*message_handler_t)(const task_t* task);

void register_message_handler(message_handler_t handler);

void deregister_message_handler(void);