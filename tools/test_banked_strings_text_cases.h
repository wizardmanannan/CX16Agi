enum
{
	TEXT_CLEANUP_MAP_BASE = 0xDA00,
	TEXT_CLEANUP_ROW_BYTES = 128,
	TEXT_CLEANUP_MAP_ROWS = 32,
	TEXT_CLEANUP_MAP_WIDTH = 64,
	TEXT_CLEANUP_TRANSPARENT = 128,
	TEXT_CLEANUP_CLEAR_PALETTE = 0x10,
	TEXT_CLEANUP_SENTINEL = 0x5A,
	TEXT_CLEANUP_BUFFER_BYTES = 2000,
	TEXT_CLEANUP_MAX_STAGING_BYTES = TEXT_CLEANUP_MAP_ROWS * (TEXT_CLEANUP_MAP_WIDTH + 1),
	TEXT_CLEANUP_IRQ_RETURN = 0x02F0
};

static void set_resident_byte(const char *name, uint8_t value)
{
	uint16_t address = symbol(name);
	CHECK(address < BANK_START || address >= BANK_END,
		"Text cleanup test expected a resident state byte");
	ram[address] = value;
}

static void set_bank_byte(uint8_t bank, const char *name, uint8_t value)
{
	uint16_t address = symbol(name);
	CHECK(address >= BANK_START && address < BANK_END,
		"Text cleanup test expected a banked state byte");
	bank_ram[bank][address - BANK_START] = value;
}

static void prepare_text_cleanup_case(void)
{
	uint16_t text_buffers = symbol("_b3TextBuffers");

	prepare();
	CHECK(text_buffers >= BANK_START && (unsigned)text_buffers + TEXT_CLEANUP_BUFFER_BYTES <= BANK_END,
		"Text buffer is not wholly contained in its expected bank");
	memset(bank_ram[3] + text_buffers - BANK_START, 0, TEXT_CLEANUP_BUFFER_BYTES);
	set_bank_byte(3, "_b3LastBoxLines", 0);
	set_bank_byte(3, "_b3LastBoxStartLine", 0);
	set_bank_byte(SOUND_BANK, "_bBChannelsPlaying", 0);
	set_resident_byte("_inputLineDirty", 0);
	set_resident_byte("_menuDirty", 0);
	set_resident_byte("_menuShown", 0);
	set_resident_byte("_menuAllowed", 0);
	set_resident_byte("_mainLoopCounter", 0xFF);
	ram[VERA_ISR] = 0;
	ram[0xFFFE] = (uint8_t)symbol("custom_irq_handler");
	ram[0xFFFF] = (uint8_t)(symbol("custom_irq_handler") >> 8);
	ram[symbol("default_irq_vector")] = (uint8_t)TEXT_CLEANUP_IRQ_RETURN;
	ram[symbol("default_irq_vector") + 1] = (uint8_t)(TEXT_CLEANUP_IRQ_RETURN >> 8);
	ram[TEXT_CLEANUP_IRQ_RETURN] = 0x40; /* RTI: return from the real production IRQ handler. */
	memset(vram, TEXT_CLEANUP_SENTINEL, sizeof(vram));
}

static void check_text_cleanup_canary_row(unsigned start)
{
	unsigned offset;
	for (offset = 0; offset < TEXT_CLEANUP_ROW_BYTES; ++offset)
		CHECK(vram[start + offset] == TEXT_CLEANUP_SENTINEL,
			"Text display or cleanup changed a neighboring VRAM row");
}

