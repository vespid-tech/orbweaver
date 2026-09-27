/*
 * Orbweaver - link.c
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
#include <stdlib.h>
#include <string.h>

#include "inc/hw_gpt.h"
#include "inc/hw_fcfg1.h"
#include "driverlib/prcm.h"
#include "driverlib/timer.h"
#include "driverlib/ioc.h"
#include "driverlib/crypto.h"


#include "cipher.h"
#include "cmd_proc.h"
#include "radio.h"
#include "clocks.h"
#include "leds.h"
#include "rng.h"
#include "link.h"

/* This is how long it takes to send payloads of the following lengths in bytes at 50 kbps:
 * 0   = 2.78 ms
 * 10  = 3.75 ms
 * 50  = 10.14 ms
 * 100 = 18.14 ms
 * 200 = 34.14 ms
 * 250 = 42.14 ms
 */

/*
 * North and South America 915 MHz ISM band range: 902-928 MHz
 * Default frequency: 915 MHz
 *
 * European, African, and West Asian equivalent: 863-870 MHz
 * Default frequency: 866.5 MHz
 */

/*
 * link packet description:
 * bytes 0..7 MAC address
 * bytes 8..n payload
 * bytes n+1..n+8 random number (optional, used as part of nonce when encryption is enabled)
 * bytes n+9..n+24 authentication tag (optional, when encryption is enabled)
 * byte  n+25 (n+1 if encryption is disabled) fragment counter (optional, high nibble = total fragments. low nibble = current fragment when frequency hopping and packet fragmentation are both enabled)
 */
#define BUFF_LEN_MAX    512

#define MAC_ADDR_LEN    8

#define POINT_5_FREQ    0x8000  //Used when setting the frequecy to some whole number Mhz + 0.5

/*
 * FCC requires us to hop once every 400 ms when transmitting at 1 watt (+30 dBm) or above. The CC1310 max transmit power is +15 dBm (31.5 mW), so we don't legally have to hop.
 *
 * It takes ~314 us to hop to another frequency in the same band
 * It takes ~52 us to switch to receive mode
 *
 * It takes 1.28 ms to transmit the preamble and sync word. After the peer receives the sync word, it won't hop until after the rest of the message is received.
 * So don't try to transmit when there is 1.3 ms or less left before a channel hop. Start the hop when there is 0.7 ms left.
 */
#define HOP_TIME_FRAME  19200000 //At 48 MHz, this gives us 400 ms
#define HOP_TIME_RX     86000    //Allow 2 ms before hopping for any late messages to come in. Don't transmit during this time.
#define HOP_TIME_TXRX   HOP_TIME_FRAME - HOP_TIME_RX    //Time where TX is allowed
#define HOP_TIME_HOP    48000   //Allow 1 ms for us and the peer to hop channels

#define SYNC_COUNT_MIN        960000  //Only send a sync command if we have at least 20 ms on the hop timer to avoid the timer expiring when the sync is in progress
#define SYNC_SEND_TIME_CLEAR  206880  //It takes about 4.31 ms for the radio prepare and transmit a sync and the receiver to process it
#define SYNC_SEND_TIME_CRYPTO 434592

#define SYNC_COUNT_LOW  480000                    //Sync count range low (10 ms)
#define SYNC_COUNT_HIGH HOP_TIME_TXRX - 96000     //Sync count range high (the TXRX timer minus 2 ms)

/*
 * After the synchronize command is sent to the peer, it takes the peer about 263 us extra to fully
 * process the sync command if encryption is disabled. With encryption, it takes about 370 us.
 */
#define HOP_CLOCK_OFFSET_CLEAR  12624  //263 us at 48 MHz
#define HOP_CLOCK_OFFSET_CRYPTO 14304  //298 us at 48 MHz

#define LINK_TIMEOUT    5

/*
 * The part of the TX buffer that is transmitted in clear text rather than being encrypted.
 * This is the AAD (sequence, source, and destination) and TTL.
 */
#define CLEAR_BUFF_LEN (sizeof(aad_t) + sizeof(((system_cmd_header_t*)0)->ttl))

