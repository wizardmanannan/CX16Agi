#include "textLayer.h"
#pragma code-name (push, "BANKRAM06")
//#define VERBOSE_CHAR_SET_LOAD
//#define TEST_CHARSET
//#define VERBOSE_DISPLAY_TEXT
//#define VERBOSE_SET_PALETTE
#ifdef VERBOSE_CHAR_SET_LOAD
byte printOn = TRUE;
int byteCounter = 0;
#endif

boolean charSetInited = FALSE;
#pragma bss-name (push, "BANKRAM03")
byte b3CurrentForegroundColour;
byte b3CurrentBackgroundColour;
byte b3LastBoxLines;
byte b3LastBoxStartLine;
static byte b3LastBoxFirstRow;
char b3TextBuffers[TEXTBUFFER_TOTAL_SIZE];
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM03")
void b3MakeTopBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + TOP_BORDER * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i < BYTES_PER_CHARACTER - BYTES_PER_CELL)
		{
			WRITE_BYTE_DEF_TO_ASSM(0, VERA_data0); //White square
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b11111111, VERA_data0); //Red line
		}
	}
}

void b3MakeLeftBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + LEFT_BORDER * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i % 2 == 0)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b11101010, VERA_data0); //Red border
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White square
		}
	}
}

void b3MakeBottomBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + BOTTOM_BORDER * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i < BYTES_PER_CELL)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b11111111, VERA_data0); //Red line
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0, VERA_data0); //White square
		}
	}
}

void b3MakeRightBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + RIGHT_BORDER * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i % 2 == 1)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101011, VERA_data0); //Red border
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White square
		}
	}
}


void b3MakeMenuTopEnd()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + MENU_TOP_END * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i == 0 || i == 1)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b01010101, VERA_data0); //Black Horizonal Border
		}
		else if (i % 2 == 0)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b01101010, VERA_data0); //Black Vertical Border 
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White Space 
		}
	}
}

void b3MakeMenuTop()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + MENU_TOP * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);
	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i == 0 || i == 1)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b01010101, VERA_data0); //Black Horizonal Border
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White Space 
		}
	}
}

void b3MakeMenuVertical()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + MENU_VERTICAL * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);
	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i % 2 == 0)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b01101010, VERA_data0); //Black Vertical Border 
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White Space 
		}
	}
}

void b3ConvertsOneBitPerPixCharToTwoBitPerPixelChars()
{
	int i;
	byte j; //Must be int because it needs to be unsigned
	byte romPixel, output = 0, romRow;
	byte resultByteShift = 6;

	for (i = 0; i < NO_CHARS * SIZE_PER_CHAR_CHAR_SET_ROM; i++)
	{
		READ_BYTE_VAR_FROM_ASSM(romRow, VERA_data0);

		for (j = 7; j != 255; j--) //Overflow means the loop is done
		{

			romPixel = romRow >> j & 1;

			if (i >= TRANSPARENT_CHAR_BYTE && i <= LAST_BYTE_TRANSPARENT_CHAR)
			{
				romPixel = 0;
			}
			else if (!romPixel)
			{
				romPixel = 2; //Note: We have four colors trans:0,b:1,w:2,red:4. Therefore a value of 0 (white in the ROM needs to become 2)
			}

			output |= (romPixel << resultByteShift);

			resultByteShift -= 2;
			if (resultByteShift == 254) //Underflow means we are ready to start again
			{
				resultByteShift = 6;

				WRITE_BYTE_VAR_TO_ASSM(output, VERA_data1);

				output = 0;
			}
		}
	}
}

