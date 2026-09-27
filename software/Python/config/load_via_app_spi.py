import argparse
from email import header
import sys
from pathlib import Path
from ctypes import *
from enum import IntEnum, auto
import spidev
import os
import math
import time
import zlib
import RPi.GPIO as GPIO

from shared.ow_protocol import *
from shared.cmd_proc import BROADCAST_ADDR, system_cmd_header_t, system_cmd_t, START_BYTE, STOP_BYTE
from shared.spi import DATA_READY_PIN, RADIO_BUSY_PIN, spi_recv_msg


FLASH_METADATA_ADDR = 0xFF00
FLASH_APP_START_ADDR = 0x10000

FRAG_SIZE = 200

def byte_value(value):
    value = int(value, 0)
    if not 0 <= value <= 0xFF:
        raise argparse.ArgumentTypeError("must be between 0 and 255")
    return value


def wait_for_radio(timeout=1.0):
    start_time = time.monotonic()

    while GPIO.input(RADIO_BUSY_PIN) != GPIO.HIGH:
        if time.monotonic() - start_time >= timeout:
            return False
        time.sleep(0.001)

    return True

def wait_for_data_ready(timeout=1.0):
    start_time = time.monotonic()

    while GPIO.input(DATA_READY_PIN) == GPIO.HIGH:
        if time.monotonic() - start_time >= timeout:
            return False
        time.sleep(0.001)

    return True

def send_cmd_spi(spi_dev: spidev.SpiDev, our_addr: c_uint8, radio_addr: c_uint8, sys_cmd: system_cmd_t, param: bytes):

    sys_cmd_header = system_cmd_header_t()
    sys_cmd_header.aad.sequence = 0
    sys_cmd_header.aad.addr_src = our_addr
    sys_cmd_header.aad.addr_dest = radio_addr
    sys_cmd_header.ttl = 0

    if param is not None:
        sys_cmd_header.length = sizeof(system_cmd_header_t) + len(param)
    else:
        sys_cmd_header.length = sizeof(system_cmd_header_t)
    sys_cmd_header.command = sys_cmd

    if None == param:

        send_msg = bytes([START_BYTE]) + bytes(sys_cmd_header) + bytes([STOP_BYTE])

    else:

        send_msg = bytes([START_BYTE]) + bytes(sys_cmd_header) + param + bytes([STOP_BYTE])

    wait_for_radio()

    spi_dev.writebytes(send_msg)
    


def flash_erase(spi_dev: spidev.SpiDev, our_addr: c_uint8, radio_addr: c_uint8, flash_erase_param: flash_erase_param_t):

    for i in range(3):

        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.flash_erase_cmd, bytes(flash_erase_param))

        wait_for_data_ready()

        header, _ = spi_recv_msg(spi_dev)

        if header is not None and header.command == system_cmd_t.ack_cmd:
            break
                
        elif i < 2:
            continue

        else:
            exit_with_error(spi_dev, our_addr, "Flash Erase failed.")


def is_radio_local(spi_dev: spidev.SpiDev, our_addr: c_uint8, radio_addr: c_uint8):

    for i in range(3):

        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.status_request_cmd, None)

        wait_for_data_ready()
        
        header, param = spi_recv_msg(spi_dev)

        if header is not None and header.command == system_cmd_t.status_cmd:

            status_param = status_param_t.from_buffer_copy(param)

            if our_addr in status_param.spi_peers:
                return True
            else:
                return False

        elif i < 2:
            continue

        else:
            exit_with_error(spi_dev, our_addr, "Unable to communicate with radio.")


def exit_with_error(spi_dev: spidev.SpiDev, our_addr: c_uint8, message):
    print(f"Error: {message}")
    send_cmd_spi(spi_dev, our_addr, BROADCAST_ADDR, system_cmd_t.spi_peers_cmd, None)
    sys.exit(1)

