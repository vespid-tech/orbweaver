/*
 * Orbweaver - cmd_proc.c
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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "./inc/hw_memmap.h"
#include "./inc/hw_fcfg1.h"
#include "./inc/hw_types.h"
#include "./inc/hw_gpt.h"
#include "./inc/hw_gpt.h"
#include "./driverlib/sys_ctrl.h"

#include "uart.h"
#include "radio.h"
#include "leds.h"
#include "clocks.h"
#include "link.h"
#include "spi.h"
#include "flash.h"
#include "network.h"
#include "status.h"
#include "cmd_proc.h"


/*
 * Different ways commands are passed through the system:
 *
 * UART | SPI --> command processor --> UART | SPI
 * RF --> Link --> command processor --> Link --> RF
 * UART | SPI --> command processor --> Link --> RF
 * etc.
 *
 * Every module (i.e. UART, SPI, RF, Link, Command processor) has one receive and one send buffer.
 *
 * The slowest data rate is RF, which times out after RADIO_XMIT_TIMEOUT (82 ms). If an RF TX/RX times out, the RF TX/RX buffer will be freed up
 * for another message. Because of this, if everyone's buffer is full, any message that anyone tries to send to the command processor, might have
 * to wait up to RADIO_XMIT_TIMEOUT, therefore, all messages heading toward the command processor should timeout after RADIO_XMIT_TIMEOUT in case the
 * command processor is waiting for a radio transmission to complete. The command processor waits forever to send anything. This effectively
 * gives us two FIFOs, one RX and one TX, which have between 3 to 5 elements, depending on the path of the message through the system.
 *
 * The downside of this is that every message gets copied up to five times as it moves through the system. In the future, we might want to just
 * have some buffer that we can pass pointers to, rather than coping the messages over and over.
 *
 */

static uint8_t cmd_buff_rx[CMD_LEN_MAX];
static uint8_t cmd_buff_tx[CMD_LEN_MAX];
static system_cmd_header_t* recv_header = (system_cmd_header_t*)cmd_buff_rx;
static system_cmd_header_t* tx_header   = (system_cmd_header_t*)cmd_buff_tx;
static uint8_t* recv_param = cmd_buff_rx + sizeof(system_cmd_header_t);
static uint8_t* param_tx = cmd_buff_tx + sizeof(system_cmd_header_t);

static bool rx_ready = false;

/*
 * Processes all received messages.
 *
 * This won't block and won't timeout. It will try forever to process, send, or forward messages.
 */
