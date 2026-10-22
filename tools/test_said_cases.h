#ifndef TEST_SAID_CASES_H
#define TEST_SAID_CASES_H

#define SAID_LOGIC_BANK 0x24u
#define SAID_LOGIC_ADDRESS 0xA100u
#define SAID_LOGIC_OFFSET (SAID_LOGIC_ADDRESS - BANK_START)
#define SAID_CODE_FIXTURE_SIZE 700u
#define SAID_INPUT_FLAG_ADDRESS (0x400u + 257u + 2u)
#define SAID_HAD_MATCH_FLAG_ADDRESS (0x400u + 257u + 4u)

static uint16_t said_code_cursor(void)
{
	return (uint16_t)(get_word(0xA9) + get_word(symbol("cwCurrentCode")));
}

static void check_said_case(bool condition, const char *case_name, const char *message)
{
	if (!condition)
		fprintf(stderr, "Said case %s failed at cursor $%04X: ",
			case_name, said_code_cursor());
	CHECK(condition, message);
}

static void run_said_case_with_state(const uint16_t *expected_words, unsigned expected_count,
	const uint16_t *input_words, unsigned input_count, bool expect_match,
	bool add_rest_regression_recovery, bool input_flag, bool had_match_flag,
	bool expect_no_input_reads, bool exercise_match_lifecycle, const char *case_name)
{
	uint8_t said_logic[3 + 2 * 255 + 4];
	unsigned code_size = 0;
	unsigned i;
	uint16_t code_cursor;

	CHECK(expected_count <= 255, "Said fixture exceeds its encoded byte-sized word count");
	CHECK(input_count <= 10, "Said fixture exceeds the production parser's ten-word buffer");
	prepare();
	memset(bank_ram[SAID_LOGIC_BANK] + SAID_LOGIC_OFFSET, 0xFC, SAID_CODE_FIXTURE_SIZE);
	said_logic[code_size++] = 0xFF;
	said_logic[code_size++] = 0x0E;
	said_logic[code_size++] = (uint8_t)expected_count;
	for (i = 0; i < expected_count; ++i)
	{
		said_logic[code_size++] = (uint8_t)expected_words[i];
		said_logic[code_size++] = (uint8_t)(expected_words[i] >> 8);
	}
	said_logic[code_size++] = 0xFF;
	said_logic[code_size++] = 0;
	said_logic[code_size++] = 0;
	said_logic[code_size++] = 0;
	memcpy(bank_ram[SAID_LOGIC_BANK] + SAID_LOGIC_OFFSET, said_logic, code_size);
	if (add_rest_regression_recovery)
	{
		/* Let the original REST_OF_LINE cursor bug close the IF and reach its assertion. */
		bank_ram[SAID_LOGIC_BANK][SAID_LOGIC_OFFSET + 260] = 0xFF;
		bank_ram[SAID_LOGIC_BANK][SAID_LOGIC_OFFSET + 261] = 0;
		bank_ram[SAID_LOGIC_BANK][SAID_LOGIC_OFFSET + 262] = 0;
	}
	put_word(symbol("_testSaidCodeSize"), (uint16_t)code_size);
	put_word(symbol("_numInputWords"), (uint16_t)input_count);
	for (i = 0; i < input_count; ++i)
		put_word((uint16_t)(symbol("_inputWords") + i * 2), input_words[i]);
	ram[SAID_INPUT_FLAG_ADDRESS] = input_flag ? 1 : 0;
	ram[SAID_HAD_MATCH_FLAG_ADDRESS] = had_match_flag ? 1 : 0;
	said_input_read_start = symbol("_inputWords");
	said_input_readable_bytes = input_count * 2;
	said_input_reads = 0;
	said_input_overread_reads = 0;
	said_input_flag_address = SAID_INPUT_FLAG_ADDRESS;
	said_input_flag_writes = 0;
	said_had_match_flag_address = SAID_HAD_MATCH_FLAG_ADDRESS;
	said_had_match_flag_writes = 0;
	said_return_true_pc = symbol("returnFromOpCodeTrue");
	said_return_false_pc = symbol("returnFromOpCodeFalse");
	said_return_true_count = 0;
	said_return_false_count = 0;
	said_input_read_monitor = true;
	said_flag_write_monitor = true;
	said_return_monitor = true;
	call_target_caller("_testRunSaid");
	if (exercise_match_lifecycle)
	{
		check_said_case(ram[SAID_HAD_MATCH_FLAG_ADDRESS] == 1 &&
			said_return_true_count == 1 && said_return_false_count == 0 &&
			said_input_reads == input_count * 2 && said_had_match_flag_writes == 1 &&
			said_code_cursor() == SAID_LOGIC_ADDRESS + code_size,
			case_name, "Said did not record its initial match in the lifecycle case");

		said_input_reads = 0;
		said_input_overread_reads = 0;
		said_input_flag_writes = 0;
		said_had_match_flag_writes = 0;
		said_return_true_count = 0;
		said_return_false_count = 0;
		call_target_caller("_testRunSaid");
		check_said_case(ram[SAID_HAD_MATCH_FLAG_ADDRESS] == 1 &&
			said_return_true_count == 0 && said_return_false_count == 1 &&
			said_input_reads == 0 && said_had_match_flag_writes == 0 &&
			said_code_cursor() == SAID_LOGIC_ADDRESS + code_size,
			case_name, "Said matched again before HADMATCH was reset");

		ram[SAID_HAD_MATCH_FLAG_ADDRESS] = 0;
		said_input_reads = 0;
		said_input_overread_reads = 0;
		said_input_flag_writes = 0;
		said_had_match_flag_writes = 0;
		said_return_true_count = 0;
		said_return_false_count = 0;
		call_target_caller("_testRunSaid");
	}
	said_input_read_monitor = false;
	said_flag_write_monitor = false;
	said_return_monitor = false;

	code_cursor = (uint16_t)(get_word(0xA9) + get_word(symbol("cwCurrentCode")));
	check_said_case((ram[SAID_HAD_MATCH_FLAG_ADDRESS] != 0) ==
		(expect_match || had_match_flag), case_name,
		expect_match ? "Said rejected matching recognized input" :
		"Said changed HADMATCH on a failed match");
	check_said_case(said_return_true_count == (expect_match ? 1u : 0u) &&
		said_return_false_count == (expect_match ? 0u : 1u), case_name,
		"Said did not return through the expected production opcode branch exactly once");
	check_said_case((ram[SAID_INPUT_FLAG_ADDRESS] != 0) == input_flag, case_name,
		"Said changed the INPUT flag");
	check_said_case(said_input_flag_writes == 0, case_name, "Said wrote the INPUT flag");
	check_said_case(said_had_match_flag_writes == (expect_match ? 1u : 0u), case_name,
		"Said did not write HADMATCH exactly once on success and never on failure");
	check_said_case(said_input_overread_reads == 0, case_name,
		"Said read beyond the recognized input word count");
	if (expect_no_input_reads)
		check_said_case(said_input_reads == 0, case_name,
			"Said read recognized input while its flag gate was closed");
	check_said_case(code_cursor == SAID_LOGIC_ADDRESS + code_size, case_name,
		"Said did not leave the interpreter at the next bytecode");
}

