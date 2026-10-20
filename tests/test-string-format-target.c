#include <ascii_charmap.h>
#include <cx16.h>

#include <string.h>

#include "helpers.h"
#include "logic.h"
#include "object.h"
#include "stringFormat.h"

#define TEST_LOGICS 2
#define TEST_MESSAGES 8
#define MESSAGE_BANK 0x14
#define SOURCE_BANK 0x15
#define DESTINATION_BANK 0x16
#define SOURCE_ADDRESS 0xA100
#define OUTPUT_ADDRESS 0xA300

byte variables[256];
byte* var = variables;
int numInputWords;
int currentLog;
unsigned int bankedCopyCalls;
byte targetResult;
byte targetBankBefore;
byte targetBankAfter;

#pragma bss-name (push, "BANKRAM07")
char b7WordText[10][80];
char string[12][40];
char b7Temp[256], b7CurrentInputStr[41], b7OutputString[80];
byte inputLineCanary;
#pragma bss-name (pop)

#pragma bss-name (push, "BANKRAM0D")
objectType bDObjects[MAX_OBJECTS];
int bDNumObjects;
#pragma bss-name (pop)

#pragma bss-name (push, "BANKRAM03")
char b3TextBuffers[2000];
byte borderCanary;
#pragma bss-name (pop)
byte b3LastBoxLines, bridgeMaximum;
int bridgeWidth;
extern byte __fastcall__ b3WrapText(char* text, int width, byte* maximum);
extern void __fastcall__ b3DrawBorder(byte width, size_t messageSize);
byte bridgeActive, bridgeControls, bridgeLines, bridgeCounts[2], bridgeErase;
char cursorChar;
extern void __fastcall__ b7RefreshInputLine(boolean erasePrevious);
extern byte __fastcall__ b3ClipTextToMap(char* text);
extern void __fastcall__ b3ApplyTextModeControls(char* text);
extern void __fastcall__ b7StripExtraChars(char* text);
extern char** __fastcall__ b7TokenizeWords(char* text, char** tokens);
byte* bridgeMessages[2][256];
size_t bridgeCapacity;

static LOGICFile logicFiles[TEST_LOGICS];
static byte* messagePointers[TEST_LOGICS][TEST_MESSAGES];

void* memCpyBanked(byte* destination, byte* source, byte bank, size_t length)
{
	byte previousBank = RAM_BANK;
	void* result;

	RAM_BANK = bank;
	bankedCopyCalls++;
	result = memcpy(destination, source, length);
	RAM_BANK = previousBank;
	return result;
}

void b5GetLogicFile(LOGICFile* result, byte logicNo)
{
	if (logicNo >= TEST_LOGICS) {
		memset(result, 0, sizeof(*result));
		return;
	}
	*result = logicFiles[logicNo];
	if (bridgeActive) {
		result->numMessages = bridgeCounts[logicNo];
		result->messages = bridgeMessages[logicNo];
		result->messageBank = logicNo ? MESSAGE_BANK : MESSAGE_BANK - 1;
	}
}

char* getMessagePointer(byte logicNo, byte messageNo)
{
	if (bridgeActive && logicNo < 2) return (char*)bridgeMessages[logicNo][messageNo];
	if (logicNo >= TEST_LOGICS || messageNo >= TEST_MESSAGES) return NULL;
	return (char*)messagePointers[logicNo][messageNo];
}

static void putBankedString(byte bank, unsigned address, const char* text)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = bank;
	strcpy((char*)address, text);
	RAM_BANK = previousBank;
}

static void fillBanked(byte bank, unsigned address, byte value, size_t length)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = bank;
	memset((void*)address, value, length);
	RAM_BANK = previousBank;
}

static void setParserString(byte index, const char* text)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = PARSER_BANK;
	strcpy(string[index], text);
	RAM_BANK = previousBank;
}

static void setWord(const char* text)
{
	byte previousBank = RAM_BANK;
	RAM_BANK = PARSER_BANK;
	strcpy(b7WordText[0], text);
	RAM_BANK = previousBank;
}

