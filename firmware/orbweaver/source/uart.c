/*
 * Orbweaver - uart.c
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

#include "inc/hw_udma.h"
#include "driverlib/ioc.h"
#include "driverlib/udma.h"
#include "driverlib/uart.h"

#include "cmd_proc.h"
#include "radio.h"
#include "clocks.h"
#include "uart.h"


#define UDMA_O_REQDONE_UART0_TX   0x00000004

#define uart_BUFF_LEN 255

const uint8_t START_BYTE = 0xAA;
const uint8_t STOP_BYTE  = 0xCC;

//Note: at 115200 baud, a 256 byte message should take ~18 ms to receive.
#define MSG_TIMEOUT 36

typedef enum
{
    void_ss = 0,
    idle_ss,
    recv_header_ss,
    recv_param_ss,
    wait_for_stop_ss,
    process_cmd_ss
}uart_state_t;

static bool uart_busy = false;

static uint8_t recv_byte;
static uint8_t buff_recv[uart_BUFF_LEN + sizeof(START_BYTE) + sizeof(STOP_BYTE)];
static uint8_t buff_send[uart_BUFF_LEN + sizeof(START_BYTE) + sizeof(STOP_BYTE)];

static system_cmd_header_t* header_recv = (system_cmd_header_t*)buff_recv;

static uint8_t buff_recv_index = 0;

static uint32_t msg_start_time = 0;

/*
 * Initialize the serial port and DMA.
 */
void uart_init(void)
{
    HWREG(IOC_BASE + IOC_O_IOCFG2) = IOC_IOCFG2_IE | IOC_IOCFG2_PULL_CTL_DIS | IOC_IOCFG2_PORT_ID_UART0_RX;     //Make DIO2 UART0 RX
    HWREG(IOC_BASE + IOC_O_IOCFG3) = IOC_IOCFG3_PULL_CTL_DIS | IOC_IOCFG3_PORT_ID_UART0_TX;                     //Make DIO3 UART0 TX
    HWREG(IOC_BASE + IOC_O_IOCFG28) = IOC_IOCFG2_IE | IOC_IOCFG2_PULL_CTL_DIS | IOC_IOCFG2_PORT_ID_UART0_CTS;   //Make DIO28 UART0 CTS
    HWREG(IOC_BASE + IOC_O_IOCFG29) = IOC_IOCFG3_PULL_CTL_DIS | IOC_IOCFG2_PORT_ID_UART0_RTS;                   //Make DIO29 UART0 RTS

/*
    HWREG(UART0_BASE + UART_O_IBRD) = 0x0000001A;   //Set the UART to run at 115200 baud
    HWREG(UART0_BASE + UART_O_FBRD) = 0x00000003;   //Set the UART to run at 115200 baud
*/

    HWREG(UART0_BASE + UART_O_IBRD) = 0x00000006;   //Set the UART to run at 460800 baud
    HWREG(UART0_BASE + UART_O_FBRD) = 0x00000021;   //Set the UART to run at 460800 baud

    HWREG(UART0_BASE + UART_O_LCRH) = UART_LCRH_WLEN_8; //Use 8 bit words
    HWREG(UART0_BASE + UART_O_LCRH) |= UART_LCRH_FEN;   //Enable the FIFOs

    HWREG(UART0_BASE + UART_O_CTL) = UART_CTL_UARTEN;   //Enable the UART
    //HWREG(UART0_BASE + UART_O_CTL) |= UART_CTL_RXE | UART_CTL_TXE;
    HWREG(UART0_BASE + UART_O_CTL) |= UART_CTL_RXE | UART_CTL_TXE | UART_CTL_CTSEN | UART_CTL_RTSEN;  //Enable UART TX, RX, CTS, and RTS

    HWREG(UART0_BASE + UART_O_CTL) |= UART_CTL_RTS;

    HWREG(UDMA0_BASE + UDMA_O_CFG) |= UDMA_CFG_MASTERENABLE;        //Enable the DMA

    HWREG(UDMA0_BASE + UDMA_O_CTRL) = (uint32_t)channelControlTable;    //Point to the DMA channel control table

    uDMAChannelAttributeDisable(UDMA0_BASE, UDMA_CHAN_UART0_TX, UDMA_ATTR_ALTSELECT | UDMA_ATTR_USEBURST | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK);    //Clear channel attributes. Probably not necessary, but do it just in case.

    uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_UART0_TX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_4); //Set up the DMA for UART transmits

    IntEnable(INT_UART0_COMB);  //Enable UART interrupts
}


