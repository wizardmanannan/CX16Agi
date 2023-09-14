// TestMemoryAllocation.c.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "../../memoryManager.h"
#include <assert.h>

extern MemoryArea* _memoryAreas;
extern byte* banked;

void cleanUp()
{
	memset(_memoryAreas[0].start, 0, _noSegments);
}

void entireArrayCanBePopulatedAndFreed()
{
	printf("Running entireArrayCanBePopulatedAndFreed\n");
	const testDataSize = TINY_SIZE;
	for (int i = 0; i < _noSegments; i++)
	{
		int bank = 0;

		unsigned int result = b8Bbanked_alloc(testDataSize, &bank);
		assert(result > 0);

		assert(bank);
		assert(banked[i]);
	}

	for (int i = 0; i < _noSegments; i++)
	{
		assert(banked[i]);
	}

	cleanUp();
}

void ramOnFirstBankOfTinySegmentZeroCanBeFreed()
{
	printf("Running ramOnFirstBankOfTinySegmentZeroCanBeFreed\n");
	const testDataSize = TINY_SIZE;
	int bank = 0;
	byte* allocatedAddress = b8Bbanked_alloc(testDataSize, &bank);
	
	assert(banked[0]);
	assert(allocatedAddress);
	
	assert(banked_dealloc(allocatedAddress, TINY_FIRST_BANK));
	assert(!banked[0]);

	cleanUp();
}

void ramIsAllocatedToNextBankWhenOneBankIsFull()
{
	printf("Running ramIsAllocatedToNextBankWhenOneBankIsFull\n");
	const testDataSize = SMALL_SIZE;
	int bank = 0;


	for (int i = 0; i * _memoryAreas[SMALL_SEG_ORDER].segmentSize < BANK_SIZE; i++)
	{
		b8Bbanked_alloc(testDataSize, &bank);
	}

	byte* allocatedAddress = b8Bbanked_alloc(testDataSize, &bank);

	for (int i = _memoryAreas[SMALL_SEG_ORDER].start; i < _memoryAreas[SMALL_SEG_ORDER].noSegments + 1; i++)
	{
		assert(banked[i]);
	}
	assert(allocatedAddress == &banked[0]);

	allocatedAddress = b8Bbanked_alloc(testDataSize, &bank);
	assert(allocatedAddress == &banked[0] + _memoryAreas[SMALL_SEG_ORDER].segmentSize);

	cleanUp();
}

void allSizesCanBeAllocated()
{
	printf("allSizesCanBeAllocated\n");
	int bank = 0;
	

	byte* allocatedAddress = b8Bbanked_alloc(TINY_SEG_ORDER, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[TINY_SEG_ORDER].firstBank);

	allocatedAddress = b8Bbanked_alloc(EXTRA_SMALL_SIZE, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[EXTRA_SMALL_SEG_ORDER].firstBank);

	allocatedAddress = b8Bbanked_alloc(SMALL_SIZE, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[SMALL_SEG_ORDER].firstBank);

	allocatedAddress = b8Bbanked_alloc(MEDIUM_SIZE, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[MEDIUM_SEG_ORDER].firstBank);

	allocatedAddress = b8Bbanked_alloc(LARGE_SIZE, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[LARGE_SEG_ORDER].firstBank);

	cleanUp();
}

void canAllocateToSecondLargeBank()
{
	printf("can allocate to second large bank\n");
	int bank = 0;

	byte* allocatedAddress = b8Bbanked_alloc(LARGE_SIZE, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[LARGE_SEG_ORDER].firstBank);

	allocatedAddress = b8Bbanked_alloc(LARGE_SIZE, &bank);
	assert(allocatedAddress == &banked[0]);
	assert(bank == _memoryAreas[LARGE_SEG_ORDER].firstBank + 1);

	cleanUp();
}


int main()
{
	int _ = 0;
	memoryMangerInit();
	entireArrayCanBePopulatedAndFreed();
	ramOnFirstBankOfTinySegmentZeroCanBeFreed();
	ramIsAllocatedToNextBankWhenOneBankIsFull();
	allSizesCanBeAllocated();
	canAllocateToSecondLargeBank();
	printf("Press any key");
	
	scanf("%d", _);
}
