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
#pragma code-name (pop);