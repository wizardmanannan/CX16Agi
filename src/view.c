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

//#define VERBOSE_LOAD_VIEWS;
//#define VERBOSE_UPDATE_OBJECTS

//#define VERBOSE_ALLOC_WATCH

View* loadedViews = (View*)&BANK_RAM[LOADED_VIEW_START];
BITMAP* spriteScreen;

extern byte* var;
extern boolean* flag;
extern char string[12][40];
extern byte horizon;
extern int dirnOfEgo;

ViewTable* viewtab = (ViewTable*)&BANK_RAM[VIEWTAB_START];

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

void agi_blitTrampoline(BITMAP* bmp, int x, int y, int w, int h, byte trans, byte pNum);

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

void getLocalLoop(View* loadedView, Loop* returnedLocalLoop, byte localLoopNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedView->loopsBank;

	*returnedLocalLoop = *(loadedView->loops + (localLoopNumber * sizeof(Loop)));

	RAM_BANK = previousRamBank;
}

void setLocalLoop(View* loadedView, Loop* localLoop, byte localLoopNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedView->loopsBank;

	*(loadedView->loops + (localLoopNumber * sizeof(Loop))) = *localLoop;

	RAM_BANK = previousRamBank;
}

void getLocalCel(Loop* loadedLoop, Cel* returnedLocalCell, byte localCellNumber)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = loadedLoop->celBank;

	//TODO: This deferenced address ends up in the wrong place. Investigate when we do sprites
	//*returnedLocalCell = *(loadedLoop->cels + (localCellNumber * sizeof(Loop)));

	RAM_BANK = previousRamBank;
}

void setLocalCel(Loop* loadedLoop, Cel* localCell, byte localCellNumber)
{
	byte previousRamBank = RAM_BANK;

	//printf("The cel bank is %d \n", loadedLoop->celBank);
	//RAM_BANK = loadedLoop->celBank;

	//printf("Attempting to dereference %p. (%p + %d)\n", loadedLoop->cels + (localCellNumber * sizeof(Loop)), loadedLoop->cels, (localCellNumber * sizeof(Loop)));
	//*(loadedLoop->cels + (localCellNumber * sizeof(Loop))) = *localCell;

	RAM_BANK = previousRamBank;
}
#pragma code-name (push, "BANKRAM09")
void b9InitViews()
{
	int i;
	View localView;
	for (i = 0; i < 256; i++) {
		localView.description = 0;
		localView.descriptionBank = 0;
		localView.loaded = FALSE;
		localView.loops = 0;
		localView.loopsBank = 0;
		localView.numberOfLoops = 0;

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
		localViewtab.bgPic = NULL;
		localViewtab.bgPri = NULL;
		localViewtab.bgX = 0;
		localViewtab.bgY = 0;
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

		destroy_bitmap(localViewtab.bgPic);
		localViewtab.bgPic = NULL;
		destroy_bitmap(localViewtab.bgPri);
		localViewtab.bgPri = NULL;
		localViewtab.flags &= ~(UPDATE | ANIMATED);

		setViewTab(&localViewtab, entryNum);
	}
}

const char* _emptyDecription = "\0";

