import time
from ctypes import *
from email import header
from unicodedata import numeric
import serial
import sys
from enum import IntEnum, auto
import secrets

from load_via_app_uart import send_cmd_uart
from load_via_app_uart import send_cmd_uart
from uart import UART_BAUD
from cmd_proc import system_cmd_t
from cmd_proc import system_cmd_header_t
from cmd_proc import START_BYTE
from cmd_proc import STOP_BYTE

class bitrate_t(IntEnum):
    null_br = 0
    bps_625_br = auto()
    kbps_1p2_br = auto()
    kbps_2p4_br = auto()
    kbps_50_br = auto()
    kbps_200_br = auto()
    kbps_300_br = auto()
    kbps_400_br = auto()
    kbps_500_br = auto()

class freq_band_t(IntEnum):
    null_fb = 0
    itu_1_fb = auto()
    itu_2_fb = auto()


def recv_cmd_uart(serial_port):

    parameter = b""

    start_byte = serial_port.read(1)

    if start_byte == bytes([START_BYTE]):
        
        header = serial_port.read(sizeof(system_cmd_header_t))

        if len(header) == sizeof(system_cmd_header_t):

            recv_header = system_cmd_header_t.from_buffer_copy(header)

            msg_len = recv_header.length
                            
            if msg_len >= sizeof(system_cmd_header_t):

                parameter_len = msg_len - sizeof(system_cmd_header_t)

                if parameter_len > 0:

                    parameter = serial_port.read(parameter_len)

                stop_byte = serial_port.read(1)
                    
                if STOP_BYTE == stop_byte[0]:

                    return recv_header, parameter
                
    return None, None

    
def configuration_get(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8):

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)

    serial_port.reset_input_buffer()
    serial_port.reset_output_buffer()

    send_cmd_uart(serial_port, addr_src, 255, system_cmd_t.uart_peers_cmd, bytes([addr_src]))

    for i in range(3):  # Get MAC address

        send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.mac_addr_get_cmd, None)

        header, param = recv_cmd_uart(serial_port)

        if header is not None and header.command == system_cmd_t.mac_addr_cmd:

            mac_address = ":".join(f"{byte:02X}" for byte in param[::-1])
            print("Radio MAC address: " + mac_address)

            break;

        elif i < 2:
            continue  # Retry if not successful

        else:
            print("Failed to get radio MAC address.")


    for i in range(3):  # Get firmware version

        send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.firmware_ver_get_cmd, None)

        header, param = recv_cmd_uart(serial_port)

        if header is not None and header.command == system_cmd_t.firmware_ver_cmd:

            print("Radio firmware version: " + param.rstrip(b"\x00").decode("ascii"))

            break;
        elif i < 2:
            continue  # Retry if not successful

        else:
            print("Failed to get radio firmware version.")

    for i in range(3):  # Get radio bitrate

        send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.bitrate_get_cmd, None)

        header, param = recv_cmd_uart(serial_port)

        if header is not None and header.command == system_cmd_t.bitrate_cmd:
            print("Radio bitrate: ", end="")
            if bitrate_t.bps_625_br == param[0]:
                print("625 bps.")
            elif bitrate_t.kbps_1p2_br == param[0]:
                print("2.5 kbps.")
            elif bitrate_t.kbps_2p4_br == param[0]:
                print("5 kbps.")
            elif bitrate_t.kbps_50_br == param[0]:
                print("50 kbps.")
            elif bitrate_t.kbps_200_br == param[0]:
                print("200 kbps.")
            elif bitrate_t.kbps_300_br == param[0]:
                print("300 kbps.")
            elif bitrate_t.kbps_400_br == param[0]:
                print("400 kbps.")
            elif bitrate_t.kbps_500_br == param[0]:                                        
                print("500 kbps.")

            break;

        elif i < 2:
            continue

        else:
            print("Failed to get radio bitrate.")


    for i in range(3):  # Get radio address

        send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.echo_cmd, None)

        header, param = recv_cmd_uart(serial_port)

        if header is not None and header.command == system_cmd_t.echo_response_cmd:

            print("Radio address: " + str(header.aad.addr_src))

            break

        elif i < 2:
            continue

        else:
            print("Failed to get radio address.")

    for i in range(3): # Get frequency band

        send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.freq_band_get_cmd, None)

        header, param = recv_cmd_uart(serial_port)

        if header is not None and header.command == system_cmd_t.freq_band_cmd:
            frequency_band = {1: "ITU 1", 2: "ITU 2"}.get(param[0], str(param[0]))
            print("Radio frequency band: " + frequency_band)
            print("Default channel: " + str(param[1]))

            break;

        elif i < 2:
            continue

        else:
            print("Failed to get radio frequency band.")

    send_cmd_uart(serial_port, addr_src, 255, system_cmd_t.uart_peers_cmd, None)

    serial_port.close()


