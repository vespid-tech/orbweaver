/*
 * Orbweaver - programmer.c
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
#include <driverlib/vims.h>
#include <driverlib/cpu.h>
#include <driverlib/flash.h>

#include "leds.h"
#include "clocks.h"
#include "flash.h"
#include "programmer.h"


/*
 * Program flash has 32 pages. Each page is 4 kB.
 *
 * Pages 0-6  (28 kB) bootloader
 * Page  7    (4 kB)  application length, CRC, and boot command
 * Pages 8-30 (92 kB) application (not including CCFG)
 * Page  31   (4 kB)  CCFG
 *
 * Program flash layout:
 * 0x00000000   bootloader start
 * 0x00006FFF   bootloader end
 * 0x00007000   application length (app metadata, page 7 start)
 * 0x00007004   application CRC
 * 0x00007FFF   page 7 (app metadata) end
 * 0x00008000   application start
 * 0x0001EFFF   application end (not including CCFG)
 * 0x0001F000   start of last page
 * 0x0001FFA8   CCFG
 * 0x0001FFFF   end of flash
 */

#define MEM_PAGE_SIZE       0x00001000
#define MEM_METADATA_PAGE   7
#define MEM_APP_PAGE_START  8
#define MEM_APP_PAGE_END    30

#define MEM_APP_METADATA_ADDR 0x00007000
#define MEM_APPLICATION_ADDR  0x00008000

#define MAX_LAUNCH_ATTEMPTS 3

#define BUTTONS_TIMEOUT 50

typedef enum
{
    void_ps = 0,
    idle_ps,
    read_len_crc_ps,
    read_len_crc_wait_ps,
    crc_read_binary_ps,
    crc_calc_ps
}programmer_state_t;

static uint8_t read_buff[FLASH_PAGE_SIZE];

/*
 * This is used to calculate the application CRC
 */
const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};


void clear_bootloader_cmd(void);

/*
 * Call this after everything else is initialized.
 */
