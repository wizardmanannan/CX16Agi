#include "graphics.h"
#pragma code-name (push, "BANKRAM06")

//#define VERBOSE_CHAR_SET_LOAD

#ifdef VERBOSE_CHAR_SET_LOAD
byte printOn = FALSE;
int byteCounter = 0;
#endif
void b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(byte* romAddress, byte** storeWhere)
{
    byte i;
    int j; //Must be int because it needs to be unsigned
    byte romPixel, romPixelRow;
    byte resultByteShift = 0;

    for (i = 0; i < SIZE_PER_CHAR_CHAR_SET_ROM; i++)
    {
#ifdef VERBOSE_CHAR_SET_LOAD
        if (printOn)
        {
            PRINTF("set pixelrow = romAddress[i] (%d) (%p) \n", romAddress[i], &romAddress[i]);
        }
#endif // VERBOSE
   
        romPixelRow = romAddress[i];

        for (j = 7; j >= 0; j--)
        {
            romPixel = romPixelRow >> j & 1;

            if (!romPixel)
            {
                romPixel = 2; //Note: We have four colors trans:0,b:1,w:2,red:4. Therefore a value of 0 (white in the ROM needs to become 2)
            }

#ifdef VERBOSE_CHAR_SET_LOAD
            if (printOn)
            {
              PRINTF("&storewhere is %p\n", *storeWhere);
            }
#endif

            **storeWhere |= (romPixel << resultByteShift);

#ifdef VERBOSE_CHAR_SET_LOAD
            if (printOn)
            {
              PRINTF("storewhere = %p << %p (%p)\n", romPixel, resultByteShift, **storeWhere);
            }
#endif // VERBOSE_CHAR_SET_LOAD

            resultByteShift += 2;
            if (resultByteShift == 8)
            {
                resultByteShift = 0;
                (*storeWhere)++;
#ifdef VERBOSE_CHAR_LOAD
                byteCounter++;
#endif
            }
        }
    }
}

byte* b6InitCharset()
{
    byte previousRomBank = ROM_BANK;
    byte* newCharset = malloc(SIZE_OF_CHARSET);
    int i;
    //byte nonSequencedCharsToGet[9] = {31, 32, 38, 39, 40};


    memset(newCharset, 0, SIZE_OF_CHARSET);
    printf("mallocing : %p \n", newCharset);

#define SPACE (32 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define EQ_MARK (33 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define QUOTE (34 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define OPEN_BRACKET (40 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define SEMI_COLON (60 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define QUESTION_MARK (64 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define AT 0
#define UPPER_A (64 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define UPPER_Z (88 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define LOWER_A (1 * SIZE_PER_CHAR_CHAR_SET_ROM)
#define LOWER_Z (24 * SIZE_PER_CHAR_CHAR_SET_ROM)

    printf("Initializing CharSet. . .\n");

    ROM_BANK = CHARSET_ROM;

#ifdef VERBOSE_CHAR_SET_LOAD
    PRINTF("The address of new charset buffer is %p\n", newCharset);
#endif // VERBOSE_CHAR_SET_LOAD

    for (i = 0; i < NO_CHARS; i++)
    {
        b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(& CHAR_SET_ROM[i], &newCharset);
    }
       

    ROM_BANK = previousRomBank;

    newCharset -= SIZE_OF_CHARSET;

    //Transparent
    memset(newCharset, TRANSPARENT_CHAR, BYTES_PER_CHARACTER);

    printf("returning : %p. The byte counter is %d\n.", newCharset, byteCounter);

    return newCharset;
}
#pragma code-name (pop)