/**************************************************************************
** loadViewFile
**
** Purpose: Loads a VIEW file into memory storing it in the loadedViews
** array.
**************************************************************************/
void b9LoadViewFile(byte viewNum)
{
#define NO_VIEW_START_BYTES 5
#define POSITION_OF_LOOPS_OFFSET 5
#define LOOP_OFFSET_BYTES 2
#define CEL_OFFSET_BYTES 2
#define CEL_WIDTH_OFFSET 0
#define CEL_HEIGHT_OFFSET 1
#define CEL_TRANSPARENCY_OFFSET 2
	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	byte* loopStart, * celStart, cWidth;
	byte l, c, x, y, chunk, xTotal, colour, len, loopIndex, viewIndex, trans;
	byte viewStart[NO_VIEW_START_BYTES];
	View localView;
	Loop localLoop;
	Cel localCel;
	int i;
	byte cellPositionBytes[NO_CODE_BANKS]; //TODO: Investigate seems like an odd choice for size
	int loopHeaderOffset;
	int cellHeaderOffset;
	const char* description;
	int descriptionOffset;
	int descriptionLength;

#ifdef VERBOSE_LOAD_VIEWS
	printf("Attempt to load viewNum %d", viewNum);
#endif // VERBOSE_LOAD_VIEWS

	getLogicDirectory(&agiFilePosType, &viewdir[viewNum]);

#ifdef VERBOSE_LOAD_VIEWS
	printf("&localView is %p ", &localView);
#endif
	getLoadedView(&localView, viewNum);

    b6LoadAGIFile(VIEW, &agiFilePosType, &tempAGI);

	memCpyBanked(&viewStart[0], tempAGI.code, tempAGI.codeBank, NO_VIEW_START_BYTES);

	//printf("View header bytes %d, %d, %d, %d, %d \n", viewStart[0], viewStart[1], viewStart[2], viewStart[3], viewStart[4]);

	descriptionOffset = viewStart[3] || viewStart[4];

	if (descriptionOffset)
	{
		description = (const char*)(tempAGI.code + viewStart[3] + viewStart[4] * 256);
		descriptionLength = strlen(description);

		localView.description = (char*)b10BankedAlloc(descriptionLength, &localView.descriptionBank);
#ifdef VERBOSE_ALLOC_WATCH
		printf("Description length %d bank %p address %p\n", descriptionLength, localView.description, localView.descriptionBank);
#endif // VERBOSE_ALLOC_WATCH


		memCpyBanked(&GOLDEN_RAM[LOCAL_WORK_AREA_SIZE], (const char*)&description, tempAGI.codeBank, descriptionLength <= LOCAL_WORK_AREA_SIZE ? descriptionLength : LOCAL_WORK_AREA_SIZE);
		memCpyBanked((byte*)localView.description, &GOLDEN_RAM[LOCAL_WORK_AREA_SIZE], localView.descriptionBank, descriptionLength <= LOCAL_WORK_AREA_SIZE ? descriptionLength : LOCAL_WORK_AREA_SIZE);

		if (descriptionLength + 1 > LOCAL_WORK_AREA_SIZE)
		{
			memCpyBanked((char)localView.description + LOCAL_WORK_AREA_SIZE - 1, (byte*)_emptyDecription, localView.descriptionBank, 1);
		}
	}
	else {
		localView.description = _emptyDecription;
		localView.descriptionBank = 0;
	}

	localView.description = ((viewStart[3] || viewStart[4]) ?
		// strdup((byte *)(viewStart+viewStart[3]+viewStart[4]*256)) : strdup(""));
		(const char*)(tempAGI.code + viewStart[3] + viewStart[4] * 256) : _emptyDecription);

	copyStringFromBanked(localView.description, localView.description, 0, COPY_EVERYTHING, tempAGI.codeBank, TRUE);

#ifdef VERBOSE_LOAD_VIEWS
	if (localView.description == _emptyDecription)
	{
		printf("\nThe description is empty\n");
	}
	else
	{
		printf("The description is not empty\n");

		copyStringFromBanked(localView.description, &GOLDEN_RAM[LOCAL_WORK_AREA_START], 0, 100, tempAGI.codeBank, FALSE);
		printf("It has a value of %s ", &GOLDEN_RAM[LOCAL_WORK_AREA_START]);
	}
#endif

	localView.numberOfLoops = viewStart[2];

	localView.loops = (Loop*)b10BankedAlloc(viewStart[2] * sizeof(Loop), &localView.loopsBank);
#ifdef VERBOSE_ALLOC_WATCH
	printf("loop length %d bank %p address %p\n", viewStart[2] * sizeof(Loop), &localView.loopsBank, localView.loops);
#endif // VERBOSE_ALLOC_WATCH

	for (l = 0, viewIndex = POSITION_OF_LOOPS_OFFSET; l < localView.numberOfLoops; l++, viewIndex += 2) {
		getLocalLoop(&localView, &localLoop, l);
		memCpyBanked(&loopHeaderOffset, tempAGI.code + POSITION_OF_LOOPS_OFFSET + (l * LOOP_OFFSET_BYTES), tempAGI.codeBank, LOOP_OFFSET_BYTES);

		memCpyBanked(&localLoop.numberOfCels, tempAGI.code + loopHeaderOffset, tempAGI.codeBank, 1);

#ifdef VERBOSE_LOAD_VIEWS
		printf("You have %d loops and the num of cells is %d and a loop pos of %d", localView.numberOfLoops, localLoop.numberOfCels, loopHeaderOffset);
#endif // VERBOSE_LOAD_VIEWS
		
		printf("Stop Here 4");
		localLoop.cels = (Cel*)b10BankedAlloc(localLoop.numberOfCels * sizeof(Cel), &localLoop.celBank);
		printf("Exit Here 4");
#ifdef VERBOSE_ALLOC_WATCH
		printf("cels length %d bank %p address %p\n", localLoop.numberOfCels * sizeof(Cel), &localLoop.celBank, localLoop.cels);
#endif // VERBOSE_ALLOC_WATCH

		for (c = 0, loopIndex = 1; c < localLoop.numberOfCels; c++, loopIndex += 2) {

			getLocalCel(&localLoop, &localCel, c);

			memCpyBanked(&cellHeaderOffset, tempAGI.code + loopHeaderOffset + (c * CEL_OFFSET_BYTES) + 1, tempAGI.codeBank, LOOP_OFFSET_BYTES);

#ifdef VERBOSE_LOAD_VIEWS
			printf("The address of tempAGI.code plus offset is %p it is on bank %d and the cell header offset is %d\n", tempAGI.code + cellHeaderOffset, tempAGI.codeBank, cellHeaderOffset);
#endif

			memCpyBanked(&localCel.width, tempAGI.code + loopHeaderOffset + cellHeaderOffset + CEL_WIDTH_OFFSET, tempAGI.codeBank, 1);
			memCpyBanked(&localCel.height, tempAGI.code + loopHeaderOffset + cellHeaderOffset + CEL_HEIGHT_OFFSET, tempAGI.codeBank, 1);
			memCpyBanked(&localCel.transparency, tempAGI.code + loopHeaderOffset + cellHeaderOffset + CEL_TRANSPARENCY_OFFSET, tempAGI.codeBank, 1);

#ifdef VERBOSE_LOAD_VIEWS
			printf("The cell has w: %d, h: %d t: %d \n", localCel.width, localCel.height, localCel.transparency);
#endif
			setLocalCel(&localLoop, &localCel, c);

			//      for (y=0; y<loadedViews[viewNum].loops[l].cels[c].height; y++) {
		  //         xTotal=0;
		  //         if ((trans & 0x80) && (((trans & 0x70) >> 4) != l)) { /* Flip */
		  //            while (chunk = *celStart++) { /* Until the end of the line */
		  //               colour = ((chunk & 0xF0) >> 4);
		  //               len = (chunk & 0x0F);
		  //               for (x=xTotal; x<(xTotal+len); x++) {
		  //                  if (colour == (trans & 0x0f))
		  //                     loadedViews[viewNum].loops[l].cels[c].bmp->
		  //                        line[y][(cWidth-x)-1] = colour + 1;
		  //                  else
		  //                     loadedViews[viewNum].loops[l].cels[c].bmp->
		  //                        line[y][(cWidth-x)-1] = colour + 1;
		  //               }
		  //               xTotal += len;
		  //            }
		  //            for (x=xTotal; x<loadedViews[viewNum].loops[l].cels[c].width; x++) {
		  //               loadedViews[viewNum].loops[l].cels[c].bmp->
		  //                  line[y][(cWidth-x)-1] = (trans & 0x0F) + 1;
		  //            }
		  //         }
		  //         else {
		  //            while (chunk = *celStart++) { /* Until the end of the line */
		  //               colour = ((chunk & 0xF0) >> 4);
		  //               len = (chunk & 0x0F);
		  //               for (x=xTotal; x<(xTotal+len); x++) {
		  //                  if (colour == (trans & 0x0f))
		  //                     loadedViews[viewNum].loops[l].cels[c].bmp->
		  //                        line[y][x] = colour + 1;
		  //                  else
		  //                     loadedViews[viewNum].loops[l].cels[c].bmp->
		  //                        line[y][x] = colour + 1;
		  //               }
		  //               xTotal += len;
		  //            }
		  //            for (x=xTotal; x<loadedViews[viewNum].loops[l].cels[c].width; x++) {
		  //               loadedViews[viewNum].loops[l].cels[c].bmp->line[y][x] =
		  //                  (trans & 0x0F) + 1;
		  //            }
		  //         }
		  //      }
		}

		setLocalLoop(&localView, &localLoop, l);
	}


	localView.loaded = TRUE;

	b10BankedDealloc(tempAGI.code, tempAGI.codeBank);

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

			getLocalLoop(&localView, &localLoop, l);

			for (c = 0; c < localLoop.numberOfCels; c++) {
				getLocalCel(&localLoop, &localCel, c);
				//destroy_bitmap(localView.loops[l].cels[c].bmp);
				localCel.bmp = NULL;
				localCel.height = 0;
				localCel.transparency = 0;
				localCel.width = 0;

				setLocalCel(&localLoop, &localCel, c);
			}
#ifdef VERBOSE_ALLOC_WATCH
			printf("dealloc cels bank %p address %p\n", localLoop.celBank, (byte*)localLoop.cels);
#endif
			b10BankedDealloc((byte*)localLoop.cels, localLoop.celBank); //TODO:FIX
			localLoop.celBank = 0;
			localLoop.cels = NULL;
			localLoop.numberOfCels = 0;

			setLocalLoop(&localView, &localLoop, l);
		}

		if (localView.description != _emptyDecription)
		{
#ifdef VERBOSE_ALLOC_WATCH
			printf("dealloc desc bank %p address %p\n", localView.descriptionBank, localView.description);
#endif // VERBOSE_ALLOC_WATCH
			b10BankedDealloc((byte*)localView.description, localView.descriptionBank);
		}


