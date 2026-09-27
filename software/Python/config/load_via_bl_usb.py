from pathlib import Path
import zlib
import struct
import serial
import argparse
import sys

# All the orbweaver bootloader commands
PING_CMD            = 0x11
ERASE_FLASH_CMD     = 0x22
WRITE_METADATA_CMD  = 0x33
WRITE_APP_CMD       = 0x44
VERIFY_FLASH_CMD    = 0x55
LOAD_MEM_CMD        = 0x66
VERIFY_MEM_CMD      = 0x77
LAUNCH_CMD          = 0x88
ERASE_MEM_CMD       = 0x99
ACK_CMD             = 0xAA
NAK_CMD             = 0xBB

ACTIVATE_BOOTLOADER_CMD = 1
LOAD_APP_CMD = 2
LAUNCH_APP_BOOT_CMD = 3

SERIAL_BAUD = 115200

def wait_for_ack(ser):
        
    ack = bytes(ser.read(1))

    if ack[0] == ACK_CMD:
         return True
    else:
        return False

def erase_mem_bl_serial(serial_port):

    ser = serial.Serial(port=serial_port, baudrate=SERIAL_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=None)

    ser.write([PING_CMD])

    if wait_for_ack(ser):
        print("Ping OK")

    ser.write([ERASE_MEM_CMD])

    if wait_for_ack(ser):
        print("Erase program memory OK")


def load_via_bl_serial(bin_path, serial_port):

    bin_path = Path(bin_path)

    if not bin_path.is_file():
        print(f"Error: File not found {bin_path}")
        sys.exit(1)  

    ser = serial.Serial(port=serial_port, baudrate=SERIAL_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=None)

    ser.write([PING_CMD])

    if wait_for_ack(ser):
        print("Ping OK")

    ser.write([ERASE_FLASH_CMD])

    if wait_for_ack(ser):
        print("Erase flash OK")

    file_size = Path(bin_path).stat().st_size

    crc = 0

    with open(bin_path, "rb") as f:
            while chunk := f.read(65536):
                crc = zlib.crc32(chunk, crc)

    crc32 = crc & 0xFFFFFFFF

    # If you don't want the bootloader to launch the app after the reset, then change this command.
    metadata = struct.pack('<III', file_size, crc32, LAUNCH_APP_BOOT_CMD)

    # The metadata must be written before the app, so the bootloader know how big the app is.
    ser.write([WRITE_METADATA_CMD])
    ser.write(metadata)

    if wait_for_ack(ser):
        print("Write Metadata OK")

    ser.write([WRITE_APP_CMD])

    iterator = 0

    with open(bin_path, "rb") as firmware_file:
        while True:
            chunk = firmware_file.read(256)

            if not chunk:
                break

            ser.write(list(chunk))

            if wait_for_ack(ser):
                print(f"\rWrite OK {iterator}", end="", flush=True)
                
                iterator += 1
                
            else:
                print("Write App failed")
                break

    print("")

    print("Verifying flash...")

    ser.write([VERIFY_FLASH_CMD])

    if wait_for_ack(ser):
        print("Verify Flash OK")
    else:
        print("Verify Flash failed")

    print("Loading program memory...")

    ser.write([LOAD_MEM_CMD])

    if wait_for_ack(ser):
        print("Program memory OK")

    ser.write([VERIFY_MEM_CMD])

    if wait_for_ack(ser):
        print("Verify Memory OK")

    ser.write([LAUNCH_CMD])

    ser.close()
    
    print("Loading complete.")


if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument("-f", "--file", type=Path, required=True, help="Path to bin file",)

    parser.add_argument("-p", "--port", type=str, required=True, help="COM port name",)

    args = parser.parse_args()

    bin_path = args.file.resolve()
    serial_port = args.port

    load_via_bl_serial(bin_path, serial_port)

