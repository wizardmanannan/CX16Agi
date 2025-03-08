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
extern boolean OPTIMISTIC_MODE;

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
	byte i, j, powI, powJ;
	VeraSpriteAddress result;
	for (i = 0, powI = 1; i < 4; i++, powI *= 2)
	{
		for (j = 0, powJ = 1; j < 4; j++, powJ *= 2)
		{
			bDResetSpriteMemoryManager();

			fillWithBlocks((SpriteAllocationSize)(powI * 8), (SpriteAllocationSize)(powJ * 8), FALSE);
					
			bDDeleteAllocation(SPRITES_DATA_START, (SpriteAllocationSize)(powI * 8), (SpriteAllocationSize)(powJ * 8));
				
			result = bDFindFreeVramBlock((SpriteAllocationSize)(powI * 8), (SpriteAllocationSize)(powJ * 8));
			
			//asm("stp");
			printf("can repopulate after delete for %d,%d expected %lx got %lx.", (SpriteAllocationSize)(powI * 8), (SpriteAllocationSize)(powJ * 8), (VeraSpriteAddress) SPRITES_DATA_START, result);
			if (result == SPRITES_DATA_START)
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

void runTests()
{
	byte i, j, powI, powJ;
	RAM_BANK = SPRITE_MEMORY_MANAGER_NEW_BANK;
	
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