void b3InitCharset()
{
#define ORIGINAL_CHARSET_ADDRESS 0x1f000


	int i;
	//printf("Initializing CharSet. . .\n");

	SCREEN_SET_CHAR_SET(ISO);

#ifdef VERBOSE_CHAR_SET_LOAD
	PRINTF("The address of new charset buffer is %p\n", buffer);
#endif // VERBOSE_CHAR_SET_LOAD

	SET_VERA_ADDRESS(ORIGINAL_CHARSET_ADDRESS, ADDRESSSEL0, 1);
	SET_VERA_ADDRESS(TILEBASE, ADDRESSSEL1, 1);

	b3ConvertsOneBitPerPixCharToTwoBitPerPixelChars();

	//While we could just flip them it will take less cycles when we write text just to have chars for both, as that way our stride can be two
	b3MakeTopBorder();
	b3MakeLeftBorder();
	b3MakeBottomBorder();
	b3MakeRightBorder();
	b3MakeMenuTopEnd();
	b3MakeMenuTop();
	b3MakeMenuVertical();

#ifdef VERBOSE_CHAR_SET_LOAD
	printf("returning : %p. The byte counter is %d\n.", buffer, byteCounter);
#endif // VERBOSE_CHAR_SET_LOAD

	charSetInited = TRUE;
}

#ifdef TEST_CHARSET
void b6TestCharset()
{
	int i;
	byte j;
	byte* veraDcVideo = (byte*)VERA_DCVIDEO;

#define LAYER_1_2_ENABLE 0x31;

	*veraDcVideo = LAYER_1_2_ENABLE;

	SET_VERA_ADDRESS(MAPBASE, ADDRESSSEL0, 2);

	for (i = 0; i < NO_CHARS; i++)
	{
		WRITE_BYTE_VAR_TO_ASSM(i, VERA_data0);

		if (i && !((i + 1) % MAX_CHAR_ACROSS))
		{
			for (j = 0; j < TILE_LAYER_WIDTH - MAX_CHAR_ACROSS; j++)
			{
				WRITE_BYTE_VAR_TO_ASSM(TRANSPARENT_CHAR, VERA_data0);
			}
		}
	}

	while (1);
}
#endif

static void b3FillCharAt(byte firstRow, byte startLine, unsigned int endLine,
	byte paletteNumber, byte charToFill)
{
	unsigned int line, lastLine, chunkStartLine, chunkLastLine;
	const unsigned int maxRowsPerChunk = TEXTBUFFER_TOTAL_SIZE / (TILE_LAYER_WIDTH + 1);
	unsigned int lastMapLine;

	if (firstRow < 1 || firstRow > TILE_LAYER_HEIGHT)
	{
		return;
	}

	lastMapLine = TILE_LAYER_HEIGHT - firstRow;
	if (startLine > lastMapLine || startLine > endLine)
	{
		return;
	}

	lastLine = endLine < lastMapLine ? endLine : lastMapLine;
	for (line = startLine; line <= lastLine;)
	{
		char* clearBuffer = b3TextBuffer1;
		chunkStartLine = line;
		chunkLastLine = line + maxRowsPerChunk - 1;
		if (chunkLastLine > lastLine) chunkLastLine = lastLine;
		while (line <= chunkLastLine)
		{
			memset(clearBuffer, charToFill, TILE_LAYER_WIDTH);
			clearBuffer += TILE_LAYER_WIDTH;
			if (line < chunkLastLine) *clearBuffer++ = NEW_LINE;
			++line;
		}
		*clearBuffer = '\0';
		b3DisplayMessageBox(b3TextBuffer1, TEXT_CODE_BANK, chunkStartLine, 0,
			paletteNumber, 0, TRUE, firstRow);
	}
}

void b3FillChar(byte startLine, byte endLine, byte paletteNumber, byte charToFill)
{
	b3FillCharAt(FIRST_TEXT_ROW, startLine, endLine, paletteNumber, charToFill);
}

//Thanks to https://www.rosettacode.org/wiki/Word_wrap#In-place_greedy
//Agi text does not have newlines and requires the programmer to manually wrap the text
byte b3WrapText(char* line_start, int width, byte* maxWidth) {
	char* last_space = 0;
	char* p;
	size_t lineWidth;
	byte numberOfLines = 1;
	*maxWidth = 0;

	for (p = line_start; *p; p++) {
		if (*p == ' ') {
			last_space = p;
		}

		if (*p == NEW_LINE) {
			lineWidth = p - line_start;
			if (lineWidth > *maxWidth)
			{
				*maxWidth = lineWidth > 255 ? 255 : (byte)lineWidth;
			}
			line_start = p + 1;
			last_space = 0;
			if (numberOfLines != 255) numberOfLines++;
		}
		else if (p - line_start > width && last_space) {
			*last_space = NEW_LINE;
			lineWidth = last_space - line_start;
			if (lineWidth > *maxWidth)
			{
				*maxWidth = lineWidth > 255 ? 255 : (byte)lineWidth;
			}
			line_start = last_space + 1;
			last_space = 0;
			if (numberOfLines != 255) numberOfLines++;

		}
	}

	lineWidth = p - line_start;
	if (lineWidth > *maxWidth)
	{
		*maxWidth = lineWidth > 255 ? 255 : (byte)lineWidth;
	}

	return numberOfLines;
}

