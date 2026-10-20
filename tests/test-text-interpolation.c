#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "textLayer.h"
#include "logic.h"
#include "object.h"
#include "parser.h"

byte RAM_BANK;
unsigned long displayTextAddressToCopyTo;
char* currentTextBuffer;
long b3PaletteAddress;
byte b3PaletteRows, b3PaletteNumber;
byte variables[256];
byte* var = variables;
boolean flagValues[256];
boolean* flag = flagValues;
byte keyState[256];
byte* key = keyState;
byte _assmByte;
extern char b7OutputString[80], b7Temp[256], b7CurrentInputStr[41], cursorChar;
extern void b7RefreshInputLine(boolean erasePrevious);
extern boolean b7ReadingString, b7InputReady;
extern byte b7InputRow, b7InputColumn;
static boolean observingStringInput;
extern byte b3WrapText(char* text, int width, byte* maxWidth);
extern void b3DrawBorder(byte width, size_t messageSize);
int currentLog = 106;
int screenMode;
int user_input_line = 23, controlMode = 1, dirnOfEgo;
boolean inputLineDisplayed;
void b1ResetControllers(void) {}
boolean b1SetControllerByPetscii(byte ch) { (void)ch; return FALSE; }
boolean b5IsDebuggingEnabled(void) { return FALSE; }
void bDbgShowPriority(void) {}
void trampolineDebug(void (*function)(void)) { function(); }
void getViewTab(ViewTable* view, byte index) { (void)index; memset(view, 0, sizeof(*view)); }
void setViewTab(ViewTable* view, byte index) { (void)view; (void)index; }
objectType bDObjects[MAX_OBJECTS];
int bDNumObjects;
static byte* messages[26];
static unsigned int displays;
static unsigned int clearDisplays;
static boolean boxed;
static boolean observingClear;
static const char* controlExpected;
static boolean observingFill;
static unsigned int fillRows, fillChunks, expectedFillRow;
static byte expectedFirstRow;
static boolean observingClip;
static const byte* inputKeys;
static size_t inputKeyCount, inputKeyPosition;
static char bankWindow[80], bank24[80];
static const char expected[] = "\"You're up to no good, Gwydion\" Manannan snarls through clenched teeth. \"Never again will you discover my secrets!\"";

void native_asm(const char* instruction, ...)
{
	va_list args;
	if (strcmp(instruction, "jsr %w") != 0) return;
	va_start(args, instruction);
	if (va_arg(args, int) == GETIN_ADDRESS) {
        if (observingStringInput) {
            assert(b7ReadingString && b7InputReady);
            assert(b7InputRow == 2 && b7InputColumn == 10);
            assert(strncmp(b7OutputString, "Code 7:", 7) == 0);
        }
		assert(inputKeyPosition < inputKeyCount);
		_assmByte = inputKeys[inputKeyPosition++];
	}
	va_end(args);
}

static void test_idle_input(void)
{
	static const byte keys[] = { 0, 'h', 'i', KEY_ENTER };
	char result[40] = "unchanged";
	inputKeys = keys;
	inputKeyCount = sizeof(keys);
	inputKeyPosition = 0;
	b7GetString("Name:", 21, result, STRING_BANK, 1, 1, 39);
	assert(strcmp(result, "hi") == 0);
	assert(inputKeyPosition == sizeof(keys));
}

static void test_modal_keyboard_encoding(void)
{
    static const byte keys[] = { KEY_LOWER_A, KEY_CAP_A, KEY_RIGHT, KEY_F1, KEY_ENTER };
    char result[40] = "unchanged";
    inputKeys = keys;
    inputKeyCount = sizeof(keys);
    inputKeyPosition = 0;
    b7GetString("Name:", 21, result, STRING_BANK, 1, 1, 39);
    assert(strcmp(result, "aA") == 0);
}


static void test_modal_cancel(void)
{
    static const byte keys[] = { '1', KEY_ESC };
    char result[40] = "unchanged";
    strcpy(b7CurrentInputStr, "previous");
    inputKeys = keys;
    inputKeyCount = sizeof(keys);
    inputKeyPosition = 0;
    b7GetString("Name:", 21, result, STRING_BANK, 1, 1, 39);
    assert(result[0] == 0);
    assert(strcmp(b7CurrentInputStr, "previous") == 0);
    assert(!b7ReadingString && !b7InputReady);
}


