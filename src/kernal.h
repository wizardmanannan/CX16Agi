#ifndef _KERNAL_H_
#define _KERNAL_H_


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
