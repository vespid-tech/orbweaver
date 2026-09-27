/*
 * Orbweaver - network.c
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "link.h"
#include "radio.h"
#include "status.h"
#include "cmd_proc.h"
#include "clocks.h"
#include "uart.h"
#include "spi.h"
#include "flash.h"
#include "cipher.h"
#include "rng.h"
#include "network.h"

#define NETWORK_GATHER_TIMEOUT 2000

#define NETWORK_CMD_TIMEOUT 600     //The flash chip needs 300 ms to begin operation after power on

#define FLASH_CONFIG_ADDR 0

#define CIPHER_KEY_DEFAULT_0 0
#define CIPHER_KEY_DEFAULT_1 0
#define CIPHER_KEY_DEFAULT_2 0
#define CIPHER_KEY_DEFAULT_3 0

typedef enum
{
    void_ns = 0,
    idle_ns,
    init_0_ns,
    init_1_ns,
    set_radio_default_ns,
    set_radio_default_peer_ns,
    gather_peers_ns,
    gather_peers_wait_ns,
    request_peers_spi_ns,
    request_peers_uart_ns,
    read_config_ns,
    get_config_ns,
    delete_save_config_ns,
    save_config_ns,
    delete_config_ns,
    rand_send_time_check_ns,
    randomize_channel_list_ns,
    error_ns
}network_state_t;


const uint8_t channel_list_default[CHANNEL_COUNT_ITU_2] =  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                                            10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                                            20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                                                            30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
                                                            40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                                                            50, 51, 52};

static uint8_t peer_count = 0;  //Number of peers we believe are in our network

/*
 * Our subnet consists of ourselves and any peers reachable via UART or SPI.
 *
 * 0 is an invalid ID. 0xFF is the universal ID.
 */
static uint8_t uart_peers[MAX_WIRED_PEERS] = {ADDR_INVALID};
static uint8_t spi_peers[MAX_WIRED_PEERS] = {ADDR_INVALID};

static uint8_t config_buff[FLASH_PAGE_SIZE];
static uint8_t config_offset;
static config_data_t config_data;

static uint8_t buff_tx[CMD_LEN_MAX];    //Used to store delayed send messages

static bool gather_network_flag = false;
static bool request_subnet_peers_flag = false;
static bool save_config_flag = false;
static bool delete_config_flag = false;
static bool randomize_channel_list_flag = false;

static uint32_t network_start_time = 0;

static uint64_t channel_list_index = 0;     //Used to randomize the channel list

static bool config_ready = false;

/*
 * We responding to 0xFF or retransmitting, we need to have a random delay before transmitting to ensure
 * we don't transmit over any other radios. The radio will wait to transmit if it knows a  packet is in
 * progress. If two radio start at almost the same time, they will likely interfere with each other.
 */
static uint32_t rand_send_start_time = 0;
static uint32_t rand_send_timeout = 0;
static bool rand_send_timer_active = false;

bool verify_config(config_data_t config_data);
void set_config_default(void);
bool verify_freq_band(freq_band_t new_freq_band, uint8_t new_default_channel);
bool verify_channel_list(uint8_t* channel_list, uint8_t length);