static void initializeFixture(void)
{
	byte previousBank = RAM_BANK;
	byte logicNo;
	byte messageNo;

	bridgeActive = 0;
	bridgeControls = 0;
	memset(variables, 0, sizeof(variables));
	memset(logicFiles, 0, sizeof(logicFiles));
	memset(messagePointers, 0, sizeof(messagePointers));
	bankedCopyCalls = 0;
	variables[1] = 42;
	variables[2] = 255;
	numInputWords = 1;
	currentLog = 1;

	RAM_BANK = PARSER_BANK;
	memset(string, 0, sizeof(string));
	memset(b7WordText, 0, sizeof(b7WordText));
	RAM_BANK = OBJECT_BANK;
	memset(bDObjects, 0, sizeof(bDObjects));
	bDNumObjects = 1;
	bDObjects[0].name = (char*)0xA100;
	RAM_BANK = previousBank;

	for (logicNo = 0; logicNo < TEST_LOGICS; logicNo++) {
		logicFiles[logicNo].numMessages = TEST_MESSAGES;
		logicFiles[logicNo].messages = messagePointers[logicNo];
		logicFiles[logicNo].messageBank = MESSAGE_BANK;
		for (messageNo = 0; messageNo < TEST_MESSAGES; messageNo++)
			messagePointers[logicNo][messageNo] = (byte*)0xA200 + messageNo * 32;
	}
	putBankedString(MESSAGE_BANK, 0xA200, "Global");
	putBankedString(MESSAGE_BANK, 0xA220, "42 later");
	putBankedString(MESSAGE_BANK, 0xA2A0, "Gwydion");
	putBankedString(MESSAGE_BANK, 0xA2C0, "Manannan");
	putBankedString(OBJECT_BANK, 0xA100, "key");
	setWord("look");
}

static int expectFormat(const char* input, const char* expected, size_t destinationMax)
{
	putBankedString(SOURCE_BANK, SOURCE_ADDRESS, input);
	fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5A, 256);
	RAM_BANK = 13;
	targetBankBefore = RAM_BANK;
	expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
			  (char*)OUTPUT_ADDRESS, DESTINATION_BANK, destinationMax);
	targetBankAfter = RAM_BANK;
	if (targetBankBefore != targetBankAfter) return 0;
	RAM_BANK = DESTINATION_BANK;
	if (strcmp((char*)OUTPUT_ADDRESS, expected) != 0) return 0;
	RAM_BANK = 13;
	return 1;
}

void targetPrepareKq3(void)
{
	static const char warning[] = "\"You're up to no good, %m6\" %m7 snarls through clenched teeth. \"Never again will you discover my secrets!\"";

	initializeFixture();
	putBankedString(SOURCE_BANK, SOURCE_ADDRESS, warning);
	fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5A, 256);
	targetResult = 0xFF;
	RAM_BANK = 13;
}

void targetExpandKq3(void)
{
	RAM_BANK = 13;
	targetBankBefore = RAM_BANK;
	expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
			  (char*)OUTPUT_ADDRESS, DESTINATION_BANK, 116);
	targetBankAfter = RAM_BANK;
}

void targetValidateKq3(void)
{
	static const char expected[] = "\"You're up to no good, Gwydion\" Manannan snarls through clenched teeth. \"Never again will you discover my secrets!\"";
	static const char warning[] = "\"You're up to no good, %m6\" %m7 snarls through clenched teeth. \"Never again will you discover my secrets!\"";
	int valid;

	RAM_BANK = DESTINATION_BANK;
	valid = strlen((char*)OUTPUT_ADDRESS) == 115 && strcmp((char*)OUTPUT_ADDRESS, expected) == 0;
	valid = valid && ((char*)OUTPUT_ADDRESS)[115] == '\0';
	RAM_BANK = SOURCE_BANK;
	valid = valid && strlen((char*)SOURCE_ADDRESS) == 106 && strcmp((char*)SOURCE_ADDRESS, warning) == 0;
	RAM_BANK = 13;
	valid = valid && targetBankBefore == targetBankAfter;
	RAM_BANK = DESTINATION_BANK;
	valid = valid && ((byte*)OUTPUT_ADDRESS)[116] == 0x5A;
	RAM_BANK = 13;
	targetResult = valid ? 0 : 1;
}

void targetPrepareLiteral(void)
{
    char text[80];
    initializeFixture();
    memset(text, 'w', 79);
    text[79] = 0;
    memcpy(text + 2, "%v1\\x", 5);
    setWord(text);
    fillBanked(OBJECT_BANK, 0xA100, 'y', 79);
    fillBanked(OBJECT_BANK, 0xA14F, 0, 1);
    fillBanked(MESSAGE_BANK, 0xA200, 'z', 79);
    fillBanked(MESSAGE_BANK, 0xA24F, 0, 1);
    putBankedString(SOURCE_BANK, SOURCE_ADDRESS, "%w1/%01/%g1");
    fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5a, 256);
}

