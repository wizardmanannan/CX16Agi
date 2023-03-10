/**************************************************************************
** VIEW.H
**************************************************************************/

#ifndef _VIEW_H_
#define _VIEW_H_

#include "general.h"
#include "stub.h"
#include "memoryManager.h"
#include "agifiles.h"

typedef struct {
	byte width;
	byte height;
	byte transparency;
	BITMAP* bmp;
} Cel;

typedef struct {
	byte numberOfCels;
	Cel* cels;
	byte celBank;
} Loop;

typedef struct {
	boolean loaded;
	byte numberOfLoops;
	Loop* loops;
	byte loopsBank;
	char* description;
	byte descriptionBank;
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
	BITMAP* bgPic;             /* Storage for background behind drawn view */
	BITMAP* bgPri;
	word bgX;                  /* Position to place background bmp */
	word bgY;
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

#define TABLESIZE  20  // 100
extern ViewTable* viewtab;

#define MAXVIEW  256
extern View* loadedViews;

extern BITMAP* spriteScreen;

typedef void (*fnTrampolineViewUpdater0)(ViewTable* localViewtab);

typedef void (*fnTrampolineViewUpdater1Int)(ViewTable* localViewtab, int data);
typedef void (*fnTrampolineViewUpdater1BytePtr)(ViewTable* localViewtab, byte* data1);

typedef void (*fnTrampolineViewUpdater2Int)(ViewTable* localViewtab, int data1, int data2);

extern void getViewTab(ViewTable* returnedViewTab, byte viewTabNumber);
extern void setViewTab(ViewTable* viewTab, byte viewTabNumber);

extern void getLoadedView(View* returnedLoadedView, byte loadedViewNumber);
extern void setLoadedView(View* loadedView, byte loadedViewNumber);

extern void trampolineViewUpdater0(fnTrampolineViewUpdater0 func, ViewTable* localViewtab, byte bank);

extern void trampolineViewUpdater1Int(fnTrampolineViewUpdater1Int func, ViewTable* localViewtab, int celNum, byte bank);
extern void trampolineViewUpdater1Pointer(fnTrampolineViewUpdater1BytePtr func, ViewTable* localViewtab, byte* data, byte bank);

extern void trampolineAddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol);

void b9LoadViewFile(byte viewNum);
void b9DiscardView(byte viewNum);
void b9AddViewToTable(ViewTable* localViewtab, byte viewNum);

extern void bAUpdateObj(int entryNum);
extern void b9SetCel(ViewTable* localViewtab, byte celNum);
extern void b9SetLoop(ViewTable* localViewtab, byte loopNum);
extern void bADrawObject(int entryNum);
extern void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol);
extern void bDShowObjectState(int objNum);
extern void b9ResetViews();
extern void bCCalcObjMotion();
extern void b9InitViews();
extern void b9InitObjects();
extern void bBUpdateObjects();

#endif   /* _VIEW_H_ */

