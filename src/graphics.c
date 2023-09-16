#include "graphics.h"
#pragma code-name (push, "BANKRAM06")

//#define VERBOSE_CHAR_SET_LOAD

#ifdef VERBOSE_CHAR_SET_LOAD
byte printOn = FALSE;
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

            resultByteShift++;
            if (resultByteShift == 8)
            {
                resultByteShift = 0;
                (*storeWhere)++;
            }
        }
    }
}

void b6InitCharset()
{
    byte previousRomBank = ROM_BANK;
    byte* newCharset = malloc(CHARSET_TOTAL_SIZE);
    int i;
    //byte nonSequencedCharsToGet[9] = {31, 32, 38, 39, 40};

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

    printf("Initializing CharSet. . .");

    ROM_BANK = CHARSET_ROM;

#ifdef VERBOSE_CHAR_SET_LOAD
    PRINTF("The address of new charset buffer is %p\n", newCharset);
#endif // VERBOSE_CHAR_SET_LOAD


    //Transparent
    memset(newCharset, 0, BYTES_PER_CHARACTER);
    newCharset += 2;

    b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[SPACE], &newCharset);

    b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[EQ_MARK], &newCharset);
    b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[QUOTE], &newCharset);

    for (i = OPEN_BRACKET; i <= SEMI_COLON; i = i + SIZE_PER_CHAR_CHAR_SET_ROM)
    {
        b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[i], &newCharset);
    }

    b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[QUESTION_MARK], &newCharset);
    b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[AT], &newCharset);

    for (i = UPPER_A; i <= UPPER_Z; i = i + SIZE_PER_CHAR_CHAR_SET_ROM)
    {
        b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[i], &newCharset);
    }

    for (i = LOWER_A; i <= LOWER_Z; i = i + SIZE_PER_CHAR_CHAR_SET_ROM)
    {
        b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(&CHAR_SET[i], &newCharset);
    }

    //The top border char Which is a red border on the top, and zero everywhere else
    *newCharset++ = 0xAA;
    *newCharset++ = 0xAA;
    memset(newCharset, 0, BYTES_PER_CHARACTER - 2);

    //The side border character in which every row starts with a red pixel
    memset(newCharset, 0x4000, ROWS_PER_CHARACTER);

    ROM_BANK = previousRomBank;
}
#pragma code-name (pop)