void network_service(void)
{
    static network_state_t network_state = void_ns;

    switch (network_state)
    {
        case void_ns:

            config_ready = false;

            network_start_time = systick;

            network_state = init_0_ns;

        break;

        case idle_ns:

            if (gather_network_flag)
            {
                gather_network_flag = false;

                if (peer_count > 0)
                {
                    network_start_time = systick;

                    network_state = set_radio_default_peer_ns;  //We have connected peers. Gather them first.
                }
                else
                {
                    network_state = set_radio_default_ns;
                }
            }
            else if (request_subnet_peers_flag)
            {
                request_subnet_peers_flag = false;

                network_start_time = systick;

                network_state = request_peers_spi_ns;
            }
            else if (save_config_flag)
            {
                save_config_flag = false;

                network_start_time = systick;

                network_state = read_config_ns;
            }
            else if (delete_config_flag)
            {
                delete_config_flag = false;

                network_start_time = systick;

                network_state = delete_config_ns;
            }
            else if (rand_send_timer_active)
            {
                network_state = rand_send_time_check_ns;
            }
            else if (randomize_channel_list_flag)
            {
                randomize_channel_list_flag = false;

                channel_list_index = config_data.channel_count - 1;   //We'll need this index to randomize the channel list.

                network_state = randomize_channel_list_ns;
            }

        break;

        case init_0_ns:

            if (FLASH_SUCCESS == flash_read(0, FLASH_PAGE_SIZE))
            {
                network_start_time = systick;

                network_state = init_1_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                set_config_default();   //The flash seems broke. Use default config.

                config_ready = true;    //Let everyone know the config is ready.

                network_state = idle_ns;
            }

        break;

        case init_1_ns:
        {
            int i;

            if (FLASH_SUCCESS == flash_get_data(config_buff, FLASH_PAGE_SIZE))
            {
                for (i=0; i<FLASH_PAGE_SIZE; i++)
                {
                    if (0xFF != config_buff[i]) //Empty flash is 0xFF, which is not a valid ID, so the first value that is not 0xFF is our ID (if we have one assigned)
                    {
                        memcpy(&config_data, &config_buff[i], sizeof(config_data_t));

                        if (verify_config(config_data))
                        {
                            load_cipher_key(config_data.cipher_key);    //Load the cipher key into the crypto system

                            load_default_channel(); //Load the default channel into the link layer

                            load_radio_bitrate();

                            config_ready = true;    //Let everyone know the config is ready.
                        }
                        else
                        {
                            config_delete();    //The config data is corrupted, delete it.

                            set_config_default();

                            config_ready = true;    //Let everyone know the config is ready.
                        }

                        break;
                    }
                }

                if (!config_ready)
                {
                    set_config_default();   //There was no config in flash. Set config to default values.

                    config_ready = true;    //Let everyone know the config is ready.
                }

                request_subnet_peers_flag = true;

                network_state = idle_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
               set_config_default();   //The flash seems broke. Use default config.

               network_state = idle_ns;
            }
        }
        break;

        case set_radio_default_ns:
        {
            freq_hop_enable(false);     //This will tune the radio to the default channel after frequency hopping is stopped

            network_start_time = systick;

            network_state = gather_peers_ns;
        }
        break;

        case set_radio_default_peer_ns:
        {
            uint8_t enable = 0x00;

            if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(uint8_t), freq_hop_enable_cmd, ADDR_BROADCAST, &enable, false))
            {
                network_state = set_radio_default_ns;
            }
            else if ((systick - network_start_time) >= get_radio_timeout())
            {
                report_error(network_timeout_err);

                network_state = error_ns;
            }
        }
        break;

        case gather_peers_ns:
        {
            frequency_t frequency;
            uint8_t channel;
            uint8_t enable = 0x00;

            //These next few lines calculate the channel from the radio frequency.
            frequency = get_current_freq();

            channel = ((frequency.freq - FREQ_MIN) * 2);

            if (frequency.frac)
            {
                channel++;
            }

            if (channel == channel_get(current_channel_index_get()))   //If the channel calculated from radio freq matches the current channel, then the channel change is complete
            {
                //Tell anyone that might be on the current channel to stop hopping and go to the default channel
                if (CMD_RESULT_SUCCESS == create_and_send_cmd(sizeof(system_cmd_header_t) + sizeof(uint8_t), freq_hop_enable_cmd, ADDR_BROADCAST, &enable, false))
                {
                    network_start_time = systick;

                    network_state = gather_peers_wait_ns;
                }
            }

            if ((systick - network_start_time) >= get_radio_timeout())
            {
                report_error(network_timeout_err);

                network_state = error_ns;
            }

        }
        break;

        case gather_peers_wait_ns:
        {
            uint8_t channel;

            if (get_cmd_proc_status() && get_link_status() && get_radio_status())   //If these are all free, the hop disable message has been sent.
            {
                channel = channel_get(current_channel_index_get());

                channel++;      //Switch to the next channel we need to gather on

                if (channel >= config_data.channel_count)
                {
                    change_channel(config_data.default_channel);    //We're all done. If things went well, every radio in range should now be on the default channel, so we should go there too.

                    network_state = idle_ns;
                }
                else
                {
                    change_channel(channel);    //Change to the new channel we need to gather on.

                    network_start_time = systick;

                    network_state = gather_peers_ns;
                }
            }
            else if ((systick - network_start_time) >= get_radio_timeout())
            {
                report_error(network_timeout_err);

                network_state = error_ns;
            }

        }
        break;

        case request_peers_spi_ns:
        {
            uint8_t msg_buff[sizeof(system_cmd_header_t)];

            create_cmd(msg_buff, sizeof(system_cmd_header_t), ADDR_BROADCAST, spi_peers_request_cmd, NULL);

            if (spi_send(msg_buff, sizeof(system_cmd_header_t)))
            {
                network_state = request_peers_uart_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_spi_err);

                network_start_time = systick;

                network_state = request_peers_uart_ns;
            }
        }
        break;

        case request_peers_uart_ns:
        {
            uint8_t msg_buff[sizeof(system_cmd_header_t)];

            create_cmd(msg_buff, sizeof(system_cmd_header_t), ADDR_BROADCAST, uart_peers_request_cmd, NULL);

            if (uart_send(msg_buff, sizeof(system_cmd_header_t)))
            {
                network_state = idle_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_uart_err);

                network_state = idle_ns;
            }
        }
        break;

        case read_config_ns:
        {
            if (FLASH_SUCCESS == flash_read(0, FLASH_PAGE_SIZE))
            {
                network_start_time = systick;

                network_state = get_config_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_flash_err);

                network_state = idle_ns;
            }

        }
        break;

        case get_config_ns:
        {
            int i;

            if (FLASH_SUCCESS == flash_get_data(config_buff, FLASH_PAGE_SIZE))
            {
                config_offset = 0;  //Assume there is no config data in flash

                network_state = save_config_ns;

                for (i=0; i<FLASH_PAGE_SIZE; i++)
                {
                    if (0xFF != config_buff[i]) //Empty flash is 0xFF, which is not a valid ID, so the first value that is not 0xFF is our ID (if we have one assigned)
                    {
                        config_offset = i;      //Turns out there is a config in flash

                        network_start_time = systick;

                        network_state = delete_save_config_ns;

                        break;
                    }
                }
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_flash_err);

                network_state = idle_ns;
            }
        }
        break;

        case delete_save_config_ns:

            if (FLASH_SUCCESS == flash_erase(FLASH_CONFIG_ADDR, page_fet))
            {
                network_start_time = systick;

                network_state = save_config_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_flash_err);

                network_state = idle_ns;
            }

        break;

        case save_config_ns:
        {
            uint32_t save_addr;

            save_addr = config_offset + sizeof(config_data_t);

            if ((save_addr + sizeof(config_data_t)) > FLASH_PAGE_SIZE)
            {
                save_addr = 0;
            }

            if (FLASH_SUCCESS == flash_write(save_addr, (uint8_t*)&config_data, sizeof(config_data_t)))
            {
                network_state = idle_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_flash_err);

                network_state = idle_ns;
            }
        }
        break;

        case delete_config_ns:

            if (FLASH_SUCCESS == flash_erase(FLASH_CONFIG_ADDR, page_fet))
            {
                network_state = idle_ns;
            }
            else if ((systick - network_start_time) >= NETWORK_CMD_TIMEOUT)
            {
                report_error(network_timeout_flash_err);

                network_state = idle_ns;
            }

        break;

        case rand_send_time_check_ns:
        {
            system_cmd_header_t* buff_tx_header = (system_cmd_header_t*)buff_tx;

            if ((systick - rand_send_start_time) >= rand_send_timeout)
            {
                rand_send_timer_active = false;

                link_send(buff_tx, buff_tx_header->length);
            }

            network_state = idle_ns;
        }
        break;

        case randomize_channel_list_ns:
        {
            uint64_t rand_index;

            rand_index = rng_get();

            rand_index = rand_index % (channel_list_index + 1);

            if (channel_list_index != rand_index)
            {
                config_data.channel_list[channel_list_index] ^= config_data.channel_list[rand_index];
                config_data.channel_list[rand_index]         ^= config_data.channel_list[channel_list_index];
                config_data.channel_list[channel_list_index] ^= config_data.channel_list[rand_index];
            }

            channel_list_index--;

            if (0 == channel_list_index)
            {
                save_config_flag = true;

                network_state = idle_ns;
            }
        }

        break;


        case error_ns:

            network_state = void_ns;

        break;
    }
}


