#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "memoryManager.h"
#include "helpers.h"
#include <cx16.h>
#include <stdlib.h>

#define BYTES_PER_CHARACTER 16
#define NO_CHARS 160
#define SIZE_OF_CHARSET (BYTES_PER_CHARACTER * NO_CHARS)

#define ADDRESSSEL0 0 
#define ADDRESSSEL1 1
#define TRANSPARENT_CHAR 128
#define TRANSPARENT_CHAR_BYTE TRANSPARENT_CHAR * SIZE_PER_CHAR_CHAR_SET_ROM
#define LAST_BYTE_TRANSPARENT_CHAR (TRANSPARENT_CHAR + 1) * SIZE_PER_CHAR_CHAR_SET_ROM - 1

void b6InitCharset();

#endif