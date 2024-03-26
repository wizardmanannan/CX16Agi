#include "graphics.h"

#pragma code-name (push, "BANKRAM06")
void b6InitVeraMemory()
{
	long i;
#define INITIAL_VALUE 0

    asm("sei");
	SET_VERA_ADDRESS(SPRITES_DATA_START, 0, 1);

	for (i = SPRITES_DATA_START; i <= VERA_END; i++)
	{
		WRITE_BYTE_DEF_TO_ASSM(0, VERA_data0);
	}
	 REENABLE_INTERRUPTS();
}

unsigned int b6SetPaletteToInt(byte paletteReference)
{
	int result;
	byte low,high;

	SET_VERA_ADDRESS_ABSOLUTE(PALETTE_START + (paletteReference * 2), 0, 1);
	READ_BYTE_VAR_FROM_ASSM(low, VERA_data0);
	READ_BYTE_VAR_FROM_ASSM(high, VERA_data0);

	result = low;
	*(((byte*)&result) + 1) = high;

	return result;
}

#pragma code-name (pop);