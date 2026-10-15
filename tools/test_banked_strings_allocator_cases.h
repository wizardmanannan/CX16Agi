#ifndef TEST_BANKED_STRINGS_ALLOCATOR_CASES_H
#define TEST_BANKED_STRINGS_ALLOCATOR_CASES_H

static void run_allocator_oom_case(void)
{
	char expected_text[64];
	uint8_t expected_output[64];
	size_t expected_output_length;
	uint16_t successful_pointers, successful_banks, allocation_array;
	uint16_t expected_count, bitmap_length, allocation_start, first_bank, expected_size;
	uint16_t unexpected_pointer, unexpected_bank, reached_exhaustion, post_call_marker;
	uint16_t exit_status;
	unsigned count, i;
	bool reached_exit;

	prepare();
	allocator_oom_runtime_active = true;
	allocator_oom_output_capture_active = true;
	reached_exit = call_target_caller_until_exit("_testRunAllocatorExhaustion");
	allocator_oom_output_capture_active = false;
	CHECK(reached_exit, "Real allocator returned after all large allocations were consumed");

	expected_count = symbol("_testAllocatorExpectedCount");
	bitmap_length = symbol("_testAllocatorExpectedBitmapLength");
	allocation_start = symbol("_testAllocatorExpectedAllocationStart");
	first_bank = symbol("_testAllocatorExpectedFirstBank");
	expected_size = symbol("_testAllocatorExpectedSize");
	successful_pointers = symbol("_testAllocatorSuccessfulPointers");
	successful_banks = symbol("_testAllocatorSuccessfulBanks");
	unexpected_pointer = symbol("_testAllocatorUnexpectedPointer");
	unexpected_bank = symbol("_testAllocatorUnexpectedBank");
	reached_exhaustion = symbol("_testAllocatorReachedExhaustion");
	post_call_marker = symbol("_testAllocatorPostCallMarker");
	allocation_array = symbol("_allocationArray");
	count = ram[symbol("_testAllocatorSuccessfulCount")];
	CHECK(count > 0 && count == ram[expected_count], "Target caller did not complete every configured large allocation");
	CHECK(ram[reached_exhaustion] == 1, "Target caller did not reach its final real allocation call");
	CHECK(ram[post_call_marker] == 0 && get_word(unexpected_pointer) == 0 && ram[unexpected_bank] == 0,
		"Real allocator returned from an exhausted allocation");

	CHECK(allocation_array >= BANK_START && allocation_array < BANK_END,
		"Real allocator bitmap is outside the memory-management bank window");
	CHECK((unsigned)(allocation_array - BANK_START) + ram[bitmap_length] <= BANK_SIZE,
		"Real allocator bitmap exceeds its bank window");
	CHECK((unsigned)ram[allocation_start] + count <= ram[bitmap_length],
		"Real allocator's large segment range exceeds its bitmap");
	for (i = 0; i < count; ++i)
	{
		uint16_t pointer = get_word((uint16_t)(successful_pointers + i * 2));
		uint8_t bank = ram[successful_banks + i];
		CHECK(pointer == BANK_START && bank == (uint8_t)(ram[first_bank] + i),
			"A successful target allocation did not retain its expected bank owner");
	}
	for (i = 0; i < ram[bitmap_length]; ++i)
	{
		uint8_t allocated = bank_ram[MEMORY_MANAGEMENT_BANK]
			[(allocation_array - BANK_START) + i];
		uint8_t expected = i >= ram[allocation_start] && i < ram[allocation_start] + count;
		CHECK(allocated == expected, "Allocator exhaustion changed an unexpected ownership bit");
	}

	exit_status = (uint16_t)(regs.a | ((uint16_t)regs.x << 8));
	if (exit_status != 1)
		fprintf(stderr, "cc65 _exit received A=$%02X X=$%02X\n", regs.a, regs.x);
	CHECK(exit_status == 1,
		"Real allocator exhaustion reached cc65 _exit with status 0 instead of EXIT_FAILURE");

	CHECK(!allocator_oom_output_overflow, "Allocator exhaustion printed more than the test output buffer can hold");
	CHECK(snprintf(expected_text, sizeof(expected_text), "Out of dynamic memory. Amount %d\n",
		(int)get_word(expected_size)) < (int)sizeof(expected_text), "Allocator diagnostic is too long for the host assertion");
	expected_output_length = strlen(expected_text);
	/* Match the CX16 PETSCII conversion and KERNAL's line-feed to carriage-return output. */
	for (i = 0; i < expected_output_length; ++i)
	{
		uint8_t character = (uint8_t)expected_text[i];
		if (character >= 'A' && character <= 'Z')
			character += 0x80;
		else if (character >= 'a' && character <= 'z')
			character -= 32;
		else if (character == '\n')
			character = '\r';
		expected_output[i] = character;
	}
	if (allocator_oom_output_length != expected_output_length ||
		memcmp(allocator_oom_output, expected_output, expected_output_length))
	{
		fprintf(stderr, "Captured allocator diagnostic (%lu bytes):",
			(unsigned long)allocator_oom_output_length);
		for (i = 0; i < allocator_oom_output_length; ++i)
			fprintf(stderr, " %02X", allocator_oom_output[i]);
		fputc('\n', stderr);
	}
	CHECK(allocator_oom_output_length == expected_output_length &&
		!memcmp(allocator_oom_output, expected_output, expected_output_length),
		"Production printf did not publish exactly the allocator exhaustion diagnostic");
	puts("Real target allocator exhaustion exits with failure and preserves every prior owner");
}

#endif
