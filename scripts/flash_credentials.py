import os
import subprocess
import json

# This script reads the credentials from the .credentials file and writes them to the NVS CSV file.

# Get the directory of the current script
script_dir = os.path.dirname(os.path.abspath(__file__))

# Source the ESP-IDF environment
command = 'source $HOME/esp/esp-idf/export.sh && env'
result = subprocess.run(command, shell=True, executable='/bin/zsh', capture_output=True, text=True)
if result.returncode != 0:
    raise EnvironmentError("Failed to source the ESP-IDF environment.")

# Define the path to the credentials file
credentials_file = os.path.join(script_dir, "../.credentials")

# Check if the credentials file exists
if not os.path.exists(credentials_file):
    raise FileNotFoundError(f"Credentials file not found: {credentials_file}")

# Read the credentials from the file
with open(credentials_file, 'r') as file:
    lines = file.readlines()

# Extract the credentials
credentials = {}
current_key = None
current_value = []
multi_line_value = False
for line in lines:
    line = line.strip()
    if multi_line_value:
        if line.endswith('"""'):
            current_value.append(line[:-3])
            credentials[current_key] = '\n'.join(current_value)
            multi_line_value = False
        else:
            current_value.append(line)
    elif '=' in line:
        if current_key:
            credentials[current_key] = '\n'.join(current_value)
        key, value = line.split('=', 1)
        if value.startswith('"""'):
            current_key = key
            current_value = [value[3:]]
            multi_line_value = True
        else:
            credentials[key] = value
            current_key = None
            current_value = []
    else:
        current_value.append(line)
if current_key:
    credentials[current_key] = '\n'.join(current_value)

# Define the path to the NVS CSV file
nvs_csv_file = os.path.join(script_dir, "../nvs.csv")

# Write the credentials to the NVS CSV file
try:
    with open(nvs_csv_file, 'w') as file:
        file.write("key,type,encoding,value\n")
        for key, value in credentials.items():
            if len(key) > 15:
                print(f"Error: Length of key `{key}` should be <= 15 characters.")
                continue
            value = value.replace('\n', '\\n')  # Replace newline characters with \n
            file.write(f"{key},data,string,{value}\n")
except Exception as e:
    raise IOError(f"Failed to write to NVS CSV file: {e}")

# Define the path to the NVS partition binary
nvs_partition_bin = os.path.join(script_dir, "../build/nvs_partition.bin")

# Path to the nvs_partition_gen.py script
idf_path = os.environ.get('IDF_PATH')
if not idf_path:
    raise EnvironmentError("IDF_PATH environment variable is not set. Please source the ESP-IDF environment.")
nvs_partition_gen_script = os.path.join(idf_path, 'components', 'nvs_flash', 'nvs_partition_generator', 'nvs_partition_gen.py')

# Generate the NVS partition binary
command = [
    'python3', nvs_partition_gen_script, 'generate', nvs_csv_file, nvs_partition_bin, '0x6000'
]
result = subprocess.run(command, check=True)
if result.returncode != 0:
    raise RuntimeError("Failed to generate NVS partition binary.")

# Read the serial port from the .vscode/settings.json file
settings_file = os.path.join(script_dir, "../.vscode/settings.json")
with open(settings_file, 'r') as file:
    settings = json.load(file)
    serial_port = settings.get("idf.port")

if not serial_port:
    raise ValueError("Serial port not found in .vscode/settings.json")

# Flash the NVS partition binary
command = [
    'esptool.py', '--chip', 'esp32c6', '--port', serial_port, '--baud', '115200', 'write_flash',
    '0x9000', nvs_partition_bin
]
result = subprocess.run(command, check=True)
if result.returncode != 0:
    raise RuntimeError("Failed to flash NVS partition binary.")

print("Credentials have been flashed into NVS.")