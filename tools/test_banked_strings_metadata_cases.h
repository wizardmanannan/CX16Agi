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

#define TEST_METADATA_CACHE_CAPACITY 10
#define TEST_METADATA_BITMAP_MAX 256
#define TEST_METADATA_EXPECTED_UNSET_SLOT 21u /* SPRITE_SLOTS + 1 */

static uint16_t metadata_view_mask(unsigned first, unsigned last)
{
	uint16_t mask = 0;
	unsigned view;

	for (view = first; view <= last; ++view)
		mask |= (uint16_t)(1u << view);
	return mask;
}

static size_t metadata_check_snapshot(uint8_t expected_active, uint16_t expected_cached,
	uint8_t expected_slot, uint8_t expected_next, unsigned expected_allocations,
	uint8_t *allocation_bitmap)
{
	uint16_t active_pointer_address = symbol("_testMetadataLifecycleActivePointer");
	uint16_t list_pointer_address = symbol("_testMetadataLifecycleListPointer");
	uint16_t cached_views = symbol("_testMetadataLifecycleCachedViews");
	uint16_t cached_pointers = symbol("_testMetadataLifecycleCachedPointers");
	uint16_t cached_banks = symbol("_testMetadataLifecycleCachedBanks");
	uint16_t cached_owners = symbol("_testMetadataLifecycleCachedOwners");
	uint16_t bitmap_address = symbol("_testMetadataLifecycleAllocatorBitmap");
	uint16_t active_pointer, list_pointer, seen_cached = 0;
	unsigned metadata_owner_count = 0, allocation_count = 0, i, j;
	uint8_t active = ram[symbol("_testMetadataLifecycleActiveView")];
	uint8_t active_bank = ram[symbol("_testMetadataLifecycleActiveBank")];
	uint8_t active_owner = ram[symbol("_testMetadataLifecycleActiveOwner")];
	uint8_t list_bank = ram[symbol("_testMetadataLifecycleListBank")];
	uint8_t list_owner = ram[symbol("_testMetadataLifecycleListOwner")];
	uint8_t mapped_slot = ram[symbol("_testMetadataLifecycleMappedSlot")];
	uint8_t next_slot = ram[symbol("_testMetadataLifecycleNextSlot")];
	uint8_t bitmap_length = ram[symbol("_testMetadataLifecycleAllocatorBitmapLength")];
	uint8_t word_size = ram[symbol("_testMetadataLifecycleWordSize")];
	uint8_t unset_slot = ram[symbol("_testMetadataLifecycleUnsetSlot")];

	CHECK(word_size == sizeof(uint16_t), "Metadata target word size differs from the host snapshot reader");
	CHECK(unset_slot == TEST_METADATA_EXPECTED_UNSET_SLOT,
		"Metadata fixture sentinel differs from SPRITE_SLOTS + 1");
	CHECK(active == expected_active, "Metadata switch selected the wrong active view");
	CHECK(mapped_slot == expected_slot, "Metadata switch changed the view-table slot mapping");
	CHECK(next_slot == expected_next, "Metadata switch changed the next metadata slot unexpectedly");
	active_pointer = get_word(active_pointer_address);
	list_pointer = get_word(list_pointer_address);
	if (expected_active)
	{
		CHECK(active_pointer != 0 && active_owner, "Active metadata pointer is not owned by the real allocator");
		metadata_owner_count = 1;
	}
	else
	{
		CHECK(active_pointer == 0 && !active_owner, "Reset metadata retained an active allocation");
		CHECK(active_bank == 0, "Reset metadata retained an active allocation bank");
	}
	if (expected_allocations)
		CHECK(list_pointer != 0 && list_owner, "Inactive-list pointer is not owned by the real allocator");
	else
		CHECK(list_pointer == 0 && !list_owner && list_bank == 0,
			"Empty metadata state retained an inactive-list allocation");

	for (i = 0; i < TEST_METADATA_CACHE_CAPACITY; ++i)
	{
		uint8_t view = ram[cached_views + i];
		uint8_t bank = ram[cached_banks + i];
		uint16_t pointer = get_word((uint16_t)(cached_pointers + i * word_size));
		uint16_t bit;

		if (pointer)
		{
			CHECK(view > 0 && view <= 12, "Inactive metadata contains an unexpected view ID");
			bit = (uint16_t)(1u << view);
			CHECK(expected_cached & bit, "Inactive metadata contains an unexpected view");
			CHECK(!(seen_cached & bit), "Inactive metadata contains a duplicate view");
			CHECK(ram[cached_owners + i], "Inactive metadata pointer is not owned by the real allocator");
			if (view == expected_active && expected_active)
				CHECK(pointer == active_pointer && bank == active_bank,
					"Cached active view lost its exact pointer and bank alias");
			else
			{
				CHECK(pointer != active_pointer || bank != active_bank,
					"Distinct active and inactive views alias the same banked allocation");
				for (j = 0; j < i; ++j)
				{
					uint16_t previous_pointer = get_word((uint16_t)(cached_pointers + j * word_size));
					uint8_t previous_bank = ram[cached_banks + j];
					CHECK(!previous_pointer || previous_pointer != pointer || previous_bank != bank,
						"Distinct cached views alias the same banked allocation");
				}
				++metadata_owner_count;
			}
			seen_cached |= bit;
		}
		else
		{
			CHECK(view == 0 && bank == 0 && !ram[cached_owners + i],
				"Empty inactive metadata slot retained stale owner fields");
		}
	}
	CHECK(seen_cached == expected_cached, "Inactive metadata list has the wrong live view set");
	CHECK(metadata_owner_count + (list_pointer != 0) == expected_allocations,
		"Active and inactive records have the wrong number of unique owners");

	CHECK(bitmap_length > 0, "Metadata allocator snapshot is empty");
	for (i = 0; i < bitmap_length; ++i)
	{
		uint8_t allocated = ram[bitmap_address + i];
		CHECK(allocated <= 1, "Metadata allocator bitmap contains an invalid flag");
		allocation_bitmap[i] = allocated;
		allocation_count += allocated != 0;
	}
	CHECK(allocation_count == expected_allocations, "Real allocator owns the wrong number of metadata allocations");
	CHECK(allocation_count == ram[symbol("_testMetadataLifecycleAllocatorCount")],
		"Target metadata allocation count disagrees with its allocator bitmap");
	return bitmap_length;
}