static void run_text_cleanup_case(const char *display_entry, unsigned physical_row)
{
	unsigned row_start = TEXT_CLEANUP_MAP_BASE + physical_row * TEXT_CLEANUP_ROW_BYTES;
	unsigned offset;

	prepare_text_cleanup_case();
	service_text_render_irqs = true;
	call_target_caller(display_entry);
	service_text_render_irqs = false;
	CHECK(vram[row_start] == ((uint8_t)'H' | 0x80) &&
		vram[row_start + 2] == ((uint8_t)'I' | 0x80),
		"Compiled text display did not write its requested physical row");
	CHECK(vram[row_start + 1] == 0 && vram[row_start + 3] == 0,
		"Compiled text display did not write the requested palette");
	CHECK(vram[row_start + 4] == TEXT_CLEANUP_SENTINEL,
		"Compiled text display overwrote unused cells in its row");
	check_text_cleanup_canary_row(row_start - TEXT_CLEANUP_ROW_BYTES);
	check_text_cleanup_canary_row(row_start + TEXT_CLEANUP_ROW_BYTES);

	service_text_render_irqs = true;
	call_target_caller("_testRunTextCleanupClear");
	service_text_render_irqs = false;
	for (offset = 0; offset < TEXT_CLEANUP_MAP_WIDTH; ++offset)
	{
		CHECK(vram[row_start + offset * 2] == TEXT_CLEANUP_TRANSPARENT,
			"Compiled cleanup left a visible tile in the displayed row");
		CHECK(vram[row_start + offset * 2 + 1] == TEXT_CLEANUP_CLEAR_PALETTE,
			"Compiled cleanup omitted the displayed row's palette bytes");
	}
	check_text_cleanup_canary_row(row_start - TEXT_CLEANUP_ROW_BYTES);
	check_text_cleanup_canary_row(row_start + TEXT_CLEANUP_ROW_BYTES);
}

static void check_full_map_canaries(void)
{
	unsigned offset;
	for (offset = 0; offset < TEXT_CLEANUP_ROW_BYTES; ++offset)
	{
		CHECK(vram[TEXT_CLEANUP_MAP_BASE - TEXT_CLEANUP_ROW_BYTES + offset] == TEXT_CLEANUP_SENTINEL,
			"Maximum-height cleanup changed the VRAM row before the tile map");
		CHECK(vram[TEXT_CLEANUP_MAP_BASE + TEXT_CLEANUP_MAP_ROWS * TEXT_CLEANUP_ROW_BYTES + offset] ==
			TEXT_CLEANUP_SENTINEL,
			"Maximum-height cleanup changed the VRAM row after the tile map");
	}
}