/*
 * When frequency hopping, we need to keep all the radios synced up. Our crystal has a 50 ppm
 * tolerance. That means we can lose or gain 3 ms per minute per radio. So the worst case is
 * two radios will drift away from each other by 6 ms every minute. To keep accuracy within 1 ms,
 * we need to resync every 10 seconds. We don't want to send out sync commands at exactly 10 seconds,
 * or our radios will interfere with each other as they all try to send a sync at once. Instead,
 * every radio should send a sync command at a random time between RESYNC_MIN_MS and
 * (RESYNC_MIN_MS + RESYNC_WINDOW_MS). The resync timer should be reset whenever a sync command is
 * received. Over time, this should keep the entire network in sync.
 */
#define RESYNC_MIN_MS 5000
#define RESYNC_WINDOW_MS 10000

#define LINK_RESULT_BUSY    0
#define LINK_RESULT_SUCCESS 1

typedef enum
{
    void_ls = 0,
    idle_ls,
    recv_ls,
    decrypt_ls,
    decrypt_wait_ls,
    encrypt_ls,
    encrypt_wait_ls,
    send_ls,
    hop_ls,
    sync_send_ls,
    sync_synchronize_ls,
    disable_hopping_ls,
    error_ls
}link_state_t;


static uint64_t mac_addr      = 0;

static uint8_t current_channel_index = 0;   //Currently active channel index

static bool synchronizing = false;          //When true, indicates we are synchronizing freq hopping with another radio
static bool hopping = false;                //When true, indicates we are currently hopping to the next frequency in the channel list
static bool tx_allowed = true;              //When true, indicate radio TX is allowed. See comments above for when TX is allowed or not

static uint32_t sync_start_time = 0;
static uint32_t sync_timeout = 0;

static bool radio_hop_flag = false;

static uint8_t buff_tx[BUFF_LEN_MAX];
static uint8_t buff_rx[BUFF_LEN_MAX];

static uint16_t buff_len_rx;
static uint16_t buff_len_tx;

static uint8_t* aad_tx = buff_tx;                           //The AAD is at the start of the buffer
static uint8_t* buff_encrypt_tx = &buff_tx[CLEAR_BUFF_LEN]; //The encryption buffer (the part that gets encrypted) starts after the clear text

static uint8_t* aad_rx = buff_rx;                           //The AAD is at the start of the buffer
static uint8_t* buff_encrypt_rx = &buff_rx[CLEAR_BUFF_LEN]; //The encryption buffer (the part that gets encrypted) starts after the clear text

static bool recv_active = false;
static bool send_active = false;

static bool enable_hopping_flag = false;
static bool disable_hopping_flag = false;
static bool hopping_enabled = false;

static uint8_t nonce[NONCE_LEN];

int8_t link_send_direct(uint8_t device_id, uint8_t len, system_cmd_t command, uint8_t* param);
void current_channel_set(void);
void hopping_enable(void);
void hopping_disable(void);
uint8_t channel_index_get(uint8_t channel);


void link_init(void)
{
    TimerConfigure(GPT0_BASE, TIMER_CFG_PERIODIC);                              //32-bit periodic timer that counts at 48 MHz. Used to time our hopping.

    TimerLoadSet(GPT0_BASE, TIMER_A, HOP_TIME_TXRX);

    TimerConfigure(GPT1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_ONE_SHOT);     //16-bit one-shot timer that counts at 48 MHz. Used to time the hop cool-down period.

    TimerLoadSet(GPT1_BASE, TIMER_A, HOP_TIME_HOP);

    HWREG(GPT1_BASE + GPT_O_IMR) |= GPT_IMR_TATOIM_EN;      //Unmask the timeout interrupt
    HWREG(GPT1_BASE + GPT_O_ICLR) = GPT_ICLR_TATOCINT;      //Clear the timeout interrupt
    IntEnable(INT_GPT1A);                                   //Enable the interrupt

    mac_addr  = HWREG(FCFG1_BASE + FCFG1_O_MAC_15_4_1);     //The MAC address is put in ROM by TI. Retrieve it.
    mac_addr  = 0xFFFFFFFF00000000 & (mac_addr << 32);
    mac_addr |= HWREG(FCFG1_BASE + FCFG1_O_MAC_15_4_0);
}


