#include "stringFormat.h"

#include <cx16.h>
#include "helpers.h"
#include "logic.h"
#include "object.h"

extern byte* var;
extern int numInputWords;
extern char b7WordText[10][80];
extern char string[12][40];
extern int currentLog;
extern char* getMessagePointer(byte logicFileNo, byte messageNo);

#define B7_MAX_VARS 256
#define B7_NUM_STRINGS 12
#define B7_MAX_EXPANSION_DEPTH 8
#define B7_OUTPUT_CHUNK 50

typedef struct {
	char* dst;
	byte dstBank;
	size_t dstMax;
	size_t length;
	size_t flushed;
	byte buffered;
	char buffer[B7_OUTPUT_CHUNK];
} B7FormatOutput;

typedef struct {
	const char* sources[B7_MAX_EXPANSION_DEPTH];
	byte banks[B7_MAX_EXPANSION_DEPTH];
	byte depth;
} B7ExpansionPath;

#pragma code-name (push, "CODE")
static void flushOutput(B7FormatOutput* output)
{
	if (output->buffered == 0) return;
	memCpyBanked((byte*)output->dst + output->flushed, (byte*)output->buffer,
			    output->dstBank, output->buffered);
	output->flushed += output->buffered;
	output->buffered = 0;
}

static void appendCurrent(B7FormatOutput* output, const char* src, size_t length)
{
	size_t amount, bufferRoom, destinationRoom;

	while (length > 0 && output->length < output->dstMax - 1) {
		bufferRoom = B7_OUTPUT_CHUNK - output->buffered;
		destinationRoom = output->dstMax - 1 - output->length;
		amount = length;
		if (amount > bufferRoom) amount = bufferRoom;
		if (amount > destinationRoom) amount = destinationRoom;
		if (amount == 0) break;

		memcpy(output->buffer + output->buffered, src, amount);
		output->buffered += (byte)amount;
		output->length += amount;
		src += amount;
		length -= amount;
		if (output->buffered == B7_OUTPUT_CHUNK || output->length == output->dstMax - 1)
			flushOutput(output);
	}
}

static void appendBankedString(B7FormatOutput* output, const char* src, byte bank)
{
	byte previousBank;
	size_t amount, bufferRoom, destinationRoom;

	previousBank = RAM_BANK;
	RAM_BANK = bank;
	while (*src != '\0' && output->length < output->dstMax - 1) {
		bufferRoom = B7_OUTPUT_CHUNK - output->buffered;
		destinationRoom = output->dstMax - 1 - output->length;
		if (bufferRoom > destinationRoom) bufferRoom = destinationRoom;
		if (bufferRoom == 0) break;
		for (amount = 0; amount < bufferRoom && src[amount] != '\0'; amount++) {}
		if (amount == 0) break;
		appendCurrent(output, src, amount);
		src += amount;
	}
	RAM_BANK = previousBank;
}

static boolean isOnExpansionPath(const B7ExpansionPath* path, const char* src, byte bank)
{
	byte i;
	for (i = 0; i < path->depth; i++) {
		if (path->sources[i] == src && path->banks[i] == bank) return TRUE;
	}
	return FALSE;
}

