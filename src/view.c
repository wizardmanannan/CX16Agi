/***************************************************************************
** VIEW.C
**
** These functions load VIEWs into memory. VIEWs are not automatically
** stored into the VIEW table when they are loaded. The VIEW table only
** holds those VIEWs that have been allocated a slot via the set.view()
** AGI function. The views that are stored in the VIEW table appear to
** be those that are going to be animated. Compare this with add-to-pics
** which are placed on the screen but are never dealt with again, or
** inventory item VIEWs that are shown but not controlled (animated).
**
** (c) 1997 Lance Ewing - Original code (3 July 97)
**                      - Changes       (25 Aug 97)
**                                      (15 Jan 98)
**                                      (22 Jul 98)
***************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "general.h"
#include "agifiles.h"
#include "view.h"

//#define VERBOSE_SET_VIEW;
//#define VERBOSE_SET_LOOPS
//#define VERBOSE_SET_CEL
//#define VERBOSE_LOAD_VIEWS;
//#define VERBOSE_UPDATE_OBJECTS

//#define VERBOSE_ALLOC_WATCH
//#define VERBOSE_ADD_TO_PIC;

View* loadedViews = (View*)&BANK_RAM[LOADED_VIEW_START];
BITMAP* spriteScreen;

extern byte* var;
extern boolean* flag;
extern char string[12][40];
extern byte horizon;
extern int dirnOfEgo;

#pragma bss-name (push, "BANKRAM09")
ViewTable viewtab[TABLESIZE];
#pragma bss-name (pop)

//Temp From Allogro
#define FONT_SIZE    224  
typedef struct FONT_8x8             /* a simple 8x8 font */
{
	unsigned char dat[FONT_SIZE][8];
} FONT_8x8;


typedef struct FONT_8x16            /* a simple 8x16 font */
{
	unsigned char dat[FONT_SIZE][16];
} FONT_8x16;


typedef struct FONT_PROP            /* a proportional font */
{
	BITMAP* dat[FONT_SIZE];
} FONT_PROP;


typedef struct FONT                 /* can be either */
{
	int height;
	union {
		FONT_8x8* dat_8x8;
		FONT_8x16* dat_8x16;
		FONT_PROP* dat_prop;
	} dat;
} FONT;

void textout(BITMAP* bmp, FONT* f, char* str, int x, int y, int color)
{

}

FONT* font;

//

extern void b9CelToVera(Cel* localCel, long veraAddress, byte pNum, byte bCol, byte drawingAreaWidth);

void getViewTab(ViewTable* returnedViewTab, byte viewTabNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = VIEWTAB_BANK;

	*returnedViewTab = viewtab[viewTabNumber];

	RAM_BANK = previousRamBank;
}

void setViewTab(ViewTable* localViewtab, byte viewTabNumber)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = VIEWTAB_BANK;

	viewtab[viewTabNumber] = *localViewtab;

	RAM_BANK = previousRamBank;
}

