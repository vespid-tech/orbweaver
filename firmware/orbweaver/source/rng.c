/*
 * Orbweaver - rng.c
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

#include "driverlib/trng.h"

#include "clocks.h"
#include "status.h"
#include "rng.h"

/*
 * The system needs random numbers for various purposes including encryption. The TNRG takes a certain amount
 * of time to generate a truly random number. Rather starting the TRNG and waiting for it when we need a
 * random number, this module keeps a buffer of random numbers in reserve, so one is available whenever it
 * is needed. Each time one is used, and new one is generated to replace it.
 */

#define RNG_BUFF_LEN 5

#define RNG_TIMEOUT 100

typedef enum
{
    void_rng = 0,
    idle_rng,
    init_rng,
    get_rng,
    error_rng
}rng_state_t;


static uint64_t rng_buff[RNG_BUFF_LEN];
static uint8_t rng_index_read = 0;
static uint8_t rng_index_write = 0;

static uint32_t rng_start_time = 0;

inline void increment_index(uint8_t* index);
bool get_random_number(uint64_t* random_number);

void rng_service(void)
{
    static rng_state_t rng_state = void_rng;

    switch (rng_state)
    {
        case void_rng:

            TRNGEnable();

            rng_index_write = 0;

            rng_start_time = systick;

            rng_state = init_rng;

        break;

        case idle_rng:

            if (rng_index_write != rng_index_read)
            {
                TRNGEnable();

                HWREG(TRNG_BASE + TRNG_O_IRQFLAGCLR) = 0x1; //Start a new random number generation

                rng_start_time = systick;

                rng_state = get_rng;
            }

        break;

        case init_rng:

            if (get_random_number(&rng_buff[rng_index_write]))
            {
                increment_index(&rng_index_write);

                if (0 != rng_index_write)
                {
                    HWREG(TRNG_BASE + TRNG_O_IRQFLAGCLR) = 0x1; //Start a new random number generation
                }
                else
                {
                    TRNGDisable();

                    rng_state = idle_rng;
                }
            }
            else if ((systick - rng_start_time) >= RNG_TIMEOUT)
            {
                report_error(rng_timeout_err);

                rng_state = error_rng;
            }

        break;

        case get_rng:

            if (get_random_number(&rng_buff[rng_index_write]))
            {
                increment_index(&rng_index_write);

                if (rng_index_write != rng_index_read)
                {
                    HWREG(TRNG_BASE + TRNG_O_IRQFLAGCLR) = 0x1; //Start a new random number generation
                }
                else
                {
                    TRNGDisable();

                    rng_state = idle_rng;
                }
            }
            else if ((systick - rng_start_time) >= RNG_TIMEOUT)
            {
                report_error(rng_timeout_err);

                rng_state = error_rng;
            }

        break;

        case error_rng:

            rng_state = void_rng;

        break;
    }
}


uint64_t rng_get(void)
{
    uint64_t rand = rng_buff[rng_index_read];

    increment_index(&rng_index_read);

    return rand;
}

inline void increment_index(uint8_t* index)
{
    (*index)++;

    if (*index >= RNG_BUFF_LEN)
    {
        *index = 0;
    }
}

/*
 * Returns true if the random number was successfully generated and retrieved
 */
bool get_random_number(uint64_t* random_number)
{
    if (!(TRNG_NUMBER_READY & TRNGStatusGet()))
    {
        return false;
    }

    *random_number  = HWREG(TRNG_BASE + TRNG_O_OUT0);                   //Get the lower 32 bits
    *random_number |= ((uint64_t)HWREG(TRNG_BASE + TRNG_O_OUT1)) << 32; //Get the upper 32 bits

    return true;
}
