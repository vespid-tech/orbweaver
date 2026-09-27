/*
 * Orbweaver - radio.c
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


#include <stdlib.h>
#include "./driverlib/rf_prop_cmd.h"
#include "./driverlib/sys_ctrl.h"
#include "./inc/hw_rfc_dbell.h"
#include "./driverlib/rf_data_entry.h"
#include "./driverlib/rf_mailbox.h"
#include "./driverlib/setup.h"

#include "./inc/hw_gpt.h"
#include "./inc/hw_rfc_rat.h"
#include "./inc/hw_rfc_dbell.h"
#include "./driverlib/rfc.h"
#include "./driverlib/rf_mailbox.h"
#include "./driverlib/rf_prop_mailbox.h"

#include "smartrf_settings.h"
#include "cmd_proc.h"
#include "clocks.h"
#include "leds.h"
#include "link.h"
#include "radio.h"



//Internal limitations in the the radio modem limits the packet length that can be received to 8191 bytes including header and CRC

//The radio always sends the following before anything that appears in the receive buffer:
//4 bytes of preamble
//4 bytes of sync word

//After the packet the radio sends the following that doesn't appear in the receive buffer:
//2 bytes CRC

//For PROP_RX_ADV, The receive buffer contains the following before the payload (defined in rfc_dataEntry_t):
//Bytes 0..3 pointer to next buffer
//Byte 4 status
//Byte 5 configuration
//Byte 6..7 buffer length
//Byte 8 length written to receive buffer not including bytes 0..7 (byte 0 of header)
//Byte 9 length of payload (byte 1 of header)
//Byte 10 payload start
//Byte after payload RSSI

//For PROP_RX (which we are not using), the receive buffer contains the following before the payload (defined in rfc_dataEntry_t):
//Bytes 0..3 pointer to next buffer
//Byte 4 status
//Byte 5 configuration
//Byte 6..7 buffer length
//Byte 8..9 length of payload
//Byte 10 payload start

#define RX_LEN_OFFSET   8
#define RX_METADATA_LEN 9
#define PACKET_BUFF_LEN 512
#define PAYLOAD_INDEX   10

#define LENGTH_LEN   2

#define RF_CMD0 0x0607

#define RADIO_CMD_TIMEOUT  100//5

//Radio timeout is calculated as ~(1.5 * 256 * 8) / bitrate)
//with a minimum value of 10 ms.
//e.g. 50 kbps the timeout is 61 ms
#define RADIO_TIMEOUT_DEFAULT 61
#define RADIO_TIMEOUT_625     4915
#define RADIO_TIMEOUT_2p5k    1229
#define RADIO_TIMEOUT_5k      614
#define RADIO_TIMEOUT_50k     61
#define RADIO_TIMEOUT_200k    16
#define RADIO_TIMEOUT_300k    11
#define RADIO_TIMEOUT_400k    10
#define RADIO_TIMEOUT_500k    10

typedef enum
{
    void_rs = 0,
    idle_rs,
    rat_wait_rs,
    config_radio_wait_rs,
    stop_rs,
    stop_wait_rs,
    recv_set_rs,
    recv_wait_rs,
    recv_rs,
    recv_busy_rs,
    xmit_set_rs,
    xmit_rs,
    freq_set_rs,
    freq_wait_rs,
    process_cmd_rs,
    power_off_wait_rs,
    power_on_wait_rs,
    error_rs
}radio_state_t;


//static volatile rfc_CMD_PROP_RADIO_DIV_SETUP_t pRadioSetup;

dataQueue_t g_data_queue;

static uint8_t packet_tx[PACKET_BUFF_LEN];
static uint8_t packet_rx[PACKET_BUFF_LEN];

static uint16_t* rx_packet_len =  (uint16_t*)&packet_rx[RX_LEN_OFFSET];

static rfc_dataEntry_t* entry;
static uint8_t* command_rx = &packet_rx[PAYLOAD_INDEX];

static int8_t rssi;     //RSSI of the last received packet

static bool send_ready  = false;
static bool command_done = false;
static bool packet_recv = false;
static bool freq_set_flag = false;
static bool load_bitrate_flag = false;
static bool recv_busy = false;

static uint32_t radio_start_time = 0;

static frequency_t radio_freq =
{
     .freq = FREQ_DEFAULT,
     .frac = FREQ_FRAC_DEFAULT
};

static radio_stats_t radio_stats =
{
     .count_sent = 0,
     .count_recv = 0
};

static uint32_t radio_timeout = RADIO_TIMEOUT_DEFAULT;

void radio_isr(void);
void radio_hw_isr(void);
void load_bitrate(void);

void radio_init(void)
{

    //SetupTrimDevice();    //This called in ResetISC() in the startup file.

    HWREG(RFC_PWR_NONBUF_BASE + RFC_PWR_O_PWMCLKEN) = RFC_PWR_PWMCLKEN_CPE_M | RFC_PWR_PWMCLKEN_CPERAM_M;  //Enable the the command processor and command processor RAM clocks

    //If you want to turn on the clocks via the RFC_PWR_O_PWMCLKEN register, it seems
    //you have to wait (for the CPE to boot?) after its clock is enabled, so just use this command instead.
    RFCDoorbellSendTo(CMDR_DIR_CMD_2BYTE(RF_CMD0, RFC_PWR_PWMCLKEN_MDMRAM | RFC_PWR_PWMCLKEN_RFERAM));  //Turn on the modem and RF engine RAM clocks

    /*
    RF_Mode RF_prop = rf_mode_get();

    //Apply the necessary patch functions
    if (RF_prop.cpePatchFxn != NULL)
        RF_prop.cpePatchFxn();
    if (RF_prop.mcePatchFxn != NULL)
        RF_prop.mcePatchFxn();
    if (RF_prop.rfePatchFxn != NULL)
        RF_prop.rfePatchFxn();
*/
    RFCDoorbellSendTo(CMDR_DIR_CMD_1BYTE(CMD_BUS_REQUEST, 1));  //Set system bus to needed

    RFCAdi3VcoLdoVoltageMode(true); //Set VCOLDO reference to voltage mode.

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEISL) = 0x0;  //Select what set of interrupt vectors the command and packet engine (CPE)should use
    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = 0x0;  //Clear all CPE interrupt flags

    IntPendClear(INT_RFC_CPE_0);    //Unpend (i.e. clear unhandled) command and packet engine (CPE) interrupts
    IntEnable(INT_RFC_CPE_0);       //Enable CPE interrupts

    IntPendClear(INT_RFC_HW_COMB);  //Unpend (i.e. clear unhandled) RFC HW interrupts
    IntEnable(INT_RFC_HW_COMB);     //Enable RFC HW interrupts

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFHWIFG) = ~RFC_DBELL_RFHWIFG_MDMSOFT;    //Clear the synch word received interrupt flag
    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFHWIEN) = RFC_DBELL_RFHWIFG_MDMSOFT;     //Enable the synch word received interrupt. It is used to indicate a packet reception has started.
}


