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

typedef struct { //Careful about changing order _b9ViewToVera depends on this order
	byte width;
	byte height;
	byte transparency;
	byte* bmp;
	byte bitmapBank;
	boolean flipped;
} Cel;

typedef struct {
	byte numberOfCels;
	Cel* cels;
	byte celsBank;
} Loop;

typedef struct {
	boolean loaded;
	byte numberOfLoops;
	Loop* loops;
	byte loopsBank;
	char* description; //Always on the same bank as code
	byte* codeBlock;
	byte codeBlockBank;
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
	unsigned long veraSpriteDataAddress; //These two on a modern system would be pointers, but CX16 doesn't support three byte pointers
	unsigned long veraSpriteAttributeAddress;
} ViewTable;

#define VIEW_TABLE_SIZE  20  // 100
extern ViewTable viewtab[VIEW_TABLE_SIZE];

#define MAXVIEW  256
extern View* loadedViews;

extern BITMAP* spriteScreen;

extern void getViewTab(ViewTable* returnedViewTab, byte viewTabNumber);
extern void setViewTab(ViewTable* viewTab, byte viewTabNumber);

extern void getLoadedView(View* returnedLoadedView, byte loadedViewNumber);
extern void setLoadedView(View* loadedView, byte loadedViewNumber);

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_1)
void b9LoadViewFile(byte viewNum);
void b9DiscardView(byte viewNum);
void b9AddViewToTable(ViewTable* localViewtab, byte viewNum);
extern void b9SetCel(ViewTable* localViewtab, byte celNum);
extern void b9SetLoop(ViewTable* localViewtab, byte loopNum);
extern void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol);
extern void b9ResetViews();
extern void b9InitViews();
extern void b9InitObjects();
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
extern void bAUpdateObj(int entryNum);
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

#endif   /* _VIEW_H_ */