void getLoadedView(View* returnedLoadedView, byte loadedViewNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = LOADED_VIEW_BANK;

	*returnedLoadedView = loadedViews[loadedViewNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedView(View* loadedView, byte loadedViewNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = LOADED_VIEW_BANK;

	loadedViews[loadedViewNumber] = *loadedView;

	RAM_BANK = previousRamBank;
}

void getLoadedLoop(View* loadedView, Loop* returnedLocalLoop, byte localLoopNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedView->loopsBank;

	*returnedLocalLoop = loadedView->loops[localLoopNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedLoop(View* loadedView, Loop* localLoop, byte localLoopNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedView->loopsBank;

	loadedView->loops[localLoopNumber] = *localLoop;

	RAM_BANK = previousRamBank;
}

void getLoadedCel(Loop* loadedLoop, Cel* localCell, byte localCellNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedLoop->celsBank;

	*localCell = loadedLoop->cels[localCellNumber];

	RAM_BANK = previousRamBank;
}

void setLoadedCel(Loop* loadedLoop, Cel* localCell, byte localCellNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedLoop->celsBank;


	loadedLoop->cels[localCellNumber] = *localCell;

	RAM_BANK = previousRamBank;
}

#pragma code-name (push, "BANKRAM09")
extern byte spritesUpdatedBuffer[TOTAL_SIZE_SPRITES_UPDATED_BUFFER];
void b9InitSpriteData()
{
	memset(spritesUpdatedBuffer, 0, TOTAL_SIZE_SPRITES_UPDATED_BUFFER);
}


void b9InitViews()
{
	int i;
	View localView;
	for (i = 0; i < 256; i++) {
		localView.description = 0;
		localView.loaded = FALSE;
		localView.loops = 0;
		localView.loopsBank = 0;
		localView.numberOfLoops = 0;
		localView.codeBlock = NULL;
		localView.codeBlockBank = 0;

		setLoadedView(&localView, i);
	}

	spriteScreen = create_bitmap(160, 168);
}

void b9InitObjects()
{
	int entryNum;
	ViewTable localViewtab;

	//spriteScreen = create_bitmap(160, 168);

	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		localViewtab.stepTime = 1;
		localViewtab.stepTimeCount = 1;
		localViewtab.xPos = 0;
		localViewtab.yPos = 0;
		localViewtab.currentView = 0;
		localViewtab.viewData = NULL;
		localViewtab.currentLoop = 0;
		localViewtab.numberOfLoops = 0;
		localViewtab.loopData = NULL;
		localViewtab.currentCel = 0;
		localViewtab.numberOfCels = 0;
		localViewtab.celData = NULL;
		localViewtab.xsize = 0;
		localViewtab.ysize = 0;
		localViewtab.stepSize = 1;
		localViewtab.cycleTime = 1;
		localViewtab.cycleTimeCount = 1;
		localViewtab.direction = 0;
		localViewtab.motion = 0;
		localViewtab.cycleStatus = 0;
		localViewtab.priority = 0;
		localViewtab.flags = 0;

		setViewTab(&localViewtab, entryNum);
	}
}

void b9ResetViews()     /* Called after new.room */
{
	int entryNum;
	ViewTable localViewtab;

	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		localViewtab.flags &= ~(UPDATE | ANIMATED);

		setViewTab(&localViewtab, entryNum);
	}
}


#define VIEW_HEADER_BUFFER_SIZE 501
#define LOOP_HEADER_BUFFER_SIZE 501
#define VIEW_BUFFERS_BANK 9
extern byte viewHeaderBuffer[VIEW_HEADER_BUFFER_SIZE];
extern byte loopHeaderBuffer[LOOP_HEADER_BUFFER_SIZE];

#define POSITION_OF_NO_LOOPS 2
#define POSITION_OF_LOOPS_OFFSET 5
#define POSITION_OF_DESCRIPTION 3

#define NO_LOOPS_INDEX_BYTES_AVERAGE 14
#define NO_CELLS_INDEX_BYTES_AVERAGE 14

void setViewData(byte viewNum, AGIFile* tempAGI, View* localView)
{
	byte numberOfLoops;
	boolean isThereADescription;
	const char* description;
	int descriptionLength;
	byte loopsBank;
#ifdef VERBOSE_LOAD_VIEWS
	byte tmp[10];
#endif

	getLoadedView(localView, viewNum);

	if (!localView->loaded)
	{
		memCpyBankedBetween((byte*)&viewHeaderBuffer[0], VIEW_BUFFERS_BANK, tempAGI->code, tempAGI->codeBank, NO_LOOPS_INDEX_BYTES_AVERAGE + POSITION_OF_LOOPS_OFFSET); //Guessing at 7 loops, this should copy enough 99% of the time, and we can copy again once we have the first byte (loopCounter), is necessary . If there is less loops than we have just copied bytes which will be ignored
		numberOfLoops = viewHeaderBuffer[POSITION_OF_NO_LOOPS];

		if (numberOfLoops > NO_LOOPS_INDEX_BYTES_AVERAGE)
		{
			memCpyBankedBetween((byte*)&viewHeaderBuffer[0], VIEW_BUFFERS_BANK, tempAGI->code, tempAGI->codeBank, numberOfLoops * 2 + POSITION_OF_LOOPS_OFFSET);
		}


#ifdef VERBOSE_SET_VIEW
		printf("setting view number %d\n", viewNum);
		printf("there are %d loops\n", numberOfLoops);
#endif

		isThereADescription = (const char*)viewHeaderBuffer[POSITION_OF_DESCRIPTION] || viewHeaderBuffer[POSITION_OF_DESCRIPTION + 1];

		if (isThereADescription)
		{
			description = (const char*)(tempAGI->code + viewHeaderBuffer[POSITION_OF_DESCRIPTION] + viewHeaderBuffer[POSITION_OF_DESCRIPTION] * 256);
			descriptionLength = strLenBanked((char*)description, tempAGI->codeBank);

#ifdef VERBOSE_SET_VIEWS
			printf("Description length %d. The description is %s\n", descriptionLength, description);
#endif // VERBOSE_ALLOC_WATCH
		}
		else {

#ifdef VERBOSE_SET_VIEWS
			printf("there is no description");
#endif // VERBOSE_ALLOC_WATCH
			//TODO: Use /0
			description = (const char*)&tempAGI->code[POSITION_OF_DESCRIPTION]; //Going to be zero. We can do this even though we are not on the bank; there is no deference
		}

#ifdef VERBOSE_SET_VIEWS
		memCpyBanked(&tmp[0], (byte*)localView->description, localView->loopsBank, descriptionLength);
		if (tmp[0] == 0)
		{
			printf("The description is empty\n");
		}
		else
		{
			printf("The description is not empty\n");
			printf("It has a value of %s ", description);
		}
#endif

		localView->loaded = TRUE;
		localView->loops = (Loop*)b10BankedAlloc(numberOfLoops * sizeof(Loop), &loopsBank);

		localView->numberOfLoops = numberOfLoops;
		localView->description = description;
		localView->loopsBank = loopsBank;
	}
	else
	{
		printf("View %d is already loaded\n", viewNum);
	}

	localView->codeBlock = tempAGI->code;
	localView->codeBlockBank = tempAGI->codeBank;
	setLoadedView(&localView, viewNum);
}

#define POSITION_OF_NO_CELS 0
#define POSITION_OF_CELS_OFFSET 1
void setLoopData(AGIFile* tempAGI, View* localView, Loop* localLoop, byte* loopHeaderData, byte viewNum, byte loopNum)
{
	byte numberOfCels;
	byte celsBank;
#ifdef VERBOSE_LOAD_VIEWS
	byte tmp[10];
#endif

	if (localView->loaded) //If the view is loaded then so are all of the loops
	{
		getLoadedLoop(localView, localLoop, loopNum);

		memCpyBankedBetween(&loopHeaderBuffer[0], VIEW_BUFFERS_BANK, loopHeaderData, tempAGI->codeBank, NO_CELLS_INDEX_BYTES_AVERAGE + POSITION_OF_CELS_OFFSET); //Guessing at 7 cels, this should copy enough 99% of the time, and we can copy again once we have the first byte (loopCounter), is necessary . If there is less loops than we have just copied bytes which will be ignored
		numberOfCels = loopHeaderBuffer[POSITION_OF_NO_CELS];

		if (numberOfCels > NO_CELLS_INDEX_BYTES_AVERAGE)
		{
			memCpyBankedBetween((byte*)&loopHeaderData[0], VIEW_BUFFERS_BANK, loopHeaderData, tempAGI->codeBank, numberOfCels * 2 + POSITION_OF_CELS_OFFSET);
		}

#ifdef VERBOSE_SET_LOOPS
		printf("setting loop number %d on view %d\n", loopNum, viewNum);
		printf("there are %d cels\n", numberOfCels);
		printf("The address of loop header buffer is %p\n", &loopHeaderBuffer[0]);
		printf("Loop header data is at %p, on bank %d\n", loopHeaderData, tempAGI->codeBank);
#endif

		localLoop->cels = (Cel*)b10BankedAlloc(numberOfCels * sizeof(Cel), &celsBank);
		localLoop->numberOfCels = numberOfCels;
		localLoop->celsBank = celsBank;

		setLoadedLoop(localView, localLoop, loopNum);
	}
	else
	{
		printf("Fail %d is not loaded\n", viewNum);
	}
}

#define POSITION_OF_CEL_WIDTH 0
#define POSITION_OF_CEL_HEIGHT 1
#define POSTION_OF_CEL_TRANSPARENCY_AND_MIRRORING 2
#define POSITION_OF_CEL_DATA 3
#define CEL_HEADER_SIZE 3
/**************************************************************************
** loadViewFile
**
** Purpose: Loads a VIEW file into memory storing it in the loadedViews
** array.
**************************************************************************/
void b9LoadViewFile(byte viewNum)
{
	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	byte l, c, trans;
	View localView;
	Loop localLoop;
	Cel localCel;
	byte* cellPosition;
	int* loopOffsets = (int*)(viewHeaderBuffer + POSITION_OF_LOOPS_OFFSET);
	int* cellOffsets;
	byte celHeader[CEL_HEADER_SIZE];
#ifdef VERBOSE_LOAD_VIEWS
	printf("Attempt to load viewNum %d\n", viewNum);
#endif // VERBOSE_LOAD_VIEWS

	getLogicDirectory(&agiFilePosType, &viewdir[viewNum]);
	b6LoadAGIFile(VIEW, &agiFilePosType, &tempAGI);

#ifdef VERBOSE_LOAD_VIEWS
	printf("loaded agiFile of total size %u\n", tempAGI.totalSize);
#endif

	setViewData(viewNum, &tempAGI, &localView);

#ifdef VERBOSE_LOAD_VIEWS
	printf("The view desc %s, loaded %d, loops %p, loopsBank %d, numberOfLoops %d\n", localView.description, localView.loaded, localView.loops, localView.loopsBank, localView.numberOfLoops);
	printf("The address of viewHeaderBuffer is %p", &viewHeaderBuffer[0]);
#endif // VERBOSE_LOAD_VIEWS

#define POSITION_BYTES 2
	for (l = 0; l < localView.numberOfLoops; l++) {
#ifdef VERBOSE_LOAD_VIEWS
		printf("Loading loop %d at %x\n", l, loopOffsets[l]);
#endif // VERBOSE_LOAD_VIEWS

#ifdef VERBOSE_SET_LOOPS
		printf("View code starts at %p. The loop offset is %x, we means we expect to find a loop at %x\n", tempAGI.code, loopOffsets[l], tempAGI.code + loopOffsets[l]);
#endif // VERBOSE_SET_LOOPS

		setLoopData(&tempAGI, &localView, &localLoop, tempAGI.code + loopOffsets[l], viewNum, l);
		cellOffsets = (int*)(loopHeaderBuffer + POSITION_OF_CELS_OFFSET);

		for (c = 0; c < localLoop.numberOfCels; c++) {
			cellPosition = tempAGI.code + loopOffsets[l] + cellOffsets[c];
			memCpyBanked(celHeader, cellPosition, tempAGI.codeBank, CEL_HEADER_SIZE);

			getLoadedCel(&localLoop, &localCel, c);
			trans = celHeader[POSTION_OF_CEL_TRANSPARENCY_AND_MIRRORING];
			
			localCel.bitmapBank = tempAGI.codeBank;
			localCel.bmp = cellPosition + POSITION_OF_CEL_DATA;
			localCel.width = celHeader[POSITION_OF_CEL_WIDTH];
			localCel.height = celHeader[POSITION_OF_CEL_HEIGHT];
			localCel.flipped = (trans & 0x80) && (((trans & 0x70) >> 4) != l);

#ifdef VERBOSE_SET_CEL
			printf("The viewNum is %d\n", viewNum);
			printf("The address of celHeader is %p\n", celHeader);
			printf("bitmapBank %d, bmp %p, height %d, width %d, flipped %d \n", localCel.bitmapBank, localCel.bmp, localCel.height, localCel.width, localCel.flipped);
#endif // VERBOSE_SET_CEL
			setLoadedCel(&localLoop, &localCel, c);
		}
}
	setLoadedView(&localView, viewNum);
}

/***************************************************************************
** discardView
**
** Purpose: To deallocate memory associated with view number.
***************************************************************************/
void b9DiscardView(byte viewNum)
{
	byte l, c;
	View localView;
	Loop localLoop;
	Cel localCel;

	getLoadedView(&localView, viewNum);

	if (localView.loaded) {
		for (l = 0; l < localView.numberOfLoops; l++) {

			getLoadedLoop(&localView, &localLoop, l);

			for (c = 0; c < localLoop.numberOfCels; c++) {
				getLoadedCel(&localLoop, &localCel, c);
				localCel.bmp = NULL;
				localCel.height = 0;
				localCel.transparency = 0;
				localCel.width = 0;

				setLoadedCel(&localLoop, &localCel, c);
			}
#ifdef VERBOSE_ALLOC_WATCH
			printf("dealloc cels bank %p address %p\n", localLoop.celBank, (byte*)localLoop.cels);
#endif
			b10BankedDealloc((byte*)localLoop.cels, localLoop.celsBank);
			localLoop.celsBank = 0;
			localLoop.cels = NULL;
			localLoop.numberOfCels = 0;

			setLoadedLoop(&localView, &localLoop, l);
		}

#ifdef VERBOSE_ALLOC_WATCH
		printf("dealloc loops bank %p address %p\n", localView.loopsBank, localView.loops);
#endif // VERBOSE_ALLOC_WATCH
		b10BankedDealloc((byte*)localView.loops, localView.loopsBank);
		localView.loaded = FALSE;

		b10BankedDealloc(localView.codeBlock, localView.codeBlockBank);
		localView.codeBlock = NULL;
		localView.codeBlockBank = 0;

		setLoadedView(&localView, viewNum);
	}
}

void b9SetCel(ViewTable* localViewtab, byte celNum)
{
	Loop temp;
	View localLoadedView;
	Cel localCel;

	getLoadedView(&localLoadedView, localViewtab->currentView);
	getLoadedLoop(&localLoadedView, &temp, localViewtab->currentLoop);
	getLoadedCel(&temp, &localCel, celNum);

	localViewtab->currentCel = celNum;
	localViewtab->xsize = localCel.width;
	localViewtab->ysize = localCel.height;
}

void b9SetLoop(ViewTable* localViewtab, byte loopNum)
{
	View temp;
	Loop loop;
	getLoadedView(&temp, localViewtab->currentView);
	getLoadedLoop(&temp, &loop, loopNum);

	localViewtab->currentLoop = loopNum;
	localViewtab->numberOfCels = loop.numberOfCels;
	/* Might have to set the cel as well */
	/* It's probably better to do that in the set_loop function */
}


/**************************************************************************
** addViewToTable
**
** Purpose: To add a loaded VIEW to the VIEW table.
**************************************************************************/
void b9AddViewToTable(ViewTable* localViewtab, byte viewNum)
{
	View localView;
	Loop localLoop;

	getLoadedView(&localView, viewNum);
	getLoadedLoop(&localView, &localLoop, 0);

	localViewtab->currentView = viewNum;
	localViewtab->numberOfLoops = localView.numberOfLoops;
	b9SetLoop(localViewtab, 0);

	localViewtab->numberOfCels = localLoop.numberOfCels;
	b9SetCel(localViewtab, 0);
	/* Might need to set some more defaults here */
}

void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol)
{
	int i, j, trans, c, boxWidth;
	View localView;
	Loop localLoop;
	Cel localCel;

	getLoadedView(&localView, vNum);
	getLoadedLoop(&localView, &localLoop, lNum);
	getLoadedCel(&localLoop, &localCel, cNum);
	
#ifdef VERBOSE_ADD_TO_PIC
	printf("view %p loop %p cel %p\n", &localView, &localLoop, &localCel);
	printf("cel %d loaded %d bmp %p. View %d. Loop %d, Cel %d\n", cNum, localView.loaded, localCel.bmp, vNum, lNum, cNum);
	printf("x and y are (%d,%d). Adjusted Height %d. The address is %lx.\n ", x, y, y - localCel.height + 1, b2GetVeraPictureAddress(x, (y - localCel.height) + 1));
	printf("w %d h %d\n", localCel.width, localCel.height);
#endif // VERBOSE_ADD_TO_PIC

	b9CelToVera(&localCel, b4GetVeraPictureAddress(x, (y - localCel.height) + 1), pNum, bCol, BYTES_PER_ROW);

	//TODO: Finish implementing the priority and control line stuff
//
//	for (i = 0; i < w; i++) {
//		for (j = 0; j < h; j++) {
//			c = localCell.bmp->line[j][i];
//			if ((c != (trans + 1)) && (pNum >= priority->line[y + j][x + i])) {
//				priority->line[y + j][x + i] = pNum;
//				picture->line[y + j][x + i] = c;
//			}
//		}
//	}
//
//	/* Maybe the box height only extends to the next priority band */
//
//	boxWidth = ((h >= 7) ? 7 : h);
//	if (bCol < 4) rect(control, x, (y + h) - (boxWidth), (x + w) - 1, (y + h) - 1, bCol);
}

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_1)
/***************************************************************************
** agi_blit
***************************************************************************/
void b9AgiBlit(byte* bmp, int x, int y, int w, int h, byte trans, byte pNum)
{
	int i, j, c;

	//for (i = 0; i < w; i++) {
	//	for (j = 0; j < h; j++) {
	//		c = bmp->line[j][i];
	//		 Next line will be removed when error is found.
	//		if (((y + j) < 168) && ((x + i) < 160) && ((y + j) >= 0) && ((x + i) >= 0))

	//			if ((c != (trans + 1)) && (pNum >= priority->line[y + j][x + i])) {
	//				priority->line[y + j][x + i] = pNum;
	//				picture->line[y+j][x+i] = c;
	//				spriteScreen->line[y + j][x + i] = c;
	//			}
	//	}
	//}
}
#pragma wrapped-call (pop)

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0A")
#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
void bACalcDirection(ViewTable* localViewtab)
{
	if (!(localViewtab->flags & FIXLOOP)) {
		if (localViewtab->numberOfLoops < 4) {

			switch (localViewtab->direction) {
			case 1: break;
			case 2: b9SetLoop(localViewtab, 0); break;
			case 3: b9SetLoop(localViewtab, 0); break;
			case 4: b9SetLoop(localViewtab, 0); break;
			case 5: break;
			case 6: b9SetLoop(localViewtab, 1); break;
			case 7: b9SetLoop(localViewtab, 1); break;
			case 8: b9SetLoop(localViewtab, 1); break;
			}
		}
		else {
			switch (localViewtab->direction) {
			case 1: b9SetLoop(localViewtab, 3); break;
			case 2: b9SetLoop(localViewtab, 0); break;
			case 3: b9SetLoop(localViewtab, 0);  break;
			case 4: b9SetLoop(localViewtab, 0); break;
			case 5: b9SetLoop(localViewtab, 2); break;
			case 6: b9SetLoop(localViewtab, 1); break;
			case 7: b9SetLoop(localViewtab, 1); break;
			case 8: b9SetLoop(localViewtab, 1); break;

			}
		}
	}
}
#pragma wrapped-call (pop)

