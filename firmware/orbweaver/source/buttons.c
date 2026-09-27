/*
 * Orbweaver - buttons.c
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
#include <stddef.h>
#include <stdbool.h>

#include "./driverlib/ioc.h"
#include "./driverlib/sys_ctrl.h"

#include "cmd_proc.h"
#include "clocks.h"
#include "buttons.h"

#define DEBOUNCE_TIME 100
#define RESET_TIME    3000

void buttons_init(void)
{
    IOCPortConfigureSet(IOID_27, IOC_PORT_GPIO, IOC_NO_IOPULL | IOC_INPUT_ENABLE); //Button 1
    IOCPortConfigureSet(IOID_4, IOC_PORT_GPIO, IOC_NO_IOPULL | IOC_INPUT_ENABLE);  //Button 2
}


void buttons_service(void)
{
    uint32_t buttons;
    static uint32_t button_start_time_1 = 0;
    static uint32_t button_start_time_2 = 0;
    static uint32_t reset_start_time = 0;
    static bool debouncing_1 = false;
    static bool debouncing_2 = false;
    static bool on_1 = false;
    static bool on_2 = false;

    buttons = HWREG(GPIO_BASE + GPIO_O_DIN31_0);

    if (debouncing_1)
    {
        if ((systick - button_start_time_1) >= DEBOUNCE_TIME)
        {
            debouncing_1 = false;

            if (buttons & GPIO_DIN31_0_DIO27)
            {
                on_1 = true;

                led_set_red(true);

                reset_start_time = systick;
            }
            else
            {
                on_1 = false;

                led_set_red(false);
            }
        }
    }
    else if ((buttons & GPIO_DIN31_0_DIO27) && (!on_1))
    {
        debouncing_1 = true;    //Button 1 pressed

        button_start_time_1 = systick;
    }
    else if ((!(buttons & GPIO_DIN31_0_DIO27)) && on_1)
    {
        debouncing_1 = true;    //Button 1 released

        button_start_time_1 = systick;
    }

    if (debouncing_2)
    {
        if ((systick - button_start_time_2) >= DEBOUNCE_TIME)
        {
            debouncing_2 = false;

            if (buttons & GPIO_DIN31_0_DIO4)
            {
                on_2 = true;

                led_set_amber(true);

                reset_start_time = systick;
            }
            else
            {
                on_2 = false;

                led_set_amber(false);
            }
        }
    }
    else if ((buttons & GPIO_DIN31_0_DIO4) && (!on_2))
    {
        debouncing_2 = true;    //Button 2 pressed

        button_start_time_2 = systick;
    }
    else if ((!(buttons & GPIO_DIN31_0_DIO4)) && on_2)
    {
        debouncing_2 = true;    //Button 2 Released

        button_start_time_2 = systick;
    }

    if (on_1 && on_2)
    {
        if ((systick - reset_start_time) >= RESET_TIME)
        {
            SysCtrlSystemReset();
        }
    }
}
