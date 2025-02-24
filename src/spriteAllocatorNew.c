#include "spriteAllocatorNew.h"

#define RUN_TESTS 1

/*


64 × 8	8	1	8
64 × 16	8	2	16
64 × 32	8	4	32
64 × 64	8	8	64
*/

#ifdef RUN_TESTS

boolean trap = FALSE;


extern byte bDSpriteAllocTable[TOTAL_REAL_BLOCKS];
extern byte blocksBySize[4][4];
extern void bDResetSpriteMemoryManager();
extern unsigned long findFreeVRamLowByteLoop;
extern void bDResetSpriteTablePointer();

void checkAllocationTableFilledWithValue(byte value)
{
	int i;
	boolean result = TRUE;

	printf("checking allocation table is filled with %d\n", value);
	for (i = 0; i < TOTAL_REAL_BLOCKS && result; i++)
	{
		result = bDSpriteAllocTable[i] == value;

		if (!result)
		{
			printf("Check allocation table filled with value expected %d at %d got %d. Result %d \n", value, i, bDSpriteAllocTable[i], result);
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


void canFillWithBlocks(SpriteAllocationSize width, SpriteAllocationSize height)
{
	VeraSpriteAddress i;
	unsigned long result;
	boolean pass;

	printf("run canFillWithBlocks width height %d %d\n", width, height);

	for (i = 0; i < TOTAL_REAL_BLOCKS / blocksBySize[width / 8 - 1][height / 8 - 1]; i++)
	{
#define EXPECTED (i * 32 * blocksBySize[width / 8 - 1][height / 8 - 1]  + VRAM_START)
	
		if (height == SPR_ATTR_16 && i == 1)
		{
			trap = TRUE;
		}

		result = bDFindFreeVramBlock(width, height);
		pass = result == EXPECTED;

		printf("result: %d on %lu expected %lx got %lx\n", pass, i, EXPECTED, result);

		if (!pass)
		{
			exit(0);
		}
	}

	trap = TRUE;
	result = bDFindFreeVramBlock(SPR_SIZE_8, SPR_SIZE_8);
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
	checkAllocationTableFilledWithValue(1);
}

void runTests()
{
	RAM_BANK = SPRITE_MEMORY_MANAGER_NEW_BANK;
		
	canFillWithBlocks(SPR_SIZE_8, SPR_SIZE_8);
	bDResetSpriteMemoryManager();
	
	canFillWithBlocks(SPR_SIZE_8, SPR_SIZE_16);

	exit(0);
}
#endif // RUN_TESTS

#pragma rodata-name (push, "BANKRAM0D")

byte blocksBySize[4][4] = { {1, 2, 4, 8}, {2, 4, 8, 16}, {4, 8, 16, 32}, {8, 16, 32, 64} };

#define FAST_LOOKUP_SIZE 130
extern byte bDBlocksBySizeFastLookup[FAST_LOOKUP_SIZE];

#pragma rodata-name (pop)

void bDResetSpriteMemoryManager()
{
	memset(bDSpriteAllocTable, 0, TOTAL_REAL_BLOCKS);
	bDResetSpriteTablePointer();
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

		}

#ifdef RUN_TESTS
		runTests();
#endif // RUN_TESTS

	}
}