/* Called by draw() */
void bADrawObject(int entryNum)
{
	word objFlags;
	int dummy;
	ViewTable localViewtab;

	if (entryNum == 4) {
		dummy = 1;
	}

	getViewTab(&localViewtab, entryNum);

	objFlags = localViewtab.flags;

	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}

	bACalcDirection(&localViewtab);

	b9AgiBlit(localViewtab.celData->bmp,
		localViewtab.xPos,
		(localViewtab.yPos - localViewtab.ysize) + 1,
		localViewtab.xsize,
		localViewtab.ysize,
		localViewtab.celData->transparency & 0x0f,
		localViewtab.priority);

	setViewTab(&localViewtab, entryNum);
}

/***************************************************************************
** updateEgoDirection
**
** Purpose: To update var[6] when ego is moved with adjustPosition().
***************************************************************************/
void bAUpdateEgoDirection(int oldX, int oldY, int newX, int newY)
{
	int dx = (newX - oldX);
	int dy = (newY - oldY);
	ViewTable localViewtab;

	getViewTab(&localViewtab, 0);

	if ((dx == 0) && (dy == 0)) var[6] = dirnOfEgo = 0;
	if ((dx == 0) && (dy == -1)) var[6] = dirnOfEgo = 1;
	if ((dx == 1) && (dy == -1)) var[6] = dirnOfEgo = 2;
	if ((dx == 1) && (dy == 0)) var[6] = dirnOfEgo = 3;
	if ((dx == 1) && (dy == 1)) var[6] = dirnOfEgo = 4;
	if ((dx == 0) && (dy == 1)) var[6] = dirnOfEgo = 5;
	if ((dx == -1) && (dy == 1)) var[6] = dirnOfEgo = 6;
	if ((dx == -1) && (dy == 0)) var[6] = dirnOfEgo = 7;
	if ((dx == -1) && (dy == -1)) var[6] = dirnOfEgo = 8;

	bACalcDirection(&localViewtab);

	setViewTab(&localViewtab, 0);
}