void radio_service(void)
{
    static radio_state_t radio_state = void_rs;

    switch (radio_state)
    {
        case void_rs:

            if (get_config_ready()) //Don't continue unless we know we have a good configuration to use.
            {
                radio_state = idle_rs;
            }

        break;

        case idle_rs:

            //Wait for either the Network layer or Command Proc to to configure us.
            //Once configured, we should never return to the idle state.
            if (load_bitrate_flag)
            {
                load_bitrate_flag = false;

                RFCDoorbellSendTo(CMDR_DIR_CMD(CMD_START_RAT)); //Start the radio timer (RAT)

                radio_start_time = systick;

                radio_state = rat_wait_rs;
            }

        break;

        case rat_wait_rs:

            if (0 != HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_CMDSTA))
            {
                command_done = false;

                load_bitrate();

                radio_start_time = systick;

                radio_state = config_radio_wait_rs;
            }
            else if ((systick - radio_start_time) >= RADIO_CMD_TIMEOUT)
            {
                report_error(radio_init_rat_err);

                radio_state = error_rs;
            }

        break;

        case config_radio_wait_rs:

            if (command_done)
            {
                radio_state = freq_set_rs;
            }
            else if ((systick - radio_start_time) >= RADIO_CMD_TIMEOUT)
            {
                report_error(radio_init_prop_div_err);

                radio_state = error_rs;
            }

        break;

        case stop_rs:

            HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_COMMAND_DONE;     //Clear the command done interrupt
            HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIEN) = RFC_DBELL_RFCPEIFG_COMMAND_DONE;      //Enable the command done interrupt

            command_done = false;

            RFCDoorbellSendTo(CMDR_DIR_CMD(CMD_STOP));  //Send the stop command

            radio_start_time = systick;

            radio_state = stop_wait_rs;

        break;

        case stop_wait_rs:

            if (command_done)
            {
                command_done = false;

                if (PROP_DONE_STOPPED != RF_cmdPropRxAdv.status)
                {
                    report_error(radio_prop_done_stopped_err);

                    radio_state = error_rs;
                }
                else if (send_ready)
                {
                    radio_state = xmit_set_rs;
                }
                else if (freq_set_flag)
                {
                    freq_set_flag = false;

                    radio_state = freq_set_rs;
                }
                else if (load_bitrate_flag)
                {
                    load_bitrate_flag = false;

                    command_done = false;

                    load_bitrate();

                    radio_start_time = systick;

                    radio_state = config_radio_wait_rs;
                }
                else
                {
                    radio_state = recv_set_rs;  //If there is nothing else to do, set the radio in RX mode, and wait for something to arrive.
                }
            }
            else if ((systick - radio_start_time) >= get_radio_timeout())
            {
                report_error(radio_stop_wait_time_err);

                radio_state = error_rs;
            }

        break;

        case recv_set_rs:

            radio_recv_set();

            radio_start_time = systick;

            radio_state = recv_wait_rs;

        break;

        case recv_wait_rs:

           //It takes ~52 us to switch to receive mode
           if (ACTIVE == RF_cmdPropRxAdv.status)
           {
               radio_state = recv_rs;
           }
           else if ((systick - radio_start_time) >= RADIO_CMD_TIMEOUT)
           {
               report_error(radio_recv_wait_time_err);

               radio_state = error_rs;
           }

        break;

        case recv_rs:

            if (recv_busy)
            {
                radio_start_time = systick; //We have started receiving a message. Set a timeout.

                radio_state = recv_busy_rs;
            }
            else if ((send_ready || freq_set_flag || load_bitrate_flag) && !recv_busy)
            {
                radio_state = stop_rs;  //A message is ready, we have to change bitrates, or we have to change the frequency, and we aren't receiving, so stop the radio and go do it.
            }
            else if (PROP_DONE_RXERR == RF_cmdPropRxAdv.status)
            {
                report_error(radio_recv_err);   //We are no longer receiving, but we didn't receive anything good or bad, so something is wrong.

                radio_state = recv_set_rs;  //RX error probably means a mangled packet was received. Go back to receive.
            }
            else if (ACTIVE != RF_cmdPropRxAdv.status)
            {
                radio_state = recv_set_rs;
            }

        break;

        case recv_busy_rs:

            if (packet_recv)
            {
                if (*rx_packet_len >= sizeof(system_cmd_header_t))
                {
                    rssi = command_rx[*rx_packet_len];  //Record the RSSI in case we need it later.

                    radio_stats.count_recv++;

                    radio_start_time = systick;

                    radio_state = process_cmd_rs;
                }
                else
                {
                    radio_state = recv_set_rs;
                }
            }
            else if ((systick - radio_start_time) >= get_radio_timeout())
            {
                recv_busy = false;

                radio_state = recv_set_rs;
            }

        break;

        case xmit_set_rs:

            HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_COMMAND_DONE; //We use the command done interrupt to know when our TX completed.
            HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIEN) = RFC_DBELL_RFCPEIFG_COMMAND_DONE;

            command_done = false;

            RFCDoorbellSendTo((uint32_t)&RF_cmdPropTxAdv);

            radio_start_time = systick;

            radio_state = xmit_rs;

        break;

        case xmit_rs:

            if (command_done)
            {
                command_done = false;

                send_ready  = false;

                if (PROP_DONE_OK == RF_cmdPropTxAdv.status)
                {
                    led_toggle_amber();

                    radio_stats.count_sent++;

                    radio_state = recv_set_rs;  //TX has completed. Go back to receive.
                }
                else
                {
                    report_error(radio_xmit_err);

                    radio_state = error_rs;
                }

            }
            else if ((systick - radio_start_time) >= get_radio_timeout())
            {
                send_ready  = false;

                report_error(radio_xmit_time_err);

                radio_state = error_rs;
            }

        break;

        case freq_set_rs:

            //It takes ~314 us to change the frequency
            RF_cmdFs.frequency = radio_freq.freq;
            RF_cmdFs.fractFreq = radio_freq.frac;

            radio_start_time = systick;

            command_done = false;

            HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_CMDR) = (uint32_t)&RF_cmdFs; //Configure the frequency synthesizer

            radio_state = freq_wait_rs;

        break;

        case freq_wait_rs:

            if (command_done)
            {
                /*
                led_set_amber(0x01 & current_channel_index_get());
                led_set_red(0x02 & current_channel_index_get());
*/
                radio_state = recv_set_rs;
            }
            else if ((systick - radio_start_time) >= RADIO_CMD_TIMEOUT)
            {
                report_error(radio_init_prop_div_err);

                break;
            }

        break;


        case process_cmd_rs:
        {
            if (CMD_RESULT_SUCCESS == link_recv(command_rx, *rx_packet_len))
            {
                packet_recv = false;

                radio_state = recv_set_rs;
            }
            else if ((systick - radio_start_time) >= get_radio_timeout())
            {
                report_error(radio_cmd_proc_time_err);

                radio_state = error_rs;
            }
        }
        break;

        case power_off_wait_rs:

            if (PRCMPowerDomainStatus(PRCM_DOMAIN_RFCORE) != PRCM_DOMAIN_POWER_ON)
            {
                PRCMPowerDomainOn(PRCM_DOMAIN_RFCORE);

                radio_state = power_on_wait_rs;
            }

        break;

        case power_on_wait_rs:

            if (PRCMPowerDomainStatus(PRCM_DOMAIN_RFCORE) == PRCM_DOMAIN_POWER_ON)
            {
                radio_init();

                RFCDoorbellSendTo(CMDR_DIR_CMD(CMD_START_RAT)); //Start the radio timer (RAT)

                radio_start_time = systick;

                radio_state = rat_wait_rs;
            }

        break;

        case error_rs:

            report_error(radio_err);

            //TODO: shutting down and reinitializing the radio is using a pretty big hammer.
            //In the future maybe try resetting it, or handling things in a more graceful way first.
            PRCMPowerDomainOff(PRCM_DOMAIN_RFCORE);

            radio_state = power_off_wait_rs;

        break;
    }
}

