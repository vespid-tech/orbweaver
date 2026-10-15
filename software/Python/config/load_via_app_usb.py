import serial
import argparse
import sys
from pathlib import Path
from ctypes import *
import os
import math
import time
import zlib
from functools import wraps
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from shared.ow_protocol import *
from shared.cmd_proc import BROADCAST_ADDR
from shared.cmd_proc import system_cmd_header_t, system_cmd_t
from shared.cmd_proc import START_BYTE
from shared.cmd_proc import STOP_BYTE
from shared.serial import SERIAL_BAUD, serial_recv_msg

FLASH_METADATA_ADDR = 0xFF00
FLASH_APP_START_ADDR = 0x10000

FRAG_SIZE = 48

def close_serial_port(function):
    @wraps(function)
    def wrapper(our_addr, radio_addr, bin_path, port_name):
        bin_path = Path(bin_path)
        if not bin_path.is_file():
            print(f"Error: File not found {bin_path}")
            sys.exit(1)

        port = serial.Serial(
            port=port_name,
            baudrate=SERIAL_BAUD,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=2,
            rtscts=True,
        )
        try:
            return function(our_addr, radio_addr, bin_path, port)
        finally:
            if port.is_open:
                port.close()

    return wrapper


def byte_value(value):
    value = int(value, 0)
    if not 0 <= value <= 0xFF:
        raise argparse.ArgumentTypeError("must be between 0 and 255")
    return value


def send_cmd_serial(serial_port: serial.Serial, our_addr: c_uint8, radio_addr: c_uint8, sys_cmd: system_cmd_t, param: bytes):

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

    serial_port.write(send_msg)


def wait_for_ack(serial_port):

    header, _ = serial_recv_msg(serial_port)
    
    if header is not None and header.command == system_cmd_t.ack_cmd:
        return True

    else:
        return False
    
    
def flash_erase(serial_port, flash_erase_param: flash_erase_param_t, our_addr, radio_addr):

        for i in range(3):

            send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.flash_erase_cmd, bytes(flash_erase_param))

            if True == wait_for_ack(serial_port):
                return True

            elif i >= 2:
                exit_with_error(serial_port, our_addr, "Flash erase failed.")

def is_radio_local(serial_port, our_addr, radio_addr):

    for i in range(3):

        send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.status_request_cmd, None)

        header, parameter = serial_recv_msg(serial_port)

        if header is not None and header.command == system_cmd_t.status_cmd:

            status_param = status_param_t.from_buffer_copy(parameter)

            if our_addr in status_param.uart_peers:
                return True
            else:
                return False

        elif i < 2:
            continue

        else:
            exit_with_error(serial_port, our_addr, "Unable to communicate with radio.")


def exit_with_error(serial_port, our_addr, message):
    print(f"Error: {message}")
    send_cmd_serial(serial_port, our_addr, BROADCAST_ADDR, system_cmd_t.uart_peers_cmd, None)
    sys.exit(1)
    
# def recv_cmd_serial():

#     global serial_port
#     parameter = b""

#     start_byte = serial_port.read(1)

#     if start_byte == bytes([START_BYTE]):
        
#         header = serial_port.read(sizeof(system_cmd_header_t))

#         if len(header) == sizeof(system_cmd_header_t):
        
#             recv_header = system_cmd_header_t.from_buffer_copy(header)

#             msg_len = recv_header.length
                            
#             if msg_len >= sizeof(system_cmd_header_t):

#                 parameter_len = msg_len - sizeof(system_cmd_header_t)

#                 if parameter_len > 0:

#                     parameter = serial_port.read(parameter_len)

#                 stop_byte = serial_port.read(1)
                    
#                 if STOP_BYTE == stop_byte[0]:

#                     return recv_header, parameter
                
