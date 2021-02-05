//
//  dev_auth.c
//  dev_auth_test
//
//  Created by David Crespo on 18/06/2018.
//  Copyright © 2018 Virtual Code SL. All rights reserved.
//

#include "dev_auth.h"

// comentar/eliminar esta linea para alocatar estáticamente el contexto de blowfish (~4kb)
//#define USE_MALLOC_FOR_BLOWFISH_CONTEXT 1

#include <stdlib.h>
#include "blowfish.h"

static unsigned long (*ul1_for_vec8)(const void*) = NULL;
static unsigned long (*ul2_for_vec8)(const void*) = NULL;

static unsigned long ul1LE_for_vec8(const void* buffer) {
    unsigned long res = 0;
    const unsigned char* data = (const unsigned char*)buffer;
    res |= data[0] <<  0;
    res |= data[1] <<  8;
    res |= data[2] << 16;
    res |= data[3] << 24;
    return res;
}

static unsigned long ul2LE_for_vec8(const void* buffer) {
    unsigned long res = 0;
    const unsigned char* data = (const unsigned char*)buffer;
    res |= data[4] <<  0;
    res |= data[5] <<  8;
    res |= data[6] << 16;
    res |= data[7] << 24;
    return res;
}

static unsigned long ul1BE_for_vec8(const void* buffer) {
    unsigned long res = 0;
    const unsigned char* data = (const unsigned char*)buffer;
    res |= data[0] << 24;
    res |= data[1] << 16;
    res |= data[2] <<  8;
    res |= data[3] <<  0;
    return res;
}

static unsigned long ul2BE_for_vec8(const void* buffer) {
    unsigned long res = 0;
    const unsigned char* data = (const unsigned char*)buffer;
    res |= data[4] << 24;
    res |= data[5] << 16;
    res |= data[6] <<  8;
    res |= data[7] <<  0;
    return res;
}

static void (*ul1_to_vec8)(unsigned long, void*) = NULL;
static void (*ul2_to_vec8)(unsigned long, void*) = NULL;

static void ul1LE_to_vec8(unsigned long val, void* buffer) {
    unsigned char* data = (unsigned char*)buffer;
    data[0] = (val >>  0) & 0xFF;
    data[1] = (val >>  8) & 0xFF;
    data[2] = (val >> 16) & 0xFF;
    data[3] = (val >> 24) & 0xFF;
}

static void ul2LE_to_vec8(unsigned long val, void* buffer) {
    unsigned char* data = (unsigned char*)buffer;
    data[4] = (val >>  0) & 0xFF;
    data[5] = (val >>  8) & 0xFF;
    data[6] = (val >> 16) & 0xFF;
    data[7] = (val >> 24) & 0xFF;
}

static void ul1BE_to_vec8(unsigned long val, void* buffer) {
    unsigned char* data = (unsigned char*)buffer;
    data[0] = (val >> 24) & 0xFF;
    data[1] = (val >> 16) & 0xFF;
    data[2] = (val >>  8) & 0xFF;
    data[3] = (val >>  0) & 0xFF;
}

static void ul2BE_to_vec8(unsigned long val, void* buffer) {
    unsigned char* data = (unsigned char*)buffer;
    data[4] = (val >> 24) & 0xFF;
    data[5] = (val >> 16) & 0xFF;
    data[6] = (val >>  8) & 0xFF;
    data[7] = (val >>  0) & 0xFF;
}

static void encrypt_vec(BLOWFISH_CTX* ctx, const void* input_vec, void* output_vec) {
    unsigned long io1, io2;
    io1 = ul1_for_vec8(input_vec);
    io2 = ul2_for_vec8(input_vec);
    Blowfish_Encrypt(ctx, &io1, &io2);  // encriptado in-place
    ul1_to_vec8(io1, output_vec);
    ul2_to_vec8(io2, output_vec);
}

static void decrypt_vec(BLOWFISH_CTX* ctx, const void* input_vec, void* output_vec) {
    unsigned long io1, io2;
    io1 = ul1_for_vec8(input_vec);
    io2 = ul2_for_vec8(input_vec);
    Blowfish_Decrypt(ctx, &io1, &io2);  // desencriptado in-place
    ul1_to_vec8(io1, output_vec);
    ul2_to_vec8(io2, output_vec);
}

static void detect_endian() {
    int val = 0x12345678;
    void* pv = (void*)(&val);
    unsigned char* puc = (unsigned char*)pv;
    if (*puc == 0x78) {
        // LITTLE ENDIAN
        ul1_for_vec8 = ul1BE_for_vec8;
        ul2_for_vec8 = ul2BE_for_vec8;
        ul1_to_vec8 = ul1BE_to_vec8;
        ul2_to_vec8 = ul2BE_to_vec8;
    }
    else {
        // BIG ENDIAN
        ul1_for_vec8 = ul1LE_for_vec8;
        ul2_for_vec8 = ul2LE_for_vec8;
        ul1_to_vec8 = ul1LE_to_vec8;
        ul2_to_vec8 = ul2LE_to_vec8;
    }
}

#ifndef USE_MALLOC_FOR_BLOWFISH_CONTEXT
static BLOWFISH_CTX static_blowfish_ctx;
#endif

static BLOWFISH_CTX* ctx;

static unsigned char shufsn[10];

void dev_auth_init(const void* device_serial)
{
    // detectamos endian para elegir funciones de conversión de array de bytes a entero de 32 bits
    detect_endian();
    
    unsigned char* normsn = (unsigned char*)device_serial;
    // barajamos digitos y hacemos XOR
    shufsn[0] = normsn[5] ^ 0x55;
    shufsn[1] = normsn[9] ^ 0xA5;
    shufsn[2] = normsn[2] ^ 0xAA;
    shufsn[3] = normsn[4] ^ 0x5A;
    shufsn[4] = normsn[3] ^ 0x55;
    shufsn[5] = normsn[1] ^ 0xA5;
    shufsn[6] = normsn[7] ^ 0xAA;
    shufsn[7] = normsn[0] ^ 0x5A;
    shufsn[8] = normsn[6] ^ 0x55;
    shufsn[9] = normsn[8] ^ 0xA5;

#ifdef USE_MALLOC_FOR_BLOWFISH_CONTEXT
    ctx = (BLOWFISH_CTX*)malloc(sizeof(BLOWFISH_CTX));
#else
    ctx = &static_blowfish_ctx;
#endif
    
    Blowfish_Init(ctx, shufsn, 10);
}

static unsigned char* challenge_result[10];

const void* dev_auth_challenge(const void* challenge_input)
{
    encrypt_vec(ctx, challenge_input, challenge_result);
    return challenge_result;
}

void dev_auth_close(void)
{
#ifdef USE_MALLOC_FOR_BLOWFISH_CONTEXT
    free(ctx);
#endif
}