def load_via_spi(our_addr: c_uint8, radio_addr: c_uint8, bin_path: Path):

    global spi

    if not bin_path.is_file():
        print(f"Error: File not found {bin_path}")
        sys.exit(1)  

    spi_dev = spidev.SpiDev()

    spi_dev.open(0, 0)

    spi_dev.max_speed_hz = 1000000  # Run the SPI bus at 1 MHz.
    spi_dev.mode = 1                # Sets the SPI polarity and phase CPOL=0, CPHA=1

    # Configure the data ready pin. The radio pulls this pin low when it has data ready to send.
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    # Configure the radio busy pin. The radio pulls this pin low when it is busy.
    GPIO.setup(RADIO_BUSY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    send_cmd_spi(spi_dev, our_addr, BROADCAST_ADDR, system_cmd_t.spi_peers_cmd, bytes([our_addr]))


    radio_is_local = True

    if radio_addr == 255:

        print("Updating local radio.")

    else:

        radio_is_local = is_radio_local(spi_dev, our_addr, radio_addr)

        if radio_is_local:
            print("Updating local radio.")
        else:
            radio_is_local = False
            print("Updating remote radio.")

    for i in range(3):
        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.firmware_ver_get_cmd, None)

        wait_for_data_ready()

        header, param = spi_recv_msg(spi_dev)

        if header is not None and header.command == system_cmd_t.firmware_ver_cmd:
            break
                
        elif i < 2:
            continue

        else:
            exit_with_error(spi_dev, our_addr, "Unable to communicate with radio.")

    received_version = param.split(b"\x00", 1)[0].decode("ascii")

    print(f"Current firmware version: {received_version}")

    flash_erase_param = flash_erase_param_t()

    flash_erase_param.address = FLASH_METADATA_ADDR
    flash_erase_param.flash_erase_type = flash_erase_type_t.page_fet

    print("Erasing metadata...")

    flash_erase(spi_dev, our_addr, radio_addr, flash_erase_param)

    for i in range(1, 16):

        erase_addr = i * FLASH_APP_START_ADDR
        flash_erase_param.address = erase_addr
        flash_erase_param.flash_erase_type = flash_erase_type_t.block_fet

        if i < 15:
            print(f"\rErasing block {i}", end="", flush=True)
        else:
            print(f"\rErasing block {i}")

        flash_erase(spi_dev, our_addr, radio_addr, flash_erase_param)

    crc = 0

    with open(bin_path, "rb") as f:
        while chunk := f.read(65536):
            crc = zlib.crc32(chunk, crc)

    crc32 = crc & 0xFFFFFFFF

    print(f"New CRC32: 0x{crc32:08X}")    

    file_size = os.path.getsize(bin_path)
    frag_count = math.ceil(file_size / FRAG_SIZE)

    file = open(bin_path, 'rb')

    for i in range(frag_count):

        fragment = file.read(FRAG_SIZE)

        if i < frag_count - 1:
            print(f"\rWriting fragment {i}", end="", flush=True)
        else:
            print(f"\rWriting fragment {i}")

        flash_write_param = flash_write_param_t()
        flash_write_param.address = FLASH_APP_START_ADDR + (FRAG_SIZE * i)
        flash_write_param.length = len(fragment)

        for j in range(3):
            send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.flash_write_cmd, bytes(flash_write_param) + fragment)

            wait_for_data_ready()

            header, _ = spi_recv_msg(spi_dev)

            if header is not None and header.command == system_cmd_t.ack_cmd:
                break
                    
            elif j < 2:
                continue

            else:
                exit_with_error(f"Write fragment {i} failed.")


    write_metadata = app_metadata_param_t()

    write_metadata.app_len = file_size
    write_metadata.app_crc = crc32
    write_metadata.boot_cmd = boot_command_t.load_app_bc
    metadata_bytes = bytes(write_metadata)

    print("Writing metadata...")

    flash_write_param = flash_write_param_t()
    flash_write_param.address = FLASH_METADATA_ADDR
    flash_write_param.length = len(metadata_bytes)

    for i in range(3):
        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.flash_write_cmd, bytes(flash_write_param) + metadata_bytes)

        wait_for_data_ready()

        header, param = spi_recv_msg(spi_dev)

        if header is not None and header.command == system_cmd_t.ack_cmd:
            break
                
        elif i < 2:
            continue

        else:
            exit_with_error(spi_dev, our_addr, "Write metadata failed.")

    flash_read_param = flash_read_param_t()
    flash_read_param.address = FLASH_METADATA_ADDR
    flash_read_param.length = len(metadata_bytes)

    for i in range(3):
        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.flash_read_cmd, bytes(flash_read_param))

        wait_for_data_ready()

        header, param = spi_recv_msg(spi_dev)

        if header is not None and header.command == system_cmd_t.flash_data_cmd:
            break
                
        elif i < 2:
            continue

        else:
            exit_with_error(spi_dev, our_addr, "Metadata read failed..")

    received_metadata = app_metadata_param_t.from_buffer_copy(param)

    if bytes(received_metadata) != metadata_bytes:
        exit_with_error(spi_dev, our_addr, "Read metadata does not match written metadata.")
        
    print("Metadata verified.")

    with open(bin_path, "rb") as expected_file:
        for i in range(frag_count):
            expected_fragment = expected_file.read(FRAG_SIZE)

            flash_read_param = flash_read_param_t()
            flash_read_param.address = FLASH_APP_START_ADDR + (FRAG_SIZE * i)
            flash_read_param.length = len(expected_fragment)

            for j in range(3):
                send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.flash_read_cmd, bytes(flash_read_param))

                wait_for_data_ready()

                header, received_fragment = spi_recv_msg(spi_dev)

                if header is not None and header.command == system_cmd_t.flash_data_cmd:

                    if received_fragment != expected_fragment:
                        exit_with_error(f"File data mismatch at fragment {i}.")
                        
                elif j < 2:
                    continue

                else:
                    exit_with_error(f"File data mismatch at fragment {i}.")

            print(f"\rVerified fragment {i}", end="", flush=True)

    print("\nApplication flash verified.")

    print("Resetting radio...")

    if radio_is_local:

        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.reset_radio_cmd, None)

        wait_for_data_ready(10)

        header, param = spi_recv_msg(spi_dev)

        if header is not None and system_cmd_t.spi_peers_request_cmd == header.command:

            send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.spi_peers_cmd, bytes([our_addr]))

        else:

            exit_with_error(spi_dev, our_addr, "Reset radio failed.")

    else:

        for i in range(3):

            send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.reset_radio_cmd, None)

            time.sleep(10)

            send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.echo_cmd, None)

            wait_for_data_ready(10)

            header, _ = spi_recv_msg(spi_dev)

            if header is not None and system_cmd_t.echo_response_cmd == header.command:
                break;
            
            elif i < 2:
                continue

            else:
                exit_with_error(spi_dev, our_addr, "Reset radio failed.")

    for i in range(3):

        send_cmd_spi(spi_dev, our_addr, radio_addr, system_cmd_t.firmware_ver_get_cmd, None)

        wait_for_data_ready()

        header, param = spi_recv_msg(spi_dev)

        if header is not None and system_cmd_t.firmware_ver_cmd == header.command:
            break;
            
        elif i < 2:
            continue

        else:
            exit_with_error(spi_dev, our_addr, "Unable to get firmware version.")

    received_version = param.split(b"\x00", 1)[0].decode("ascii")
    expected_version = bin_path.stem.rsplit("_", 1)[-1].split(".", 1)[0]

    if received_version != expected_version:
        exit_with_error(f"Firmware version mismatch. Expected {expected_version}, received {received_version}.")

    print(f"Firmware version successfully loaded: {received_version}")
    print("Loading complete.")




if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument("-r", "--recv_address", type=byte_value, required=True, help="The address this script should use to receive messages (do not use an address that is currently in use)",)

    parser.add_argument("-a", "--radio_address", type=byte_value, required=True, help="The address of the radio (use 255 if the radio is unconfigured)",)

    parser.add_argument("-f", "--file", type=Path, required=True, help="Path to bin file",)

    args = parser.parse_args()

    our_addr = args.recv_address
    radio_addr = args.radio_address
    bin_path = args.file.resolve()

    if not 1 <= radio_addr <= 0xFF:

        print("Error: Radio address invalid.")
        sys.exit(1)  

    # our_addr = 254

    # radio_addr = 13

    # bin_path = Path(r"/home/jo-jo/projects/bridge_0/software/Python/raspberry_pi/orbweaver_ac3793c-dirty.bin")

    load_via_spi(our_addr, radio_addr, bin_path)
