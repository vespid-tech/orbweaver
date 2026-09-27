from enum import IntEnum, auto
import struct
from ctypes import *

START_BYTE = 0xAA
STOP_BYTE = 0xCC

# Address 0xFF is broadcast.
# 0 is an invalid address. 
# All other addresses can be used by the radio on the network
INVALID_ADDR = 0x00 
BROADCAST_ADDR = 0xFF

class system_cmd_t(IntEnum):
    void_cmd = 0
    echo_cmd = auto()
    echo_response_cmd = auto()
    error_cmd = auto()
    freq_band_set_cmd = auto()
    freq_band_get_cmd = auto()
    freq_band_cmd = auto()
    channel_set_cmd = auto()
    channel_get_cmd = auto()
    channel_cmd = auto()
    bitrate_set_cmd = auto()
    bitrate_get_cmd = auto()
    bitrate_cmd = auto()
    set_cipher_key_cmd = auto()
    encryption_enable_cmd = auto()
    freq_hop_enable_cmd = auto()
    randomize_channel_list_cmd = auto()
    channel_list_set_cmd = auto()
    channel_list_get_cmd = auto()
    channel_list_cmd = auto()
    clock_sync_cmd = auto()
    led_set_cmd = auto()
    flash_write_cmd = auto()
    flash_read_cmd = auto()
    flash_get_cmd = auto()
    flash_data_cmd = auto()
    flash_erase_cmd = auto()
    uart_enable_cmd = auto()
    network_gather_cmd = auto()
    status_request_cmd = auto()
    status_cmd = auto()
    assign_addr_cmd = auto()
    radio_xmit_cmd = auto()
    build_subnet_cmd = auto()
    uart_peers_request_cmd = auto()
    uart_peers_cmd = auto()
    spi_peers_request_cmd = auto()
    spi_peers_cmd = auto()
    tcp_peers_cmd = auto()
    rssi_get_cmd = auto()
    rssi_cmd = auto()
    reset_radio_cmd = auto()
    config_set_cmd = auto()
    config_delete_cmd = auto()
    firmware_ver_get_cmd = auto()
    firmware_ver_cmd = auto()
    ack_cmd = auto()
    nak_cmd = auto()
    radio_stats_get_cmd = auto()
    radio_stats_clear_cmd = auto()
    radio_stats_cmd = auto()
    test_endurance_cmd = auto()
    file_get_cmd = auto()
    file_set_cmd = auto()
    file_transfer_init_cmd = auto()
    file_transfer_start_cmd = auto()
    file_frag_request_cmd = auto()
    file_frag_cmd = auto()
    file_transfer_complete_cmd = auto()
    mac_addr_get_cmd = auto()
    mac_addr_cmd = auto()
    blob_cmd = auto()
    cmd_max_cmd = auto()


# typedef struct __attribute__((packed))
# {
#     uint8_t sequence;
#     uint8_t addr_src;
#     uint8_t addr_dest;
# }aad_t;

# typedef struct __attribute__((packed))
# {
#     aad_t;
#     uint8_t ttl;
#     uint8_t length;
#     system_cmd_t command;
# }system_cmd_header_t;


class aad_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("sequence", c_uint8),
        ("addr_src", c_uint8),
        ("addr_dest", c_uint8),
    ]

class system_cmd_header_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("aad", aad_t),
        ("ttl", c_uint8),
        ("length", c_uint8),
        ("command", c_uint8),
    ]

class flash_erase_type_t(IntEnum):
    void_fet = 0
    page_fet = auto()
    half_block_fet = auto()
    block_fet = auto()
    chip_fet = auto()

class boot_command_t(IntEnum):
    void_bc = 0
    activate_bootloader_bc = auto()
    load_app_bc = auto()
    launch_app_bc = auto()


class flash_erase_param_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("address", c_uint32),
        ("flash_erase_type", c_uint8),
    ]

class flash_write_param_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("address", c_uint32),
        ("length", c_uint8),
    ]

class flash_read_param_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("address", c_uint32),
        ("length", c_uint8),
    ]

class app_metadata_param_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("app_len", c_uint32),
        ("app_crc", c_uint32),
        ("boot_cmd", c_uint32),
    ]

class status_param_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("mac_addr", c_uint64),
        ("addr", c_uint8),
        ("spi_peers", c_uint8 * 6),
        ("uart_peers", c_uint8 * 6),
    ]
