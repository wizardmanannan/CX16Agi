#include "spriteMemoryManager.h"

#define SEGMENT_SMALL ((32 * 32) / 2)
#define SPRITE_ALLOC_TABLE_SIZE (SPRITES_DATA_END - SPRITES_DATA_START) / SEGMENT_SMALL

extern byte bESpriteAddressTableMiddle[SPRITE_ALLOC_TABLE_SIZE];
extern byte bESpriteHighByteStart;

#pragma code-name (push, "BANKRAM0E")

//#define VERBOSE_MEMORY_INIT 

void bEInitSpriteMemoryManager()
{
	byte i;
	unsigned long address;
	
	byte highByte;
	byte middleByte;

	printf("Initing Sprite Memory Manager");

#ifdef VERBOSE_MEMORY_INIT
	printf("The address of address is %p\n", &address);
	printf("The allocation table size is %d \n", (byte) ((0x1F9BE - 0xEA00) / (32 * 32)) * 2);
#endif

	for (i = 0; i < SPRITE_ALLOC_TABLE_SIZE; i++)
	{
		address = (unsigned long)SEGMENT_SMALL * i + SPRITES_DATA_START;

		printf("Address ((%d * %d) /2) * %d + %p = %lx \n", 32, 32, i, SPRITES_DATA_START, address);

		if (!bESpriteHighByteStart)
		{
			highByte = (byte)(address >> 16);

#ifdef VERBOSE_MEMORY_INIT
			printf("The highByte is %lx >> 16 = %p\n", address, (byte) highByte);
#endif

			if (highByte)
			{
				bESpriteHighByteStart = i;
			}
		}

		middleByte = (byte) ((address >> 8) & 0xFF);
		bESpriteAddressTableMiddle[i] = middleByte;

#ifdef VERBOSE_MEMORY_INIT
		printf("The middle byte is (%lx >> 8) & 0xff = %p. The address of middlebyte is %p\n", address, (byte)(address >> 8) & 0xFF, &middleByte);
#endif
	}

#ifdef VERBOSE_MEMORY_INIT

	printf("Middle starts at %p\n", &bESpriteAddressTableMiddle[0]);

	asm("stp");
#endif
}

#pragma code-name (pop)