static void run_said_case(const uint16_t *expected_words, unsigned expected_count,
	const uint16_t *input_words, unsigned input_count, bool expect_match,
	bool add_rest_regression_recovery, const char *case_name)
{
	run_said_case_with_state(expected_words, expected_count, input_words, input_count,
		expect_match, add_rest_regression_recovery, true, false, false, false, case_name);
}

static void run_said_rest_of_line_regression(void)
{
	static const uint16_t expected_words[] = { 9999 };
	static const uint16_t input_words[] = { 0x0412 };
	run_said_case(expected_words, 1, input_words, 1, true, true, "rest_only_regression");
}

static void run_said_cases(void)
{
	static const uint16_t exact_expected[] = { 0x0412, 0x0534 };
	static const uint16_t exact_input[] = { 0x0412, 0x0534 };
	static const uint16_t mismatch_expected[] = { 0x0412, 0x0535, 0x0678 };
	static const uint16_t mismatch_input[] = { 0x0412, 0x0534 };
	static const uint16_t extra_input[] = { 0x0412, 0x0534 };
	static const uint16_t any_expected[] = { 1, 0x0534 };
	static const uint16_t any_input[] = { 0x0412, 0x0534 };
	static const uint16_t rest_expected[] = { 0x0412, 9999, 0x1234, 1 };
	static const uint16_t rest_input[] = { 0x0412, 0x0534, 0x0678 };
	static const uint16_t rest_empty_tail_expected[] = { 0x0412, 9999, 0x1234, 0x5678 };
	static const uint16_t rest_empty_tail_input[] = { 0x0412 };
	static const uint16_t rest_prefix_mismatch_input[] = { 0x0413 };
	static const uint16_t ordinary_low_one_expected[] = { 0x0101 };
	static const uint16_t ordinary_low_one_input[] = { 0x0101 };
	static const uint16_t ordinary_low_one_high_mismatch[] = { 0x0201 };
	static const uint16_t one_expected[] = { 0x0412 };
	static const uint16_t one_input[] = { 0x0412 };
	uint16_t many_expected[255];
	uint16_t ten_input[10];
	unsigned i;

	run_said_rest_of_line_regression();
	run_said_case(rest_empty_tail_expected, 4, rest_empty_tail_input, 1, true, false,
		"rest_after_recognized_prefix_empty_tail");
	run_said_case(exact_expected, 2, exact_input, 2, true, false, "exact_match");
	run_said_case(mismatch_expected, 3, mismatch_input, 2, false, false, "word_mismatch");
	run_said_case(extra_input, 1, extra_input, 2, false, false, "extra_input");
	run_said_case(any_expected, 2, any_input, 2, true, false, "any_word");
	run_said_case(ordinary_low_one_expected, 1, ordinary_low_one_input, 1, true, false,
		"ordinary_word_low_one_high_nonzero");
	run_said_case(ordinary_low_one_expected, 1, ordinary_low_one_high_mismatch, 1, false, false,
		"ordinary_word_low_one_high_mismatch");
	run_said_case(rest_empty_tail_expected, 4, rest_prefix_mismatch_input, 1, false, false,
		"rest_after_mismatched_prefix");
	run_said_case(rest_expected, 4, rest_input, 3, true, false, "rest_of_line_nonempty_tail");
	run_said_case(NULL, 0, one_input, 1, false, false, "no_expected_words");
	run_said_case(one_expected, 1, NULL, 0, false, false, "no_input_words");
	run_said_case(NULL, 0, NULL, 0, false, false, "empty_expected_and_input");
	run_said_case(exact_expected, 2, one_input, 1, false, false, "input_exhausted");

	for (i = 0; i < 10; ++i)
	{
		ten_input[i] = (uint16_t)(0x0300 + i);
		many_expected[i] = ten_input[i];
	}
	for (i = 10; i < 255; ++i) many_expected[i] = 0x1234;
	run_said_case_with_state(many_expected, 255, ten_input, 10, false, false,
		false, false, true, false, "input_flag_off_long_arguments");
	run_said_case_with_state(many_expected, 255, ten_input, 10, false, false,
		true, true, true, false, "hadmatch_flag_set_long_arguments");
	run_said_case_with_state(one_expected, 1, one_input, 1, true, false,
		true, false, false, true, "hadmatch_match_reset_lifecycle");
	run_said_case(many_expected, 127, ten_input, 10, false, false, "long_arguments_127");
	run_said_case(many_expected, 128, ten_input, 10, false, false, "long_arguments_128");
	run_said_case(many_expected, 255, ten_input, 10, false, false, "long_arguments_255");
	run_said_case(many_expected, 255, NULL, 0, false, false, "long_arguments_255_no_input");
	/* TEST-image span: original 485 bytes, corrected matcher 475 before sharing success. */
	unsigned matcher_bytes = symbol("b1Compare_stringsCCall") - symbol("bFSaid");
	fprintf(stderr, "Production Said matcher: %u test-image bytes (baselines 485/475; require <475)\n",
		matcher_bytes);
	CHECK(matcher_bytes < 475, "Said matcher retained duplicate success-flag publication");
	puts("Production Said opcode passes exact, wildcard, REST_OF_LINE, flag lifecycle, exhaustion and long-argument cases");
}

#endif
