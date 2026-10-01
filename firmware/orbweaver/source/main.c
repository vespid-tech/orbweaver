/*
 * Orbweaver - main.c
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

#include <cipher.h>
#include <stdbool.h>
#include <stdint.h>

#include "driverlib/rf_prop_cmd.h"
#include "driverlib/sys_ctrl.h"
#include "inc/hw_rfc_dbell.h"
#include "inc/hw_nvic.h"
#include "driverlib/rf_mailbox.h"

#include "inc/hw_rfc_rat.h"
#include "inc/hw_rfc_dbell.h"
#include "driverlib/rfc.h"
#include "driverlib/rf_mailbox.h"

#include "clocks.h"
#include "power.h"
#include "leds.h"
#include "watchdog.h"
#include "buttons.h"
#include "radio.h"
#include "link.h"
#include "uart.h"
#include "spi.h"
#include "flash.h"
#include "cmd_proc.h"
#include "link.h"
#include "network.h"
#include "rng.h"
#include "status.h"


int main(void)
{
    //    CPUdelay(16000); // About 1ms delay

    /*
     * The interrupt vector table pointer will be pointing at the ROM bootloader interrupt vector table.
     * We need to point it to the table defined in the start-up file, which is at address 0x00000000.
     */
    //HWREG(NVIC_VTABLE) = 0x00000000;    //Point to our static interrupt vector table
    HWREG(NVIC_VTABLE) = 0x00008000;    //Point to our static interrupt vector table

    clocks_init();
    power_init();
    watchdog_init();
    leds_init();
    buttons_init();
    uart_init();
    spi_init();
    flash_init();
    cipher_init();
    link_init();
    radio_init();

    while(true)
    {
        leds_service();
        watchdog_service();
        buttons_service();
        uart_service();
        spi_service();
        flash_service();
        radio_service();
        link_service();
        network_service();
        rng_service();
        cmd_proc_service();
        status_service();
    }
}
