/*
 * Orbweaver - flash.c
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

#include "./inc/hw_ssi.h"
#include <./driverlib/ssi.h>
#include <./driverlib/prcm.h>
#include <./driverlib/ioc.h>

#include "clocks.h"
#include "cmd_proc.h"
#include "flash.h"

/*
 * MISO->DIO26
 * MOSI->DIO23
 * SCLK->DIO24
 * CS->DIO25
 */

/*
 * External flash memory map:
 *
 * total flash size: 1 MByte
 * size available to application: 960 kB
 * page size: 256 Bytes
 *
 * 0x00000000   Config Data start (page 0 start)
 * 0x000000FF   Config Data end (page 0 end)
 * 0x00000100   Unused Start (page 1 start)
 * 0x0000FF00   Unused End (page 254 end)
 * 0x0000FF00   Boot command (page 255 start)
 * 0x0000FF04   application size
 * 0x0000FF08   application CRC
 * 0x0000FFFF   page 255 end and block 0 end
 * 0x00010000   application start (Block 1 start)
 * 0x000FFFFF   flash end and block 15 end
 *
 */

#define FLASH_BUFF_HEADER_LEN   4
#define FLASH_BUFF_LEN          (FLASH_BUFF_HEADER_LEN + FLASH_PAGE_SIZE)

#define FLASH_STAT_0_BIT_WIP    0x01

#define FLASH_BUFF_OFFSET_CMD    0
#define FLASH_BUFF_OFFSET_ADDR_1 1
#define FLASH_BUFF_OFFSET_ADDR_2 2
#define FLASH_BUFF_OFFSET_ADDR_3 3
#define FLASH_BUFF_OFFSET_DATA   4

#define FLASH_CMD_PAGE_PROGRAM      0x02
#define FLASH_CMD_READ              0x03
#define FLASH_CMD_READ_STAT_0       0x05
#define FLASH_CMD_WRITE_ENABLE      0x06
#define FLASH_CMD_WRITE_DISABLE     0x04
#define FLASH_CMD_PAGE_ERASE        0x81
#define FLASH_CMD_BLOCK_ERASE       0xD8
#define FLASH_CMD_HALF_BLOCK_ERASE  0x52
#define FLASH_CMD_CHIP_ERASE        0x60

#define FLASH_TIMEOUT   600 //The flash chip needs 300 ms to begin operation after power on

#define DUMMY_BYTE   0xFF

#define UDMA_O_REQDONE_SSI1_RX   0x00010000
#define UDMA_O_REQDONE_SSI1_TX   0x00020000

typedef enum
{
    void_fs = 0,
    idle_fs,
    read_stat_0_fs,
    read_stat_0_wait_fs,
    write_enable_fs,
    write_enable_wait_fs,
    page_program_wait_fs,
    transfer_wait_fs
}flash_state_t;

/*
 * There is no SSI TX buffer empty interrupt, but there is a DMA finished interrupt.
 * However, when the DMA finishes, the SSI will still be sending bytes, so we have to
 * wait for the DMA to be finished, then wait for the SSI to not be busy. We can't just
 * set-up the DMA, then wait for the SSI to not be busy, because (although it is probably
 * unlikely), we might check the SSI to see if it is busy before the DMA has written the
 * first byte to it, so it will not be busy yet. Because of this we need both transfer_busy
 * and dma_finished in order to properly transmit data safely.
 */
static bool transfer_busy = false;
static bool dma_finished = false;
static bool write_busy = false;
static flash_erase_type_t flash_erase_type = void_fet;

static uint32_t flash_start_time = 0;

static uint8_t buff_flash_recv[FLASH_BUFF_LEN];     //Holds data that is read via SPI
static uint8_t buff_flash_send[FLASH_BUFF_LEN];     //Holds data to be sent via SPI

static uint8_t flash_write_buff[FLASH_BUFF_LEN];    //Holds data to be written to flash
static uint32_t flash_write_addr;
static uint8_t flash_write_len;
static uint8_t flash_write_offset;      //Used in case we write over the end of a page, which necessitates performing two page programs.

static uint32_t flash_write_addr_2;     //These 3 hold the values to use for a second page program
static uint32_t flash_write_len_2;
static uint8_t flash_write_offset_2;

static uint32_t dummy_read;

