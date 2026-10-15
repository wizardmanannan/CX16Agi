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

	puts("Compiled object commands exercise real BANKRAM0F/BANKRAM03 handlers and BANKRAM0D accessors");
}

#endif