void gather_network(void)
{
    gather_network_flag = true;
}

void request_subnet_peers(void)
{
    memset(uart_peers, 0x00, MAX_WIRED_PEERS);
    memset(spi_peers, 0x00, MAX_WIRED_PEERS);

    request_subnet_peers_flag = true;
}

void set_peers_spi(uint8_t* peers, uint8_t number_of_peers)
{
    memset(spi_peers, 0, MAX_WIRED_PEERS);

    memcpy(spi_peers, peers, number_of_peers);
}

void set_peers_uart(uint8_t* peers, uint8_t number_of_peers)
{
    memset(uart_peers, 0, MAX_WIRED_PEERS);

    memcpy(uart_peers, peers, number_of_peers);
}

void get_peers_spi(uint8_t* buff)
{
    memcpy(buff, spi_peers, MAX_WIRED_PEERS);
}

void get_peers_uart(uint8_t* buff)
{
    memcpy(buff, uart_peers, MAX_WIRED_PEERS);
}

bool is_peer_spi(uint8_t peer_id)
{
    int i;

    for (i=0; i<MAX_WIRED_PEERS; i++)
    {
        if (peer_id == spi_peers[i])
        {
            return true;
        }
    }

    return false;
}

bool is_peer_uart(uint8_t peer_id)
{
    int i;

    for (i=0; i<MAX_WIRED_PEERS; i++)
    {
        if (peer_id == uart_peers[i])
        {
            return true;
        }
    }

    return false;
}


