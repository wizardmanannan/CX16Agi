/**************************************************************************
** VIEW.H
**************************************************************************/

#ifndef _VIEW_H_
#define _VIEW_H_

#include "general.h"
#include "stub.h"
#include "memoryManager.h"
#include "agifiles.h"
#include "picture.h"
#include "spriteMemoryManager.h"
#include "paletteManager.h"
#include "helpers.h"
#include "irq.h"

#define MAXVIEW  256
#define MAX_JOINED_SPRITES 6
#define MAX_SPRITES_ROW_OR_COLUMN_SIZE 4

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
} Cel;

#define PALETTE_NOT_SET 255

typedef enum {
	SPR_ATTR_8 = 0,
	SPR_ATTR_16 = 1,
	SPR_ATTR_32 = 2,
	SPR_ATTR_64 = 3
} SpriteAttributeSize;
extern byte* var;

typedef struct {
	byte numberOfCels;
	Cel* cels;
	byte celsBank;
	SpriteAttributeSize allocationHeight;
	SpriteAttributeSize allocationWidth;
	byte veraSlotsWidth;
	byte veraSlotsHeight;
	byte palette;
} Loop;

typedef struct {
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

#define DRAWN         0x0001
#define IGNOREBLOCKS  0x0002
#define FIXEDPRIORITY 0x0004
#define IGNOREHORIZON 0x0008
#define UPDATE        0x0010
#define CYCLING       0x0020
#define ANIMATED      0x0040
#define MOTION        0x0080
#define ONWATER       0x0100
#define IGNOREOBJECTS 0x0200
#define ONLAND        0x0800
#define FIXLOOP       0x2000

#define MAX_SPRITES_SLOTS_PER_VIEW_TAB 6
typedef struct {
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
} ViewTable;

#define SPRITE_SLOTS (VIEW_TABLE_SIZE)
extern ViewTable viewtab[VIEW_TABLE_SIZE];

extern View* loadedViews;

extern BITMAP* spriteScreen;

extern void getViewTab(ViewTable* returnedViewTab, byte viewTabNumber);
extern void setViewTab(ViewTable* viewTab, byte viewTabNumber);

extern void getLoadedView(View* returnedLoadedView, byte loadedViewNumber);
extern void setLoadedView(View* loadedView, byte loadedViewNumber);

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_1)
void b9LoadViewFile(byte viewNum);
void b9DiscardView(byte viewNum);
void b9AddViewToTable(ViewTable* localViewtab, byte viewNum, byte entryNum);
extern void b9SetCel(ViewTable* localViewtab, byte celNum);
extern void b9SetLoop(ViewTable* localViewtab, byte loopNum);
extern void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol);
extern void b9ResetViews();
extern void b9InitViews();
extern void b9InitObjects();
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
extern void bBUpdateObj(int entryNum);
extern void bADrawObject(int entryNum);
extern void bAFollowEgo(int entryNum);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_3)
extern void bBUpdateObjects();
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_4)
extern void bCCalcObjMotion();
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_5)
extern void bDShowObjectState(int objNum);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, SPRITE_UPDATED_BANK)
extern void bEClearSpriteAttributes();
#pragma wrapped-call (pop)

#endif   /* _VIEW_H_ */