int8_t flash_read_stat_0(void);
void flash_page_program(uint32_t address, uint8_t* buff, uint32_t buff_len);
int8_t flash_start_erase(uint32_t address, flash_erase_type_t erase_type);
int8_t flash_write_enable(bool enable);
void flash_start_transfer(uint32_t length);
void flash_write_byte_blocking(uint32_t address, uint8_t byte);
uint8_t flash_read_byte_blocking(uint32_t address);
void flash_page_erase_blocking(uint32_t address);
void spi_1_isr(void);

void flash_init(void)
{
    IOCPinTypeSsiMaster(SSI1_BASE, IOID_26, IOID_23, IOID_UNUSED, IOID_24);

    IOCPortConfigureSet(IOID_25, IOC_PORT_GPIO, IOC_STD_OUTPUT);
    HWREG(GPIO_BASE + GPIO_O_DOE31_0) |= GPIO_DOE31_0_DIO25;
    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

    SSIDisable(SSI1_BASE);

    SSIConfigSetExpClk(SSI1_BASE, 48000000, SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 100000, 8);

    HWREG(UDMA0_BASE + UDMA_O_CFG) |= UDMA_CFG_MASTERENABLE;

    HWREG(UDMA0_BASE + UDMA_O_CTRL) = (uint32_t)channelControlTable;

    IntEnable(INT_SSI1_COMB);

    IntMasterEnable();

    SSIEnable(SSI1_BASE);
}

/*
 * To write: split into two buffers if the write is about to go over the end of the page.
 *
 * Check the ensure WIP is 0.
 * Write first buffer.
 * Write second buffer it is exists.
 * clear transfer busy flag
 */
void flash_service(void)
{
    static flash_state_t flash_state = void_fs;

    switch (flash_state)
    {
        case void_fs:
            flash_state = idle_fs;
        break;

        case idle_fs:

            if (write_busy || (void_fet != flash_erase_type))
            {
                flash_start_time = systick;

                flash_state = write_enable_fs;
            }
            else if (transfer_busy)
            {
                flash_start_time = systick;

                flash_state = transfer_wait_fs;
            }

        break;

        case write_enable_fs:

            if (FLASH_SUCCESS == flash_write_enable(true))
            {
                flash_start_time = systick;

                flash_state = write_enable_wait_fs;
            }
            else if ((systick - flash_start_time) >= FLASH_TIMEOUT)
            {
                write_busy = false;
                flash_erase_type = void_fet;

                flash_state = void_fs;
            }

        break;

        case write_enable_wait_fs:

            if (dma_finished && (!SSIBusy(SSI1_BASE)))
            {
                HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

                dma_finished = false;
                transfer_busy = false;

                if (void_fet != flash_erase_type)
                {
                    flash_start_erase(flash_write_addr, flash_erase_type);
                }
                else
                {
                    flash_page_program(flash_write_addr, &flash_write_buff[flash_write_offset], flash_write_len);
                }

                flash_start_time = systick;

                flash_state = page_program_wait_fs;
            }
            else if ((systick - flash_start_time) >= FLASH_TIMEOUT)
            {
                write_busy = false;
                flash_erase_type = void_fet;

                flash_state = void_fs;
            }

        break;

        case page_program_wait_fs:

            if (dma_finished && (!SSIBusy(SSI1_BASE)))
            {
                HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

                dma_finished = false;
                transfer_busy = false;

                flash_start_time = systick;

                flash_state = read_stat_0_fs;
            }
            else if ((systick - flash_start_time) >= FLASH_TIMEOUT)
            {
                write_busy = false;
                flash_erase_type = void_fet;

                flash_state = void_fs;
            }

        break;

        case read_stat_0_fs:

            if (FLASH_SUCCESS == flash_read_stat_0())
            {
                flash_start_time = systick;

                flash_state = read_stat_0_wait_fs;
            }
            else if ((systick - flash_start_time) >= FLASH_TIMEOUT)
            {
                write_busy = false;
                flash_erase_type = void_fet;

                flash_state = void_fs;
            }

        break;

        case read_stat_0_wait_fs:

            if (dma_finished && (!SSIBusy(SSI1_BASE)))
            {
                HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

                dma_finished = false;
                transfer_busy = false;

                if (buff_flash_recv[1] & FLASH_STAT_0_BIT_WIP)
                {
                    flash_state = read_stat_0_fs;   //Write or erase is still in progress. Poll again.
                }
                else
                {
                    if ( (0 != flash_write_len_2) && (0 != flash_write_offset_2) )
                    {
                        flash_write_addr = flash_write_addr_2;          //Write is complete, but we have a second write to do.
                        flash_write_offset = flash_write_offset_2;
                        flash_write_len = flash_write_len_2;

                        flash_state = write_enable_fs;
                    }
                    else
                    {
                        write_busy = false;             //Write or erase is complete
                        flash_erase_type = void_fet;

                        flash_state = idle_fs;
                    }
                }
            }
            else if ((systick - flash_start_time) >= FLASH_TIMEOUT)
            {
                write_busy = false;
                flash_erase_type = void_fet;

                flash_state = void_fs;
            }


        break;

        case transfer_wait_fs:

            if (dma_finished && (!SSIBusy(SSI1_BASE)))
            {
                HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

                dma_finished = false;
                transfer_busy = false;

                flash_state = idle_fs;
            }
            else if ((systick - flash_start_time) >= FLASH_TIMEOUT)
            {
                write_busy = false;
                flash_erase_type = void_fet;

                flash_state = void_fs;
            }

        break;
    }
}