/***************************************************************************
** adjustPosition
**
** Purpose: To adjust the given objects position so that it moves closer
** to the given position. The routine is similar to a line draw and is used
** for the move.obj. If the object is ego, then var[6] has to be updated.
***************************************************************************/
#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
void bAAdjustPosition(ViewTable* localViewtab, int fx, int fy)
{
	//int height, width, startX, startY, x1, y1, x2, y2, count, stepVal, dx, dy;
	//float x, y, addX, addY;
	//int dummy;

	///* Set up start and end points */
	//x1 = localViewtab.xPos;
	//y1 = localViewtab.yPos;
	//x2 = fx;
	//y2 = fy;

	//height = (y2 - y1);
	//width = (x2 - x1);
	//addX = (height==0?height:(float)width/abs(height));
	//addY = (width==0?width:(float)height/abs(width));

	///* Will find the point on the line that is stepSize pixels away */
	//if (abs(width) > abs(height)) {
	//   y = y1;
	//   addX = (width == 0? 0 : (width/abs(width)));
	//   switch ((int)addX) {
	//      case 0:
	//         if (addY < 0)
	//            localViewtab.direction = 1;
	//         else
	//            localViewtab.direction = 5;
	//         break;
	//      case -1:
	//         if (addY < 0)
	//            localViewtab.direction = 8;
	//         else if (addY > 0)
	//            localViewtab.direction = 6;
	//         else
	//            localViewtab.direction = 7;
	//         break;
	//      case 1:
	//         if (addY < 0)
	//            localViewtab.direction = 2;
	//         else if (addY > 0)
	//            localViewtab.direction = 4;
	//         else
	//            localViewtab.direction = 3;
	//         break;
	//   }
	//   count = 0;
	//   stepVal = localViewtab.stepSize;
	//   for (x=x1; (x!=x2) && (count<(stepVal+1)); x+=addX, count++) {
	//      dx = ceil(x);
	//      dy = ceil(y);
	   //    y+=addY;
	//   }
	//   if ((x == x2) && (count < (stepVal+1))) {
	//      dx = ceil(x);
	//      dy = ceil(y);
	//   }
	//}
	//else {
	//   x = x1;
	//   addY = (height == 0? 0 : (height/abs(height)));
	//   switch ((int)addY) {
	//      case 0:
	//         if (addX < 0)
	//            localViewtab.direction = 7;
	//         else
	//            localViewtab.direction = 3;
	//         break;
	//      case -1:
	//         if (addX < 0)
	//            localViewtab.direction = 8;
	//         else if (addX > 0)
	//            localViewtab.direction = 2;
	//         else
	//            localViewtab.direction = 1;
	//         break;
	//      case 1:
	//         if (addX < 0)
	//            localViewtab.direction = 6;
	//         else if (addX > 0)
	//            localViewtab.direction = 4;
	//         else
	//            localViewtab.direction = 5;
	//         break;
	//   }
	//   count = 0;
	//   stepVal = localViewtab.stepSize;
	//   for (y=y1; (y!=y2) && (count<(stepVal+1)); y+=addY, count++) {
	//      dx = ceil(x);
	//      dy = ceil(y);
	   //    x+=addX;
	//   }
	//   if ((y == y2) && (count < (stepVal+1))) {
	//      dx = ceil(x);
	//      dy = ceil(y);
	//   }
	//}

	//localViewtab.xPos = dx;
	//localViewtab.yPos = dy;

	//if (entryNum == 0) {
	//   updateEgoDirection(x1, y1, dx, dy);
	//}
}
#pragma wrapped-call (pop)

void bAFollowEgo(int entryNum) /* This needs to be more intelligent. */
{
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	bAAdjustPosition(&localViewtab, localViewtab.xPos, localViewtab.yPos);

	setViewTab(&localViewtab, entryNum);
}

#pragma wrapped-call (push, trampoline, VIEW_CODE_BANK_2)
void bANormalAdjust(int entryNum, int dx, int dy)
{
	int tempX, tempY, testX, startX, endX, waterCount = 0;
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	tempX = (localViewtab.xPos + dx);
	tempY = (localViewtab.yPos + dy);

	if (entryNum == 0) {
		if (tempX < 0) {   /* Hit left edge */
			var[2] = 4;
			return;
		}
		if (tempX > (160 - localViewtab.xsize)) {   /* Hit right edge */
			var[2] = 2;
			return;
		}
		if (tempY > 167) {   /* Hit bottom edge */
			var[2] = 3;
			return;
		}
		if (tempY < horizon) {   /* Hit horizon */
			var[2] = 1;
			return;
		}
	}
	else {   /* For all other objects */
		if (tempX < 0) {   /* Hit left edge */
			var[5] = 4;
			var[4] = entryNum;
			return;
		}
		if (tempX > (160 - localViewtab.xsize)) {   /* Hit right edge */
			var[5] = 2;
			var[4] = entryNum;
			return;
		}
		if (tempY > 167) {   /* Hit bottom edge */
			var[5] = 3;
			var[4] = entryNum;
			return;
		}
		if (tempY < horizon) {   /* Hit horizon */
			var[5] = 1;
			var[4] = entryNum;
			return;
		}
	}

	if (entryNum == 0) {
		flag[3] = 0;
		flag[0] = 0;

		/* End points of the base line */
		startX = tempX;
		endX = startX + localViewtab.xsize;
		for (testX = startX; testX < endX; testX++) {
			switch (control->line[tempY][testX]) {
			case 0: return;   /* Unconditional obstacle */
			case 1:
				if (localViewtab.flags & IGNOREBLOCKS) break;
				return;    /* Conditional obstacle */
			case 3:
				waterCount++;
				break;
			case 2: flag[3] = 1; /* Trigger */
				localViewtab.xPos = tempX;
				localViewtab.yPos = tempY;
				return;
			}
		}
		if (waterCount == localViewtab.xsize) {
			localViewtab.xPos = tempX;
			localViewtab.yPos = tempY;
			flag[0] = 1;
			return;
		}
	}
	else {
		/* End points of the base line */
		startX = tempX;
		endX = startX + localViewtab.xsize;
		for (testX = startX; testX < endX; testX++) {
			if ((localViewtab.flags & ONWATER) &&
				(control->line[tempY][testX] != 3)) {
				return;
			}
		}
	}

	localViewtab.xPos = tempX;
	localViewtab.yPos = tempY;

	setViewTab(&localViewtab, entryNum);
}
#pragma wrapped-call (pop)


