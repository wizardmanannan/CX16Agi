/* Host-check the production text preparation C with VERA/IRQ boundaries stubbed. */
#define _CX16_H
#define _MSC_VER
#define __fastcall__
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned hardwareOperations;
#define asm(...) (++hardwareOperations)
#include "../src/textLayer.c"

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); \
		exit(1); \
	} \
} while (0)

unsigned long displayTextAddressToCopyTo;
char* currentTextBuffer;
long b3PaletteAddress;
byte b3PaletteRows;
byte b3PaletteNumber;
byte _assmByte;
unsigned int _assmUInt;
long _assmLong;

static unsigned displayCalls;
static unsigned displayedCells;
static unsigned displayedNewlines;
static unsigned unexpectedText;
static unsigned captureFillOutput;
static unsigned lowestFillMapRow;
static unsigned highestFillMapRow;
static unsigned fillMapRowsSeen;
static unsigned bankCopyCalls;
static size_t bankCopyLength;
static unsigned bankWriteCalls;
static char* boundedMessageSource;
static int paletteId;
static byte paletteSlot;
static PaletteGetResult paletteResult;
static unsigned foregroundEnters;
static unsigned foregroundLeaves;

void b6SetAndWaitForIrqState(IRQ_COMMAND state)
{
	const unsigned char* p;
	unsigned mapRow, rows = 1;
	CHECK(state == DISPLAY_TEXT, "Text preparation sent an unexpected IRQ command");
	++displayCalls;
	if (!captureFillOutput) return;
	mapRow = (unsigned)((displayTextAddressToCopyTo - MAPBASE) / TILE_LAYER_BYTES_PER_ROW);
	for (p = (const unsigned char*)currentTextBuffer; *p; ++p)
	{
		if (*p == TRANSPARENT_CHAR) ++displayedCells;
		else if (*p == NEW_LINE)
		{
			++displayedNewlines;
			++rows;
		}
		else ++unexpectedText;
	}
	if (!fillMapRowsSeen || mapRow < lowestFillMapRow) lowestFillMapRow = mapRow;
	if (!fillMapRowsSeen || mapRow + rows - 1 > highestFillMapRow)
		highestFillMapRow = mapRow + rows - 1;
	++fillMapRowsSeen;
}

void b6EnterForegroundVera(void) { ++foregroundEnters; }
void b6LeaveForegroundVera(void) { ++foregroundLeaves; }

unsigned int b6SetPaletteToInt(byte color) { return color; }

byte bFGetPalette(int id, PaletteGetResult* result)
{
	paletteId = id;
	*result = paletteResult;
	return paletteSlot;
}

size_t strLenBanked(char* text, int bank)
{
	(void)bank;
	return strlen(text);
}

size_t strLenBankedBounded(char* text, int bank, size_t byteLimit, boolean* terminated)
{
	size_t length = 0;
	(void)bank;
	if (terminated) *terminated = FALSE;
	if (!text) return 0;
	while (length < byteLimit && text[length]) ++length;
	if (terminated) *terminated = length < byteLimit;
	return length;
}

void* memCpyBanked(byte* destination, byte* source, byte bank, size_t length)
{
	(void)bank;
	++bankWriteCalls;
	return memcpy(destination, source, length);
}

void memCpyBankedBetween(byte* destination, byte bankDestination,
	byte* source, byte bankSource, size_t length)
{
	byte chunk[50];
	size_t offset, amount;
	(void)bankDestination;
	(void)bankSource;
	++bankCopyCalls;
	bankCopyLength = length;
	if (source == (byte*)boundedMessageSource)
		CHECK(source[TEXTBUFFER_SIZE * 2 - 1] == 'X',
			"Production changed the long message before copying it");
	CHECK(length <= TEXTBUFFER_SIZE * 2,
		"Production requested a copy larger than both text buffers");
	for (offset = 0; offset < length; offset += amount)
	{
		amount = length - offset < sizeof(chunk) ? length - offset : sizeof(chunk);
		memcpy(chunk, source + offset, amount);
		memcpy(destination + offset, chunk, amount);
	}
}

