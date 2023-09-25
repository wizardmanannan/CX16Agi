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
#define TRANSPARENT_CHAR_BYTE (TRANSPARENT_CHAR * SIZE_PER_CHAR_CHAR_SET_ROM)
#define LAST_BYTE_TRANSPARENT_CHAR ((TRANSPARENT_CHAR + 1) * SIZE_PER_CHAR_CHAR_SET_ROM - 1)

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define TILE_LAYER_WIDTH 64 //Best fit for 8*8 tiles on a 320 by 240 screen
#define TILE_LAYER_HEIGHT 32
#define TILE_LAYER_NO_TILES (TILE_LAYER_WIDTH * TILE_LAYER_HEIGHT)
#define TILE_MAP (TILE_LAYER_NO_TILES * 2) //Two bytes per tile

#define MAX_CHAR_ACROSS 40


void b6InitCharset();

#endif