void programmer_init(void)
{
    app_metadata_t app_metadata;
    uint32_t button_start_time = 0;

    app_metadata = flash_external_metadata_read();

    IOCPortConfigureSet(IOID_27, IOC_PORT_GPIO, IOC_NO_IOPULL | IOC_INPUT_ENABLE); //Button 1
    IOCPortConfigureSet(IOID_4, IOC_PORT_GPIO, IOC_NO_IOPULL | IOC_INPUT_ENABLE);  //Button 2

    button_start_time = systick;

    while ((HWREG(GPIO_BASE + GPIO_O_DIN31_0) & GPIO_DIN31_0_DIO27) && (HWREG(GPIO_BASE + GPIO_O_DIN31_0) & GPIO_DIN31_0_DIO4))
    {
        if ((systick - button_start_time) >= BUTTONS_TIMEOUT)    //Wait 50 ms before returning to ensure the button presses aren't some glitch
        {
            return;
        }
    }

    if (verify_app_metadata(app_metadata))
    {
        if (activate_bootloader_bc == app_metadata.boot_cmd)
        {
            clear_bootloader_cmd();

            return;
        }
        else if (load_app_bc == app_metadata.boot_cmd)
        {
            if (verify_flash_app())
            {
                load_app();

                if (verify_mem_app())
                {
                    clear_bootloader_cmd();

                    launch_app();
                }

                return;
            }
        }
        else if (launch_app_bc == app_metadata.boot_cmd)
        {
            if (verify_mem_app())
            {
                launch_app();
            }
            else
            {
                if (verify_flash_app())
                {
                    load_app();

                    if (verify_mem_app())
                    {
                        launch_app();
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    return;
                }
            }
        }
    }
    else
    {
        if (verify_mem_app())
        {
            launch_app();
        }
        else
        {
            return;
        }
    }
}


bool verify_app_metadata(app_metadata_t app_metadata)
{
    if ((0 == app_metadata.app_len)                ||
        (app_metadata.app_len > FLASH_MAX_APP_LEN) )
    {
        return false;
    }

    if (app_metadata.boot_cmd >= boot_cmd_max_bc)
    {
        return false;
    }

    return true;
}


void load_app(void)
{
    int i;
    uint32_t app_index = 0;
    uint32_t read_addr = FLASH_ADDR_APP;
    uint32_t read_len;
    uint32_t write_addr;
    uint32_t vimsMode;
    app_metadata_t app_metadata;


    app_metadata = flash_external_metadata_read();

    if (!verify_app_metadata(app_metadata))
    {
        return;
    }

    vimsMode = VIMSModeGet(VIMS_BASE);
    VIMSModeSet(VIMS_BASE, VIMS_MODE_DISABLED);

    while (VIMS_MODE_DISABLED != VIMSModeGet(VIMS_BASE))
    {
        asm(" NOP");
    }

    CPUcpsid();

    for (i=MEM_METADATA_PAGE; i<=MEM_APP_PAGE_END; i++)
    {
        FlashSectorErase(MEM_PAGE_SIZE * i);
    }

    write_addr = MEM_APPLICATION_ADDR;

    while (app_index < app_metadata.app_len)
    {
        led_toggle_red();

        read_len = app_metadata.app_len - app_index;

        if (read_len > FLASH_PAGE_SIZE)
        {
            read_len = FLASH_PAGE_SIZE;
        }

        flash_read_blocking(read_buff, read_addr, read_len);

        FlashProgram(read_buff, write_addr, read_len);

        read_addr += read_len;
        write_addr += read_len;
        app_index += read_len;
    }

    app_metadata.boot_cmd = void_bc;

    FlashProgram((uint8_t*)&app_metadata, MEM_APP_METADATA_ADDR, sizeof(app_metadata));

    CPUcpsie();

    VIMSModeSet(VIMS_BASE, vimsMode);
}


void erase_program_memory(void)
{
    int i;
    uint32_t vimsMode;

    vimsMode = VIMSModeGet(VIMS_BASE);
    VIMSModeSet(VIMS_BASE, VIMS_MODE_DISABLED);

    while (VIMS_MODE_DISABLED != VIMSModeGet(VIMS_BASE))
    {
        asm(" NOP");
    }

    CPUcpsid();

    for (i=MEM_METADATA_PAGE; i<=MEM_APP_PAGE_END; i++)
    {
        FlashSectorErase(MEM_PAGE_SIZE * i);
    }

    CPUcpsie();

    VIMSModeSet(VIMS_BASE, vimsMode);
}


void flash_external_erase_app(void)
{
    int i;
    uint8_t retry_count;

    for (i=FLASH_BLOCK_APP_START; i<=FLASH_BLOCK_APP_END; i++)
    {
        flash_write_enable_blocking(true);

        flash_erase_blocking((BLOCK_ADDR_1 * i), block_fet);

        retry_count = 50;

        while (get_wip_blocking())
        {
            CPUdelay(8000);  //About 500 us delay

            retry_count--;
        }
    }
}

void flash_external_erase(void)
{
    uint8_t retry_count = 50;

    flash_write_enable_blocking(true);

    flash_erase_chip_blocking();

    while (get_wip_blocking())
    {
        CPUdelay(8000); // About 500 us delay

        retry_count--;

        if (0 == retry_count)
        {
            return;
        }
    }
}

void flash_external_erase_metadata(void)
{
    uint8_t retry_count = 50;

    flash_write_enable_blocking(true);

    flash_erase_blocking(FLASH_ADDR_APP_METADATA, page_fet);

    while (get_wip_blocking())
    {
        CPUdelay(8000); // About 500 us delay

        retry_count--;

        if (0 == retry_count)
        {
            return;
        }
    }
}

void flash_external_erase_all(void)
{
    uint8_t retry_count = 50;

    flash_write_enable_blocking(true);

    flash_erase_chip_blocking();

    while (get_wip_blocking())
    {
        CPUdelay(8000); // About 500 us delay

        retry_count--;

        if (0 == retry_count)
        {
            return;
        }
    }
}

int32_t flash_external_metadata_write(app_metadata_t app_metadata)
{
    uint8_t retry_count = 50;

    flash_write_enable_blocking(true);

    flash_page_program_blocking(FLASH_ADDR_APP_METADATA, (uint8_t*)&app_metadata, sizeof(app_metadata));

    while (get_wip_blocking())
    {
        CPUdelay(8000); // About 500 us delay

        retry_count--;

        if (0 == retry_count)
        {
            return PROGRAMMER_FAIL;
        }
    }

    return PROGRAMMER_SUCCESS;
}


app_metadata_t flash_external_metadata_read(void)
{
    app_metadata_t app_metadata;

    flash_read_blocking((uint8_t*)&app_metadata, FLASH_ADDR_APP_METADATA, sizeof(app_metadata));

    return app_metadata;
}


bool verify_flash_app(void)
{
    app_metadata_t app_metadata;
    uint32_t app_index = 0;
    uint32_t read_addr = FLASH_ADDR_APP;
    uint32_t read_len;
    uint32_t crc_calc = 0xFFFFFFFF; // Standard initial value
    int i;

    app_metadata = flash_external_metadata_read();

    if (!verify_app_metadata(app_metadata))
    {
        return false;
    }

    while (app_index < app_metadata.app_len)
    {
        read_len = app_metadata.app_len - app_index;

        if (read_len > FLASH_PAGE_SIZE)
        {
            read_len = FLASH_PAGE_SIZE;
        }

        flash_read_blocking(read_buff, read_addr, FLASH_PAGE_SIZE);

        for (i=0; i<read_len; i++)
        {
            crc_calc = (crc_calc >> 8) ^ crc32_tab[(crc_calc ^ read_buff[i]) & 0xFF];
        }

        app_index += read_len;
        read_addr += read_len;

        led_toggle_amber();
    }

    crc_calc ^= 0xFFFFFFFF;

    if (crc_calc ==  app_metadata.app_crc)
    {
        return true;
    }
    else
    {
        return false;
    }
}


bool verify_mem_app(void)
{
    app_metadata_t* app_metadata = (app_metadata_t*)MEM_APP_METADATA_ADDR;
    uint8_t* app_index = (uint8_t*)MEM_APPLICATION_ADDR;
    uint32_t crc_calc = 0xFFFFFFFF; // Standard initial value
    int i;


    if (!verify_app_metadata(*app_metadata))
    {
        return false;
    }

    for (i=0; i<app_metadata->app_len; i++)
    {
        crc_calc = (crc_calc >> 8) ^ crc32_tab[(crc_calc ^ app_index[i]) & 0xFF];
    }

    crc_calc ^= 0xFFFFFFFF;

    if (crc_calc ==  app_metadata->app_crc)
    {
        return true;
    }
    else
    {
        return false;
    }
}


int32_t flash_external_page_program(uint32_t addr, uint8_t* buff, uint32_t buff_len)
{
    uint8_t retry_count = 50;

    flash_write_enable_blocking(true);

    flash_page_program_blocking(addr, buff, buff_len);

    while (get_wip_blocking())
    {
        CPUdelay(8000); // About 500 us delay

        retry_count--;

        if (0 == retry_count)
        {
            return PROGRAMMER_FAIL;
        }
    }

    return PROGRAMMER_SUCCESS;
}


void clear_bootloader_cmd(void)
{
    app_metadata_t app_metadata;

    app_metadata = flash_external_metadata_read();

    app_metadata.boot_cmd = launch_app_bc;

    flash_external_erase_metadata();

    flash_external_metadata_write(app_metadata);
}


void launch_app(void)
{
    uint32_t appStack = *(volatile uint32_t *)APPLICATION_ADDR;         //APPLICATION_ADDR holds the RAM start address of the application stack.

    CPUcpsid();

    __set_MSP(appStack);

    HWREG(NVIC_VTABLE) = APPLICATION_ADDR;

    asm(" MOV R1, #0x00008004");    //0x00008004 holds the address of the application reset handler. Move it into into R1
    asm(" LDR R2, [R1]");           //Load the reset handler address into R2
    asm(" BX R2");                  //Branch to the application reset handler
}

