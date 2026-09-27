/*
 * Orbweaver - status.h
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

#ifndef STATUS_H
#define STATUS_H

typedef enum  __attribute__((packed))
{
    ok_err = 0,
    radio_err,
    radio_prop_done_stopped_err,
    radio_stop_wait_time_err,
    radio_recv_wait_time_err,
    radio_recv_err,
    radio_xmit_err,
    radio_xmit_time_err,
    radio_init_rat_err,
    radio_init_prop_div_err,
    radio_init_cmd_fs_err,
    radio_cmd_proc_time_err,
    uart_cmd_proc_err,
    link_cmd_result_err,
    link_send_err,
    link_header_err,
    link_err,
    link_encryption_err,
    link_decryption_err,
    link_timeout_err,
    link_buffer_err,
    uart_start_err,
    uart_header_err,
    uart_header_time_err,
    uart_param_err,
    uart_stop_err,
    uart_stop_time_err,
    cmd_send_busy_err,
    cmd_header_err,
    network_timeout_err,
    network_timeout_spi_err,
    network_timeout_uart_err,
    network_timeout_flash_err,
    rng_timeout_err,

    error_size_err = 0xFFFF
}system_err_t;

void status_init(void);
void status_service(void);
void status_set(uint8_t status);
void get_version(uint8_t* buff);
void report_error(system_err_t system_err);

#endif
