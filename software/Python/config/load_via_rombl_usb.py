import argparse
import time
import serial
from pathlib import Path


SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 2
UART_SYNC = bytes([0x55, 0x55])
UART_ACK = bytes([0x00, 0xCC])

# CC13xx Bootloader Commands
CMD_PING = 0x20
CMD_DOWNLOAD = 0x21
CMD_BANK_ERASE = 0x2C
CMD_SEND_DATA = 0x24
CMD_RESET = 0x25


def build_packet(cmd, data=None):
    """Build a TI ROM bootloader command packet."""
    payload = list(data or [])
    size = len(payload) + 3
    checksum = (cmd + sum(payload)) & 0xFF
    return [size, checksum, cmd] + payload


def sync_device(serial_port):
    """Synchronize UART baud detection with the ROM bootloader."""
    for _ in range(3):
        serial_port.reset_input_buffer()
        serial_port.write(UART_SYNC)
        response = serial_port.read(len(UART_ACK))
        if response == UART_ACK:
            return True
        time.sleep(0.1)
    return False


def send_packet(serial_port, cmd, data=None, timeout=SERIAL_TIMEOUT):
    """Send a ROM bootloader packet over UART and return its response byte."""
    previous_timeout = serial_port.timeout
    serial_port.timeout = timeout
    try:
        serial_port.write(bytes(build_packet(cmd, data)))
        return serial_port.read(len(UART_ACK))
    finally:
        serial_port.timeout = previous_timeout


def ping_device(serial_port):
    """Pings the device to see if the bootloader is responding."""
    for _ in range(3):
        if send_packet(serial_port, CMD_PING) == UART_ACK:
            return True
        time.sleep(0.1)
    return False

def erase_device(serial_port):
    """Issues the flash erase command to the CC13xx bootloader."""
    for _ in range(3):
        if send_packet(serial_port, CMD_BANK_ERASE, timeout=30) == UART_ACK:
            return True
        time.sleep(0.1)
    return False

def flash_firmware(serial_port, firmware_bytes):
    """Sends the firmware data to the CC1310."""
    # 1. Send Download Command (Target Address + Size)
    # Example targets flash start: 0x00000000
    start_address = [0x00, 0x00, 0x00, 0x00] 
    total_size = len(firmware_bytes)
    size_bytes = list(total_size.to_bytes(4, byteorder='big'))
    
    if send_packet(serial_port, CMD_DOWNLOAD, start_address + size_bytes) != UART_ACK:
        raise RuntimeError("CC1310 rejected download command.")

    print("Sending firmware via ROM BL...")    

    # 2. Send Data in chunks (Max payload size for TI bootloader is usually 252 bytes)
    chunk_size = 252
    for i in range(0, total_size, chunk_size):
        chunk = list(firmware_bytes[i:i+chunk_size])
        print(f"Writing bytes {i}", end="\r", flush=True)
        if send_packet(serial_port, CMD_SEND_DATA, chunk) != UART_ACK:
            raise RuntimeError(f"Failed to send chunk starting at byte {i}")

    print("")

def load_rombl_serial(bin_path, serial_port):
    try:
        firmware_bytes = Path(bin_path).read_bytes()
    except FileNotFoundError:
        print(f"Error: firmware file not found: {bin_path}")
        return

    port = serial.Serial(
        port=serial_port,
        baudrate=SERIAL_BAUD,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=SERIAL_TIMEOUT,
    )

    try:
        if not sync_device(port):
            print("UART bootloader synchronization failed.")
            return

        if not ping_device(port):
            print("Bootloader ping failed.")
            return

        if not erase_device(port):
            print("Failed to erase CC1310 flash.")
            return

        flash_firmware(port, firmware_bytes)

        send_packet(port, CMD_RESET)
        print("ROM bootloading complete.")

    finally:
        port.close()

if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument("-f", "--file", type=Path, required=True, help="Path to bin file",)
    parser.add_argument("-p", "--port", required=True, help="Windows COM port name (for example COM3)")
    
    args = parser.parse_args()
    
    bin_path = args.file.resolve()

    load_rombl_serial(bin_path, args.port)