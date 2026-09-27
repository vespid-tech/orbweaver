/*
 * Orbweaver - spi.c
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
#include <string.h>

#include "inc/hw_ssi.h"
#include "driverlib/ssi.h"
#include "driverlib/prcm.h"
#include "driverlib/ioc.h"

#include "clocks.h"
#include "cmd_proc.h"
#include "radio.h"
#include "spi.h"

/*
 * MISO->DIO8
 * MOSI->DIO9
 * SCLK->DIO10
 * CS->DIO11
 */

#define UDMA_O_REQDONE_SSI0_TX   0x00000010

#define SPI_MSG_TIMEOUT 5

typedef enum
{
    void_spi = 0,
    idle_spi,
    recv_msg_spi,
    process_cmd_spi,
    wait_for_master_spi,
    send_msg_spi
}spi_state_t;

#define SPI_BUFF_LEN   257

static bool spi_send_flag = false;

static uint8_t buff_spi_recv[SPI_BUFF_LEN];
static uint8_t buff_spi_send[SPI_BUFF_LEN];

static system_cmd_header_t* header_recv = (system_cmd_header_t*)&buff_spi_recv[1];

void data_ready(bool value);
void reset_spi_rx(void);
void spi_0_isr(void);

void spi_init(void)
{
    IOCPortConfigureSet(IOID_18, IOC_PORT_GPIO, IOC_NO_IOPULL | IOC_IOMODE_OPEN_DRAIN_NORMAL);  //SPI busy pin. This is pulled up and also used by the Pi to activate the bootloader, so make sure it's open drain.
    IOCPortConfigureSet(IOID_19, IOC_PORT_GPIO, IOC_NO_IOPULL | IOC_IOMODE_OPEN_DRAIN_NORMAL);  //Data ready pin

    HWREG(GPIO_BASE + GPIO_O_DOE31_0) |= GPIO_DOE31_0_DIO18;        //Enable DIO18 (SPI busy) as an output
    HWREG(GPIO_BASE + GPIO_O_DOE31_0) |= GPIO_DOE31_0_DIO19;        //Enable DIO19 (data ready) as an output

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO18;      //Set it high. Set it low when SPI is busy.
    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO19;      //Set it high. Set it low when data is ready.

    IOCPinTypeSsiSlave(SSI0_BASE, IOID_9, IOID_8, IOID_11, IOID_10);

    SSIConfigSetExpClk(SSI0_BASE, 48000000, SSI_FRF_MOTO_MODE_1, SSI_MODE_SLAVE, 1000000, 8);   //Set the bus to operate at 1 MHz in slave mode.

    HWREG(UDMA0_BASE + UDMA_O_CFG) |= UDMA_CFG_MASTERENABLE;            //Enable the DMA if it isn't already

    HWREG(UDMA0_BASE + UDMA_O_CTRL) = (uint32_t)channelControlTable;    //Point to the DMA channel control table, if we aren't already pointing at it

    uDMAChannelAttributeDisable(UDMA0_BASE, UDMA_CHAN_SSI0_RX, UDMA_ATTR_ALTSELECT | UDMA_ATTR_USEBURST | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK);
    uDMAChannelAttributeDisable(UDMA0_BASE, UDMA_CHAN_SSI0_TX, UDMA_ATTR_ALTSELECT | UDMA_ATTR_USEBURST | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK);

    uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_SSI0_RX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_4);  //Set up the DMA for SPI RX
    uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_SSI0_RX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (void*)(SSI0_BASE + SSI_O_DR), (void*)buff_spi_recv, SPI_BUFF_LEN);    //Set up the DMA to transfer the max possible number of bytes (SPI_BUFF_LEN)

    uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_SSI0_TX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_4);  //Set up the DMA for SPI TX

    uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_SSI0_RX);
    SSIDMAEnable(SSI0_BASE, SSI_DMA_RX);

    IntEnable(INT_SSI0_COMB);   //TODO: Why do we need this? It doesn't seem like we are even using the ISR.

    IntMasterEnable();

    SSIEnable(SSI0_BASE);
}


