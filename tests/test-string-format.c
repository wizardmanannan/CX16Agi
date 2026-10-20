#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <cx16.h>
#include "helpers.h"
#include "logic.h"
#include "object.h"
#include "stringFormat.h"

#define TEST_BANK 1
#define TEST_OUTPUT_BANK 2
#define TEST_LOGICS 2
#define TEST_MESSAGES 4

byte RAM_BANK;
byte variables[256];
byte* var = variables;
int numInputWords;
char b7WordText[10][80];
char string[12][40];
int currentLog;
objectType bDObjects[MAX_OBJECTS];
int bDNumObjects;

static LOGICFile logicFiles[TEST_LOGICS];
static byte* messagePointers[TEST_LOGICS][TEST_MESSAGES];
static char* messageText[TEST_LOGICS][TEST_MESSAGES];
static char messageStorage[TEST_LOGICS][TEST_MESSAGES][80];
static char objectName[40];
static char output[256];
static unsigned long bankedCopyCalls;
void* memCpyBanked(byte* dst, byte* src, byte bank, size_t len)
{
	byte previousBank;
	void* result;
	previousBank = RAM_BANK;
	RAM_BANK = bank;
	bankedCopyCalls++;
	result = memcpy(dst, src, len);
	RAM_BANK = previousBank;
	return result;
}

void b5GetLogicFile(LOGICFile* result, byte logicNo)
{
	assert(logicNo < TEST_LOGICS);
	*result = logicFiles[logicNo];
}

char* getMessagePointer(byte logicNo, byte messageNo)
{
	assert(logicNo < TEST_LOGICS);
	assert(messageNo < TEST_MESSAGES);
	return messageText[logicNo][messageNo];
}

static void expect(const char* src, const char* expected)
{
	memset(output, 0x5a, sizeof(output));
	expandFormatCodes(src, TEST_BANK, output, TEST_OUTPUT_BANK, sizeof(output));
	assert(strcmp(output, expected) == 0);
}

static void test_tokens_and_escapes(void)
{
	variables[0] = 0;
	variables[1] = 42;
	variables[2] = 255;
	strcpy(bDObjects[0].name, "key");
	bDNumObjects = 1;
	strcpy(b7WordText[0], "look");
	numInputWords = 1;
	strcpy(string[1], "inner %v1");
	strcpy(messageText[0][0], "Global");
	strcpy(messageText[1][1], "42 later");

	RAM_BANK = 13;
	expect("v=%v0/%v1/%v1|4/%v2 obj=%01 global=%g1 word=%w1 string=%s1 message=%m2!",
	       "v=0/42/0042/255 obj=key global=Global word=look string=inner 42 message=42 later!");
	assert(RAM_BANK == 13);
	expect("value:%v1|99/%v2|2/%v2|0", "value:000000000000042/55/");
	expect("plain text", "plain text");
	expect("keep \\% and \\\\ backslash", "keep % and \\ backslash");
	expect("trailing%", "trailing%");
	expect("trailing\\", "trailing\\");
}

static void test_invalid_indices(void)
{
	expect("%v256|2/%s12/%w10/%m0/%q9/end", "/////end");
	expect("%v999999999999999999999tail", "tail");
}

static void test_recursive_substitutions(void)
{
	strcpy(string[0], "A%s0");
	expect("%s0", "A");

	strcpy(string[0], "A%s1");
	strcpy(string[1], "B%s0");
	expect("%s0", "AB");

	strcpy(string[0], "answer %v1");
	strcpy(messageText[1][1], "current %s0");
	expect("%m2", "current answer 42");

	strcpy(string[0], "A%s1");
	strcpy(string[1], "B%s2");
	strcpy(string[2], "C%s3");
	strcpy(string[3], "D%s4");
	strcpy(string[4], "E%s5");
	strcpy(string[5], "F%s6");
	strcpy(string[6], "G%s7");
	strcpy(string[7], "H%s8");
	strcpy(string[8], "I");
	expect("%s0", "ABCDEFG");
}

static void test_chunked_output(void)
{
	char text[201];
	memset(text, 'x', sizeof(text) - 1);
	text[200] = '\0';
	bankedCopyCalls = 0;
	expect(text, text);
	/* Four 50-byte chunks and one terminator, without per-character bank copies. */
	assert(bankedCopyCalls == 5);
}

static void test_destination_bounds(void)
{
	char small[6];

	memset(small, 0x5a, sizeof(small));
	expandFormatCodes("abcdef", TEST_BANK, small, TEST_OUTPUT_BANK, 4);
	assert(strcmp(small, "abc") == 0);
	assert((unsigned char)small[4] == 0x5a);

	strcpy(string[0], "expanded");
	memset(small, 0x5a, sizeof(small));
	expandFormatCodes("%s0", TEST_BANK, small, TEST_OUTPUT_BANK, 4);
	assert(strcmp(small, "exp") == 0);
	assert((unsigned char)small[4] == 0x5a);

	memset(small, 0x5a, sizeof(small));
	expandFormatCodes("abcdef", TEST_BANK, small, TEST_OUTPUT_BANK, 1);
	assert(small[0] == '\0');
	assert((unsigned char)small[1] == 0x5a);

	small[0] = 'Q';
	expandFormatCodes("abcdef", TEST_BANK, small, TEST_OUTPUT_BANK, 0);
	assert(small[0] == 'Q');
}

int main(void)
{
	byte logicNo;
	byte messageNo;

	bDObjects[0].name = objectName;
	for (logicNo = 0; logicNo < TEST_LOGICS; logicNo++) {
		logicFiles[logicNo].messageBank = (byte)(20 + logicNo);
		logicFiles[logicNo].messages = messagePointers[logicNo];
		logicFiles[logicNo].numMessages = TEST_MESSAGES;
		for (messageNo = 0; messageNo < TEST_MESSAGES; messageNo++) {
			messageText[logicNo][messageNo] = messageStorage[logicNo][messageNo];
			messagePointers[logicNo][messageNo] = (byte*)messageText[logicNo][messageNo];
		}
	}
	currentLog = 1;

	test_tokens_and_escapes();
	test_invalid_indices();
	test_recursive_substitutions();
	test_destination_bounds();
	test_chunked_output();
	puts("string format tests passed");
	return 0;
}
