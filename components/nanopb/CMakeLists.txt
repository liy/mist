# Collect all .c files in the main directory
file(GLOB MAIN_SRCS "pb_common.c" "pb_encode.c" "pb_decode.c")

# Collect all .c files in the proto_generated directory
file(GLOB PROTO_SRCS "./proto_generated/*.c")

idf_component_register(
  SRCS ${MAIN_SRCS} ${PROTO_SRCS}
  INCLUDE_DIRS "."

  # Include the protobuf generated files
  "./proto_generated"
)
