/* Test callers for production metadata cleanup with real allocator data. */
#include "helpers.h"
#include "memoryManager.h"
#include "view.h"
#include <cx16.h>

extern ViewTableMetadata viewTableMetadata[SPRITE_SLOTS];

#pragma wrapped-call (push, trampoline, SPRITE_METADATA_BANK)
extern void bEResetInactiveViewTableMetadata(ViewTableMetadata *localViewTableMetadata);
extern void bEResetViewTableMetadata(void);
#pragma wrapped-call (pop)

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
