#ifndef TEST_OBJECT_COMMANDS_CASES_H
#define TEST_OBJECT_COMMANDS_CASES_H

#define OBJECT_COMMAND_SENTINEL_ROOM 0x66u
#define OBJECT_COMMAND_SENTINEL_NAME 0xBEEFu

static unsigned object_command_objects;
static uint8_t object_command_last;

static void set_object_target_byte(const char *name, uint8_t value)
{
	ram[symbol(name)] = value;
}

static uint8_t get_object_target_byte(const char *name)
{
	return ram[symbol(name)];
}

static void set_object_target_word(const char *name, uint16_t value)
{
	put_word(symbol(name), value);
}

static uint16_t get_object_target_word(const char *name)
{
	return get_word(symbol(name));
}

static void set_object_var(uint8_t index, uint8_t value)
{
	ram[symbol("_testObjectVars") + index] = value;
}

static uint8_t get_object_var(uint8_t index)
{
	return ram[symbol("_testObjectVars") + index];
}

static void check_object_name_cache_canary(void)
{
	uint16_t address = symbol("_bDObjData");
	unsigned index;
	CHECK(address >= BANK_START && address + 8u <= BANK_END,
		"Production object name cache is outside BANKRAM0D");
	for (index = 0; index < 8; ++index)
		CHECK(bank_ram[0x0D][address - BANK_START + index] == 0xD0 + index,
			"Invalid object access wrote into the production name cache");
}

static void call_object_target(const char *name)
{
	call_target_caller(name);
	CHECK((regs.status & FLAG_INTERRUPT) == 0,
		"Object target caller left interrupts disabled");
}

static void set_object_code(const uint8_t *operands, unsigned count)
{
	CHECK(count <= 2, "Object command fixture has too many operands");
	memset(ram + symbol("codeWindow"), 0, 50);
	memcpy(ram + symbol("codeWindow"), operands, count);
	put_word(symbol("cwCurrentCode"), 0);
}

static void run_object_command(const char *caller, const uint8_t *operands, unsigned count)
{
	set_object_code(operands, count);
	call_object_target(caller);
	CHECK(get_word(symbol("cwCurrentCode")) == count,
		"Object command did not consume all encoded operands");
}

static void reset_object_fixture(unsigned objectCount)
{
	prepare();
	set_object_target_word("_testObjectCount", (uint16_t)objectCount);
	call_object_target("_testObjectCommandsReset");
	call_object_target("_testObjectCommandsReadCount");
	CHECK(get_object_target_word("_testObjectObservedCount") == objectCount,
		"Object fixture did not initialize the production object count");
}

static void select_object(uint8_t objectNumber)
{
	set_object_target_byte("_testObjectIndex", objectNumber);
}

static void inspect_object(uint8_t objectNumber, uint8_t expectedRoom, uint16_t expectedName)
{
	select_object(objectNumber);
	call_object_target("_testObjectCommandsInspect");
	CHECK(get_object_target_byte("_testObjectObservedRoom") == expectedRoom,
		"Production object table contains an unexpected room number");
	CHECK(get_object_target_word("_testObjectObservedName") == expectedName,
		"Production object table contains an unexpected name pointer");
}

static void read_object(uint8_t objectNumber, uint8_t seedRoom, uint16_t seedName,
	uint8_t expectedResult, uint8_t expectedRoom, uint16_t expectedName)
{
	select_object(objectNumber);
	set_object_target_byte("_testObjectSeedRoom", seedRoom);
	set_object_target_word("_testObjectSeedName", seedName);
	call_object_target("_testObjectCommandsRead");
	CHECK(get_object_target_byte("_testObjectResult") == (uint8_t)expectedResult,
		"Production object getter returned the wrong validity result");
	CHECK(get_object_target_byte("_testObjectObservedRoom") == expectedRoom,
		"Production object getter changed or returned the wrong room number");
	CHECK(get_object_target_word("_testObjectObservedName") == expectedName,
		"Production object getter changed or returned the wrong name pointer");
}

