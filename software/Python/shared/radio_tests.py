import time
import os
from ctypes import sizeof
from datetime import datetime
from pathlib import Path

from queues import QElement, test_queue, cmd_proc_queue
from cmd_proc import OUR_ADDR, system_cmd_header_t, system_cmd_t

LOG_FILE_PATH = Path(__file__).resolve().parent / f"radio_endurance_test_{datetime.now():%Y%m%d_%H%M%S}.txt"


def append_log_line(log_line: str) -> None:
    with open(LOG_FILE_PATH, "a", encoding="utf-8") as log_file:
        log_file.write(log_line + "\n")


def create_echo_command(param: bytes, sequence: int = 0, address: int = 0) -> QElement:
    sys_cmd_header = system_cmd_header_t()
    sys_cmd_header.aad.sequence = sequence
    sys_cmd_header.aad.addr_src = OUR_ADDR
    sys_cmd_header.aad.addr_dest = address
    sys_cmd_header.ttl = 0
    sys_cmd_header.length = sizeof(system_cmd_header_t) + len(param)
    sys_cmd_header.command = system_cmd_t.echo_cmd

    return QElement(cmd="MSG", param=bytes(sys_cmd_header) + param)


def endurance_test_thread():
    sequence = 0

    append_log_line(f"{datetime.now():%Y%m%d_%H%M%S} Endurance test initiated")
    while True:

        log_line = f"{datetime.now():%Y%m%d_%H%M%S} Sending echo. "

        for i in range(3, 7):

            param_send = os.urandom(10)
            q_msg = create_echo_command(param_send, sequence, i)
            cmd_proc_queue.put(q_msg)
            sequence = (sequence + 1) & 0xFF
    
            try:
                q_msg = test_queue.get(block=True, timeout=0.1)

                if q_msg is not None and q_msg.cmd == "STOP":
                    log_line = log_line + " Stopped."
                    append_log_line(log_line)
                    return

                if q_msg is not None and q_msg.cmd == "MSG":
                    sys_cmd_header_recv = system_cmd_header_t.from_buffer_copy(q_msg.param)

                    if (sys_cmd_header_recv.command == system_cmd_t.echo_response_cmd) and (sys_cmd_header_recv.aad.addr_src == i):
                        param_recv = q_msg.param[sizeof(system_cmd_header_t):]

                        if param_recv != param_send:
                            log_line = log_line + str(i) + " Failed. Params don't match. "
                        else:
                            log_line = log_line + str(i) + " Success. "

            except Exception:
                log_line = log_line + str(i) + " Failed. No response. "
                q_msg = None

        append_log_line(log_line)

        time.sleep(60)