def radio_addr_set(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8, new_addr: c_uint8):

    if new_addr <= 0 or new_addr >= 255:
        print("Error: Address out of range. Addresses must be between 1-addr_src.")
        sys.exit(1)  

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)

    send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.assign_addr_cmd, bytes((int(new_addr),)),)

    serial_port.close()

    print("Radio address set: " + str(new_addr))


def bitrate_set(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8,  new_bitrate: bitrate_t):

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)

    send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.set_bitrate_cmd, bytes((int(new_bitrate),)))

    serial_port.close()

    print("Radio bitrate set: ", end="")

    if bitrate_t.bps_625_br == new_bitrate:
        print("625 bps.")
    elif bitrate_t.kbps_1p2_br == new_bitrate:
        print("2.5 kbps.")
    elif bitrate_t.kbps_2p4_br == new_bitrate:
        print("5 kbps.")
    elif bitrate_t.kbps_50_br == new_bitrate:
        print("50 kbps.")
    elif bitrate_t.kbps_200_br == new_bitrate:
        print("200 kbps.")
    elif bitrate_t.kbps_300_br == new_bitrate:
        print("300 kbps.")
    elif bitrate_t.kbps_400_br == new_bitrate:
        print("400 kbps.")
    elif bitrate_t.kbps_500_br == new_bitrate:                                        
        print("500 kbps.")



def encryption_set(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8,  enabled: bool):

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)

    send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.encryption_enable_cmd, bytes((int(enabled),)),)

    serial_port.close()

    if enabled:

        print("Radio encryption enabled.")

    else:

        print("Radio encryption disabled.")


def cipher_key_set(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8,  cipher_key: str):

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)
    
    send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.set_cipher_key_cmd, bytes.fromhex(cipher_key))
    
    serial_port.close()

    print("Encryption key set. " + cipher_key)


def freq_band_set(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8,  freq_band: freq_band_t, default_channel: c_uint8):

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)
    
    send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.set_freq_band_cmd, bytes((int(freq_band), int(default_channel))),)
    
    serial_port.close()

    print("Frequency band set: ", end="")

    if freq_band_t.itu_1_fb == freq_band:
        print("ITU Region 1")
    elif freq_band_t.itu_2_fb == freq_band:
        print("ITU Region 2")

    print("Default channel: " + str(default_channel))

def is_radio_local(serial_port, addr_src: c_uint8, addr_dest: c_uint8):

    for i in range(3):

        send_cmd_uart(serial_port, addr_src, 255, system_cmd_t.echo_cmd, None)

        header, _ = recv_cmd_uart(serial_port)

        if header is not None and header.command == system_cmd_t.echo_response_cmd:

            if header.aad.addr_src == addr_dest:
                return True
            else:
                return False

        elif i < 2:
            continue

        return None
            

def radio_reset(uart_port: str, addr_src: c_uint8, addr_dest: c_uint8):

    serial_port = serial.Serial(port=uart_port, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2, rtscts=True)

    serial_port.reset_input_buffer()
    serial_port.reset_output_buffer()

    send_cmd_uart(serial_port, addr_src, 255, system_cmd_t.uart_peers_cmd, bytes([addr_src]))

    radio_is_local = is_radio_local(serial_port, addr_src, addr_dest)

    if radio_is_local is None:

        print("Failed to reset radio.")
            
        serial_port.close()

        return

    if radio_is_local:

        serial_port.timeout = 10

        for i in range(3):

            send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.reset_radio_cmd, None)

            header, _ = recv_cmd_uart(serial_port)
        
            if header is not None and header.command == system_cmd_t.uart_peers_request_cmd:

                print("Radio reset")

                break

            elif i < 2:
                continue

            else:
                print("Failed to get radio address.")

    else:

        for i in range(3):

            send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.reset_radio_cmd, None)

            time.sleep(1)

            send_cmd_uart(serial_port, addr_src, addr_dest, system_cmd_t.echo_cmd, None)

            header, _ = recv_cmd_uart(serial_port)

            if header is not None and header.command == system_cmd_t.echo_response_cmd:

                print("Radio reset")

                break

            elif i < 2:
                continue

            else:
                print("Failed to reset radio.")
    
    serial_port.close()

    