static void write_object(uint8_t objectNumber, uint8_t room, uint16_t name,
	uint8_t expectedResult)
{
	select_object(objectNumber);
	set_object_target_byte("_testObjectRoom", room);
	set_object_target_word("_testObjectName", name);
	call_object_target("_testObjectCommandsWrite");
	CHECK(get_object_target_byte("_testObjectResult") == (uint8_t)expectedResult,
		"Production object setter returned the wrong validity result");
}

static void run_valid_object_commands(void)
{
	const uint8_t get_operand[] = { object_command_last };
	const uint8_t get_var_operand[] = { 12 };
	const uint8_t drop_operand[] = { object_command_last };
	const uint8_t put_operand[] = { object_command_last, 0x47 };
	const uint8_t put_var_operand[] = { object_command_last, 19 };
	const uint8_t get_room_var_operand[] = { 20, 21 };
	const uint8_t has_operand[] = { object_command_last };
	const uint8_t in_room_operand[] = { object_command_last, 22 };

	reset_object_fixture(object_command_objects);
	read_object(object_command_last, 0xA5, 0xDEAD, 1,
		OBJECT_COMMAND_SENTINEL_ROOM, OBJECT_COMMAND_SENTINEL_NAME);
	write_object(object_command_last, 255, 0x4567, 1);
	inspect_object(object_command_last, 255, 0x4567);

	write_object(object_command_last, 0x11, 0x4567, 1);
	run_object_command("_testObjectCommandsRunB3Get", get_operand, sizeof(get_operand));
	inspect_object(object_command_last, 255, 0x4567);

	write_object(object_command_last, 0x22, 0x4567, 1);
	set_object_var(12, object_command_last);
	run_object_command("_testObjectCommandsRunB3GetVar", get_var_operand,
		sizeof(get_var_operand));
	inspect_object(object_command_last, 255, 0x4567);

	run_object_command("_testObjectCommandsRunB3Drop", drop_operand, sizeof(drop_operand));
	inspect_object(object_command_last, 0, 0x4567);

	run_object_command("_testObjectCommandsRunB3Put", put_operand, sizeof(put_operand));
	inspect_object(object_command_last, 0x47, 0x4567);

	set_object_var(19, 0x68);
	run_object_command("_testObjectCommandsRunB3PutVar", put_var_operand,
		sizeof(put_var_operand));
	inspect_object(object_command_last, 0x68, 0x4567);

	set_object_var(20, object_command_last);
	set_object_var(21, 0xA7);
	run_object_command("_testObjectCommandsRunB3GetRoomVar", get_room_var_operand,
		sizeof(get_room_var_operand));
	CHECK(get_object_var(21) == 0x68,
		"get.room.v did not write the last object's room to the requested variable");

	run_object_command("_testObjectCommandsRunB1Has", has_operand, sizeof(has_operand));
	CHECK(get_object_target_byte("_testObjectResult") == 0,
		"has returned true for the last object outside the inventory");
	set_object_target_byte("_testObjectRoom", 255);
	write_object(object_command_last, 255, 0x4567, 1);
	run_object_command("_testObjectCommandsRunB1Has", has_operand, sizeof(has_operand));
	CHECK(get_object_target_byte("_testObjectResult") == 1,
		"has did not recognize the last object in inventory");

	set_object_var(22, 255);
	run_object_command("_testObjectCommandsRunB1ObjInRoom", in_room_operand,
		sizeof(in_room_operand));
	CHECK(get_object_target_byte("_testObjectResult") == 1,
		"obj.in.room did not match the last object's room");
	set_object_var(22, 254);
	run_object_command("_testObjectCommandsRunB1ObjInRoom", in_room_operand,
		sizeof(in_room_operand));
	CHECK(get_object_target_byte("_testObjectResult") == 0,
		"obj.in.room matched a different room");
}

