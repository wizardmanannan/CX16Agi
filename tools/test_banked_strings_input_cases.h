#ifndef TEST_BANKED_STRINGS_INPUT_CASES_H
#define TEST_BANKED_STRINGS_INPUT_CASES_H

static bool input_case_failed;

static void input_expect(bool condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "%s\n", message);
		input_case_failed = true;
	}
}

static void run_get_string_return_bank(void)
{
	uint16_t current_input = symbol("_b7CurrentInputStr");
	uint16_t input_offset = (uint16_t)(current_input - BANK_START);
	static const uint8_t keys[] = { 0, '7', 0x0D };
	prepare();
	memset(bank_ram[7] + input_offset, 0, 41);
	memset(bank_ram[7] + symbol("_b7Temp") - BANK_START, 0, 256);
	ram[symbol("_strPos")] = 0;
	put_word(symbol("_testInputPrompt"), 0xA100);
	ram[symbol("_testInputPromptBank")] = 0x24;
	put_word(symbol("_testInputDestination"), current_input);
	ram[symbol("_testInputDestinationBank")] = 0x35;
	put_word(symbol("_testInputLimit"), 3);
	bank_ram[0x35][input_offset] = 'S';
	bank_ram[0x35][input_offset + 1] = 'S';
	bank_ram[0x35][input_offset + 2] = 0;
	memcpy(getin_keys, keys, sizeof(keys));
	getin_count = sizeof(keys);
	getin_index = 0;
	getin_enabled = true;
	call_target_caller("_testRunGetString");
	getin_enabled = false;
	CHECK(getin_index == sizeof(keys), "b7GetString exited when GETIN briefly returned zero");
	CHECK(bank_ram[0x35][input_offset] == '7' && bank_ram[0x35][input_offset + 1] == 0,
		"b7GetString did not copy its result to the requested bank");
	CHECK(bank_ram[7][input_offset] == '7' && bank_ram[7][input_offset + 1] == 0,
		"b7GetString changed its parser-bank input while copying to another bank");
}

static void run_get_string_stale_limit(void)
{
	uint16_t current_input = symbol("_b7CurrentInputStr");
	uint16_t input_offset = (uint16_t)(current_input - BANK_START);
	static const uint8_t keys[] = { 'Z', 'Q', 0x0D };
	prepare();
	memset(bank_ram[7] + input_offset, 0, 41);
	memcpy(bank_ram[7] + input_offset, "ABCDE", 6);
	memset(bank_ram[7] + symbol("_b7Temp") - BANK_START, 0, 256);
	ram[symbol("_strPos")] = 5;
	put_word(symbol("_testInputPrompt"), 0xA100);
	ram[symbol("_testInputPromptBank")] = 0x24;
	put_word(symbol("_testInputDestination"), current_input);
	ram[symbol("_testInputDestinationBank")] = 0x35;
	put_word(symbol("_testInputLimit"), 1);
	bank_ram[0x35][input_offset] = 'S';
	bank_ram[0x35][input_offset + 1] = 'S';
	bank_ram[0x35][input_offset + 2] = 0;
	memcpy(getin_keys, keys, sizeof(keys));
	getin_count = sizeof(keys);
	getin_index = 0;
	getin_enabled = true;
	call_target_caller("_testRunGetString");
	getin_enabled = false;
	CHECK(getin_index == sizeof(keys), "b7GetString did not consume the complete bounded input");
	CHECK(bank_ram[0x35][input_offset] == 'Z' && bank_ram[0x35][input_offset + 1] == 0,
		"b7GetString retained a stale input position beyond its requested length");
	CHECK(bank_ram[7][input_offset] == 'Z' && bank_ram[7][input_offset + 1] == 0,
		"b7GetString input did not respect its one-character limit");
}

