# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Compile the protobuf files
message(STATUS "Running proto_compile.sh to compile proto files...")
execute_process(
    COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/proto_compile.sh"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

# Check for errors
if(result)
    message(FATAL_ERROR "Error running proto_compile.sh: ${error}\nOutput: ${output}")
else()
    message(STATUS "Proto compile completed successfully.")
endif()

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(mist)