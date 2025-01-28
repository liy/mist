#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

#define I2C_MASTER_SCL_IO 22       // GPIO for SCL
#define I2C_MASTER_SDA_IO 21       // GPIO for SDA
#define I2C_MASTER_NUM I2C_NUM_0   // I2C port number
#define I2C_MASTER_FREQ_HZ 100000 // Frequency of I2C clock
#define SHT4X_ADDR 0x44            // I2C address of SHT4x sensor
#define SHT4X_CMD_MEASURE_HIGH 0xFD // Command to measure T and RH with high repeatability
#define TAG "MIST_POT"

typedef enum {
    SENSOR_DISCONNECTED,
    SENSOR_CONNECTED
} sensor_state_t;

sensor_state_t sensor_state = SENSOR_DISCONNECTED;

// Initialize I2C
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) return ret;
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Check if the sensor is connected
static esp_err_t check_sensor_connection(void) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHT4X_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Read temperature and humidity from SHT4x
static esp_err_t read_sensor_data(float *temperature, float *humidity) {
    uint8_t data[6];
    esp_err_t ret;

    // Send the measurement command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHT4X_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SHT4X_CMD_MEASURE_HIGH, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send measurement command: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for the measurement to complete
    vTaskDelay(pdMS_TO_TICKS(10));

    // Read the measurement data
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHT4X_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
        return ret;
    }

    // Convert raw data to temperature and humidity
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_humidity = (data[3] << 8) | data[4];

    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity = 100.0f * ((float)raw_humidity / 65535.0f);

    ESP_LOGI(TAG, "Temperature: %.2f °C, Humidity: %.2f %%", *temperature, *humidity);

    return ESP_OK;
}

// Task to monitor sensor connection
void sensor_monitor_task(void *param) {
    while (1) {
        esp_err_t ret = check_sensor_connection();
        if (ret == ESP_OK) {
            if (sensor_state == SENSOR_DISCONNECTED) {
                ESP_LOGI(TAG, "Sensor connected!");
                sensor_state = SENSOR_CONNECTED;
            }
        } else {
            if (sensor_state == SENSOR_CONNECTED) {
                ESP_LOGW(TAG, "Sensor disconnected!");
                sensor_state = SENSOR_DISCONNECTED;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(sensor_state == SENSOR_CONNECTED ? 2000 : 10000));
    }
}

// Main application
void app_main(void) {
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // Start sensor monitor task
    xTaskCreate(sensor_monitor_task, "sensor_monitor_task", 2048, NULL, 10, NULL);

    while (1) {
        if (sensor_state == SENSOR_CONNECTED) {
            float temperature = 0.0f, humidity = 0.0f;
            esp_err_t ret = read_sensor_data(&temperature, &humidity);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to read sensor data");
            }
        } else {
            ESP_LOGI(TAG, "Waiting for sensor...");
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); // Main task delay
    }
}