static void call_input_line_handler(void)
{
	uint8_t previous_bank = ram[0];
	unsigned steps = 0;
	regs.sp = 0x1FD;
	regs.a = regs.x = regs.y = 0;
	regs.status = FLAG_CONSTANT;
	ram[0x1FE] = 0xFF;
	ram[0x1FF] = 0x01;
	ram[0] = PARSER_BANK;
	regs.pc = symbol("b7HandleInputLine");
	while (regs.pc != RETURN_PC)
	{
		CHECK(++steps < 10000, "Production input-line handler did not return");
		step6502();
	}
	CHECK(regs.sp == 0x1FF, "Production input-line handler changed hardware stack");
	CHECK(ram[0] == PARSER_BANK && ram[1] == 0x53,
		"Production input-line handler changed its RAM or ROM bank");
	CHECK(get_word(symbol("c_sp")) == 0x9000,
		"Production input-line handler changed the C stack");
	CHECK(!(regs.status & FLAG_INTERRUPT),
		"Production input-line handler changed the caller interrupt flag");
	ram[0] = previous_bank;
}

static void run_get_string_clears_visible_input(void)
{
	uint16_t current_input = symbol("_b7CurrentInputStr");
	uint16_t input_offset = (uint16_t)(current_input - BANK_START);
	uint16_t dirty = symbol("_inputLineDirty");
	static const uint8_t keys[] = { 0x0D };
	uint8_t dirty_after_get_string;
	unsigned cell;
	prepare();
	memset(bank_ram[PARSER_BANK] + input_offset, 0, 41);
	memcpy(bank_ram[PARSER_BANK] + input_offset, "STALE", 6);
	memset(bank_ram[PARSER_BANK] + symbol("_b7Temp") - BANK_START, 0, 256);
	ram[symbol("_strPos")] = 5;
	ram[symbol("_inputLineDisplayed")] = 1;
	ram[dirty] = 0;
	put_word(symbol("_testInputPrompt"), 0xA100);
	ram[symbol("_testInputPromptBank")] = 0x24;
	put_word(symbol("_testInputDestination"), current_input);
	ram[symbol("_testInputDestinationBank")] = PARSER_BANK;
	put_word(symbol("_testInputLimit"), 40);
	bank_ram[0x24][0x100] = 0;
	for (cell = 0; cell < 41; ++cell)
	{
		vram[INPUT_STRING_ADDRESS + cell * 2] = cell == 0 ? '>' : 'S';
		vram[INPUT_STRING_ADDRESS + cell * 2 + 1] = 0xA5;
	}
	vram[INPUT_STRING_ADDRESS + 82] = 0xD1;
	vram[INPUT_STRING_ADDRESS + 83] = 0xD2;
	memcpy(getin_keys, keys, sizeof(keys));
	getin_count = sizeof(keys);
	getin_index = 0;
	getin_enabled = true;
	call_target_caller("_testRunGetString");
	getin_enabled = false;
	CHECK(getin_index == sizeof(keys), "b7GetString did not consume Enter");
	CHECK(bank_ram[PARSER_BANK][input_offset] == 0,
		"Enter did not clear the stale parser input buffer");
	dirty_after_get_string = ram[dirty];
	call_input_line_handler();
	CHECK(vera_data0_writes == 43,
		"Production input-line handler did not redraw the cleared input after Enter");
	CHECK(dirty_after_get_string == 1,
		"b7GetString cleared the visible input without invalidating its row");
	CHECK(vram[INPUT_STRING_ADDRESS] == '>' &&
		vram[INPUT_STRING_ADDRESS + 2] == 0x5F,
		"Production input-line handler did not draw the prompt and cursor after clearing input");
	for (cell = 2; cell < 41; ++cell)
		CHECK(vram[INPUT_STRING_ADDRESS + cell * 2] == 0x80,
			"Production input-line handler left stale text visible after Enter");
	for (cell = 0; cell < 41; ++cell)
		CHECK(vram[INPUT_STRING_ADDRESS + cell * 2 + 1] == 0xA5,
			"Production input-line handler changed an input-row attribute");
	CHECK(vram[INPUT_STRING_ADDRESS + 82] == 0xD1 &&
		vram[INPUT_STRING_ADDRESS + 83] == 0xD2,
		"Production input-line handler overwrote the adjacent tile");
	CHECK(ram[dirty] == 0, "Production input-line handler did not consume the dirty flag");
}