extern void b3DrawBorder(byte boxWidth, size_t messageSize);

static void test_fill_full_layer(void)
{
	displayCalls = displayedCells = displayedNewlines = unexpectedText = 0;
	fillMapRowsSeen = 0;
	captureFillOutput = 1;
	b3FillChar(0, 255, 0, TRANSPARENT_CHAR);
	captureFillOutput = 0;
	CHECK(fillMapRowsSeen == 1, "Clearing the visible map range was split unexpectedly");
	CHECK(displayCalls > 0, "Clearing the layer did not submit text");
	CHECK(displayedCells == TILE_LAYER_WIDTH * (TILE_LAYER_HEIGHT - FIRST_TEXT_ROW + 1),
		"Clearing the layer omitted cells");
	CHECK(displayedNewlines + displayCalls == TILE_LAYER_HEIGHT - FIRST_TEXT_ROW + 1,
		"Clearing the layer changed its row boundaries");
	CHECK(lowestFillMapRow == FIRST_TEXT_ROW - 1 &&
		highestFillMapRow == TILE_LAYER_HEIGHT - 1,
		"Clearing the layer escaped the physical tile-map range");
	CHECK(!unexpectedText, "Clearing the layer submitted unexpected text");
}

static void test_fill_rejects_wrapped_row(void)
{
	displayCalls = 0;
	memset(b3TextBuffer1, 0x5A, TEXTBUFFER_SIZE);
	b3FillChar(TILE_LAYER_HEIGHT - FIRST_TEXT_ROW + 1,
		TILE_LAYER_HEIGHT - 1, 0, TRANSPARENT_CHAR);
	CHECK(!displayCalls, "A row below the tile-map bottom was rendered");
	CHECK((unsigned char)b3TextBuffer1[0] == 0x5A,
		"An out-of-map row modified the text buffer");

	b3FillChar(255, 255, 0, TRANSPARENT_CHAR);
	CHECK(!displayCalls, "An invalid row was rendered");
	CHECK((unsigned char)b3TextBuffer1[0] == 0x5A,
		"An invalid row modified the text buffer");
}

static void test_fill_clear_lines_map_range(void)
{
	displayCalls = displayedCells = displayedNewlines = unexpectedText = 0;
	fillMapRowsSeen = 0;
	captureFillOutput = 1;
	b3FillChar(0, 23, 0, TRANSPARENT_CHAR);
	captureFillOutput = 0;
	CHECK(fillMapRowsSeen == 1, "The AGI clear_lines range was split unexpectedly");
	CHECK(lowestFillMapRow == FIRST_TEXT_ROW - 1,
		"AGI row zero no longer maps to the first gameplay map row");
	CHECK(highestFillMapRow == FIRST_TEXT_ROW + 23 - 1,
		"AGI row 23 changed its established map row");
	CHECK(highestFillMapRow < TILE_LAYER_HEIGHT,
		"The production clear_lines range writes past the tile map");
	CHECK(displayedCells == TILE_LAYER_WIDTH * 24,
		"Clearing AGI rows 0 through 23 omitted cells");
	CHECK(!unexpectedText, "Clearing AGI rows submitted unexpected text");
}

static void test_long_message_is_bounded_and_unchanged(void)
{
	char message[TEXTBUFFER_SIZE * 2 + 101];
	memset(message, 'X', sizeof(message) - 1);
	message[sizeof(message) - 1] = '\0';
	bankCopyCalls = bankWriteCalls = 0;
	displayCalls = 0;
	boundedMessageSource = message;
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, FIRST_TEXT_ROW);
	boundedMessageSource = NULL;
	CHECK(!bankWriteCalls, "Displaying a long message modified its source");
	CHECK(bankCopyCalls == 1 && bankCopyLength == TEXTBUFFER_SIZE * 2,
		"Long message copy exceeded the available text buffers");
	CHECK(message[TEXTBUFFER_SIZE * 2 - 1] == 'X',
		"Long message source terminator was overwritten");
	CHECK(b3TextBuffer1[TEXTBUFFER_SIZE * 2 - 1] == '\0',
		"Long message destination was not terminated at its capacity");
	CHECK(displayCalls == 1, "Long message was not submitted for display");
}

