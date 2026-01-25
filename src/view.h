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
#include "spriteAllocator.h"
#include "paletteManager.h"
#include "helpers.h"
#include "irq.h"
#include "celToVeraZp.h"
#include "fixed.h"
#include <limits.h>
#include "movement.h"

#define MAX_JOINED_SPRITES 6
#define MAX_SPRITES_ROW_OR_COLUMN_SIZE 4
#define PALETTE_NOT_SET 255

extern byte* var;

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
extern BITMAP* spriteScreen;

extern void getViewTab(ViewTable* returnedViewTab, byte viewTabNumber);
extern void setViewTab(ViewTable* viewTab, byte viewTabNumber);

extern void getLoadedView(View* returnedLoadedView, byte loadedViewNumber);
extern void setLoadedView(View* loadedView, byte loadedViewNumber);

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_1)
void b9LoadViewFile(byte viewNum);
void b9DiscardView(byte viewNum);
void b9SetView(byte viewNum, byte entryNum);
extern void b9SetCel(ViewTable* localViewTab, byte entryNum, byte celNum);
extern void b9SetLoop(ViewTable* localViewTab, byte entryNum, byte loopNum);
extern void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
extern void bAPopulatePrecomputedPriorityTable();
extern void bAResetSpriteMemory(boolean clearBuffer);
extern void bAInitViews();
extern void bAInitObjects();
extern void bAWander(ViewTable* localViewTab, byte entryNum);
extern void bBUpdateObj(int entryNum);
extern void bAResetViews();
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

extern byte priorityBase;

#define MIN_PRIORITY 4

#endif   /* _VIEW_H_ */