static void test_string_input_publication(void)
{
    static const byte keys[] = { 0, '4', '2', KEY_ENTER };
    char result[40];
    strcpy(b7CurrentInputStr, "unfinished");
    variables[1] = 7;
    inputKeys = keys;
    inputKeyCount = sizeof(keys);
    inputKeyPosition = 0;
    observingStringInput = TRUE;
    b7GetString("Code %v1:", 21, result, STRING_BANK, 10, 2, 39);
    observingStringInput = FALSE;
    assert(strcmp(result, "42") == 0);
    assert(strcmp(b7CurrentInputStr, "unfinished") == 0);
    assert(!b7ReadingString && !b7InputReady);
}

static void test_keyboard_zero_digit(void)
{
    static const byte keys[] = { '1', '0', '0', 0 };
    b7InitEvents();
    assert(strcmp(string[0], ">") == 0);
    strcpy(string[0], "Cmd%v2:");
    variables[2] = 4;
    b7CurrentInputStr[0] = 0;
    inputLineDisplayed = TRUE;
    inputKeys = keys;
    inputKeyCount = sizeof(keys);
    inputKeyPosition = 0;
    b7PollKeyboard();
    assert(strcmp(b7CurrentInputStr, "100") == 0);
    assert(strcmp(b7OutputString, "Cmd4:100_") == 0 && b7InputReady);
    assert(inputKeyPosition == sizeof(keys));
    inputLineDisplayed = FALSE;
}

static void test_get_string_backspace(void)
{
    static const byte keys[][4] = {
        { 'a', 'b', KEY_BACKSPACE, KEY_ENTER },
        { 'a', 'b', 0x08, KEY_ENTER }
    };
    unsigned int i;
    char result[40];
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        inputKeys = keys[i];
        inputKeyCount = sizeof(keys[i]);
        inputKeyPosition = 0;
        b7GetString(">", 21, result, STRING_BANK, 1, 1, 39);
        assert(strcmp(result, "a") == 0);
        assert(strcmp(b7OutputString, ">a_ ") == 0);
    }
}

static void test_input_line_composition(void)
{
    static const size_t promptLengths[] = { 0, 37, 38, 39, 79, 255 };
    size_t i;
    boolean erase;
    char expected[80], savedCursor = cursorChar;
    cursorChar = '?';
    memset(b7CurrentInputStr, 'x', 40);
    b7CurrentInputStr[40] = 0;
    for (i = 0; i < sizeof(promptLengths) / sizeof(promptLengths[0]); i++) {
        memset(b7Temp, 'p', promptLengths[i]);
        b7Temp[promptLengths[i]] = 0;
        for (erase = FALSE; erase <= TRUE; erase++) {
            /* The prior snprintf contract is an independent output oracle. */
            snprintf(expected, sizeof(expected), "%s%s%c%s", b7Temp, b7CurrentInputStr, cursorChar, erase ? " " : "");
            b7RefreshInputLine(erase);
            assert(strcmp(b7OutputString, expected) == 0);
        }
    }
    cursorChar = savedCursor;
}

static void test_input_capacity(void)
{
	byte keys[51];
	memset(keys, 'x', 50);
	keys[50] = KEY_ENTER;
	strcpy(string[1], "guard");
	inputKeys = keys;
	inputKeyCount = sizeof(keys);
	inputKeyPosition = 0;
	b7GetInternalString("Name:", 21, 0, 1, 1, 255);
	assert(strlen(string[0]) == sizeof(string[0]) - 1);
	assert(strcmp(string[1], "guard") == 0);
	assert(inputKeyPosition == sizeof(keys));
}

static void test_banked_input(void)
{
	static const byte keys[] = { '4', '2', KEY_ENTER };
	strcpy(bankWindow, "wrong bank");
	strcpy(bank24, "unchanged");
	inputKeys = keys;
	inputKeyCount = sizeof(keys);
	inputKeyPosition = 0;
	b7GetString("Number:", 21, bankWindow, 24, 1, 1, 3);
	assert(strcmp(bank24, "42") == 0);
	assert(strcmp(bankWindow, "wrong bank") == 0);
}

int b12FindSynonymNum(char* word, byte bank)
{
	size_t length = strlen(word);
	assert(bank == PARSER_BANK);
	while (length && word[length - 1] == ' ') length--;
	return length == 2 && word[0] == 'g' && word[1] == 'o' ? 1 : -1;
}

