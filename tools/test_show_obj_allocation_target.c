/* Compiled-C allocation lifecycle test for the production show.obj helpers. */
#include "showObj.h"
#include "spriteAllocator.h"

#define TEST_SHOW_OBJ_MAX_ALLOCATIONS (TOTAL_REAL_BLOCKS / ((SPR_SIZE_64 / 8) * (SPR_SIZE_64 / 8)))

byte testShowObjPartialCounts[MAX_SHOW_OBJ_SPLIT_SPRITES];
byte testShowObjTrailingNonZero[MAX_SHOW_OBJ_SPLIT_SPRITES];
byte testShowObjRetryCounts[MAX_SHOW_OBJ_SPLIT_SPRITES];
byte testShowObjFillCounts[MAX_SHOW_OBJ_SPLIT_SPRITES];
byte testShowObjSlotCapacity = MAX_SHOW_OBJ_SPLIT_SPRITES;
byte testShowObjMaxAllocationCount = TEST_SHOW_OBJ_MAX_ALLOCATIONS;

void testShowObjAllocationPositions(void)
{
	VeraSpriteAddress filled[TEST_SHOW_OBJ_MAX_ALLOCATIONS];
	VeraSpriteAddress slots[MAX_SHOW_OBJ_SPLIT_SPRITES];
	byte failureAt, i, filledCount, partialCount, retryCount;

	bDInitSpriteMemoryManager();
	for (failureAt = 0; failureAt < MAX_SHOW_OBJ_SPLIT_SPRITES; ++failureAt)
	{
		bDResetSpriteMemoryManager();
		filledCount = 0;
		for (i = 0; i < TEST_SHOW_OBJ_MAX_ALLOCATIONS; ++i)
		{
			filled[i] = bDFindFreeVramBlock(SPR_SIZE_64, SPR_SIZE_64);
			if (!filled[i])
				break;
			++filledCount;
		}
		testShowObjFillCounts[failureAt] = filledCount;
		if (filledCount != TEST_SHOW_OBJ_MAX_ALLOCATIONS)
			continue;

		for (i = 0; i < failureAt; ++i)
			bDDeleteAllocation(filled[filledCount - i - 1], SPR_SIZE_64, SPR_SIZE_64);

		/* Seed the unfilled suffix with live allocations to expose stale slots. */
		for (i = 0; i < MAX_SHOW_OBJ_SPLIT_SPRITES; ++i)
			slots[i] = filled[i];
		partialCount = b11AllocateShowObjSlots(SPR_SIZE_64, SPR_SIZE_64,
			MAX_SHOW_OBJ_SPLIT_SPRITES, slots);
		testShowObjPartialCounts[failureAt] = partialCount;
		testShowObjTrailingNonZero[failureAt] = 0;
		for (i = partialCount; i < MAX_SHOW_OBJ_SPLIT_SPRITES; ++i)
		{
			if (slots[i])
				testShowObjTrailingNonZero[failureAt] = 1;
		}

		b11ReleaseShowObjSlots(SPR_SIZE_64, SPR_SIZE_64, partialCount, slots);
		for (i = 0; i < MAX_SHOW_OBJ_SPLIT_SPRITES; ++i)
			bDDeleteAllocation(filled[i], SPR_SIZE_64, SPR_SIZE_64);

		retryCount = b11AllocateShowObjSlots(SPR_SIZE_64, SPR_SIZE_64,
			MAX_SHOW_OBJ_SPLIT_SPRITES, slots);
		testShowObjRetryCounts[failureAt] = retryCount;
		b11ReleaseShowObjSlots(SPR_SIZE_64, SPR_SIZE_64, retryCount, slots);
	}
}
