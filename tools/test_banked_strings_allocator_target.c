/* Exhaust the production allocator's large segment class from a target caller. */
#include "helpers.h"
#include "memoryManager.h"

extern MemoryArea *_memoryAreas;
extern byte allocationArray[ALLOCATION_ARRAY_SIZE];

byte testAllocatorSuccessfulCount;
byte testAllocatorExpectedCount;
byte testAllocatorExpectedBitmapLength;
byte testAllocatorExpectedAllocationStart;
byte testAllocatorExpectedFirstBank;
int testAllocatorExpectedSize;
byte *testAllocatorSuccessfulPointers[LARGE_NO_SEGMENTS];
byte testAllocatorSuccessfulBanks[LARGE_NO_SEGMENTS];
byte *testAllocatorUnexpectedPointer;
byte testAllocatorUnexpectedBank;
byte testAllocatorReachedExhaustion;
byte testAllocatorPostCallMarker;

void testRunAllocatorExhaustion(void)
{
	byte previousBank = RAM_BANK;
	byte bank = 0;
	byte i;

	testAllocatorSuccessfulCount = 0;
	testAllocatorExpectedCount = LARGE_NO_SEGMENTS;
	testAllocatorExpectedBitmapLength = ALLOCATION_ARRAY_SIZE;
	testAllocatorExpectedFirstBank = LARGE_FIRST_BANK;
	testAllocatorExpectedSize = LARGE_SIZE;
	testAllocatorUnexpectedPointer = 0;
	testAllocatorUnexpectedBank = 0;
	testAllocatorReachedExhaustion = 0;
	testAllocatorPostCallMarker = 0;

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	testAllocatorExpectedAllocationStart = (byte)(_memoryAreas[LARGE_SEG_ORDER].start - allocationArray);
	RAM_BANK = previousBank;

	for (i = 0; i < LARGE_NO_SEGMENTS; ++i)
	{
		testAllocatorSuccessfulPointers[i] = b10BankedAlloc(LARGE_SIZE, &bank);
		testAllocatorSuccessfulBanks[i] = bank;
		++testAllocatorSuccessfulCount;
	}
	testAllocatorReachedExhaustion = 1;
	testAllocatorUnexpectedPointer = b10BankedAlloc(LARGE_SIZE, &testAllocatorUnexpectedBank);
	testAllocatorPostCallMarker = 1;
}
