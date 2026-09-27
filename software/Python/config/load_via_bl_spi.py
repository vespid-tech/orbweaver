from pathlib import Path
import sys
import zlib
import struct
import spidev
import RPi.GPIO as GPIO
import argparse

# All the orbweaver bootloader commands
PING_CMD            = 0x11
ERASE_FLASH_CMD     = 0x22
ERASE_FLASH_ALL_CMD = 0x23
WRITE_METADATA_CMD  = 0x33
WRITE_APP_CMD       = 0x44
VERIFY_FLASH_CMD    = 0x55
LOAD_MEM_CMD        = 0x66
VERIFY_MEM_CMD      = 0x77
LAUNCH_CMD          = 0x88
ERASE_MEM_CMD       = 0x99
ACK_CMD             = 0xAA
NAK_CMD             = 0xBB

DATA_READY_PIN = 27

ACTIVATE_BOOTLOADER_CMD = 1
LOAD_APP_CMD = 2
LAUNCH_APP_BOOT_CMD = 3


def wait_for_ack(spi):
    
    while GPIO.input(DATA_READY_PIN) == GPIO.HIGH:
        pass

    ack = bytes(spi.readbytes(1))

    if ack[0] == ACK_CMD:
         return True
    else:
        return False

def erase_mem_bl_spi():

    spi = spidev.SpiDev()

    spi.open(0, 0)

    spi.max_speed_hz = 50000    # Run the SPI bus at 50 kHz. The bootloader polls the SPI bus, which is slow, so don't run it faster.
    spi.mode = 1                # Sets the SPI polarity and phase CPOL=0, CPHA=1

    # Configure the data ready pin. The radio pulls this pin low when it has data ready to send.
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    spi.writebytes([PING_CMD])

    if wait_for_ack(spi):
        print("Ping OK")

    spi.writebytes([ERASE_MEM_CMD])

    if wait_for_ack(spi):
        print("Erase program memory OK")

    spi.close()
    GPIO.cleanup([DATA_READY_PIN])


def erase_flash_all_bl_spi():

    spi = spidev.SpiDev()

    spi.open(0, 0)

    spi.max_speed_hz = 50000    # Run the SPI bus at 50 kHz. The bootloader polls the SPI bus, which is slow, so don't run it faster.
    spi.mode = 1                # Sets the SPI polarity and phase CPOL=0, CPHA=1

    # Configure the data ready pin. The radio pulls this pin low when it has data ready to send.
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    spi.writebytes([PING_CMD])

    if wait_for_ack(spi):
        print("Ping OK")

    spi.writebytes([ERASE_FLASH_ALL_CMD])

    if wait_for_ack(spi):
        print("Erase program memory OK")

    spi.close()
    GPIO.cleanup([DATA_READY_PIN])


def ping_bl_spi():

    spi = spidev.SpiDev()

    spi.open(0, 0)

    spi.max_speed_hz = 50000    # Run the SPI bus at 50 kHz. The bootloader polls the SPI bus, which is slow, so don't run it faster.
    spi.mode = 1                # Sets the SPI polarity and phase CPOL=0, CPHA=1

    # Configure the data ready pin. The radio pulls this pin low when it has data ready to send.
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    spi.writebytes([PING_CMD])

    if wait_for_ack(spi):
        print("Ping OK")
    else:
        print("Error: bootloader ping not received")

    spi.close()
    GPIO.cleanup([DATA_READY_PIN])


def load_via_spi(bin_path):

    if not bin_path.file.is_file():
        print(f"Error: File not found {bin_path.file}")
        sys.exit(1)  


    file_path = bin_path.file.resolve()

    spi = spidev.SpiDev()

    spi.open(0, 0)

    spi.max_speed_hz = 50000    # Run the SPI bus at 50 kHz. The bootloader polls the SPI bus, which is slow, so don't run it faster.
    spi.mode = 1                # Sets the SPI polarity and phase CPOL=0, CPHA=1

    # Configure the data ready pin. The radio pulls this pin low when it has data ready to send.
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    spi.writebytes([PING_CMD])

    if wait_for_ack(spi):
        print("Ping OK")


    spi.writebytes([ERASE_FLASH_CMD])

    if wait_for_ack(spi):
        print("Erase flash OK")

    file_size = Path(file_path).stat().st_size

    print(f"File size: {file_size} bytes")

    crc = 0

    with open(file_path, "rb") as f:
            while chunk := f.read(65536):
                crc = zlib.crc32(chunk, crc)

    crc32 = crc & 0xFFFFFFFF

    print(f"CRC32: {crc32:08x}")

    # If you don't want the bootloader to launch the app after the reset, then change is command.
    metadata = struct.pack('<III', file_size, crc32, LAUNCH_APP_BOOT_CMD)

    # The metadata must be written before the app, so the bootloader know how big the app is.
    spi.writebytes([WRITE_METADATA_CMD])
    spi.writebytes(metadata)

    if wait_for_ack(spi):
        print("Write Metadata OK")

    spi.writebytes([WRITE_APP_CMD])

    iterator = 0

    with open(file_path, "rb") as firmware_file:
        while True:
            chunk = firmware_file.read(256)

            if not chunk:
                break

            spi.writebytes(list(chunk))

            if wait_for_ack(spi):
                print(f"\rWrite OK {iterator}", end="", flush=True)
                
                iterator += 1
                
            else:
                print("Write App failed")
                break


    print("")

    spi.writebytes([VERIFY_FLASH_CMD])

    if wait_for_ack(spi):
        print("Verify Flash OK")
    else:
        print("Verify Flash failed")

    spi.writebytes([LOAD_MEM_CMD])

    if wait_for_ack(spi):
        print("Program OK")

    spi.writebytes([VERIFY_MEM_CMD])

    if wait_for_ack(spi):
        print("Verify Memory OK")

    spi.writebytes([LAUNCH_CMD])

    print("Loading complete.")



if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument("-f", "--file", type=Path, required=True, help="Path to bin file",)

    args = parser.parse_args()

    load_via_spi(args)