void bAUpdateObj(int entryNum)
{
	int oldX, oldY, celNum;
	word objFlags;
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	objFlags = localViewtab.flags;


	//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

	   //if (objFlags & UPDATE) {

	if (objFlags & CYCLING) {
		localViewtab.cycleTimeCount++;
		if (localViewtab.cycleTimeCount >
			localViewtab.cycleTime) {
			localViewtab.cycleTimeCount = 1;
			celNum = localViewtab.currentCel;
			switch (localViewtab.cycleStatus) {
			case 0: /* normal.cycle */
				celNum++;
				if (celNum >= localViewtab.numberOfCels)
					celNum = 0;

				b9SetCel(&localViewtab, celNum);
				break;
			case 1: /* end.of.loop */
				celNum++;
				if (celNum >= localViewtab.numberOfCels) {
					flag[localViewtab.param1] = 1;
					/* localViewtab.flags &= ~CYCLING; */
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 2: /* reverse.loop */
				celNum--;
				if (celNum < 0) {
					flag[localViewtab.param1] = 1;
					/* localViewtab.flags &= ~CYCLING; */
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 3: /* reverse.cycle */
				celNum--;
				if (celNum < 0)
					celNum = localViewtab.numberOfCels - 1;
				b9SetCel(&localViewtab, celNum);
				break;
			}
		}
	} /* CYCLING */

	if (objFlags & MOTION) {
		localViewtab.stepTimeCount++;
		if (localViewtab.stepTimeCount >
			localViewtab.stepTime) {
			localViewtab.stepTimeCount = 1;
			switch (localViewtab.motion) {
			case 0: /* normal.motion */
				switch (localViewtab.direction) {
				case 0: break;
				case 1: bANormalAdjust(entryNum, 0, -1); break;
				case 2: bANormalAdjust(entryNum, 1, -1); break;
				case 3: bANormalAdjust(entryNum, 1, 0); break;
				case 4: bANormalAdjust(entryNum, 1, 1); break;
				case 5: bANormalAdjust(entryNum, 0, 1); break;
				case 6: bANormalAdjust(entryNum, -1, 1); break;
				case 7: bANormalAdjust(entryNum, -1, 0); break;
				case 8: bANormalAdjust(entryNum, -1, -1); break;
				}
				getViewTab(&localViewtab, entryNum);
				break;
			case 1: /* wander */
				oldX = localViewtab.xPos;
				oldY = localViewtab.yPos;
				switch (localViewtab.direction) {
				case 0: break;
				case 1: bANormalAdjust(entryNum, 0, -1); break;
				case 2: bANormalAdjust(entryNum, 1, -1); break;
				case 3: bANormalAdjust(entryNum, 1, 0); break;
				case 4: bANormalAdjust(entryNum, 1, 1); break;
				case 5: bANormalAdjust(entryNum, 0, 1); break;
				case 6: bANormalAdjust(entryNum, -1, 1); break;
				case 7: bANormalAdjust(entryNum, -1, 0); break;
				case 8: bANormalAdjust(entryNum, -1, -1); break;
				}
				getViewTab(&localViewtab, entryNum);
				if ((localViewtab.xPos == oldX) &&
					(localViewtab.yPos == oldY)) {
					localViewtab.direction = (rand() % 8) + 1;
				}
				break;
			case 2: /* follow.ego */
				break;
			case 3: /* move.obj */
				bAAdjustPosition(&localViewtab, localViewtab.param1,
					localViewtab.param2);

				if ((localViewtab.xPos == localViewtab.param1) &&
					(localViewtab.yPos == localViewtab.param2)) {
					localViewtab.motion = 0;
					localViewtab.flags &= ~MOTION;
					flag[localViewtab.param4] = 1;
				}
				break;
			}
		}
	} /* MOTION */

 //} /* UPDATE */


	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}


	if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		/* Draw new cel onto picture\priority bitmaps */
		b9AgiBlit(localViewtab.celData->bmp,
			localViewtab.xPos,
			(localViewtab.yPos - localViewtab.ysize) + 1,
			localViewtab.xsize,
			localViewtab.ysize,
			localViewtab.celData->transparency & 0x0f,
			localViewtab.priority);
	}

	setViewTab(&localViewtab, entryNum);
	show_mouse(NULL);
	stretch_sprite(agi_screen, spriteScreen, 0, 0, 640, 336);
	show_mouse(screen);
	b6ShowPicture();
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0B")

/* Called by force.update */
void bBUpdateObj2(int entryNum)
{
	int oldX, oldY, celNum;
	word objFlags;
	ViewTable localViewtab;

	getViewTab(&localViewtab, entryNum);

	objFlags = localViewtab.flags;


	//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

	   //if (objFlags & UPDATE) {

	if (objFlags & CYCLING) {
		localViewtab.cycleTimeCount++;
		if (localViewtab.cycleTimeCount >
			localViewtab.cycleTime) {
			localViewtab.cycleTimeCount = 1;
			celNum = localViewtab.currentCel;

			setViewTab(&localViewtab, entryNum);
			switch (localViewtab.cycleStatus) {
			case 0: /* normal.cycle */
				celNum++;
				if (celNum >= localViewtab.numberOfCels)
					celNum = 0;
				b9SetCel(&localViewtab, celNum);
				break;
			case 1: /* end.of.loop */
				celNum++;
				if (celNum >= localViewtab.numberOfCels) {
					flag[localViewtab.param1] = 1;
					localViewtab.flags &= ~CYCLING;
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 2: /* reverse.loop */
				celNum--;
				if (celNum < 0) {
					flag[localViewtab.param1] = 1;
					localViewtab.flags &= ~CYCLING;
				}
				else
					b9SetCel(&localViewtab, celNum);
				break;
			case 3: /* reverse.cycle */
				celNum--;
				if (celNum < 0)
					celNum = localViewtab.numberOfCels - 1;
				b9SetCel(&localViewtab, celNum);
				break;
			}
			getViewTab(&localViewtab, entryNum);
		}
	} /* CYCLING */

	if (objFlags & MOTION) {
		localViewtab.stepTimeCount++;
		if (localViewtab.stepTimeCount >
			localViewtab.stepTime) {
			localViewtab.stepTimeCount = 1;

			setViewTab(&localViewtab, entryNum);
			switch (localViewtab.motion) {
			case 0: /* normal.motion */
				switch (localViewtab.direction) {
				case 0: break;
				case 1: bANormalAdjust(entryNum, 0, -1); break;
				case 2: bANormalAdjust(entryNum, 1, -1); break;
				case 3: bANormalAdjust(entryNum, 1, 0); break;
				case 4: bANormalAdjust(entryNum, 1, 1); break;
				case 5: bANormalAdjust(entryNum, 0, 1); break;
				case 6: bANormalAdjust(entryNum, -1, 1); break;
				case 7: bANormalAdjust(entryNum, -1, 0); break;
				case 8: bANormalAdjust(entryNum, -1, -1); break;
				}
				break;
			case 1: /* wander */
				oldX = localViewtab.xPos;
				oldY = localViewtab.yPos;
				switch (localViewtab.direction) {
				case 0: break;
				case 1: bANormalAdjust(entryNum, 0, -1); break;
				case 2: bANormalAdjust(entryNum, 1, -1); break;
				case 3: bANormalAdjust(entryNum, 1, 0); break;
				case 4: bANormalAdjust(entryNum, 1, 1); break;
				case 5: bANormalAdjust(entryNum, 0, 1); break;
				case 6: bANormalAdjust(entryNum, -1, 1); break;
				case 7: bANormalAdjust(entryNum, -1, 0); break;
				case 8: bANormalAdjust(entryNum, -1, -1); break;
				}
				if ((localViewtab.xPos == oldX) &&
					(localViewtab.yPos == oldY)) {
					localViewtab.direction = (rand() % 8) + 1;
				}
				break;
			case 2: /* follow.ego */
				break;
			case 3: /* move.obj */
				bAAdjustPosition(&localViewtab, localViewtab.param1,
					localViewtab.param2);
				if ((localViewtab.xPos == localViewtab.param1) &&
					(localViewtab.yPos == localViewtab.param2)) {
					localViewtab.motion = 0;
					localViewtab.flags &= ~MOTION;
					flag[localViewtab.param4] = 1;
				}
				break;
			}
		}
	} /* MOTION */

 //} /* UPDATE */

	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}

	/* Draw new cel onto picture\priority bitmaps */
	/*
	agi_blit(localViewtab.celData->bmp,
	   localViewtab.xPos,
	   (localViewtab.yPos - localViewtab.ysize) + 1,
	   localViewtab.xsize,
	   localViewtab.ysize,
	   localViewtab.celData->transparency & 0x0f,
	   localViewtab.priority);
	*/
	//}

	if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		/* Draw new cel onto picture\priority bitmaps */
		b9AgiBlit(localViewtab.celData->bmp,
			localViewtab.xPos,
			(localViewtab.yPos - localViewtab.ysize) + 1,
			localViewtab.xsize,
			localViewtab.ysize,
			localViewtab.celData->transparency & 0x0f,
			localViewtab.priority);
	}

	setViewTab(&localViewtab, entryNum);
	b6ShowPicture();
}