static void test_source_length_is_bounded(void)
{
	char message[TEXTBUFFER_TOTAL_SIZE + 1];
	memset(message, 'X', sizeof(message) - 1);
	message[sizeof(message) - 1] = '\0';
	bankCopyCalls = bankWriteCalls = 0;
	displayCalls = 0;
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, FIRST_TEXT_ROW);
	CHECK(bankCopyCalls == 1 && bankCopyLength == TEXTBUFFER_TOTAL_SIZE,
		"Source length was not bounded by the available text buffers");
	CHECK(b3TextBuffer1[TEXTBUFFER_TOTAL_SIZE - 1] == '\0',
		"Source at destination capacity was not terminated at capacity");
	CHECK(displayCalls == 1,
		"Maximum reported string length was not submitted for display");
}

static void test_border_rejects_oversized_output(void)
{
	char message[41];
	memset(message, 'X', sizeof(message) - 1);
	message[sizeof(message) - 1] = '\0';
	displayCalls = 0;
	b3DisplayMessageBox(message, 0, 0, 0, 0, 254, FALSE, FIRST_TEXT_ROW);
	CHECK(b3TextBuffer2[0] == '\0', "Oversized border output was not rejected");
	CHECK(!displayCalls, "Rejected border output was still submitted for display");
}

static void test_border_keeps_content_inside_requested_width(void)
{
	char message[] = "abcdefghijklmnopqrstuvwxzy1234567890";
	char* line = b3TextBuffer2;
	size_t lineLength;
	const size_t maximumRowLength = 11; /* b3DrawBorder receives boxWidth - 2. */
	b3DisplayMessageBox(message, 0, 0, 0, 0, 12, FALSE, FIRST_TEXT_ROW);
	if (!b3TextBuffer2[0]) return; /* Rejecting an unsafe box is valid. */
	while (*line)
	{
		char* newline = strchr(line, NEW_LINE);
		CHECK(newline != NULL, "Border output ended without a row boundary");
		lineLength = (size_t)(newline - line);
		CHECK(lineLength <= maximumRowLength,
			"Border content row extends beyond its requested width");
		line = newline + 1;
	}
}

static void test_border_accepts_empty_text(void)
{
	b3TextBuffer1[0] = '\0';
	b3TextBuffer2[0] = 'X';
	b3DrawBorder(10, 1);
	CHECK(b3TextBuffer2[0] == '\0', "Empty border input did not produce empty output");
}

static void test_border_preserves_trailing_blank_line(void)
{
	char message[] = "first\n";
	char* line;
	unsigned rows = 0;
	b3DisplayMessageBox(message, 0, 0, 0, 0, 12, FALSE, FIRST_TEXT_ROW);
	for (line = b3TextBuffer2; *line; ++line)
		if (*line == NEW_LINE) ++rows;
	++rows;
	CHECK(rows == 6, "Boxed trailing newline lost its final blank content row");
	CHECK(b3LastBoxLines == rows && b3PaletteRows == rows,
		"Boxed trailing blank row was omitted from published row metadata");
}

