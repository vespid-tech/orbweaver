/*
 * Orbweaver - radio.h
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

#ifndef RADIO_H
#define RADIO_H

#define FREQ_MIN          902   //TODO: Make this configurable
#define FREQ_DEFAULT      915   //TODO: Make this configurable
#define FREQ_FRAC_DEFAULT 0
#define FREQ_MAX          928   //TODO: Make this configurable

typedef struct
{
    uint16_t freq;
    uint16_t frac;
}frequency_t;

typedef struct __attribute__((packed))
{
    uint32_t count_sent;
    uint32_t count_recv;
}radio_stats_t;

typedef enum __attribute__((packed))
{
    null_br = 0,
    bps_625_br,
    kbps_2p5_br,
    kbps_5_br,
    kbps_50_br,
    kbps_200_br,
    kbps_300_br,
    kbps_400_br,
    kbps_500_br
}radio_bitrate_t;

void radio_init(void);
void radio_service(void);
int8_t radio_send_blocking(uint8_t* buff, uint16_t buff_len);
int8_t radio_send(uint8_t* buff, uint16_t buff_len);
bool get_radio_status(void);
int8_t radio_recv_blocking(void);
int8_t radio_recv_set(void);
void radio_set_freq(frequency_t new_freq);
void load_radio_bitrate(void);
void encryption_enable(bool enable);
frequency_t get_current_freq(void);
int8_t get_rssi(void);
radio_stats_t radio_stats_get(void);
void radio_stats_clear(void);
uint32_t get_radio_timeout(void);

#endif