static void run_invalid_object_commands(uint8_t invalidObject)
{
	const uint8_t get_operand[] = { invalidObject };
	const uint8_t get_var_operand[] = { 30 };
	const uint8_t drop_operand[] = { invalidObject };
	const uint8_t put_operand[] = { invalidObject, 0x45 };
	const uint8_t put_var_operand[] = { invalidObject, 31 };
	const uint8_t get_room_var_operand[] = { 32, 33 };
	const uint8_t has_operand[] = { invalidObject };
	const uint8_t in_room_operand[] = { invalidObject, 34 };

	read_object(invalidObject, 0xA5, 0xDEAD, 0, 0xA5, 0xDEAD);
	write_object(invalidObject, 0x91, 0x1234, 0);

	run_object_command("_testObjectCommandsRunB1Has", has_operand, sizeof(has_operand));
	CHECK(get_object_target_byte("_testObjectResult") == 0,
		"has accepted an invalid object operand");
	run_object_command("_testObjectCommandsRunB1ObjInRoom", in_room_operand,
		sizeof(in_room_operand));
	CHECK(get_object_target_byte("_testObjectResult") == 0,
		"obj.in.room accepted an invalid object operand");
	run_object_command("_testObjectCommandsRunB3Get", get_operand, sizeof(get_operand));
	set_object_var(30, invalidObject);
	run_object_command("_testObjectCommandsRunB3GetVar", get_var_operand,
		sizeof(get_var_operand));
	run_object_command("_testObjectCommandsRunB3Drop", drop_operand, sizeof(drop_operand));
	run_object_command("_testObjectCommandsRunB3Put", put_operand, sizeof(put_operand));
	set_object_var(31, 0x92);
	run_object_command("_testObjectCommandsRunB3PutVar", put_var_operand,
		sizeof(put_var_operand));
	set_object_var(32, invalidObject);
	set_object_var(33, 0xB7);
	memset(ram + 0x8F00, 0x55, 0x100);
	run_object_command("_testObjectCommandsRunB3GetRoomVar", get_room_var_operand,
		sizeof(get_room_var_operand));
	CHECK(get_object_var(33) == 0xB7,
		"get.room.v changed its destination variable for an invalid object");
	inspect_object(object_command_last, OBJECT_COMMAND_SENTINEL_ROOM,
		OBJECT_COMMAND_SENTINEL_NAME);
	check_object_name_cache_canary();
}

static unsigned seed_object_file(unsigned objectCount)
{
	uint16_t dataAddress = symbol("_bDObjData");
	unsigned cacheSize = get_object_target_word("_testObjectNameCacheSize");
	unsigned headerLength = objectCount * 3u;
	unsigned nameOffset = headerLength + 3u;
	unsigned fileLength = nameOffset + 5u;
	unsigned bankOffset;
	unsigned index;

	CHECK(dataAddress >= BANK_START && dataAddress + cacheSize <= BANK_END,
		"Production object file buffer is outside BANKRAM0D");
	CHECK(fileLength <= cacheSize,
		"Object parser fixture exceeds the production input buffer");
	bankOffset = dataAddress - BANK_START;
	memset(bank_ram[0x0D] + bankOffset, 0xA5, cacheSize);
	bank_ram[0x0D][bankOffset] = (uint8_t)headerLength;
	bank_ram[0x0D][bankOffset + 1] = (uint8_t)(headerLength >> 8);
	for (index = 0; index < objectCount; ++index)
	{
		unsigned record = bankOffset + 3u + index * 3u;
		bank_ram[0x0D][record] = (uint8_t)headerLength;
		bank_ram[0x0D][record + 1] = (uint8_t)(headerLength >> 8);
		bank_ram[0x0D][record + 2] = (uint8_t)index;
	}
	memcpy(bank_ram[0x0D] + bankOffset + nameOffset, "item", 5);
	return fileLength;
}