static void test_border_exact_capacity_includes_terminator(void)
{
	char* line;
	char* newline;
	size_t row;
	size_t lineLength;
	memset(b3TextBuffer1, 'X', 197);
	b3TextBuffer1[197] = '\0';
	b3DrawBorder(198, 198);
	CHECK(strlen(b3TextBuffer2) == TEXTBUFFER_SIZE - 1,
		"Exact-capacity border output did not leave room for its terminator");
	line = b3TextBuffer2;
	for (row = 0; row < 4; ++row)
	{
		newline = strchr(line, NEW_LINE);
		CHECK(newline != NULL, "Exact-capacity border omitted a row boundary");
		lineLength = (size_t)(newline - line);
		CHECK(lineLength == 199, "Exact-capacity border row has the wrong width");
		line = newline + 1;
	}
	CHECK(strlen(line) == 199,
		"Exact-capacity border has the wrong number of rows or wrong final width");
	CHECK(line[199] == '\0', "Exact-capacity border omitted its final NUL");
}

static void test_wrap_width_includes_last_line(void)
{
	char text[] = "abc";
	byte maxWidth;
	CHECK(b3WrapText(text, 10, &maxWidth) == 1,
		"Unwrapped text reported the wrong line count");
	CHECK(maxWidth == 3, "Unwrapped text omitted its final line width");
}

static void test_wrap_newline_without_spaces(void)
{
	char text[] = "abc\nxy";
	byte maxWidth;
	b3LastBoxLines = 1;
	CHECK(b3WrapText(text, 10, &maxWidth) == 2,
		"Explicit newline reported the wrong line count");
	CHECK(maxWidth == 3, "Explicit newline lost the widest line");
	CHECK(!strcmp(text, "abc\nxy"), "Explicit newline changed the text");
}

static void test_wrap_width_excludes_replaced_separator(void)
{
	char text[] = "abc def";
	byte maxWidth;
	b3LastBoxLines = 1;
	CHECK(b3WrapText(text, 3, &maxWidth) == 2,
		"Wrapped separator reported the wrong line count");
	CHECK(maxWidth == 3,
		"Wrapped separator was counted as a visible text column");
}

static void test_wrapped_box_fits_its_content(void)
{
	char text[] = "aaaa bbbb cccc dddd eeee ffff gggg hhhh";
	static const char expectedContent[4][10] =
	{
		"aaaa bbbb", "cccc dddd", "eeee ffff", "gggg hhhh"
	};
	char* line;
	char* rowStart;
	char* newline;
	size_t lineLength;
	size_t renderedRows = 0;
	size_t contentRows = 0;
	const size_t expectedRowLength = 11; /* Nine text columns and two borders. */
	const size_t expectedRows = 8; /* Top/padding + four content rows + padding/bottom. */
	b3DisplayMessageBox(text, 0, 0, 0, 0, 12, TRUE, FIRST_TEXT_ROW);
	CHECK(currentTextBuffer == b3TextBuffer2,
		"Wrapped box did not select the bordered output buffer");
	CHECK(b3TextBuffer2[0] != '\0', "Wrapped box rejected content that should fit");
	line = b3TextBuffer2;
	while (*line)
	{
		rowStart = line;
		newline = strchr(line, NEW_LINE);
		if (newline)
		{
			lineLength = (size_t)(newline - line);
			line = newline + 1;
		}
		else
		{
			lineLength = strlen(line); /* Bottom border has no trailing newline. */
			line += lineLength;
		}
		CHECK(lineLength == expectedRowLength,
			"Wrapped box row did not match its exact content width plus two borders");
		if (renderedRows >= 2 && renderedRows < 6)
		{
			CHECK((unsigned char)rowStart[0] == LEFT_BORDER &&
				(unsigned char)rowStart[10] == RIGHT_BORDER,
				"Wrapped content row lost a border character");
			CHECK(!memcmp(rowStart + 1, expectedContent[contentRows], 9),
				"Wrapped box changed or misplaced word content");
			++contentRows;
		}
		++renderedRows;
	}
	CHECK(renderedRows == expectedRows,
		"Wrapped box emitted the wrong number of padded/content rows");
	CHECK(contentRows == sizeof(expectedContent) / sizeof(expectedContent[0]),
		"Wrapped box did not render all expected content rows");
}

