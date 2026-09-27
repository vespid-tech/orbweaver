/*
 * Orbweaver - network.h
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

#ifndef NETWORK_H
#define NETWORK_H

#include "cipher.h"
#include "radio.h"

#define NETWORK_RESULT_ERR    -1
#define NETWORK_RESULT_SUCCESS 1

#define ADDR_INVALID   0x00
#define ADDR_BROADCAST 0xFF

#define MAX_WIRED_PEERS 6   //The maximum number of peers we can have connected on each wired bus

#define CHANNEL_COUNT_ITU_1   15    //Europe, Africa, and West Asia
#define CHANNEL_COUNT_ITU_2   53    //North and South America

#define ITU_2_DEFAULT_CHANNEL 26

typedef enum __attribute__((packed))
{
    null_fb = 0,
    itu_1_fb,
    itu_2_fb
}freq_band_t;

typedef struct __attribute__((packed))
{
    uint8_t addr;
    freq_band_t freq_band;
    radio_bitrate_t radio_bitrate;
    uint32_t cipher_key[CIPHER_KEY_LEN];
    bool crypto_enabled;
    uint8_t channel_count;
    uint8_t default_channel;
    uint8_t channel_list[CHANNEL_COUNT_ITU_2];   //Use ITU region 2 because it is the longer of the two regions we support
}config_data_t;


void network_service(void);
void gather_network(void);
void request_subnet_peers(void);
void set_peers_spi(uint8_t* peers, uint8_t number_of_peers);
void set_peers_uart(uint8_t* peers, uint8_t number_of_peers);
void get_peers_spi(uint8_t* buff);
void get_peers_uart(uint8_t* buff);
bool is_peer_spi(uint8_t peer_id);
bool is_peer_uart(uint8_t peer_id);
void cipher_key_set(uint32_t* new_key);
void our_addr_set(uint8_t new_device_id);
uint8_t our_addr_get(void);
void crypto_set(bool crypto_enabled);
bool is_crypto_enabled(void);
int32_t config_set(config_data_t* new_config_data);
void freq_band_get(freq_band_t* freq_band, uint8_t* default_channel);
void config_delete(void);
int8_t send_cmd(uint8_t* cmd_buff_new, bool delay_xmit);
int8_t xmit_cmd_delayed(uint8_t* cmd_buff);
int8_t retransmit_cmd(uint8_t* cmd_buff_new);
void randomize_channel_list(void);
int32_t channel_list_set(uint8_t* new_channel_list, uint8_t len);
uint8_t channel_list_get(uint8_t** channel_list_ptr);
uint8_t channel_count_get(void);
void radio_bitrate_set(radio_bitrate_t radio_bitrate);
radio_bitrate_t radio_bitrate_get(void);
int32_t freq_band_set(freq_band_t new_freq_band, uint8_t new_default_channel);
uint8_t channel_get(uint8_t channel_index);
uint8_t default_channel_get(void);
bool get_config_ready(void);

#endif