int8_t flash_read_stat_0(void)
{
    if (transfer_busy)
    {
        return FLASH_BUSY;
    }


    buff_flash_send[0] = FLASH_CMD_READ_STAT_0;
    buff_flash_send[1] = DUMMY_BYTE;

    flash_start_transfer(2);

    return FLASH_SUCCESS;
}


bool get_wip_blocking(void)
{
    uint32_t stat_0;

    buff_flash_send[0] = FLASH_CMD_READ_STAT_0;
    buff_flash_send[1] = DUMMY_BYTE;

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    SSIDataPut(SSI1_BASE, FLASH_CMD_READ_STAT_0);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, DUMMY_BYTE);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &stat_0);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

    return (stat_0 & FLASH_STAT_0_BIT_WIP);
}

void flash_write_enable_blocking(bool enable)
{
    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    if (enable)
    {
        SSIDataPut(SSI1_BASE, FLASH_CMD_WRITE_ENABLE);
    }
    else
    {
        SSIDataPut(SSI1_BASE, FLASH_CMD_WRITE_DISABLE);
    }

    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;
}


int8_t flash_write_enable(bool enable)
{
    if (transfer_busy)
    {
        return FLASH_BUSY;
    }

    if (enable)
    {
        buff_flash_send[0] = FLASH_CMD_WRITE_ENABLE;
    }
    else
    {
       buff_flash_send[0] = FLASH_CMD_WRITE_DISABLE;
    }

    flash_start_transfer(1);

    return FLASH_SUCCESS;
}


int8_t flash_write(uint32_t address, uint8_t* buff, uint32_t buff_len)
{
    if (write_busy || transfer_busy || (void_fet != flash_erase_type))
    {
        return FLASH_BUSY;
    }

    if (buff_len > FLASH_PAGE_SIZE)
    {
        return FLASH_ERROR;
    }

    write_busy = true;

    flash_write_addr = address;
    memcpy(flash_write_buff, buff, buff_len);
    flash_write_len = buff_len;
    flash_write_offset = 0;

    return FLASH_SUCCESS;
}


int8_t flash_erase(uint32_t address, flash_erase_type_t erase_type)
{
    if (write_busy || transfer_busy || (void_fet != flash_erase_type))
    {
        return FLASH_BUSY;
    }

    flash_erase_type = erase_type;

    flash_write_addr = address;

    flash_write_len = 0;          //An erase is treated like a write. Just zero all these out.
    flash_write_addr_2 = 0;
    flash_write_len_2 = 0;
    flash_write_offset_2 = 0;

    return FLASH_SUCCESS;
}