void spi_service(void)
{
    static spi_state_t spi_state = void_spi;
    static uint32_t dma_bytes_left = 0;
    static uint32_t spi_start_time = 0;

    switch (spi_state)
    {
        case void_spi:

            if (get_config_ready()) //Don't continue unless we know we have a good configuration to use.
            {
                spi_state = idle_spi;
            }

        break;

        case idle_spi:

            dma_bytes_left = uDMAChannelSizeGet(UDMA0_BASE, UDMA_CHAN_SSI0_RX | UDMA_PRI_SELECT);   //Get the number of bytes remaining to transfer

            if  (spi_send_flag)
            {
                spi_start_time = systick;

                spi_state = wait_for_master_spi;
            }
            else if (START_BYTE == buff_spi_recv[0])     //If these two numbers are not equal, that means a transfer has started and maybe finished.
            {
                HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO18; //Set the busy pin low. TODO: this should be set as soon as the master brings the CS low just in case an entire command is received before we get here.

                spi_start_time = systick;   //Set the timeout start time

                spi_state = recv_msg_spi;   //Receive the rest of the message
            }
            else if ((START_BYTE != buff_spi_recv[0]) && (dma_bytes_left < SPI_BUFF_LEN))
            {
                reset_spi_rx();    //There might have been a byte left over from the last transmit. Just reset the SPI.
            }

        break;

        case recv_msg_spi:

            if (START_BYTE == buff_spi_recv[0])
            {
                if (verify_header(header_recv))
                {
                    if (STOP_BYTE == buff_spi_recv[header_recv->length + sizeof(uint8_t)])  //sizeof is the start byte
                    {
                        spi_start_time = systick;

                        spi_state = process_cmd_spi;
                    }
                }
            }

            if ((systick - spi_start_time) >= SPI_MSG_TIMEOUT)
            {
                reset_spi_rx();

                spi_state = idle_spi;
            }

        break;

        case process_cmd_spi:

            if (CMD_RESULT_SUCCESS == process_cmd(header_recv))
            {
                buff_spi_recv[header_recv->length + sizeof(START_BYTE)] = 0x00;     //Clear the stop byte, so we don't accidentally check it next time we receive a message.

                reset_spi_rx();

                spi_state = idle_spi;
            }
            else if ((systick - spi_start_time) >= get_radio_timeout())
            {
                reset_spi_rx();

                spi_state = idle_spi;
            }

        break;

        case wait_for_master_spi:

            if (!(HWREG(GPIO_BASE + GPIO_O_DIN31_0) & GPIO_DIN31_0_DIO11))  //Wait for the SS line to be pulled low by the master
            {
                data_ready(false);          //The transfer has started. Set the data ready line high.

                spi_state = send_msg_spi;   //Go wait for the transfer to finish
            }
            else if ((systick - spi_start_time) >= SPI_MSG_TIMEOUT)
            {
                SSIDMADisable(SSI0_BASE, SSI_DMA_TX);

                uDMAChannelDisable(UDMA0_BASE, UDMA_CHAN_SSI0_TX);

                data_ready(false);

                spi_send_flag = false;

                spi_state = idle_spi;
            }

        break;

        case send_msg_spi:

            if (HWREG(SSI0_BASE + SSI_O_SR) & SSI_SR_TFE)   //Check if the transmit FIFO is empty
            {
                spi_send_flag = false;      //The FIFO is empty, which means the transfer has completed.

                reset_spi_rx();

                spi_state = idle_spi;
            }
            else if ((systick - spi_start_time) >= SPI_MSG_TIMEOUT)
            {
                spi_send_flag = false;

                data_ready(false);

                SSIDMADisable(SSI0_BASE, SSI_DMA_TX);

                uDMAChannelDisable(UDMA0_BASE, UDMA_CHAN_SSI0_TX);

                reset_spi_rx();

                spi_state = idle_spi;
            }

        break;
    }
}


/*
 * Sets up a SPI transfer, and set the data ready line low to let the master know
 * a message is waiting.
 *
 * This function adds start and stop bytes to whatever is being sent.
 *
 * This function does not block.
 */
int8_t spi_send(uint8_t* buff, uint8_t buff_len)
{
    uint16_t send_len;

    if(spi_send_flag)
    {
        return CMD_RESULT_BUSY;
    }

    spi_send_flag = true;

    buff_spi_send[0] = START_BYTE;

    memcpy(&buff_spi_send[sizeof(START_BYTE)], buff, buff_len);

    buff_spi_send[buff_len + sizeof(START_BYTE)] = STOP_BYTE;

    send_len = buff_len + sizeof(START_BYTE) + sizeof(STOP_BYTE);

    uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_SSI0_TX);

    uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_SSI0_TX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (void*)buff_spi_send, (void*)(SSI0_BASE + SSI_O_DR), send_len);

    SSIDMAEnable(SSI0_BASE, SSI_DMA_TX);

    data_ready(true);

    return CMD_RESULT_SUCCESS;
}


/*
 * Not currently used.
 */
void spi_write_byte_blocking(uint8_t write_byte)
{
    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO11;

    SSIDataPut(SSI0_BASE, write_byte);

    while(SSIBusy(SSI0_BASE));

    //SSIDataGet(SSI0_BASE, &read_byte);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO11;
}


/*
 * This function is for testing only. Do not use it in the actual code.
 */
void spi_write_blocking(uint8_t* write_buff, uint8_t write_len)
{
    int i;

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO11;

    for (i=0; i<write_len; i++)
    {
        SSIDataPut(SSI0_BASE, write_buff[i]);

        while(SSIBusy(SSI0_BASE));
    }

    //SSIDataGet(SSI0_BASE, &read_byte);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO11;
}

/*
 * Sets the data ready line.
 *
 * When the line is low, it signals to the master that a message is waiting to be read.
 */
inline void data_ready(bool value)
{
    if (value)
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO19;
    }
    else
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO19;
    }
}

/*
 * Used to reset the SPI bus after a command reception completes.
 */
inline void reset_spi_rx(void)
{
    memset(buff_spi_recv, 0x00, sizeof(system_cmd_header_t) + sizeof(START_BYTE));  //Clear the start byte and header, so we don't accidentally check it next time we receive a message.

    uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_SSI0_RX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (void*)(SSI0_BASE + SSI_O_DR), (void*)buff_spi_recv, SPI_BUFF_LEN);

    uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_SSI0_RX);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO18;  //Let the master know we are no longer busy
}


void spi_0_isr(void)
{
    uint32_t int_stat;

    int_stat = HWREG(UDMA0_BASE + UDMA_O_REQDONE);

    if (int_stat & UDMA_O_REQDONE_SSI0_TX)
    {
        SSIDMADisable(SSI0_BASE, SSI_DMA_TX);
    }

    uDMAIntClear(UDMA0_BASE, int_stat); //We have to disable before we clear or we'll get a second TX interrupt.
}

