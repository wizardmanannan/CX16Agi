#include "textLayer.h"
#include "stringFormat.h"
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
char b3TextBuffer1[TEXTBUFFER_SIZE];
char b3TextBuffer2[TEXTBUFFER_SIZE];
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

/* Adapted from t3-text-cleanup-max-height: chunk before staging is exhausted. */
static void b3FillCharAt(byte firstRow, byte startLine, unsigned int endLine,
                       byte paletteNumber, byte charToFill)
{
    unsigned int line, chunkStart, chunkLast, lastMapLine;
    const unsigned int rowsPerChunk = TEXTBUFFER_SIZE / (TILE_LAYER_WIDTH + 1);
    char* buffer;
    if (!firstRow || firstRow > TILE_LAYER_HEIGHT) return;
    lastMapLine = TILE_LAYER_HEIGHT - firstRow;
    if (startLine > lastMapLine || startLine > endLine) return;
    if (endLine > lastMapLine) endLine = lastMapLine;
    for (line = startLine; line <= endLine;) {
        chunkStart = line;
        chunkLast = line + rowsPerChunk - 1;
        if (chunkLast > endLine) chunkLast = endLine;
        buffer = b3TextBuffer1;
        while (line <= chunkLast) {
            /* cc65's memset is assembly; no per-character C copy loop. */
            memset(buffer, charToFill, TILE_LAYER_WIDTH);
            buffer += TILE_LAYER_WIDTH;
            if (line < chunkLast) *buffer++ = NEW_LINE;
            line++;
        }
        *buffer = 0;
        b3DisplayMessageBox(b3TextBuffer1, TEXT_CODE_BANK, chunkStart, 0,
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
	byte numberOfLines = 1;
	*maxWidth = 0;

	for (p = line_start; *p; p++) {
		if (*p == ' ') {
			last_space = p;
		}

		if (p - line_start > width && last_space || *p == NEW_LINE) {
			if (*p != NEW_LINE)
			{
				*last_space = NEW_LINE;
				if (*maxWidth < (last_space - line_start) + 1)
				{
					*maxWidth = (last_space - line_start) + 1;
				}
			}

			line_start = (*p == NEW_LINE ? p : last_space) + 1;
			last_space = 0;
			b3LastBoxLines++;

			numberOfLines++;
		}
	}

	return numberOfLines;
}

void b3DrawBorder(byte boxWidth, size_t messageSize)
{
	byte i;
	char* currentCharToWrite = &b3TextBuffer2[0], * charToReadNext, * nextLine;
	int segmentLength;
	char* leftBarPosition;
	size_t rowBytes;
	byte rowsLeft, rowsWritten = 0;

	(void)messageSize;
	if (boxWidth < 2) boxWidth = 2;
	if (boxWidth > TILE_LAYER_WIDTH - 2) boxWidth = TILE_LAYER_WIDTH - 2;
	rowBytes = (size_t)boxWidth + 2;
	/* Reserve top/bottom borders, padding rows and the terminator. */
	rowsLeft = (byte)(TEXTBUFFER_SIZE / rowBytes - 4);
	if (rowsLeft > TEXT_ROWS - 4) rowsLeft = TEXT_ROWS - 4;

	for (i = 0; i <= boxWidth; i++)
	{
		*currentCharToWrite++ = TOP_BORDER;
	}

	*currentCharToWrite++ = NEW_LINE;

	*currentCharToWrite++ = LEFT_BORDER;

	for (i = 0; i <= boxWidth - 2; i++)
	{
		*currentCharToWrite++ = SPACE;
	}

	*currentCharToWrite++ = RIGHT_BORDER;

	*currentCharToWrite++ = NEW_LINE;

	charToReadNext = b3TextBuffer1;

	while (charToReadNext && rowsLeft)
	{
		leftBarPosition = currentCharToWrite;
		*currentCharToWrite++ = LEFT_BORDER;

		/* strchr/strlen use cc65's assembly routines and retain empty rows. */
		nextLine = strchr(charToReadNext, NEW_LINE);
		segmentLength = nextLine ? nextLine - charToReadNext : strlen(charToReadNext);
		if (segmentLength > boxWidth - 1) segmentLength = boxWidth - 1;
		memcpy(currentCharToWrite, charToReadNext, segmentLength);

		currentCharToWrite += segmentLength;

		charToReadNext = nextLine ? nextLine + 1 : NULL;

		while (currentCharToWrite < leftBarPosition + boxWidth)
		{
			*currentCharToWrite++ = SPACE;
		}

		*currentCharToWrite++ = RIGHT_BORDER;
		*currentCharToWrite++ = NEW_LINE;

		rowsWritten++;
		rowsLeft--;
	}


	*currentCharToWrite++ = LEFT_BORDER;

	for (i = 0; i <= boxWidth - 2; i++)
	{
		*currentCharToWrite++ = SPACE;
	}

	*currentCharToWrite++ = RIGHT_BORDER;

	*currentCharToWrite++ = NEW_LINE;

	for (i = 0; i <= boxWidth; i++)
	{
		*currentCharToWrite++ = BOTTOM_BORDER;
	}
	*currentCharToWrite++ = '\0';

	b3LastBoxLines = rowsWritten + 4;
}

extern void __fastcall__ b3ApplyTextModeControls(char* text);
extern byte __fastcall__ b3ClipTextToMap(char* text);
extern byte textClipColumns, textClipRows;

extern unsigned long displayTextAddressToCopyTo;
extern char* currentTextBuffer;
extern long b3PaletteAddress;
extern byte b3PaletteRows;
extern byte b3PaletteNumber;

//Box width is 0 for text that is not in a box, or the width of the box otherwise
//Supports copying from banks or putting data directly into textbuffer, which is on bank 3
void b3DisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth, boolean wrap, byte firstRow)
{
	int autoRow;
	size_t messageSize = strLenBanked(message, messageBank) + 1;
	long displayAddressCopyPaletteTo;
	byte textWidth = boxWidth;
	byte numberOfLines = 1, maxWidth;
	size_t maxMessageSize = boxWidth ? TEXTBUFFER_SIZE : TEXTBUFFER_SIZE * 2; //If there is no box, we can overflow into buffer 2 for a bigger message.

	currentTextBuffer = b3TextBuffer1;

	b3LastBoxLines = 0;
	if (!firstRow || firstRow > TILE_LAYER_HEIGHT) return;
	if (boxWidth && boxWidth < 4) boxWidth = 4;
	if (boxWidth > TILE_LAYER_WIDTH) boxWidth = TILE_LAYER_WIDTH;

	if (messageSize > 1) //Agi sometimes has empty messages. We say greater than 1 because of the terminator
	{
		displayAddressCopyPaletteTo = displayTextAddressToCopyTo + 1;

#ifdef VERBOSE_DISPLAY_TEXT
		printf("Address: %p + (%d + %d) * %d + %d * %d = %lx\n", MAPBASE, FIRST_ROW, row, TILE_LAYER_BYTES_PER_ROW, col, BYTES_PER_CELL, displayTextAddressToCopyTo);
		printf("displayAddressCopyPaletteTo = %lx\n", displayAddressCopyPaletteTo);
#endif

#ifdef VERBOSE_DISPLAY_TEXT
		printf("Copy message %p on bank %d of size %d\n", message, messageBank, messageSize);
		printf("row %d and col is %d", row, col);
#endif // VERBOSE_DISPLAY_TEXT

		if (message != (char*)b3TextBuffer1)
		{
			/* Replacement text must participate in wrapping and box sizing. */
			expandFormatCodes(message, messageBank, b3TextBuffer1, TEXT_CODE_BANK, maxMessageSize);
			messageSize = strLenBanked(b3TextBuffer1, TEXT_CODE_BANK) + 1;
		}
		if (screenMode == AGI_TEXT) {
			b3ApplyTextModeControls(b3TextBuffer1);
			messageSize = strlen(b3TextBuffer1) + 1;
		}
		if (messageSize == 1) return;

		b3LastBoxLines = 1;
		if (boxWidth)
		{
			b3LastBoxLines += 4; //Account for the top and bottom border, plus padding at the top at the bottom
		}

		if (messageSize - 1 > TILE_LAYER_WIDTH / 2)
		{
			if (wrap)
			{
				if (boxWidth)
				{
					textWidth = boxWidth - 4;
				}

				numberOfLines = b3WrapText(b3TextBuffer1, boxWidth ? textWidth : TILE_LAYER_WIDTH, &maxWidth);

				if (boxWidth && maxWidth)
				{
					boxWidth = maxWidth + 2;
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
			numberOfLines = b3LastBoxLines - 4;
			currentTextBuffer = b3TextBuffer2;
		}

		if (row == AUTO_CALC_ROW)
		{
			autoRow = TEXT_ROWS / 2 - (numberOfLines + 2) / 2;
			b3LastBoxStartLine = autoRow < 0 ? 0 : autoRow;
		}
		else
		{
			b3LastBoxStartLine = row;
		}

		if (col == AUTO_CALC_COLUMN)
		{
			col = boxWidth > MAX_CHAR_ACROSS ? 0 : MAX_CHAR_ACROSS / 2 - boxWidth / 2;
		}

		if (b3LastBoxStartLine > TILE_LAYER_HEIGHT - firstRow || col >= TILE_LAYER_WIDTH) {
			b3LastBoxLines = 0;
			return;
		}
		textClipColumns = TILE_LAYER_WIDTH - col;
		textClipRows = TILE_LAYER_HEIGHT - (firstRow + b3LastBoxStartLine - 1);
		numberOfLines = b3ClipTextToMap(currentTextBuffer);
		b3LastBoxLines = numberOfLines;
		b3LastBoxFirstRow = firstRow;
		displayTextAddressToCopyTo = MAPBASE + (firstRow + b3LastBoxStartLine - 1) * TILE_LAYER_BYTES_PER_ROW + col * BYTES_PER_CELL;
		b3PaletteAddress = MAPBASE + (firstRow + b3LastBoxStartLine - 1) * TILE_LAYER_BYTES_PER_ROW + 1; //We will set the same palette for the whole row
		b3PaletteRows = numberOfLines;
		b3PaletteNumber = paletteNumber;

		b6SetAndWaitForIrqState(DISPLAY_TEXT);
	}
}


byte b3DisplayManuallyWrappedMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth, byte boxHeight)
{
	byte ch;

	b3DisplayMessageBox(message, messageBank, AUTO_CALC_ROW, AUTO_CALC_COLUMN, TEXTBOX_PALETTE_NUMBER, boxWidth, FALSE, FIRST_TEXT_ROW);
	if (b3LastBoxLines) b3LastBoxLines = boxHeight;

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
	int textId = (int)foreground << 4 + background;
	unsigned int foreColorBytes, backColorBytes;
	long paletteWriteAddress;
	byte paletteSlot;
	PaletteGetResult palleteGetResult;
	int textPalette;

	paletteSlot = bFGetPalette(BASE_TEXT_ID + textId, &palleteGetResult);

#ifdef VERBOSE_SET_PALETTE
	printf("fore of %d and back of %d\n", foreground, background);
	printf("\n1. the palette slot is %d. the get palette result is %d\n", paletteSlot, palleteGetResult);
#endif

	paletteWriteAddress = PALETTE_START + COLOURS_PER_PALETTE * BYTES_PER_PALETTE_COLOUR * paletteSlot;
	textPalette = paletteSlot;

	b3CurrentBackgroundColour = background;
	b3CurrentForegroundColour = foreground;

	if (palleteGetResult == Allocated)
	{
		asm("sei");
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

		REENABLE_INTERRUPTS();
	}

#ifdef VERBOSE_SET_PALETTE
	printf("3 the palette slot is %d. the fore color is %p and back is %p\n", paletteSlot, foreColorBytes, backColorBytes);
#endif

	return paletteSlot;
}

void b3ClearLastPlacedText()
{
	byte startLine = b3LastBoxStartLine;
	byte firstRow = b3LastBoxFirstRow;
	unsigned int lastLine;
	if (!b3LastBoxLines) return;

#ifdef VERBOSE_DISPLAY_TEXT
	printf("Trying to clear at start line %d number of lines %d end line %d \n", lastBoxStartLine, b3LastBoxLines, lastBoxStartLine + lastBoxLines - 1);
#endif

	lastLine = (unsigned int)startLine + b3LastBoxLines - 1;
	b3FillCharAt(firstRow, startLine, lastLine, TEXTBOX_PALETTE_NUMBER, TRANSPARENT_CHAR);
	b3LastBoxStartLine = startLine;
	b3LastBoxFirstRow = firstRow;
	b3LastBoxLines = 0;
}

#pragma code-name (pop)