void flash_page_program(uint32_t address, uint8_t* buff, uint32_t buff_len)
{
    uint32_t page_addr;
    uint32_t write_len;

    page_addr = (0x000000FF & address);

    if ((page_addr + buff_len) > FLASH_PAGE_SIZE)
    {
        write_len = FLASH_PAGE_SIZE - page_addr;
        flash_write_addr_2 = (address & 0xFFFFFF00) + FLASH_PAGE_SIZE;
        flash_write_len_2  = buff_len - write_len;
        flash_write_offset_2 = write_len;
    }
    else
    {
        write_len = buff_len;
        flash_write_addr_2 = 0;
        flash_write_len_2 = 0;
        flash_write_offset_2 = 0;
    }

    buff_flash_send[FLASH_BUFF_OFFSET_CMD]    = FLASH_CMD_PAGE_PROGRAM;
    buff_flash_send[FLASH_BUFF_OFFSET_ADDR_1] = (uint8_t)((address >> 16) & 0x000000FF);
    buff_flash_send[FLASH_BUFF_OFFSET_ADDR_2] = (uint8_t)((address >> 8) & 0x000000FF);
    buff_flash_send[FLASH_BUFF_OFFSET_ADDR_3] = (uint8_t)(address & 0x000000FF);

    memcpy(&buff_flash_send[FLASH_BUFF_OFFSET_DATA], buff, write_len);

    flash_start_transfer(FLASH_BUFF_HEADER_LEN + write_len);
}


int8_t flash_read(uint32_t address, uint32_t len)
{
    if (write_busy || transfer_busy|| (void_fet != flash_erase_type))
    {
        return FLASH_BUSY;
    }

    if (len > FLASH_PAGE_SIZE)
    {
       return FLASH_ERROR;
    }

    buff_flash_send[FLASH_BUFF_OFFSET_CMD]    = FLASH_CMD_READ;
    buff_flash_send[FLASH_BUFF_OFFSET_ADDR_1] = (uint8_t)((address >> 16) & 0x000000FF);
    buff_flash_send[FLASH_BUFF_OFFSET_ADDR_2] = (uint8_t)((address >> 8) & 0x000000FF);
    buff_flash_send[FLASH_BUFF_OFFSET_ADDR_3] = (uint8_t)(address & 0x000000FF);

    memset(&buff_flash_send[FLASH_BUFF_OFFSET_DATA], DUMMY_BYTE, len);

    flash_start_transfer(FLASH_BUFF_HEADER_LEN + len);

    return FLASH_SUCCESS;
}


int8_t flash_start_erase(uint32_t address, flash_erase_type_t erase_type)
{
    if (transfer_busy)
    {
        return FLASH_BUSY;
    }

    if (chip_fet == erase_type)
    {
        buff_flash_send[0] = FLASH_CMD_CHIP_ERASE;

        flash_start_transfer(1);

        return FLASH_SUCCESS;
    }
    else if (page_fet == erase_type)
    {
        buff_flash_send[0] = FLASH_CMD_PAGE_ERASE;
    }
    else if (half_block_fet == erase_type)
    {
        buff_flash_send[0] = FLASH_CMD_HALF_BLOCK_ERASE;
    }
    else if (block_fet == erase_type)
    {
        buff_flash_send[0] = FLASH_CMD_BLOCK_ERASE;
    }

    buff_flash_send[1] = (uint8_t)((address >> 16) & 0x000000FF);
    buff_flash_send[2] = (uint8_t)((address >> 8) & 0x000000FF);
    buff_flash_send[3] = (uint8_t)(address & 0x000000FF);

    flash_start_transfer(4);

    return FLASH_SUCCESS;
}


int8_t flash_get_data(uint8_t* buff, uint32_t length)
{
    if (write_busy || transfer_busy)
    {
        return FLASH_BUSY;  //Don't return anything while a transfer is in progress
    }

    memcpy(buff, &buff_flash_recv[FLASH_BUFF_OFFSET_DATA], length);

    return FLASH_SUCCESS;
}


void flash_start_transfer(uint32_t length)
{
    dma_finished = false;
    transfer_busy = true;

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_SSI1_TX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_4);
    uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_SSI1_TX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (void*)buff_flash_send, (void*)(SSI1_BASE + SSI_O_DR), length);

    uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_SSI1_RX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_4);
    uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_SSI1_RX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (void*)(SSI1_BASE + SSI_O_DR), (void*)buff_flash_recv, length);

    uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_SSI1_TX);
    uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_SSI1_RX);

    SSIDMAEnable(SSI1_BASE, SSI_DMA_RX);
    SSIDMAEnable(SSI1_BASE, SSI_DMA_TX);
}

int8_t flash_status_get(void)
{
    if (write_busy || transfer_busy || (void_fet != flash_erase_type))
    {
        return FLASH_BUSY;
    }

    return FLASH_SUCCESS;
}


