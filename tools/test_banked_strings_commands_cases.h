#ifndef TEST_BANKED_STRINGS_COMMANDS_CASES_H
#define TEST_BANKED_STRINGS_COMMANDS_CASES_H

static void run_get_num_allocated_bank(void)
{
	static const uint8_t keys[] = { '7', 0x0D };
	prepare();
	memset(bank_ram[7] + symbol("_b7CurrentInputStr") - BANK_START, 0, 41);
	memset(bank_ram[7] + symbol("_b7Temp") - BANK_START, 0, 256);
	ram[symbol("codeWindow")] = 1;
	ram[symbol("codeWindow") + 1] = 7;
	ram[0x0807] = 0xA5;
	bank_ram[0x24][0x100] = '?';
	bank_ram[0x24][0x101] = 0;
	memcpy(getin_keys, keys, sizeof(keys));
	getin_count = sizeof(keys);
	getin_index = 0;
	getin_enabled = true;
	call_target_caller("_testRunGetNum");
	getin_enabled = false;
	CHECK(getin_index == sizeof(keys), "get.num did not consume the complete numeric input");
	CHECK(ram[0x0807] == 7,
		"get.num parsed its allocation while the command code bank was selected");
	CHECK(bank_ram[0x13][0] == '7' && bank_ram[0x13][1] == 0,
		"get.num did not retain its input in the allocator-selected bank");
}

static void run_commands_cases(void)
{
	run_get_num_allocated_bank();
}

#endif
