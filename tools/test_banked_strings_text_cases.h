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
	TEXT_CLEANUP_IRQ_RETURN = 0x02F0,
	KQ1_LONG_CREDIT_LINES = 14,
	KQ1_LONG_CREDIT_WIDTH = 40,
	KQ1_LONG_CREDIT_SOURCE_BYTES = KQ1_LONG_CREDIT_LINES * KQ1_LONG_CREDIT_WIDTH +
		KQ1_LONG_CREDIT_LINES - 1
};

static const char *const kq1StartupMessages[25] = {
	[0] = "Designed and    ", [1] = "    Written by: ", [2] = "Roberta Williams", [3] = "                ",
	[23] = "(c) copyright SIERRA On-Line Inc.  1987", [24] = "Press any key to continue."
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

static void check_startup_text_line(unsigned logicalRow, unsigned column, const char *message,
	uint8_t expected_guard_tile)
{
	unsigned physicalRow = logicalRow + 3;
	unsigned start = TEXT_CLEANUP_MAP_BASE + physicalRow * TEXT_CLEANUP_ROW_BYTES + column * 2;
	unsigned length = strlen(message);
	unsigned i;

	for (i = 0; i < length; ++i)
	{
		if (vram[start + i * 2] != (uint8_t)message[i])
			fprintf(stderr, "KQ1 startup row %u column %u cell %u contains $%02X, expected $%02X\n",
				logicalRow, column, i, vram[start + i * 2], (uint8_t)message[i]);
		CHECK(vram[start + i * 2] == (uint8_t)message[i],
			"Compiled KQ1 startup display command overwrote or misplaced a credit character");
	}
	CHECK(vram[start - 2] == expected_guard_tile &&
		vram[start + length * 2] == expected_guard_tile,
		"Compiled KQ1 startup display command wrote outside its message width");
}

static void begin_kq1_startup_trace(void)
{
	startup_display_c_sp_address = symbol("c_sp");
	startup_display_command_entry = symbol("_b3DisplayWithoutTextbox");
	startup_display_v_entry = symbol("_b3Display_v");
	startup_display_message_entry = symbol("_b3DisplayMessageBox");
	memset(&startup_display_trace, 0, sizeof(startup_display_trace));
	startup_display_trace_active = true;
}

static void check_kq1_startup_trace(void)
{
	CHECK(startup_display_trace.command_seen && startup_display_trace.message_seen,
		"Logic83 title did not reach the compiled display command and text renderer");
	CHECK(startup_display_trace.message == 24 && startup_display_trace.command_column == 1 &&
		startup_display_trace.command_row == 22,
		"Logic83 title display command supplied the wrong message, row, or column");
	CHECK(startup_display_trace.first_row == 4 && startup_display_trace.wrap == 0 &&
		startup_display_trace.width == 0 && startup_display_trace.column == 1 &&
		startup_display_trace.row == 22 && startup_display_trace.message_bank == 0x24 &&
		startup_display_trace.message_pointer == 0xA6C0 &&
		startup_display_trace.first_message_byte == '(',
		"Logic83 title did not resolve the expected raw banked copyright resource");
}

static void seed_startup_display_code_window(void)
{
	uint16_t codeWindowAddress = symbol("codeWindow");
	uint16_t codeWindowCursor = symbol("cwCurrentCode");
	unsigned codeOffset = 0;
	unsigned cycle, messageNo;

	ram[codeWindowCursor] = 0;
	ram[codeWindowCursor + 1] = 0;
	ram[codeWindowAddress + codeOffset++] = 22;
	ram[codeWindowAddress + codeOffset++] = 1;
	ram[codeWindowAddress + codeOffset++] = 24;
	ram[codeWindowAddress + codeOffset++] = 24;
	ram[codeWindowAddress + codeOffset++] = 5;
	ram[codeWindowAddress + codeOffset++] = 25;
	for (cycle = 1; cycle <= 4; ++cycle)
	{
		for (messageNo = cycle; messageNo > 0; --messageNo)
		{
			ram[codeWindowAddress + codeOffset++] = 1;
			ram[codeWindowAddress + codeOffset++] = 2;
			ram[codeWindowAddress + codeOffset++] = 3;
		}
	}
}

static void seed_kq1_startup_messages(void)
{
	unsigned messageNo;
	for (messageNo = 0; messageNo < 25; ++messageNo)
	{
		unsigned offset = 0x100 + messageNo * 0x40;
		size_t length;
		if (!kq1StartupMessages[messageNo]) continue;
		length = strlen(kq1StartupMessages[messageNo]) + 1;
		CHECK(length <= 0x40, "KQ1 startup message does not fit its banked resource slot");
		memcpy(bank_ram[0x24] + offset, kq1StartupMessages[messageNo], length);
	}
}

static void seed_startup_palette_colors(void)
{
	const unsigned palette_start = 0x1FA00;
	memset(vram + palette_start, 0, 16 * 16 * 2);
	/* Match graphicsAsm.s: palette 0 entry 15 is white; palette 1 is black/black/white/red. */
	vram[palette_start + 15 * 2] = 0xFF;
	vram[palette_start + 15 * 2 + 1] = 0x0F;
	vram[palette_start + 16 * 2 + 2 * 2] = 0xFF;
	vram[palette_start + 16 * 2 + 2 * 2 + 1] = 0x0F;
	vram[palette_start + 16 * 2 + 3 * 2] = 0x00;
	vram[palette_start + 16 * 2 + 3 * 2 + 1] = 0x0F;
}

static void seed_startup_credit_map_row(void)
{
	const unsigned row_start = TEXT_CLEANUP_MAP_BASE + 21 * TEXT_CLEANUP_ROW_BYTES;
	unsigned cell;
	for (cell = 0; cell < TEXT_CLEANUP_MAP_WIDTH; ++cell)
	{
		vram[row_start + cell * 2] = TEXT_CLEANUP_TRANSPARENT;
		vram[row_start + cell * 2 + 1] = TEXT_CLEANUP_CLEAR_PALETTE;
	}
}

static uint8_t kq1_long_credit_tile(unsigned line, unsigned column)
{
	return (uint8_t)('A' + ((line * 7 + column) % 26));
}

static void seed_kq1_long_credit_message(void)
{
	unsigned line, column, offset = 0;
	for (line = 0; line < KQ1_LONG_CREDIT_LINES; ++line)
	{
		for (column = 0; column < KQ1_LONG_CREDIT_WIDTH; ++column)
			bank_ram[0x24][0x100 + offset++] = kq1_long_credit_tile(line, column);
		if (line + 1 < KQ1_LONG_CREDIT_LINES)
			bank_ram[0x24][0x100 + offset++] = '\n';
	}
	bank_ram[0x24][0x100 + offset] = 0;
	CHECK(offset == KQ1_LONG_CREDIT_SOURCE_BYTES && offset > 512,
		"Long compiled display fixture does not cross the 512-byte IRQ copy slice");
}

static void seed_single_startup_display_command(void)
{
	uint16_t code_window = symbol("codeWindow");
	uint16_t current_code = symbol("cwCurrentCode");
	ram[current_code] = 0;
	ram[current_code + 1] = 0;
	ram[code_window] = 1;
	ram[code_window + 1] = 2;
	ram[code_window + 2] = 3;
}

static void seed_startup_long_credit_map(void)
{
	unsigned line, column;
	for (line = 0; line < KQ1_LONG_CREDIT_LINES; ++line)
	{
		unsigned row_start = TEXT_CLEANUP_MAP_BASE + (3 + line) * TEXT_CLEANUP_ROW_BYTES;
		for (column = 0; column < TEXT_CLEANUP_MAP_WIDTH; ++column)
		{
			vram[row_start + column * 2] = TEXT_CLEANUP_TRANSPARENT;
			vram[row_start + column * 2 + 1] = TEXT_CLEANUP_CLEAR_PALETTE;
		}
	}
}

static bool run_kq1_startup_long_display_case(void)
{
	uint16_t baseline_generation;
	uint16_t baseline_vsync;
	const unsigned palette_start = 0x1FA00;
	uint16_t textbox_background;
	uint16_t expected_dynamic_background;
	uint8_t dynamic_palette;
	unsigned sample, visible_cells = 0;
	bool pending_vsync_seen = false;
	bool palette_ok = true;

	prepare_text_cleanup_case();
	seed_startup_palette_colors();
	seed_kq1_long_credit_message();
	set_bank_byte(3, "_b3CurrentForegroundColour", 15);
	set_bank_byte(3, "_b3CurrentBackgroundColour", 0);
	call_target_caller("_testPrepareKq1StartupLongDisplay");
	seed_single_startup_display_command();
	seed_startup_long_credit_map();
	startup_credit_first_physical_row = 3;
	startup_credit_first_column = 0;
	startup_credit_line_count = KQ1_LONG_CREDIT_LINES;
	startup_credit_line_width = KQ1_LONG_CREDIT_WIDTH;
	startup_credit_partial_text = true;
	startup_credit_vsync_count = 0;
	startup_credit_irq_pending = false;
	startup_credit_palette_capture_active = true;
	baseline_generation = get_word(symbol("irqCompletionGeneration"));
	baseline_vsync = get_word(symbol("_vSyncCounter"));
	service_text_render_irqs = true;
	call_target_caller("_testRunKq1StartupLongCredit");
	startup_credit_palette_capture_active = false;
	service_text_render_irqs = false;
	CHECK(!startup_credit_irq_pending, "Long startup credit VSYNC capture did not reach the IRQ return");
	CHECK(startup_credit_vsync_count > 0, "Long startup credit did not publish work to a VSYNC");
	dynamic_palette = startup_credit_vsyncs[0].expected_attribute >> 4;
	CHECK(dynamic_palette >= 4 && dynamic_palette != 1,
		"Long compiled b3SetTextColor did not select a palette distinct from textbox palette 1");
	textbox_background = (uint16_t)(vram[palette_start + 1 * 32 + 4] |
		((uint16_t)vram[palette_start + 1 * 32 + 5] << 8));
	CHECK(textbox_background == 0x0FFF,
		"Long text fixture did not initialize textbox palette 1 with its actual white background");
	expected_dynamic_background = (uint16_t)(vram[palette_start + dynamic_palette * 32 + 4] |
		((uint16_t)vram[palette_start + dynamic_palette * 32 + 5] << 8));
	CHECK(expected_dynamic_background == 0,
		"Long compiled b3SetTextColor(15,0) did not create a black-background palette");
	for (sample = 0; sample < startup_credit_vsync_count; ++sample)
	{
		visible_cells += startup_credit_vsyncs[sample].visible_cells;
		if (startup_credit_vsyncs[sample].pending_at_entry)
			pending_vsync_seen = true;
		if (startup_credit_vsyncs[sample].wrong_visible_cell_palettes ||
			startup_credit_vsyncs[sample].wrong_visible_glyphs)
		{
			fprintf(stderr,
				"Long startup credit VSYNC %u: frame=%u pending=%u completion=%u work=%u stage=%u visible-cells=%u wrong-glyphs=%u wrong-palette-cells=%u old-palette-row-cells=%u background-RGB=$%04X expected-background-RGB=$%04X\n",
				sample, startup_credit_vsyncs[sample].vsync_counter - baseline_vsync,
				startup_credit_vsyncs[sample].pending_at_entry,
				startup_credit_vsyncs[sample].completion_generation,
				startup_credit_vsyncs[sample].work_kind,
				startup_credit_vsyncs[sample].text_stage,
				startup_credit_vsyncs[sample].visible_cells,
				startup_credit_vsyncs[sample].wrong_visible_glyphs,
				startup_credit_vsyncs[sample].wrong_visible_cell_palettes,
				startup_credit_vsyncs[sample].old_palette_row_cells,
				startup_credit_vsyncs[sample].visible_background_rgb,
				expected_dynamic_background);
		}
		CHECK(startup_credit_vsyncs[sample].wrong_visible_glyphs == 0,
			"Long startup credit slice wrote a wrong tile before VSYNC return");
		if (startup_credit_vsyncs[sample].wrong_visible_cell_palettes)
			palette_ok = false;
	}
	CHECK(startup_credit_vsync_count >= 2 && startup_credit_vsyncs[0].visible_cells == 500,
		"Long startup credit did not expose exactly the first 500 glyphs from its initial 512-byte source slice");
	CHECK(pending_vsync_seen, "Long startup credit capture missed a VSYNC with its display work pending");
	CHECK(startup_credit_vsyncs[startup_credit_vsync_count - 1].completion_generation ==
		(uint16_t)(baseline_generation + 1),
		"Long startup credit did not complete its published display work exactly once");
	CHECK(startup_credit_vsyncs[startup_credit_vsync_count - 1].visible_cells ==
		KQ1_LONG_CREDIT_LINES * KQ1_LONG_CREDIT_WIDTH,
		"Long startup credit completed without rendering all 560 message glyphs");
	for (sample = 0; sample < KQ1_LONG_CREDIT_LINES; ++sample)
	{
		unsigned row_start = TEXT_CLEANUP_MAP_BASE + (3 + sample) * TEXT_CLEANUP_ROW_BYTES;
		for (unsigned column = KQ1_LONG_CREDIT_WIDTH; column < TEXT_CLEANUP_MAP_WIDTH; ++column)
			CHECK(vram[row_start + column * 2] == TEXT_CLEANUP_TRANSPARENT,
				"Long startup credit changed a tile outside its 40-column message footprint");
	}
	for (unsigned offset = 0; offset < TEXT_CLEANUP_ROW_BYTES; ++offset)
	{
		CHECK(vram[TEXT_CLEANUP_MAP_BASE + 2 * TEXT_CLEANUP_ROW_BYTES + offset] == TEXT_CLEANUP_SENTINEL,
			"Long startup credit changed the VRAM row before its first text line");
		CHECK(vram[TEXT_CLEANUP_MAP_BASE + 17 * TEXT_CLEANUP_ROW_BYTES + offset] == TEXT_CLEANUP_SENTINEL,
			"Long startup credit changed the VRAM row after its final text line");
	}
	fprintf(stderr,
		"Long startup credit: source-bytes=%u VSYNC-slices=%u visible-cell-observations=%u palette-errors=%s\n",
		KQ1_LONG_CREDIT_SOURCE_BYTES, startup_credit_vsync_count, visible_cells,
		palette_ok ? "none" : "observed");
	return palette_ok;
}

static void run_kq1_startup_display_case(void)
{
	uint16_t baseline_generation;
	uint16_t baseline_vsync;
	unsigned palette_start = 0x1FA00;
	uint16_t textbox_background;
	uint16_t expected_dynamic_background;
	uint8_t dynamic_palette;
	unsigned sample;
	bool pending_vsync_seen = false;
	bool first_credit_palette_ok = true;
	bool long_credit_palette_ok;

	prepare_text_cleanup_case();
	seed_startup_palette_colors();
	seed_kq1_startup_messages();
	seed_startup_display_code_window();
	set_bank_byte(3, "_b3CurrentForegroundColour", 15);
	set_bank_byte(3, "_b3CurrentBackgroundColour", 0);
	begin_kq1_startup_trace();
	service_text_render_irqs = true;
	call_target_caller("_testPrepareKq1StartupDisplay");
	seed_startup_credit_map_row();
	startup_credit_first_physical_row = 21;
	startup_credit_first_column = 12;
	startup_credit_line_count = 1;
	startup_credit_line_width = strlen(kq1StartupMessages[0]);
	startup_credit_partial_text = false;
	startup_credit_vsync_count = 0;
	startup_credit_irq_pending = false;
	startup_credit_palette_capture_active = true;
	baseline_generation = get_word(symbol("irqCompletionGeneration"));
	baseline_vsync = get_word(symbol("_vSyncCounter"));
	call_target_caller("_testRunKq1StartupFirstCredit");
	startup_credit_palette_capture_active = false;
	CHECK(!startup_credit_irq_pending, "Startup credit VSYNC capture did not reach the IRQ return");
	CHECK(startup_credit_vsync_count > 0, "Startup credit did not publish work to a VSYNC");
	CHECK(startup_display_trace.display_v_seen && startup_display_trace.credit_message_seen &&
		startup_display_trace.credit_message_bank == 0x24 &&
		startup_display_trace.credit_message_pointer == 0xA100 &&
		startup_display_trace.credit_row == 18 && startup_display_trace.credit_column == 12 &&
		startup_display_trace.credit_first_message_byte == 'D',
		"KQ1 Logic83 first credit did not reach compiled b3Display_v with raw message 1 at row 18, column 12");
	dynamic_palette = startup_credit_vsyncs[0].expected_attribute >> 4;
	CHECK(dynamic_palette >= 4 && dynamic_palette != 1,
		"Compiled b3SetTextColor did not select a managed palette distinct from textbox palette 1");
	textbox_background = (uint16_t)(vram[palette_start + 1 * 32 + 4] |
		((uint16_t)vram[palette_start + 1 * 32 + 5] << 8));
	CHECK(textbox_background == 0x0FFF,
		"Startup text fixture did not initialize textbox palette 1 with its actual white background");
	expected_dynamic_background = (uint16_t)(vram[palette_start + dynamic_palette * 32 + 4] |
		((uint16_t)vram[palette_start + dynamic_palette * 32 + 5] << 8));
	CHECK(expected_dynamic_background == 0,
		"Compiled b3SetTextColor(15,0) did not create a black-background dynamic palette");
	for (sample = 0; sample < startup_credit_vsync_count; ++sample)
	{
		if (startup_credit_vsyncs[sample].pending_at_entry)
			pending_vsync_seen = true;
		if (startup_credit_vsyncs[sample].wrong_visible_cell_palettes ||
			startup_credit_vsyncs[sample].wrong_visible_glyphs)
		{
			fprintf(stderr,
				"Startup credit VSYNC %u: frame=%u pending=%u completion=%u work=%u stage=%u tile=$%02X palette=$%02X expected=$%02X wrong-glyphs=%u wrong-palette-cells=%u visible-cells=%u old-palette-row-cells=%u background-RGB=$%04X expected-background-RGB=$%04X\n",
				sample, startup_credit_vsyncs[sample].vsync_counter - baseline_vsync,
				startup_credit_vsyncs[sample].pending_at_entry,
				startup_credit_vsyncs[sample].completion_generation,
				startup_credit_vsyncs[sample].work_kind,
				startup_credit_vsyncs[sample].text_stage,
				startup_credit_vsyncs[sample].first_tile,
				startup_credit_vsyncs[sample].first_attribute,
				startup_credit_vsyncs[sample].expected_attribute,
				startup_credit_vsyncs[sample].wrong_visible_glyphs,
				startup_credit_vsyncs[sample].wrong_visible_cell_palettes,
				startup_credit_vsyncs[sample].visible_cells,
				startup_credit_vsyncs[sample].old_palette_row_cells,
				startup_credit_vsyncs[sample].visible_background_rgb,
				expected_dynamic_background);
		}
		CHECK(startup_credit_vsyncs[sample].wrong_visible_glyphs == 0,
			"Startup credit glyphs are not visible with their published message at VSYNC return");
		if (startup_credit_vsyncs[sample].wrong_visible_cell_palettes)
			first_credit_palette_ok = false;
	}
	CHECK(pending_vsync_seen, "Startup credit capture missed the VSYNC with its published display work pending");
	CHECK(startup_credit_vsyncs[startup_credit_vsync_count - 1].completion_generation ==
		(uint16_t)(baseline_generation + 1),
		"Startup credit did not complete its published display work exactly once");
	call_target_caller("_testRunKq1StartupRemainingCredits");
	service_text_render_irqs = false;
	startup_display_trace_active = false;
	check_kq1_startup_trace();
	CHECK(ram[symbol("_inputLineDirty")] == 0,
		"Hidden input redraw erased the Logic83 copyright row");

	check_startup_text_line(22, 1, "(c) copyright SIERRA On-Line Inc.  1987", TEXT_CLEANUP_SENTINEL);
	check_startup_text_line(24, 5, "Press any key to continue.", TEXT_CLEANUP_SENTINEL);
	check_startup_text_line(18, 12, "                ", TEXT_CLEANUP_TRANSPARENT);
	check_startup_text_line(17, 12, "Roberta Williams", TEXT_CLEANUP_SENTINEL);
	check_startup_text_line(16, 12, "    Written by: ", TEXT_CLEANUP_SENTINEL);
	check_startup_text_line(15, 12, "Designed and    ", TEXT_CLEANUP_SENTINEL);
	long_credit_palette_ok = run_kq1_startup_long_display_case();
	CHECK(first_credit_palette_ok,
		"Startup credit glyph background uses the previous textbox palette at VSYNC return");
	CHECK(long_credit_palette_ok,
		"Long startup credit glyph background uses the previous textbox palette during a partial IRQ slice");
	puts("Compiled Logic83 startup display commands keep credits and fixed text on their requested VRAM rows");
}

static void run_text_cleanup_cases(void)
{
	run_kq1_startup_display_case();
	run_text_cleanup_case("_testRunTextCleanupRow1", 0);
	run_text_cleanup_case("_testRunTextCleanupRow4", 3);
	run_text_cleanup_case("_testRunTextCleanupLastRow", TEXT_CLEANUP_MAP_ROWS - 1);
	run_text_cleanup_maximum_case("_testRunTextCleanupMaximumOrigin4", 4);
	run_text_cleanup_maximum_case("_testRunTextCleanupMaximumOrigin2", 2);
	run_text_cleanup_maximum_case("_testRunTextCleanupMaximumOrigin1", 1);
	puts("Compiled text cleanup passes origin 1/4/32 with real IRQ-rendered VRAM rows and canaries");
}
