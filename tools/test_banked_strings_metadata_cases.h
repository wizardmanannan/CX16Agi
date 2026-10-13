#ifndef TEST_BANKED_STRINGS_METADATA_CASES_H
#define TEST_BANKED_STRINGS_METADATA_CASES_H

static void run_metadata_bank_alias(void)
{
	uint16_t map;
	uint16_t active_owner_after_inactive = symbol("_testMetadataActiveOwnerAfterInactiveReset");
	uint16_t list_owner_after_inactive = symbol("_testMetadataListOwnerAfterInactiveReset");
	uint16_t first_pointer = symbol("_testMetadataActivePointer");
	uint16_t cross_bank_pointer = symbol("_testMetadataCrossBankPointer");
	uint16_t inactive_list = symbol("_testMetadataInactiveList");
	uint16_t first_bank = symbol("_testMetadataActiveBank");
	uint16_t cross_bank = symbol("_testMetadataCrossBankBank");
	uint16_t list_bank = symbol("_testMetadataInactiveBank");
	prepare();
	call_target_caller("_testRunMetadataBankAlias");
	CHECK(get_word(first_pointer) == BANK_START && ram[first_bank] == 0x15,
		"Real allocator did not produce the active owner at A000 bank 15");
	CHECK(get_word(cross_bank_pointer) == BANK_START && ram[cross_bank] == 0x16,
		"Real allocator did not produce the inactive owner at A000 bank 16");
	CHECK(get_word(inactive_list) == BANK_START + 1600 && ram[list_bank] == 0x15,
		"Real allocator did not place the inactive metadata list after the bank-15 active owner");
	map = (uint16_t)(symbol("_allocationArray") - BANK_START);
	CHECK(bank_ram[MEMORY_MANAGEMENT_BANK][map + 130] == 1 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 131] == 1 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 135] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 136] == 1,
		"Inactive cleanup did not free only the different-bank owner and preserve the same-bank owner");
	for (unsigned i = 132; i <= 134; ++i)
		CHECK(bank_ram[MEMORY_MANAGEMENT_BANK][map + i] == 1,
			"Inactive cleanup changed a filler allocation owned by another slot");
	CHECK(ram[active_owner_after_inactive] == 1 && ram[list_owner_after_inactive] == 1,
		"Inactive cleanup changed active or list ownership before full reset");
}

static void run_metadata_full_reset(void)
{
	uint16_t map;
	uint16_t owner_cleared_after_reset = symbol("_testMetadataOwnerClearedAfterReset");
	uint16_t list_cleared_after_reset = symbol("_testMetadataListClearedAfterReset");
	uint16_t first_pointer = symbol("_testMetadataActivePointer");
	uint16_t cross_bank_pointer = symbol("_testMetadataCrossBankPointer");
	uint16_t inactive_list = symbol("_testMetadataInactiveList");
	uint16_t first_bank = symbol("_testMetadataActiveBank");
	uint16_t cross_bank = symbol("_testMetadataCrossBankBank");
	uint16_t list_bank = symbol("_testMetadataInactiveBank");
	prepare();
	call_target_caller("_testRunMetadataFullReset");
	CHECK(get_word(first_pointer) == BANK_START && ram[first_bank] == 0x15,
		"Real allocator did not produce the active owner at A000 bank 15");
	CHECK(get_word(cross_bank_pointer) == BANK_START && ram[cross_bank] == 0x16,
		"Real allocator did not produce the inactive owner at A000 bank 16");
	CHECK(get_word(inactive_list) == BANK_START + 1600 && ram[list_bank] == 0x15,
		"Real allocator did not place the inactive metadata list after the bank-15 active owner");
	map = (uint16_t)(symbol("_allocationArray") - BANK_START);
	CHECK(bank_ram[MEMORY_MANAGEMENT_BANK][map + 130] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 131] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 135] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 136] == 1,
		"Full metadata reset did not release active, cross-bank and list owners while preserving the filler");
	for (unsigned i = 132; i <= 134; ++i)
		CHECK(bank_ram[MEMORY_MANAGEMENT_BANK][map + i] == 1,
			"Full metadata reset changed a filler allocation owned by another slot");
	CHECK(ram[owner_cleared_after_reset] == 1 && ram[list_cleared_after_reset] == 1,
		"Full metadata reset did not clear the active and list owners");
	call_target_caller("_testRunMetadataBankAliasRepeatReset");
	CHECK(bank_ram[MEMORY_MANAGEMENT_BANK][map + 130] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 131] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 135] == 0 &&
		bank_ram[MEMORY_MANAGEMENT_BANK][map + 136] == 1,
		"Repeated metadata reset changed released allocation ownership");
	for (unsigned i = 132; i <= 134; ++i)
		CHECK(bank_ram[MEMORY_MANAGEMENT_BANK][map + i] == 1,
			"Repeated metadata reset changed a filler allocation owned by another slot");
}

static void run_metadata_cases(void)
{
	run_metadata_bank_alias();
	run_metadata_full_reset();
}

#endif
