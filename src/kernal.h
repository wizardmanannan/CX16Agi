#ifndef _KERNAL_H_
#define _KERNAL_H_

#include "helpers.h"


//KernalFunctions
#define SCREEN_SET_CHAR_SET_ADDRESS 0xFF62
//Args
#define ISO 1
#define SCREEN_SET_CHAR_SET(CHARSET) \
    do { \
        asm("lda #%w", CHARSET); \
        asm("jsr %w", SCREEN_SET_CHAR_SET_ADDRESS); \
    } while(0)

#endif

#define GETIN_ADDRESS 0xFFE4

#define GET_IN(output) \
do { \
        asm("jsr %w", GETIN_ADDRESS); \
        asm("sta %v", _assmByte); \
        output = _assmByte; \
} while (0)