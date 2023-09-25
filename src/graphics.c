#include "graphics.h"
#pragma code-name (push, "BANKRAM06")

//#define VERBOSE_CHAR_SET_LOAD
//#define TEST_CHARSET
#ifdef VERBOSE_CHAR_SET_LOAD
byte printOn = TRUE;
int byteCounter = 0;
#endif

//Stride must be constant
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


void b6MakeBottomBorder()
{
    byte i;

    SET_VERA_ADDRESS(TILEBASE + (BOTTOM_BORDER - 1) * BYTES_PER_CHARACTER, ADDRESSSEL0, 1);

    for (i = 0; i < BYTES_PER_CHARACTER; i++)
    {
        if (i < BYTES_PER_CHARACTER - BYTES_CHAR_PER_ROW)
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

            output  |= (romPixel << resultByteShift);
            
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
    byte* veraData0, *veraData1;
   
    printf("Initializing CharSet. . .\n");

    SCREEN_SET_CHAR_SET(ISO);

#ifdef VERBOSE_CHAR_SET_LOAD
    PRINTF("The address of new charset buffer is %p\n", buffer);
#endif // VERBOSE_CHAR_SET_LOAD

    SET_VERA_ADDRESS(ORIGINAL_CHARSET_ADDRESS, ADDRESSSEL0, 1);
    SET_VERA_ADDRESS(TILEBASE, ADDRESSSEL1, 1);
    
    b6ConvertsOneBitPerPixCharToTwoBitPerPixelChars();

    b6MakeBottomBorder();
    b6MakeLeftBorder();

#ifdef VERBOSE_CHAR_SET_LOAD
    printf("returning : %p. The byte counter is %d\n.", buffer, byteCounter);
#endif // VERBOSE_CHAR_SET_LOAD
 
}

#ifdef TEST_CHARSET
void b6TestCharset()
{
    int i;
    byte j;
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
