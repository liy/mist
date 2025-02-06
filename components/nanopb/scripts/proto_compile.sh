#!/bin/bash

# Define paths based on the component root
ROOT_DIR=$(dirname $(dirname "$(realpath "$0")"))  # Parent directory of scripts
VENV_PATH="$ROOT_DIR/.env"
PROTO_PATH="$ROOT_DIR/proto"
PROTO_OUTPUT_PATH="$ROOT_DIR/proto_generated"
GENERATOR_PATH="$ROOT_DIR/generator"

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_PATH" ]; then
    echo "Creating .env virtual environment..."
    python3 -m venv "$VENV_PATH"
    # Install dependencies from requirements.txt (optional, uncomment if needed)
    "$VENV_PATH/bin/pip" install -r "$ROOT_DIR/requirements.txt"
fi

# Activate the virtual environment
source "$VENV_PATH/bin/activate"

# Create proto_generated directory if it doesn't exist
if [ ! -d "$PROTO_OUTPUT_PATH" ]; then
    echo "Creating proto_generated directory..."
    mkdir -p "$PROTO_OUTPUT_PATH"
fi

# List all .proto files and process them
for proto_file in "$PROTO_PATH"/*.proto; do
    if [ -f "$proto_file" ]; then
        proto_file_name=$(basename "$proto_file" .proto)
        proto_c_output="$PROTO_OUTPUT_PATH/$proto_file_name.pb.c"
        proto_h_output="$PROTO_OUTPUT_PATH/$proto_file_name.pb.h"
        
        # Generate .c and .h files
        echo "Generating $proto_file_name.pb.c and $proto_file_name.pb.h from $proto_file..."
        "$VENV_PATH/bin/python3" "$GENERATOR_PATH/nanopb_generator.py" -I"$PROTO_PATH" -D"$PROTO_OUTPUT_PATH" "$proto_file"
    fi
done

echo "Proto compilation completed."