/*
 * Used by Link to send radio messages. Only Link should call this function.
 *
 * Copies the buff into the radio TX buff, and lets the radio know data is ready to be sent.
 *
 * Returns busy if data is already waiting to be sent.
 */
int8_t radio_send(uint8_t* buff, uint16_t buff_len)
{
    if (send_ready)
    {
        return CMD_RESULT_BUSY;
    }

    packet_tx[0] = (uint8_t)buff_len;
    packet_tx[1] = (uint8_t)(buff_len >> 8);

    memcpy(&packet_tx[2], buff, buff_len);

    RF_cmdPropTxAdv.pktLen = buff_len + LENGTH_LEN;
    RF_cmdPropTxAdv.pPkt = packet_tx;

    send_ready = true;

    return CMD_RESULT_SUCCESS;
}


/*
 * returns true is the radio is free. False if it is busy.
 */
inline bool get_radio_status(void)
{
    if (send_ready || recv_busy || packet_recv)
    {
        return false;
    }

    return true;
}

/*
 * Put the radio in RX mode.
 */
int8_t radio_recv_set(void)
{
    memset(packet_rx, 0, sizeof(packet_rx));    //Clear the packet_rf buffer

    entry = (rfc_dataEntry_t *)&packet_rx[0];   //Configure a new RF data entry if it isn't already configured.
    entry->config.type = DATA_ENTRY_TYPE_GEN;
    entry->config.lenSz = 0;
    entry->length = PACKET_BUFF_LEN;

    RF_cmdPropRxAdv.pQueue    = &g_data_queue;  //Configure a proprietary RX advanced command

    entry->status = DATA_ENTRY_PENDING;
    g_data_queue.pCurrEntry = &packet_rx[0];    //Put incoming bytes in the packet_rx buffer
    g_data_queue.pLastEntry = NULL;             //Use a circular buffer

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_RX_OK;
    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_RX_NOK;

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIEN) = (RFC_DBELL_RFCPEIFG_RX_OK | RFC_DBELL_RFCPEIFG_RX_NOK);   //Set the RX OK and not OK interrupts. One of them should trigger whenever we receive a packet.

    packet_recv = false;    //Clear our flags just in case they aren't already
    command_done = false;

    RFCDoorbellSendTo((uint32_t)&RF_cmdPropRxAdv);  //Send the command

    return CMD_RESULT_SUCCESS;
}