inline void link_service(void)
{
    static link_state_t link_state = void_ls;
    static uint32_t link_start_time = 0;
    static uint32_t timer_sync_count;

    switch (link_state)
    {
        case void_ls:

            if (get_config_ready()) //Don't continue unless we know we have a good configuration to use.
            {
                link_state = idle_ls;
            }

        break;

        case idle_ls:

            if (radio_hop_flag)
            {
                radio_hop_flag = false;         //It's time to hop to the next frequency

                link_start_time = systick;      //Start the timer in case we get hung up.

                link_state = hop_ls;
            }
            else if (recv_active)
            {
                link_start_time = systick;      //Start the timer in case we get hung up.

                if (is_crypto_enabled())
                {
                    link_state = decrypt_ls;    //The radio has placed a message in our RX buffer. Decrypt it.
                }
                else
                {
                    link_state = recv_ls;       //The radio has placed a message in our RX buffer. Go process it.
                }
            }
            else if (send_active && tx_allowed && (!hopping))
            {
                link_start_time = systick;      //Start the timer in case we get hung up.

                if (is_crypto_enabled())        //A message is waiting to be sent, we're not hopping, and we are allowed to TX.
                {
                    link_state = encrypt_ls;
                }
                else
                {
                    link_state = send_ls;       //No encryption needed. Just send the message.
                }
            }
            else if (enable_hopping_flag)
            {
                enable_hopping_flag = false;    //We need to start frequency hopping.

                send_active = true;             //We're about to send a channel map, so set send active now.

                link_start_time = systick;      //Start the timer in case we get hung up.

                link_state = sync_send_ls;
            }
            else if (disable_hopping_flag)
            {
                disable_hopping_flag = false;

                link_state = disable_hopping_ls;
            }
            else if (hopping_enabled && tx_allowed && (!hopping) && (HWREG(GPT0_BASE + GPT_O_TAILR) >= SYNC_COUNT_MIN))
            {
                if ((systick - sync_start_time) >= sync_timeout)
                {
                    link_state = sync_send_ls;
                }
            }

        break;

        case recv_ls:

            if (CMD_RESULT_SUCCESS == process_cmd((system_cmd_header_t*)buff_rx))
            {
                recv_active = false;

                link_state = idle_ls;
            }
            else if ((systick - link_start_time) >= get_radio_timeout())
            {
                report_error(link_cmd_result_err);

                link_state = error_ls;
            }

        break;

        case decrypt_ls:
        {
            if (CIPHER_SUCCESS == msg_decrypt(buff_encrypt_rx, (buff_len_rx - CLEAR_BUFF_LEN), aad_rx, sizeof(aad_t)))
            {
                link_state = decrypt_wait_ls;
            }
            else
            {
                report_error(link_decryption_err);

                recv_active = false;

                link_state = error_ls;
            }
        }
        break;

        case decrypt_wait_ls:

            if (AES_SUCCESS == CRYPTOCcmInvAuthDecryptStatus())
            {
                buff_len_rx -= NONCE_LEN;

                //(uint32_t*)(buff_rx + buff_len_rx - AUTH_TAG_LEN) points at the authentication tag which is stored in the buff_rx
                if (AES_SUCCESS == CRYPTOCcmInvAuthDecryptResultGet(AUTH_TAG_LEN, (uint32_t*)buff_encrypt_rx, (buff_len_rx - CLEAR_BUFF_LEN), (uint32_t*)(buff_rx + buff_len_rx - AUTH_TAG_LEN)))
                {
                    link_state = recv_ls;
                }
                else
                {
                    report_error(link_decryption_err);

                    recv_active = false;

                    link_state = error_ls;
                }
            }
            else if ((systick - link_start_time) >= LINK_TIMEOUT)
            {
                report_error(link_timeout_err);

                link_state = error_ls;
            }

        break;

        case encrypt_ls:
        {
            uint64_t random_number;

            random_number = rng_get();

            memcpy(nonce, &random_number, sizeof(uint64_t));

            random_number = rng_get();

            memcpy(&nonce[NONCE_LEN - sizeof(uint64_t)], &random_number, sizeof(uint64_t));

            msg_encrypt(buff_encrypt_tx, (buff_len_tx - CLEAR_BUFF_LEN), aad_tx, sizeof(aad_t), nonce);

            link_state = encrypt_wait_ls;
        }
        break;

        case encrypt_wait_ls:
        {
            uint32_t crypto_result;

            crypto_result = CRYPTOCcmAuthEncryptStatus();

            if (AES_SUCCESS == crypto_result)
            {
                if (AES_SUCCESS == CRYPTOCcmAuthEncryptResultGet(AUTH_TAG_LEN, (uint32_t*)(buff_tx + buff_len_tx))) //Copy the authorization tag into the tx buffer after the end of the message
                {
                    buff_len_tx = buff_len_tx + AUTH_TAG_LEN + NONCE_LEN;   //Message now has nonce and Auth tag attached, so increase the length

                    link_state = send_ls;
                }
                else
                {
                    report_error(link_encryption_err);

                    link_state = error_ls;
                }
            }
            else if (AES_DMA_BUS_ERROR == crypto_result)
            {
                report_error(link_encryption_err);

                link_state = error_ls;
            }
            else if ((systick - link_start_time) >= LINK_TIMEOUT)
            {
                report_error(link_timeout_err);

                link_state = error_ls;
            }
        }
        break;

        case send_ls:
        {
            if (buff_len_tx > BUFF_LEN_MAX)
            {
                send_active = false;

                report_error(link_buffer_err);

                link_state = error_ls;

                break;
            }

            if (CMD_RESULT_SUCCESS == radio_send(buff_tx, buff_len_tx))
            {
                if (synchronizing)
                {
                   link_state = sync_synchronize_ls;        //Since we are synchronizing, the message must have been a synch send message.
                }
                else
                {
                   send_active = false;         //Send complete.

                   link_state = idle_ls;        //Go wait for something to happen.
                }
            }
            else if ((systick - link_start_time) >= get_radio_timeout())
            {
                link_state = error_ls;
            }
        }
        break;

        case hop_ls:

            if (get_radio_status())                 //Make sure the radio isn't sending or receiving.
            {
                current_channel_set();              //Change frequencies.

                TimerEnable(GPT1_BASE, TIMER_A);    //Reset the hop timer

                link_state = idle_ls;               //We're done hopping. go wait for something to happen.
            }
            else if ((systick - link_start_time) >= get_radio_timeout())
            {
                report_error(link_timeout_err);

                link_state = error_ls;
            }

        break;

        case sync_send_ls:

            if (is_crypto_enabled())
            {
                timer_sync_count = HWREG(GPT0_BASE + GPT_O_TAV) - SYNC_SEND_TIME_CRYPTO;
            }
            else
            {
                timer_sync_count = HWREG(GPT0_BASE + GPT_O_TAV) - SYNC_SEND_TIME_CLEAR;
            }

            if ((timer_sync_count <= SYNC_COUNT_HIGH) && (timer_sync_count >= SYNC_COUNT_LOW))
            {
                synchronizing = true;

                buff_len_tx = sizeof(system_cmd_header_t) + sizeof(clock_sync_param_t);

                create_cmd(buff_tx, sizeof(system_cmd_header_t) + sizeof(clock_sync_param_t), BROADCAST_ID, clock_sync_cmd, (uint8_t*)&timer_sync_count);

                link_start_time = systick;

                if (is_crypto_enabled())
                {
                    link_state = encrypt_ls;
                }
                else
                {
                    link_state = send_ls;
                }
            }
            else
            {
                sync_start_time = systick;                                                  //The sync count is goofed up. Just reset the timer and try again next time
                sync_timeout = (uint32_t)((rng_get() % RESYNC_WINDOW_MS) + RESYNC_MIN_MS);

                synchronizing = false;
                send_active = false;

                link_state = idle_ls;
            }
        break;

        case sync_synchronize_ls:

            if (get_radio_status())  //When the radio is done sending, it means the peer received the sync message.
            {
                if (is_crypto_enabled())
                {
                    timer_sync_count += HOP_CLOCK_OFFSET_CRYPTO; //Offset the start_count to account for the peer's command processing time.
                }
                else
                {
                    timer_sync_count += HOP_CLOCK_OFFSET_CLEAR; //Offset the start_count to account for the peer's command processing time.
                }

                sync_hop_clock(true, timer_sync_count);   //Synchronize our hop clock. We should be pretty synced up with the peer now.

                synchronizing = false;
                send_active = false;

                link_state = idle_ls;
            }
            else if ((systick - link_start_time) >= get_radio_timeout())
            {
                synchronizing = false;
                send_active = false;

                report_error(link_timeout_err);

                link_state = error_ls;
            }

        break;

        case disable_hopping_ls:

            if (get_radio_status())  //Make sure we aren't going to interrupt the radio.
            {
                hopping_disable();

                link_state = idle_ls;
            }
            else if ((systick - link_start_time) >= get_radio_timeout())
            {
                report_error(link_timeout_err);

                link_state = error_ls;
            }

        break;

        case error_ls:

            recv_active = false;
            send_active = false;

            report_error(link_err);

            link_state = void_ls;

        break;
    }
}