static void request_metadata_view(uint8_t view)
{
	ram[symbol("_testMetadataLifecycleRequestedView")] = view;
	call_target_caller("_testRunMetadataLifecycleSwitch");
}

static void run_metadata_switch_lifecycle(void)
{
	uint8_t before_revisit[TEST_METADATA_BITMAP_MAX];
	uint8_t after_revisit[TEST_METADATA_BITMAP_MAX];
	uint8_t after_first_reset[TEST_METADATA_BITMAP_MAX];
	uint8_t after_second_reset[TEST_METADATA_BITMAP_MAX];
	uint16_t cached_pointers, cached_views;
	uint16_t cached_view_pointer = 0;
	uint8_t cached_view_bank = 0;
	uint8_t found_cached_view = 0;
	uint8_t word_size;
	size_t before_length, after_length, first_reset_length, second_reset_length;
	uint16_t expected_cached;
	unsigned view, i;

	prepare();
	call_target_caller("_testPrepareMetadataLifecycle");
	metadata_check_snapshot(0, 0, ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, before_revisit);

	for (view = 1; view <= 5; ++view)
	{
		request_metadata_view((uint8_t)view);
		metadata_check_snapshot((uint8_t)view, metadata_view_mask(1, view - 1), 0, 1,
			view + 1, before_revisit);
	}

	cached_views = symbol("_testMetadataLifecycleCachedViews");
	cached_pointers = symbol("_testMetadataLifecycleCachedPointers");
	word_size = ram[symbol("_testMetadataLifecycleWordSize")];
	for (i = 0; i < TEST_METADATA_CACHE_CAPACITY; ++i)
	{
		if (ram[cached_views + i] == 2)
		{
			cached_view_pointer = get_word((uint16_t)(cached_pointers + i * word_size));
			cached_view_bank = ram[symbol("_testMetadataLifecycleCachedBanks") + i];
			found_cached_view = 1;
			break;
		}
	}
	CHECK(found_cached_view && cached_view_pointer != 0,
		"Metadata cache-hit fixture did not contain view 2 before revisiting it");
	before_length = metadata_check_snapshot(5, metadata_view_mask(1, 4), 0, 1, 6, before_revisit);
	request_metadata_view(2);
	expected_cached = metadata_view_mask(1, 5);
	after_length = metadata_check_snapshot(2, expected_cached, 0, 1, 6, after_revisit);
	CHECK(get_word(symbol("_testMetadataLifecycleActivePointer")) == cached_view_pointer &&
		ram[symbol("_testMetadataLifecycleActiveBank")] == cached_view_bank,
		"Cached metadata revisit did not restore the original pointer and bank");
	CHECK(before_length == after_length && memcmp(before_revisit, after_revisit, before_length) == 0,
		"Cached metadata revisit changed real allocator ownership");
	call_target_caller("_testRunMetadataLifecycleReset");
	first_reset_length = metadata_check_snapshot(0, 0,
		ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, after_first_reset);
	request_metadata_view(2);
	metadata_check_snapshot(2, 0, 0, 1, 2, after_revisit);
	call_target_caller("_testRunMetadataLifecycleReset");
	second_reset_length = metadata_check_snapshot(0, 0,
		ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, after_second_reset);
	CHECK(first_reset_length == second_reset_length &&
		memcmp(after_first_reset, after_second_reset, first_reset_length) == 0,
		"Reset after a cached-owner alias changed allocator ownership");
	call_target_caller("_testPrepareMetadataLifecycle");
	metadata_check_snapshot(0, 0, ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, before_revisit);

	for (view = 1; view <= 11; ++view)
	{
		request_metadata_view((uint8_t)view);
		metadata_check_snapshot((uint8_t)view, metadata_view_mask(1, view - 1), 0, 1,
			view + 1, after_revisit);
	}

	request_metadata_view(12);
	metadata_check_snapshot(12, (uint16_t)(1u << 11), 0, 1, 3, after_revisit);

	call_target_caller("_testRunMetadataLifecycleReset");
	first_reset_length = metadata_check_snapshot(0, 0,
		ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, after_first_reset);
	call_target_caller("_testRunMetadataLifecycleReset");
	second_reset_length = metadata_check_snapshot(0, 0,
		ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, after_second_reset);
	CHECK(first_reset_length == second_reset_length &&
		memcmp(after_first_reset, after_second_reset, first_reset_length) == 0,
		"Repeated real metadata reset changed allocator ownership");

	request_metadata_view(12);
	metadata_check_snapshot(12, 0, 0, 1, 2, after_revisit);
	call_target_caller("_testRunMetadataLifecycleReset");
	metadata_check_snapshot(0, 0, ram[symbol("_testMetadataLifecycleUnsetSlot")], 0, 0, after_revisit);
	puts("Real metadata switch, cache hit, eviction and reset lifecycle passes");
}

static void run_metadata_cases(void)
{
	run_metadata_bank_alias();
	run_metadata_full_reset();
	run_metadata_switch_lifecycle();
}

#endif