void uart_service(void)
{
    static uart_state_t uart_state = void_ss;

    switch (uart_state)
    {
        case void_ss:

            if (get_config_ready()) //Don't continue unless we know we have a good configuration to use.
            {
                buff_recv_index = 0;

                uart_state = idle_ss;
            }

        break;

        case idle_ss:

            if (!(HWREG(UART0_BASE + UART_O_FR) &= UART_FR_RXFE))   //Check for any received bytes.
            {
                recv_byte = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.

                if (START_BYTE == recv_byte)
                {
                    msg_start_time = systick;   //Set a start time. If it takes too long to receive a whole message, we'll bail out.

                    buff_recv_index = 0;

                    uart_state = recv_header_ss;
                }
                else
                {
                    report_error(uart_start_err);
                }
            }

        break;

        case recv_header_ss:

            while (!(HWREG(UART0_BASE + UART_O_FR) &= UART_FR_RXFE))   //Check for any received bytes.
            {
                buff_recv[buff_recv_index] = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.

                buff_recv_index++;

                if (buff_recv_index >= sizeof(system_cmd_header_t))
                {
                    if (verify_header(header_recv))
                    {
                        if (header_recv->length > CMD_LEN_MIN)
                        {
                            uart_state = recv_param_ss;
                        }
                        else
                        {
                            uart_state = wait_for_stop_ss;
                        }
                    }
                    else
                    {
                        report_error(uart_header_err);

                        uart_state = void_ss;
                    }

                    break;
                }
            }

            if ((systick - msg_start_time) >= MSG_TIMEOUT)
            {
                report_error(uart_header_time_err);

                uart_state = void_ss;
            }

        break;

        case recv_param_ss:

            while (!(HWREG(UART0_BASE + UART_O_FR) &= UART_FR_RXFE))   //Check for any received bytes.
            {
                buff_recv[buff_recv_index] = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.

                buff_recv_index++;

                if (buff_recv_index >= header_recv->length)
                {
                    uart_state = wait_for_stop_ss;

                    break;
                }
            }

            if ((systick - msg_start_time) >= MSG_TIMEOUT)
            {
                report_error(uart_param_err);

                uart_state = void_ss;
            }

        break;

        case wait_for_stop_ss:

            if (!(HWREG(UART0_BASE + UART_O_FR) &= UART_FR_RXFE))   //Check for any received bytes.
            {
                recv_byte = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.

                if (STOP_BYTE == recv_byte)
                {
                    buff_recv_index = 0;

                    msg_start_time = systick;   //When processing, we might have to wait for a radio transmission to finish.

                    uart_state = process_cmd_ss;
                }
                else
                {
                    report_error(uart_stop_err);
                }
            }
            else if ((systick - msg_start_time) >= MSG_TIMEOUT)
            {
                report_error(uart_stop_time_err);

                uart_state = void_ss;
            }

        break;

        case process_cmd_ss:
        {
            if (CMD_RESULT_SUCCESS == process_cmd((system_cmd_header_t*)buff_recv))
            {
                uart_state = idle_ss;
            }
            else if ((systick - msg_start_time) >= get_radio_timeout())
            {
                report_error(uart_cmd_proc_err);

                uart_state = void_ss;
            }
        }
        break;
    }
}


/*
 * Copies the buffer into the UART send buffer and starts a DMA transfer.
 *
 * This function will add start and stop bytes to whatever is passed in.
 *
 * This function does not block.
 */
int8_t uart_send(uint8_t* buff, uint8_t buff_len)
{
    uint16_t send_len;

    if(uart_busy)
    {
        return CMD_RESULT_BUSY;
    }

    uart_busy = true;

    buff_send[0] = START_BYTE;

    memcpy(&buff_send[sizeof(START_BYTE)], buff, buff_len);

    buff_send[buff_len + sizeof(START_BYTE)] = STOP_BYTE;

    send_len = buff_len + sizeof(START_BYTE) + sizeof(STOP_BYTE);

    uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_UART0_TX);

    uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_UART0_TX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (void*)buff_send, (void*)(UART0_BASE + UART_O_DR), send_len);

    UARTDMAEnable(UART0_BASE, UART_DMA_TX);

    return CMD_RESULT_SUCCESS;
}


void uart_isr(void)
{
    uint32_t int_stat;

    int_stat = HWREG(UDMA0_BASE + UDMA_O_REQDONE);

    if (int_stat & UDMA_O_REQDONE_UART0_TX)
    {
        UARTDMADisable(UART0_BASE, UART_DMA_TX);

        uDMAIntClear(UDMA0_BASE, int_stat);

        uart_busy = false;
    }
}

/*
 * This isn't currently used.
 */
void udma_isr(void)
{
    UARTDMADisable(UART0_BASE, UART_DMA_TX);

    uDMAIntClear(UDMA0_BASE, UDMA_O_REQDONE_UART0_TX);
}