static void run_text_cleanup_maximum_case(const char *display_entry, unsigned firstRow)
{
	unsigned expectedRows = TEXT_CLEANUP_MAP_ROWS - firstRow + 1;
	unsigned firstMapRow = firstRow - 1;
	unsigned row, column, wrongCells = 0;
	uint16_t textBuffers;
	unsigned linesAddress, startAddress;

	prepare_text_cleanup_case();
	textBuffers = symbol("_b3TextBuffers");
	CHECK((unsigned)textBuffers + TEXT_CLEANUP_MAX_STAGING_BYTES <= BANK_END,
		"Target text staging guard is outside bank 3");
	service_text_render_irqs = true;
	call_target_caller(display_entry);
	service_text_render_irqs = false;

	linesAddress = symbol("_b3LastBoxLines");
	startAddress = symbol("_b3LastBoxStartLine");
	CHECK(bank_ram[3][linesAddress - BANK_START] == expectedRows &&
		bank_ram[3][startAddress - BANK_START] == 0,
		"Compiled maximum-height display published the wrong extent");
	for (row = 0; row < TEXT_CLEANUP_MAP_ROWS; ++row)
	{
		unsigned rowStart = TEXT_CLEANUP_MAP_BASE + row * TEXT_CLEANUP_ROW_BYTES;
		for (column = 0; column < TEXT_CLEANUP_MAP_WIDTH; ++column)
		{
			uint8_t expectedTile = row >= firstMapRow && column == 0 ? (uint8_t)('A' | 0x80) :
				TEXT_CLEANUP_SENTINEL;
			uint8_t expectedPalette = row >= firstMapRow ? 0 : TEXT_CLEANUP_SENTINEL;
			if (vram[rowStart + column * 2] != expectedTile ||
				vram[rowStart + column * 2 + 1] != expectedPalette)
			{
				fprintf(stderr, "Maximum-height display row %u cell %u: tile $%02X palette $%02X, expected $%02X/$%02X\n",
					row, column, vram[rowStart + column * 2], vram[rowStart + column * 2 + 1],
					expectedTile, expectedPalette);
				CHECK(false, "Compiled maximum-height display wrote the wrong physical tile or palette");
			}
		}
	}
	check_full_map_canaries();

	text_buffer_overflow_start = textBuffers + TEXT_CLEANUP_BUFFER_BYTES;
	text_buffer_overflow_end = textBuffers + TEXT_CLEANUP_MAX_STAGING_BYTES;
	text_buffer_overflow_writes = 0;
	set_bank_byte(3, "_b3CurrentForegroundColour", 0x36);
	set_bank_byte(3, "_b3CurrentBackgroundColour", 0x69);
	watch_text_buffer_overflow = true;
	service_text_render_irqs = true;
	call_target_caller("_testRunTextCleanupClear");
	service_text_render_irqs = false;
	watch_text_buffer_overflow = false;
	if (text_buffer_overflow_writes)
		fprintf(stderr, "Text buffer watch saw %u writes beginning at $%04X (guard $%04X-$%04X)\n",
			text_buffer_overflow_writes, first_text_buffer_overflow_address,
			text_buffer_overflow_start, text_buffer_overflow_end);
	CHECK(text_buffer_overflow_writes == 0,
		"Production cleanup wrote beyond the 2,000-byte banked text buffer");
	CHECK(bank_ram[3][linesAddress - BANK_START] == expectedRows &&
		bank_ram[3][startAddress - BANK_START] == 0,
		"Compiled chunked cleanup replaced the original display extent");
	CHECK(bank_ram[3][symbol("_b3CurrentForegroundColour") - BANK_START] == 0x36 &&
		bank_ram[3][symbol("_b3CurrentBackgroundColour") - BANK_START] == 0x69,
		"Compiled cleanup changed text-color state while staging the clear");

	for (row = 0; row < TEXT_CLEANUP_MAP_ROWS; ++row)
	{
		unsigned rowStart = TEXT_CLEANUP_MAP_BASE + row * TEXT_CLEANUP_ROW_BYTES;
		for (column = 0; column < TEXT_CLEANUP_MAP_WIDTH; ++column)
		{
			uint8_t expectedTile = row >= firstMapRow ? TEXT_CLEANUP_TRANSPARENT : TEXT_CLEANUP_SENTINEL;
			uint8_t expectedPalette = row >= firstMapRow ? TEXT_CLEANUP_CLEAR_PALETTE : TEXT_CLEANUP_SENTINEL;
			if (vram[rowStart + column * 2] != expectedTile ||
				vram[rowStart + column * 2 + 1] != expectedPalette)
			{
				if (wrongCells < 10)
					fprintf(stderr, "Maximum-height cleanup row %u cell %u: tile $%02X palette $%02X, expected $%02X/$%02X\n",
						row, column, vram[rowStart + column * 2], vram[rowStart + column * 2 + 1],
						expectedTile, expectedPalette);
				++wrongCells;
			}
		}
	}
	CHECK(!wrongCells, "Compiled maximum-height cleanup left a tile or palette byte incorrect");
	check_full_map_canaries();
}

static void run_text_cleanup_cases(void)
{
	run_text_cleanup_case("_testRunTextCleanupRow1", 0);
	run_text_cleanup_case("_testRunTextCleanupRow4", 3);
	run_text_cleanup_case("_testRunTextCleanupLastRow", TEXT_CLEANUP_MAP_ROWS - 1);
	run_text_cleanup_maximum_case("_testRunTextCleanupMaximumOrigin4", 4);
	run_text_cleanup_maximum_case("_testRunTextCleanupMaximumOrigin2", 2);
	run_text_cleanup_maximum_case("_testRunTextCleanupMaximumOrigin1", 1);
	puts("Compiled text cleanup passes origin 1/4/32 with real IRQ-rendered VRAM rows and canaries");
}
