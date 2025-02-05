# Collect all .c files in the main directory
file(GLOB MAIN_SRCS "*.c")

# Collect all .c files in the proto_generated directory
file(GLOB PROTO_SRCS "../proto_generated/*.c")

idf_component_register(
        SRCS ${MAIN_SRCS} ${PROTO_SRCS}
        PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi
        INCLUDE_DIRS "."

        # Include the protobuf generated files
        "../proto_generated"

        # Make sure nanopb component is included
        REQUIRES nanopb wireless time_sync
)