/*
 * Orbweaver - leds.c
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

#include "driverlib/ioc.h"

#include "clocks.h"
#include "leds.h"

#define LED_TIMEOUT 500

#define DISPLAY_TIMEOUT 2000

static bool display_enabled = false;
static uint8_t display_num = 0x00;
static uint32_t display_start_time = 0;

/*
 * Initializes the LEDs.
 */
void leds_init(void)
{
    IOCPortConfigureSet(IOID_5, IOC_PORT_GPIO, IOC_STD_OUTPUT); //Red LED
    IOCPortConfigureSet(IOID_6, IOC_PORT_GPIO, IOC_STD_OUTPUT); //Amber LED
    IOCPortConfigureSet(IOID_7, IOC_PORT_GPIO, IOC_STD_OUTPUT); //Green LED

    HWREG(GPIO_BASE + GPIO_O_DOE31_0) |= GPIO_DOE31_0_DIO5 | GPIO_DOE31_0_DIO6 | GPIO_DOE31_0_DIO7;

    led_set_green(false);
    led_set_amber(false);
    led_set_red(false);
}


void leds_service(void)
{
    static uint32_t led_start_time = 0;

    if (display_enabled)
    {
        led_set_green(display_num & 0x01);
        led_set_amber(display_num & 0x02);
        led_set_red(display_num & 0x04);

        if ((systick - display_start_time) >= DISPLAY_TIMEOUT)
        {
            led_set_green(false);
            led_set_amber(false);
            led_set_red(false);

            led_start_time = systick;

            display_enabled = false;
        }
    }
    else
    {
        if ((systick - led_start_time) >= LED_TIMEOUT)
        {
            led_start_time = systick;

            led_toggle_green();
            //led_toggle_amber();
            //led_toggle_red();
        }
    }
}


inline void  led_set_green(bool value)
{
    if (value)
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO7;
    }
    else
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO7;
    }
}


inline void led_set_amber(bool value)
{
    if (value)
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO6;
    }
    else
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO6;
    }
}


inline void led_set_red(bool value)
{
    if (value)
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) |= GPIO_DOUT31_0_DIO5;
    }
    else
    {
        HWREG(GPIO_BASE + GPIO_O_DOUT31_0) &= ~GPIO_DOUT31_0_DIO5;
    }
}

inline void led_toggle_green(void)
{
    HWREG(GPIO_BASE + GPIO_O_DOUTTGL31_0) = GPIO_DOUTTGL31_0_DIO7;
}

inline void led_toggle_amber(void)
{
    HWREG(GPIO_BASE + GPIO_O_DOUTTGL31_0) = GPIO_DOUTTGL31_0_DIO6;
}

inline void led_toggle_red(void)
{
    HWREG(GPIO_BASE + GPIO_O_DOUTTGL31_0) = GPIO_DOUTTGL31_0_DIO5;
}

void led_display_num(uint8_t new_display_num)
{
    display_num = new_display_num;
    display_enabled = true;

    display_start_time = systick;
}
