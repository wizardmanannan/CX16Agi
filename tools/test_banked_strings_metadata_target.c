/* Test callers for production metadata cleanup with real allocator data. */
#include "helpers.h"
#include "memoryManager.h"
#include "view.h"
#include <cx16.h>

extern ViewTableMetadata viewTableMetadata[SPRITE_SLOTS];
extern byte viewTabNoToMetaData[MAXVIEW];
extern byte nextViewMetadataSlot;
extern MemoryArea *_memoryAreas;
extern byte allocationArray[ALLOCATION_ARRAY_SIZE];

#pragma wrapped-call (push, trampoline, SPRITE_METADATA_BANK)
extern void bEResetInactiveViewTableMetadata(ViewTableMetadata *localViewTableMetadata);
extern void bEResetViewTableMetadata(void);
extern void bEResetSpritePointers(void);
extern void bESwitchMetadata(ViewTable *localViewTab, View *localView, byte viewNum, byte entryNum);
#pragma wrapped-call (pop)

#define TEST_METADATA_CACHE_CAPACITY 10
#define TEST_METADATA_UNSET_SLOT (SPRITE_SLOTS + 1)

byte *testMetadataActivePointer;
byte testMetadataActiveBank;
byte *testMetadataCrossBankPointer;
byte testMetadataCrossBankBank;
byte *testMetadataInactiveList;
byte testMetadataInactiveBank;
byte testMetadataActiveOwnerAfterInactiveReset;
byte testMetadataListOwnerAfterInactiveReset;
byte testMetadataOwnerClearedAfterReset;
byte testMetadataListClearedAfterReset;
static View testMetadataLifecycleView;
static ViewTable testMetadataLifecycleViewTable;
byte testMetadataLifecycleRequestedView;
byte testMetadataLifecycleActiveView;
word testMetadataLifecycleActivePointer;
byte testMetadataLifecycleActiveBank;
byte testMetadataLifecycleActiveOwner;
word testMetadataLifecycleListPointer;
byte testMetadataLifecycleListBank;
byte testMetadataLifecycleListOwner;
byte testMetadataLifecycleMappedSlot;
byte testMetadataLifecycleNextSlot;
byte testMetadataLifecycleCachedViews[TEST_METADATA_CACHE_CAPACITY];
word testMetadataLifecycleCachedPointers[TEST_METADATA_CACHE_CAPACITY];
byte testMetadataLifecycleCachedBanks[TEST_METADATA_CACHE_CAPACITY];
byte testMetadataLifecycleCachedOwners[TEST_METADATA_CACHE_CAPACITY];
byte testMetadataLifecycleAllocatorBitmap[ALLOCATION_ARRAY_SIZE];
byte testMetadataLifecycleAllocatorBitmapLength;
byte testMetadataLifecycleAllocatorCount;
byte testMetadataLifecycleWordSize = sizeof(word);
byte testMetadataLifecycleUnsetSlot = TEST_METADATA_UNSET_SLOT;

static boolean testMetadataLifecycleAllocationOwned(byte *pointer, byte bank)
{
	byte area, segment;
	unsigned long offset;
	int allocationIndex;

	if (!pointer)
		return FALSE;

	for (area = 0; area < NO_SIZES; ++area)
	{
		for (segment = 0; segment < _memoryAreas[area].noSegments; ++segment)
		{
			offset = (unsigned long)segment * _memoryAreas[area].segmentSize;
			if (pointer == (byte*)((unsigned long)BANK_MIN + offset % MEMORY_MANAGER_BANK_SIZE) &&
				bank == (byte)(offset / MEMORY_MANAGER_BANK_SIZE + _memoryAreas[area].firstBank))
			{
				allocationIndex = _memoryAreas[area].start - allocationArray + segment;
				return allocationIndex >= 0 && allocationIndex < ALLOCATION_ARRAY_SIZE &&
					allocationArray[allocationIndex] != 0;
			}
		}
	}
	return FALSE;
}

static void testCaptureMetadataLifecycle(void)
{
	byte previousBank = RAM_BANK;
	byte i;
	byte *list;
	ViewTableMetadata active, cached;

	RAM_BANK = SPRITE_METADATA_BANK;
	active = viewTableMetadata[0];
	testMetadataLifecycleActiveView = active.viewNum;
	testMetadataLifecycleActivePointer = (word)active.loopsVeraAddressesPointers;
	testMetadataLifecycleActiveBank = active.viewTableMetadataBank;
	testMetadataLifecycleListPointer = (word)active.inactive;
	testMetadataLifecycleListBank = active.inactiveBank;
	testMetadataLifecycleMappedSlot = viewTabNoToMetaData[0];
	testMetadataLifecycleNextSlot = nextViewMetadataSlot;
	list = (byte *)active.inactive;
	for (i = 0; i < TEST_METADATA_CACHE_CAPACITY; ++i)
	{
		memset(&cached, 0, sizeof(cached));
		if (list)
		{
			RAM_BANK = active.inactiveBank;
			cached = ((ViewTableMetadata *)list)[i];
		}
		testMetadataLifecycleCachedViews[i] = cached.viewNum;
		testMetadataLifecycleCachedPointers[i] = (word)cached.loopsVeraAddressesPointers;
		testMetadataLifecycleCachedBanks[i] = cached.viewTableMetadataBank;
	}

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	testMetadataLifecycleAllocatorBitmapLength = sizeof(allocationArray);
	testMetadataLifecycleAllocatorCount = 0;
	for (i = 0; i < ALLOCATION_ARRAY_SIZE; ++i)
	{
		testMetadataLifecycleAllocatorBitmap[i] = allocationArray[i];
		if (allocationArray[i])
			++testMetadataLifecycleAllocatorCount;
	}
	testMetadataLifecycleActiveOwner = testMetadataLifecycleAllocationOwned(
		(byte *)testMetadataLifecycleActivePointer, testMetadataLifecycleActiveBank);
	testMetadataLifecycleListOwner = testMetadataLifecycleAllocationOwned(
		(byte *)testMetadataLifecycleListPointer, testMetadataLifecycleListBank);
	for (i = 0; i < TEST_METADATA_CACHE_CAPACITY; ++i)
		testMetadataLifecycleCachedOwners[i] = testMetadataLifecycleAllocationOwned(
			(byte *)testMetadataLifecycleCachedPointers[i], testMetadataLifecycleCachedBanks[i]);
	RAM_BANK = previousBank;
}