static void test_word_capacity(void)
{
	char input[] = "go go go go go go go go go go go go go";
	b7LookupWords(input);
	assert(numInputWords == 10);
	assert(variables[9] == 11);
	assert(strncmp(b7WordText[9], "go", 2) == 0);
}

static void test_word_lookup_uses_cleaned_input(void)
{
	char punctuated[] = "go!";
	char separated[] = "go,  go!";

	numInputWords = 0;
	b7LookupWords(punctuated);
	assert(numInputWords == 1 && inputWords[0] == 1);
	assert(strcmp(b7WordText[0], "go") == 0);
	assert(strcmp(punctuated, "go!") == 0);

	numInputWords = 0;
	b7LookupWords(separated);
	assert(numInputWords == 2 && inputWords[0] == 1 && inputWords[1] == 1);
	assert(strncmp(b7WordText[0], "go", 2) == 0 && strncmp(b7WordText[1], "go", 2) == 0);
	assert(strcmp(separated, "go,  go!") == 0);
}

void b5GetLogicFile(LOGICFile* result, byte logicNo)
{
	assert(logicNo == 106 || logicNo == 0);
	memset(result, 0, sizeof(*result));
	result->messages = messages;
	result->numMessages = logicNo == 0 ? 0 : 26;
	result->messageBank = 21;
}

char* getMessagePointer(byte logicNo, byte messageNo)
{
	assert(logicNo == 106 && messageNo < 26);
	return (char*)messages[messageNo];
}

size_t strLenBanked(char* text, int bank)
{
	(void)bank;
	return strlen(text);
}

void* memCpyBanked(byte* dst, byte* src, byte bank, size_t length)
{
	byte previousBank = RAM_BANK;
	void* result;
	RAM_BANK = bank;
	result = memcpy(dst, src, length);
	RAM_BANK = previousBank;
	return result;
}

void memCpyBankedBetween(byte* dst, byte dstBank, byte* src, byte srcBank, size_t length)
{
	assert(srcBank == STRING_BANK);
	if (dst == (byte*)bankWindow) {
		assert(dstBank == 24 && length <= sizeof(bank24));
		dst = (byte*)bank24;
	}
	memcpy(dst, src, length);
}

void b6SetAndWaitForIrqState(IRQ_COMMAND state)
{
	assert(state == DISPLAY_TEXT);
	displays++;
	if (observingClip) {
		assert(displayTextAddressToCopyTo == MAPBASE +
		       (TILE_LAYER_HEIGHT - 1) * TILE_LAYER_BYTES_PER_ROW +
		       (TILE_LAYER_WIDTH - 2) * BYTES_PER_CELL);
		assert(strcmp(currentTextBuffer, "ab") == 0);
		assert(b3PaletteRows == 1 && b3LastBoxLines == 1);
		return;
	}
	if (observingFill) {
		size_t size = strlen(currentTextBuffer);
		unsigned int rows = (size + 1) / (TILE_LAYER_WIDTH + 1);
		assert(currentTextBuffer == b3TextBuffer1);
		assert(size < TEXTBUFFER_SIZE);
		assert(rows && size == rows * (TILE_LAYER_WIDTH + 1) - 1);
		assert(displayTextAddressToCopyTo == MAPBASE +
		       (expectedFirstRow + expectedFillRow - 1) * TILE_LAYER_BYTES_PER_ROW);
		fillRows += rows;
		expectedFillRow += rows;
		fillChunks++;
		return;
	}
	if (controlExpected) {
		assert(strcmp(currentTextBuffer, controlExpected) == 0);
		return;
	}
	if (observingClear) {
		clearDisplays++;
		return;
	}
	if (boxed) {
		assert(currentTextBuffer == b3TextBuffer2);
		assert(strstr(currentTextBuffer, "Gwydion") != NULL);
		assert(strstr(currentTextBuffer, "Manannan") != NULL);
		assert(strstr(currentTextBuffer, "%m") == NULL);
		return;
	}
	if (strcmp(b3TextBuffer1, expected) != 0) {
		fprintf(stderr, "wizard dialogue: expected [%s], got [%s]\n", expected, b3TextBuffer1);
		assert(0);
	}
}

