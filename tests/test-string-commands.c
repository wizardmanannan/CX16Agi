#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "logic.h"
#include "object.h"

byte RAM_BANK;
byte variables[256];
byte* var = variables;
boolean flags[256];
boolean* flag = flags;
char string[12][40];
char b7WordText[10][80];
int numInputWords;
int inputWords[10];
static byte operands[32];
static unsigned int operandCount;
static char parsed[40];
static byte* messages[2];
extern void b4Parse(void);
extern void b4Set_string(void);
extern void b4Word_to_string(void);
extern void b4Get_num(void);
extern void b4Set_cursor_char(void);
extern boolean b1Said(void);
char cursorChar = '_';
static char allocatedWindow[80];
static unsigned int allocations;

byte* b10BankedAlloc(int length, byte* bank)
{
	assert(length <= sizeof(allocatedWindow));
	allocations++;
	*bank = 24;
	strcpy(allocatedWindow, "wrong bank");
	return (byte*)allocatedWindow;
}

boolean b10BankedDealloc(byte* memory, byte bank)
{
	assert(memory == (byte*)allocatedWindow && bank == 24);
	return TRUE;
}

void b7GetString(char* prompt, byte promptBank, char* result, byte resultBank, int x, int y, int limit)
{
	(void)prompt; (void)promptBank; (void)x; (void)y;
	assert(resultBank == 13);
	assert(limit == 3);
	/* A banked result is not visible through the caller's current window. */
	if (result != allocatedWindow) strcpy(result, "123");
}

byte loadAndIncWinCode(void)
{
	assert(operandCount < sizeof(operands));
	return operands[operandCount++];
}

byte b7GetInputWord(byte number)
{
	assert(number < sizeof(inputWords) / sizeof(inputWords[0]));
	return inputWords[number];
}

void incCodeBy(int jumpAmount)
{
	operandCount += jumpAmount;
	assert(operandCount <= sizeof(operands));
}

void b5GetLogicFile(LOGICFile* result, byte number)
{
	assert(number == 106);
	memset(result, 0, sizeof(*result));
	result->messages = messages;
	result->numMessages = 2;
	result->messageBank = 21;
}

char* b7GetInternalStringPtr(byte number, size_t* length)
{
	assert(operandCount >= 1);
	assert(number < 12);
	*length = strlen(string[number]);
	return string[number];
}

void b7LookupWords(char* text) { strcpy(parsed, text); }

size_t strLenBanked(char* text, int bank)
{
	assert(bank == 21 || bank == STRING_BANK);
	return strlen(text);
}

void memCpyBankedBetween(byte* dst, byte dstBank, byte* src, byte srcBank, size_t length)
{
	assert(dstBank == STRING_BANK && srcBank == 21);
	memcpy(dst, src, length);
}

void* memCpyBanked(byte* dst, byte* src, byte bank, size_t length)
{
	byte previousBank = RAM_BANK;
	void* result;
	if (dst == (byte*)&cursorChar) {
		assert(src == messages[0] && bank == 21 && length == 1);
	}
	else {
		assert(bank == STRING_BANK);
	}
	RAM_BANK = bank;
	result = memcpy(dst, src, length);
	RAM_BANK = previousBank;
	return result;
}

static void test_set_string(void)
{
	char longMessage[80];
	messages[0] = (byte*)"Gwydion";
	operands[0] = 0;
	operands[1] = 1;
	operandCount = 0;
	b4Set_string();
	assert(strcmp(string[0], "Gwydion") == 0);
	memset(longMessage, 'x', sizeof(longMessage) - 1);
	longMessage[79] = '\0';
	messages[0] = (byte*)longMessage;
	strcpy(string[1], "guard");
	operandCount = 0;
	b4Set_string();
	assert(strlen(string[0]) == 39);
	assert(strcmp(string[1], "guard") == 0);
}

static void test_word_to_string(void)
{
	strcpy(b7WordText[0], "look");
	strcpy(string[0], "old suffix");
	numInputWords = 1;
	operands[0] = operands[1] = 0;
	operandCount = 0;
	b4Word_to_string();
	assert(strcmp(string[0], "look") == 0);
	assert(strcmp(b7WordText[0], "look") == 0);
}

static void test_numeric_input(void)
{
	messages[0] = (byte*)"Number:";
	operands[0] = 1;
	operands[1] = 5;
	operandCount = 0;
	RAM_BANK = 13;
	b4Get_num();
	assert(variables[5] == 123);
	assert(RAM_BANK == 13);
	assert(allocations == 0);
}

static void test_set_cursor_char(void)
{
	messages[0] = (byte*)">";
	operands[0] = 1;
	operandCount = 0;
	cursorChar = '_';
	RAM_BANK = 13;
	b4Set_cursor_char();
	assert(operandCount == 1);
	assert(cursorChar == '>');
	assert(RAM_BANK == 13);

	messages[0] = (byte*)"";
	operands[0] = 1;
	operandCount = 0;
	cursorChar = '?';
	b4Set_cursor_char();
	assert(cursorChar == '?');
	assert(RAM_BANK == 13);
}

static void set_said_argument(unsigned int index, int value)
{
	operands[1 + index * 2] = value & 0xff;
	operands[2 + index * 2] = value >> 8;
}

static void test_said_rejects_excess_arguments(void)
{
	int i;
	operands[0] = 11;
	for (i = 0; i < 10; i++) inputWords[i] = i + 2;
	for (i = 0; i < 11; i++) set_said_argument(i, i + 2);
	numInputWords = 10;
	flag[2] = TRUE;
	flag[4] = FALSE;
	operandCount = 0;
	assert(!b1Said());
	assert(operandCount == 23);
	assert(numInputWords == 10 && flag[2] == TRUE && flag[4] == FALSE);
}

static void test_said_trailing_wildcard(void)
{
	operands[0] = 2;
	set_said_argument(0, 42);
	set_said_argument(1, 9999);
	inputWords[0] = 42;
	numInputWords = 10;
	flag[2] = TRUE;
	flag[4] = FALSE;
	operandCount = 0;
	assert(b1Said());
	assert(operandCount == 5);
	assert(numInputWords == 0 && flag[2] == FALSE && flag[4] == TRUE);
}

int main(void)
{
	currentLog = 106;
	strcpy(string[0], "look");
	operands[0] = 0;
	b4Parse();
	assert(operandCount == 1);
	assert(strcmp(parsed, "look") == 0);
	test_set_string();
	test_word_to_string();
	test_numeric_input();
	test_set_cursor_char();
	test_said_rejects_excess_arguments();
	test_said_trailing_wildcard();
	puts("string command tests passed");
	return 0;
}
