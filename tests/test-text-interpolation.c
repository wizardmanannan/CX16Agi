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
extern char b7OutputString[80];
extern byte b3WrapText(char* text, int width, byte* maxWidth);
extern void b3DrawBorder(byte width, size_t messageSize);
int currentLog = 106;
objectType bDObjects[MAX_OBJECTS];
int bDNumObjects;
static byte* messages[26];
static unsigned int displays;
static unsigned int clearDisplays;
static boolean boxed;
static boolean observingClear;
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
	byte maxWidth;
	assert(b3WrapText(hardBreak, 8, &maxWidth) == 2);
	assert(strcmp(hardBreak, "abc\ndef ghi") == 0);
	assert(b3WrapText(hardBreakAfterSpace, 8, &maxWidth) == 2);
	assert(strcmp(hardBreakAfterSpace, "ab cd\nef gh ij") == 0);
}

static void test_border_capacity(void)
{
	size_t i;
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
	size_t i;
	size_t expectedRows = TEXTBUFFER_SIZE / TILE_LAYER_WIDTH;
	size_t expectedSize = expectedRows * TILE_LAYER_WIDTH;
	memset(b3TextBuffer1, 'x', TEXTBUFFER_SIZE - 1);
	for (i = 1; i < TEXTBUFFER_SIZE - 1; i += 2) b3TextBuffer1[i] = NEW_LINE;
	b3TextBuffer1[TEXTBUFFER_SIZE - 1] = '\0';
	memset(b3TextBuffer2, 0x5a, sizeof(b3TextBuffer2));
	b3DrawBorder(TILE_LAYER_WIDTH - 2, TEXTBUFFER_SIZE);
	assert(b3LastBoxLines == expectedRows);
	assert(b3LastBoxLines <= TEXT_ROWS);
	assert(strlen(b3TextBuffer2) == expectedSize - 1);
	assert(b3TextBuffer2[expectedSize - 1] == '\0');
	assert((byte)b3TextBuffer2[expectedSize] == 0x5a);
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
	test_empty_message_does_not_clear(warning);
	test_idle_input();
	test_input_capacity();
	test_banked_input();
	test_word_capacity();
	test_word_lookup_uses_cleaned_input();
	puts("text interpolation tests passed");
	return 0;
}