#ifdef VERBOSE_ALLOC_WATCH
		printf("dealloc loops bank %p address %p\n", localView.loopsBank, localView.loops);
#endif // VERBOSE_ALLOC_WATCH
		b10BankedDealloc((byte*)localView.loops, localView.loopsBank);
		localView.loaded = FALSE;
	}
}

void b9SetCel(ViewTable* localViewtab, byte celNum)
{
	Loop temp;
	View localLoadedView;
	Cel localCel;

	getLoadedView(&localLoadedView, localViewtab->currentView);
	getLocalLoop(&localLoadedView, &temp, localViewtab->currentLoop);
	getLocalCel(&temp, &localCel, celNum);

	localViewtab->currentCel = celNum;
	localViewtab->xsize = localCel.width;
	localViewtab->ysize = localCel.height;
}

void b9SetLoop(ViewTable* localViewtab, byte loopNum)
{
	View temp;
	Loop loop;
	getLoadedView(&temp, localViewtab->currentView);
	getLocalLoop(&temp, &loop, loopNum);

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
	getLocalLoop(&localView, &localLoop, 0);

	localViewtab->currentView = viewNum;
	localViewtab->numberOfLoops = localView.numberOfLoops;
	b9SetLoop(localViewtab, 0);

	localViewtab->numberOfCels = localLoop.numberOfCels;
	b9SetCel(localViewtab, 0);
	/* Might need to set some more defaults here */
}

