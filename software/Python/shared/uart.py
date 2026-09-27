import serial
import threading
import time
from ctypes import *

from cmd_proc import OUR_ADDR, BROADCAST_ADDR, system_cmd_t
from cmd_proc import system_cmd_header_t
from cmd_proc import START_BYTE
from cmd_proc import STOP_BYTE

from queues import QElement, cmd_proc_queue, uart_queue

# UART Configuration
UART_PORT = "/dev/ttyACM0"  # Raspberry Pi UART port (adjust as needed)
UART_BAUD = 460800

ser = None

def init_uart():

    global ser

    try:
        ser = serial.Serial(port=UART_PORT, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=None, rtscts=True)
    except:
        print(f"Error could not open serial port: {UART_PORT}")


def uart_tx_thread():
    
    global ser

    print("UART TX thread started.")
    while True:

        try:
            
            q_msg = uart_queue.get(block=True)  # Wait forever for a message
        
            print("UART TX thread received a message.")
            
            if q_msg and q_msg.cmd == "SEND":
        
                send_msg = bytes([START_BYTE]) + q_msg.param + bytes([STOP_BYTE])
                ser.write(send_msg)

        except:

            print("UART send failed.")

def uart_recv_msg(ser_port):

    parameter = b""
    recv_byte = ser_port.read(1)
      
    if recv_byte:

        if START_BYTE == recv_byte[0]:

            header = ser_port.read(sizeof(system_cmd_header_t))

            if len(header) == sizeof(system_cmd_header_t):

                sys_cmd_header = system_cmd_header_t.from_buffer_copy(header)

                msg_len = sys_cmd_header.length
                
                if msg_len >= sizeof(system_cmd_header_t):

                    parameter_len = msg_len - sizeof(system_cmd_header_t)

                    if parameter_len > 0:
                        parameter = ser_port.read(parameter_len)

                        if len(parameter) != parameter_len:
                            return None, None

                    stop_byte = ser_port.read(1)

                    if not stop_byte:

                        return None, None  

                    if STOP_BYTE == stop_byte[0]:

                        return sys_cmd_header, parameter

    return None, None  

def uart_rx_thread():

    global ser

    init_uart()

    while True:

        header, parameter = uart_recv_msg(ser)

        if header is not None and parameter is not None:

            if len(parameter) > 0:
                complete_msg = bytes(header) + parameter
            else:
                complete_msg = bytes(header)

            q_msg = QElement(cmd="MSG", param=complete_msg)
            cmd_proc_queue.put(q_msg)