void bBUpdateObjects()
{
	int entryNum, celNum, oldX, oldY;
	word objFlags;
	ViewTable localViewtab;

	/* If the show.pic() command was executed, display the picture
	** with this object update.
	*/
	/*
	if (okToShowPic) {
	   okToShowPic = FALSE;
	   blit(picture, spriteScreen, 0, 0, 0, 0, 160, 168);
	} else {
	   clear(spriteScreen);
	}*/
	clear(spriteScreen);

	/******************* Place all background bitmaps *******************/
	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		   /* Add saved background to picture\priority bitmaps */
		//}
		setViewTab(&localViewtab, entryNum);
	}

	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		objFlags = localViewtab.flags;
		getViewTab(&localViewtab, entryNum);

#ifdef VERBOSE_UPDATE_OBJECTS
		printf("Checking entry num %d it has objFlags of %d \n", entryNum, objFlags);
#endif // VERBOSE_UPDATE_OBJECTS

		if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

			if (objFlags & UPDATE) {

				if (objFlags & CYCLING) {

#ifdef VERBOSE_UPDATE_OBJECTS
					printf("Now inside %d\n", entryNum);
#endif // VERBOSE_UPDATE_OBJECTS

					localViewtab.cycleTimeCount++;
					if (localViewtab.cycleTimeCount >
						localViewtab.cycleTime) {
						localViewtab.cycleTimeCount = 1;
						celNum = localViewtab.currentCel;

						setViewTab(&localViewtab, entryNum);

						switch (localViewtab.cycleStatus) {
						case 0: /* normal.cycle */
							celNum++;
							if (celNum >= localViewtab.numberOfCels)
								celNum = 0;
							b9SetCel(&localViewtab, celNum);
							break;
						case 1: /* end.of.loop */
							//Debug Here
							celNum++;
							if (celNum >= localViewtab.numberOfCels) {

								flag[localViewtab.param1] = 1;
								/* localViewtab.flags &= ~CYCLING; */
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 2: /* reverse.loop */
							celNum--;
							if (celNum < 0) {
								flag[localViewtab.param1] = 1;
								/* localViewtab.flags &= ~CYCLING; */
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 3: /* reverse.cycle */
							celNum--;
							if (celNum < 0)
								celNum = localViewtab.numberOfCels - 1;
							b9SetCel(&localViewtab, celNum);
							break;
						}
						setViewTab(&localViewtab, entryNum);
					}
			} /* CYCLING */
		} /* UPDATE */

		/* Determine priority for unfixed priorities */
			if (!(objFlags & FIXEDPRIORITY)) {
				if (localViewtab.yPos < 60)
					localViewtab.priority = 4;
				else
					localViewtab.priority = (localViewtab.yPos / 12 + 1);
			}
	}

		setViewTab(&localViewtab, entryNum);
}

	/* Draw all cels */
	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
			/* Draw new cel onto picture\priority bitmaps */
			b9AgiBlit(localViewtab.celData->bmp,
				localViewtab.xPos,
				(localViewtab.yPos - localViewtab.ysize) + 1,
				localViewtab.xsize,
				localViewtab.ysize,
				localViewtab.celData->transparency & 0x0f,
				localViewtab.priority);
		}

		setViewTab(&localViewtab, entryNum);
	}

	show_mouse(NULL);
	stretch_sprite(agi_screen, spriteScreen, 0, 0, 640, 336);
	show_mouse(screen);
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0C")

void bCupdateObjects2()
{
	int entryNum, celNum, oldX, oldY;
	word objFlags;
	ViewTable localViewtab;

	/* Place all background bitmaps */
	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		//if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		   /* Add saved background to picture\priority bitmaps */
		//}

		setViewTab(&localViewtab, entryNum);
	}

	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;

		if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {

			if (objFlags & UPDATE) {

				if (objFlags & CYCLING) {
					localViewtab.cycleTimeCount++;
					if (localViewtab.cycleTimeCount >
						localViewtab.cycleTime) {
						localViewtab.cycleTimeCount = 1;
						celNum = localViewtab.currentCel;
						switch (localViewtab.cycleStatus) {
						case 0: /* normal.cycle */
							celNum++;
							if (celNum >= localViewtab.numberOfCels)
								celNum = 0;
							b9SetCel(&localViewtab, celNum);
							break;
						case 1: /* end.of.loop */
							celNum++;
							if (celNum >= localViewtab.numberOfCels) {
								flag[localViewtab.param1] = 1;
								localViewtab.flags &= ~CYCLING;
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 2: /* reverse.loop */
							celNum--;
							if (celNum < 0) {
								flag[localViewtab.param1] = 1;
								localViewtab.flags &= ~CYCLING;
							}
							else
								b9SetCel(&localViewtab, celNum);
							break;
						case 3: /* reverse.cycle */
							celNum--;
							if (celNum < 0)
								celNum = localViewtab.numberOfCels - 1;
							b9SetCel(&localViewtab, celNum);
							break;
						}
					}
				} /* CYCLING */
	/*
				if (objFlags & MOTION) {
				   localViewtab.stepTimeCount++;
				   if (localViewtab.stepTimeCount >
					   localViewtab.stepTime) {
					  localViewtab.stepTimeCount = 1;
					  switch (localViewtab.motion) {
						 case 0: // normal.motion
							switch (localViewtab.direction) {
							   case 0: break;
							   case 1: normalAdjust(entryNum, 0, -1); break;
							   case 2: normalAdjust(entryNum, 1, -1); break;
							   case 3: normalAdjust(entryNum, 1, 0); break;
							   case 4: normalAdjust(entryNum, 1, 1); break;
							   case 5: normalAdjust(entryNum, 0, 1); break;
							   case 6: normalAdjust(entryNum, -1, 1); break;
							   case 7: normalAdjust(entryNum, -1, 0); break;
							   case 8: normalAdjust(entryNum, -1, -1); break;
							}
							break;
						 case 1: // wander
							oldX = localViewtab.xPos;
							oldY = localViewtab.yPos;
							switch (localViewtab.direction) {
							   case 0: break;
							   case 1: normalAdjust(entryNum, 0, -1); break;
							   case 2: normalAdjust(entryNum, 1, -1); break;
							   case 3: normalAdjust(entryNum, 1, 0); break;
							   case 4: normalAdjust(entryNum, 1, 1); break;
							   case 5: normalAdjust(entryNum, 0, 1); break;
							   case 6: normalAdjust(entryNum, -1, 1); break;
							   case 7: normalAdjust(entryNum, -1, 0); break;
							   case 8: normalAdjust(entryNum, -1, -1); break;
							}
							if ((localViewtab.xPos == oldX) &&
								(localViewtab.yPos == oldY)) {
							   localViewtab.direction = (rand() % 8) + 1;
							}
							break;
						 case 2: // follow.ego
							break;
						 case 3: // move.obj
							adjustPosition(entryNum, localViewtab.param1,
							   localViewtab.param2);
							if ((localViewtab.xPos == localViewtab.param1) &&
								(localViewtab.yPos == localViewtab.param2)) {
							   localViewtab.motion = 0;
							   localViewtab.flags &= ~MOTION;
							   flag[localViewtab.param4] = 1;
							}
							break;
					  }
				   }
				} // MOTION
	*/
			} /* UPDATE */

			/* Determine priority for unfixed priorities */
			if (!(objFlags & FIXEDPRIORITY)) {
				if (localViewtab.yPos < 60)
					localViewtab.priority = 4;
				else
					localViewtab.priority = (localViewtab.yPos / 12 + 1);
			}

			/* Draw new cel onto picture\priority bitmaps */
			/*
			agi_blit(localViewtab.celData->bmp,
			   localViewtab.xPos,
			   (localViewtab.yPos - localViewtab.ysize) + 1,
			   localViewtab.xsize,
			   localViewtab.ysize,
			   localViewtab.celData->transparency & 0x0f,
			   localViewtab.priority);
			*/
			setViewTab(&localViewtab, entryNum);
		}
	}

	/* Draw all cels */
	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {
		objFlags = localViewtab.flags;
		getViewTab(&localViewtab, entryNum);

		if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
			/* Draw new cel onto picture\priority bitmaps */
			b9AgiBlit(localViewtab.celData->bmp,
				localViewtab.xPos,
				(localViewtab.yPos - localViewtab.ysize) + 1,
				localViewtab.xsize,
				localViewtab.ysize,
				localViewtab.celData->transparency & 0x0f,
				localViewtab.priority);
		}

		setViewTab(&localViewtab, entryNum);
	}

	b6ShowPicture();
}