void targetExpandLiteral(void)
{
    RAM_BANK = 13;
    targetBankBefore = RAM_BANK;
    expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
                      (char*)OUTPUT_ADDRESS, DESTINATION_BANK, 240);
    targetBankAfter = RAM_BANK;
}

void targetValidateLiteral(void)
{
    unsigned int i;
    byte expected;
    int valid = targetBankBefore == targetBankAfter && targetBankAfter == 13;
    RAM_BANK = DESTINATION_BANK;
    for (i = 0; i < 239; i++) {
        expected = i < 79 ? 'w' : i == 79 || i == 159 ? '/' : i < 159 ? 'y' : 'z';
        if (i >= 2 && i < 7) expected = "%v1\\x"[i - 2];
        if (((byte*)OUTPUT_ADDRESS)[i] != expected) valid = 0;
    }
    valid = valid && ((byte*)OUTPUT_ADDRESS)[239] == 0 && ((byte*)OUTPUT_ADDRESS)[240] == 0x5a;
    RAM_BANK = SOURCE_BANK;
    valid = valid && strcmp((char*)SOURCE_ADDRESS, "%w1/%01/%g1") == 0;
    RAM_BANK = 13;
    targetResult = valid ? 0 : 3;
}

static int expectCapacity(const char* input, const char* expected, size_t max,
			  byte firstGuard, byte afterGuard)
{
	if (!expectFormat(input, expected, max)) return 0;
	RAM_BANK = DESTINATION_BANK;
	if (max == 4 && ((byte*)OUTPUT_ADDRESS)[4] != afterGuard) return 0;
	if (max == 1 && ((byte*)OUTPUT_ADDRESS)[1] != afterGuard) return 0;
	if (max == 0 && ((byte*)OUTPUT_ADDRESS)[0] != firstGuard) return 0;
	RAM_BANK = 13;
	return 1;
}

