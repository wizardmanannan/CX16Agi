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

#pragma bss-name (push, "BANKRAM03")
byte _currentForegroundColour;
byte _currentBackgroundColour;
#pragma bss-name (pop)

void b6MakeTopBorder()
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

void b6MakeLeftBorder()
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

void b6MakeBottomBorder()
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

void b6MakeRightBorder()
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

void b6ConvertsOneBitPerPixCharToTwoBitPerPixelChars()
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

void b6InitCharset()
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

	b6ConvertsOneBitPerPixCharToTwoBitPerPixelChars();

	//While we could just flip them it will take less cycles when we write text just to have chars for both, as that way our stride can be two
	b6MakeTopBorder();
	b6MakeLeftBorder();
	b6MakeBottomBorder();
	b6MakeRightBorder();

#ifdef VERBOSE_CHAR_SET_LOAD
	printf("returning : %p. The byte counter is %d\n.", buffer, byteCounter);
#endif // VERBOSE_CHAR_SET_LOAD

}

#ifdef TEST_CHARSET
void b6TestCharset()
{
	int i;
	byte j;
	byte* veraDcVideo = (byte*) VERA_DCVIDEO;

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



#pragma code-name (pop)

#pragma code-name (push, "BANKRAM03")
void b3FillChar(byte startLine, byte endLine, byte paletteNumber, byte charToFill)
{
	byte i, j;

	char* clearBuffer = textBuffer1;

	for (i = startLine; i <= endLine; i++)
	{
		for (j = 0; j < TILE_LAYER_WIDTH; j++)
		{
			*clearBuffer = charToFill;
			clearBuffer++; // Moved out from dereferencing for clarity
		}

		// It's a good idea to use brackets {} for multiline if-else statements
		if (clearBuffer > &clearBuffer[LOCAL_WORK_AREA_SIZE] - TILE_LAYER_WIDTH - 1
			|| i == endLine) // Minus one so the terminator can fit in
		{
			*clearBuffer = '\0';
			b3DisplayMessageBox(textBuffer1, TEXT_CODE_BANK, startLine, 0, paletteNumber, 0);
		}
		else
		{
			*clearBuffer = NEW_LINE;
			clearBuffer++; // Increment the buffer after adding '\n' to avoid potential buffer overruns in the next iteration.
		}
	}
}

extern byte lastBoxLines;
extern byte lastBoxStartLine;

//Thanks to https://www.rosettacode.org/wiki/Word_wrap#In-place_greedy
//Agi text does not have newlines and requires the programmer to manually wrap the text
byte b3WrapText(char* line_start, int width) {
	char* last_space = 0;
	char* p;
	byte numberOfLines = 1;

	for (p = line_start; *p; p++) {
		if (*p == ' ') {
			last_space = p;
		}

		if (p - line_start > width && last_space || *p == NEW_LINE) {
			if (*p != NEW_LINE)
			{
				*last_space = NEW_LINE;
			}
			
			line_start = last_space + 1;
			last_space = 0;
			lastBoxLines++;

		    numberOfLines++;
		}
	}

	return numberOfLines;
}

void b3DrawBorder(byte boxWidth, size_t messageSize)
{
	byte i;
	char* currentCharToWrite = &textBuffer2[0], *charToReadNext;
	int segmentLength;
	char* leftBarPosition;
	char delimiter[2];

	delimiter[0] = NEW_LINE;
	delimiter[1] = '\0';

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

	charToReadNext = strtok(textBuffer1, delimiter);
	
	do
	{
		leftBarPosition = currentCharToWrite;
		*currentCharToWrite++ = LEFT_BORDER;

	    segmentLength = strlen(charToReadNext);
		memcpy(currentCharToWrite, charToReadNext, segmentLength);

		currentCharToWrite += segmentLength;

		charToReadNext = strtok(NULL, delimiter);

		while (currentCharToWrite < leftBarPosition + boxWidth)
		{
			*currentCharToWrite++ = SPACE;
		}

		*currentCharToWrite++ = RIGHT_BORDER;
		*currentCharToWrite++ = NEW_LINE;

	} while (charToReadNext);


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

	if (currentCharToWrite > &textBuffer2[0] + TEXTBUFFER_SIZE - 1)
	{
		printf("Bounds check fail. Boxing function");
	}
}

extern unsigned long displayTextAddressToCopyTo;
extern char* currentTextBuffer;
extern long b3PaletteAddress;
extern byte b3PaletteRows;
extern byte b3PaletteNumber;

//Box width is 0 for text that is not in a box, or the width of the box otherwise
//Supports copying from banks or putting data directly into textbuffer, which is on bank 3
void b3DisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth)
{
	int i;
	char terminator = 0;
	size_t messageSize = strLenBanked(message, messageBank) + 1;
	long displayAddressCopyPaletteTo;
	byte textWidth = boxWidth;
	byte numberOfLines = 1;
	size_t maxMessageSize = boxWidth ? TEXTBUFFER_SIZE : TEXTBUFFER_SIZE * 2; //If there is no box, we can overflow into buffer 2 for a bigger message.
	
	currentTextBuffer = textBuffer1;

	lastBoxStartLine = row;
	lastBoxLines = 1;

	if (boxWidth)
	{
		lastBoxLines += 4; //Account for the top and bottom border, plus padding at the top at the bottom
	}

	if (messageSize > 1) //Agi sometimes has empty messages. We say greater than 1 because of the terminator
	{
		displayTextAddressToCopyTo = MAPBASE + (FIRST_ROW + row - 1) * TILE_LAYER_BYTES_PER_ROW + col * BYTES_PER_CELL;
		displayAddressCopyPaletteTo = displayTextAddressToCopyTo + 1;

#ifdef VERBOSE_DISPLAY_TEXT
		printf("Address: %p + (%d + %d) * %d + %d * %d = %lx\n", MAPBASE, FIRST_ROW, row, TILE_LAYER_BYTES_PER_ROW, col, BYTES_PER_CELL, displayTextAddressToCopyTo);
		printf("displayAddressCopyPaletteTo = %lx\n", displayAddressCopyPaletteTo);
#endif
		if (messageSize > maxMessageSize)
		{
			memCpyBanked((byte*)message + maxMessageSize - 1, (byte*)&terminator, messageBank, 1);
			printf("warning overflow on message. the message size is %d.\n", messageSize);
		}

#ifdef VERBOSE_DISPLAY_TEXT
		printf("Copy message %p on bank %d of size %d\n", message, messageBank, messageSize);
		printf("row %d and col is %d", row, col);
#endif // VERBOSE_DISPLAY_TEXT

		if (message != (char*) textBuffer1)
		{
			memCpyBankedBetween((byte*)textBuffer1, TEXT_CODE_BANK, (byte*)message, messageBank, messageSize);
		}

		if (messageSize - 1 > TILE_LAYER_WIDTH / 2)
		{
			if (boxWidth)
			{
				textWidth = boxWidth - 4;
			}

			numberOfLines = b3WrapText(textBuffer1, boxWidth ? textWidth : TILE_LAYER_WIDTH);
		}

		if (boxWidth)
		{
			b3DrawBorder(boxWidth - 2, messageSize);
			currentTextBuffer = textBuffer2;
		}

		b3PaletteAddress = MAPBASE + (FIRST_ROW + row - 1) * TILE_LAYER_BYTES_PER_ROW + 1; //We will set the same palette for the whole row
		b3PaletteRows = numberOfLines;
		b3PaletteNumber = paletteNumber;

		b6SetAndWaitForIrqState(DISPLAY_TEXT);
	}
}