void flash_read_blocking(uint8_t* read_buff, uint32_t address, uint32_t length)
{
    uint8_t addr_0;
    uint8_t addr_1;
    uint8_t addr_2;
    int i;

    addr_0 = (uint8_t)(address & 0x000000FF);
    addr_1 = (uint8_t)((address >> 8) & 0x000000FF);
    addr_2 = (uint8_t)((address >> 16) & 0x000000FF);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    SSIDataPut(SSI1_BASE, FLASH_CMD_READ);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_2);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_1);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_0);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    for (i=0; i<length; i++)
    {
        SSIDataPut(SSI1_BASE, DUMMY_BYTE);
        while(SSIBusy(SSI1_BASE));
        SSIDataGet(SSI1_BASE, (uint32_t*)&read_buff[i]);
    }

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;
}


/*
 * This function is for testing only. Do not use in the actual code.
 *
 * This function does not handle address roll-over if the write crosses
 * the end of the page.
 */
void flash_page_program_blocking(uint32_t address, uint8_t* write_buff, uint32_t write_len)
{
    int i;
    uint8_t addr_0;
    uint8_t addr_1;
    uint8_t addr_2;

    addr_0 = (uint8_t)(address & 0x000000FF);
    addr_1 = (uint8_t)((address >> 8) & 0x000000FF);
    addr_2 = (uint8_t)((address >> 16) & 0x000000FF);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    SSIDataPut(SSI1_BASE, FLASH_CMD_PAGE_PROGRAM);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_2);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_1);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_0);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    for (i=0; i<write_len; i++)
    {
        SSIDataPut(SSI1_BASE, write_buff[i]);
        while(SSIBusy(SSI1_BASE));
        SSIDataGet(SSI1_BASE, &dummy_read);
    }

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;
}


/*
 * This function is for testing only. Do not use in the actual code.
 *
 */
uint8_t flash_read_byte_blocking(uint32_t address)
{
    uint32_t read_word;
    uint8_t addr_0;
    uint8_t addr_1;
    uint8_t addr_2;

    addr_0 = (uint8_t)(address & 0x000000FF);
    addr_1 = (uint8_t)((address >> 8) & 0x000000FF);
    addr_2 = (uint8_t)((address >> 16) & 0x000000FF);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    SSIDataPut(SSI1_BASE, FLASH_CMD_READ);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_2);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_1);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_0);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, DUMMY_BYTE);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &read_word);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;

    return (uint8_t)read_word;
}


void flash_erase_blocking(uint32_t address, flash_erase_type_t flash_erase_type)
{
    uint8_t addr_0;
    uint8_t addr_1;
    uint8_t addr_2;

    addr_0 = (uint8_t)(address & 0x000000FF);
    addr_1 = (uint8_t)((address >> 8) & 0x000000FF);
    addr_2 = (uint8_t)((address >> 16) & 0x000000FF);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    if (page_fet == flash_erase_type)
    {
        SSIDataPut(SSI1_BASE, FLASH_CMD_PAGE_ERASE);
    }
    else if (half_block_fet == flash_erase_type)
    {
        SSIDataPut(SSI1_BASE, FLASH_CMD_BLOCK_ERASE);
    }
    else if (block_fet == flash_erase_type)
    {
        SSIDataPut(SSI1_BASE, FLASH_CMD_HALF_BLOCK_ERASE);
    }

    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_2);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_1);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    SSIDataPut(SSI1_BASE, addr_0);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &dummy_read);

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;
}

void flash_erase_chip_blocking(void)
{

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO25;

    SSIDataPut(SSI1_BASE, FLASH_CMD_CHIP_ERASE);

    while(SSIBusy(SSI1_BASE));

    HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO25;
}

void spi_1_isr(void)
{
    uint32_t int_stat;

    int_stat = HWREG(UDMA0_BASE + UDMA_O_REQDONE);

    if (int_stat & UDMA_O_REQDONE_SSI1_TX)
    {
        dma_finished = true;

        SSIDMADisable(SSI1_BASE, SSI_DMA_TX);               //We have to disable before we clear or we'll get a second TX interrupt.
        uDMAIntClear(UDMA0_BASE, UDMA_O_REQDONE_SSI1_TX);
    }

    if (int_stat & UDMA_O_REQDONE_SSI1_RX)
    {
        SSIDMADisable(SSI1_BASE, SSI_DMA_RX);               //The RX interrupt doesn't have to be disabled first, but do it anyway, so things are uniform.
        uDMAIntClear(UDMA0_BASE, UDMA_O_REQDONE_SSI1_RX);
    }
}
