/*
 * Orbweaver - watchdog.c
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

#include "driverlib/watchdog.h"

#include "watchdog.h"

#define WDT_RESET_VALUE 1500000 //Should reset after 2 seconds. The main loop should never hang for close to that long.

/*
 * The watchdog timer counts at a rate of SCLK_HF (48 MHz) divided by 32, which is 1.5 MHz. The
 * first time it counts down to 0, an interrupt is asserted, and the timer is reset. If it counts
 * down to 0 a second time without the interrupt being cleared, the device is reset. This means
 * a count of 750,000 will reset the device after 1 second.
 */
void watchdog_init(void)
{
    WatchdogUnlock();
    WatchdogReloadSet(WDT_RESET_VALUE);

    WatchdogResetEnable();

    WatchdogLock();
}


void watchdog_service(void)
{
    WatchdogUnlock();
    WatchdogIntClear();
    WatchdogLock();
}
