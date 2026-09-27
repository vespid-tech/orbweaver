import os
import math
from ctypes import *

from isort import file

from shared.cmd_proc import system_cmd_header_t, OUR_ADDR, system_cmd_t
from shared.queues import QElement, cmd_proc_queue, file_transfer_queue

def file_transfer_thread():

    frag_size = 0
    file_path = ""
    frags_recv = []
    
    while True:

        q_msg = file_transfer_queue.get(block=True)

        if q_msg.cmd == "MSG":

            sys_cmd_header_recv = system_cmd_header_t.from_buffer_copy(q_msg.param)

            match sys_cmd_header_recv.command:

                case system_cmd_t.file_get_cmd:

                    frag_size = q_msg.param[sizeof(system_cmd_header_t)]
                    file_path = q_msg.param[sizeof(system_cmd_header_t) + 1:].decode('utf-8')   #+1 for the fragment size

                    sys_cmd_header = system_cmd_header_t()
                    sys_cmd_header.aad.addr_src = OUR_ADDR
                    sys_cmd_header.aad.addr_dest = sys_cmd_header_recv.aad.addr_src
                    
                    if not os.path.isfile(file_path):
            
                        sys_cmd_header.length = sizeof(system_cmd_header_t)
                        sys_cmd_header.command = system_cmd_t.nak_cmd
            
                        q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header))
                        
                        cmd_proc_queue.put(q_msg)
            
                        continue
            
                    file_size = os.path.getsize(file_path)
                    frag_count = math.ceil(file_size / frag_size)
            
                    sys_cmd_header.length = sizeof(system_cmd_header_t) + sizeof(c_uint32) + sizeof(c_uint32)
                    sys_cmd_header.command = system_cmd_t.file_transfer_init_cmd
            
                    q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header) + bytes(c_uint32(file_size)) + bytes(c_uint32(frag_count)))
                    
                    cmd_proc_queue.put(q_msg)

                    pass

                case system_cmd_t.file_transfer_start_cmd:

                    file = open(file_path, 'rb')
                            
                    for i in range(frag_count):
            
                        data = file.read(frag_size)
            
                        sys_cmd_header.length = sizeof(c_uint32) + sizeof(system_cmd_header_t) + len(data)
            
                        sys_cmd_header.command = system_cmd_t.file_frag_cmd
            
                        q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header) + bytes(c_uint32(i)) + bytes(data))
            
                        cmd_proc_queue.put(q_msg)
                    
                    pass

                case system_cmd_t.file_frag_request_cmd:

                    fragment_index = c_uint32.from_buffer_copy(q_msg.param[sizeof(system_cmd_header_t):]).value
                    file_offset = fragment_index * frag_size

                    file.seek(file_offset)  # Move to offset
                    data = file.read(frag_size)

                    sys_cmd_header.length = sizeof(c_uint32) + sizeof(system_cmd_header_t) + len(data)
                    sys_cmd_header.command = system_cmd_t.file_frag_cmd

                    q_msg = QElement(cmd="MSG", param= bytes(sys_cmd_header) + bytes(c_uint32(fragment_index)) + bytes(data))

                    cmd_proc_queue.put(q_msg)
                    
                    pass

                case system_cmd_t.file_set_cmd:

                    frag_size = q_msg.param[sizeof(system_cmd_header_t)]
                    file_path = q_msg.param[sizeof(system_cmd_header_t) + 1:].decode('utf-8')   #+1 for the fragment size

                    sys_cmd_header = system_cmd_header_t()
                    sys_cmd_header.aad.addr_src = OUR_ADDR
                    sys_cmd_header.aad.addr_dest = sys_cmd_header_recv.aad.addr_src
                    sys_cmd_header.length = sizeof(system_cmd_header_t)

                    directory = os.path.dirname(file_path)

                    if os.path.isdir(directory):
                        
                        sys_cmd_header.command = system_cmd_t.ack_cmd

                    else:

                        sys_cmd_header.command = system_cmd_t.nak_cmd
                        
                    q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header))
                    
                    cmd_proc_queue.put(q_msg)

                    pass

                case system_cmd_t.file_transfer_init_cmd:

                    file_size = c_uint32.from_buffer_copy(q_msg.param[sizeof(system_cmd_header_t):sizeof(system_cmd_header_t) + sizeof(c_uint32)]).value
                    frag_count = c_uint32.from_buffer_copy(q_msg.param[sizeof(system_cmd_header_t) + sizeof(c_uint32):sizeof(system_cmd_header_t) + 2 * sizeof(c_uint32)]).value

                    frags_recv = [None] * frag_count  # Initialize the list with None values

                    sys_cmd_header = system_cmd_header_t()
                    sys_cmd_header.aad.addr_src = OUR_ADDR
                    sys_cmd_header.aad.addr_dest = sys_cmd_header_recv.aad.addr_src
                    sys_cmd_header.length = sizeof(system_cmd_header_t)    
                    sys_cmd_header.command = system_cmd_t.file_transfer_start_cmd

                    q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header))
                    cmd_proc_queue.put(q_msg)        

                    for i in range(frag_count):

                        try:
                            q_msg = file_transfer_queue.get(block=True, timeout=2)

                            if q_msg.cmd == "MSG":
                            
                                sys_cmd_header_recv = system_cmd_header_t.from_buffer_copy(q_msg.param)
                    
                                if (system_cmd_t.file_frag_cmd == sys_cmd_header_recv.command) and (sys_cmd_header_recv.aad.addr_src == sys_cmd_header.aad.addr_dest):

                                    frag_num = c_uint32.from_buffer_copy(q_msg.param[sizeof(system_cmd_header_t):sizeof(system_cmd_header_t) + sizeof(c_uint32)]).value
                                    frag = q_msg.param[sizeof(system_cmd_header_t) + sizeof(c_uint32):]

                                    frags_recv[frag_num] = frag

                                if frag_num == frag_count - 1:

                                    break;
                                
                                else:

                                    continue
                            else:

                                continue

                        except:

                            break

                    if None != get_missing_fragments(frags_recv, sys_cmd_header_recv.aad.addr_src):
                    
                        with open(file_path, 'wb') as f:
                            for fragment in frags_recv:
                                f.write(fragment)

                        sys_cmd_header.length = sizeof(system_cmd_header_t)    
                        sys_cmd_header.command = system_cmd_t.file_transfer_complete_cmd

                        q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header))
                        cmd_proc_queue.put(q_msg)        
                    pass

                case system_cmd_t.file_transfer_complete_cmd:
                    pass



