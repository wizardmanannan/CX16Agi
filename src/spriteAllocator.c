#include "spriteAllocator.h"
//#define RUN_TESTS 1

/*


64 x — 8	8	1	8
64 x — 16	8	2	16
64 x — 32	8	4	32
64 x — 64	8	8	64
*/

extern byte bDSpriteAllocTable[TOTAL_REAL_BLOCKS];
extern void bDResetSpriteMemoryManager();
extern void bDResetSpriteTablePointer();

#ifdef RUN_TESTS

boolean trap = FALSE;

extern byte blocksBySize[4][4];
extern unsigned long findFreeVRamLowByteLoop;
extern boolean OPTIMISTIC_MODE;

// cc65?style typedefs
// VRAM parameters (avoid your VRAM_SIZE name)
#define TOTAL_SPRITE_VRAM_BYTES  69120u  // 0x10E00
#define BLOCK_BYTES                 32u  // one 8×8 block
#define MIN_ALLOC_BYTES             BLOCK_BYTES

// all 16 width×height combos
static const struct { byte w, h; } sizeTable[16] = {
	{  8,  8 }, {  8, 16 }, {  8, 32 }, {  8, 64 },
	{ 16,  8 }, { 16, 16 }, { 16, 32 }, { 16, 64 },
	{ 32,  8 }, { 32, 16 }, { 32, 32 }, { 32, 64 },
	{ 64,  8 }, { 64, 16 }, { 64, 32 }, { 64, 64 }
};