byte b3SetTextColor(byte foreground, byte background)
{
	int textId = (int)foreground << 4 + background;
	unsigned int foreColorBytes, backColorBytes;
	long paletteWriteAddress;
	byte paletteSlot;
    PaletteGetResult palleteGetResult;
	int textPalette;

	paletteSlot = bEGetPalette(BASE_TEXT_ID + textId, &palleteGetResult);

	#ifdef VERBOSE_SET_PALETTE
	printf("fore of %d and back of %d\n", foreground, background);
	printf("\n1. the palette slot is %d. the get palette result is %d\n", paletteSlot, palleteGetResult);
    #endif

	paletteWriteAddress = PALETTE_START + COLOURS_PER_PALETTE * BYTES_PER_PALETTE_COLOUR * paletteSlot;
	textPalette = paletteSlot;

	_currentBackgroundColour = background;
	_currentForegroundColour = foreground;

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
#ifdef VERBOSE_DISPLAY_TEXT
	printf("Trying to clear at start line %d number of lines %d end line %d \n", lastBoxStartLine, lastBoxLines, lastBoxStartLine + lastBoxLines - 1);
#endif

	b3FillChar(lastBoxStartLine, lastBoxStartLine + lastBoxLines - 1, TEXTBOX_PALETTE_NUMBER, TRANSPARENT_CHAR);
}

#pragma code-name (pop)