void targetEdgeCases(void)
{
	char expanded200[201];
    static char expanded300[301];
    unsigned int i, j, n, used;
    static const unsigned int capacities[] = { 0, 1, 2, 49, 50, 51, 99, 100, 101, 255, 256, 257, 301, 302 };
	int valid = 1;
	byte previousBank;

	initializeFixture();
	variables[0] = 0;
	setParserString(1, "inner %v1");
	setWord("look");
	putBankedString(MESSAGE_BANK, 0xA200, "Global");
	putBankedString(MESSAGE_BANK, 0xA220, "42 later");
	valid = valid && expectFormat("v=%v0/%v1/%v1|4/%v2 obj=%01 global=%g1 word=%w1 string=%s1 message=%m2!",
		"v=0/42/0042/255 obj=key global=Global word=look string=inner 42 message=42 later!", 256);
	valid = valid && expectFormat("keep \\% and \\\\ backslash", "keep % and \\ backslash", 256);
	valid = valid && expectFormat("trailing%", "trailing%", 256);
	valid = valid && expectFormat("trailing\\", "trailing\\", 256);
	valid = valid && expectFormat("%v256|2/%s12/%w10/%m0/%q9/end", "/////end", 256);
	valid = valid && expectFormat("%v999999999999999999999tail", "tail", 256);
	setParserString(11, "last string");
	RAM_BANK = PARSER_BANK;
	strcpy(b7WordText[9], "last word");
	numInputWords = 10;
	RAM_BANK = OBJECT_BANK;
	bDNumObjects = MAX_OBJECTS;
	bDObjects[MAX_OBJECTS - 1].name = (char*)0xA600;
	RAM_BANK = 13;
	putBankedString(OBJECT_BANK, 0xA600, "last object");
	valid = valid && expectFormat("%s11/%w10/%0100/%0101", "last string/last word/last object/", 256);

    /* Highest one-based message index crosses the table's first page. */
    logicFiles[1].numMessages = 255;
    logicFiles[1].messages = (byte**)0xA700;
    RAM_BANK = MESSAGE_BANK;
    ((byte**)0xA700)[254] = (byte*)0xA200;
    valid = valid && expectFormat("%m255/%g1", "Global/Global", 256);
    logicFiles[1].numMessages = TEST_MESSAGES;
    logicFiles[1].messages = messagePointers[1];
    valid = valid && expectFormat("%m9", "", 256);
    logicFiles[1].messages = NULL;
    valid = valid && expectFormat("%m1", "", 256);
    logicFiles[1].messages = messagePointers[1];

	setParserString(0, "A%s0");
	valid = valid && expectFormat("%s0", "A", 256);
	setParserString(0, "A%s1");
	setParserString(1, "B%s0");
	valid = valid && expectFormat("%s0", "AB", 256);
	setParserString(0, "answer %v1");
	putBankedString(MESSAGE_BANK, 0xA220, "current %s0");
	valid = valid && expectFormat("%m2", "current answer 42", 256);
	setParserString(0, "A%s1");
	setParserString(1, "B%s2");
	setParserString(2, "C%s3");
	setParserString(3, "D%s4");
	setParserString(4, "E%s5");
	setParserString(5, "F%s6");
	setParserString(6, "G%s7");
	setParserString(7, "H%s8");
	setParserString(8, "I");
	valid = valid && expectFormat("%s0", "ABCDEFG", 256);

	memset(expanded200, 'x', sizeof(expanded200) - 1);
	expanded200[200] = '\0';
	putBankedString(SOURCE_BANK, SOURCE_ADDRESS, expanded200);
	fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5A, 256);
	bankedCopyCalls = 0;
	RAM_BANK = 13;
	expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
			  (char*)OUTPUT_ADDRESS, DESTINATION_BANK, 201);
	RAM_BANK = DESTINATION_BANK;
	valid = valid && strcmp((char*)OUTPUT_ADDRESS, expanded200) == 0;
	RAM_BANK = 13;
	RAM_BANK = DESTINATION_BANK;
	valid = valid && ((byte*)OUTPUT_ADDRESS)[201] == 0x5A;
	RAM_BANK = 13;


    /* A literal message crosses the indexed cursor's 256-byte boundary. */
    fillBanked(MESSAGE_BANK, 0xA200, 'q', 300);
    fillBanked(MESSAGE_BANK, 0xA32C, 0, 1);
    RAM_BANK = MESSAGE_BANK;
    memcpy((char*)0xA200 + 254, "%v1\\x", 5);
    putBankedString(SOURCE_BANK, SOURCE_ADDRESS, "%g1");
    fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5a, 302);
    RAM_BANK = 13;
    expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
                      (char*)OUTPUT_ADDRESS, DESTINATION_BANK, 302);
    valid = valid && RAM_BANK == 13;
    RAM_BANK = DESTINATION_BANK;
    for (i = 0; i < 300; i++)
        valid = valid && ((char*)OUTPUT_ADDRESS)[i] == (i >= 254 && i < 259 ? "%v1\\x"[i - 254] : 'q');
    valid = valid && ((byte*)OUTPUT_ADDRESS)[300] == 0 && ((byte*)OUTPUT_ADDRESS)[301] == 0x5a;
    RAM_BANK = 13;


    /* End literal and recursive runs on either side of every flush boundary. */
    fillBanked(MESSAGE_BANK, 0xA200, 'q', 300);
    fillBanked(MESSAGE_BANK, 0xA32C, 0, 1);
    for (n = 0; n < 2; n++) {
        putBankedString(SOURCE_BANK, SOURCE_ADDRESS, n ? "%m1" : "%g1");
        for (j = 0; j < sizeof(capacities) / sizeof(capacities[0]); j++) {
            fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5a, 303);
            RAM_BANK = 13;
            expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
                              (char*)OUTPUT_ADDRESS, DESTINATION_BANK, capacities[j]);
            valid = valid && RAM_BANK == 13;
            RAM_BANK = DESTINATION_BANK;
            used = capacities[j] ? capacities[j] - 1 : 0;
            if (used > 300) used = 300;
            for (i = 0; i < used; i++) valid = valid && ((byte*)OUTPUT_ADDRESS)[i] == 'q';
            if (capacities[j]) valid = valid && ((byte*)OUTPUT_ADDRESS)[used] == 0;
            valid = valid && ((byte*)OUTPUT_ADDRESS)[capacities[j]] == 0x5a;
        }
    }
    RAM_BANK = 13;


    /* Save a token cursor after indexed wrap on an unaligned banked source. */
    memset(expanded300, 'x', 300);
    expanded300[300] = 0;
    memcpy(expanded300 + 259, "%v1", 3);
    memcpy(expanded300 + 290, "\\%", 2);
    putBankedString(SOURCE_BANK, 0xA0FD, expanded300);
    fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5a, 303);
    RAM_BANK = 13;
    expandFormatCodes((char*)0xA0FD, SOURCE_BANK,
                      (char*)OUTPUT_ADDRESS, DESTINATION_BANK, 301);
    valid = valid && RAM_BANK == 13;
    RAM_BANK = DESTINATION_BANK;
    for (i = 0; i < 298; i++)
        valid = valid && ((char*)OUTPUT_ADDRESS)[i] == (i == 259 ? '4' : i == 260 ? '2' : i == 289 ? '%' : 'x');
    valid = valid && ((byte*)OUTPUT_ADDRESS)[298] == 0 && ((byte*)OUTPUT_ADDRESS)[299] == 0x5a;
    RAM_BANK = SOURCE_BANK;
    valid = valid && strcmp((char*)0xA0FD, expanded300) == 0;
    RAM_BANK = 13;

	valid = valid && expectCapacity("abcdef", "abc", 4, 0x5A, 0x5A);
	valid = valid && expectCapacity("abcdef", "", 1, 0x5A, 0x5A);
	fillBanked(DESTINATION_BANK, OUTPUT_ADDRESS, 0x5A, 256);
	RAM_BANK = DESTINATION_BANK;
	((char*)OUTPUT_ADDRESS)[0] = 'Q';
	RAM_BANK = 13;
	previousBank = RAM_BANK;
	putBankedString(SOURCE_BANK, SOURCE_ADDRESS, "abcdef");
	expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
			  (char*)OUTPUT_ADDRESS, DESTINATION_BANK, 0);
	targetBankAfter = RAM_BANK;
	RAM_BANK = DESTINATION_BANK;
	valid = valid && ((char*)OUTPUT_ADDRESS)[0] == 'Q';
	RAM_BANK = previousBank;
	valid = valid && targetBankAfter == previousBank && RAM_BANK == 13;

	targetResult = valid ? 0 : 2;
}