def get_missing_fragments(frags_recv, peer_addr):

    sys_cmd_header = system_cmd_header_t()
    sys_cmd_header.aad.addr_src = OUR_ADDR
    sys_cmd_header.aad.addr_dest = peer_addr
    
    for fragment in frags_recv:

        if fragment is None:

            frag_recv = None

            for i in range(3):

                sys_cmd_header.length = sizeof(system_cmd_header_t) + sizeof(c_uint32)    
                sys_cmd_header.command = system_cmd_t.file_frag_request_cmd

                q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header) + bytes(c_uint32(frags_recv.index(fragment))))
                cmd_proc_queue.put(q_msg)

                try:
                    q_msg = file_transfer_queue.get(block=True, timeout=2)

                    if q_msg.cmd == "MSG":

                        sys_cmd_header_recv = system_cmd_header_t.from_buffer_copy(q_msg.param)

                        if (system_cmd_t.file_frag_cmd == sys_cmd_header_recv.command) and (sys_cmd_header_recv.aad.addr_src == sys_cmd_header.aad.addr_dest):

                            frag_num = c_uint32.from_buffer_copy(q_msg.param[sizeof(system_cmd_header_t):sizeof(system_cmd_header_t) + sizeof(c_uint32)]).value
                            frag_recv = q_msg.param[sizeof(system_cmd_header_t) + sizeof(c_uint32):]

                            frags_recv[frag_num] = frag_recv

                except:
                
                    continue

            if None == frag_recv:

                sys_cmd_header.command = system_cmd_t.nak_cmd
                sys_cmd_header.length = sizeof(system_cmd_header_t)

                q_msg = QElement(cmd="MSG", param=bytes(sys_cmd_header))
                cmd_proc_queue.put(q_msg)

                return None

    return frags_recv
