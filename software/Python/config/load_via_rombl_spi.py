import argparse
import time
import spidev
import gpiod
from pathlib import Path
from gpiod.line import Direction, Value


# --- CONFIGURATION ---
SPI_BUS = 0
SPI_DEVICE = 0
SPI_SPEED = 500000  # 500 kHz

# Adjust these to match your Raspberry Pi Hat's wiring (BCM numbers)
RESET_PIN = 17 
BL_PIN = 22    # Bootloader enable pin

# CC13xx Bootloader Commands
CMD_PING = 0x20
CMD_DOWNLOAD = 0x21
CMD_BANK_ERASE = 0x2C
CMD_SEND_DATA = 0x24
CMD_RESET = 0x25
RET_COMMAND_OK = 0x40
RET_SUCCESS = 0x4C

def hardware_reset_to_bootloader():
    """Toggles Reset and BL pins using early gpiod v1.x individual line lookups."""
    
    chip = gpiod.Chip("/dev/gpiochip4")
    
    # 2. Configure line settings
    line_settings = gpiod.LineSettings(
        direction=Direction.OUTPUT, 
        output_value=Value.INACTIVE
    )
    
    # 3. Call request_lines directly ON the chip object
    with chip.request_lines(
        consumer="cc1310-bootloader",
        config={
            RESET_PIN: line_settings,
            BL_PIN: line_settings,
        },
    ) as lines:
        
        # Drive both BL and RESET low
        lines.set_values({RESET_PIN: Value.INACTIVE, BL_PIN: Value.INACTIVE})
        time.sleep(0.2)
        
        # Release RESET (high) while keeping BL low
        lines.set_values({RESET_PIN: Value.ACTIVE, BL_PIN: Value.INACTIVE})
        time.sleep(0.2)
        
        # Release BL (high)
        lines.set_values({BL_PIN: Value.ACTIVE})
        time.sleep(0.2)
    
    # Clean up the chip handle
    chip.close()








def build_packet(cmd, data=None):
    """Builds a TI bootloader SPI packet with the expected length and checksum."""
    payload = list(data or [])
    size = len(payload) + 3
    checksum = (cmd + sum(payload)) & 0xFF
    return [size, checksum, cmd] + payload


def calculate_checksum(cmd, data):
    """Calculates the TI bootloader checksum."""
    return build_packet(cmd, data)[1]


def send_packet(spi, cmd, data=None):
    """Frames and sends a packet over SPI, then waits for an ACK byte."""
    packet = build_packet(cmd, data)
    spi.xfer2(packet)

    # Give the chip a tiny micro-pause to process the packet before polling.
    time.sleep(0.001)

    max_attempts = 50
    response_byte = 0x00

    for _ in range(max_attempts):
        rx = spi.xfer2([0x00])
        response_byte = rx[0]

        if response_byte != 0x00:
            break

        time.sleep(0.0005)

    return response_byte


def ping_device(spi):
    """Pings the device to see if the bootloader is responding."""
    for _ in range(3):
        if send_packet(spi, CMD_PING) == 0xCC:
            return True
        time.sleep(0.1)
    return False

def erase_device(spi):
    """Issues the flash erase command to the CC13xx bootloader."""
    for _ in range(3):
        if send_packet(spi, CMD_BANK_ERASE) == 0xCC:
            return True
        time.sleep(0.1)
    return False

def flash_firmware(spi, firmware_bytes):
    """Sends the firmware data to the CC1310."""
    # 1. Send Download Command (Target Address + Size)
    # Example targets flash start: 0x00000000
    start_address = [0x00, 0x00, 0x00, 0x00] 
    total_size = len(firmware_bytes)
    size_bytes = list(total_size.to_bytes(4, byteorder='big'))
    
    if send_packet(spi, CMD_DOWNLOAD, start_address + size_bytes) != 0xCC:
        raise RuntimeError("CC1310 rejected download command.")

    print("Sending firmware via ROM BL...")    

    # 2. Send Data in chunks (Max payload size for TI bootloader is usually 252 bytes)
    chunk_size = 252
    for i in range(0, total_size, chunk_size):
        chunk = list(firmware_bytes[i:i+chunk_size])
        print(f"Writing bytes {i}", end="\r", flush=True)
        if send_packet(spi, CMD_SEND_DATA, chunk) != 0xCC:
            raise RuntimeError(f"Failed to send chunk starting at byte {i}")

    print("")

def load_rombl_spi(bin_path):
    # Read your compiled .bin file
    try:

        with open(bin_path, "rb") as f:
            firmware_bytes = f.read()
    except FileNotFoundError:
        print("Error: bridge_0.bin not found.")
        return

    # Hardware reset sequence
    hardware_reset_to_bootloader()

    # Initialize SPI
    spi = spidev.SpiDev()
    spi.open(SPI_BUS, SPI_DEVICE)
    spi.max_speed_hz = SPI_SPEED
    # spi.mode = 0b00  # Mode 0 (CPOL=0, CPHA=0)
    spi.mode = 3  # Mode 3 (CPOL=1, CPHA=1)

    try:
        for i in range(3):

            if ping_device(spi):

                break;
            
            elif i >= 2:
                print("Bootloader ping failed.")
                spi.close()

                return

        if erase_device(spi):

            flash_firmware(spi, firmware_bytes)

            # Reset device into execution mode
            send_packet(spi, CMD_RESET)

            print("ROM bootloading complete.")

        else:
            print("Failed to connect to CC1310 bootloader.")

    finally:
        spi.close()

if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument("-f", "--file", type=Path, required=True, help="Path to bin file",)
    
    args = parser.parse_args()
    
    bin_path = args.file.resolve()

    # bin_path = "./orbweaver_bl.bin"

    load_rombl_spi(bin_path)