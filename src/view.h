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
#include "celToVeraZp.h"
#include "spriteGarbage.h"

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
void b9ResetSpriteMemory(boolean clearBuffer);
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
extern void bAWander(ViewTable* localViewTab, byte entryNum);
extern void bAFindPosition(int entryNum, ViewTable* viewTab);
extern void bBUpdateObj(int entryNum);
extern void bADrawObject(ViewTable* viewTab);
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

