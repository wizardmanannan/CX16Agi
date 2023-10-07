#include "textLayer.h"
#pragma code-name (push, "BANKRAM06")

//#define VERBOSE_CHAR_SET_LOAD
//#define TEST_CHARSET
//#define VERBOSE_DISPLAY_TEXT
#ifdef VERBOSE_CHAR_SET_LOAD
byte printOn = TRUE;
int byteCounter = 0;
#endif

//must be constants
#define SET_VERA_ADDRESS(VeraAddress, AddressSel, Stride) \
    do {      \
        asm("lda #%w", AddressSel);                  \
        asm("sta %w", VERA_ctrl);                  \
        asm("lda #%w", Stride << 4);                           \
        asm("ora #^%l", VeraAddress);           \
        asm("sta %w", VERA_addr_bank);             \
        asm("lda #< %l", VeraAddress);          \
        asm("sta %w", VERA_addr_low);             \
        asm("lda #> %l", VeraAddress);          \
        asm("sta %w", VERA_addr_high);              \
    } while(0)


#define SET_VERA_ADDRESS_ABSOLUTE(VeraAddress, AddressSel, Stride) \
    do {      \
				_assmLong = displayAddressCopyPaletteTo; \
        asm("lda #%w", AddressSel); \
		asm("sta %w", VERA_ctrl); \
		asm("lda %v + 2", _assmLong); \
		asm("and #$1"); \
		asm("ora #%w", Stride << 4); \
		asm("sta %w", VERA_addr_bank); \
		asm("lda %v", _assmLong); \
		asm("sta %w", VERA_addr_low); \
		asm("lda %v + 1", _assmLong); \
		asm("sta %w", VERA_addr_high); \
	} while (0)


void b6MakeBottomBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + (BOTTOM_BORDER - 1) * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i < BYTES_PER_CHARACTER - BYTES_PER_CELL)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White square
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

	SET_VERA_ADDRESS(TILEBASE + (LEFT_BORDER - 1) * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

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

void b6MakeTopBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + (TOP_BORDER - 1) * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

	for (i = 0; i < BYTES_PER_CHARACTER; i++)
	{
		if (i < BYTES_PER_CELL)
		{
			WRITE_BYTE_DEF_TO_ASSM(0b11111111, VERA_data0); //Red line
		}
		else
		{
			WRITE_BYTE_DEF_TO_ASSM(0b10101010, VERA_data0); //White square
		}
	}
}

void b6MakeRightBorder()
{
	byte i;

	SET_VERA_ADDRESS(TILEBASE + (RIGHT_BORDER - 1) * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

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

	printf("Initializing CharSet. . .\n");

	SCREEN_SET_CHAR_SET(ISO);

#ifdef VERBOSE_CHAR_SET_LOAD
	PRINTF("The address of new charset buffer is %p\n", buffer);
#endif // VERBOSE_CHAR_SET_LOAD

	SET_VERA_ADDRESS(ORIGINAL_CHARSET_ADDRESS, ADDRESSSEL0, 1);
	SET_VERA_ADDRESS(TILEBASE, ADDRESSSEL1, 1);

	b6ConvertsOneBitPerPixCharToTwoBitPerPixelChars();

	//While we could just flip them it will take less cycles when we write text just to have chars for both, as that way our stride can be two
	b6MakeBottomBorder();
	b6MakeLeftBorder();
	b6MakeTopBorder();
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
	SET_VERA_ADDRESS_PICTURE(MAPBASE, ADDRESSSEL0, 2);

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
}
#endif

void b6InitLayer1Mapbase()
{
	int i;
#define BYTE1 TRANSPARENT_CHAR
#define BYTE2 0x10 //1 Offset 0 v flip 0 h flip 0 tile index bit 8 and 9

	SET_VERA_ADDRESS(MAPBASE, ADDRESSSEL0, 1);

	for (i = 0; i < TILE_LAYER_NO_TILES; i++)
	{
		WRITE_BYTE_DEF_TO_ASSM(BYTE1, VERA_data0);
		WRITE_BYTE_DEF_TO_ASSM(BYTE2, VERA_data0);
	}

#ifdef TEST_CHARSET
	b6TestCharset();
#endif // TEST_CHARSET

}

#pragma code-name (pop)

#pragma code-name (push, "BANKRAM03")
void b3FillChar(byte startLine, byte endLine, byte paletteNumber, byte charToFill)
{
	byte i,j;

	char* clearBuffer = (char*)GOLDEN_RAM_WORK_AREA;

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
			b3DisplayMessageBox((char*)GOLDEN_RAM_WORK_AREA, 0, startLine, 0, paletteNumber, 0);
		}
		else
		{
			*clearBuffer = NEW_LINE;
			clearBuffer++; // Increment the buffer after adding '\n' to avoid potential buffer overruns in the next iteration.
		}
	}
}