static void run_get_internal_string_capacity(void)
{
	uint16_t strings = symbol("_string");
	uint16_t string_offset = (uint16_t)(strings - BANK_START);
	uint8_t keys[41];
	unsigned i;
	prepare();
	memset(bank_ram[PARSER_BANK] + string_offset, 0xA5, 80);
	bank_ram[PARSER_BANK][string_offset + 40] = 'G';
	put_word(symbol("_testInputPrompt"), 0xA100);
	ram[symbol("_testInputPromptBank")] = 0x24;
	put_word(symbol("_testInputLimit"), 40);
	bank_ram[0x24][0x100] = 0;
	for (i = 0; i < 40; ++i) keys[i] = 'A';
	keys[40] = 0x0D;
	memcpy(getin_keys, keys, sizeof(keys));
	getin_count = sizeof(keys);
	getin_index = 0;
	getin_enabled = true;
	call_target_caller("_testRunGetInternalString");
	getin_enabled = false;
	input_expect(getin_index == sizeof(keys),
		"get.string did not consume all 40 input characters and Enter");
	input_expect(memcmp(bank_ram[PARSER_BANK] + string_offset,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 39) == 0 &&
		bank_ram[PARSER_BANK][string_offset + 39] == 0,
		"get.string did not retain the maximum 39 characters and terminator in its internal string row");
	input_expect(bank_ram[PARSER_BANK][string_offset + 40] == 'G',
		"get.string overwrote the next internal string row");
}

static void run_get_string_preserves_prompt_scratch(void)
{
	uint16_t current_input = symbol("_b7CurrentInputStr");
	uint16_t input_offset = (uint16_t)(current_input - BANK_START);
	uint16_t prompt_scratch = symbol("_b7Temp");
	uint16_t prompt_offset = (uint16_t)(prompt_scratch - BANK_START);
	uint8_t keys[41];
	unsigned i;
	prepare();
	memset(bank_ram[PARSER_BANK] + input_offset, 0, 41);
	memset(bank_ram[PARSER_BANK] + prompt_offset, 0, 256);
	memset(bank_ram[PARSER_BANK] + prompt_offset, 'P', 39);
	bank_ram[PARSER_BANK][prompt_offset + 39] = 0;
	put_word(symbol("_testInputPrompt"), 0xA100);
	ram[symbol("_testInputPromptBank")] = 0x24;
	put_word(symbol("_testInputDestination"), current_input);
	ram[symbol("_testInputDestinationBank")] = PARSER_BANK;
	put_word(symbol("_testInputLimit"), 40);
	bank_ram[0x24][0x100] = 0;
	for (i = 0; i < 40; ++i) keys[i] = 'A';
	keys[40] = 0x0D;
	memcpy(getin_keys, keys, sizeof(keys));
	getin_count = sizeof(keys);
	getin_index = 0;
	getin_enabled = true;
	call_target_caller("_testRunGetString");
	getin_enabled = false;
	input_expect(getin_index == sizeof(keys),
		"b7GetString did not consume all 40 input characters and Enter");
	input_expect(memcmp(bank_ram[PARSER_BANK] + input_offset,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 40) == 0 &&
		bank_ram[PARSER_BANK][input_offset + 40] == 0,
		"b7GetString did not retain all 40 characters in its larger destination");
	input_expect(bank_ram[PARSER_BANK][prompt_offset] == 'P',
		"b7GetString's unused display formatting overwrote prompt scratch");
}

static void run_input_cases(void)
{
	input_case_failed = false;
	run_get_string_return_bank();
	run_get_string_stale_limit();
	run_get_string_clears_visible_input();
	run_get_internal_string_capacity();
	run_get_string_preserves_prompt_scratch();
	CHECK(!input_case_failed, "Production input destination or prompt-scratch boundary regression detected");
}

#endif