/*
 * tells the radio to switch to the passed in frequency as soon as it is able.
 */
void radio_set_freq(frequency_t new_freq)
{
    radio_freq = new_freq;

    freq_set_flag = true;
}


void load_radio_bitrate(void)
{
    load_bitrate_flag = true;
}

frequency_t get_current_freq(void)
{
    frequency_t frequency;

    frequency.freq = RF_cmdFs.frequency;
    frequency.frac = RF_cmdFs.fractFreq;

    return frequency;
}

int8_t get_rssi(void)
{
    return rssi;
}

/*
 * Blocks and sends data directly without waiting.
 *
 * Note: This function is for testing only. Do not use it with the real code.
 */
int8_t radio_send_blocking(uint8_t* buff, uint16_t buff_len)
{
    memcpy(packet_tx, buff, buff_len);

    RF_cmdPropTxAdv.pktLen = buff_len;
    RF_cmdPropTxAdv.pPkt = packet_tx;

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_COMMAND_DONE;
    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIEN) = RFC_DBELL_RFCPEIFG_COMMAND_DONE;

    command_done = false;
    RFCDoorbellSendTo((uint32_t)&RF_cmdPropTxAdv);

    while (!command_done)
    {
        __asm__(" NOP");
    }

    return CMD_RESULT_SUCCESS;
}


