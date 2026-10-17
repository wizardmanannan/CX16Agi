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

int main(void)
{
	currentLog = 106;
	strcpy(string[0], "look");
	operands[0] = 0;
	b4Parse();
	assert(operandCount == 1);
	assert(strcmp(parsed, "look") == 0);
	puts("string command tests passed");
	return 0;
}
