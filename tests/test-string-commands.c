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
static byte operands[8];
static unsigned int operandCount;
static char parsed[40];
static byte* messages[2];
extern void b4Parse(void);
extern void b4Set_string(void);
extern void b4Word_to_string(void);

byte loadAndIncWinCode(void) { return operands[operandCount++]; }

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
	assert(bank == 21);
	return strlen(text);
}

void memCpyBankedBetween(byte* dst, byte dstBank, byte* src, byte srcBank, size_t length)
{
	assert(dstBank == STRING_BANK && srcBank == 21);
	memcpy(dst, src, length);
}

void* memCpyBanked(byte* dst, byte* src, byte bank, size_t length)
{
	assert(bank == STRING_BANK);
	return memcpy(dst, src, length);
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

int main(void)
{
	currentLog = 106;
	strcpy(string[0], "look");
	operands[0] = 0;
	b4Parse();
	assert(operandCount == 1);
	assert(strcmp(parsed, "look") == 0);
	test_set_string();
	puts("string command tests passed");
	return 0;
}