/*
 * Use this function to send radio messages. Don't try to send direct.
 */
int8_t link_send(uint8_t* buff, uint16_t buff_len)
{
    if (send_active)
    {
        return CMD_RESULT_BUSY;
    }

    send_active = true;

    memcpy(buff_tx, buff, buff_len);
    buff_len_tx = buff_len;

    return CMD_RESULT_SUCCESS;
}

/*
 * Called by the radio when a message is received via RF.
 */
int8_t link_recv(uint8_t* buff, uint16_t buff_len)
{
    system_cmd_header_t* system_cmd_header = (system_cmd_header_t*)buff;

    if (recv_active)
    {
        return LINK_RESULT_BUSY;
    }

    if ((our_addr_get() != system_cmd_header->addr_dest) && (BROADCAST_ID != system_cmd_header->addr_dest))
    {
        if (!is_peer_spi(system_cmd_header->addr_dest) && !is_peer_uart(system_cmd_header->addr_dest))
        {
            //TODO: Check if we are mounted, and if not, increment hop counter. If we are mounted forward as usual.
            if (CMD_RESULT_SUCCESS == retransmit_cmd(buff))    //The message is not for us or our subnet. Retransmit it.
            {
                return LINK_RESULT_SUCCESS;
            }
            else
            {
                return LINK_RESULT_BUSY;
            }
        }
    }

    recv_active = true;

    buff_len_rx = buff_len;

    memcpy(buff_rx, buff, buff_len);

    return LINK_RESULT_SUCCESS;
}


