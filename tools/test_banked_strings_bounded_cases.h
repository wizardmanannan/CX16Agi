#ifndef TEST_BANKED_STRINGS_BOUNDED_CASES_H
#define TEST_BANKED_STRINGS_BOUNDED_CASES_H

static void run_string_length(uint16_t source, uint8_t bank, uint16_t byte_limit, bool interrupts_masked)
{
	put_word(symbol("_testBankedStringSource"), source);
	ram[symbol("_testBankedStringBank")] = bank;
	put_word(symbol("_testBankedStringByteLimit"), byte_limit);
	ram[symbol("_testBankedStringLength")] = 0xA5;
	ram[symbol("_testBankedStringLength") + 1] = 0xA5;
	ram[symbol("_testBankedStringTerminated")] = 0xA5;
	regs.sp = 0x1FD;
	regs.a = regs.x = regs.y = 0;
	regs.status = FLAG_CONSTANT | (interrupts_masked ? FLAG_INTERRUPT : 0);
	ram[0x1FE] = 0xFF;
	ram[0x1FF] = 0x01;
	bank_window_reads = 0;
	out_of_window_reads = 0;
	track_source_reads = true;
	regs.pc = symbol("_testRunBankedString");
	unsigned steps = 0;
	while (regs.pc != RETURN_PC)
	{
		CHECK(++steps < 100000, "Banked string caller did not return");
		step6502();
	}
	track_source_reads = false;
	CHECK(regs.sp == 0x1FF, "Banked string caller changed hardware stack");
	CHECK(get_word(symbol("c_sp")) == 0x9000, "Banked string caller corrupted C argument stack");
	CHECK(ram[0] == CALLER_BANK && ram[1] == 0x53, "Banked string helper did not restore caller banks");
	CHECK((bool)(regs.status & FLAG_INTERRUPT) == interrupts_masked, "Banked string caller changed caller I flag");
}

static uint16_t result_length(void)
{
	return get_word(symbol("_testBankedStringLength"));
}

static bool result_terminated(void)
{
	return ram[symbol("_testBankedStringTerminated")] != 0;
}

static void put_bank_byte(uint8_t bank, uint16_t address, uint8_t value)
{
	CHECK(address >= BANK_START && address < BANK_END, "Fixture byte is outside the bank window");
	bank_ram[bank][address - BANK_START] = value;
}

static void check_result(uint16_t expected_length, bool expected_terminated)
{
	CHECK(result_length() == expected_length, "Bounded banked string length was incorrect");
	CHECK(result_terminated() == expected_terminated, "Bounded banked string termination result was incorrect");
	CHECK(out_of_window_reads == 0, "Bounded banked string read at or beyond $C000");
}

static void run_game_id_message(const char *message, bool should_accept)
{
	static const uint8_t previous_id[] = { 'S', 'A', 'F', 'E', 0, 0x5A, 0xC3 };
	uint16_t game_id;
	uint16_t logic_index;
	uint16_t sound_index;
	uint16_t view_index;
	uint16_t is_inited;
	prepare();
	game_id = symbol("_gameId");
	logic_index = symbol("_b4LogicIndex");
	sound_index = symbol("_b4SoundIndex");
	view_index = symbol("_b4ViewIndex");
	is_inited = symbol("_b4IsInited");
	CHECK(game_id < BANK_START, "Game id test expects the published id in resident RAM");
	CHECK(logic_index >= BANK_START && logic_index < BANK_END &&
		sound_index >= BANK_START && sound_index < BANK_END &&
		view_index >= BANK_START && view_index < BANK_END,
		"Dependency tables are outside their production bank window");
	memcpy(ram + game_id, previous_id, sizeof(previous_id));
	memset(bank_ram[4] + logic_index - BANK_START, 0, 768);
	memset(bank_ram[4] + sound_index - BANK_START, 0, 768);
	memset(bank_ram[4] + view_index - BANK_START, 0, 768);
	bank_ram[4][is_inited - BANK_START] = 0;
	ram[symbol("codeWindow")] = 1;
	put_word(symbol("_testGameIdMessage"), 0xA100);
	ram[symbol("_testGameIdMessageBank")] = 0x24;
	memcpy(bank_ram[0x24] + 0x100, message, strlen(message) + 1);
	call_target_caller("_testRunSetGameId");
	if (should_accept)
	{
		CHECK(!memcmp(ram + game_id, message, strlen(message) + 1),
			"Six-character game id was not accepted");
		CHECK(cbm_open_calls == 6, "Valid game id did not initialize metadata");
	}
	else
	{
		CHECK(!memcmp(ram + game_id, previous_id, sizeof(previous_id)),
			"Overlength game-id message overwrote the published id");
		CHECK(cbm_open_calls == 0,
			"Overlength game-id message initialized metadata before rejection");
	}
}