inline void cmd_proc_service(void)
{
    if (!rx_ready)
    {
        return; //There is no command waiting to be processed, so just bail.
    }

    if (verify_header(recv_header)) //Is the header good?
    {
        if ((recv_header->addr_dest != our_addr_get()) &&    //Is the message not for us and
            (BROADCAST_ID != recv_header->addr_dest)  )         //not a broadcast message (which means it's for everyone).
        {
            if (CMD_RESULT_SUCCESS ==  send_cmd(cmd_buff_rx, false))    //The message is not for us. Forward it.
            {
                rx_ready = false;
            }

            return;
        }
    }
    else
    {
        report_error(cmd_header_err);

        rx_ready = false;

        return;
    }

    switch (recv_header->command)   //If we got this far, there is a message for us. Process it.
    {
        case echo_cmd:

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(recv_header->length, echo_response_cmd, recv_header->addr_src, recv_param, (BROADCAST_ID == recv_header->addr_dest)))
            {
                rx_ready = false;
            }

        break;

        case freq_band_set_cmd:
        {
            freq_band_param_t freq_band_param;

            memcpy(&freq_band_param, recv_param, sizeof(freq_band_param));

            freq_band_set(freq_band_param.freq_band, freq_band_param.default_channel);

            rx_ready = false;
        }
        break;

        case freq_band_get_cmd:
        {
            freq_band_param_t freq_band_param;

            freq_band_get(&freq_band_param.freq_band, &freq_band_param.default_channel);

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(freq_band_param), freq_band_cmd, recv_header->addr_src, &freq_band_param, (BROADCAST_ID == recv_header->addr_dest)))
            {
                rx_ready = false;
            }
        }

        break;

        case channel_set_cmd:
        {
            uint8_t* new_channel = recv_param;

            change_channel(*new_channel);

            rx_ready = false;
        }
        break;

        case channel_get_cmd:
        {
            uint8_t channel = channel_get(current_channel_index_get());

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(channel), channel_cmd, recv_header->addr_src, &channel, (BROADCAST_ID == recv_header->addr_dest)))
            {
                rx_ready = false;
            }
        }
        break;

        case bitrate_set_cmd:
        {
            radio_bitrate_t* radio_bitrate = (radio_bitrate_t*)recv_param;

            radio_bitrate_set(*radio_bitrate);

            rx_ready = false;
        }
        break;

        case bitrate_get_cmd:
        {
            radio_bitrate_t radio_bitrate = radio_bitrate_get();

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(radio_bitrate), bitrate_cmd, recv_header->addr_src, &radio_bitrate, (BROADCAST_ID == recv_header->addr_dest)))   //We already put the parameter in the command, so pass in a NULL
            {
                rx_ready = false;
            }
        }
        break;

        case set_cipher_key_cmd:

            cipher_key_set((uint32_t*)recv_param);

            rx_ready = false;

        break;

        case encryption_enable_cmd:

            crypto_set(*recv_param);

            rx_ready = false;

        break;

        case freq_hop_enable_cmd:

            freq_hop_enable(*recv_param);

            rx_ready = false;

        break;

        case randomize_channel_list_cmd:

            randomize_channel_list();

            rx_ready = false;

        break;

        case channel_list_set_cmd:

            channel_list_set(recv_param, (recv_header->length) - sizeof(system_cmd_header_t));

            rx_ready = false;

        break;

        case channel_list_get_cmd:
        {
            uint8_t* channel_list_ptr = NULL;
            uint8_t channel_count = channel_list_get(&channel_list_ptr);

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + channel_count, channel_list_cmd, recv_header->addr_src, channel_list_ptr, (BROADCAST_ID == recv_header->addr_dest)))   //We already put the parameter in the command, so pass in a NULL
            {
                rx_ready = false;
            }
        }
        break;

        case clock_sync_cmd:
        {
            clock_sync_param_t* clock_sync_param = (clock_sync_param_t*)recv_param;

            sync_hop_clock(false, clock_sync_param->timer_count);

            rx_ready = false;
        }
        break;

        case led_set_cmd:
        {
            set_led_param_t* set_led_param = (set_led_param_t*)recv_param;

            if (LED_GREEN == set_led_param->led)
            {
                led_set_green(set_led_param->state);
            }
            else if (LED_AMBER == set_led_param->led)
            {
                led_set_amber(set_led_param->state);
            }
            else if (LED_RED == set_led_param->led)
            {
                led_set_red(set_led_param->state);
            }

            rx_ready = false;
        }

        break;

        case flash_write_cmd:
        {
            flash_write_param_t* flash_write_param = (flash_write_param_t*)recv_param;

            if (FLASH_SUCCESS == flash_write(flash_write_param->address, &(flash_write_param->data), flash_write_param->length))
            {
                if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t), ack_cmd, recv_header->addr_src, NULL, (BROADCAST_ID == recv_header->addr_dest)))
                {
                    rx_ready = false;
                }
            }
        }
        break;

        case flash_read_cmd:
        {
            flash_read_param_t* flash_read_param = (flash_read_param_t*)recv_param;
            flash_get_param_t* flash_get_param = (flash_get_param_t*)recv_param;

            if (FLASH_SUCCESS == flash_read(flash_read_param->address, flash_read_param->length))
            {
                recv_header->length = sizeof(system_cmd_header_t) + sizeof(flash_get_param_t);
                recv_header->command = flash_get_cmd;
                flash_get_param->length = flash_read_param->length;
            }
        }
        break;

        case flash_get_cmd:
        {
            flash_get_param_t* flash_get_param = (flash_get_param_t*)recv_param;

            if (FLASH_SUCCESS == flash_get_data(param_tx, flash_get_param->length))
            {
                if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + flash_get_param->length, flash_data_cmd, recv_header->addr_src, NULL, (BROADCAST_ID == recv_header->addr_dest)))   //We already put the parameter in the command, so pass in a NULL
                {
                    rx_ready = false;
                }
            }
        }
        break;

        case flash_erase_cmd:
        {
            flash_erase_param_t* flash_erase_param = (flash_erase_param_t*)recv_param;

            if (FLASH_SUCCESS == flash_erase(flash_erase_param->address, flash_erase_param->flash_erase_type))
            {
                if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t), ack_cmd, recv_header->addr_src, NULL, (BROADCAST_ID == recv_header->addr_dest)))
                {
                    rx_ready = false;
                }
            }
        }
        break;

        case uart_enable_cmd:

            rx_ready = false;

        break;

        case network_gather_cmd:

            gather_network();

            rx_ready = false;

        break;

        case status_request_cmd:
        {
            status_param_t status_param;

            status_param.mac_addr = get_mac_addr();
            status_param.addr = our_addr_get();
            get_peers_spi(status_param.spi_peers);
            get_peers_uart(status_param.uart_peers);

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(status_param_t), status_cmd, recv_header->addr_src, (uint8_t*)&status_param, (BROADCAST_ID == recv_header->addr_dest)))
            {
                rx_ready = false;
            }
        }
        break;

        case assign_addr_cmd:
        {
            assign_addr_param_t* assign_addr_param = (assign_addr_param_t*)recv_param;

            our_addr_set(assign_addr_param->addr);

            rx_ready = false;
        }
        break;

        case radio_xmit_cmd:

            if (link_send(recv_param, recv_header->length - sizeof(system_cmd_header_t)))
            {
                rx_ready = false;
            }

        break;

        case build_subnet_cmd:

            request_subnet_peers();

            rx_ready = false;

        break;

        case uart_peers_cmd:
        {
            uint8_t number_of_peers;

            number_of_peers = recv_header->length - sizeof(system_cmd_header_t);

            set_peers_uart(recv_param, number_of_peers);

            rx_ready = false;
        }
        break;

        case spi_peers_cmd:
        {
            uint8_t number_of_peers;

            number_of_peers = recv_header->length - sizeof(system_cmd_header_t);

            set_peers_spi(recv_param, number_of_peers);

            rx_ready = false;
        }
        break;

        case rssi_get_cmd:
        {
            rssi_param_t rssi_param;

            rssi_param.rssi = get_rssi();

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(rssi_param_t), rssi_cmd, recv_header->addr_src, (uint8_t*)&rssi_param, (BROADCAST_ID == recv_header->addr_dest)))
            {
               rx_ready = false;
            }
        }
        break;

        case reset_radio_cmd:

            SysCtrlSystemReset();

        break;

        case config_set_cmd:

            config_set((config_data_t*)recv_param);

            rx_ready = false;

        break;

        case config_delete_cmd:

            config_delete();

            rx_ready = false;

        break;

        case firmware_ver_get_cmd:

            get_version(param_tx);

            tx_header->length = sizeof(system_cmd_header_t) + strlen((char*)param_tx) + 1; //+1 for string null terminator
            tx_header->command = firmware_ver_cmd;
            tx_header->addr_src = our_addr_get();
            tx_header->addr_dest = recv_header->addr_src;

            if (CMD_RESULT_SUCCESS == send_cmd(cmd_buff_tx, (BROADCAST_ID == recv_header->addr_dest)))
            {
               rx_ready = false;
            }

        break;

        case radio_stats_get_cmd:
        {
            radio_stats_t radio_stats = radio_stats_get();

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(radio_stats_t), radio_stats_cmd, recv_header->addr_src, (uint8_t*)&radio_stats, (BROADCAST_ID == recv_header->addr_dest)))
            {
               rx_ready = false;
            }

        }
        break;

        case radio_stats_clear_cmd:

            radio_stats_clear();

            rx_ready = false;

        break;

        case mac_addr_get_cmd:
        {
            mac_addr_param_t mac_addr_param;

            mac_addr_param.mac_addr = get_mac_addr();

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(mac_addr_param_t), mac_addr_cmd, recv_header->addr_src, (uint8_t*)&mac_addr_param, (BROADCAST_ID == recv_header->addr_dest)))
            {
               rx_ready = false;
            }
        }
        break;


        default:
            rx_ready = false;
        break;

    }


    return;
}