/*
 * Blocks, forces the radio into RX mode, and waits for data to arrive.
 *
 * Note: This function is for testing only. Do not use it with the real code.
 */
int8_t radio_recv_blocking(void)
{
    rfc_dataEntry_t* entry;

    memset(packet_rx, 0, sizeof(packet_rx));

    entry = (rfc_dataEntry_t *)&packet_rx[0];
    entry->config.type = DATA_ENTRY_TYPE_GEN;
    entry->config.lenSz = 2;
    entry->length = PACKET_BUFF_LEN;

    RF_cmdPropRxAdv.pQueue    = &g_data_queue;

    entry->status = DATA_ENTRY_PENDING;
    g_data_queue.pCurrEntry = &packet_rx[0];
    g_data_queue.pLastEntry = NULL;             //Circular buffer

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_RX_OK;
    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_RX_NOK;

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIEN) = (RFC_DBELL_RFCPEIFG_RX_OK | RFC_DBELL_RFCPEIFG_RX_NOK);

    packet_recv = false;
    command_done = false;
    RFCDoorbellSendTo((uint32_t)&RF_cmdPropRxAdv);

    while (!packet_recv)
    {
       __asm__(" NOP");
    }
    return 1;
}

radio_stats_t radio_stats_get(void)
{
    return radio_stats;
}

void radio_stats_clear(void)
{
    radio_stats.count_sent = 0;
    radio_stats.count_recv = 0;
}