static void run_object_file_capacity_cases(void)
{
	unsigned capacity = get_object_target_word("_testObjectCapacity");
	unsigned fileLength;
	unsigned nameAddress;

	prepare();
	fileLength = seed_object_file(capacity);
	nameAddress = symbol("_bDObjData") + capacity * 3u + 3u;
	set_object_target_word("_testObjectFileLength", (uint16_t)fileLength);
	call_object_target("_testObjectCommandsParseFile");
	CHECK(get_object_target_byte("_testObjectParseResult"),
		"Object parser rejected exactly MAX_OBJECTS records");
	CHECK(get_object_target_word("_testObjectParsedCount") == capacity,
		"Object parser returned the wrong maximum object count");
	CHECK(get_object_target_word("_testObjectParsedName") == nameAddress &&
		get_object_target_byte("_testObjectParsedRoom") == (uint8_t)(capacity - 1) &&
		get_object_target_byte("_testObjectParsedObjectNum") == (uint8_t)(capacity - 1) &&
		get_object_target_byte("_testObjectParsedNameLength") == 4,
		"Object parser did not preserve the last valid record and its name");

	fileLength = seed_object_file(capacity + 1u);
	set_object_target_word("_testObjectFileLength", (uint16_t)fileLength);
	call_object_target("_testObjectCommandsParseFile");
	CHECK(!get_object_target_byte("_testObjectParseResult") &&
		get_object_target_word("_testObjectParsedCount") == 0,
		"Object parser accepted a declared object count above MAX_OBJECTS");
	CHECK(get_object_target_word("_testObjectParsedHeader") == (capacity + 1u) * 3u,
		"Rejected object table overwrote its input buffer");

	fileLength = seed_object_file(1);
	{
		unsigned bankOffset = symbol("_bDObjData") - BANK_START;
		bank_ram[0x0D][bankOffset + 3] = 0xFF;
		bank_ram[0x0D][bankOffset + 4] = 0xFF;
	}
	set_object_target_word("_testObjectFileLength", (uint16_t)fileLength);
	call_object_target("_testObjectCommandsParseFile");
	CHECK(!get_object_target_byte("_testObjectParseResult") &&
		get_object_target_word("_testObjectParsedCount") == 0,
		"Object parser accepted a name offset outside its input");

	fileLength = seed_object_file(1);
	{
		unsigned bankOffset = symbol("_bDObjData") - BANK_START;
		memcpy(bank_ram[0x0D] + bankOffset + 6, "abcde", 5);
	}
	set_object_target_word("_testObjectFileLength", (uint16_t)fileLength);
	call_object_target("_testObjectCommandsParseFile");
	CHECK(!get_object_target_byte("_testObjectParseResult") &&
		get_object_target_word("_testObjectParsedCount") == 0,
		"Object parser read a name without a terminator inside its input");
}

