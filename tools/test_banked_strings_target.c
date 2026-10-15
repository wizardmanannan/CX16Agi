/* Shared compiled-C callers for production and integration tests. */
#include "helpers.h"
#include "irq.h"
#include "spriteAllocator.h"
#include "sound.h"
#include "textLayer.h"
#include "words.h"
#include <cx16.h>

char *testBankAliasSource;
byte testBankAliasBank;
char testWordsQuery[MAX_WORD_SIZE];
int testWordsResult;
byte testCombinedSoundEndFlag;
byte testSpriteFixtureWidth;
byte testSpriteFixtureHeight;
byte testSpriteFixtureError;
byte testSpriteFixtureGapBlocks;
unsigned int testSpriteFixtureAllocationCount;
unsigned int testSpriteFixtureReleasedCount;
VeraSpriteAddress testSpriteFixtureGapAddress;

#define TEST_SPRITE_FIXTURE_BITMAP_SIZE ((TOTAL_REAL_BLOCKS + 7) / 8)
static byte testSpriteFixtureOwned[TEST_SPRITE_FIXTURE_BITMAP_SIZE];

#pragma wrapped-call (push, trampoline, SPRITE_MEMORY_MANAGER_BANK)
extern void bDResetSpriteTablePointer(void);
#pragma wrapped-call (pop)

static void testClearSpriteFixture(void)
{
	unsigned int i;
	for (i = 0; i < TEST_SPRITE_FIXTURE_BITMAP_SIZE; ++i)
		testSpriteFixtureOwned[i] = 0;
	testSpriteFixtureAllocationCount = 0;
	testSpriteFixtureReleasedCount = 0;
	testSpriteFixtureError = 0;
	testSpriteFixtureGapBlocks = 0;
	testSpriteFixtureGapAddress = 0;
}

static void testRecordSpriteFixtureBlock(VeraSpriteAddress address)
{
	unsigned long offset;
	unsigned int block;
	byte mask;

	if (address < VRAM_START || address >= VRAM_START + VRAM_SIZE)
	{
		testSpriteFixtureError = 1;
		return;
	}
	offset = address - VRAM_START;
	if (offset % BLOCK_SIZE)
	{
		testSpriteFixtureError = 2;
		return;
	}
	block = (unsigned int)(offset / BLOCK_SIZE);
	mask = (byte)(1 << (block & 7));
	if (testSpriteFixtureOwned[block >> 3] & mask)
	{
		testSpriteFixtureError = 3;
		return;
	}
	testSpriteFixtureOwned[block >> 3] |= mask;
	++testSpriteFixtureAllocationCount;
}

/* Reserve every currently free 8x8 slot through the production allocator. */
void testReserveSpriteFixtureBlocks(void)
{
	VeraSpriteAddress address;

	testClearSpriteFixture();
	bDResetSpriteTablePointer();
	for (;;)
	{
		address = bDFindFreeVramBlock(SPR_SIZE_8, SPR_SIZE_8);
		if (!address) break;
		testRecordSpriteFixtureBlock(address);
		if (testSpriteFixtureError) break;
	}
	bDResetSpriteTablePointer();
}

/* Leave exactly one real allocator run of the requested geometry free. */
void testPrepareSpriteFixturePartialRun(void)
{
	VeraSpriteAddress gap;
	VeraSpriteAddress address;
	SpriteAllocationSize width, height;

	testClearSpriteFixture();
	bDResetSpriteTablePointer();
	width = (SpriteAllocationSize)testSpriteFixtureWidth;
	height = (SpriteAllocationSize)testSpriteFixtureHeight;
	if ((width != SPR_SIZE_8 && width != SPR_SIZE_16 && width != SPR_SIZE_32 && width != SPR_SIZE_64) ||
		(height != SPR_SIZE_8 && height != SPR_SIZE_16 && height != SPR_SIZE_32 && height != SPR_SIZE_64))
	{
		testSpriteFixtureError = 4;
		return;
	}
	gap = bDFindFreeVramBlock(width, height);
	if (!gap)
	{
		testSpriteFixtureError = 5;
		return;
	}
	testSpriteFixtureGapAddress = gap;
	testSpriteFixtureGapBlocks = (byte)((width / 8) * (height / 8));
	for (;;)
	{
		address = bDFindFreeVramBlock(SPR_SIZE_8, SPR_SIZE_8);
		if (!address) break;
		testRecordSpriteFixtureBlock(address);
		if (testSpriteFixtureError) break;
	}
	bDDeleteAllocation(gap, width, height);
	bDResetSpriteTablePointer();
}