static void test_explicit_line_breaks(void)
{
	char hardBreak[] = "abc\ndef ghi";
	char hardBreakAfterSpace[] = "ab cd\nef gh ij";
	char crossing[263];
	byte maxWidth;
	assert(b3WrapText(hardBreak, 8, &maxWidth) == 2);
	assert(strcmp(hardBreak, "abc\ndef ghi") == 0);
	assert(b3WrapText(hardBreakAfterSpace, 8, &maxWidth) == 2);
	assert(strcmp(hardBreakAfterSpace, "ab cd\nef gh ij") == 0);
    memset(crossing, 'x', 260);
    strcpy(crossing + 260, " z");
    assert(b3WrapText(crossing, 255, &maxWidth) == 2);
    assert(crossing[260] == '\n' && crossing[261] == 'z');
    assert(maxWidth == 5); /* Existing byte-sized width across a page. */
    strcpy(crossing, "a b");
    assert(b3WrapText(crossing, -1, &maxWidth) == 2);
    assert(strcmp(crossing, "a\nb") == 0);
}

static void test_kq3_clock_display(void)
{
    boolean wasBoxed = boxed;
    boxed = FALSE;
    variables[117] = 1;
    variables[116] = 2;
    variables[115] = 3;
    controlExpected = "1:02:03 ";
    b3DisplayMessageBox("%v117:%v116|2:%v115|2 ", 21, 2, 20, 1, 0, FALSE, FIRST_TEXT_ROW);
    controlExpected = NULL;
    boxed = wasBoxed;
}

static void test_display_buffer_address_in_another_bank(void)
{
    boolean wasBoxed = boxed;
    unsigned int priorDisplays = displays;
    boxed = FALSE;
    variables[117] = 1;
    /* A banked message can have the same address as the staging buffer. */
    strcpy(b3TextBuffer1, "%v117");
    controlExpected = "1";
    b3DisplayMessageBox(b3TextBuffer1, 21, 2, 20, 1, 0, FALSE, FIRST_TEXT_ROW);
    assert(displays == priorDisplays + 1);
    /* Text deliberately staged in the display bank remains literal. */
    strcpy(b3TextBuffer1, "%v117");
    controlExpected = "%v117";
    b3DisplayMessageBox(b3TextBuffer1, TEXT_CODE_BANK, 2, 20, 1, 0, FALSE, FIRST_TEXT_ROW);
    assert(displays == priorDisplays + 2);
    controlExpected = NULL;
    boxed = wasBoxed;
}