/*
 * Used by anyone to enable/disable frequency hopping.
 *
 * When frequency hopping is stopped, the radio will be tuned
 * to the default frequency.
 */
void freq_hop_enable(bool enable)
{
    if (enable)
    {
        enable_hopping_flag = true;
    }
    else
    {
        disable_hopping_flag = true;
    }
}


/*
 * Used by the link layer to enable hopping.
 */
void hopping_enable(void)
{
    hopping_enabled = true;

    HWREG(GPT0_BASE + GPT_O_IMR) |= GPT_IMR_TATOIM_EN;

    HWREG(GPT0_BASE + GPT_O_ICLR) = GPT_ICLR_TATOCINT;

    IntPendClear(INT_GPT0A);

    IntEnable(INT_GPT0A);

    TimerEnable(GPT0_BASE, TIMER_A);
}

/*
 * Used by the link layer to disable hopping.
 */
void hopping_disable(void)
{
    hopping_enabled = false;

    TimerDisable(GPT0_BASE, TIMER_A);

    IntPendClear(INT_GPT0A);

    HWREG(GPT0_BASE + GPT_O_IMR) &= ~GPT_IMR_TATOIM_EN;

    IntDisable(INT_GPT0A);

    tx_allowed = true;  //Enable TX if it isn't enabled already.

    radio_hop_flag = false; //Make sure we aren't about to hop.

    current_channel_index = channel_index_get(default_channel_get());    //Go back to the default channel.

    current_channel_set();
}

/*
 *  Used by the link layer to set the current channel radio frequency.
 */
inline void current_channel_set(void)
{
    frequency_t frequency = current_freq_get();

    radio_set_freq(frequency);
}


frequency_t current_freq_get(void)
{
    frequency_t frequency;
    uint8_t current_channel = channel_get(current_channel_index);

    frequency.freq = FREQ_MIN + (uint16_t)(current_channel >> 1); //Divide the current channel by 2.

    if (0 == (current_channel % 2))   //If the current channel is odd, add 0.5 MHz to the frequency.
    {
        frequency.frac = 0;
    }
    else
    {
        frequency.frac = POINT_5_FREQ;
    }

    return frequency;
}

inline uint8_t current_channel_index_get(void)
{
    return current_channel_index;
}


