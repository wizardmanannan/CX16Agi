#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include "graphics.h"
#include "helpers.h"
#include "irq.h"
#include "general.h"

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

typedef unsigned long VeraSpriteAddress; 

#pragma wrapped-call (push, trampoline, GRAPHICS_BANK)
unsigned int b6SetPaletteToInt(byte paletteReference);
#pragma wrapped-call (pop);

#define PICTURE_WIDTH   160  /* Picture resolution */
#define PICTURE_HEIGHT  168
#define BITMAP_WIDTH 320
#define BITMAP_HEIGHT 240
#define BYTES_PER_ROW (SCREEN_WIDTH / 2)
#define STARTING_ROW ((SCREEN_HEIGHT / 2) - (PICTURE_HEIGHT / 2))
#define STARTING_BYTE (STARTING_ROW * BYTES_PER_ROW)

//The data is the top two fields are all stored at the same bank alloced address on the bank in the bank field
typedef struct ViewTableMetadata {
	VeraSpriteAddress** loopsVeraAddressesPointers;
	VeraSpriteAddress* veraAddresses;
	byte viewTableMetadataBank;
	byte viewNum;
	void* inactive; //Sometimes a single viewtab can have views swapped rapidly, we keep the previous ones here so we can easily switch between them
	byte inactiveBank;
	VeraSpriteAddress* backBuffers;
	boolean isOnBackBuffer;
	byte backBufferSize;
} ViewTableMetadata;

typedef struct Cel {
	byte width;
	byte height;
	byte transparency;
	byte* bmp;
	byte bitmapBank;
	boolean flipped;
	byte** splitCelPointers; //Some cels may have sprites that larger than the maximum 64x64 the CX16 allows. 
	//In this case the sprite is split and this array points to the places in the bmp where the indiviual segments are.  If the cel is not large enough this value is null. Note if split the cel data may be on a different bank to the view file data, hence the bank
	byte splitCelBank;
	byte splitSegments; //If this is not split it will be 1
	byte veraSlotsWidth;
	byte veraSlotsHeight;
} Cel;

typedef enum {
	SPR_ATTR_8 = 0,
	SPR_ATTR_16 = 1,
	SPR_ATTR_32 = 2,
	SPR_ATTR_64 = 3
} SpriteAttributeSize;

typedef struct {
	byte numberOfCels;
	Cel* cels;
	byte celsBank;
	SpriteAttributeSize allocationHeight;
	SpriteAttributeSize allocationWidth;
	byte palette;
} Loop;

typedef struct View {
	boolean loaded;
	byte numberOfLoops;
	Loop* loops;
	byte loopsBank;
	char* description; //Always on the same bank as code
	byte* codeBlock;
	byte codeBlockBank;
	byte maxCels;
	byte maxVeraSlots;
} View;


typedef struct ViewTable {
	byte stepTime;
	byte stepTimeCount;
	word xPos;
	word yPos;
	byte currentView;
	View* viewData;             /* This pointer points to the loaded view */
	byte currentLoop;
	byte numberOfLoops;
	Loop* loopData;             /* ditto */
	byte currentCel;
	byte numberOfCels;
	Cel* celData;              /* ditto */
	word xsize;
	word ysize;
	byte stepSize;
	byte cycleTime;
	byte cycleTimeCount;
	byte direction;
	byte motion;
	byte cycleStatus;
	byte priority;
	word flags;
	byte param1;
	byte param2;
	byte param3;
	byte param4;
	boolean repositioned;
	byte staleCounter; //Required as we need to blit a moving object one more time after it stops moving to prevent screen glitches
	boolean stopped;
	byte previousX;
	byte previousY;
	boolean noAdvance;
} ViewTable;

#define VIEW_TABLE_SIZE  20 
#define SPRITE_SLOTS (VIEW_TABLE_SIZE)
#define MAXVIEW  256

extern ViewTable viewtab[VIEW_TABLE_SIZE];
extern View loadedViews[MAXVIEW];
extern ViewTableMetadata viewTableMetadata[SPRITE_SLOTS];
extern boolean* flag;


#endif