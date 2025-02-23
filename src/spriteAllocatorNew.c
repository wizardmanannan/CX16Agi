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
		printf("pass");
	}
}


void canFillWith8SizeBlocks()
{
	VeraSpriteAddress i;
	unsigned long result;
	boolean pass;

	printf("run canFillWith8SizeBlocks\n");

	for (i = 0; i < TOTAL_REAL_BLOCKS; i++)
	{
#define EXPECTED (i * 32 + VRAM_START)

		result = bDFindFreeVramBlock(SPR_SIZE_8, SPR_SIZE_8);
		pass = result == EXPECTED;

		printf("result: %d on %lu expected %lu got %lu\n", pass, i, EXPECTED, result);

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
		printf("got zero result after trying to allocate when full");
	}

	printf("checking the table is fulling allocated\n");
	checkAllocationTableFilledWithValue(1);
}

void runTests()
{
	RAM_BANK = SPRITE_MEMORY_MANAGER_NEW_BANK;
	canFillWith8SizeBlocks();

	exit(0);
}
#endif // RUN_TESTS

#pragma rodata-name (push, "BANKRAM0D")

byte blocksBySize[4][4] = { {1, 2, 4, 8}, {2, 4, 8, 16}, {4, 8, 16, 32}, {8, 16, 32, 64} };

#define FAST_LOOKUP_SIZE 130
extern byte bDBlocksBySizeFastLookup[FAST_LOOKUP_SIZE];

#pragma rodata-name (pop)

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