void b9AddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol)
{
	int i, j, w, h, trans, c, boxWidth;
	View localLoadedView;
	Loop localLoop;
	Cel localCell;

	getLoadedView(&localLoadedView, vNum);
	getLocalLoop(&localLoadedView, &localLoop, lNum);
	getLocalCel(&localLoop, &localCell, cNum);

	trans = localCell.transparency & 0x0F;
	w = localCell.width;
	h = localCell.height;
	y = (y - h) + 1;

	//TODO: Fix
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

/***************************************************************************
** agi_blit
***************************************************************************/
void b9Agi_blit(BITMAP* bmp, int x, int y, int w, int h, byte trans, byte pNum)
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

	/* Store background */
	localViewtab.bgPic = create_bitmap(localViewtab.xsize, localViewtab.ysize);
	localViewtab.bgPri = create_bitmap(localViewtab.xsize, localViewtab.ysize);
	blit(picture, localViewtab.bgPic, localViewtab.xPos,
		localViewtab.yPos - localViewtab.ysize,
		0, 0, localViewtab.xsize, localViewtab.ysize);
	blit(priority, localViewtab.bgPri, localViewtab.xPos,
		localViewtab.yPos - localViewtab.ysize,
		0, 0, localViewtab.xsize, localViewtab.ysize);
	localViewtab.bgX = localViewtab.xPos;
	localViewtab.bgY = localViewtab.yPos - localViewtab.ysize;

	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}

	bACalcDirection(&localViewtab);

	agi_blitTrampoline(localViewtab.celData->bmp,
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

	/* Add saved background to picture\priority bitmaps */
	if (localViewtab.bgPic != NULL) {
		blit(localViewtab.bgPic, spriteScreen, 0, 0,
			localViewtab.bgX, localViewtab.bgY,
			localViewtab.bgPic->w, localViewtab.bgPic->h);
		destroy_bitmap(localViewtab.bgPic);
		localViewtab.bgPic = NULL;
	}
	if (localViewtab.bgPri != NULL) {
		blit(localViewtab.bgPri, priority, 0, 0,
			localViewtab.bgX, localViewtab.bgY,
			localViewtab.bgPri->w, localViewtab.bgPri->h);
		destroy_bitmap(localViewtab.bgPri);
		localViewtab.bgPri = NULL;
	}


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

 /* Store background */
	localViewtab.bgPic = create_bitmap(localViewtab.xsize, localViewtab.ysize);
	localViewtab.bgPri = create_bitmap(localViewtab.xsize, localViewtab.ysize);
	blit(picture, localViewtab.bgPic, localViewtab.xPos,
		localViewtab.yPos - localViewtab.ysize,
		0, 0, localViewtab.xsize, localViewtab.ysize);
	blit(priority, localViewtab.bgPri, localViewtab.xPos,
		localViewtab.yPos - localViewtab.ysize,
		0, 0, localViewtab.xsize, localViewtab.ysize);
	localViewtab.bgX = localViewtab.xPos;
	localViewtab.bgY = localViewtab.yPos - localViewtab.ysize;

	/* Determine priority for unfixed priorities */
	if (!(objFlags & FIXEDPRIORITY)) {
		if (localViewtab.yPos < 60)
			localViewtab.priority = 4;
		else
			localViewtab.priority = (localViewtab.yPos / 12 + 1);
	}


	if ((objFlags & ANIMATED) && (objFlags & DRAWN)) {
		/* Draw new cel onto picture\priority bitmaps */
		agi_blitTrampoline(localViewtab.celData->bmp,
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
	b11ShowPicture();
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

	/* Add saved background to picture\priority bitmaps */
	if (localViewtab.bgPic != NULL) {
		blit(localViewtab.bgPic, picture, 0, 0,
			localViewtab.bgX, localViewtab.bgY,
			localViewtab.bgPic->w, localViewtab.bgPic->h);
		destroy_bitmap(localViewtab.bgPic);
		localViewtab.bgPic = NULL;
	}
	if (localViewtab.bgPri != NULL) {
		blit(localViewtab.bgPri, priority, 0, 0,
			localViewtab.bgX, localViewtab.bgY,
			localViewtab.bgPri->w, localViewtab.bgPri->h);
		destroy_bitmap(localViewtab.bgPri);
		localViewtab.bgPri = NULL;
	}


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

 /* Store background */
	localViewtab.bgPic = create_bitmap(localViewtab.xsize, localViewtab.ysize);
	localViewtab.bgPri = create_bitmap(localViewtab.xsize, localViewtab.ysize);
	blit(picture, localViewtab.bgPic, localViewtab.xPos,
		localViewtab.yPos - localViewtab.ysize,
		0, 0, localViewtab.xsize, localViewtab.ysize);
	blit(priority, localViewtab.bgPri, localViewtab.xPos,
		localViewtab.yPos - localViewtab.ysize,
		0, 0, localViewtab.xsize, localViewtab.ysize);
	localViewtab.bgX = localViewtab.xPos;
	localViewtab.bgY = localViewtab.yPos - localViewtab.ysize;

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
		agi_blitTrampoline(localViewtab.celData->bmp,
			localViewtab.xPos,
			(localViewtab.yPos - localViewtab.ysize) + 1,
			localViewtab.xsize,
			localViewtab.ysize,
			localViewtab.celData->transparency & 0x0f,
			localViewtab.priority);
	}

	setViewTab(&localViewtab, entryNum);
	b11ShowPicture();
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
		if (localViewtab.bgPic != NULL) {
			blit(localViewtab.bgPic, spriteScreen, 0, 0,
				localViewtab.bgX, localViewtab.bgY,
				localViewtab.bgPic->w, localViewtab.bgPic->h);
			destroy_bitmap(localViewtab.bgPic);
			localViewtab.bgPic = NULL;
		}
		if (localViewtab.bgPri != NULL) {
			blit(localViewtab.bgPri, priority, 0, 0,
				localViewtab.bgX, localViewtab.bgY,
				localViewtab.bgPri->w, localViewtab.bgPri->h);
			destroy_bitmap(localViewtab.bgPri);
			localViewtab.bgPri = NULL;
		}
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

			/* Store background */
			localViewtab.bgPic = create_bitmap(localViewtab.xsize, localViewtab.ysize);
			localViewtab.bgPri = create_bitmap(localViewtab.xsize, localViewtab.ysize);
			blit(picture, localViewtab.bgPic, localViewtab.xPos,
				(localViewtab.yPos + 1) - localViewtab.ysize,
				0, 0, localViewtab.xsize, localViewtab.ysize);
			blit(priority, localViewtab.bgPri, localViewtab.xPos,
				(localViewtab.yPos + 1) - localViewtab.ysize,
				0, 0, localViewtab.xsize, localViewtab.ysize);
			localViewtab.bgX = localViewtab.xPos;
			localViewtab.bgY = (localViewtab.yPos + 1) - localViewtab.ysize;

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
			agi_blitTrampoline(localViewtab.celData->bmp,
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
		if (localViewtab.bgPic != NULL) {
			blit(localViewtab.bgPic, picture, 0, 0,
				localViewtab.bgX, localViewtab.bgY,
				localViewtab.bgPic->w, localViewtab.bgPic->h);
			destroy_bitmap(localViewtab.bgPic);
			localViewtab.bgPic = NULL;
		}
		if (localViewtab.bgPri != NULL) {
			blit(localViewtab.bgPri, priority, 0, 0,
				localViewtab.bgX, localViewtab.bgY,
				localViewtab.bgPri->w, localViewtab.bgPri->h);
			destroy_bitmap(localViewtab.bgPri);
			localViewtab.bgPri = NULL;
		}
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

			/* Store background */
			localViewtab.bgPic = create_bitmap(localViewtab.xsize, localViewtab.ysize);
			localViewtab.bgPri = create_bitmap(localViewtab.xsize, localViewtab.ysize);
			blit(picture, localViewtab.bgPic, localViewtab.xPos,
				localViewtab.yPos - localViewtab.ysize,
				0, 0, localViewtab.xsize, localViewtab.ysize);
			blit(priority, localViewtab.bgPri, localViewtab.xPos,
				localViewtab.yPos - localViewtab.ysize,
				0, 0, localViewtab.xsize, localViewtab.ysize);
			localViewtab.bgX = localViewtab.xPos;
			localViewtab.bgY = localViewtab.yPos - localViewtab.ysize;

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
			agi_blitTrampoline(localViewtab.celData->bmp,
				localViewtab.xPos,
				(localViewtab.yPos - localViewtab.ysize) + 1,
				localViewtab.xsize,
				localViewtab.ysize,
				localViewtab.celData->transparency & 0x0f,
				localViewtab.priority);
		}

		setViewTab(&localViewtab, entryNum);
	}

	b11ShowPicture();
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

void trampolineAddToPic(int vNum, int lNum, int cNum, int x, int y, int pNum, int bCol)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = VIEW_CODE_BANK_1;
	b9AddToPic(vNum, lNum, cNum, x, y, pNum, bCol);

	RAM_BANK = previousRamBank;
}

void agi_blitTrampoline(BITMAP* bmp, int x, int y, int w, int h, byte trans, byte pNum)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = VIEW_CODE_BANK_1;
	b9Agi_blit(bmp, x, y, w, h, trans, pNum);

	RAM_BANK = previousRamBank;
}
