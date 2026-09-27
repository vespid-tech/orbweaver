/*
 * Orbweaver - uart_bl.c
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

#include "clocks.h"
#include "flash.h"
#include "programmer.h"
#include "uart_bl.h"

#define UART_TIMEOUT   100

#define UDMA_O_REQDONE_UART0_TX   0x00000004

#define UART_BUFF_LEN FLASH_PAGE_SIZE

//Note: at 115200 baud, a 256 byte message should take ~18 ms to receive.
#define MSG_TIMEOUT 50

typedef enum
{
    void_us = 0,
    idle_us,
    erase_flash_us,
    erase_flash_all_us,
    recv_metadata_us,
    recv_binary_us,
    verify_app_flash_us,
    verify_app_mem_us,
    program_us,
    launch_us,
    erase_program_mem_us

}uart_state_t;


/*
 * Initialize the serial port
 */
void uart_init(void)
{
    HWREG(IOC_BASE + IOC_O_IOCFG2) = IOC_IOCFG2_IE | IOC_IOCFG2_PULL_CTL_DIS | IOC_IOCFG2_PORT_ID_UART0_RX;     //Make DIO2 UART0 RX
    HWREG(IOC_BASE + IOC_O_IOCFG3) = IOC_IOCFG3_PULL_CTL_DIS | IOC_IOCFG3_PORT_ID_UART0_TX;                     //Make DIO3 UART0 TX

    HWREG(UART0_BASE + UART_O_IBRD) = 0x0000001A;   //Set the UART to run at 115200 baud
    HWREG(UART0_BASE + UART_O_FBRD) = 0x00000003;   //Set the UART to run at 115200 baud

    HWREG(UART0_BASE + UART_O_LCRH) = UART_LCRH_WLEN_8; //Use 8 bit words
    HWREG(UART0_BASE + UART_O_LCRH) |= UART_LCRH_FEN;   //Enable the FIFOs

    HWREG(UART0_BASE + UART_O_CTL) = UART_CTL_UARTEN;   //Enable the UART

    HWREG(UART0_BASE + UART_O_CTL) |= UART_CTL_RXE | UART_CTL_TXE;  //Enable UART transmit
}


void uart_service(void)
{
    static uart_state_t uart_state = void_us;

    switch (uart_state)
    {
        case void_us:

            uart_state = idle_us;

        break;

        case idle_us:
        {
            uint8_t recv_byte;

            if (!(HWREG(UART0_BASE + UART_O_FR) & UART_FR_RXFE))   //Check for any received bytes.
            {
                recv_byte = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.

                if (PING_CMD == recv_byte)
                {
                    HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;
                }
                else if (ERASE_FLASH_CMD == recv_byte)
                {
                    uart_state = erase_flash_us;
                }
                else if (ERASE_FLASH_ALL_CMD == recv_byte)
                {
                    uart_state = erase_flash_all_us;
                }
                else if (WRITE_METADATA_CMD == recv_byte)
                {
                    uart_state = recv_metadata_us;
                }
                else if (WRITE_APP_CMD == recv_byte)
                {
                    uart_state = recv_binary_us;
                }
                else if (VERIFY_FLASH_CMD == recv_byte)
                {
                    uart_state = verify_app_flash_us;
                }
                else if (LOAD_MEM_CMD == recv_byte)
                {
                    uart_state = program_us;
                }
                else if (VERIFY_MEM_CMD == recv_byte)
                {
                    uart_state = verify_app_mem_us;
                }
                else if (LAUNCH_CMD == recv_byte)
                {
                    uart_state = launch_us;
                }
                else if (ERASE_MEM_CMD == recv_byte)
                {
                    uart_state = erase_program_mem_us;
                }
            }
        }
        break;

        case erase_flash_us:

            flash_external_erase_metadata();

            flash_external_erase_app();

            HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;

            uart_state = idle_us;

        break;

        case erase_flash_all_us:

            flash_external_erase_all();

            HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;

            uart_state = idle_us;

        break;

        case recv_metadata_us:
        {
            uint8_t recv_buff[sizeof(app_metadata_t)];
            int i;
            uint32_t timer_start = systick;

            for (i=0; i<sizeof(app_metadata_t); i++)
            {
                while (HWREG(UART0_BASE + UART_O_FR) & UART_FR_RXFE)   //Wait for the RX FIFO to not be empty
                {
                    if ((systick - timer_start) >= UART_TIMEOUT)
                    {
                        HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;

                        uart_state = idle_us;

                        return;
                    }
                }

                recv_buff[i] = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.
            }

            if (PROGRAMMER_SUCCESS == flash_external_page_program(FLASH_ADDR_APP_METADATA, recv_buff, sizeof(app_metadata_t)))
            {
                HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;
            }
            else
            {
                HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;
            }

            uart_state = idle_us;
        }
        break;

        case recv_binary_us:
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
                HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;

                uart_state = idle_us;

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
                    while (HWREG(UART0_BASE + UART_O_FR) & UART_FR_RXFE)   //Wait for the RX FIFO to not be empty
                    {
                        if ((systick - timer_start) >= UART_TIMEOUT)
                        {
                            HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;

                            uart_state = idle_us;

                            return;
                        }
                    }

                    recv_buff[i] = HWREG(UART0_BASE + UART_O_DR);  //We received a byte. Read it.
                }

                app_index += recv_len;

                if (FLASH_SUCCESS == flash_external_page_program(write_addr, recv_buff, recv_len))
                {
                    HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;
                }
                else
                {
                    HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;

                    uart_state = idle_us;

                    return;
                }

                write_addr += recv_len;
            }

            uart_state = idle_us;
        }

        case verify_app_flash_us:

            if (verify_flash_app())
            {
                HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;
            }
            else
            {
                HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;
            }

            uart_state = idle_us;

        break;

        case verify_app_mem_us:

            if (verify_mem_app())
            {
                HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;
            }
            else
            {
                HWREG(UART0_BASE + UART_O_DR) = NAK_CMD;
            }

            uart_state = idle_us;

        break;

        case program_us:

            load_app();

            HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;

            uart_state = idle_us;

        break;

        case launch_us:

            launch_app();

        break;

        case erase_program_mem_us:

            erase_program_memory();

            HWREG(UART0_BASE + UART_O_DR) = ACK_CMD;

            uart_state = idle_us;

        break;
    }
}