static void run_object_reader_capacity_case(void)
{
	unsigned capacity;
	uint8_t *data;
	static const uint8_t short_header[] = { 0, 0 };
	static const uint8_t one_byte[] = { 'A' };

	prepare();
	capacity = get_object_target_word("_testObjectNameCacheSize");
	data = malloc(capacity + 1u);
	CHECK(data, "Cannot allocate OBJECT reader boundary stream");
	memset(data, 'A', capacity + 1u);
	set_object_file_stream(data, capacity, false);
	monitor_object_buffer_writes();
	call_target_caller("_testObjectCommandsReadFile");
	stop_monitoring_object_buffer_writes();
	CHECK(object_buffer_out_of_bounds_writes == 0,
		"OBJECT reader wrote beyond bDObjData at exact capacity");
	CHECK(get_object_target_byte("_testObjectFileLfn") == 2 &&
		get_object_target_word("_testObjectFileLength") == capacity,
		"OBJECT reader rejected an exactly full input buffer");
	CHECK(object_file_close_calls == 1,
		"OBJECT reader did not close an exactly full input file");

	prepare();
	set_object_file_stream(data, capacity + 1u, false);
	monitor_object_buffer_writes();
	call_target_caller("_testObjectCommandsReadFile");
	stop_monitoring_object_buffer_writes();
	CHECK(object_buffer_out_of_bounds_writes == 0,
		"OBJECT reader wrote beyond bDObjData for a capacity-plus-one file");
	CHECK(get_object_target_byte("_testObjectFileLfn") == 0 &&
		get_object_target_word("_testObjectFileLength") == 0xFFFF,
		"OBJECT reader did not publish capacity-plus-one failure");
	CHECK(object_file_close_calls == 1,
		"OBJECT reader did not close a capacity-plus-one input file");

	prepare();
	set_object_file_stream(one_byte, sizeof(one_byte), true);
	monitor_object_buffer_writes();
	call_target_caller("_testObjectCommandsReadFile");
	stop_monitoring_object_buffer_writes();
	CHECK(get_object_target_byte("_testObjectFileLfn") == 0 &&
		get_object_target_word("_testObjectFileLength") == 0xFFFF,
		"OBJECT reader treated cbm_read's -1 error as file data");
	CHECK(object_file_close_calls == 1,
		"OBJECT reader did not close a file after cbm_read failed");
	CHECK(object_buffer_out_of_bounds_writes == 0,
		"OBJECT reader wrote to its destination after cbm_read failed");

	prepare();
	set_object_file_stream(NULL, 0, false);
	CHECK(call_target_caller_until_exit("_testObjectCommandsLoadObjectFile"),
		"OBJECT loader returned for a zero-length file");
	CHECK((regs.a | regs.x) != 0 && object_file_close_calls == 1,
		"OBJECT loader did not reject a zero-length file with a nonzero exit");

	prepare();
	set_object_file_stream(short_header, sizeof(short_header), false);
	CHECK(call_target_caller_until_exit("_testObjectCommandsLoadObjectFile"),
		"OBJECT loader returned for a short header");
	CHECK((regs.a | regs.x) != 0 && object_file_close_calls == 1,
		"OBJECT loader did not reject a short header with a nonzero exit");

	prepare();
	set_object_file_stream(one_byte, sizeof(one_byte), false);
	set_object_target_byte("_testObjectFileNullLength", 1);
	call_target_caller("_testObjectCommandsReadFile");
	CHECK(get_object_target_byte("_testObjectFileLfn") == 0 && cbm_open_calls == 0,
		"OBJECT reader opened a file with a null length parameter");

	prepare();
	set_object_file_stream(one_byte, sizeof(one_byte), false);
	set_object_target_byte("_testObjectFileNullBuffer", 1);
	call_target_caller("_testObjectCommandsReadFile");
	CHECK(get_object_target_byte("_testObjectFileLfn") == 0 &&
		get_object_target_word("_testObjectFileLength") == 0xFFFF && cbm_open_calls == 0,
		"OBJECT reader opened a file with a null destination buffer");
	free(data);
}

static void expect_object_startup_rejection(const uint8_t *data, unsigned length)
{
	prepare();
	set_object_file_stream(data, length, false);
	set_object_target_byte("_testObjectStartupContinuation", 0);
	CHECK(call_target_caller_until_exit("_testObjectCommandsInitObjects"),
		"Malformed OBJECT input returned through startup after loading");
	CHECK(regs.pc == symbol("_exit") && (regs.a | regs.x) != 0,
		"Malformed OBJECT input did not exit with a nonzero status");
	CHECK(get_object_target_byte("_testObjectStartupContinuation") == 0,
		"Malformed OBJECT input reached post-load initialization");
	CHECK(object_file_close_calls == 1,
		"Malformed OBJECT input did not close its file before exiting");
}