static void expandInto(const char* src, byte srcBank, B7FormatOutput* output, B7ExpansionPath* path)
{
	size_t pos, runStart;
	char c, code;
	byte digit, width, previousBank;
	int numVal, idx;
	boolean invalidIndex, hasWidth;
	LOGICFile logicFile;
	char* textPtr;

	if (path->depth >= B7_MAX_EXPANSION_DEPTH || isOnExpansionPath(path, src, srcBank)) return;
	path->sources[path->depth] = src;
	path->banks[path->depth] = srcBank;
	path->depth++;
	previousBank = RAM_BANK;
	RAM_BANK = srcBank;

	pos = 0;
	runStart = 0;
	while (src[pos] != '\0' && output->length < output->dstMax - 1) {
		c = src[pos];
		if (c != '%' && c != '\\') { pos++; continue; }
		if (pos > runStart) appendCurrent(output, src + runStart, pos - runStart);
		if (output->length == output->dstMax - 1) break;
		if (c == '\\') {
			pos++;
			if (src[pos] != '\0') {
				c = src[pos++];
			} else {
				c = '\\';
			}
			appendCurrent(output, &c, 1);
			runStart = pos;
			continue;
		}
		pos++;
		if (src[pos] == '\0') {
			runStart = pos - 1;
			break;
		}
		code = src[pos++];

		numVal = 0;
		invalidIndex = FALSE;
		while (src[pos] >= '0' && src[pos] <= '9') {
			digit = (byte)(src[pos] - '0');
			if (numVal > 25 || (numVal == 25 && digit > 5)) invalidIndex = TRUE;
			else if (!invalidIndex) numVal = numVal * 10 + digit;
			pos++;
		}
		if (invalidIndex) numVal = -1;

		width = 0;
		hasWidth = FALSE;
		if (code == 'v' && src[pos] == '|') {
			hasWidth = TRUE;
			pos++;
			while (src[pos] >= '0' && src[pos] <= '9') {
				int parsedWidth;
				if (width < 15) {
					parsedWidth = (int)width * 10 + (src[pos] - '0');
					width = (byte)(parsedWidth > 15 ? 15 : parsedWidth);
				}
				pos++;
			}
		}

		switch (code) {
		case 'v': {
			char digits[3], formatted[15];
			byte digitCount, i, used, padding, targetWidth;
			unsigned int v;
			if (numVal < 0 || numVal >= B7_MAX_VARS) break;
			v = var[numVal];
			digitCount = 0;
			do {
				digits[digitCount++] = (char)('0' + v % 10);
				v /= 10;
			} while (v > 0);
			targetWidth = hasWidth ? width : digitCount;
			used = digitCount > targetWidth ? targetWidth : digitCount;
			padding = targetWidth - used;
			for (i = 0; i < padding; i++) formatted[i] = '0';
			for (i = 0; i < used; i++) formatted[padding + i] = digits[used - i - 1];
			appendCurrent(output, formatted, targetWidth);
			break;
		}
		case '0': {
			byte objectBank;
			int count;
			idx = numVal - 1;
			if (idx < 0) break;
			objectBank = RAM_BANK;
			RAM_BANK = OBJECT_BANK;
			count = bDNumObjects;
			if (idx < count) textPtr = bDObjects[idx].name;
			RAM_BANK = objectBank;
			if (idx < count) appendBankedString(output, textPtr, OBJECT_BANK);
			break;
		}
		case 'g': {
			idx = numVal - 1;
			if (idx < 0) break;
			b5GetLogicFile(&logicFile, 0);
			if (logicFile.messages == NULL || idx >= logicFile.numMessages) break;
			textPtr = getMessagePointer(0, (byte)idx);
			appendBankedString(output, textPtr, logicFile.messageBank);
			break;
		}
		case 'w': {
			idx = numVal - 1;
			if (idx < 0 || idx >= numInputWords || idx >= 10) break;
			textPtr = b7WordText[idx];
			appendBankedString(output, textPtr, PARSER_BANK);
			break;
		}
		case 's': {
			idx = numVal;
			if (idx < 0 || idx >= B7_NUM_STRINGS) break;
			expandInto(string[idx], PARSER_BANK, output, path);
			break;
		}
		case 'm': {
			idx = numVal - 1;
			if (idx < 0) break;
			b5GetLogicFile(&logicFile, (byte)currentLog);
			if (logicFile.messages == NULL || idx >= logicFile.numMessages) break;
			textPtr = getMessagePointer((byte)currentLog, (byte)idx);
			expandInto(textPtr, logicFile.messageBank, output, path);
			break;
		}
		default:
			break;
		}
		runStart = pos;
	}
	if (src[pos] == '\0' && pos > runStart && output->length < output->dstMax - 1)
		appendCurrent(output, src + runStart, pos - runStart);

	path->depth--;
	RAM_BANK = previousBank;
}

void expandFormatCodes(const char* src, byte srcBank, char* dst, byte dstBank, size_t dstMax)
{
	char zero;
	B7FormatOutput output;
	B7ExpansionPath path;

	if (dstMax == 0) return;
	output.dst = dst;
	output.dstBank = dstBank;
	output.dstMax = dstMax;
	output.length = 0;
	output.flushed = 0;
	output.buffered = 0;
	path.depth = 0;

	expandInto(src, srcBank, &output, &path);
	flushOutput(&output);
	zero = '\0';
	memCpyBanked((byte*)dst + output.flushed, (byte*)&zero, dstBank, 1);
}
#pragma code-name (pop)
