#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "memoryManager.h"
#include "helpers.h"
#include <cx16.h>
#include <stdlib.h>
#include "kernal.h"
#include "irq.h"

#define BYTES_PER_CHARACTER 16
#define NO_CHARS 160
#define SIZE_OF_CHARSET (BYTES_PER_CHARACTER * NO_CHARS)
#define BYTES_PER_CELL 2

#define ADDRESSSEL0 0 
#define ADDRESSSEL1 1

#define TRANSPARENT_CHAR 128
#define TOP_BORDER 158
#define BOTTOM_BORDER 157
#define LEFT_BORDER 159
#define RIGHT_BORDER 156
#define NEW_LINE 10
#define SPACE 32

#define TRANSPARENT_CHAR_BYTE (TRANSPARENT_CHAR * SIZE_PER_CHAR_CHAR_SET_ROM)
#define LAST_BYTE_TRANSPARENT_CHAR ((TRANSPARENT_CHAR + 1) * SIZE_PER_CHAR_CHAR_SET_ROM - 1)

#define TILE_LAYER_WIDTH 64 //Best fit for 8*8 tiles on a 320 by 240 screen
#define TILE_LAYER_HEIGHT 32
#define TILE_LAYER_NO_TILES (TILE_LAYER_WIDTH * TILE_LAYER_HEIGHT)
#define TILE_MAP (TILE_LAYER_NO_TILES * 2) //Two bytes per tile
#define DEFAULT_BOX_WIDTH (TILE_LAYER_WIDTH / 4)

#define TILE_LAYER_BYTES_PER_ROW (BYTES_PER_CELL * TILE_LAYER_WIDTH)

#define MAX_CHAR_ACROSS 40

#define TEXTBUFFER_SIZE 1000

#define FIRST_ROW 4

#define TEXTBOX_PALETTE_NUMBER 1
#define DISPLAY_PALETTE_NUMBER 2

void b6InitLayer1Mapbase();

void b3DisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth);
void b3FillChar(byte startLine, byte endLine, byte paletteNumber, byte charToFill);
void b3ClearLastPlacedText();

void trampolinefillChar(byte startLine, byte endLine, byte paletteNumber, byte charToFill);
void trampolineDisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth);

extern char textBuffer1[TEXTBUFFER_SIZE];
extern char textBuffer2[TEXTBUFFER_SIZE];


#endif