/*
 * Called by anyone that receives a message.
 */
int8_t process_cmd(system_cmd_header_t* header)
{
    if (rx_ready)
    {
        return CMD_RESULT_BUSY;
    }

    memcpy(cmd_buff_rx, header, header->length);

    led_toggle_red();

    rx_ready = true;

    return CMD_RESULT_SUCCESS;
}


/*
 * Called by anyone that wants to send a message to anyone regardless if it is via a wired
 * connection or not. How the message will be sent is determined later.
 */
int8_t create_and_send_cmd(uint8_t len, system_cmd_t command, uint8_t destination, uint8_t* param, bool xmit_delay)
{
    tx_header->length = len;
    tx_header->command = command;
    tx_header->addr_src = our_addr_get();
    tx_header->addr_dest = destination;

    if (param)
    {
        memcpy(&cmd_buff_tx[CMD_PARAM_OFFSET], param, len-sizeof(system_cmd_header_t));
    }

    return send_cmd(cmd_buff_tx, xmit_delay);
}


/*
 * Returns true if the header is good.
 */
bool verify_header(system_cmd_header_t* system_cmd_header)
{
    if ( (system_cmd_header->command > void_cmd)        &&
         (system_cmd_header->command < cmd_max_cmd)     &&
         (system_cmd_header->length >= CMD_LEN_MIN)     &&
         (system_cmd_header->length <= CMD_LEN_MAX)     &&
         (system_cmd_header->addr_dest != ADDR_INVALID) )
    {
        return true;
    }
    else
    {
        return false;
    }
}


inline void create_cmd(uint8_t* msg_buff, uint8_t len, uint8_t destination, system_cmd_t command, uint8_t* param)
{
    system_cmd_header_t* tx_header   = (system_cmd_header_t*)msg_buff;

    tx_header->length = len;
    tx_header->command = command;
    tx_header->addr_src = our_addr_get();
    tx_header->addr_dest = destination;

    if (param)
    {
        memcpy(&msg_buff[sizeof(system_cmd_header_t)], param, len-sizeof(system_cmd_header_t));
    }
}

/*
 * Returns true if the command processor is free. Returns false otherwise.
 */
inline bool get_cmd_proc_status(void)
{
    if (rx_ready)
    {
        return false;
    }

    return true;
}