static void run_object_startup_validation_cases(void)
{
	static const uint8_t empty_objects[] = { 0, 0, 0 };
	static const uint8_t short_objects[] = { 0, 0 };
	static const uint8_t invalid_table_size[] = { 1, 0, 0 };
	static const uint8_t invalid_name_offset[] = { 3, 0, 0, 0, 0, 0 };
	static const uint8_t unterminated_name[] = { 3, 0, 0, 3, 0, 0, 'a', 'b', 'c' };
	unsigned objectCount = object_command_objects;
	unsigned tableSize = objectCount * 3u;
	unsigned tableEnd = tableSize + 3u;
	unsigned fullLength = tableEnd + objectCount * 2u;
	uint8_t full_objects[3 + 5 * 100];
	unsigned index;

	CHECK(objectCount == 100,
		"Production OBJECT target no longer uses MAX_OBJECTS=100");
	CHECK(fullLength <= sizeof(full_objects),
		"Full OBJECT table fixture exceeds its test buffer");

	prepare();
	set_object_file_stream(empty_objects, sizeof(empty_objects), false);
	call_target_caller("_testObjectCommandsInitObjects");
	CHECK(get_object_target_byte("_testObjectStartupContinuation") == 1 &&
		get_object_target_word("_testObjectObservedCount") == 0,
		"Valid empty OBJECT table did not complete initialization");
	CHECK(object_file_close_calls == 1,
		"Valid empty OBJECT table did not close its file");

	memset(full_objects, 0, sizeof(full_objects));
	full_objects[0] = (uint8_t)tableSize;
	full_objects[1] = (uint8_t)(tableSize >> 8);
	for (index = 0; index < objectCount; ++index)
	{
		unsigned record = 3u + index * 3u;
		unsigned name = tableEnd + index * 2u;
		unsigned nameOffset = name - 3u;
		full_objects[record] = (uint8_t)nameOffset;
		full_objects[record + 1u] = (uint8_t)(nameOffset >> 8);
		full_objects[record + 2u] = (uint8_t)index;
		full_objects[name] = (uint8_t)('A' + (index % 26u));
	}
	prepare();
	set_object_file_stream(full_objects, fullLength, false);
	call_target_caller("_testObjectCommandsInitObjects");
	CHECK(get_object_target_byte("_testObjectStartupContinuation") == 1 &&
		get_object_target_word("_testObjectObservedCount") == objectCount,
		"Valid full OBJECT table did not complete initialization");
	CHECK(object_file_close_calls == 1,
		"Valid full OBJECT table did not close its file");

	expect_object_startup_rejection(short_objects, sizeof(short_objects));
	expect_object_startup_rejection(invalid_table_size, sizeof(invalid_table_size));
	expect_object_startup_rejection(invalid_name_offset, sizeof(invalid_name_offset));
	expect_object_startup_rejection(unterminated_name, sizeof(unterminated_name));
}

static void load_object_crypto_stream(const uint8_t *stream, unsigned length,
	const uint8_t *expectedData, unsigned expectedCount,
	unsigned expectedNameOffset, unsigned expectedNameLength)
{
	unsigned dataAddress;
	unsigned dataOffset;
	uint8_t *objectData;

	prepare();
	set_object_file_stream(stream, length, false);
	monitor_object_buffer_reads(length);
	call_target_caller("_testObjectCommandsLoadObjectFile");
	stop_monitoring_object_buffer_reads();
	CHECK(object_buffer_prefix_reads == 0,
		"OBJECT crypto detector read before bDObjData");
	CHECK(object_buffer_suffix_reads == 0,
		"OBJECT crypto detector read beyond the input file");
	CHECK(object_file_close_calls == 1,
		"OBJECT loader did not close the input file");
	dataAddress = symbol("_bDObjData");
	if (get_object_target_word("_testObjectObservedCount") != expectedCount)
		fprintf(stderr, "OBJECT fixture length %u expected %u objects, got %u; stream %u/%u\n",
			length, expectedCount, get_object_target_word("_testObjectObservedCount"),
			object_file_stream_position, object_file_stream_length);
	CHECK(get_object_target_word("_testObjectObservedCount") == expectedCount,
		"OBJECT loader returned the wrong object count");

	dataOffset = dataAddress - BANK_START;
	objectData = bank_ram[0x0D] + dataOffset;
	CHECK(memcmp(objectData, expectedData, length) == 0,
		"OBJECT loader changed the input file while checking encryption");
	if (expectedCount != 0)
	{
		CHECK(get_object_target_word("_testObjectLoadedName") == dataAddress + expectedNameOffset &&
			get_object_target_word("_testObjectLoadedRoom") == 0 &&
			get_object_target_byte("_testObjectLoadedObjectNum") == 0 &&
			get_object_target_byte("_testObjectLoadedNameLength") == expectedNameLength,
			"OBJECT loader did not parse the first object's room and name");
	}
}

