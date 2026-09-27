/*
 * Orbweaver - spi_bl.c
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
#include "programmer.h"
#include "spi.h"

/*
 * MISO->DIO8
 * MOSI->DIO9
 * SCLK->DIO10
 * CS->DIO11
 *
 * DR->DIO19
 */

#define UDMA_O_REQDONE_SSI0_TX   0x00000010

#define SPI_TIMEOUT 400

typedef enum
{
    void_spi = 0,
    idle_spi,
    erase_flash_spi,
    erase_flash_all_spi,
    recv_metadata_spi,
    recv_binary_spi,
    verify_app_flash_spi,
    verify_app_mem_spi,
    program_spi,
    launch_spi,
    erase_program_mem_spi

}spi_state_t;

#define SPI_BUFF_LEN   257

static spi_state_t spi_state = void_spi;

void data_ready(bool value);
void reset_spi(void);
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

    SSIConfigSetExpClk(SSI0_BASE, 48000000, SSI_FRF_MOTO_MODE_1, SSI_MODE_SLAVE, 50000, 8);    //Set the bus to operate at 50 kHz in slave mode.

    SSIEnable(SSI0_BASE);
}


void spi_service(void)
{
    switch (spi_state)
    {
        case void_spi:

            spi_state = idle_spi;

        break;

        case idle_spi:
        {
            uint8_t recv_byte;

            if (HWREG(SSI0_BASE + SSI_O_SR) & SSI_SR_RNE)   //Check for any received bytes.
            {
                recv_byte = HWREG(SSI0_BASE + SSI_O_DR);

                if (PING_CMD == recv_byte)
                {
                    spi_write_byte_blocking(ACK_CMD);
                }
                else if (ERASE_FLASH_CMD == recv_byte)
                {
                    spi_state = erase_flash_spi;
                }
                else if (ERASE_FLASH_ALL_CMD == recv_byte)
                {
                    spi_state = erase_flash_all_spi;
                }
                else if (WRITE_METADATA_CMD == recv_byte)
                {
                    spi_state = recv_metadata_spi;
                }
                else if (WRITE_APP_CMD == recv_byte)
                {
                    spi_state = recv_binary_spi;
                }
                else if (VERIFY_FLASH_CMD == recv_byte)
                {
                    spi_state = verify_app_flash_spi;
                }
                else if (LOAD_MEM_CMD == recv_byte)
                {
                    spi_state = program_spi;
                }
                else if (VERIFY_MEM_CMD == recv_byte)
                {
                    spi_state = verify_app_mem_spi;
                }
                else if (LAUNCH_CMD == recv_byte)
                {
                    spi_state = launch_spi;
                }
                else if (ERASE_MEM_CMD == recv_byte)
                {
                    spi_state = erase_program_mem_spi;
                }
            }
        }
        break;

        case erase_flash_spi:

            flash_external_erase_metadata();

            flash_external_erase_app();

            spi_write_byte_blocking(ACK_CMD);

            spi_state = idle_spi;

        break;

        case erase_flash_all_spi:

            flash_external_erase_all();

            spi_write_byte_blocking(ACK_CMD);

            spi_state = idle_spi;

        break;

        case recv_metadata_spi:
        {
            uint8_t recv_buff[sizeof(app_metadata_t)];
            int i;
            uint32_t timer_start = systick;

            for (i=0; i<sizeof(app_metadata_t); i++)
            {
                while (!(HWREG(SSI0_BASE + SSI_O_SR) & SSI_SR_RNE))   //Wait for the RX FIFO to not be empty
                {
                    if ((systick - timer_start) >= SPI_TIMEOUT)
                    {
                        spi_write_byte_blocking(NAK_CMD);

                        spi_state = idle_spi;

                        return;
                    }
                }

                recv_buff[i] = HWREG(SSI0_BASE + SSI_O_DR);  //We received a byte. Read it.
            }

            if (PROGRAMMER_SUCCESS == flash_external_page_program(FLASH_ADDR_APP_METADATA, recv_buff, sizeof(app_metadata_t)))
            {
                spi_write_byte_blocking(ACK_CMD);
            }
            else
            {
                spi_write_byte_blocking(NAK_CMD);
            }

            spi_state = idle_spi;
        }
        break;

        case recv_binary_spi:
        {
            uint8_t recv_buff[FLASH_PAGE_SIZE];
            uint32_t recv_len;
            int i;
            uint32_t app_index = 0;
            uint32_t write_addr = FLASH_ADDR_APP;
            uint32_t timer_start;
            app_metadata_t app_metadata;

            app_metadata = flash_external_metadata_read();

            if (!verify_app_metadata(app_metadata))
            {
                spi_write_byte_blocking(NAK_CMD);

                spi_state = idle_spi;

                return;
            }

            while (app_index < app_metadata.app_len)
            {
                recv_len = app_metadata.app_len - app_index;

                if (recv_len > FLASH_PAGE_SIZE)
                {
                    recv_len = FLASH_PAGE_SIZE;
                }

                timer_start = systick;

                for (i=0; i<recv_len; i++)
                {
                    while (!(HWREG(SSI0_BASE + SSI_O_SR) & SSI_SR_RNE))   //Wait for the RX FIFO to not be empty
                    {
                        if ((systick - timer_start) >= SPI_TIMEOUT)
                        {
                            spi_write_byte_blocking(NAK_CMD);

                            spi_state = idle_spi;

                            return;
                        }
                    }

                    recv_buff[i] = HWREG(SSI0_BASE + SSI_O_DR);  //We received a byte. Read it.
                }

                app_index += recv_len;

                if (FLASH_SUCCESS == flash_external_page_program(write_addr, recv_buff, recv_len))
                {
                    spi_write_byte_blocking(ACK_CMD);
                }
                else
                {
                    spi_write_byte_blocking(NAK_CMD);

                    spi_state = idle_spi;

                    return;
                }

                write_addr += recv_len;
            }

            spi_state = idle_spi;
        }

        break;

        case verify_app_flash_spi:

            if (verify_flash_app())
            {
                spi_write_byte_blocking(ACK_CMD);
            }
            else
            {
                spi_write_byte_blocking(NAK_CMD);
            }

            spi_state = idle_spi;

        break;

        case verify_app_mem_spi:

            if (verify_mem_app())
            {
                spi_write_byte_blocking(ACK_CMD);
            }
            else
            {
                spi_write_byte_blocking(NAK_CMD);
            }

            spi_state = idle_spi;

        break;

        case program_spi:

            load_app();

            spi_write_byte_blocking(ACK_CMD);

            spi_state = idle_spi;

        break;

        case launch_spi:

            launch_app();

        break;

        case erase_program_mem_spi:

            erase_program_memory();

            spi_write_byte_blocking(ACK_CMD);

            spi_state = idle_spi;

        break;
    }
}



void spi_write_byte_blocking(uint8_t write_byte)
{
    volatile uint32_t read_byte;
    uint32_t start_time = systick;

    HWREG(SSI0_BASE + SSI_O_DR) = write_byte;

    data_ready(true);

    while (!(HWREG(SSI0_BASE + SSI_O_SR) & SSI_SR_TFE))
    {
        if ((systick - start_time) >= SPI_TIMEOUT)
        {
            data_ready(false);

            return;
        }
    }

    data_ready(false);

    while (!(HWREG(SSI0_BASE + SSI_O_SR) & SSI_SR_RNE))
    {
        asm(" NOP");
    }

    read_byte = HWREG(SSI0_BASE + SSI_O_DR);
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