void wrap_text(char* line_start, int width) {
	char* last_space = 0;
	char* p;

	for (p = line_start; *p; p++) {
		if (*p == '\n') {
			line_start = p + 1;
		}

		if (*p == ' ') {
			last_space = p;
		}

		if (p - line_start > width && last_space) {
			*last_space = NEW_LINE;
			line_start = last_space + 1;
			last_space = 0;
		}
	}
}

extern unsigned long displayTextAddressToCopyTo;
void b3DisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth) //Even though message is 
{
	int i;
	char terminator = 0;
	size_t messageSize = strLenBanked(message, messageBank) + 1;
	long displayAddressCopyPaletteTo;
	byte paletteByte = paletteNumber << 4;

	if (messageSize > 1) //Agi sometimes has empty messages. We say greater than 1 because of the terminator
	{
		displayTextAddressToCopyTo = MAPBASE + (FIRST_ROW + row - 1) * TILE_LAYER_BYTES_PER_ROW + col * BYTES_PER_CELL;
		displayAddressCopyPaletteTo = displayTextAddressToCopyTo + 1;

#ifdef VERBOSE_DISPLAY_TEXT
		printf("Address: %p + (%d + %d) * %d + %d * %d = %lx\n", MAPBASE, FIRST_ROW, row, TILE_LAYER_BYTES_PER_ROW, col, BYTES_PER_CELL, displayTextAddressToCopyTo);
		printf("displayAddressCopyPaletteTo = %lx\n", displayAddressCopyPaletteTo);
#endif
		if (messageSize > TEXTBUFFER_SIZE)
		{
			memCpyBanked((byte*)message + TEXTBUFFER_SIZE, (byte*)&terminator, messageBank, 1);
			printf("Warning overflow on message\n");
		}

#ifdef VERBOSE_DISPLAY_TEXT
		printf("Copy message %p on bank %d of size %d\n", message, messageBank, messageSize);
		printf("row %d and col is %d", row, col);
#endif // VERBOSE_DISPLAY_TEXT

		memCpyBankedBetween(TEXTBUFFER, TEXT_BANK, (byte*)message, messageBank, messageSize);

		if (messageSize - 1 > TILE_LAYER_WIDTH)
		{
			wrap_text((char*) TEXTBUFFER, boxWidth ? boxWidth : TILE_LAYER_WIDTH);
		}

		SET_VERA_ADDRESS_ABSOLUTE(displayAddressCopyPaletteTo, 0, 2);
		for (i = 0; i < messageSize - 1; i++) //Ignore the terminator it doesn't print and doesn't have a palette byte
		{
			WRITE_BYTE_VAR_TO_ASSM(paletteByte, VERA_data0);
		}
		////TODO: Doesn't return anything but I don't want to add any more trampoline methods. Come up with a more memory efficient way of handling this then constanting adding them
				
		trampoline_1ByteRByte(&b6SetAndWaitForIrqState, DISPLAY_TEXT, IRQ_BANK);
	}
}
#pragma code-name (pop)

void trampolinefillChar(byte startLine, byte endLine, byte paletteNumber, byte charToFill)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = TEXT_BANK;

	b3FillChar(startLine, endLine, paletteNumber, charToFill);

	RAM_BANK = previousRamBank;
}

void trampolineDisplayMessageBox(char* message, byte messageBank, byte row, byte col, byte paletteNumber, byte boxWidth)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = TEXT_BANK;

	b3DisplayMessageBox(message, messageBank, row, col, paletteNumber, boxWidth);

	RAM_BANK = previousRamBank;
}
