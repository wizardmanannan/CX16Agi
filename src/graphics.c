#include "graphics.h"
#pragma code-name (push, "BANKRAM06")

//#define VERBOSE_CHAR_SET_LOAD

#ifdef VERBOSE_CHAR_SET_LOAD
byte printOn = TRUE;
int byteCounter = 0;
#endif
void b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(byte* romAddress, byte** buffer)
{
    byte i;
    int j; //Must be int because it needs to be unsigned
    byte romPixel;
    byte resultByteShift = 0;

    for (i = 0; i < SIZE_PER_CHAR_CHAR_SET_ROM; i++)
    {
#ifdef VERBOSE_CHAR_SET_LOAD
        if (printOn)
        {
            PRINTF("set rowPixel = romAddress[i] (%d) (%p) \n", romAddress[i], &romAddress[i]);
        }
#endif // VERBOSE
   
        for (j = 7; j >= 0; j--)
        {
            romPixel = romAddress[i];
#ifdef VERBOSE_CHAR_SET_LOAD
            if (printOn)
            {
                PRINTF("%p >> %d & 1 = %d\n", romPixel, j, romPixel >> j & 1);
            }
#endif // VERBOSE_CHAR_SET_LOAD
            romPixel = romPixel >> j & 1;

            if (!romPixel)
            {
                romPixel = 2; //Note: We have four colors trans:0,b:1,w:2,red:4. Therefore a value of 0 (white in the ROM needs to become 2)
#ifdef VERBOSE_CHAR_SET_LOAD
                if (printOn)
                {
                    PRINTF("Change to 2\n", 0);
                }
#endif // VERBOSE_CHAR_SET_LOAD

            }

#ifdef VERBOSE_CHAR_SET_LOAD
            if (printOn)
            {
                if (printOn)
                {
                    PRINTF("&buffer previously is %p. It's address is %p\n", **buffer, *buffer);
                }
            }
#endif

            **buffer |= (romPixel << resultByteShift);

#ifdef VERBOSE_CHAR_SET_LOAD
            if (printOn)
            {
              PRINTF("buffer or = %p << %p (%p)\n", romPixel, resultByteShift, **buffer);
            }
#endif // VERBOSE_CHAR_SET_LOAD

            resultByteShift += 2;
            if (resultByteShift == 8)
            {
                resultByteShift = 0;
                (*buffer)++;
#ifdef VERBOSE_CHAR_LOAD
                byteCounter++;
#endif
            }
        }
    }
}

void b6InitCharset(byte* buffer)
{
    byte previousRomBank = ROM_BANK;
    int i;
    //byte nonSequencedCharsToGet[9] = {31, 32, 38, 39, 40};


    memset(buffer, 0, SIZE_OF_CHARSET);

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
    PRINTF("The address of new charset buffer is %p\n", buffer);
#endif // VERBOSE_CHAR_SET_LOAD

    for (i = 0; i < NO_CHARS; i++)
    {
        b6ConvertOneBitPerPixCharToTwoBitPerPixelChar(& CHAR_SET_ROM[i], &buffer);
    }
    ROM_BANK = previousRomBank;

    buffer -= SIZE_OF_CHARSET;

    //Transparent
    memset(&buffer[TRANSPARENT_CHAR * BYTES_PER_CHARACTER], 0, BYTES_PER_CHARACTER);

#ifdef VERBOSE_CHAR_SET_LOAD
    printf("returning : %p. The byte counter is %d\n.", buffer, byteCounter);
#endif // VERBOSE_CHAR_SET_LOAD

}
#pragma code-name (pop)