#pragma code-name (push, "CODE")
static void b3ClipPreparedText(char* text, unsigned int maxColumns, unsigned int maxRows)
{
	char* source = text;
	char* destination = text;
	char* lineEnd;
	unsigned int row = 1;
	size_t lineLength;

	if (!maxColumns || !maxRows)
	{
		*text = '\0';
		return;
	}

	while (*source && row <= maxRows)
	{
		lineEnd = source;
		while (*lineEnd && *lineEnd != NEW_LINE) ++lineEnd;
		lineLength = (size_t)(lineEnd - source);
		if (lineLength > maxColumns) lineLength = maxColumns;
		if (lineLength)
		{
			memmove(destination, source, lineLength);
			destination += lineLength;
		}
		if (*lineEnd != NEW_LINE || row == maxRows) break;
		*destination++ = NEW_LINE;
		source = lineEnd + 1;
		++row;
	}
	*destination = '\0';
}
#pragma code-name (pop)

#pragma code-name (push, "CODE")
void b3DrawBorder(byte boxWidth, size_t messageSize)
{
	byte i;
	char* currentCharToWrite = b3TextBuffer2;
	char* scan;
	char* sourceEnd;
	char* lineStart;
	size_t lineLength, rowSize, remaining, spaces, lineCount;

	if (boxWidth < 2 || boxWidth == 255 || messageSize < 2)
	{
		b3TextBuffer2[0] = '\0';
		return;
	}

	if (messageSize > TEXTBUFFER_SIZE)
	{
		messageSize = TEXTBUFFER_SIZE;
	}

	rowSize = (size_t)boxWidth + 2;
	if (rowSize > TEXTBUFFER_SIZE / 4)
	{
		b3TextBuffer2[0] = '\0';
		return;
	}

	remaining = TEXTBUFFER_SIZE;
	scan = b3TextBuffer1;
	sourceEnd = b3TextBuffer1 + messageSize;
	lineCount = 0;

	if (remaining < rowSize) goto border_failed;
	for (i = 0; i <= boxWidth; i++)
	{
		*currentCharToWrite++ = TOP_BORDER;
	}
	*currentCharToWrite++ = NEW_LINE;
	remaining -= rowSize;

	if (remaining < rowSize) goto border_failed;
	*currentCharToWrite++ = LEFT_BORDER;
	for (i = 0; i < boxWidth - 1; i++)
	{
		*currentCharToWrite++ = SPACE;
	}
	*currentCharToWrite++ = RIGHT_BORDER;
	*currentCharToWrite++ = NEW_LINE;
	remaining -= rowSize;

	while (scan < sourceEnd && *scan)
	{
		lineStart = scan;
		while (scan < sourceEnd && *scan && *scan != NEW_LINE)
		{
			++scan;
		}
		if (scan == sourceEnd)
		{
			goto border_failed;
		}

		lineLength = (size_t)(scan - lineStart);
		if (lineLength > (size_t)boxWidth - 1 || remaining < rowSize)
		{
			goto border_failed;
		}

		*currentCharToWrite++ = LEFT_BORDER;
		memcpy(currentCharToWrite, lineStart, lineLength);
		currentCharToWrite += lineLength;
		spaces = (size_t)boxWidth - 1 - lineLength;
		while (spaces > 0)
		{
			*currentCharToWrite++ = SPACE;
			--spaces;
		}
		*currentCharToWrite++ = RIGHT_BORDER;
		*currentCharToWrite++ = NEW_LINE;
		remaining -= rowSize;
		++lineCount;
		if (*scan == NEW_LINE) ++scan;
	}

	if (scan == sourceEnd || !lineCount)
	{
		goto border_failed;
	}
	if (scan > b3TextBuffer1 && scan[-1] == NEW_LINE)
	{
		if (remaining < rowSize) goto border_failed;
		*currentCharToWrite++ = LEFT_BORDER;
		for (i = 0; i < boxWidth - 1; i++)
		{
			*currentCharToWrite++ = SPACE;
		}
		*currentCharToWrite++ = RIGHT_BORDER;
		*currentCharToWrite++ = NEW_LINE;
		remaining -= rowSize;
	}

	if (remaining < rowSize) goto border_failed;
	*currentCharToWrite++ = LEFT_BORDER;
	for (i = 0; i < boxWidth - 1; i++)
	{
		*currentCharToWrite++ = SPACE;
	}
	*currentCharToWrite++ = RIGHT_BORDER;
	*currentCharToWrite++ = NEW_LINE;
	remaining -= rowSize;

	if (remaining < rowSize) goto border_failed;
	for (i = 0; i <= boxWidth; i++)
	{
		*currentCharToWrite++ = BOTTOM_BORDER;
	}
	*currentCharToWrite++ = '\0';
	return;

	border_failed:
	b3TextBuffer2[0] = '\0';
}
#pragma code-name (pop)