static void run_object_crypto_bounds_case(void)
{
	static const uint8_t empty_object_table[] = { 0, 0, 0 };
	uint8_t plain_objects[44] = { 0 };
	static const uint8_t avis_key[11] = {
		0x41, 0x76, 0x69, 0x73, 0x20, 0x44, 0x75, 0x72, 0x67, 0x61, 0x6E
	};
	uint8_t encrypted_objects[sizeof(plain_objects)];
	unsigned objectCount = 7;
	unsigned index;

	plain_objects[0] = objectCount * 3u;
	for (index = 0; index < objectCount; ++index)
	{
		unsigned record = 3u + index * 3u;
		unsigned nameOffset = objectCount * 3u + index * 3u;
		unsigned nameStart = nameOffset + 3u;
		plain_objects[record] = (uint8_t)nameOffset;
		plain_objects[record + 1u] = 0;
		plain_objects[record + 2u] = (uint8_t)index;
		plain_objects[nameStart] = (uint8_t)('A' + index);
	}
	for (index = 0; index < sizeof(plain_objects); ++index)
		encrypted_objects[index] = plain_objects[index] ^ avis_key[index % 11];

	load_object_crypto_stream(empty_object_table, sizeof(empty_object_table),
		empty_object_table, 0, 0, 0);
	load_object_crypto_stream(plain_objects, sizeof(plain_objects),
		plain_objects, objectCount, objectCount * 3u + 3u, 1);
	load_object_crypto_stream(encrypted_objects, sizeof(encrypted_objects),
		plain_objects, objectCount, objectCount * 3u + 3u, 1);
}

static void run_object_crypto_pointer_case(void)
{
	unsigned objectDataAddress;
	unsigned alternateDataAddress;

	prepare();
	objectDataAddress = symbol("_bDObjData");
	alternateDataAddress = symbol("_testObjectCryptoData");
	memset(bank_ram[0x0D] + objectDataAddress - BANK_START, 0, 20);
	memset(ram + alternateDataAddress, 0, 20);
	ram[alternateDataAddress + 19] = 0x01;
	set_object_target_word("_testObjectFileLength", 20);
	call_target_caller("_testObjectCommandsCheckCrypto");
	CHECK(get_object_target_byte("_testObjectCryptoResult"),
		"OBJECT crypto detector ignored its supplied data pointer");
}

static void run_object_commands_cases(void)
{
	prepare();
	object_command_objects = get_object_target_word("_testObjectCapacity");
	CHECK(object_command_objects > 0 && object_command_objects <= 255,
		"Production object capacity does not fit the byte command operand");
	CHECK(get_object_target_word("_testObjectStride") == 5,
		"Production object record stride is not the declared five-byte type");
	object_command_last = (uint8_t)(object_command_objects - 1);
	run_valid_object_commands();

	reset_object_fixture(object_command_objects);
	run_invalid_object_commands((uint8_t)object_command_objects);
	reset_object_fixture(object_command_objects);
	run_invalid_object_commands(255);

	reset_object_fixture(0);
	read_object(0, 0xA5, 0xDEAD, 0, 0xA5, 0xDEAD);
	write_object(0, 0x91, 0x1234, 0);
	run_invalid_object_commands(0);
	inspect_object(0, OBJECT_COMMAND_SENTINEL_ROOM, OBJECT_COMMAND_SENTINEL_NAME);
	run_object_file_capacity_cases();
	run_object_reader_capacity_case();
	run_object_startup_validation_cases();

	puts("Compiled object commands exercise real BANKRAM0F/BANKRAM03 handlers and BANKRAM0D accessors");
}

#endif