if __name__ == "__main__":

    global serial_port

    print("Welcome to the Orbweaver Command Line Configurator.")

    uart_port = input("Enter the comm port you would like to use for this session: ")
    addr_src_str = input("Enter the source address you would like to use for this session (press enter to use 254): ")
    addr_dest_str = input("Enter the destination address you would like to use for this session (press enter to use 255): ")


    if "" == addr_src_str:
        addr_src = 254
    else:
        addr_src = int(addr_src_str)
            
    if "" == addr_dest_str:
        addr_dest = 255

    else:
        addr_dest = int(addr_dest_str)


    while True:

        print("")
        print("Enter command number or type \"e\" to exit: ")
        print("1: set radio address")
        print("2: set bitrate")
        print("3: set cipher key")
        print("4: enable/disable encryption")
        print("5: set frequency band")
        print("6: get radio configuration")
        print("7: reset radio")
        print("8: exit configure")


        cmd_num = input("")
        print("")

        match cmd_num:

            case "1":

                new_addr_str = input("Enter address (1-254): ")

                new_addr = int(new_addr_str)

                if new_addr > 0 and new_addr < 255:

                    radio_addr_set(uart_port, addr_src, addr_dest, new_addr)

            case "2":

                print("Please select a bitrate from the following list:")
                print("1: 625 bps")
                print("2: 2.5 kbps")
                print("3: 5   kbps")
                print("4: 50  kbps")
                print("5: 200 kbps")
                print("6: 300 kbps")
                print("7: 400 kbps")
                print("8: 500 kbps")

                selection = input("")

                if "1" == selection:
                    new_bitrate = bitrate_t.bps_625_br
                elif "2" == selection:
                    new_bitrate = bitrate_t.kbps_1p2_br
                elif "3" == selection:
                    new_bitrate = bitrate_t.kbps_2p4_br
                elif "4" == selection:
                    new_bitrate = bitrate_t.kbps_50_br
                elif "5" == selection:
                    new_bitrate = bitrate_t.kbps_200_br
                elif "6" == selection:
                    new_bitrate = bitrate_t.kbps_300_br
                elif "7" == selection:
                    new_bitrate = bitrate_t.kbps_400_br
                elif "8" == selection:
                    new_bitrate = bitrate_t.kbps_500_br
                else:
                    continue

                bitrate_set(uart_port, addr_src, addr_dest,  new_bitrate)

            case "3":

                print("1: Generate and load an encryption key")
                print("2: Enter an encryption key to load")
                selection = input("")

                if "1" == selection:

                    cipher_key = secrets.token_hex(16)

                    cipher_key_set(uart_port, addr_src, addr_dest,  cipher_key)

                if "2" == selection:

                    cipher_key = input("Enter an encryption key (16 hex digits): ")

                    cipher_key_set(uart_port, addr_src, addr_dest, cipher_key)
                     
            case "4":

                print("1: Enable encryption")
                print("2: Disable encryption")
                selection = input("")

                if "1" == selection:

                    encryption_set(uart_port, addr_src, addr_dest, True)

                elif "2" == selection:
                
                    encryption_set(uart_port, addr_src, addr_dest, False)
                                
            case "5":

                print("Please select a frequency band from the following list:")
                print("1: ITU Region 1 (863-870 MHz). Europe, Africa, West Asia")
                print("2: ITU region 2 (902-928 MHz). North and Sounth America")
                selection = input("")

                if "1" == selection:

                    freq_band = freq_band_t.itu_1_fb

                    default_channel_str = input("Enter a default channel 0-14 (press enter to use 7): ")

                    if "" == default_channel_str:

                        default_channel = 7

                    else:
                        try:
                            default_channel = int(default_channel_str)

                        except:
                            default_channel = 7

                    if default_channel >= 0 and default_channel <= 14:

                        freq_band_set(uart_port, addr_src, addr_dest,  freq_band, default_channel)

                if "2" == selection:
                    
                    freq_band = freq_band_t.itu_2_fb

                    default_channel_str = input("Enter a default channel 0-52 (press enter to use 26): ")

                    if "" == default_channel_str:
                    
                        default_channel = 26

                    else:
                        try:
                            default_channel = int(default_channel_str)

                        except:
                            default_channel = 26

                    if default_channel >= 0 and default_channel <= 52:

                        freq_band_set(uart_port, addr_src, addr_dest,  freq_band, default_channel)         

            case "6":     
                
                configuration_get(uart_port, addr_src, addr_dest)    

            case "7":

                radio_reset(uart_port, addr_src, addr_dest)

            case "8":

                break;

