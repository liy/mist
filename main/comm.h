/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#define ESPNOW_QUEUE_SIZE           6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

// /* User defined field of ESPNOW data in this example. */
// typedef struct {
//     uint16_t crc;                         //CRC16 value of ESPNOW data.
//     uint8_t payload[0];                   //Real payload of ESPNOW data. Flexiable array member should be the last member of espnow_data_t.
// } __attribute__((packed)) espnow_data_t;

// /* Parameters of sending ESPNOW data. */
// typedef struct {
//     bool broadcast;                       //Send broadcast ESPNOW data.
//     uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
//     uint16_t buffer_size;                 //Length of ESPNOW data.
//     uint8_t des_mac[ESP_NOW_ETH_ALEN];   //MAC address of destination device.
// } espnow_send_param_t;

typedef enum {
    MSG_TYPE_ADDRESS,
    MSG_TYPE_SENSOR,
    MSG_TYPE_QUERY,
    MSG_TYPE_INSTRUCTION
} msg_type_t;

typedef struct {
    msg_type_t type; // Common property to determine the struct type
} msg_header_t;

typedef struct {
    msg_header_t header;
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
} __attribute__((packed)) addr_msg_t;


typedef struct {
    msg_header_t header;
    uint8_t temperature;
    uint8_t humidity;
} __attribute__((packed)) sensor_msg_t;



typedef struct {
    msg_header_t header;
    bool use_cache;
} __attribute__((packed)) query_msg_t;

typedef struct {
    msg_header_t header;
    uint8_t instruction;
} __attribute__((packed)) instruction_msg_t;



typedef struct {
    msg_type_t type;
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *buffer;
    size_t buffer_size;
} task_t;

task_t* create_task(msg_header_t *msg, uint8_t mac_addr[ESP_NOW_ETH_ALEN]);

esp_err_t comm_init(void);

// Send data to a specific MAC address
esp_err_t send(msg_header_t *msg, uint8_t des_mac[ESP_NOW_ETH_ALEN]);

// Broadcast data to all peers
esp_err_t broadcast(msg_header_t *msg);

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