static void test_text_color_ids(void)
{
	byte foreground, background;
	paletteResult = Allocated;
	paletteSlot = 7;
	for (foreground = 0; foreground < 16; ++foreground)
	{
		for (background = 0; background < 16; ++background)
		{
			CHECK(b3SetTextColor(foreground, background) == paletteSlot,
				"Text color returned the wrong palette slot");
			CHECK(paletteId == BASE_TEXT_ID + foreground * 16 + background,
				"Text color used the wrong palette cache ID");
			CHECK(b3CurrentForegroundColour == foreground &&
				b3CurrentBackgroundColour == background,
				"Allocated text color was not published");
		}
	}
}

static void test_text_color_allocation_failure(void)
{
	b3CurrentForegroundColour = 3;
	b3CurrentBackgroundColour = 4;
	paletteResult = FailToAllocate;
	paletteSlot = 0;
	foregroundEnters = foregroundLeaves = 0;
	hardwareOperations = 0;
	CHECK(b3SetTextColor(9, 7) == 0,
		"Failed text color allocation did not use the fallback palette");
	CHECK(b3CurrentForegroundColour == 3 && b3CurrentBackgroundColour == 4,
		"Failed text color allocation replaced the active colors");
	CHECK(!foregroundEnters && !foregroundLeaves,
		"Failed text color allocation entered a VERA write section");
	CHECK(!hardwareOperations, "Failed text color allocation wrote VERA registers");
}

static void check_published_text_rows(char* message, byte width, boolean wrap, unsigned expected)
{
	displayCalls = 0;
	b3DisplayMessageBox(message, 0, 1, 0, 2, width, wrap, FIRST_TEXT_ROW);
	CHECK(displayCalls == 1, "Multiline text was not submitted");
	unsigned rows = 1;
	for (const char* p = currentTextBuffer; *p; ++p)
		if (*p == NEW_LINE) ++rows;
	CHECK(rows == expected, "Prepared text has an unexpected row count");
	CHECK(b3PaletteRows == rows, "Published palette rows omit rendered text rows");
	CHECK(b3LastBoxLines == rows, "Published cleanup rows omit rendered text rows");
	captureFillOutput = 1;
	displayedCells = displayedNewlines = unexpectedText = fillMapRowsSeen = 0;
	b3ClearLastPlacedText();
	captureFillOutput = 0;
	CHECK(displayedCells == expected * TILE_LAYER_WIDTH &&
		highestFillMapRow - lowestFillMapRow + 1 == expected,
		"Clearing multiline text omitted displayed rows");
}

static void test_multiline_unwrapped_rows(void)
{
	char message[] = "first\nsecond\nthird";
	check_published_text_rows(message, 0, FALSE, 3);
}

static void test_multiline_box_rows(void)
{
	char message[] = "first\nsecond\nthird";
	check_published_text_rows(message, 12, FALSE, 7);
}

static void test_box_preserves_blank_rows(void)
{
	char message[] = "first\n\nthird";
	check_published_text_rows(message, 12, FALSE, 7);
}

static void test_short_wrapped_explicit_rows(void)
{
	char message[] = "a\nb";
	check_published_text_rows(message, 0, TRUE, 2);
}

static void test_auto_center_unboxed_text(void)
{
	char message[] = "abc";
	unsigned int column;
	b3DisplayMessageBox(message, 0, AUTO_CALC_ROW, AUTO_CALC_COLUMN, 0, 0, FALSE, FIRST_TEXT_ROW);
	column = (unsigned int)((displayTextAddressToCopyTo - MAPBASE -
		(FIRST_TEXT_ROW + b3LastBoxStartLine - 1) * TILE_LAYER_BYTES_PER_ROW) / BYTES_PER_CELL);
	CHECK(column == (MAX_CHAR_ACROSS - 3) / 2,
		"Automatically centered unboxed text ignored its content width");
}

