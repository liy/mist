# Collect all .c files in the main directory
file(GLOB MAIN_SRCS "main.c" "*.c")

idf_component_register(
        SRCS ${MAIN_SRCS}
        PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
        INCLUDE_DIRS "."

        # Make sure other components are included
        REQUIRES mist_messages mist_wireless time_sync mist_comm mqtt sed
)

# This line auto flash the nvs.csv file to the partition table, but it will override all the data in the partition table
# Therefore it is commented out during development
# nvs_create_partition_image(nvs ../nvs.csv FLASH_IN_PROJECT)