void testPrepareMetadataLifecycle(void)
{
	byte previousBank = RAM_BANK;

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	RAM_BANK = SPRITE_METADATA_BANK;
	memset(viewTableMetadata, 0, sizeof(viewTableMetadata));
	memset(viewTabNoToMetaData, TEST_METADATA_UNSET_SLOT, MAXVIEW);
	nextViewMetadataSlot = 0;
	memset(&testMetadataLifecycleView, 0, sizeof(testMetadataLifecycleView));
	memset(&testMetadataLifecycleViewTable, 0, sizeof(testMetadataLifecycleViewTable));
	testMetadataLifecycleView.numberOfLoops = 1;
	testMetadataLifecycleView.maxCels = 1;
	testMetadataLifecycleView.maxVeraSlots = 1;
	testMetadataLifecycleRequestedView = 0;
	testCaptureMetadataLifecycle();
	RAM_BANK = previousBank;
}

void testRunMetadataLifecycleSwitch(void)
{
	byte previousBank = RAM_BANK;

	RAM_BANK = SPRITE_METADATA_BANK;
	bESwitchMetadata(&testMetadataLifecycleViewTable, &testMetadataLifecycleView,
		testMetadataLifecycleRequestedView, 0);
	testCaptureMetadataLifecycle();
	RAM_BANK = previousBank;
}

void testRunMetadataLifecycleReset(void)
{
	byte previousBank = RAM_BANK;

	RAM_BANK = SPRITE_METADATA_BANK;
	bEResetViewTableMetadata();
	bEResetSpritePointers();
	testCaptureMetadataLifecycle();
	RAM_BANK = previousBank;
}

static void testPrepareMetadataBankAlias(void)
{
	byte previousBank = RAM_BANK;
	byte allocationBanks[7];
	byte *allocations[7];
	ViewTableMetadata *inactive;
	byte i;

	RAM_BANK = MEMORY_MANAGEMENT_BANK;
	b10InitDynamicMemory();
	for (i = 0; i < 7; ++i)
		allocations[i] = b10BankedAlloc(161, &allocationBanks[i]);

	testMetadataActivePointer = allocations[0];
	testMetadataActiveBank = allocationBanks[0];
	testMetadataCrossBankPointer = allocations[5];
	testMetadataCrossBankBank = allocationBanks[5];
	testMetadataInactiveList = allocations[1];
	testMetadataInactiveBank = allocationBanks[1];

	RAM_BANK = allocationBanks[1];
	inactive = (ViewTableMetadata *)allocations[1];
	memset(inactive, 0, 161);
	inactive[0].loopsVeraAddressesPointers = (VeraSpriteAddress **)allocations[5];
	inactive[0].viewTableMetadataBank = allocationBanks[5];
	inactive[1].loopsVeraAddressesPointers = (VeraSpriteAddress **)allocations[0];
	inactive[1].viewTableMetadataBank = allocationBanks[0];

	RAM_BANK = SPRITE_METADATA_BANK;
	memset(viewTableMetadata, 0, sizeof(viewTableMetadata));
	viewTableMetadata[0].loopsVeraAddressesPointers = (VeraSpriteAddress **)allocations[0];
	viewTableMetadata[0].viewTableMetadataBank = allocationBanks[0];
	viewTableMetadata[0].inactive = allocations[1];
	viewTableMetadata[0].inactiveBank = allocationBanks[1];
	RAM_BANK = previousBank;
}

void testRunMetadataBankAlias(void)
{
	byte previousBank = RAM_BANK;
	testPrepareMetadataBankAlias();
	RAM_BANK = SPRITE_METADATA_BANK;
	bEResetInactiveViewTableMetadata(&viewTableMetadata[0]);
	testMetadataActiveOwnerAfterInactiveReset =
		viewTableMetadata[0].loopsVeraAddressesPointers == (VeraSpriteAddress **)testMetadataActivePointer &&
		viewTableMetadata[0].viewTableMetadataBank == testMetadataActiveBank;
	testMetadataListOwnerAfterInactiveReset =
		viewTableMetadata[0].inactive == testMetadataInactiveList &&
		viewTableMetadata[0].inactiveBank == testMetadataInactiveBank;
	RAM_BANK = previousBank;
}

void testRunMetadataFullReset(void)
{
	byte previousBank = RAM_BANK;
	testPrepareMetadataBankAlias();
	RAM_BANK = SPRITE_METADATA_BANK;
	bEResetViewTableMetadata();
	testMetadataOwnerClearedAfterReset =
		viewTableMetadata[0].loopsVeraAddressesPointers == NULL &&
		viewTableMetadata[0].viewTableMetadataBank == 0;
	testMetadataListClearedAfterReset =
		viewTableMetadata[0].inactive == NULL && viewTableMetadata[0].inactiveBank == 0;
	RAM_BANK = previousBank;
}

void testRunMetadataBankAliasRepeatReset(void)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = SPRITE_METADATA_BANK;
	bEResetViewTableMetadata();
	RAM_BANK = previousBank;
}