static void test_auto_center_explicit_multiline_box(void)
{
	char message[] = "first\nsecond\nthird";
	b3DisplayMessageBox(message, 0, AUTO_CALC_ROW, AUTO_CALC_COLUMN, 0, 12, FALSE, FIRST_TEXT_ROW);
	CHECK(b3LastBoxLines == 7, "Multiline box did not publish its full rendered extent");
	CHECK(b3LastBoxStartLine == (TEXT_ROWS - b3LastBoxLines) / 2,
		"Automatically centered box ignored its final rendered height");
}

static void test_right_edge_clips_unboxed_text(void)
{
	char message[] = "AB";
	b3DisplayMessageBox(message, 0, 0, MAX_CHAR_ACROSS - 1, 0, 0, FALSE, FIRST_TEXT_ROW);
	CHECK(!strcmp(currentTextBuffer, "A"),
		"Text at the right edge was not clipped to the visible screen width");
	CHECK(b3PaletteRows == 1, "Right-edge clipping changed the visible row count");
}

static void test_bottom_edge_clips_unboxed_text(void)
{
	char message[] = "first\nsecond";
	b3DisplayMessageBox(message, 0, TILE_LAYER_HEIGHT - FIRST_TEXT_ROW,
		0, 0, 0, FALSE, FIRST_TEXT_ROW);
	CHECK(!strcmp(currentTextBuffer, "first"),
		"Text below the final tile-map row was not clipped");
	CHECK(b3PaletteRows == 1 && b3LastBoxLines == 1,
		"Bottom-edge clipping published rows outside the tile map");
}

static void test_first_row_controls_map_bounds(void)
{
	char message[] = "bottom";
	const unsigned int bottomRowAddress = MAPBASE + (TILE_LAYER_HEIGHT - 1) * TILE_LAYER_BYTES_PER_ROW;
	displayCalls = 0;
	b3DisplayMessageBox(message, 0, TILE_LAYER_HEIGHT - 1, 0, 0, 0, FALSE, 1);
	CHECK(displayCalls == 1, "First-row origin 1 rejected the last physical tile row");
	CHECK(displayTextAddressToCopyTo == bottomRowAddress,
		"First-row origin 1 did not map logical row 31 to physical row 31");

	displayCalls = 0;
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, TILE_LAYER_HEIGHT);
	CHECK(displayCalls == 1 && displayTextAddressToCopyTo == bottomRowAddress,
		"First-row origin 32 did not map logical row zero to the last physical row");

	displayCalls = 0;
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, 0);
	CHECK(!displayCalls, "A zero first-row origin was accepted");
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, TILE_LAYER_HEIGHT + 1);
	CHECK(!displayCalls, "A first-row origin below the tile map was accepted");
}

static void test_control_bytes_survive_preparation(void)
{
	char message[] = { 'A', '\t', '\r', 1, 'B', '\0' };
	b3DisplayMessageBox(message, 0, 0, 0, 0, 0, FALSE, FIRST_TEXT_ROW);
	CHECK(!memcmp(currentTextBuffer, message, sizeof(message)),
		"Text preparation changed non-newline control bytes");
}

static void test_source_alias_inside_text_buffer(void)
{
	static const char expected[] = "aliased source";
	char* aliasedSource = b3TextBuffer1 + 8;
	memcpy(aliasedSource, expected, sizeof(expected));
	b3DisplayMessageBox(aliasedSource, TEXT_CODE_BANK, 0, 0, 0, 0, FALSE, FIRST_TEXT_ROW);
	CHECK(currentTextBuffer == b3TextBuffer1 && !strcmp(currentTextBuffer, expected),
		"Source alias inside the text buffer was not prepared intact");
}