void bCCalcObjMotion()
{
	int entryNum, celNum, oldX, oldY, steps = 0;
	byte randomNum;
	word objFlags;
	ViewTable localViewtab;
	ViewTable localViewtab0;

	getViewTab(&localViewtab0, 0);

	for (entryNum = 0; entryNum < TABLESIZE; entryNum++) {

		getViewTab(&localViewtab, entryNum);

		objFlags = localViewtab.flags;
		//Warning
		if ((objFlags & MOTION) && (objFlags & UPDATE)) {
			localViewtab.stepTimeCount++;
			if (localViewtab.stepTimeCount >
				localViewtab.stepTime) {
				localViewtab.stepTimeCount = 1;

				switch (localViewtab.motion) {
				case 0: /* normal.motion */
					switch (localViewtab.direction) {
					case 0: break;
					case 1: bANormalAdjust(entryNum, 0, -1); break;
					case 2: bANormalAdjust(entryNum, 0, -1); break;
					case 3: bANormalAdjust(entryNum, 1, 0); break;
					case 4: bANormalAdjust(entryNum, 1, 1); break;
					case 5: bANormalAdjust(entryNum, 0, 1); break;
					case 6: bANormalAdjust(entryNum, -1, 1); break;
					case 7: bANormalAdjust(entryNum, -1, 0); break;
					case 8: bANormalAdjust(entryNum, -1, -1);
					}
					break;
				case 1: /* wander */
					oldX = localViewtab.xPos;
					oldY = localViewtab.yPos;
					switch (localViewtab.direction) {
					case 0: break;
					case 1: bANormalAdjust(entryNum, 0, -1); break;
					case 2: bANormalAdjust(entryNum, 0, -1); break;
					case 3: bANormalAdjust(entryNum, 1, 0); break;
					case 4: bANormalAdjust(entryNum, 1, 1); break;
					case 5: bANormalAdjust(entryNum, 0, 1); break;
					case 6: bANormalAdjust(entryNum, -1, 1); break;
					case 7: bANormalAdjust(entryNum, -1, 0); break;
					case 8: bANormalAdjust(entryNum, -1, -1); break;
					}
					if ((localViewtab.xPos == oldX) &&
						(localViewtab.yPos == oldY)) {
						randomNum = (byte)(rand() % 8) + 1;
						localViewtab.direction = 5;
					}
					break;
				case 2: /* follow.ego */
					bAFollowEgo(entryNum);
					if ((localViewtab.xPos == localViewtab0.xPos) &&
						(localViewtab.yPos == localViewtab0.yPos)) {
						localViewtab.motion = 0;
						localViewtab.flags &= ~MOTION;
						flag[localViewtab.param2] = 1;
						/* Not sure about this next line */
						localViewtab.stepSize = localViewtab.param1;
					}
					break;
				case 3: /* move.obj */
					if (flag[localViewtab.param4]) break;
					for (steps = 0; steps < localViewtab.stepSize; steps++) {
						bAAdjustPosition(entryNum, (int)localViewtab.param1,
							(int)localViewtab.param2);
						if ((localViewtab.xPos == localViewtab.param1) &&
							(localViewtab.yPos == localViewtab.param2)) {
							/* These lines really are guess work */
							localViewtab.motion = 0;
							//localViewtab.flags &= ~MOTION;
							localViewtab.direction = 0;
							if (entryNum == 0) var[6] = 0;
							flag[localViewtab.param4] = 1;
							localViewtab.stepSize = localViewtab.param3;
							break;
						}
					}
					break;
				}
			}
		} /* MOTION */

		/* Automatic change of direction if needed */
		bACalcDirection(&localViewtab);

		setViewTab(&localViewtab, entryNum);
	}
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM0D")

///***************************************************************************
//** showView
//**
//** Purpose: To display all the cells of VIEW.
//***************************************************************************/
//void bDShowView(int viewNum)
//{
//	int loopNum, celNum, maxHeight, totalWidth, totalHeight = 5;
//	int startX = 0, startY = 0;
//	BITMAP* temp = create_bitmap(800, 600);
//	BITMAP* scn = create_bitmap(320, 240);
//	char viewString[20], loopString[3];
//	boolean stillViewing = TRUE;
//
//	clear_to_color(temp, 15);
//
//	for (loopNum = 0; loopNum < loadedViews[viewNum].numberOfLoops; loopNum++) {
//		maxHeight = 0;
//		totalWidth = 25;
//		sprintf(loopString, "%2d", loopNum);
//		drawString(temp, loopString, 2, totalHeight, 0, 15);
//		for (celNum = 0; celNum < loadedViews[viewNum].loops[loopNum].numberOfCels; celNum++) {
//			if (maxHeight < loadedViews[viewNum].loops[loopNum].cels[celNum].height)
//				maxHeight = loadedViews[viewNum].loops[loopNum].cels[celNum].height;
//			//blit(loadedViews[viewNum].loops[loopNum].cels[celNum].bmp, temp,
//			//   0, 0, totalWidth, totalHeight,
//			//   loadedViews[viewNum].loops[loopNum].cels[celNum].width,
//			//   loadedViews[viewNum].loops[loopNum].cels[celNum].height);
//			stretch_blit(loadedViews[viewNum].loops[loopNum].cels[celNum].bmp,
//				temp, 0, 0,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].width,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].height,
//				totalWidth, totalHeight,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].width * 2,
//				loadedViews[viewNum].loops[loopNum].cels[celNum].height);
//			totalWidth += loadedViews[viewNum].loops[loopNum].cels[celNum].width * 2;
//			totalWidth += 3;
//		}
//		if (maxHeight < 10) maxHeight = 10;
//		totalHeight += maxHeight;
//		totalHeight += 3;
//	}
//
//	if (strcmp(loadedViews[viewNum].description, "") != 0) {
//		int i, counter = 0, descLine = 0, maxLen = 0, strPos;
//		char* tempString = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
//		char* string = loadedViews[viewNum].description;
//
//		totalHeight += 20;
//
//		for (i = 0; i < strlen(string); i++) {
//			if (counter++ > 30) {
//				for (strPos = strlen(tempString) - 1; strPos >= 0; strPos--)
//					if (tempString[strPos] == ' ') break;
//				tempString[strPos] = 0;
//				drawString(temp, tempString, 27, totalHeight + descLine, 0, 15);
//				sprintf(tempString, "%s%c", &tempString[strPos + 1], string[i]);
//				descLine += 8;
//				if (strPos > maxLen) maxLen = strPos;
//				counter = strlen(tempString);
//			}
//			else {
//				if (string[i] == 0x0A) {
//					sprintf(tempString, "%s\\n", tempString);
//					counter++;
//				}
//				else {
//					if (string[i] == '\"') {
//						sprintf(tempString, "%s\\\"", tempString);
//						counter++;
//					}
//					else {
//						sprintf(tempString, "%s%c", tempString, string[i]);
//					}
//				}
//			}
//		}
//		drawString(temp, tempString, 27, totalHeight + descLine, 0, 15);
//		rect(temp, 25, totalHeight - 2, 25 + (maxLen + 1) * 8 + 3,
//			(totalHeight - 2) + (descLine + 8) + 3, 4);
//	}
//
//	sprintf(viewString, "view.%d", viewNum);
//	rect(scn, 0, 0, 319, 9, 4);
//	rectfill(scn, 1, 1, 318, 8, 4);
//	drawString(scn, viewString, 130, 1, 0, 4);
//	rect(scn, 310, 9, 319, 230, 4);
//	rectfill(scn, 311, 10, 318, 229, 3);
//	rect(scn, 310, 230, 319, 239, 4);
//	drawChar(scn, 0xB1, 311, 231, 4, 3);
//	drawChar(scn, 0x18, 311, 11, 4, 3);
//	drawChar(scn, 0x19, 311, 222, 4, 3);
//	rect(scn, 0, 230, 310, 239, 4);
//	rectfill(scn, 1, 231, 309, 238, 3);
//	drawChar(scn, 0x1B, 2, 231, 4, 3);
//	drawChar(scn, 0x1A, 302, 231, 4, 3);
//	blit(temp, scn, 0, 0, 0, 10, 310, 220);
//	stretch_blit(scn, screen, 0, 0, 320, 240, 0, 0, 640, 480);
//
//	while (stillViewing) {
//		switch (readkey() >> 8) {
//		case KEY_ESC:
//			stillViewing = FALSE;
//			break;
//		case KEY_UP:
//			startY -= 5;
//			break;
//		case KEY_DOWN:
//			startY += 5;
//			break;
//		case KEY_LEFT:
//			startX -= 5;
//			break;
//		case KEY_RIGHT:
//			startX += 5;
//			break;
//		case KEY_PGUP:
//			startY -= 220;
//			break;
//		case KEY_PGDN:
//			startY += 220;
//			break;
//		case KEY_END:
//			startX = 489;
//			break;
//		case KEY_HOME:
//			startX = 0;
//			break;
//		}
//		if (startY < 0) startY = 0;
//		if (startY >= 380) startY = 379;
//		if (startX < 0) startX = 0;
//		if (startX >= 490) startX = 489;
//		stretch_blit(temp, screen, startX, startY, 310, 220, 0, 20, 620, 440);
//	}
//
//	destroy_bitmap(temp);
//	destroy_bitmap(scn);
//}
//
///***************************************************************************
//** showView2
//**
//** Purpose: To display AGI VIEWs and allow scrolling through the cells and
//** loops. You have to install the allegro keyboard handler to call this
//** function.
//***************************************************************************/
//void bDShowView2(int viewNum)
//{
//	int loopNum = 0, celNum = 0;
//	BITMAP* temp = create_bitmap(640, 480);
//	char viewString[20], loopString[20], celString[20];
//	boolean stillViewing = TRUE;
//
//	sprintf(viewString, "View number: %d", viewNum);
//
//	while (stillViewing) {
//		clear(temp);
//		textout(temp, font, viewString, 10, 10, 15);
//		sprintf(loopString, "Loop number: %d", loopNum);
//		sprintf(celString, "Cel number: %d", celNum);
//		textout(temp, font, loopString, 10, 18, 15);
//		textout(temp, font, celString, 10, 26, 15);
//		stretch_blit(loadedViews[viewNum].loops[loopNum].cels[celNum].bmp, temp,
//			0, 0, loadedViews[viewNum].loops[loopNum].cels[celNum].width,
//			loadedViews[viewNum].loops[loopNum].cels[celNum].height, 10, 40,
//			loadedViews[viewNum].loops[loopNum].cels[celNum].width * 4,
//			loadedViews[viewNum].loops[loopNum].cels[celNum].height * 3);
//		blit(temp, screen, 0, 0, 0, 0, 640, 480);
//		switch (readkey() >> 8) {  /* Scan code */
//		case KEY_UP:
//			loopNum++;
//			break;
//		case KEY_DOWN:
//			loopNum--;
//			break;
//		case KEY_LEFT:
//			celNum--;
//			break;
//		case KEY_RIGHT:
//			celNum++;
//			break;
//		case KEY_ESC:
//			stillViewing = FALSE;
//			break;
//		}
//
//		if (loopNum < 0) loopNum = loadedViews[viewNum].numberOfLoops - 1;
//		if (loopNum >= loadedViews[viewNum].numberOfLoops) loopNum = 0;
//		if (celNum < 0)
//			celNum = loadedViews[viewNum].loops[loopNum].numberOfCels - 1;
//		if (celNum >= loadedViews[viewNum].loops[loopNum].numberOfCels)
//			celNum = 0;
//	}
//
//	destroy_bitmap(temp);
//}

/***************************************************************************
** showObjectState
**
** This function shows the full on screen object state. It shows all view
** table variables and displays the current cell.
**
** Params:
**
**    objNum              Object number.
**
***************************************************************************/
void bDShowObjectState(int objNum)
{
	char* tempStr = (char*)&GOLDEN_RAM[LOCAL_WORK_AREA_START];
	BITMAP* temp = create_bitmap(640, 480);
	ViewTable localViewtab;

	getViewTab(&localViewtab, objNum);

	while (keypressed()) { /* Wait */ }

	show_mouse(NULL);
	blit(screen, temp, 0, 0, 0, 0, 640, 480);
	rectfill(screen, 10, 10, 630, 470, 0);
	rect(screen, 10, 10, 630, 470, 16);

	stretch_blit(localViewtab.celData->bmp, screen, 0, 0,
		localViewtab.xsize, localViewtab.ysize, 200, 18,
		localViewtab.xsize * 4, localViewtab.ysize * 4);

	sprintf(tempStr, "objNum: %d", objNum);
	textout(screen, font, tempStr, 18, 18, 8);
	sprintf(tempStr, "xPos: %d", localViewtab.xPos);
	textout(screen, font, tempStr, 18, 28, 8);
	sprintf(tempStr, "yPos: %d", localViewtab.yPos);
	textout(screen, font, tempStr, 18, 38, 8);
	sprintf(tempStr, "xSize: %d", localViewtab.xsize);
	textout(screen, font, tempStr, 18, 48, 8);
	sprintf(tempStr, "ySize: %d", localViewtab.ysize);
	textout(screen, font, tempStr, 18, 58, 8);
	sprintf(tempStr, "currentView: %d", localViewtab.currentView);
	textout(screen, font, tempStr, 18, 68, 8);
	sprintf(tempStr, "numOfLoops: %d", localViewtab.numberOfLoops);
	textout(screen, font, tempStr, 18, 78, 8);
	sprintf(tempStr, "currentLoop: %d", localViewtab.currentLoop);
	textout(screen, font, tempStr, 18, 88, 8);
	sprintf(tempStr, "numberOfCels: %d", localViewtab.numberOfCels);
	textout(screen, font, tempStr, 18, 98, 8);
	sprintf(tempStr, "currentCel: %d", localViewtab.currentCel);
	textout(screen, font, tempStr, 18, 108, 8);
	sprintf(tempStr, "stepSize: %d", localViewtab.stepSize);
	textout(screen, font, tempStr, 18, 118, 8);
	sprintf(tempStr, "stepTime: %d", localViewtab.stepTime);
	textout(screen, font, tempStr, 18, 128, 8);
	sprintf(tempStr, "stepTimeCount: %d", localViewtab.stepTimeCount);
	textout(screen, font, tempStr, 18, 138, 8);
	sprintf(tempStr, "cycleTime: %d", localViewtab.cycleTime);
	textout(screen, font, tempStr, 18, 148, 8);
	sprintf(tempStr, "cycleTimeCount: %d", localViewtab.cycleTimeCount);
	textout(screen, font, tempStr, 18, 158, 8);
	sprintf(tempStr, "direction: %d", localViewtab.direction);
	textout(screen, font, tempStr, 18, 168, 8);
	sprintf(tempStr, "priority: %d", localViewtab.priority);
	textout(screen, font, tempStr, 18, 178, 8);
	switch (localViewtab.motion) {
	case 0: /* Normal motion */
		sprintf(tempStr, "motion: normal motion");
		break;
	case 1: /* Wander */
		sprintf(tempStr, "motion: wander");
		break;
	case 2: /* Follow ego */
		sprintf(tempStr, "motion: follow ego");
		break;
	case 3: /* Move object */
		sprintf(tempStr, "motion: move object");
		break;
	default:
		sprintf(tempStr, "motion: unknown");
		break;
	}
	textout(screen, font, tempStr, 18, 188, 8);
	switch (localViewtab.cycleStatus) {
	case 0: /* Normal cycle */
		sprintf(tempStr, "cycleStatus: normal cycle");
		break;
	case 1: /* End of loop */
		sprintf(tempStr, "cycleStatus: end of loop");
		break;
	case 2: /* Reverse loop */
		sprintf(tempStr, "cycleStatus: reverse loop");
		break;
	case 3: /* Reverse cycle */
		sprintf(tempStr, "cycleStatus: reverse cycle");
		break;
	default:
		sprintf(tempStr, "cycleStatus: unknown");
		break;
	}
	textout(screen, font, tempStr, 18, 198, 8);

	while (!keypressed()) { /* Wait */ }

	blit(temp, screen, 0, 0, 0, 0, 640, 480);
	destroy_bitmap(temp);
	show_mouse(screen);
}

#pragma code-name (pop)
