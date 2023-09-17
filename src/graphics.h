#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "memoryManager.h"
#include "helpers.h"
#include <cx16.h>
#include <stdlib.h>

#define BYTES_PER_CHARACTER 16
#define TRANSPARENT_CHAR 0xAF
#define NO_CHARS 160
#define SIZE_OF_CHARSET (BYTES_PER_CHARACTER * NO_CHARS)

void b6InitCharset(byte* buffer, byte bank);

#endif