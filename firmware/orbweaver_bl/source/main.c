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

#include <stdbool.h>
#include <stdint.h>

#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "driverlib/watchdog.h"

#include "clocks.h"
#include "power.h"
#include "leds.h"
#include "uart_bl.h"
#include "programmer.h"
#include "flash.h"
#include "spi_bl.h"
#include "watchdog.h"

void watchdog_init(void);
void watchdog_feed(void);

int main(void)

{
    uint32_t led_start_time = 0;
    uint8_t led_cycle = 0;

    HWREG(NVIC_VTABLE) = 0x00000000;    //Point to our static interrupt vector table

    //watchdog_init();
    clocks_init();
    power_init();
    leds_init();
    uart_init();
    spi_init();
    flash_init();

    led_set_green(true);

    programmer_init();

    while(1)
    {
        uart_service();
        spi_service();
        //watchdog_feed();

        if ((systick - led_start_time) >= 500)
        {
            led_start_time = systick;

            if (0 == (led_cycle % 3))
            {
                led_set_green(true);
                led_set_amber(false);
                led_set_red(false);
            }
            else if (1 == (led_cycle % 3))
            {
                led_set_green(false);
                led_set_amber(true);
                led_set_red(false);
            }
            else if (2 == (led_cycle % 3))
            {
                led_set_green(false);
                led_set_amber(false);
                led_set_red(true);
            }

            led_cycle++;
        }
    }
}


void watchdog_init(void)
{
    WatchdogUnlock();
    WatchdogReloadSet(7500000); //This should give us 10 seconds.
    WatchdogResetEnable();
    WatchdogLock();
}


void watchdog_feed(void)
{
    WatchdogUnlock();
    WatchdogIntClear();
    WatchdogLock();
}