inline void change_channel(uint8_t new_channel)
{
    if (new_channel < channel_count_get())
    {
        current_channel_index = channel_index_get(new_channel);

        current_channel_set();
    }
}


/*
 * If local_sync is true, it indicates we are initiating the sync.
 * If local_sync is false, it indicates the peer is initiating the sync.
 */
void sync_hop_clock(bool local, uint32_t timer_count)
{
    //led_toggle_red();

    HWREG(GPT0_BASE + GPT_O_TAV) = timer_count;

    if (!hopping_enabled)
    {
        hopping_enable();
    }

    sync_start_time = systick;                                                  //We just synced, so
    sync_timeout = (uint32_t)((rng_get() % RESYNC_WINDOW_MS) + RESYNC_MIN_MS);  //reset the resync clock.
}



/*
 * Returns true if link is busy. Returns false otherwise.
 */
inline bool get_link_status(void)
{
    if (send_active || recv_active)
    {
       return false;
    }

    return true;
}


inline uint64_t get_mac_addr(void)
{
    return mac_addr;
}


void load_default_channel(void)
{
    current_channel_index = channel_index_get(default_channel_get());
}


uint8_t channel_index_get(uint8_t channel)
{
    int i;
    uint8_t channel_count = channel_count_get();

    for (i=0; i<channel_count; i++)   //The list might be randomized. Find the default channel in the list.
    {
        if (channel == channel_get(i))
        {
            return i;
        }
    }

    return 0;
}


/*
 * Used for testing.
 *
 * If you need an accurate count enable gpt0, then
 * call this function to read the count. It counts
 * at 48 MHz.
 */
inline uint32_t get_gpt0_value(void)
{
    return HWREG(GPT0_BASE + GPT_O_TAV);
}


/*
 * This function is for testing only. Don't use it in the actually code.
 *
 * This function blocks.
 */
int8_t link_send_direct(uint8_t device_id, uint8_t len, system_cmd_t command, uint8_t* param)
{
    uint8_t addr_dest;
    aad_t aad;

    aad.addr_dest = device_id;

    system_cmd_header_t* tx_header = (system_cmd_header_t*)buff_tx;

    buff_len_tx = len;

    if (1 == our_addr_get())
    {
        addr_dest = 2;
    }
    else
    {
        addr_dest = 1;
    }

    tx_header->length = len;
    tx_header->command = command;
    tx_header->addr_src = our_addr_get();
    tx_header->addr_dest = addr_dest;

    if (param)
    {
        memcpy(&buff_tx[sizeof(system_cmd_header_t)], param, len-sizeof(system_cmd_header_t));
    }

    if (is_crypto_enabled())
    {
        buff_len_tx = msg_encrypt_blocking(buff_tx, buff_len_tx, (uint8_t*)&aad, sizeof(aad_t));
    }

    buff_len_tx += MAC_ADDR_LEN;

    return radio_send(buff_tx, buff_len_tx);
}


/*
 * The hop timer ISR.
 */
void gpt0_isr(void)
{
    uint32_t interrupts;

    if (tx_allowed) //If we are in the TX allowed state, we are about to transition into the RX only state.
    {
        TimerLoadSet(GPT0_BASE, TIMER_A, HOP_TIME_RX);  //Set the RX only time, so a TX doesn't start when we are close to hopping.

        tx_allowed = false; //Disallow TX until this timer expires again, or hopping it turned off.
    }
    else
    {
        TimerLoadSet(GPT0_BASE, TIMER_A, HOP_TIME_TXRX);    //Our RX only time just expired. Load the TX and RX time.

        current_channel_index++;  //We need to hop to the next channel, so increment the current channel.

        if (current_channel_index >= channel_count_get())
        {
            current_channel_index = 0;
        }

        tx_allowed = true;      //Allow TX once hopping is complete.
        hopping = true;         //Set hopping to true, which will disallow TX until the hop completes.

        radio_hop_flag = true;  //Tell the link layer to initiate a hop to the new current channel.
    }

    interrupts = HWREG(GPT0_BASE + GPT_O_MIS);

    HWREG(GPT0_BASE + GPT_O_ICLR) = interrupts;
}


/*
 * The hop cooldown ISR.
 */
void gpt1a_isr(void)
{
    HWREG(GPT1_BASE + GPT_O_ICLR) = GPT_ICLR_TATOCINT;

    hopping = false;
}