#     return None, None

    
@close_serial_port
def load_via_serial(our_addr, radio_addr, bin_path, serial_port):

    serial_port.reset_input_buffer()
    serial_port.reset_output_buffer()

    send_cmd_serial(serial_port, our_addr, BROADCAST_ADDR, system_cmd_t.uart_peers_cmd, bytes([our_addr]))

    radio_is_local = True

    if radio_addr == 255:

        print("Updating local radio.")

    else:

        radio_is_local = is_radio_local(serial_port, our_addr, radio_addr)

        if radio_is_local:
            print("Updating local radio.")
        else:
            radio_is_local = False
            print("Updating remote radio.")


    for i in range(3):

        send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.firmware_ver_get_cmd, None)

        header, param = serial_recv_msg(serial_port)

        if header is not None and header.command == system_cmd_t.firmware_ver_cmd:
            break
                
        elif i < 2:
            continue
    
        else:
            exit_with_error(serial_port, our_addr, "Unable to get firmware version.")

    received_version = param.split(b"\x00", 1)[0].decode("ascii")

    print(f"Current firmware version: {received_version}")

    flash_erase_param = flash_erase_param_t()

    flash_erase_param.address = FLASH_METADATA_ADDR
    flash_erase_param.flash_erase_type = flash_erase_type_t.page_fet

    print("Erasing metadata...")

    flash_erase(serial_port, flash_erase_param, our_addr, radio_addr)

    for i in range(1, 16):

        time.sleep(0.01)

        erase_addr = i * FLASH_APP_START_ADDR
        flash_erase_param.address = erase_addr
        flash_erase_param.flash_erase_type = flash_erase_type_t.block_fet

        if i < 15:
            print(f"\rErasing block {i}", end="", flush=True)
        else:
            print(f"\rErasing block {i}")

        flash_erase(serial_port, flash_erase_param, our_addr, radio_addr)

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

            send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.flash_write_cmd, bytes(flash_write_param) + fragment)

            if True == wait_for_ack(serial_port):
                break
    
            elif j >= 2:
                exit_with_error(serial_port, our_addr, f"Write fragment {i} failed.")


    write_metadata = app_metadata_param_t()

    write_metadata.app_len = file_size
    write_metadata.app_crc = crc32
    write_metadata.boot_cmd = boot_command_t.load_app_bc
    metadata_bytes = bytes(write_metadata)

    print("Writing metadata...")

    flash_write_param = flash_write_param_t()
    flash_write_param.address = FLASH_METADATA_ADDR
    flash_write_param.length = len(metadata_bytes)

    time.sleep(0.01)

    for i in range(3):

        send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.flash_write_cmd, bytes(flash_write_param) + metadata_bytes)

        if True == wait_for_ack(serial_port):
            break
        
        elif i >= 2:
            exit_with_error(serial_port, our_addr, "Write metadata failed.")

    flash_read_param = flash_read_param_t()
    flash_read_param.address = FLASH_METADATA_ADDR
    flash_read_param.length = len(metadata_bytes)

    time.sleep(0.01)

    for i in range(3):

        send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.flash_read_cmd, bytes(flash_read_param))

        header, param = serial_recv_msg(serial_port)

        if header is not None and header.command == system_cmd_t.flash_data_cmd:
            break
                        
        elif i < 2:
            continue
    
        else:
            exit_with_error(serial_port, our_addr, "Flash read metadata failed.")

    received_metadata = app_metadata_param_t.from_buffer_copy(param)

    if bytes(received_metadata) != metadata_bytes:
        exit_with_error(serial_port, our_addr, "Read metadata does not match written metadata.")

    print("Metadata verified.")

    with open(bin_path, "rb") as expected_file:
        for i in range(frag_count):

            expected_fragment = expected_file.read(FRAG_SIZE)

            flash_read_param = flash_read_param_t()
            flash_read_param.address = FLASH_APP_START_ADDR + (FRAG_SIZE * i)
            flash_read_param.length = len(expected_fragment)

            if i >= 0:
            
                for j in range(3):

                    send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.flash_read_cmd, bytes(flash_read_param))

                    recv_header, received_fragment = serial_recv_msg(serial_port)

                    if recv_header is not None and (recv_header.command == system_cmd_t.flash_data_cmd) and (received_fragment == expected_fragment):
                        break;

                    elif j < 2:
                        continue

                    else:
                       exit_with_error(serial_port, our_addr, f"Read fragment {i} does not match expected data.")

            print(f"\rVerified fragment {i}", end="", flush=True)

    print("\nApplication flash verified.")

    print("Resetting radio...")

    if radio_is_local:

        send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.reset_radio_cmd, None)

        serial_port.timeout = 10
        header, param = serial_recv_msg(serial_port)

        if header is not None and system_cmd_t.uart_peers_request_cmd == header.command:

            serial_port.timeout = 3
            send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.uart_peers_cmd, bytes([our_addr]))

        else:
            exit_with_error(serial_port, our_addr, "Reset radio failed.")

    else:

        for i in range(3):

            send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.reset_radio_cmd, None)

            time.sleep(10)

            send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.echo_cmd, None)

            header, param = serial_recv_msg(serial_port)

            if header is not None and system_cmd_t.echo_response_cmd == header.command:
                break;
            
            elif i < 2:
                continue

            else:
                exit_with_error(serial_port, our_addr, "Reset radio failed.")
                

    for i in range(3):

        send_cmd_serial(serial_port, our_addr, radio_addr, system_cmd_t.firmware_ver_get_cmd, None)

        header, param = serial_recv_msg(serial_port)

        if header is not None and system_cmd_t.firmware_ver_cmd == header.command:
            break

        elif i < 2:
            continue

        else:
           exit_with_error(serial_port, our_addr, "Unable to get firmware version.")

    received_version = param.split(b"\x00", 1)[0].decode("ascii")
    expected_version = bin_path.stem.rsplit("_", 1)[-1].split(".", 1)[0]

    if received_version != expected_version:
        exit_with_error(serial_port, our_addr, f"Firmware version mismatch. Expected {expected_version}, received {received_version}.")

    send_cmd_serial(serial_port, our_addr, BROADCAST_ADDR, system_cmd_t.uart_peers_cmd, None)

    print(f"Firmware version successfully loaded: {received_version}")
    print("Loading complete.")




if __name__ == "__main__":

    # parser = argparse.ArgumentParser()

    # parser.add_argument("-r", "--recv_address", type=byte_value, required=True, help="The address this script should use to receive messages (do not use an address that is currently in use)",)

    # parser.add_argument("-a", "--radio_address", type=byte_value, required=True, help="The address of the radio (use 255 if the radio is unconfigured)",)

    # parser.add_argument("-f", "--file", type=Path, required=True, help="Path to bin file",)

    # parser.add_argument("-p", "--port", type=str, required=True, help="COM port the radio is connected to",)

    # args = parser.parse_args()

    # our_addr = args.recv_address
    # radio_addr = args.radio_address
    # bin_path = args.file
    # serial_port = args.port

    # if not 1 <= radio_addr <= 0xFF:

    #     print("Error: Radio address invalid.")
    #     sys.exit(1)  

    our_addr = 250

    radio_addr = 255

    bin_path = Path(r"C:\\Projects\\bridge_0\\public\\firmware\\orbweaver\\Debug\\orbweaver_d810f2a-dirty.bin")

    # serial_port = "/dev/ttyACM0"
    serial_port = "COM19"

    load_via_serial(our_addr, radio_addr, bin_path, serial_port)
