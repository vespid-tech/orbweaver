/*
 * Orbweaver - uart_bl.h
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

#ifndef UART_BL_H
#define UART_BL_H

void uart_init(void);
void uart_service(void);
int8_t uart_send(uint8_t* buff, uint8_t buff_len);
void uart_isr(void);
void udma_isr(void);

#endif