void targetBridge(void)
{
	RAM_BANK = 13;
	bridgeLines = 1;
	if (bridgeControls) {
		RAM_BANK = SOURCE_BANK;
		if (bridgeControls == 7) {
            RAM_BANK = PARSER_BANK;
            inputLineCanary = 0x5a;
            memset(b7OutputString, 0x5a, sizeof(b7OutputString));
            b7RefreshInputLine(bridgeErase);
            if (inputLineCanary != 0x5a || !memchr(b7OutputString, 0, sizeof(b7OutputString))) {
                targetResult = 4;
                return;
            }
        }
        else if (bridgeControls == 6) {
            bridgeLines = (byte)(((unsigned int)b7TokenizeWords((char*)SOURCE_ADDRESS, (char**)0xA800) - 0xA800 + 2) / 2);
        }
        else if (bridgeControls == 5) {
            RAM_BANK = 3;
            memset(b3TextBuffers + 1000, 0x5a, 1000);
            borderCanary = 0x5a;
            b3DrawBorder((byte)bridgeWidth, 0);
            bridgeLines = b3LastBoxLines;
            if (!memchr(b3TextBuffers + 1000, 0, 1000) ||
                borderCanary != 0x5a) {
                targetResult = 2;
                return;
            }
        }
        else if (bridgeControls == 4) bridgeLines = b3WrapText((char*)SOURCE_ADDRESS, bridgeWidth, &bridgeMaximum);
        else if (bridgeControls == 3) b7StripExtraChars((char*)SOURCE_ADDRESS);
		else if (bridgeControls == 2) bridgeLines = b3ClipTextToMap((char*)SOURCE_ADDRESS);
		else b3ApplyTextModeControls((char*)SOURCE_ADDRESS);
		RAM_BANK = 13;
	} else {
		expandFormatCodes((char*)SOURCE_ADDRESS, SOURCE_BANK,
		                  (char*)OUTPUT_ADDRESS, DESTINATION_BANK, bridgeCapacity);
	}
	targetResult = RAM_BANK == 13 ? 0 : 1;
}

int main(void)
{
	return 0;
}