/* Release only fixture-owned single-block reservations through the real API. */
void testReleaseSpriteFixtureBlocks(void)
{
	unsigned int block;
	unsigned int released = 0;
	byte mask;
	VeraSpriteAddress address;

	for (block = 0; block < TOTAL_REAL_BLOCKS; ++block)
	{
		mask = (byte)(1 << (block & 7));
		if (!(testSpriteFixtureOwned[block >> 3] & mask)) continue;
		address = (VeraSpriteAddress)VRAM_START + (unsigned long)block * BLOCK_SIZE;
		bDDeleteAllocation(address, SPR_SIZE_8, SPR_SIZE_8);
		testSpriteFixtureOwned[block >> 3] &= (byte)~mask;
		++released;
	}
	testSpriteFixtureReleasedCount = released;
	testSpriteFixtureAllocationCount = 0;
	bDResetSpriteTablePointer();
}

void testRunWordsSearch(void)
{
	testWordsResult = b12FindSynonymNum(testWordsQuery, 0);
}

void testRunBankAliasDisplay(void)
{
	b3DisplayMessageBox(testBankAliasSource, testBankAliasBank, 0, 0, 1, 0,
		TRUE, FIRST_TEXT_ROW);
}

void testRunBankBoundaryDisplay(void)
{
	b3DisplayMessageBox((char *)0xBFFC, 0x24, 0, 0, 1, 0, FALSE, FIRST_TEXT_ROW);
}

void testRunFullLayerClear(void)
{
	b3FillChar(0, TILE_LAYER_HEIGHT - FIRST_TEXT_ROW, 1, 0x46);
}

void testBeginSpritePublicationWork(void)
{
	b6EnterForegroundVera();
}

void testEndSpritePublicationWork(void)
{
	b6LeaveForegroundVera();
}

void testRunCombinedDisplayText(void)
{
	bBPlaySound(0, testCombinedSoundEndFlag);
	b6SetAndWaitForIrqState(DISPLAY_TEXT);
}

#pragma wrapped-call (push, trampoline, HELPERS_BANK)
extern int b5Multiply(int a, int b);
#pragma wrapped-call (pop)

int testHarnessSmokeResult;

void testRunHarnessSmoke(void)
{
	testHarnessSmokeResult = b5Multiply(257, 3);
}

void testRunTextCleanupRow1(void)
{
	b3DisplayMessageBox("HI", 0, 0, 0, 0, 0, FALSE, 1);
}

void testRunTextCleanupRow4(void)
{
	b3DisplayMessageBox("HI", 0, 0, 0, 0, 0, FALSE, FIRST_TEXT_ROW);
}

void testRunTextCleanupLastRow(void)
{
	b3DisplayMessageBox("HI", 0, 0, 0, 0, 0, FALSE, TILE_LAYER_HEIGHT);
}

static void displayMaximumHeightText(byte firstRow)
{
	char message[TILE_LAYER_HEIGHT * 2 + 1];
	byte rows = TILE_LAYER_HEIGHT - firstRow + 1;
	byte row;
	size_t length = 0;

	for (row = 0; row < rows; ++row)
	{
		message[length++] = 'A';
		if (row + 1 < rows) message[length++] = NEW_LINE;
	}
	message[length] = '\0';
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, firstRow);
}

void testRunTextCleanupMaximumOrigin1(void)
{
	displayMaximumHeightText(1);
}

void testRunTextCleanupMaximumOrigin2(void)
{
	displayMaximumHeightText(2);
}

void testRunTextCleanupMaximumOrigin4(void)
{
	displayMaximumHeightText(FIRST_TEXT_ROW);
}

void testRunTextCleanupClear(void)
{
	b3ClearLastPlacedText();
}