void load_bitrate(void)
{
    rfTrim_t rfTrim;
    RF_Mode RF_prop;
    radio_bitrate_t radio_bitrate = radio_bitrate_get();

    if (bps_625_br == radio_bitrate)
    {
        RF_prop = RF_prop_bps_625;
    }
    else if ( (kbps_2p5_br == radio_bitrate) ||
              (kbps_5_br == radio_bitrate) )
    {
        RF_prop = RF_prop_kbps_2p5_5;
    }
    else
    {
        RF_prop = RF_prop_kbps_50;
    }

    //Apply the necessary patch functions
    if (RF_prop.cpePatchFxn != NULL)
        RF_prop.cpePatchFxn();
    if (RF_prop.mcePatchFxn != NULL)
        RF_prop.mcePatchFxn();
    if (RF_prop.rfePatchFxn != NULL)
        RF_prop.rfePatchFxn();


    if (bps_625_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x0;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x14;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x199A;
        RF_cmdPropRadioDivSetup.rxBw = 0x20;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x5;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x0;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x8;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_bps_625;

        radio_timeout = RADIO_TIMEOUT_625;
    }
    else if (kbps_2p5_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x14;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x3333;
        RF_cmdPropRadioDivSetup.rxBw = 0x21;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x2;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x0;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x8;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_2p5;

        radio_timeout = RADIO_TIMEOUT_2p5k;
    }
    else if (kbps_5_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x14;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x333;
        RF_cmdPropRadioDivSetup.rxBw = 0x21;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x2;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x0;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x8;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_5;

        radio_timeout = RADIO_TIMEOUT_5k;
    }
    else if (kbps_50_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x64;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x8000;
        RF_cmdPropRadioDivSetup.rxBw = 0x24;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x4;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x1;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x0;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_50;

        radio_timeout = RADIO_TIMEOUT_50k;
    }
    else if (kbps_200_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x118;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x20000;
        RF_cmdPropRadioDivSetup.rxBw = 0x29;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x4;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x1;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x0;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_200;

        radio_timeout = RADIO_TIMEOUT_200k;
    }
    else if (kbps_300_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x1A4;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0x5;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x10000;
        RF_cmdPropRadioDivSetup.rxBw = 0x2C;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x4;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x1;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x0;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_300_400;

        radio_timeout = RADIO_TIMEOUT_300k;
    }
    else if (kbps_400_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x230;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x40000;
        RF_cmdPropRadioDivSetup.rxBw = 0x2C;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x4;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x1;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x0;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_300_400;

        radio_timeout = RADIO_TIMEOUT_400k;
    }
    else if (kbps_500_br == radio_bitrate)
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x2BC;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0x6;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x20000;
        RF_cmdPropRadioDivSetup.rxBw = 0x2F;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x4;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x1;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x0;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_500;

        radio_timeout = RADIO_TIMEOUT_500k;
    }
    else //Default to 50 kbps
    {
        RF_cmdPropRadioDivSetup.modulation.modType = 0x1;
        RF_cmdPropRadioDivSetup.modulation.deviation = 0x64;
        RF_cmdPropRadioDivSetup.symbolRate.preScale = 0xF;
        RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0x8000;
        RF_cmdPropRadioDivSetup.rxBw = 0x24;
        RF_cmdPropRadioDivSetup.preamConf.nPreamBytes = 0x4;
        RF_cmdPropRadioDivSetup.formatConf.nSwBits = 0x20;
        RF_cmdPropRadioDivSetup.formatConf.bMsbFirst = 0x1;
        RF_cmdPropRadioDivSetup.formatConf.fecMode = 0x0;
        RF_cmdPropRadioDivSetup.pRegOverride = pOverrides_kbps_50;

        radio_timeout = RADIO_TIMEOUT_50k;
    }

    radio_freq = current_freq_get();    //Get the current frequency from the link layer

    RF_cmdPropRadioDivSetup.centerFreq = radio_freq.freq;   //Set the radio frequency in the command we will use to set-up the radio

    //Trim (calibrate?) the radio transceiver
    RFCRTrim((rfc_radioOp_t*)&RF_cmdPropRadioDivSetup);
    RFCRfTrimRead((rfc_radioOp_t*)&RF_cmdPropRadioDivSetup, &rfTrim);
    RFCRfTrimSet(&rfTrim);

    RFCDoorbellSendTo((uint32_t)&RF_cmdPropRadioDivSetup);  //Set the radio in proprietary RF mode
}

inline uint32_t get_radio_timeout(void)
{
    return radio_timeout;
}

/*
 * This ISR handles interrupts from the RF core command and packet engine.
 *
 * We primarily use the interrupt to know when a radio command (i.e. TX or RX) has finished.
 */
void radio_isr(void)
{
    if (HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) & RFC_DBELL_RFCPEIFG_RX_OK)
    {
        HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_RX_OK;

        packet_recv = true;     //We successfully received a packet.
        recv_busy = false;      //Set this false, so we know we can use the radio again.

    }
    else if (HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) & RFC_DBELL_RFCPEIFG_RX_NOK)
    {
        HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~RFC_DBELL_RFCPEIFG_RX_NOK;

        recv_busy = false;      //Set this false, so we know we can use the radio again.
    }
    else if (HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) & RFC_DBELL_RFCPEIFG_COMMAND_DONE ||
             HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) & RFC_DBELL_RFCPEIFG_LAST_COMMAND_DONE  )
    {
        HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = ~(RFC_DBELL_RFCPEIFG_COMMAND_DONE | RFC_DBELL_RFCPEIFG_LAST_COMMAND_DONE);

        command_done = true;
    }
    else
    {
        HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFCPEIFG) = 0x00000000;
    }
}

/*
 * This ISR handles interrupts from the RF core hardware.
 *
 * We primarily use the interrupt to know when a packet reception has started.
 */
void radio_hw_isr(void)
{
    if (HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFHWIFG) & RFC_DBELL_RFHWIFG_MDMSOFT)
    {
        //Modem synch word detected, which means we have started to receive a packet.
        //Set receive busy, so we know not to interrupt the radio until the receive completes.
        recv_busy = true;
    }

    HWREG(RFC_DBELL_NONBUF_BASE + RFC_DBELL_O_RFHWIFG) = 0x00000000;    //Clear all radio hardware module interrupt flags
    IntPendClear(INT_RFC_HW_COMB);      //TODO: Why is this necessary?
}


