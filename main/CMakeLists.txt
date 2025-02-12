# Collect all .c files in the main directory
file(GLOB MAIN_SRCS "main.c" "*.c")

idf_component_register(
        SRCS ${MAIN_SRCS}
        PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
        INCLUDE_DIRS "."

        # Make sure other components are included
        REQUIRES mist_messages mist_wireless time_sync mist_comm mqtt sed
)

# Create a NVS image from the contents of the `nvs_data` CSV file
# that fits the partition named 'nvs'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
nvs_create_partition_image(nvs ../nvs.csv FLASH_IN_PROJECT)