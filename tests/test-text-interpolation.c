#include <assert.h>
#include <stdio.h>
#include <string.h>
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
static boolean boxed;
static const char expected[] = "\"You're up to no good, Gwydion\" Manannan snarls through clenched teeth. \"Never again will you discover my secrets!\"";

void b5GetLogicFile(LOGICFile* result, byte logicNo)
{
	assert(logicNo == 106);
	memset(result, 0, sizeof(*result));
	result->messages = messages;
	result->numMessages = 26;
	result->messageBank = 21;
}

char* getMessagePointer(byte logicNo, byte messageNo)
{
	assert(logicNo == 106 && messageNo < 26);
	assert(messages[messageNo] != NULL);
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

void b6SetAndWaitForIrqState(IRQ_COMMAND state)
{
	assert(state == DISPLAY_TEXT);
	displays++;
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
	memset(b3TextBuffer1, 'x', TEXTBUFFER_SIZE - 1);
	b3TextBuffer1[TEXTBUFFER_SIZE - 1] = '\0';
	b3DrawBorder(30, TEXTBUFFER_SIZE);
	assert(memchr(b3TextBuffer2, '\0', TEXTBUFFER_SIZE) != NULL);
	strcpy(b3TextBuffer1, "\n\n");
	b3DrawBorder(30, 3);
	assert(memchr(b3TextBuffer2, '\0', TEXTBUFFER_SIZE) != NULL);
}

int main(void)
{
	/* KQ3 logic 106, message 26; the IRQ stub inspects the actual display buffer. */
	char warning[] = "\"You're up to no good, %m6\" %m7 snarls through clenched teeth. \"Never again will you discover my secrets!\"";
	char prompt[128];
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
	b7GetString("%m26", 21, prompt, 2, 1, 1, 39);
	assert(memchr(b7OutputString, '\0', sizeof(b7OutputString)) != NULL);
	assert(strlen(b7OutputString) == sizeof(b7OutputString) - 1);
	assert(strncmp(b7OutputString, expected, sizeof(b7OutputString) - 1) == 0);
	test_explicit_line_breaks();
	test_border_capacity();
	puts("text interpolation tests passed");
	return 0;
}
