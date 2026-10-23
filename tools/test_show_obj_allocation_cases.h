#ifndef TEST_SHOW_OBJ_ALLOCATION_CASES_H
#define TEST_SHOW_OBJ_ALLOCATION_CASES_H

static void run_show_obj_allocation_cases(void)
{
	unsigned failureAt, slotCapacity, maxAllocationCount;
	prepare();
	call_target_caller("_testShowObjAllocationPositions");
	slotCapacity = ram[symbol("_testShowObjSlotCapacity")];
	maxAllocationCount = ram[symbol("_testShowObjMaxAllocationCount")];
	CHECK(slotCapacity > 0 && slotCapacity <= 4,
		"Compiled show.obj helper has an unsupported split-slot capacity");
	for (failureAt = 0; failureAt < slotCapacity; ++failureAt)
	{
		CHECK(ram[symbol("_testShowObjFillCounts") + failureAt] ==
			maxAllocationCount,
			"Real sprite allocator did not fill all available 64x64 slots");
		CHECK(ram[symbol("_testShowObjPartialCounts") + failureAt] == failureAt,
			"Show.obj helper failed at the wrong allocation position");
		CHECK(ram[symbol("_testShowObjTrailingNonZero") + failureAt] == 0,
			"Failed show.obj allocation left stale addresses in unused slots");
		CHECK(ram[symbol("_testShowObjRetryCounts") + failureAt] ==
			slotCapacity,
			"Show.obj allocation could not retry with the real sprite allocator");
	}
	puts("Compiled show.obj allocation helpers recover after every real allocator failure position");
}

#endif
