/*
 * Orbweaver - flash.h
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

#ifndef FLASH_H
#define FLASH_H

#define FLASH_ERROR  -1
#define FLASH_BUSY    0
#define FLASH_SUCCESS 1

#define FLASH_LEN           0x00100000
#define FLASH_BLOCK_LEN     0x00010000
#define FLASH_MAX_APP_LEN   (FLASH_LEN - FLASH_BLOCK_LEN)

#define FLASH_PAGE_SIZE 256

#define FLASH_ADDR_CONFIG       0x00000000
#define FLASH_ADDR_APP_METADATA 0x0000FF00
#define FLASH_ADDR_APP          0x00010000

#define FLASH_BLOCK_APP_START 1
#define FLASH_BLOCK_APP_END   15



#define BLOCK_ADDR_1        0x00010000  //Address of block one in external flash, which is where the binary length, CRC, and binary is stored if there is one


typedef struct
{
    uint32_t app_len;
    uint32_t app_crc;
    uint32_t boot_cmd;
}app_metadata_t;

typedef enum
{
    void_bc = 0,
    activate_bootloader_bc,
    load_app_bc,
    launch_app_bc,

    boot_cmd_max_bc,
    boot_cmd_force_size_bc = 0xFFFFFFFF,
}boot_cmd_t;

typedef enum
{
    void_fet = 0,
    page_fet,
    half_block_fet,
    block_fet,
    chip_fet
}flash_erase_type_t;

void flash_init(void);
void flash_service(void);
int8_t flash_write(uint32_t address, uint8_t* buff, uint32_t buff_len);
int8_t flash_read(uint32_t address, uint32_t len);
void flash_read_blocking(uint8_t* read_buff, uint32_t address, uint32_t length);
int8_t flash_erase(uint32_t address, flash_erase_type_t erase_type);
int8_t flash_get_data(uint8_t* buff, uint32_t length);
int8_t flash_status_get(void);
bool get_wip_blocking(void);
void flash_write_enable_blocking(bool enable);
void flash_erase_blocking(uint32_t address, flash_erase_type_t flash_erase_type);
void flash_erase_chip_blocking(void);
void flash_page_program_blocking(uint32_t address, uint8_t* write_buff, uint32_t write_len);

#endif
