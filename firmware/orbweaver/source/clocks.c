/*
 * Orbweaver - clocks.c
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

#include "driverlib/prcm.h"
#include "driverlib/osc.h"
#include "driverlib/systick.h"
#include "driverlib/udma.h"

#include "clocks.h"

//The system clock is 48 MHz, so 48000 yeilds a 1 ms systick
#define SYS_TICK_PERIOD 48000 - 1   //Minus 1 because the hardware timer counts down to zero

tDMAControlTable channelControlTable[64];       //64 channels max
#pragma DATA_ALIGN(channelControlTable, 1024)   //The DMA control table must be aligned to a 1024 byte boundary

uint32_t systick __attribute__((aligned(4))) = 0;

void sys_tick_isr(void);

/*
 * Enables systick and systick interrupt as well as all other system interrupts.
 *
 * This should probably be the first thing initialized just in case we need to time anything
 * during other initializations.
 */
void clocks_init(void)
{
    OSCClockSourceSet(OSC_SRC_CLK_MF | OSC_SRC_CLK_HF, OSC_XOSC_HF);        //Use the external high-frequency crystal oscillator to drive the MF and HF clocks.

    //The external oscillator is more accurate than the internal RC oscillator,
    //so use the external oscillator if we aren't using it already.
    if (OSCClockSourceGet(OSC_SRC_CLK_HF) != OSC_XOSC_HF)
    {
        OSCHfSourceSwitch();    //The external oscillator is not being used, so switch to it.
    }

    HWREG(NVIC_ST_CTRL) |= NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_ENABLE;  //Enable systick

    HWREG(NVIC_ST_RELOAD) = SYS_TICK_PERIOD;    //Set the systick period

    //IntRegister(INT_SYSTICK, sys_tick_isr);     //Register the systick interrupt

    HWREG(NVIC_ST_CTRL) |= NVIC_ST_CTRL_INTEN;  //Enable the systick interrupt

    CPUcpsie();                                 //Enable all system interrupts

    /*
    if (!AONRTCActive())
    {
        AONRTCEnable(); //Must be turned on for the next two functions to work.
    }
    //This is for test the crystal. amp_0 and amp_1 should be "close to each other" if the crystal is functioning properly.
    amp_0 = OSCHF_DebugGetCrystalAmplitude();
    amp_1 = OSCHF_DebugGetExpectedAverageCrystalAmplitude();
    */
}


/*
 * The systick interrupt
 */
void sys_tick_isr(void)
{
    systick++;
}