// tiny 16-bit LFSR for pseudo-random 0..15
static unsigned lfsr = 0xACE1;
static byte prng4(void) {
	unsigned bit = ((lfsr >> 0) ^ (lfsr >> 2)
		^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
	lfsr = (lfsr >> 1) | (bit << 15);
	return (byte)(lfsr & 0x0F);
}

// your cc65-compiled allocation function; returns 0 on failure
extern VeraSpriteAddress bDFindFreeVramBlock(SpriteAllocationSize w,
	SpriteAllocationSize h);

void canFillVRamWithRandomSprites(boolean printResults)
{
	/* all declarations must come first under C89/cc65: */
	const unsigned      ALL_FAILED = 0xFFFFu;
	unsigned            failedMask = 0;
	unsigned long       totalBytes = 0;
	unsigned            idx;
	byte                w, h;
	unsigned            blocks;
	unsigned long       thisBytes;
	VeraSpriteAddress   addr;
	unsigned long       slack;

	/* now the code: */
	while (failedMask != ALL_FAILED)
	{
		idx = prng4();
		w = sizeTable[idx].w;
		h = sizeTable[idx].h;
		blocks = (w >> 3) * (h >> 3);
		thisBytes = blocks * BLOCK_BYTES;

		printf("Filling memory with random sprites all the way up\n");

		addr = bDFindFreeVramBlock((SpriteAllocationSize)w,
			(SpriteAllocationSize)h);

		if (addr) {
			failedMask = 0;
			totalBytes += thisBytes;
			if (printResults) {
				printf("OK: %2u×%2u @ 0x%05lX  (+%4lu ? %5lu/%u)\n",
					(unsigned)w, (unsigned)h,
					(unsigned long)addr,
					thisBytes,
					totalBytes,
					TOTAL_SPRITE_VRAM_BYTES);
			}
		}
		else {
			failedMask |= (1u << idx);
		}
	}

	/* final slack check */
	slack = TOTAL_SPRITE_VRAM_BYTES - totalBytes;
	if (slack >= MIN_ALLOC_BYTES) {
		printf("ERROR: fragmented with %lu bytes free (? %u)\n",
			slack, MIN_ALLOC_BYTES);
	}
	else {
		if (printResults) {
			printf("SUCCESS: VRAM “full” with %lu bytes slack (< %u)\n",
				slack, MIN_ALLOC_BYTES);
		}
	}
}


void checkAllocationTableFilledWithValue(byte value, byte blockSize)
{
	int i;
	boolean result = TRUE;

	printf("checking allocation table is filled with %d\n", value);
	for (i = 0; i < TOTAL_REAL_BLOCKS && result && i + blockSize < TOTAL_REAL_BLOCKS; i++)
	{
		result = bDSpriteAllocTable[i] == value;

		if (!result)
		{
			printf("check allocation table filled with value expected %d at %d got %d. Result %d \n", value, i, bDSpriteAllocTable[i], result);
		}
	}

	if (!result)
	{
		exit(0);
	}
	else
	{
		printf("pass\n");
	}
}

byte fastLookupLookUp[9] = { 0xFF,0,1,0xF,2,0xF,0xF,0xF,3 };

void fillWithBlocks(SpriteAllocationSize width, SpriteAllocationSize height, boolean printSuccessResult)
{
	VeraSpriteAddress i;
	unsigned long result;
	boolean pass;
	byte log2Width = fastLookupLookUp[width / 8], log2Height = fastLookupLookUp[height / 8];

	for (i = 0; i < TOTAL_REAL_BLOCKS / blocksBySize[log2Width][log2Height]; i++)
	{
#define EXPECTED (i * 32 * blocksBySize[log2Width][log2Height] + VRAM_START)

		result = bDFindFreeVramBlock(width, height);
		pass = result == EXPECTED;

		if (!result || printSuccessResult)
		{
			printf("result: %d on %lu for %d by %d expected %lx got %lx\n", pass, i, width, height, EXPECTED, result);
		}

		if (!pass)
		{
			exit(0);
		}
	}
}

void canFillWithBlocks(SpriteAllocationSize width, SpriteAllocationSize height)
{
	boolean result;
	byte log2Width = fastLookupLookUp[width / 8], log2Height = fastLookupLookUp[height / 8];

	fillWithBlocks(width, height, TRUE);

	result = bDFindFreeVramBlock(width, height);
	if (result != 0)
	{
		printf("failed to get get zero result after fill up, we got %lu\n", result);
		exit(0);
	}
	else
	{
		printf("got zero result after trying to allocate when full\n");
	}

	printf("checking the table is fulling allocated\n");
	checkAllocationTableFilledWithValue(1, blocksBySize[log2Width][log2Height]);
}

void canRepopulateAfterDelete()
{
	byte i, j, k, powJ, powK;
	VeraSpriteAddress result;
	VeraSpriteAddress testValues[3][4][4] = {
		// Group 1: Beginning of memory (unchanged)
		{{SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START},
		 {SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START},
		 {SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START},
		 {SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START, SPRITES_DATA_START}},

		 // Group 2: Middle of memory (aligned properly for different sprite sizes)
		 {{0x15000, 0x15040, 0x15080, 0x150C0},  /* 8Ã—8,  8Ã—16,  8Ã—32,  8Ã—64  */
		  {0x15100, 0x15180, 0x15200, 0x15280},  /* 16Ã—8, 16Ã—16, 16Ã—32, 16Ã—64 */
		  {0x15400, 0x15480, 0x15500, 0x15580},  /* 32Ã—8, 32Ã—16, 32Ã—32, 32Ã—64 */
		  {0x15800, 0x15880, 0x15900, 0x15980}}, /* 64Ã—8, 64Ã—16, 64Ã—32, 64Ã—64 */

		  // Group 3: End of memory (smallest â†’ largest, respecting width/height order)
		{{0x1F7E0, 0x1F7C0, 0x1F780, 0x1F700},  /*  8x8,   8x16,   8x32,   8x64  */
			{0x1F760, 0x1F740, 0x1F700, 0x1F600},  /* 16x8,  16x16,  16x32,  16x64 */
			{0x1F6A0, 0x1F680, 0x1F600, 0x1F400},  /* 32x8,  32x16,  32x32,  32x64 */
			{0x1F200, 0x1F300, 0x1F400, 0x1F000}   /* 64x8,  64x16,  64x32,  64x64 */}
	};

	VeraSpriteAddress expectedValue;

	for (i = 0; i < 3; i++)
	{
		for (j = 0, powJ = 1; j < 4; j++, powJ *= 2)
		{
			for (k = 0, powK = 1; k < 4; k++, powK *= 2)
			{
				expectedValue = testValues[i][j][k];

				bDResetSpriteMemoryManager();

				fillWithBlocks((SpriteAllocationSize)(powJ * 8), (SpriteAllocationSize)(powK * 8), FALSE);

				bDDeleteAllocation(expectedValue, (SpriteAllocationSize)(powJ * 8), (SpriteAllocationSize)(powK * 8));

				if (i == 2 && j == 0 && k == 0)
				{
					trap = TRUE;
				}

				if (i == 2)
				{
					trap = TRUE;
				}
				result = bDFindFreeVramBlock((SpriteAllocationSize)(powJ * 8), (SpriteAllocationSize)(powK * 8));

				printf("can repopulate after delete for %d,%d,%d expected %lx got %lx.", i, (SpriteAllocationSize)(powJ * 8), (SpriteAllocationSize)(powK * 8), expectedValue, result);
				if (result == expectedValue)
				{
					printf("pass \n");
				}
				else
				{
					printf("fail \n");
					exit(0);
				}

				bDResetSpriteMemoryManager();
			}
		}
	}
}

void runTests()
{
	byte i, j, powI, powJ;
	RAM_BANK = SPRITE_MEMORY_MANAGER_NEW_BANK;

	canFillVRamWithRandomSprites(TRUE);

	bDResetSpriteMemoryManager();

	for (i = 0, powI = 1; i < 4; i++, powI*=2)
	{
		for (j = 0, powJ = 1; j < 4; j++, powJ*=2)
		{
			canFillWithBlocks((SpriteAllocationSize)(powI * 8), (SpriteAllocationSize)(powJ * 8));
			bDResetSpriteMemoryManager();
		}
	}

	canRepopulateAfterDelete();

	exit(0);
}
#endif // RUN_TESTS

#pragma rodata-name (push, "BANKRAM0D")

byte blocksBySize[4][4] = { {1, 2, 4, 8}, {2, 4, 8, 16}, {4, 8, 16, 32}, {8, 16, 32, 64} };

#define FAST_LOOKUP_SIZE 130
extern byte bDBlocksBySizeFastLookup[FAST_LOOKUP_SIZE];

#pragma rodata-name (pop)

extern void bDReenableOptimisticMode();

void bDResetSpriteMemoryManager()
{
	memset(bDSpriteAllocTable, 0, TOTAL_REAL_BLOCKS);
	bDResetSpriteTablePointer();
	bDReenableOptimisticMode();
}

void bDInitSpriteMemoryManager()
{
#define NO_SPRITE_SIZES 4
	byte spritesizes[NO_SPRITE_SIZES] = { 8, 16, 32, 64 }, i, j, blockSize;

	for (i = 0; i < NO_SPRITE_SIZES; i++)
	{
		for (j = 0; j < NO_SPRITE_SIZES; j++)
		{
			blockSize = spritesizes[i] + spritesizes[j];
			bDBlocksBySizeFastLookup[blockSize] = blocksBySize[i][j];

			bDBlocksBySizeFastLookup[blockSize + 1] = blocksBySize[i][j];

			//printf("ssi %d ssj %d blockSize %d bbs %d i, j %d %d, bbsaddr %p bbsp1Addr %p\n", spritesizes[i], spritesizes[j], blockSize, blocksBySize[i][j], i, j,&bDBlocksBySizeFastLookup[blockSize], &bDBlocksBySizeFastLookup[blockSize + 1]);
		}
	}
#ifdef RUN_TESTS
	runTests();
#endif // RUN_TESTS
}