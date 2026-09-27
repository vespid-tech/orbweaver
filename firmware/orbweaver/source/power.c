/*
 * Orbweaver - power.c
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

#include "driverlib/prcm.h"

#include "power.h"

/*
 * This function initializes power for all the peripherals, so it
 * must be called before any of the peripherals are initialized.
 */
void power_init(void)
{
    //Enable the peripheral power domain, which powers DMA, crypto, random number gen, GPT, GPIO SSI1, and I2S
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH);
    while(PRCMPowerDomainStatus(PRCM_DOMAIN_PERIPH) != PRCM_DOMAIN_POWER_ON)
    {
        __asm__(" NOP");
    }

    //Enable the serial power domain, which powers UART, SSI0, and I2C
    PRCMPowerDomainOn(PRCM_DOMAIN_SERIAL);
    while (PRCMPowerDomainStatus(PRCM_DOMAIN_SERIAL) != PRCM_DOMAIN_POWER_ON)
    {
        __asm__(" NOP");
    }

    PRCMPowerDomainOn(PRCM_DOMAIN_RFCORE);
    while (PRCMPowerDomainStatus(PRCM_DOMAIN_RFCORE) != PRCM_DOMAIN_POWER_ON)
    {
        __asm__(" NOP");
    }

    PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);          //Enable GPIO run mode
    PRCMPeripheralSleepEnable(PRCM_PERIPH_GPIO);        //Allow GPIO to operate when the processor is asleep
    PRCMPeripheralDeepSleepEnable(PRCM_PERIPH_GPIO);    //Allow GPIO to operate when the processor is in deep sleep

    PRCMPeripheralRunEnable(PRCM_PERIPH_UDMA);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_UDMA);
    PRCMPeripheralDeepSleepEnable(PRCM_PERIPH_UDMA);

    PRCMPeripheralRunEnable(PRCM_PERIPH_SSI0);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_SSI0);
    PRCMPeripheralDeepSleepEnable(PRCM_PERIPH_SSI0);

    PRCMPeripheralRunEnable(PRCM_PERIPH_CRYPTO);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_CRYPTO);
    PRCMPeripheralDeepSleepEnable(PRCM_PERIPH_CRYPTO);

    PRCMPeripheralRunEnable(PRCM_PERIPH_SSI1);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_SSI1);
    PRCMPeripheralDeepSleepEnable(PRCM_PERIPH_SSI1);

    PRCMPeripheralRunEnable(PRCM_PERIPH_UART0);
    PRCMPeripheralSleepEnable(PRCM_PERIPH_UART0);
    PRCMPeripheralDeepSleepEnable(PRCM_PERIPH_UART0);

    PRCMPeripheralRunEnable(PRCM_PERIPH_TIMER0);    //Used by link

    PRCMPeripheralRunEnable(PRCM_PERIPH_TIMER1);    //Used by link

    PRCMDomainEnable(PRCM_DOMAIN_RFCORE);

    PRCMPeripheralRunEnable(PRCM_PERIPH_TRNG);

    PRCMLoadSet();          //Synchronize the PRCM load settings
    while (!PRCMLoadGet())
    {
       __asm__(" NOP");    //Wait for the settings to synchronize
    }
}

