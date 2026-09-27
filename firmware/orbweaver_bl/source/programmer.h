/*
 * Orbweaver - programmer.h
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

#ifndef PROGRAMMER_H
#define PROGRAMMER_H

#include "flash.h"

#define PING_CMD            0x11
#define ERASE_FLASH_CMD     0x22
#define ERASE_FLASH_ALL_CMD 0x23
#define WRITE_METADATA_CMD  0x33
#define WRITE_APP_CMD       0x44
#define VERIFY_FLASH_CMD    0x55
#define LOAD_MEM_CMD        0x66
#define VERIFY_MEM_CMD      0x77
#define LAUNCH_CMD          0x88
#define ERASE_MEM_CMD       0x99
#define ACK_CMD             0xAA
#define NAK_CMD             0xBB

#define PROGRAMMER_FAIL    -1
#define PROGRAMMER_BUSY     0
#define PROGRAMMER_SUCCESS  1

#define APPLICATION_ADDR 0x00008000    //This is the address of our application in program flash.

void programmer_init(void);
void load_app(void);
void erase_program_memory(void);
void flash_external_erase(void);
void flash_external_erase_metadata(void);
void flash_external_erase_app(void);
void flash_external_erase_all(void);
int32_t flash_external_metadata_write(app_metadata_t app_metadata);
app_metadata_t flash_external_metadata_read(void);
bool verify_flash_app(void);
bool verify_mem_app(void);
bool verify_app_metadata(app_metadata_t app_metadata);
int32_t flash_external_page_program(uint32_t addr, uint8_t* buff, uint32_t buff_len);
void launch_app(void);


#endif
