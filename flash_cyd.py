#!/usr/bin/env python3
import sys
import os
import subprocess
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VENV_PY = os.path.join(SCRIPT_DIR, ".venv", "bin", "python")
PORT = "/dev/cu.usbserial-11120"
BUILD_DIR = os.path.join(SCRIPT_DIR, ".pio", "build", "cyd")

bootloader = os.path.join(BUILD_DIR, "bootloader.bin")
partitions = os.path.join(BUILD_DIR, "partitions.bin")
firmware = os.path.join(BUILD_DIR, "firmware.bin")

cmd = [
    VENV_PY, "-m", "esptool",
    "--chip", "esp32",
    "--port", PORT,
    "--baud", "460800",
    "--before", "default_reset",
    "--after", "hard_reset",
    "write_flash", "-z",
    "--flash_mode", "dio",
    "--flash_freq", "40m",
    "--flash_size", "4MB",
    "0x1000", bootloader,
    "0x8000", partitions,
    "0x10000", firmware
]

print("=" * 65)
print("  ESP32-035 (CYD 3.5\" ESP32-3248S035R) Firmware Flasher")
print("  Target Port: " + PORT)
print("=" * 65)
print("\n[INSTRUCTION] Please PRESS and HOLD the 'BOOT' button on the board.")
print("              (Keep holding it until you see 'Writing at 0x00010000...')\n")

max_attempts = 10
for attempt in range(1, max_attempts + 1):
    print(f"\n--- [Attempt {attempt}/{max_attempts}] Connecting to ESP32... ---")
    ret = subprocess.call(cmd)
    if ret == 0:
        print("\n" + "=" * 65)
        print(" [SUCCESS] Display Flashed Successfully!")
        print(" Look at your screen: The ESP32-035 Display is LIVE!")
        print("=" * 65 + "\n")
        sys.exit(0)
    print("Retrying in 2 seconds... (Make sure to HOLD the BOOT button!)")
    time.sleep(2)

print("\n[!] Connection timed out. Please check your cable and try again.")
sys.exit(1)
