/*
 * Orbweaver - link.h
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

#ifndef LINK_H
#define LINK_H

#include "radio.h"
#include "network.h"

//#define CHANNEL_DEFAULT_ITU_2 26  //Channel 26 is 915 Mhz, which is right in the center of the ITU region 2 band
//#define CHANNEL_DEFAULT_ITU_1 7   //Channel 7 is 866.5 Mhz, which is right in the center of the ITU region 1 band

#define FREQ_BAND_DEFAULT 0 //Default to North and South America


/*
 * The frequencies are  (channel / 2) + FREQ_MIN (which is currently 902 MHz).
 * So channels 0, 1, 2 in this list will set the radio to 902, 902.5, 903, etc.
 */
extern const uint8_t channel_list_default[CHANNEL_COUNT_ITU_2];

void link_init(void);
void link_service(void);
int8_t link_send(uint8_t* buff, uint16_t buff_len);
int8_t link_recv(uint8_t* buff, uint16_t buff_len);
void freq_hop_enable(bool enable);
void sync_hop_clock(bool local, uint32_t timer_count);
void send_hop_map(void);
void change_channel(uint8_t new_channel);
uint8_t current_channel_index_get(void);
frequency_t current_freq_get(void);
bool get_link_status(void);
uint64_t get_mac_addr(void);
void load_default_channel(void);
uint32_t get_gpt0_value(void);
void gpt0_isr(void);
void gpt1a_isr(void);

#endif
