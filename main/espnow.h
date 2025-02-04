/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"

/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

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

static inline addr_msg_t* create_addr_message(const uint8_t* addr) {
    addr_msg_t *msg = malloc(sizeof(addr_msg_t));
    if (msg != NULL) {
        msg->header.type = MSG_TYPE_ADDRESS;
        memcpy(msg->src_mac, addr, ESP_NOW_ETH_ALEN);
    }
    return msg;
}

static inline bool deserialize_addr_message(addr_msg_t *msg, const uint8_t *buffer) {
    if (msg == NULL || buffer == NULL) return false; 

    size_t offset = sizeof(msg_header_t);
    memcpy(msg->src_mac, buffer + offset, ESP_NOW_ETH_ALEN);

    return true;
}

typedef struct {
    msg_header_t header;
    uint8_t temperature;
    uint8_t humidity;
} __attribute__((packed)) sensor_msg_t;

static inline sensor_msg_t* create_sensor_message(uint8_t temperature, uint8_t humidity) {
    sensor_msg_t *msg = malloc(sizeof(sensor_msg_t));
    if (msg != NULL) {
        msg->header.type = MSG_TYPE_SENSOR;
        msg->temperature = temperature;
        msg->humidity = humidity;
    }
    return msg;
}

static inline bool deserialize_sensor_message(sensor_msg_t *msg, const uint8_t *buffer) {
    if (msg == NULL || buffer == NULL) return false; 

    size_t offset = sizeof(msg_header_t);
    memcpy(&msg->temperature, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&msg->humidity, buffer + offset, sizeof(uint8_t));

    return true;
}

typedef struct {
    msg_header_t header;
    bool use_cache;
} __attribute__((packed)) query_msg_t;

static inline query_msg_t* create_query_message(bool use_cache) {
    query_msg_t *msg = malloc(sizeof(query_msg_t));
    if (msg != NULL) {
        msg->header.type = MSG_TYPE_QUERY;
        msg->use_cache = use_cache;
    }
    return msg;
}

static inline bool deserialize_query_message(query_msg_t *msg, uint8_t *buffer) {
    if (msg == NULL || buffer == NULL) return false; 
    
    size_t offset = sizeof(msg_header_t);
    memcpy(&msg->use_cache, buffer + offset, sizeof(bool));

    return true;
}

typedef struct {
    msg_header_t header;
    uint8_t instruction;
} __attribute__((packed)) instruction_msg_t;

static inline instruction_msg_t* create_instruction_message(uint8_t instruction) {
    instruction_msg_t *msg = malloc(sizeof(instruction_msg_t));
    if (msg != NULL) {
        msg->header.type = MSG_TYPE_INSTRUCTION;
        msg->instruction = instruction;
    }
    return msg;
}

static inline bool deserialize_instruction_message(instruction_msg_t *msg, uint8_t *buffer) {
    if (msg == NULL || buffer == NULL) return false; 

    size_t offset = sizeof(msg_header_t);
    memcpy(&msg->instruction, buffer + offset, sizeof(uint8_t));

    return true;
}

typedef struct {
    msg_type_t type;
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *buffer;
    size_t buffer_size;
} task_t;

task_t* create_task(msg_header_t *msg, uint8_t mac_addr[ESP_NOW_ETH_ALEN]);

// Forward declaration of init_queue
esp_err_t espnow_init(void);

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