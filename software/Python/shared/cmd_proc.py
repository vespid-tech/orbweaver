from asyncio import subprocess
from enum import IntEnum, auto
import queue
from socket import socket
from ctypes import *
import threading
import time
import subprocess
from unittest import case

from ow_protocol import *
from queues import QElement
from queues import spi_queue
from queues import uart_queue
from queues import cmd_proc_queue
from queues import test_queue
from queues import file_transfer_queue
from status import log_error

TCP_PEER_ADDR = 0x00 
OUR_ADDR = 0x02

def cmd_proc_thread():


    global TCP_PEER_ADDR
    conn = None
    use_uart = False

    sys_cmd_header = system_cmd_header_t()
    sys_cmd_header.aad.addr_src = OUR_ADDR
    sys_cmd_header.aad.addr_dest = BROADCAST_ADDR
    sys_cmd_header.length = sizeof(system_cmd_header_t) + 1  #+1 for OUR_ADDR

    if use_uart:

        sys_cmd_header.command = system_cmd_t.uart_peers_cmd
        q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header) + bytes([OUR_ADDR]))

        uart_queue.put(q_msg)

    else:

        sys_cmd_header.command = system_cmd_t.spi_peers_cmd
        q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header) + bytes([OUR_ADDR]))
        
        spi_queue.put(q_msg)


    while True:

        q_msg = cmd_proc_queue.get(block=True)

        if q_msg.cmd == "MSG":

            sys_cmd_header_recv = system_cmd_header_t.from_buffer_copy(q_msg.param)

            if INVALID_ADDR == sys_cmd_header_recv.aad.addr_dest:

                continue  # The ID is invalid. Just ignore it and carry on.
            
            elif TCP_PEER_ADDR == sys_cmd_header_recv.aad.addr_dest: # Check if the message is for the base

                if conn is not None:
                    
                    send_msg = bytes([START_BYTE]) + q_msg.param + bytes([STOP_BYTE])   # The message is for the base. Send it via TCP.
                    
                    conn.sendall(send_msg)

                continue
            
            elif (BROADCAST_ADDR == sys_cmd_header_recv.aad.addr_dest) and (TCP_PEER_ADDR == sys_cmd_header_recv.aad.addr_src):   # If the message is a broadcast from the base, transmit it

                q_msg = QElement(cmd="SEND", param=q_msg.param)

                # Forward the message as appropriate
                if use_uart:
                    uart_queue.put(q_msg)
                else:
                    spi_queue.put(q_msg)

                continue

            elif (OUR_ADDR == sys_cmd_header_recv.aad.addr_dest) or (BROADCAST_ADDR == sys_cmd_header_recv.aad.addr_dest):
            
                match sys_cmd_header_recv.command:  # If we got this far, the message is for us.

                    case system_cmd_t.void_cmd:
                        pass

                    case system_cmd_t.echo_cmd:

                        sys_cmd_header_send = system_cmd_header_t()
                        sys_cmd_header_send.command = system_cmd_t.echo_response_cmd
                        sys_cmd_header_send.aad.addr_src = OUR_ADDR
                        sys_cmd_header_send.aad.addr_dest = sys_cmd_header_recv.aad.addr_src
                        sys_cmd_header_send.length = sys_cmd_header_recv.length

                        if (TCP_PEER_ADDR == sys_cmd_header_send.aad.addr_dest) and (conn is not None):  # If the echo response is for the base, send it via TCP

                            send_msg = bytes([START_BYTE]) + bytes(sys_cmd_header_send) + q_msg.param[sizeof(system_cmd_header_t):] + bytes([STOP_BYTE])

                            conn.sendall(send_msg)

                        elif (TCP_PEER_ADDR != sys_cmd_header_send.aad.addr_dest):

                            send_msg = bytes(sys_cmd_header_send) + q_msg.param[sizeof(system_cmd_header_t):]
                            q_msg = QElement(cmd="SEND", param=send_msg)

                            if use_uart:
                                uart_queue.put(q_msg)
                            else:
                                spi_queue.put(q_msg)
        

                    case system_cmd_t.echo_response_cmd:
                        test_queue.put(q_msg)  # If we got an echo response, it must be for the endurance test. Send it to the test queue.
                
                    case system_cmd_t.uart_enable_cmd:
                        use_uart = q_msg.param[sizeof(system_cmd_header_t)]  # The parameter is a single byte that is 1 if we should use UART and 0 if we should use SPI

                    case system_cmd_t.uart_peers_request_cmd:

                        sys_cmd_header_send = system_cmd_header_t()
                        sys_cmd_header_send.command = system_cmd_t.uart_peers_cmd
                        sys_cmd_header_send.aad.addr_src = OUR_ADDR
                        sys_cmd_header_send.aad.addr_dest = sys_cmd_header_recv.aad.addr_src
                        sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 2

                        if INVALID_ADDR == TCP_PEER_ADDR:
                        
                            sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 1
                            q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header_send) + bytes([OUR_ADDR]))

                        else:

                            sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 2
                            q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header_send) + bytes([TCP_PEER_ADDR]) + bytes([OUR_ADDR]))
                        
                        
                        uart_queue.put(q_msg)
                        
                    case system_cmd_t.spi_peers_request_cmd:

                        if not use_uart:

                            sys_cmd_header_send = system_cmd_header_t()
                            sys_cmd_header_send.command = system_cmd_t.spi_peers_cmd
                            sys_cmd_header_send.aad.addr_src = OUR_ADDR
                            sys_cmd_header_send.aad.addr_dest = sys_cmd_header_recv.aad.addr_src
                            
                            if INVALID_ADDR == TCP_PEER_ADDR:

                                sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 1
                                q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header_send) + bytes([OUR_ADDR]))

                            else:

                                sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 2
                                q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header_send) + bytes([TCP_PEER_ADDR]) + bytes([OUR_ADDR]))

                            spi_queue.put(q_msg)

                            pass
                        else:
                            pass

                    case system_cmd_t.tcp_peers_cmd:

                        TCP_PEER_ADDR = q_msg.param[sizeof(system_cmd_header_t)]

                        sys_cmd_header_send = system_cmd_header_t()
                        sys_cmd_header_send.aad.addr_src = OUR_ADDR
                        sys_cmd_header_send.aad.addr_dest = BROADCAST_ADDR

                        TCP_PEER_ADDR = q_msg.param[sizeof(system_cmd_header_t)]

                        if INVALID_ADDR == TCP_PEER_ADDR:

                            sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 1
                            q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header_send) + bytes([OUR_ADDR]))

                        else:

                            sys_cmd_header_send.length = sizeof(system_cmd_header_t) + 2
                            q_msg = QElement(cmd="SEND", param=bytes(sys_cmd_header_send) + bytes([TCP_PEER_ADDR]) + bytes([OUR_ADDR]))


                        if use_uart:
                            sys_cmd_header_send.command = system_cmd_t.uart_peers_cmd
                            uart_queue.put(q_msg)
                        else:
                            sys_cmd_header_send.command = system_cmd_t.spi_peers_cmd
                            spi_queue.put(q_msg)

                        pass

                    case system_cmd_t.reset_radio_cmd:

                        subprocess.run(["pinctrl", "set", "17", "op", "dl", "pn"])  #Reset low

                        if q_msg.param[sizeof(system_cmd_header_t)]:  #check if we need to activate the bootloader

                            subprocess.run(["pinctrl", "set", "22", "op", "dl", "pn"])  #Boot low

                            time.sleep(0.1)
                            
                            subprocess.run(["pinctrl", "set", "17", "ip", "pn"])        #Reset high

                            time.sleep(0.1)
                            
                            subprocess.run(["pinctrl", "set", "22", "ip", "pn"])        #Boot high

                        else:

                            time.sleep(0.1)
                            
                            subprocess.run(["pinctrl", "set", "17", "ip", "pn"])        #Reset High

                        pass 

                    case system_cmd_t.error_cmd:
                    
                        log_error(q_msg.param[sizeof(system_cmd_header_t)])

                        pass
                       

                    case system_cmd_t.test_endurance_cmd:

                        if q_msg.param[sizeof(system_cmd_header_t)]:

                            from radio_tests import endurance_test_thread

                            thread_radio_test = threading.Thread(target=endurance_test_thread, daemon=True)
                            thread_radio_test.start()

                        else:
                            test_queue.put(QElement(cmd="STOP", param=None))

                        pass

                    case system_cmd_t.file_get_cmd |            \
                         system_cmd_t.file_set_cmd |            \
                         system_cmd_t.file_transfer_init_cmd |  \
                         system_cmd_t.file_transfer_start_cmd | \
                         system_cmd_t.file_frag_request_cmd |   \
                         system_cmd_t.file_frag_cmd |           \
                         system_cmd_t.file_transfer_complete_cmd:

                            file_transfer_queue.put(q_msg)
                        
                            pass

            else:  # All other messages get forwarded

                q_msg = QElement(cmd="SEND", param=q_msg.param)

                # Forward the message as appropriate
                if use_uart:
                    uart_queue.put(q_msg)
                else:
                    spi_queue.put(q_msg)

                continue

        elif q_msg.cmd == "NEW_CONN":

            conn = q_msg.param  # Save the new connection. We'll need it to send messages to the base.

            print("cmd_proc_thread: New connection received.\n")
