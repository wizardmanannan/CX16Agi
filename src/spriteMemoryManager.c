#include "spriteMemoryManager.h"

#define SEGMENT_SMALL ((32 * 32) / 2)
#define SPRITE_ALLOC_TABLE_SIZE ((SPRITES_DATA_END - SPRITES_DATA_START) / SEGMENT_SMALL)

extern byte bESpriteAddressTableMiddle[SPRITE_ALLOC_TABLE_SIZE];
extern byte bESpriteAllocTable[SPRITE_ALLOC_TABLE_SIZE];
extern byte bESpriteHighByteStart;

#pragma code-name (push, "BANKRAM0E")

//#define VERBOSE_MEMORY_INIT 
#define TEST_ALLOCATE_SPRITE_MEMORY

#ifdef TEST_ALLOCATE_SPRITE_MEMORY

extern void bEAllocateSpriteMemory();

void bETestSpriteAllocateSpriteMemory()
{
	bEAllocateSpriteMemory();
}
#endif // TEST_ALLOCATE_SPRITE_MEMORY


void bEResetSpriteMemoryManager()
{
	memset(bESpriteAllocTable, 0, SPRITE_ALLOC_TABLE_SIZE);
}

void bEInitSpriteMemoryManager()
{
	byte i;
	unsigned long address;
	
	byte highByte;
	byte middleByte;

	printf("Initing Sprite Memory Manager");

	bEResetSpriteMemoryManager();

#ifdef VERBOSE_MEMORY_INIT
	printf("The address of address is %p\n", &address);
	printf("alloc size %d\n", (byte)SPRITE_ALLOC_TABLE_SIZE);
#endif

	for (i = 0; i < SPRITE_ALLOC_TABLE_SIZE; i++)
	{
		address = (unsigned long)SEGMENT_SMALL * i + SPRITES_DATA_START;

#ifdef VERBOSE_MEMORY_INIT
		printf("Address ((%d * %d) /2) * %d + %p = %lx \n", 32, 32, i, SPRITES_DATA_START, address);
#endif

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

	*((byte**)ZP_PTR_SPR_ALLOC) = &bESpriteAllocTable[0];
	*((byte**)ZP_PTR_SPR_ADDR) = &bESpriteAddressTableMiddle[0];

#ifdef  TEST_ALLOCATE_SPRITE_MEMORY
	bETestSpriteAllocateSpriteMemory();
#endif //  TEST_ALLOCATE_SPRITE_MEMORY

}

#pragma code-name (pop)