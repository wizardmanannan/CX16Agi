/* Real view loading and show.obj callers for banked lifecycle regressions. */
#include "view.h"
#include "showObj.h"

extern byte viewHeaderBuffer[501];
extern ViewTableMetadata viewTableMetadata[SPRITE_SLOTS];
byte testViewLoopErrors;
byte testViewLoaded;
byte testViewAllocationSlots = ALLOCATION_ARRAY_SIZE;

void testFillCloseupPalettes(void)
{
	byte i;
	PaletteGetResult result;
	for (i = 0; i < NO_MANAGED_PALETTES; ++i) bFGetPalette(100 + i, &result);
}

void testInitViewResource(void)
{
	byte previousBank = RAM_BANK;
	AGIFilePosType location;
	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	bAInitViews();
	bFInitPaletteManager();
	memsetBanked(viewHeaderBuffer, 0, 501, 9);
	location.fileNum = 0; location.filePos = 0;
	b10SetResourceDirectory(&location, &viewdir[0]);
	numViews = 1; numLogics = numSounds = numPictures = 0;
	RAM_BANK = previousBank;
}

void testLoadViewLoops(void)
{
	View view;
	Loop loop;
	Cel cel;
	byte i;
	b9LoadViewFile(0);
	getLoadedView(&view, 0);
	testViewLoaded = view.loaded;
	testViewLoopErrors = 0;
	if (!view.loaded) return;
	for (i = 0; i < view.numberOfLoops; ++i)
	{
		getLoadedLoop(&view, &loop, i);
		if (loop.numberOfCels != 1) { ++testViewLoopErrors; continue; }
		getLoadedCel(&loop, &cel, 0);
		if (cel.width != 1 || cel.height != 1) ++testViewLoopErrors;
	}
}

void testResetCachedCloseup(void)
{
	memsetBanked(viewTableMetadata, 0, sizeof(ViewTableMetadata) * SPRITE_SLOTS, SPRITE_METADATA_BANK);
	bAResetViews();
}

void testShowCachedCloseup(void)
{
	b11ShowObj(0);
}
