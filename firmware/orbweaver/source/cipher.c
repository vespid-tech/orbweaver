/*
 * Orbweaver - cipher.c
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

#include <stdlib.h>
#include <stdint.h>

#include "inc/hw_crypto.h"
#include "driverlib/prcm.h"
#include "driverlib/trng.h"
#include "driverlib/crypto.h"

#include "clocks.h"
#include "cipher.h"

/*
 * after encryption, msg_buff will contain the following:
 * [msg][auth tag][nonce]
 *
 * the auth tag is 16 bytes and the nonce is 13,
 * so 29 extra bytes must be available in the msg_buff
 * after the message.
 */

#define FIELD_LEN    2

uint32_t random_num;
uint32_t status;
uint32_t result;

void cipher_init(void)
{
    //TRNGReset();  //This doesn't seem to be necessary, even though the datasheet says it is.

    //TRNGConfigure(0, 256, 0);  //256 is shortest sampling time, and least random
    TRNGConfigure(0x400, 0x10000, 0);       //This takes about 2 ms to get 2 random numbers, which should be thoroughly random
}


void msg_encrypt(uint8_t* msg_buff, uint16_t msg_len, uint8_t* aad, uint8_t aadLen, uint8_t* nonce)
{
    memcpy(msg_buff + msg_len + AUTH_TAG_LEN, nonce, NONCE_LEN);    //Copy the nonce into the msg_buff after the message and authentication tag

    //(uint32_t*)(msg_buff + msg_len + AUTH_TAG_LEN) points at the nonce, which we just copied into the msg_buff TODO: Do we need to align the aad in this function since it is a pointer to a uint32_t?
    CRYPTOCcmAuthEncrypt(true, AUTH_TAG_LEN, (uint32_t*)(msg_buff + msg_len + AUTH_TAG_LEN), (uint32_t*)msg_buff, msg_len, (uint32_t*)aad, aadLen, CRYPTO_KEY_AREA_0, FIELD_LEN, false);
}

int8_t msg_decrypt(uint8_t* msg_buff, uint16_t msg_len, uint8_t* aad, uint8_t aadLen)
{
    if (msg_len <= (AUTH_TAG_LEN + NONCE_LEN))
    {
        return CIPHER_ERROR;
    }

    msg_len -= NONCE_LEN;

    CRYPTOCcmInvAuthDecrypt (true, AUTH_TAG_LEN, (uint32_t*)(msg_buff + msg_len), (uint32_t*)msg_buff, msg_len, (uint32_t*)aad, aadLen, CRYPTO_KEY_AREA_0, FIELD_LEN, false);

    return CIPHER_SUCCESS;
}


/*
 * Encrypts a message. Use the MAC address of the radio we are sending the message to as the addAuthData.
 *
 * This function blocks. It is for testing only. Don't use it in the actual application.
 */
uint16_t msg_encrypt_blocking(uint8_t* msg_buff, uint16_t msg_len, uint8_t* addAuthData, uint8_t aadLen)
{
    uint64_t random_num;

    random_num = get_random_number_blocking();

    random_num = 0;

    memcpy(msg_buff + msg_len + AUTH_TAG_LEN, &random_num, sizeof(random_num));

    random_num = get_random_number_blocking();

    memcpy(msg_buff + msg_len + AUTH_TAG_LEN + sizeof(random_num), &random_num, NONCE_LEN - sizeof(random_num));

    CRYPTOCcmAuthEncrypt(true, AUTH_TAG_LEN, (uint32_t*)(msg_buff + msg_len + AUTH_TAG_LEN), (uint32_t*)msg_buff, msg_len, (uint32_t*)addAuthData, aadLen, CRYPTO_KEY_AREA_0, FIELD_LEN, false);

    while (AES_SUCCESS != CRYPTOCcmAuthEncryptStatus())
    {
        asm(" NOP");
    }

    CRYPTOCcmAuthEncryptResultGet(AUTH_TAG_LEN, (uint32_t*)(msg_buff + msg_len));

    return msg_len + AUTH_TAG_LEN + NONCE_LEN;
}

/*
 * This blocks. For testing only. Don't use in the actual application
 */
uint64_t get_random_number_blocking(void)
{
    uint64_t random_number;
    uint64_t status_timer = systick + 2;

    TRNGEnable();

    while ( (!(TRNG_NUMBER_READY & TRNGStatusGet())) && (systick < status_timer))
    {
        asm(" NOP");
    }

    random_number  = HWREG(TRNG_BASE + TRNG_O_OUT0);  // Lower 32 bits
    random_number |= ((uint64_t)HWREG(TRNG_BASE + TRNG_O_OUT1)) << 32;  // Upper 32 bits

    HWREG(TRNG_BASE + TRNG_O_IRQFLAGCLR) = 0x1; //Start a random number generation

    TRNGDisable();

    return random_number;
}

void load_cipher_key(uint32_t* cipher_key)
{
    CRYPTOAesLoadKey(cipher_key, CRYPTO_KEY_AREA_0);   //Load the cipher key into RAM
}
