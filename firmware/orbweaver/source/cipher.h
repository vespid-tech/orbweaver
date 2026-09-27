/*
 * Orbweaver - cipher.h
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

#ifndef CIPHER_H
#define CIPHER_H

#include <stdint.h>
#include <stdbool.h>

#define CIPHER_KEY_LEN    4

#define CIPHER_SUCCESS 1
#define CIPHER_ERROR  -1

#define AUTH_TAG_LEN 16
#define NONCE_LEN    13

void cipher_init(void);
uint64_t get_random_number_blocking(void);
uint16_t msg_encrypt_blocking(uint8_t* msg_buff, uint16_t msg_len, uint8_t* addAuthData, uint8_t aadLen);
int8_t msg_decrypt(uint8_t* msg_buff, uint16_t msg_len, uint8_t* addAuthData, uint8_t aadLen);
void msg_encrypt(uint8_t* msg_buff, uint16_t msg_len, uint8_t* addAuthData, uint8_t aadLen, uint8_t* nonce);
void load_cipher_key(uint32_t* cipher_key);

#endif