inline void cipher_key_set(uint32_t* new_key)
{
    memcpy(&(config_data.cipher_key), new_key, CIPHER_KEY_LEN * sizeof(uint32_t));

    load_cipher_key(config_data.cipher_key);

    save_config_flag = true;
}


inline void our_addr_set(uint8_t new_address)
{
    config_data.addr = new_address;

    save_config_flag = true;
}

inline void crypto_set(bool crypto_enabled)
{
    config_data.crypto_enabled = crypto_enabled;

    save_config_flag = true;
}

inline bool is_crypto_enabled(void)
{
    return config_data.crypto_enabled;
}


inline int32_t config_set(config_data_t* new_config_data)
{
    if (verify_config(*new_config_data))
    {
        memcpy(&config_data, new_config_data, sizeof(config_data_t));

        load_cipher_key(config_data.cipher_key);    //Load the cipher key into the crypto system

        load_default_channel(); //Load the default channel into the link layer

        load_radio_bitrate();

        save_config_flag = true;

        return NETWORK_RESULT_ERR;
    }

    return NETWORK_RESULT_SUCCESS;
}


void config_delete(void)
{
    set_config_default();

    delete_config_flag = true;
}

void set_config_default(void)
{
    config_data.addr = 0;

    config_data.freq_band = itu_2_fb;

    config_data.radio_bitrate = kbps_50_br;

    config_data.cipher_key[0] = CIPHER_KEY_DEFAULT_0;
    config_data.cipher_key[1] = CIPHER_KEY_DEFAULT_1;
    config_data.cipher_key[2] = CIPHER_KEY_DEFAULT_2;
    config_data.cipher_key[3] = CIPHER_KEY_DEFAULT_3;

    config_data.crypto_enabled = false;

    config_data.channel_count = CHANNEL_COUNT_ITU_2;

    memcpy(config_data.channel_list, channel_list_default, CHANNEL_COUNT_ITU_2);   //Use ITU region 2 because it is the longer of the two regions we support

    config_data.default_channel = ITU_2_DEFAULT_CHANNEL;

    load_cipher_key(config_data.cipher_key);    //Load the cipher key into the crypto system

    load_default_channel();                     //Load the default channel into the link layer

    load_radio_bitrate();                       //Load the radio bitrate
}


