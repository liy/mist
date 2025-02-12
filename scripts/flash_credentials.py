import os
import subprocess
import json

# Get the directory of the current script
script_dir = os.path.dirname(os.path.abspath(__file__))

# Define the path to the NVS CSV file
nvs_csv_file = os.path.join(script_dir, "../nvs.csv")

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

print("nvs.csv has been flashed into NVS.")