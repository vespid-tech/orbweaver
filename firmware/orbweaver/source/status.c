/*
 * Orbweaver - status.c
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

#include <stdint.h>
#include <string.h>

#include "driverlib/ioc.h"

#include "radio.h"
#include "clocks.h"
#include "cmd_proc.h"
#include "version.h"
#include "status.h"

#define ERR_Q_LEN 10

#define ERR_TIMEOUT (get_radio_timeout() * MAX_WIRED_PEERS)

typedef enum
{
    void_es = 0,
    idle_es,
    get_peers_spi_es,
    send_err_spi_es,
    get_peers_uart_es,
    send_err_uart_es
}err_state_t;


static err_state_t err_state = void_es;

static uint8_t err_queue[ERR_Q_LEN] = {ok_err};
static uint8_t err_index_write = 0;
static uint8_t err_index_read = 0;

static uint32_t err_timer_start = 0;

/*
 * Status is used to send status and error information using either the radio,
 * buses, or STAT line.
 */
void status_init(void)
{
}

inline void status_set(uint8_t status)
{

}

void status_service(void)
{
    static uint8_t peers[MAX_WIRED_PEERS] = {0};
    static int peer_index;

    switch (err_state)
    {
        case void_es:
            err_state = idle_es;
        break;

        case idle_es:

            if (err_index_write != err_index_read)
            {
                err_state = get_peers_spi_es;
            }

        break;

        case get_peers_spi_es:

            get_peers_spi(peers);

            peer_index = 0;

            err_timer_start = systick;

            err_state = send_err_spi_es;

        break;

        case get_peers_uart_es:

            get_peers_uart(peers);

            peer_index = 0;

            err_timer_start = systick;

            err_state = send_err_uart_es;

        break;

        case send_err_spi_es:
        case send_err_uart_es:

            if (ADDR_INVALID != peers[peer_index])
            {
                if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(error_param_t), error_cmd, peers[peer_index], (uint8_t*)&err_queue[err_index_read], false))
                {
                    peer_index++;
                }
            }
            else
            {
                peer_index++;
            }

            if (peer_index >= MAX_WIRED_PEERS)
            {
                if (send_err_spi_es == err_state)
                {
                    err_state = get_peers_uart_es;
                }
                else if (send_err_uart_es == err_state)
                {
                    err_queue[err_index_read] = ok_err;

                    err_index_read = (err_index_read + 1) % ERR_Q_LEN;

                    err_state = idle_es;
                }
            }
            else if ((systick - err_timer_start) >= ERR_TIMEOUT)
            {
                err_index_write = 0;
                err_index_read = 0;

                err_state = idle_es;
            }

        break;
    }
}


void get_version(uint8_t* buff)
{
    strcpy((char*)buff, FIRMWARE_VERSION);
}

/*
 * Send a system error message to all peers on our wired subnet
 */
void report_error(system_err_t system_err)
{
    err_queue[err_index_write] = system_err;

    err_index_write = (err_index_write + 1) % ERR_Q_LEN;
}