extern unsigned long displayTextAddressToCopyTo;
extern char* currentTextBuffer;
extern long b3PaletteAddress;
extern byte b3PaletteRows;
extern byte b3PaletteNumber;

//Box width is 0 for text that is not in a box, or the width of the box otherwise
//Supports copying from banks or putting data directly into textbuffer, which is on bank 3
void b3DisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth, boolean wrap, byte firstRow)
{
	size_t messageSize, messageLength, copyLength;
	byte textWidth = boxWidth;
	byte maxWidth;
	char* preparedChar;
	char* preparedBuffer = b3TextBuffer1;
	unsigned int preparedRows;
	unsigned int preparedColumn, preparedStartLine, availableColumns, availableRows, displayWidth;
	unsigned int lastLogicalRow, physicalRow;
	size_t lineWidth = 0, maximumLineWidth = 0;
	boolean messageTerminated;
	/* Row fills pass their prepared map buffer directly to clear all 64 cells. */
	boolean fullLayerClear = !boxWidth && row != AUTO_CALC_ROW && col == 0 && wrap &&
		messageBank == TEXT_CODE_BANK && message == (char*)b3TextBuffer1;
	size_t maxMessageSize = boxWidth ? TEXTBUFFER_SIZE : TEXTBUFFER_TOTAL_SIZE;
	size_t sourceScanLimit = maxMessageSize;

	if (!message || firstRow < 1 || firstRow > TILE_LAYER_HEIGHT)
	{
		goto message_rejected;
	}
	lastLogicalRow = TILE_LAYER_HEIGHT - firstRow;
	if (messageBank == TEXT_CODE_BANK)
	{
		unsigned int sourceAddress = (unsigned int)message;
		unsigned int bufferAddress = (unsigned int)b3TextBuffers;
		if (sourceAddress >= bufferAddress)
		{
			unsigned int sourceOffset = sourceAddress - bufferAddress;
			if (sourceOffset < TEXTBUFFER_TOTAL_SIZE)
			{
				size_t sourceBytesRemaining = TEXTBUFFER_TOTAL_SIZE - sourceOffset;
				if (sourceScanLimit > sourceBytesRemaining) sourceScanLimit = sourceBytesRemaining;
			}
		}
	}
	messageLength = strLenBankedBounded(message, messageBank, sourceScanLimit, &messageTerminated);
	if (messageLength >= maxMessageSize)
	{
		messageSize = maxMessageSize;
		copyLength = maxMessageSize;
	}
	else
	{
		messageSize = messageLength + 1;
		copyLength = messageTerminated ? messageSize : messageLength;
	}

	if (messageSize > 1) //Agi sometimes has empty messages. We say greater than 1 because of the terminator
	{
#ifdef VERBOSE_DISPLAY_TEXT
		printf("Address: %p + (%d + %d) * %d + %d * %d = %lx\n", MAPBASE, firstRow, row, TILE_LAYER_BYTES_PER_ROW, col, BYTES_PER_CELL, displayTextAddressToCopyTo);
#endif
#ifdef VERBOSE_DISPLAY_TEXT
		printf("Copy message %p on bank %d of size %d\n", message, messageBank, messageSize);
		printf("row %d and col is %d", row, col);
#endif // VERBOSE_DISPLAY_TEXT

		if (messageBank != TEXT_CODE_BANK || message != (char*)b3TextBuffer1)
		{
			memCpyBankedBetween((byte*)b3TextBuffer1, TEXT_CODE_BANK, (byte*)message, messageBank, copyLength);
		}
		b3TextBuffer1[messageSize - 1] = '\0';

		if (messageSize - 1 > TILE_LAYER_WIDTH / 2)
		{
			if (wrap)
			{
				if (boxWidth)
				{
					textWidth = boxWidth - 4;
				}

				b3WrapText(b3TextBuffer1, boxWidth ? textWidth : TILE_LAYER_WIDTH, &maxWidth);

				if (boxWidth && maxWidth)
				{
					if (maxWidth > 252)
					{
						goto message_rejected;
					}
					boxWidth = maxWidth + 3;
				}
			}
		}
		else if (boxWidth && messageSize < boxWidth - 4)
		{
			boxWidth = messageSize + 2;
		}

		if (boxWidth)
		{
			b3DrawBorder(boxWidth - 2, messageSize);
			if (!b3TextBuffer2[0])
			{
				goto message_rejected;
			}
			preparedBuffer = b3TextBuffer2;
		}

		/* Measure the actual output so centering and clipping use rendered geometry. */
		preparedRows = 1;
		for (preparedChar = preparedBuffer; *preparedChar; ++preparedChar)
		{
			if (*preparedChar == NEW_LINE)
			{
				if (lineWidth > maximumLineWidth) maximumLineWidth = lineWidth;
				lineWidth = 0;
				++preparedRows;
			}
			else
			{
				++lineWidth;
			}
		}
		if (lineWidth > maximumLineWidth) maximumLineWidth = lineWidth;
		if (preparedRows > 255) goto message_rejected;
		displayWidth = fullLayerClear ? TILE_LAYER_WIDTH : MAX_CHAR_ACROSS;

		if (row == AUTO_CALC_ROW)
		{
			preparedStartLine = preparedRows < TEXT_ROWS ?
				(TEXT_ROWS - preparedRows) / 2 : 0;
		}
		else
		{
			preparedStartLine = row;
		}

		if (col == AUTO_CALC_COLUMN)
		{
			preparedColumn = maximumLineWidth < displayWidth ?
				(displayWidth - maximumLineWidth) / 2 : 0;
		}
		else
		{
			preparedColumn = col;
		}

		if (preparedStartLine > lastLogicalRow || preparedColumn >= displayWidth)
		{
			goto message_rejected;
		}
		availableRows = lastLogicalRow - preparedStartLine + 1;
		availableColumns = displayWidth - preparedColumn;
		if (maximumLineWidth > availableColumns || preparedRows > availableRows)
		{
			b3ClipPreparedText(preparedBuffer, availableColumns, availableRows);
			if (!preparedBuffer[0]) goto message_rejected;
			preparedRows = 1;
			for (preparedChar = preparedBuffer; *preparedChar; ++preparedChar)
			{
				if (*preparedChar == NEW_LINE) ++preparedRows;
			}
		}
		if (preparedRows > 255) goto message_rejected;

		currentTextBuffer = preparedBuffer;
		b3LastBoxLines = (byte)preparedRows;
		b3LastBoxStartLine = (byte)preparedStartLine;
		b3LastBoxFirstRow = firstRow;
		physicalRow = firstRow + preparedStartLine - 1;
		displayTextAddressToCopyTo = MAPBASE + physicalRow * TILE_LAYER_BYTES_PER_ROW + preparedColumn * BYTES_PER_CELL;
		b3PaletteAddress = MAPBASE + physicalRow * TILE_LAYER_BYTES_PER_ROW + 1; //We will set the same palette for the whole row
		b3PaletteRows = preparedRows;
		b3PaletteNumber = paletteNumber;

		if (preparedRows == 1 && preparedChar - preparedBuffer <= MAX_CHAR_ACROSS)
		{
			b3DisplayShortText();
		}
		else
		{
			b6SetAndWaitForIrqState(DISPLAY_TEXT);
		}
		return;
	}

	message_rejected:
	return;
}


byte b3DisplayManuallyWrappedMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth, byte boxHeight)
{
	byte ch;

	b3DisplayMessageBox(message, messageBank, AUTO_CALC_ROW, AUTO_CALC_COLUMN, TEXTBOX_PALETTE_NUMBER, boxWidth, FALSE, FIRST_TEXT_ROW);
	b3LastBoxLines = boxHeight;

	//b3PrintMessageInTextbox(loadAndIncWinCode(), DEFAULT_TEXTBOX_X, DEFAULT_TEXTBOX_Y, DEFAULT_BOX_WIDTH);


	do {
		GET_IN(ch);
		ch >> 8;
	} while ((ch != KEY_ESC) && (ch != KEY_ENTER));

	b3ClearLastPlacedText();

	return ch;
}

byte b3SetTextColor(byte foreground, byte background)
{
	int textId = ((int)foreground << 4) + background;
	unsigned int foreColorBytes, backColorBytes;
	long paletteWriteAddress;
	byte paletteSlot;
	PaletteGetResult palleteGetResult;

	paletteSlot = bFGetPalette(BASE_TEXT_ID + textId, &palleteGetResult);

#ifdef VERBOSE_SET_PALETTE
	printf("fore of %d and back of %d\n", foreground, background);
	printf("\n1. the palette slot is %d. the get palette result is %d\n", paletteSlot, palleteGetResult);
#endif

	if (palleteGetResult == FailToAllocate)
	{
		return paletteSlot;
	}

	paletteWriteAddress = PALETTE_START + COLOURS_PER_PALETTE * BYTES_PER_PALETTE_COLOUR * paletteSlot;

	b3CurrentBackgroundColour = background;
	b3CurrentForegroundColour = foreground;

	if (palleteGetResult == Allocated)
	{
		b6EnterForegroundVera();
		foreColorBytes = b6SetPaletteToInt(foreground);
		backColorBytes = b6SetPaletteToInt(background);

#ifdef VERBOSE_SET_PALETTE
		printf("1 the palette slot is %d. the fore color is %p and back is %p. the address of background is %p\n", paletteSlot, foreColorBytes, backColorBytes, &backColorBytes);
#endif

		SET_VERA_ADDRESS_ABSOLUTE(paletteWriteAddress + 2, 0, 1); //Skip the transparen
		WRITE_BYTE_VAR_TO_ASSM(foreColorBytes, VERA_data0);
		WRITE_BYTE_VAR_TO_ASSM(foreColorBytes >> 8, VERA_data0);
		WRITE_BYTE_VAR_TO_ASSM(backColorBytes, VERA_data0);
		WRITE_BYTE_VAR_TO_ASSM(backColorBytes >> 8, VERA_data0);

		b6LeaveForegroundVera();
	}

#ifdef VERBOSE_SET_PALETTE
	printf("3 the palette slot is %d. the fore color is %p and back is %p\n", paletteSlot, foreColorBytes, backColorBytes);
#endif

	return paletteSlot;
}

void b3ClearLastPlacedText()
{
	byte previousLines, previousStartLine, previousFirstRow;
	unsigned int lastLine;

	if (!b3LastBoxLines)
	{
		return;
	}

	previousLines = b3LastBoxLines;
	previousStartLine = b3LastBoxStartLine;
	previousFirstRow = b3LastBoxFirstRow;
	lastLine = (unsigned int)previousStartLine + previousLines - 1;

#ifdef VERBOSE_DISPLAY_TEXT
	printf("Trying to clear at start line %d number of lines %d end line %d \n", previousStartLine, previousLines, lastLine);
#endif

	b3FillCharAt(previousFirstRow, previousStartLine, lastLine,
		TEXTBOX_PALETTE_NUMBER, TRANSPARENT_CHAR);
	b3LastBoxLines = previousLines;
	b3LastBoxStartLine = previousStartLine;
	b3LastBoxFirstRow = previousFirstRow;
}

#pragma code-name (pop)
