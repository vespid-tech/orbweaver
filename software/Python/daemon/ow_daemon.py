from email import header
import enum
import socket
import threading
from ctypes import *
import subprocess
from time import time

import cmd_proc
from cmd_proc import OUR_ADDR, cmd_proc_thread
from cmd_proc import system_cmd_t
from cmd_proc import system_cmd_header_t
from cmd_proc import START_BYTE
from cmd_proc import STOP_BYTE

from spi import spi_thread
from serial import serial_rx_thread
from serial import serial_tx_thread
from serial import init_serial
from radio_tests import endurance_test_thread

from queues import QElement
from queues import cmd_proc_queue
from file_transfer import file_transfer_thread


HOST = "0.0.0.0"  # all interfaces
PORT = 7878       # choose a free port

# Receives and processes commands that are received via TCP.
#
# addr is the address of the client that is connected. We aren't currently using addr.
def connection_thread(conn: socket.socket, addr: tuple):

    print("Connection established.")

    while True: # Keep going forever until the connection is closed by the client

        recv_byte = conn.recv(1, )  # Wait forever to receive a start byte

        if not recv_byte:   # If the received byte is empty, the connection has been closed

            sys_cmd_header = system_cmd_header_t()
            sys_cmd_header.command = system_cmd_t.tcp_peers_cmd
            sys_cmd_header.aad.addr_src = OUR_ADDR
            sys_cmd_header.aad.addr_dest = OUR_ADDR
            sys_cmd_header.length = sizeof(system_cmd_header_t) + 1

            q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header) + bytes([0x00]))
            cmd_proc_queue.put(q_msg)

            print("Connection closed by client.")

            conn.close()    # Close our end of the connection
            return

        if START_BYTE == recv_byte[0]:

            header = conn.recv(sizeof(system_cmd_header_t))  # We have received a start byte, try to receive a header

            if len(header) < sizeof(system_cmd_header_t):

                header_rest = conn.recv(sizeof(system_cmd_header_t) - len(header))   # We may have received a partial header, try to receive the rest

                header += header_rest

            if sizeof(system_cmd_header_t) == len(header):

                sys_cmd_header = system_cmd_header_t.from_buffer_copy(header)

                if sys_cmd_header.command < system_cmd_t.cmd_max_cmd.value: # We have received a header's worth of data, check if the command is valid

                    parameter_len = sys_cmd_header.length - sizeof(system_cmd_header_t)

                    parameter = conn.recv(parameter_len)    # Get the parameter

                    if len(parameter) < parameter_len:

                        parameter_rest = conn.recv(parameter_len - len(parameter))  # We received a partial parameter, try to receive the rest

                        parameter += parameter_rest

                    if sys_cmd_header.length == sizeof(system_cmd_header_t) + len(parameter):
                    
                        stop_byte = conn.recv(1)    # Try to reveive the stop byte

                        if STOP_BYTE == stop_byte[0]:

                            # It looks like we have received a valid message, send it to the command processor

                            if system_cmd_t.tcp_peers_cmd == sys_cmd_header.command:

                                our_peer = parameter[0]

                            q_msg = QElement(cmd="MSG", param=header + parameter)   
                            cmd_proc_queue.put(q_msg)


subprocess.run(["pinctrl", "set", "22", "ip", "pn"])        #Boot high
# subprocess.run(["pinctrl", "set", "17", "op", "dl", "pn"])  #Reset low (output, drive low,pull none)

# time.sleep(0.1)

subprocess.run(["pinctrl", "set", "17", "ip", "pn"])        #Reset high (line input, pull none)

# subprocess.run(["pinctrl", "set", "22", "op", "dl", "pn"])  #Boot low

init_serial()

thread_cmd_proc = threading.Thread(target=cmd_proc_thread, daemon=True)
thread_cmd_proc.start()

print("CMD proc thread started.")

thread_spi = threading.Thread(target=spi_thread, daemon=True)
thread_spi.start()

print("SPI thread started.")

thread_serial_rx = threading.Thread(target=serial_rx_thread, daemon=True)
thread_serial_rx.start()

print("serial RX thread started.")

thread_serial_tx = threading.Thread(target=serial_tx_thread, daemon=True)
thread_serial_tx.start()

print("serial TX thread started.")

thread_file_transfer = threading.Thread(target=file_transfer_thread, daemon=True)
thread_file_transfer.start()

print("File transfer thread started.")

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind((HOST, PORT))

while True:

    server.listen(1)    # Only allow the OS to queue 1 connection at a time. The connection is removed from the queue once server.accept() is called.

    print("Waiting for connection...")

    connection, address = server.accept()   # Wait forever for a connection

    # Send the new connection to the command processr
    q_msg = QElement(cmd="NEW_CONN", param=connection)
    cmd_proc_queue.put(q_msg)

    # start a new connection thread, and send the connection and client address to it
    thread_connection = threading.Thread(args=(connection, address), target=connection_thread, daemon=True)
    thread_connection.start()