static void test_text_mode_controls(void)
{
	boolean wasBoxed = boxed;
	boxed = FALSE;
	screenMode = AGI_TEXT;
	controlExpected = "AC";
	b3DisplayMessageBox("AB\bC", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = "A ";
	b3DisplayMessageBox("AB\b", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = "CB";
	b3DisplayMessageBox("AB\rC", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = "A\nB";
	b3DisplayMessageBox("A\nB", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = "CB\nD";
	b3DisplayMessageBox("AB\rC\nD", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = "A\nB";
	b3DisplayMessageBox("A\n\bB", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = "B";
	b3DisplayMessageBox("\b\bB", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	screenMode = AGI_GRAPHICS;
	controlExpected = "AB\rC";
	b3DisplayMessageBox("AB\rC", 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	controlExpected = NULL;
	boxed = wasBoxed;
}

static void test_border_capacity(void)
{
	size_t i;
    strcpy(b3TextBuffer1, "a");
    b3DrawBorder(0, 1);
    assert(b3LastBoxLines == 5 && strlen(b3TextBuffer2) == 19);
    b3DrawBorder(255, 1);
    assert(b3LastBoxLines == 5 && strlen(b3TextBuffer2) == 319);
	memset(b3TextBuffer1, 'x', TEXTBUFFER_SIZE - 1);
	b3TextBuffer1[TEXTBUFFER_SIZE - 1] = '\0';
	b3DrawBorder(30, TEXTBUFFER_SIZE);
	assert(memchr(b3TextBuffer2, '\0', TEXTBUFFER_SIZE) != NULL);
	for (i = 0; i < TEXTBUFFER_SIZE - 1; i++) b3TextBuffer1[i] = i % 2 ? '\n' : 'x';
	b3TextBuffer1[TEXTBUFFER_SIZE - 1] = '\0';
	b3DrawBorder(30, TEXTBUFFER_SIZE);
	assert(b3LastBoxLines <= TEXT_ROWS);
	strcpy(b3TextBuffer1, "\n\n");
	b3DrawBorder(30, 3);
	assert(memchr(b3TextBuffer2, '\0', TEXTBUFFER_SIZE) != NULL);
}

static void test_border_width_capacity(void)
{
    static const byte widths[] = { 48, 49, TILE_LAYER_WIDTH - 2 };
    size_t i, n, expectedRows, expectedSize;
    memset(b3TextBuffer1, 'x', TEXTBUFFER_SIZE - 1);
    for (i = 1; i < TEXTBUFFER_SIZE - 1; i += 2) b3TextBuffer1[i] = NEW_LINE;
    b3TextBuffer1[TEXTBUFFER_SIZE - 1] = '\0';
    for (n = 0; n < sizeof(widths); n++) {
        expectedRows = TEXTBUFFER_SIZE / (widths[n] + 2);
        if (expectedRows > TEXT_ROWS) expectedRows = TEXT_ROWS;
        expectedSize = expectedRows * (widths[n] + 2);
        memset(b3TextBuffer2, 0x5a, TEXTBUFFER_SIZE);
        b3DrawBorder(widths[n], TEXTBUFFER_SIZE);
        assert(b3LastBoxLines == expectedRows);
        assert(strlen(b3TextBuffer2) == expectedSize - 1);
        assert(b3TextBuffer2[expectedSize - 1] == '\0');
        if (expectedSize < TEXTBUFFER_SIZE)
            assert((byte)b3TextBuffer2[expectedSize] == 0x5a);
    }
}

static void test_border_preserves_empty_lines(void)
{
	strcpy(b3TextBuffer1, "a\n\nb\n");
	b3DrawBorder(8, 7);
	assert(b3LastBoxLines == 8);
	assert(strlen(b3TextBuffer2) == 79);
	assert(b3TextBuffer2[21] == 'a');
	assert(b3TextBuffer2[31] == SPACE);
	assert(b3TextBuffer2[41] == 'b');
	assert(b3TextBuffer2[51] == SPACE);
	assert(strcmp(b3TextBuffer1, "a\n\nb\n") == 0);
}

static void test_cleanup_capacity(void)
{
	memset(b3TextBuffer2, 0x5a, TEXTBUFFER_SIZE);
	observingFill = TRUE;
	fillRows = fillChunks = expectedFillRow = 0;
	expectedFirstRow = FIRST_TEXT_ROW;
	b3FillChar(0, 255, TEXTBOX_PALETTE_NUMBER, TRANSPARENT_CHAR);
	observingFill = FALSE;
	assert(fillRows == TILE_LAYER_HEIGHT - FIRST_TEXT_ROW + 1);
	assert(fillChunks == 2);
	assert((byte)b3TextBuffer2[0] == 0x5a);
	assert((byte)b3TextBuffer2[TEXTBUFFER_SIZE - 1] == 0x5a);
	observingFill = TRUE;
	b3FillChar(255, 255, TEXTBOX_PALETTE_NUMBER, TRANSPARENT_CHAR);
	b3FillChar(10, 4, TEXTBOX_PALETTE_NUMBER, TRANSPARENT_CHAR);
	assert(fillChunks == 2);
	observingFill = FALSE;
	controlExpected = "x";
	b3DisplayMessageBox("x", 21, 3, 1, 1, 0, FALSE, 2);
	controlExpected = NULL;
	fillRows = fillChunks = 0;
	expectedFillRow = 3;
	expectedFirstRow = 2;
	observingFill = TRUE;
	b3ClearLastPlacedText();
	assert(fillRows == 1 && fillChunks == 1);
	assert(b3LastBoxLines == 0 && b3LastBoxStartLine == 3);
	b3ClearLastPlacedText();
	assert(fillChunks == 1);
	observingFill = FALSE;
}

static void test_display_map_bounds(void)
{
	unsigned int priorDisplays;
	observingClip = TRUE;
	b3DisplayMessageBox("abcd\ne", 21, TILE_LAYER_HEIGHT - FIRST_TEXT_ROW,
	                    TILE_LAYER_WIDTH - 2, 1, 0, FALSE, FIRST_TEXT_ROW);
	observingClip = FALSE;
	priorDisplays = displays;
	b3DisplayMessageBox("outside", 21, 254, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	b3DisplayMessageBox("outside", 21, 1, 254, 1, 0, FALSE, FIRST_TEXT_ROW);
	assert(displays == priorDisplays);
	assert(b3LastBoxLines == 0);
}

static void test_long_unboxed_text(void)
{
	char text[1201];
	memset(text, 'x', sizeof(text) - 1);
	text[sizeof(text) - 1] = 0;
	controlExpected = text;
	b3DisplayMessageBox(text, 21, 1, 0, 1, 0, FALSE, FIRST_TEXT_ROW);
	assert(b3LastBoxLines == 19);
	controlExpected = NULL;
}

static void test_empty_message_does_not_clear(char* priorText)
{
	boolean wasBoxed = boxed;
	size_t clearCount = clearDisplays;
	boxed = FALSE;
	b3DisplayMessageBox(priorText, 21, 7, 1, TEXTBOX_PALETTE_NUMBER, 0, FALSE, FIRST_TEXT_ROW);
	boxed = wasBoxed;
	assert(b3LastBoxStartLine == 7);

	variables[0] = 0;
	b3DisplayMessageBox("%s0", 21, 18, 1, TEXTBOX_PALETTE_NUMBER,
	                    DEFAULT_BOX_WIDTH, TRUE, FIRST_TEXT_ROW);
	observingClear = TRUE;
	b3ClearLastPlacedText();
	observingClear = FALSE;
	assert(clearDisplays == clearCount);
	assert(b3LastBoxLines == 0);
	assert(b3LastBoxStartLine == 7);

	b3DisplayMessageBox("", 21, 18, 1, TEXTBOX_PALETTE_NUMBER,
	                    DEFAULT_BOX_WIDTH, TRUE, FIRST_TEXT_ROW);
	observingClear = TRUE;
	b3ClearLastPlacedText();
	observingClear = FALSE;
	assert(clearDisplays == clearCount);
	assert(b3LastBoxLines == 0);
	assert(b3LastBoxStartLine == 7);
}

int main(void)
{
	/* KQ3 logic 106, message 26; the IRQ stub inspects the actual display buffer. */
	char warning[] = "\"You're up to no good, %m6\" %m7 snarls through clenched teeth. \"Never again will you discover my secrets!\"";
	char prompt[128];
	static const byte submit[] = { KEY_ENTER };
	messages[5] = (byte*)"Gwydion";
	messages[6] = (byte*)"Manannan";
	messages[25] = (byte*)warning;
	RAM_BANK = 13;
	b3DisplayMessageBox("%s0", 21, 1, 1, 1, DEFAULT_BOX_WIDTH, TRUE, FIRST_TEXT_ROW);
	assert(displays == 0);
	assert(RAM_BANK == 13);
	b3DisplayMessageBox(warning, 21, 1, 1, 1, 0, FALSE, FIRST_TEXT_ROW);
	assert(displays == 1);
	assert(RAM_BANK == 13);
	boxed = TRUE;
	b3DisplayMessageBox(warning, 21, AUTO_CALC_ROW, AUTO_CALC_COLUMN,
	                   TEXTBOX_PALETTE_NUMBER, DEFAULT_BOX_WIDTH, TRUE, FIRST_TEXT_ROW);
	assert(displays == 2);
	assert(RAM_BANK == 13);
	assert(strcmp(warning, "\"You're up to no good, %m6\" %m7 snarls through clenched teeth. \"Never again will you discover my secrets!\"") == 0);
	b7ProcessString("%m26", 21, prompt, 2, sizeof(prompt));
	assert(strcmp(prompt, expected) == 0);
	assert(RAM_BANK == 13);
	/* An expanded prompt may exceed the parser's 80-byte display line. */
	inputKeys = submit;
	inputKeyCount = sizeof(submit);
	inputKeyPosition = 0;
	b7GetString("%m26", 21, prompt, 2, 1, 1, 39);
	assert(memchr(b7OutputString, '\0', sizeof(b7OutputString)) != NULL);
	assert(strlen(b7OutputString) == sizeof(b7OutputString) - 1);
	assert(strncmp(b7OutputString, expected, sizeof(b7OutputString) - 1) == 0);
	test_explicit_line_breaks();
	test_border_capacity();
	test_border_width_capacity();
	test_border_preserves_empty_lines();
	test_empty_message_does_not_clear(warning);
	test_idle_input();
    test_modal_keyboard_encoding();
    test_modal_cancel();
    test_get_string_backspace();
    test_input_line_composition();
	test_input_capacity();
	test_banked_input();
	test_word_capacity();
	test_word_lookup_uses_cleaned_input();
    test_kq3_clock_display();
    test_display_buffer_address_in_another_bank();
	test_text_mode_controls();
	test_cleanup_capacity();
	test_display_map_bounds();
	test_long_unboxed_text();
    test_keyboard_zero_digit();
    test_string_input_publication();
	puts("text interpolation tests passed");
	return 0;
}