static void test_rejected_box_preserves_cleanup_extent(void)
{
	char valid[] = "first\nsecond\nthird";
	char invalid[901];
	char* previousTextBuffer;
	unsigned long previousTextAddress;
	long previousPaletteAddress;
	byte previousPaletteRows, previousPaletteNumber;
	byte previousBoxLines, previousBoxStartLine;
	memset(invalid, 'X', sizeof(invalid) - 1);
	invalid[sizeof(invalid) - 1] = '\0';
	displayCalls = 0;
	b3DisplayMessageBox(valid, 0, 1, 0, 2, 12, FALSE, FIRST_TEXT_ROW);
	CHECK(displayCalls == 1 && b3LastBoxLines == 7, "Initial box did not publish its extent");
	previousTextBuffer = currentTextBuffer;
	previousTextAddress = displayTextAddressToCopyTo;
	previousPaletteAddress = b3PaletteAddress;
	previousPaletteRows = b3PaletteRows;
	previousPaletteNumber = b3PaletteNumber;
	previousBoxLines = b3LastBoxLines;
	previousBoxStartLine = b3LastBoxStartLine;
	b3DisplayMessageBox(invalid, 0, 5, 0, 2, 254, FALSE, FIRST_TEXT_ROW);
	CHECK(displayCalls == 1 && currentTextBuffer == previousTextBuffer &&
		displayTextAddressToCopyTo == previousTextAddress &&
		b3PaletteAddress == previousPaletteAddress && b3PaletteRows == previousPaletteRows &&
		b3PaletteNumber == previousPaletteNumber && b3LastBoxLines == previousBoxLines &&
		b3LastBoxStartLine == previousBoxStartLine,
		"Rejected box changed previously published display metadata");
	b3DisplayMessageBox("", 0, 5, 0, 2, 12, FALSE, FIRST_TEXT_ROW);
	CHECK(displayCalls == 1 && currentTextBuffer == previousTextBuffer &&
		displayTextAddressToCopyTo == previousTextAddress &&
		b3PaletteAddress == previousPaletteAddress && b3PaletteRows == previousPaletteRows &&
		b3PaletteNumber == previousPaletteNumber && b3LastBoxLines == previousBoxLines &&
		b3LastBoxStartLine == previousBoxStartLine,
		"Empty message changed previously published display metadata");
	b3DisplayMessageBox(NULL, 0, 5, 0, 2, 12, FALSE, FIRST_TEXT_ROW);
	CHECK(displayCalls == 1 && currentTextBuffer == previousTextBuffer &&
		displayTextAddressToCopyTo == previousTextAddress &&
		b3PaletteAddress == previousPaletteAddress && b3PaletteRows == previousPaletteRows &&
		b3PaletteNumber == previousPaletteNumber && b3LastBoxLines == previousBoxLines &&
		b3LastBoxStartLine == previousBoxStartLine,
		"Null message changed previously published display metadata");
	memset(invalid, NEW_LINE, sizeof(invalid) - 1);
	b3DisplayMessageBox(invalid, 0, 5, 0, 2, 0, FALSE, FIRST_TEXT_ROW);
	CHECK(displayCalls == 1 && currentTextBuffer == previousTextBuffer &&
		displayTextAddressToCopyTo == previousTextAddress &&
		b3PaletteAddress == previousPaletteAddress && b3PaletteRows == previousPaletteRows &&
		b3PaletteNumber == previousPaletteNumber && b3LastBoxLines == previousBoxLines &&
		b3LastBoxStartLine == previousBoxStartLine,
		"Unrepresentable text changed previously published display metadata");
	captureFillOutput = 1;
	displayedCells = displayedNewlines = unexpectedText = fillMapRowsSeen = 0;
	b3ClearLastPlacedText();
	captureFillOutput = 0;
	CHECK(displayedCells == 7 * TILE_LAYER_WIDTH,
		"Cleanup after a rejected box left part of the previous box visible");
}

typedef void (*TextTestFunction)(void);

typedef struct
{
  const char* name;
  TextTestFunction function;
} TextTestCase;

