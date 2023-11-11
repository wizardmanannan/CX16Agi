#include "spriteMemoryManager.h"

#define SEGMENT_SMALL ((32 * 32) / 2)
#define SPRITE_ALLOC_TABLE_SIZE ((SPRITES_DATA_END - SPRITES_DATA_START) / SEGMENT_SMALL)

extern byte bESpriteAddressTableMiddle[SPRITE_ALLOC_TABLE_SIZE];
extern byte bESpriteAllocTable[SPRITE_ALLOC_TABLE_SIZE];

#pragma code-name (push, "BANKRAM0E")

//#define VERBOSE_MEMORY_INIT 
#define TEST_ALLOCATE_SPRITE_MEMORY

#ifdef TEST_ALLOCATE_SPRITE_MEMORY

extern unsigned long bEAllocateSpriteMemory32();
extern unsigned long bEAllocateSpriteMemory64();

void bEResetSpriteMemoryManager()
{
	memset(bESpriteAllocTable, 0, SPRITE_ALLOC_TABLE_SIZE);


	*((byte*)ZP_PTR_SEG_32) = 0;
	*((byte*)ZP_PTR_SEG_64) = SPRITE_ALLOC_TABLE_SIZE - 2; //64 allocator starts two from the end
}

void bETestSpriteAllocateSpriteMemory32()
{
	byte i;
	unsigned long expected = SPRITES_DATA_START, actual;

	printf("Test Allocate Sprite Memory 32");

	//1. Should allocate every block in sequence
	for (i = 0; i < SPRITE_ALLOC_TABLE_SIZE - 2; i++) //Last two positions is where the 64 allocator starts don't check those
	{
		actual = bEAllocateSpriteMemory32();

		if (actual != expected)
		{
			printf("Fail 1 on i = %d. The result was %lx\n", i, actual);
		}

		expected += SEGMENT_SMALL;
	}

	actual = bEAllocateSpriteMemory32(); 

	//2. Don't allocate when full
	if (actual)
	{
		printf("Fail 2. Expected 0 got %lx\n", actual);
	}

	bEResetSpriteMemoryManager();
}

void bETestSpriteAllocateSpriteMemory64()
{
	byte i;
	unsigned long expected = SPRITE_ALLOC_TABLE_SIZE - 2, actual;

	printf("Test Allocate Sprite Memory 64");

	//1. Should allocate every block in sequence
	for (i = 0; i < SPRITE_ALLOC_TABLE_SIZE - 1; i++) //First position is for 32 bit allocator
	{
		actual = bEAllocateSpriteMemory64();

		if (actual != expected)
		{
			printf("Fail 1 on i = %d. The result was %lx\n", i, actual);
		}

		expected -= SEGMENT_SMALL * 2;
	}

	actual = bEAllocateSpriteMemory64();

	//2. Don't allocate when full
	if (actual)
	{
		printf("Fail 2. Expected 0 got %lx\n", actual);
	}

	bEResetSpriteMemoryManager();
}
//
//void bETestSpriteAllocateSpriteMemory32()
//{
//}

void bETestSpriteAllocateSpriteMemory()
{
	bEAllocateSpriteMemory32();
	bEAllocateSpriteMemory64();
}

#endif // TEST_ALLOCATE_SPRITE_MEMORY


void bEInitSpriteMemoryManager()
{
	byte i;
	unsigned long address;
	
	byte highByte = 0;
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

		if (!highByte)
		{
			highByte = (byte)(address >> 16);

#ifdef VERBOSE_MEMORY_INIT
			printf("The highByte is %lx >> 16 = %p\n", address, (byte) highByte);
#endif

			if (highByte)
			{
				*((byte*)ZP_PTR_HIGH_BYTE_START) = i;
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

#ifdef  TEST_ALLOCATE_SPRITE_MEMORY
	bETestSpriteAllocateSpriteMemory();
	asm("stp");
#endif //  TEST_ALLOCATE_SPRITE_MEMORY

}

#pragma code-name (pop)