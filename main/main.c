#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_types.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "sht4x.h"


#define SHT4X_SDA_GPIO      CONFIG_SHT4X_I2C_SDA  /*!< gpio number for I2C master data  */
#define SHT4X_SCL_GPIO      CONFIG_SHT4X_I2C_SCL  /*!< gpio number for I2C master clock */
#define I2C_PORT CONFIG_SHT4X_I2C_NUM   // I2C port number
#define I2C_MASTER_FREQ_HZ CONFIG_SHT4X_I2C_CLK_SPEED_HZ // Frequency of I2C clock

static const char *TAG = "Mist";


static esp_err_t i2c_bus_init(i2c_master_bus_handle_t *bus_handle, uint8_t sda_io, uint8_t scl_io)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, bus_handle));
    ESP_LOGI(TAG, "I2C master bus created");

    return ESP_OK;
}

// Task to monitor sensor connection
static esp_err_t wait_connection(i2c_master_bus_handle_t *bus_handle) {
    // Probe the sensor to check if it is connected to the bus with a timeout
    while (i2c_master_probe(*bus_handle, SHT4X_I2C_ADDR_0, 200) != ESP_OK) {
        ESP_LOGI(TAG, "SHT4X sensor not found");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return ESP_OK;
}


static void sht4x_read_task(void *pvParameters)
{
    float temperature, humidity;

    while (1) {
        i2c_master_dev_handle_t sht4x_handle = (i2c_master_dev_handle_t) pvParameters;
        esp_err_t err = sht4x_start_measurement(sht4x_handle, SHT4X_CMD_READ_MEASUREMENT_HIGH);
        vTaskDelay(pdMS_TO_TICKS(50));
        err = sht4x_read_measurement(sht4x_handle, &temperature, &humidity);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.2f C, Humidity: %.2f %%", temperature, humidity);
        } else {
            ESP_LOGE(TAG, "Failed to read temperature and humidity");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    i2c_master_bus_handle_t bus_handle = NULL;
    i2c_bus_init(&bus_handle, SHT4X_SDA_GPIO, SHT4X_SCL_GPIO);

    if(bus_handle != NULL) {
        i2c_master_dev_handle_t sht4x_handle = sht4x_device_create(bus_handle, SHT4X_I2C_ADDR_0, I2C_MASTER_FREQ_HZ);
        ESP_LOGI(TAG, "Sensor initialization success");

        // Check if the sensor is connected
        wait_connection(&bus_handle);

        ESP_LOGI(TAG, "SHT4X sensor found");
        xTaskCreate(sht4x_read_task, "sht4x_read_task", 4096, sht4x_handle, 5, NULL);
    }
    else {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
    }
}