static void run_bounded_cases(void)
{
	prepare();
	put_bank_byte(STRING_BANK, 0xBFFC, 'A');
	put_bank_byte(STRING_BANK, 0xBFFD, 'B');
	put_bank_byte(STRING_BANK, 0xBFFE, 'C');
	put_bank_byte(STRING_BANK, 0xBFFF, 'D');
	run_string_length(0xBFFC, STRING_BANK, 32, true);
	check_result(4, false);
	CHECK(bank_window_reads == 4, "Unterminated bank-window string read outside its four-byte extent");

	put_bank_byte(STRING_BANK, 0xBFFC, 0);
	run_string_length(0xBFFC, STRING_BANK, 8, false);
	check_result(0, true);
	CHECK(bank_window_reads == 1, "Empty banked string read beyond its terminator");

	put_bank_byte(STRING_BANK, 0xBFFC, 'Q');
	put_bank_byte(STRING_BANK, 0xBFFD, 0);
	run_string_length(0xBFFC, STRING_BANK, 2, true);
	check_result(1, true);
	CHECK(bank_window_reads == 2, "One-character banked string read beyond its terminator");

	put_bank_byte(STRING_BANK, 0xBFFC, 'Q');
	put_bank_byte(STRING_BANK, 0xBFFD, 0);
	run_string_length(0xBFFC, STRING_BANK, 0, false);
	check_result(0, false);
	CHECK(bank_window_reads == 0, "Zero-limit scan read source data");

	put_bank_byte(STRING_BANK, 0xBFFC, 0);
	run_string_length(0xBFFC, STRING_BANK, 1, true);
	check_result(0, true);
	CHECK(bank_window_reads == 1, "One-byte limit did not inspect its possible terminator");

	put_bank_byte(STRING_BANK, 0xBFFC, 'X');
	put_bank_byte(STRING_BANK, 0xBFFD, 0);
	run_string_length(0xBFFC, STRING_BANK, 1, false);
	check_result(1, false);
	CHECK(bank_window_reads == 1, "One-byte limit read beyond its scan limit");

	put_bank_byte(STRING_BANK, 0xBFFF, 0);
	run_string_length(0xBFFF, STRING_BANK, 3, true);
	check_result(0, true);
	CHECK(bank_window_reads == 1, "Final bank byte terminator was not handled in bounds");

	put_bank_byte(STRING_BANK, 0xBFFF, 'Z');
	run_string_length(0xBFFF, STRING_BANK, 3, false);
	check_result(1, false);
	CHECK(bank_window_reads == 1, "Final bank byte scan crossed into $C000");

	memcpy(ram + 0x8800, "resident", 9);
	run_string_length(0x8800, 0x24, 16, false);
	check_result(8, true);
	CHECK(bank_window_reads == 0, "Resident string was read through the bank window");

	put_bank_byte(STRING_BANK, 0xBFF8, 'A');
	put_bank_byte(STRING_BANK, 0xBFF9, 0);
	put_bank_byte(0x24, 0xBFF8, 'B');
	put_bank_byte(0x24, 0xBFF9, 'C');
	put_bank_byte(0x24, 0xBFFA, 0);
	run_string_length(0xBFF8, STRING_BANK, 8, true);
	check_result(1, true);
	run_string_length(0xBFF8, 0x24, 8, false);
	check_result(2, true);

	for (unsigned length = 255; length <= 257; ++length)
	{
		for (unsigned i = 0; i < length; ++i) put_bank_byte(STRING_BANK, (uint16_t)(0xA100 + i), 'L');
		put_bank_byte(STRING_BANK, (uint16_t)(0xA100 + length), 0);
		run_string_length(0xA100, STRING_BANK, 300, (length & 1) != 0);
		check_result((uint16_t)length, true);
		CHECK(bank_window_reads == length + 1, "Wide string length did not scan through its terminator");
	}
}

static void run_game_id_cases(void)
{
	run_game_id_message("SIXCHR", true);
	run_game_id_message("TOOLONGID", false);
}

#endif
