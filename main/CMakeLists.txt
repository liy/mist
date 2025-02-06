# Collect all .c files in the main directory
file(GLOB MAIN_SRCS "comm.c" "main.c" "*.c")

idf_component_register(
        SRCS ${MAIN_SRCS}
        PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
        INCLUDE_DIRS "."

        # Make sure other components are included
        REQUIRES mist_messages wireless time_sync comm
)