import queue

from typing import Any
from dataclasses import dataclass

@dataclass
class QElement:
    cmd: str
    param: Any

# All the queues used to communicate between threads.
cmd_proc_queue = queue.Queue()
spi_queue = queue.Queue()
uart_queue = queue.Queue()
test_queue = queue.Queue()
file_transfer_queue = queue.Queue()