static const TextTestCase textTests[] =
{
	{ "fill_full_layer", test_fill_full_layer },
	{ "fill_rejects_wrapped_row", test_fill_rejects_wrapped_row },
	{ "fill_clear_lines_map_range", test_fill_clear_lines_map_range },
	{ "long_message_is_bounded_and_unchanged", test_long_message_is_bounded_and_unchanged },
	{ "source_length_is_bounded", test_source_length_is_bounded },
	{ "border_rejects_oversized_output", test_border_rejects_oversized_output },
	{ "border_keeps_content_inside_requested_width", test_border_keeps_content_inside_requested_width },
	{ "border_accepts_empty_text", test_border_accepts_empty_text },
	{ "border_preserves_trailing_blank_line", test_border_preserves_trailing_blank_line },
	{ "border_exact_capacity_includes_terminator", test_border_exact_capacity_includes_terminator },
	{ "wrap_width_includes_last_line", test_wrap_width_includes_last_line },
	{ "wrap_newline_without_spaces", test_wrap_newline_without_spaces },
	{ "wrap_width_excludes_replaced_separator", test_wrap_width_excludes_replaced_separator },
	{ "wrapped_box_fits_its_content", test_wrapped_box_fits_its_content },
	{ "multiline_unwrapped_rows", test_multiline_unwrapped_rows },
	{ "multiline_box_rows", test_multiline_box_rows },
	{ "box_preserves_blank_rows", test_box_preserves_blank_rows },
	{ "short_wrapped_explicit_rows", test_short_wrapped_explicit_rows },
	{ "auto_center_unboxed_text", test_auto_center_unboxed_text },
	{ "auto_center_explicit_multiline_box", test_auto_center_explicit_multiline_box },
	{ "right_edge_clips_unboxed_text", test_right_edge_clips_unboxed_text },
	{ "bottom_edge_clips_unboxed_text", test_bottom_edge_clips_unboxed_text },
	{ "first_row_controls_map_bounds", test_first_row_controls_map_bounds },
	{ "control_bytes_survive_preparation", test_control_bytes_survive_preparation },
	{ "source_alias_inside_text_buffer", test_source_alias_inside_text_buffer },
	{ "rejected_box_preserves_cleanup_extent", test_rejected_box_preserves_cleanup_extent },
	{ "text_color_ids", test_text_color_ids },
	{ "text_color_allocation_failure", test_text_color_allocation_failure }
};

static int run_isolated(const TextTestCase* test)
{
	pid_t child;
	int status;
	fflush(stdout);
	child = fork();
	if (child < 0)
	{
		fprintf(stderr, "Could not fork text test %s\n", test->name);
		return 1;
	}
	if (!child)
	{
		test->function();
		_exit(0);
	}
	if (waitpid(child, &status, 0) < 0)
	{
		fprintf(stderr, "Could not wait for text test %s\n", test->name);
		return 1;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	{
		printf("PASS text %s\n", test->name);
		return 0;
	}
	if (WIFSIGNALED(status))
		fprintf(stderr, "FAIL text %s (signal %d)\n", test->name, WTERMSIG(status));
	else if (WIFEXITED(status))
		fprintf(stderr, "FAIL text %s (exit %d)\n", test->name, WEXITSTATUS(status));
	else
		fprintf(stderr, "FAIL text %s\n", test->name);
	return 1;
}

int main(int argc, char** argv)
{
	size_t i;
	unsigned failed = 0;
	if (argc == 2)
	{
		for (i = 0; i < sizeof(textTests) / sizeof(textTests[0]); ++i)
		{
			if (!strcmp(argv[1], textTests[i].name))
				return run_isolated(&textTests[i]);
		}
		fprintf(stderr, "Unknown text test case: %s\n", argv[1]);
		return 2;
	}
	for (i = 0; i < sizeof(textTests) / sizeof(textTests[0]); ++i)
		failed += run_isolated(&textTests[i]);
	if (failed)
		return 1;
	puts("Text buffer and palette preparation checks passed");
	return 0;
}
