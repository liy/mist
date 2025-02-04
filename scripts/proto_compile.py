import os
import subprocess
import sys
from pathlib import Path

# Define the root directory of the project
ROOT_DIR = Path(__file__).resolve().parent.parent

# Define the path to the virtual environment (in the root directory of the project)
VENV_PATH = ROOT_DIR / ".env"

# Specify the proto path (directory containing the .proto files)
PROTO_PATH = ROOT_DIR / "proto"

# Define the path that contains the protobuf compiled .c and .h files
PROTO_OUTPUT_PATH = ROOT_DIR / "proto_generated"

# Create virtual environment if it doesn't exist
if not VENV_PATH.exists():
    print("Creating .env")
    subprocess.run([sys.executable, "-m", "venv", str(VENV_PATH)], check=True)

    # Install dependencies from requirements.txt (if necessary)
    subprocess.run([str(VENV_PATH / "bin" / "pip"), "install", "-r", "requirements.txt"], check=True)

# Activate the virtual environment (only if not already activated)
if not os.getenv('VIRTUAL_ENV'):
    print("Setting up Python virtual environment...")

    activate_script = VENV_PATH / "bin" / "activate"
    exec(open(activate_script).read(), {'__file__': str(activate_script)})

# Function to generate .c and .h files from .proto files
def generate_nanopb_files():
    if not PROTO_OUTPUT_PATH.exists():
        print("Creating proto_generated directory")
        PROTO_OUTPUT_PATH.mkdir(parents=True, exist_ok=True)

    GENERATOR_PATH = ROOT_DIR / "components" / "nanopb" / "generator"

    # List all .proto files
    proto_files = list(PROTO_PATH.rglob("*.proto"))

    for proto_file in proto_files:
        proto_file_name = proto_file.stem
        proto_c_output = PROTO_OUTPUT_PATH / f"{proto_file_name}.pb.c"
        proto_h_output = PROTO_OUTPUT_PATH / f"{proto_file_name}.pb.h"
        
        if not proto_c_output.exists() or not proto_h_output.exists():
            print(f"Generating {proto_file_name}.pb.c and {proto_file_name}.pb.h from {proto_file}")
            subprocess.run([str(VENV_PATH / "bin" / "python3"), str(GENERATOR_PATH / "nanopb_generator.py"),
                            f"-I{PROTO_PATH}", f"-D{PROTO_OUTPUT_PATH}", str(proto_file)], check=True)

# Call the function to generate files
generate_nanopb_files()
