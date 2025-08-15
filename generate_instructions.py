# Script for generating a firmware package

import os
import csv
import json
import zlib
import shutil

def convert_size(size_str):
    """Convert size string to bytes."""
    if size_str.endswith("K"):
        return int(size_str[:-1]) * 1024
    elif size_str.endswith("M"):
        return int(size_str[:-1]) * 1024 * 1024
    elif size_str.endswith("G"):
        return int(size_str[:-1]) * 1024 * 1024 * 1024
    else:
        return int(size_str)

def read_partition_table(filename):
    partitions = []
    with open(filename, "r") as f:
        data = f.read()
        lines = data.splitlines()
        lines = [line for line in lines if not line.strip().startswith("#")]
        reader = csv.reader(lines)
        for row in reader:
            partition = {
            "name": row[0],
            "type": row[1],
            "subtype": row[2],
            "offset": int(row[3], 16),
            "size": convert_size(row[4]),
            "flags": row[5],
            }
            partitions.append(partition)
    return partitions

def find_partition(partitions, name):
    for partition in partitions:
        if partition["name"] == name:
            return partition
    return None

def calculate_md5(filename):
    """Calculate MD5 checksum of a file."""
    import hashlib
    hash_md5 = hashlib.md5()
    with open(filename, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def get_size(filename):
    return os.path.getsize(filename)

def compress(in_file, out_file):
    with open(in_file, "rb") as input_file:
        with open(out_file, "wb") as output_file:
            output_file.write(zlib.compress(input_file.read(), 9))

# Constants
bootloader_offset = 0x2000
partition_table_offset = 0x8000
bootloader_path = "build/tanmatsu/bootloader/bootloader.bin"
partition_table_csv_path = "partition_tables/16M.csv"
partition_table_path = "build/tanmatsu/partition_table/partition-table.bin"
firmware_path = "build/tanmatsu/tanmatsu-launcher.bin"
ota_data_path = "build/tanmatsu/ota_data_initial.bin"
locfd_path = "build/tanmatsu/locfd.bin"

compress = False

# Generate instructions
partitions = read_partition_table(partition_table_csv_path)
firmware_offset = find_partition(partitions, "ota_0")["offset"]
ota_data_offset = find_partition(partitions, "otadata")["offset"]
locfd_offset = find_partition(partitions, "locfd")["offset"]


bootloader_hash = calculate_md5(bootloader_path)
partitions_hash = calculate_md5(partition_table_path)
ota_data_hash = calculate_md5(ota_data_path)
firmware_hash = calculate_md5(firmware_path)
locfd_hash = calculate_md5(locfd_path)

bootloader_size = get_size(bootloader_path)
partitions_size = get_size(partition_table_path)
ota_data_size = get_size(ota_data_path)
firmware_size = get_size(firmware_path)
locfd_size = get_size(locfd_path)

steps = [
    {"file": "bootloader." + ("zz" if compress else "bin"), "offset": bootloader_offset, "hash": bootloader_hash, "size": bootloader_size, "compressed": compress, "optional": False},
    {"file": "partitions." + ("zz" if compress else "bin"), "offset": partition_table_offset, "hash": partitions_hash, "size": partitions_size, "compressed": compress, "optional": False},
    {"file": "ota_data." + ("zz" if compress else "bin"), "offset": ota_data_offset, "hash": ota_data_hash, "size": ota_data_size, "compressed": compress, "optional": False},
    {"file": "firmware." + ("zz" if compress else "bin"), "offset": firmware_offset, "hash": firmware_hash, "size": firmware_size, "compressed": compress, "optional": False},
    {"file": "locfd." + ("zz" if compress else "bin"), "offset": locfd_offset, "hash": locfd_hash, "size": locfd_size, "compressed": compress, "optional": True},
]

instructions = {
    "information": {
        "name": "Tanmatsu launcher firmware",
        "version": "tbd"
    },
    "steps": steps
}

try:
    os.mkdir("dist")
except:
    pass

# Save instructions to JSON file
output_file = "dist/instructions.trf"
with open(output_file, "w") as f:
    json.dump(instructions, f)

if compress:
    # Compress firmware parts
    compress(bootloader_path, "dist/bootloader.zz")
    compress(partition_table_path, "dist/partitions.zz")
    compress(firmware_path, "dist/firmware.zz")
    compress(ota_data_path, "dist/ota_data.zz")
    compress(locfd_path, "dist/locfd.zz")
else:
    shutil.copyfile(bootloader_path, "dist/bootloader.bin")
    shutil.copyfile(partition_table_path, "dist/partitions.bin")
    shutil.copyfile(firmware_path, "dist/firmware.bin")
    shutil.copyfile(ota_data_path, "dist/ota_data.bin")
    shutil.copyfile(locfd_path, "dist/locfd.bin")
