#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "graphics.h"
#include "helpers.h"

//must be constants
#define SET_VERA_ADDRESS(VeraAddress, AddressSel, Stride) \
    do {      \
        asm("lda #%w", AddressSel);                  \
        asm("sta %w", VERA_ctrl);                  \
        asm("lda #%w", Stride << 4);                           \
        asm("ora #^%l", VeraAddress);           \
        asm("sta %w", VERA_addr_bank);             \
        asm("lda #< %l", VeraAddress);          \
        asm("sta %w", VERA_addr_low);             \
        asm("lda #> %l", VeraAddress);          \
        asm("sta %w", VERA_addr_high);              \
    } while(0);


#define SET_VERA_ADDRESS_ABSOLUTE(VeraAddress, AddressSel, Stride) \
    do {      \
				_assmLong = VeraAddress; \
        asm("lda #%w", AddressSel); \
		asm("sta %w", VERA_ctrl); \
		asm("lda %v + 2", _assmLong); \
		asm("and #$1"); \
		asm("ora #%w", Stride << 4); \
		asm("sta %w", VERA_addr_bank); \
		asm("lda %v", _assmLong); \
		asm("sta %w", VERA_addr_low); \
		asm("lda %v + 1", _assmLong); \
		asm("sta %w", VERA_addr_high); \
	} while (0);

#endif