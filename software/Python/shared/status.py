from pathlib import Path
from datetime import datetime

system_errors = ["ok", 
                 "radio", 
                 "radio_prop_done_stopped", 
                 "radio_stop_wait_time", 
                 "radio_recv_wait_time", 
                 "radio_recv", 
                 "radio_xmit", 
                 "radio_xmit_time", 
                 "radio_init_rat", 
                 "radio_init_prop_div", 
                 "radio_init_cmd_fs", 
                 "radio_cmd_proc_time", 
                 "uart_cmd_proc", 
                 "link_cmd_result", 
                 "link_send", 
                 "link_header", 
                 "link_err", 
                 "link_encryption", 
                 "link_decryption", 
                 "link_timeout", 
                 "link_buffer", 
                 "uart_start", 
                 "uart_header", 
                 "uart_header_time", 
                 "uart_param", 
                 "serial_stop", 
                 "serial_stop_time", 
                 "cmd_send_busy", 
                 "cmd_header", 
                 "network_timeout"]

logging_enabled = True

def log_error(system_err):

    if logging_enabled:

        err_log_path = Path(__file__).resolve().parent / "error_log.txt"

        err_line = f"{datetime.now():%Y%m%d_%H%M%S}" + f" Error: {system_errors[system_err]}"
        
        with open(err_log_path, "a", encoding="utf-8") as err_file:
                
            err_file.write(err_line + "\n")

                
                
