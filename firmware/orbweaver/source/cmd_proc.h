/*
 * Orbweaver - cmd_proc.h
 * Copyright (C) 2026 Vespid
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CMD_PROC_H
#define CMD_PROC_H

#include <stdint.h>
#include <stdbool.h>

#include "driverlib/udma.h"
#include "status.h"
#include "cipher.h"
#include "network.h"
#include "flash.h"

extern const uint8_t START_BYTE;
extern const uint8_t STOP_BYTE;

#define CMD_LEN_MAX   255
#define CMD_LEN_MIN   sizeof(system_cmd_header_t)

#define CMD_LEN_OFFSET   0
#define CMD_CMD_OFFSET   1
#define CMD_PARAM_OFFSET sizeof(system_cmd_header_t)

#define CMD_RESULT_ERROR  -1
#define CMD_RESULT_BUSY    0
#define CMD_RESULT_SUCCESS 1

#define BROADCAST_ID  0xFF


typedef enum __attribute__((packed))
{
    void_cmd,
    echo_cmd,
    echo_response_cmd,
    error_cmd,
    freq_band_set_cmd,
    freq_band_get_cmd,
    freq_band_cmd,
    channel_set_cmd,
    channel_get_cmd,
    channel_cmd,
    bitrate_set_cmd,
    bitrate_get_cmd,
    bitrate_cmd,
    set_cipher_key_cmd,
    encryption_enable_cmd,
    freq_hop_enable_cmd,
    randomize_channel_list_cmd,
    channel_list_set_cmd,
    channel_list_get_cmd,
    channel_list_cmd,
    clock_sync_cmd,
    led_set_cmd,
    flash_write_cmd,
    flash_read_cmd,
    flash_get_cmd,
    flash_data_cmd,
    flash_erase_cmd,
    uart_enable_cmd,
    network_gather_cmd,
    status_request_cmd,
    status_cmd,
    assign_addr_cmd,
    radio_xmit_cmd,
    build_subnet_cmd,
    uart_peers_request_cmd,
    uart_peers_cmd,
    spi_peers_request_cmd,
    spi_peers_cmd,
    tcp_peers_cmd,
    rssi_get_cmd,
    rssi_cmd,
    reset_radio_cmd,
    config_set_cmd,
    config_delete_cmd,
    firmware_ver_get_cmd,
    firmware_ver_cmd,
    ack_cmd,
    nak_cmd,
    radio_stats_get_cmd,
    radio_stats_clear_cmd,
    radio_stats_cmd,
    test_endurance_cmd,
    file_get_cmd,
    file_set_cmd,
    file_transfer_init_cmd,
    file_transfer_start_cmd,
    file_frag_request_cmd,
    file_frag_cmd,
    file_transfer_complete_cmd,
    mac_addr_get_cmd,
    mac_addr_cmd,
    blob_cmd,
    cmd_max_cmd
}system_cmd_t;


typedef struct __attribute__((packed))
{
    uint8_t sequence;
    uint8_t addr_src;
    uint8_t addr_dest;
}aad_t;

typedef struct __attribute__((packed))
{
    aad_t;
    uint8_t ttl;
    uint8_t length;
    system_cmd_t command;
}system_cmd_header_t;


typedef struct __attribute__((packed))
{
    system_err_t system_err;
}error_param_t;

typedef struct __attribute__((packed))
{
    freq_band_t freq_band;
    uint8_t default_channel;
}freq_band_param_t;

typedef struct __attribute__((packed))
{
    uint16_t freq;
    uint16_t frac_freq;
}set_freq_param_t;

typedef struct __attribute__((packed))
{
    radio_bitrate_t radio_bitrate;
}set_bitrate_param_t;

typedef struct __attribute__((packed))
{
    uint32_t new_cipher_key[CIPHER_KEY_LEN];
}set_cipher_key_param_t;

typedef struct __attribute__((packed))
{
    uint8_t enable;
}encryption_enable_param_t;


typedef struct __attribute__((packed))
{
    uint32_t timer_count;
}clock_sync_param_t;

typedef struct __attribute__((packed))
{
    uint8_t led;
    uint8_t state;
}set_led_param_t;

typedef struct __attribute__((packed))
{
    uint32_t address;
    uint8_t length;
    uint8_t data;
}flash_write_param_t;

typedef struct __attribute__((packed))
{
    uint32_t address;
    uint8_t length;
}flash_read_param_t;

typedef struct __attribute__((packed))
{
    uint8_t length;
}flash_get_param_t;

typedef struct __attribute__((packed))
{
    uint32_t address;
    flash_erase_type_t flash_erase_type;
}flash_erase_param_t;

typedef struct __attribute__((packed))
{
    uint8_t enable;
}uart_enable_param_t;

typedef struct __attribute__((packed))
{
    uint64_t mac_addr;
    uint8_t addr;
    uint8_t spi_peers[MAX_WIRED_PEERS];
    uint8_t uart_peers[MAX_WIRED_PEERS];
}status_param_t;

typedef struct __attribute__((packed))
{
    uint8_t addr;
}assign_addr_param_t;

typedef struct __attribute__((packed))
{
    int8_t rssi;
}rssi_param_t;

typedef struct __attribute__((packed))
{
    config_data_t config_data;
}load_config_param_t;

typedef struct __attribute__((packed))
{
    uint64_t mac_addr;
}mac_addr_param_t;


void cmd_proc_service(void);
int8_t process_cmd(system_cmd_header_t* recv_header);
int8_t create_and_send_cmd(uint8_t len, system_cmd_t system_cmd, uint8_t destination, uint8_t* param, bool xmit_delay);
bool verify_header(system_cmd_header_t* system_cmd_header);
void create_cmd(uint8_t* msg_buff, uint8_t len, uint8_t destination, system_cmd_t command, uint8_t* param);
bool get_cmd_proc_status(void);

#endif
