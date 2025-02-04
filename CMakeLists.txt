# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

message(STATUS "CMAKE_SOURCE_DIR ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

# Define the path to the virtual environment (in the root directory of the project)
set(VENV_PATH "${CMAKE_SOURCE_DIR}/.env")

# Create virtual env dir if not exist
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.env")
    message(STATUS "creating .env")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} python3 -m venv .env
    )

    # Install dependencies from requirements.txt
    execute_process(
        COMMAND ${VENV_PATH}/bin/pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result)
        message(FATAL_ERROR "Error installing requirements: ${error}\nOutput: ${output}")
    else()
        message(STATUS "Requirements installed successfully.")
    endif()
endif()
# Activate Python virtual environment setup
if (NOT DEFINED ENV{VIRTUAL_ENV})
    message(STATUS "Setting up Python virtual environment...")
    # Now run the python virtual environment activation (no need for chdir)
    execute_process(
        COMMAND sh -c "source ${CMAKE_SOURCE_DIR}/.env/bin/activate && echo 'Virtual environment activated'"
    )
endif()

# Function to generate .c and .h files from .proto files
function(generate_nanopb_files)
    # Specify the proto path (directory containing the .proto files)
    set(PROTO_PATH "${CMAKE_SOURCE_DIR}/proto")

    # List all .proto files
    file(GLOB_RECURSE PROTO_FILES "${PROTO_PATH}/*.proto")

    foreach(proto_file ${PROTO_FILES})
        execute_process(
            COMMAND ${CMAKE_SOURCE_DIR}/.env/bin/python3 ${CMAKE_SOURCE_DIR}/components/nanopb/generator/nanopb_generator.py -I${PROTO_PATH} -D${PROTO_PATH} ${proto_file}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
        )

        # Always output results for better debugging
        message(STATUS "Output: ${output}")
        message(STATUS "Error: ${error}")

        if(result)
            message(FATAL_ERROR "Error generating nanopb files for ${proto_file}: ${error}\nOutput: ${output}")
        endif()
    endforeach()
endfunction()

# Call the function to generate files
generate_nanopb_files()

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(mist)