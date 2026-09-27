import time
from typing import Any
from functools import partial
from ctypes import *

import spidev
import RPi.GPIO as GPIO
import queue

from shared.cmd_proc import OUR_ADDR, BROADCAST_ADDR, system_cmd_t, system_cmd_header_t, START_BYTE, STOP_BYTE
from shared.queues import QElement, spi_queue, cmd_proc_queue


DATA_READY_PIN = 27
RADIO_BUSY_PIN = 22

# Handles data ready events (when the radio pulls the data ready pin low)
def data_ready_event_handler(channel):
    spi_queue.put(QElement(cmd="RECV", param=None))

def spi_recv_msg(spi):

    parameter = b""
    
    # If the first byte we receive is not the start byte, it could be that the radio had data
    # left it the TX FIFO from an aborted message transfer attempt. The Radio TX FIFO is 8 bytes
    # deep, so read at least 9 bytes to clear anything that might be left in the FIFO.
    for i in range(9):  
        start_byte = spi.readbytes(1)

        if START_BYTE == start_byte[0]:
            break

    if START_BYTE != start_byte[0]:
        return None, None

    header = bytes(spi.readbytes(sizeof(system_cmd_header_t)))

    sys_cmd_header = system_cmd_header_t.from_buffer_copy(header)

    if sys_cmd_header.command < system_cmd_t.cmd_max_cmd.value: # Make sure the command makes sense

        param_len = sys_cmd_header.length - sizeof(system_cmd_header_t)

        if param_len > 0:
            parameter = bytes(spi.readbytes(param_len))

        stop_byte = bytes(spi.readbytes(1))

        if STOP_BYTE == stop_byte[0]:

            return sys_cmd_header, parameter

    return None, None


def spi_thread():

    spi = spidev.SpiDev()

    spi.open(0, 0)

    spi.max_speed_hz = 1000000  # Run the SPI bus at 1 MHz.
    spi.mode = 1                # Sets the SPI polarity and phase CPOL=0, CPHA=1

    # Configure the data ready pin. The radio pulls this pin low when it has data ready to send.
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    # Configure the radio busy pin. The radio pulls this pin low when it is busy.
    GPIO.setup(RADIO_BUSY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    # Set-up an event handler to be called when the data ready pin goes low
    GPIO.add_event_detect(DATA_READY_PIN, GPIO.FALLING, callback=data_ready_event_handler)

    while True:

        try:
            q_msg_spi = spi_queue.get(block=True)   # Wait forever for a command to be received

            if q_msg_spi.cmd == "RECV":

                header, parameter = spi_recv_msg(spi)

                if header is not None and parameter is not None:

                    # Send the received message to the command processor
                    if len(parameter) > 0:
                        q_msg = QElement(cmd="MSG", param=bytes(header) + parameter)
                    else:
                        q_msg = QElement(cmd="MSG", param=header)

                    cmd_proc_queue.put(q_msg)

                else:

                    dummy_bytes = spi.readbytes(16)
                    
            elif q_msg_spi.cmd == "SEND":

                send_msg = bytes([START_BYTE]) + bytes(q_msg_spi.param) + bytes([STOP_BYTE])

                start_time = time.time()
                timeout = 1  # Wait 1 second for the radio to become free.

                while GPIO.input(RADIO_BUSY_PIN) == 0:

                    time.sleep(0.001)

                    if time.time() - start_time > timeout:

                        print("Radio busy timeout!")
                        break

                print("Sending message:", " ".join(f"{b:02x}" for b in send_msg))
                spi.writebytes(send_msg)
            
        except queue.Empty:
            pass

    spi.close()