inline uint8_t our_addr_get(void)
{
    return config_data.addr;
}


inline bool verify_freq_band(freq_band_t new_freq_band, uint8_t new_default_channel)
{
    if (itu_2_fb == new_freq_band)
    {
        if (new_default_channel >= CHANNEL_COUNT_ITU_2)
        {
            return false;
        }
    }
    else if (itu_1_fb == new_freq_band)
    {
        if (new_default_channel >= CHANNEL_COUNT_ITU_1)
        {
            return false;
        }
    }

    return true;
}


inline bool verify_config(config_data_t new_config_data)
{
    if ( (new_config_data.addr == 0)            ||
         (new_config_data.addr == 0xFF)         ||
         (new_config_data.crypto_enabled > 1)   ||
         (new_config_data.freq_band > itu_2_fb) )
    {
        return false;
    }

    if (!verify_freq_band(new_config_data.freq_band, new_config_data.default_channel))
    {
        return false;
    }

    return verify_channel_list(new_config_data.channel_list, new_config_data.channel_count);
}


inline bool verify_channel_list(uint8_t* channel_list, uint8_t length)
{
    int i;

    if ((CHANNEL_COUNT_ITU_1 == length) || (CHANNEL_COUNT_ITU_2 == length))
    {
        for (i=0; i<length; i++)
        {
            if (channel_list[i] >= length)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

/*
 * Determines how a command should be sent and sends it.
 *
 * If delay_xmit is true, and the command needs to be transmitted,
 * the transmission will be delayed by a random amount of time. This is
 * to prevent everyone from transmitting at the same time and interfering
 * with each other when responding to broadcast messages.
 */
int8_t send_cmd(uint8_t* cmd_buff_new, bool delay_xmit)
{
    system_cmd_header_t* cmd_buff_header_new = (system_cmd_header_t*)cmd_buff_new;

    if (is_peer_spi(cmd_buff_header_new->addr_dest))
    {
        return spi_send(cmd_buff_new, cmd_buff_header_new->length);
    }
    else if (is_peer_uart(cmd_buff_header_new->addr_dest))
    {
        return uart_send(cmd_buff_new, cmd_buff_header_new->length);
    }
    else if (is_peer_spi(cmd_buff_header_new->addr_src) || is_peer_uart(cmd_buff_header_new->addr_src))
    {
        return link_send(cmd_buff_new, cmd_buff_header_new->length);   //The message originated from someone on our subnet, forward it regardless of TTL and don't delay.
    }
    else if ((cmd_buff_header_new->addr_src == our_addr_get()))
    {
        if (delay_xmit)
        {
            return xmit_cmd_delayed(cmd_buff_new);  //The message originated with us, and requires a delay.
        }
        else
        {
            return link_send(cmd_buff_new, cmd_buff_header_new->length);   //The message originated with us, and doesn't require a delay.
        }
    }
    else if (cmd_buff_header_new->ttl > 0)                      //The message didn't originate from our subnet and isn't for our subnet, which means we are retransmitting it. Don't retransmit if TTL is 0
    {
        cmd_buff_header_new->ttl -= cmd_buff_header_new->ttl;   //Subtract 1 from TTL

        return xmit_cmd_delayed(cmd_buff_new);  //Delay before transmitting in case some other radio is about to do the same.
    }
    else
    {
        return CMD_RESULT_SUCCESS;  //The command is not going to or from our subnet, and TTL is 0. Don't forward it. Just carry on.
    }
}

/*
 * Transmits a command via the radio after a random delay.
 *
 * Some messages can't be sent immediately because they might interfere with
 * message sent by other radios. In that case, this function should be used.
 */
int8_t xmit_cmd_delayed(uint8_t* cmd_buff)
{
    system_cmd_header_t* cmd_buff_header = (system_cmd_header_t*)cmd_buff;
    uint64_t random_number;

    if (rand_send_timer_active)
    {
        return CMD_RESULT_BUSY; //We already have a message waiting to be sent.
    }
    else
    {
        random_number = rng_get();

        rand_send_timeout = random_number & 0x000000000000007F; //Should give us between 0 and 127 ms

        memcpy(buff_tx, cmd_buff, cmd_buff_header->length);     //Copy the command into the transmit buffer. We'll send it later.

        rand_send_start_time = systick;

        rand_send_timer_active = true;

        return CMD_RESULT_SUCCESS;
    }
}


int8_t retransmit_cmd(uint8_t* cmd_buff_new)
{
    system_cmd_header_t* cmd_buff_header_new = (system_cmd_header_t*)cmd_buff_new;

    if (is_peer_spi(cmd_buff_header_new->addr_src) || is_peer_uart(cmd_buff_header_new->addr_src))
    {
        return link_send(cmd_buff_new, cmd_buff_header_new->length);   //The message originated from our subnet, forward it regardless of TTL.
    }
    else if (cmd_buff_header_new->ttl > 0)      //The message didn't originate from our subnet. Don't retransmit if TTL is 0
    {
        cmd_buff_header_new->ttl--;             //Subtract 1 from TTL

        return link_send(cmd_buff_new, cmd_buff_header_new->length);
    }
    else
    {
        return CMD_RESULT_SUCCESS;  //The command is not going to or from our subnet, and TTL is 0. Don't forward it. Just carry on.
    }
}

void randomize_channel_list(void)
{
    randomize_channel_list_flag = true;
}

/*
 * Assigns a new channel list
 */
int32_t channel_list_set(uint8_t* new_channel_list, uint8_t len)
{
    if (verify_channel_list(new_channel_list, len))
    {
        memcpy(config_data.channel_list, new_channel_list, len);

        config_data.channel_count = len;

        load_default_channel(); //Load the default channel into the link layer

        load_radio_bitrate(); //Reconfigure the radio

        save_config_flag = true;

        return CMD_RESULT_SUCCESS;
    }

    return CMD_RESULT_ERROR;
}

uint8_t channel_list_get(uint8_t** channel_list_ptr)
{
    *channel_list_ptr = config_data.channel_list;

    return config_data.channel_count;
}

uint8_t channel_count_get(void)
{
    return config_data.channel_count;
}


void radio_bitrate_set(radio_bitrate_t new_radio_bitrate)
{
    config_data.radio_bitrate = new_radio_bitrate;

    load_radio_bitrate(); //Load the new bitrate into the radio

    save_config_flag = true;
}

inline radio_bitrate_t radio_bitrate_get(void)
{
    return config_data.radio_bitrate;
}

/*
 * Calling this function with a new frequency band will cause the channel list
 * to change to a sorted list (i.e. if it was shuffled, it will not be unshuffled).
 */
int32_t freq_band_set(freq_band_t new_freq_band, uint8_t new_default_channel)
{
    if (!verify_freq_band(new_freq_band, new_default_channel))
    {
        return CMD_RESULT_ERROR;
    }

    if (config_data.freq_band != new_freq_band) //Don't update the channel list unless we are changing bitrates.
    {
        if (itu_1_fb == new_freq_band)
        {
            config_data.channel_count = CHANNEL_COUNT_ITU_1;

            memcpy(config_data.channel_list, channel_list_default, CHANNEL_COUNT_ITU_1);
        }
        else
        {
            config_data.channel_count = CHANNEL_COUNT_ITU_2;

            memcpy(config_data.channel_list, channel_list_default, CHANNEL_COUNT_ITU_2);
        }
    }

    config_data.default_channel = new_default_channel;

    save_config_flag = true;

    return CMD_RESULT_SUCCESS;
}

void freq_band_get(freq_band_t* freq_band, uint8_t* default_channel)
{

    *freq_band = config_data.freq_band;

    *default_channel = config_data.default_channel;
}

uint8_t channel_get(uint8_t channel_index)
{
    return config_data.channel_list[channel_index];
}

uint8_t default_channel_get(void)
{
    return config_data.default_channel;
}

bool get_config_ready(void)
{
    return config_ready;
}
