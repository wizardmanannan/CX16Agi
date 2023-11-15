#include "spriteMemoryManager.h"

#define SEGMENT_SMALL ((32 * 32) / 2)
#define SPRITE_ALLOC_TABLE_SIZE (((unsigned long) SPRITES_DATA_END - SPRITES_DATA_START) / SEGMENT_SMALL)

extern byte bESpriteAddressTableMiddle[SPRITE_ALLOC_TABLE_SIZE];
extern byte bESpriteAllocTable[SPRITE_ALLOC_TABLE_SIZE];

#pragma code-name (push, "BANKRAM0E")

//#define VERBOSE_MEMORY_INIT 
#define TEST_ALLOCATE_SPRITE_MEMORY

void bEResetSpriteMemoryManager()
{
	memset(bESpriteAllocTable, 0, SPRITE_ALLOC_TABLE_SIZE);


	 *((byte*)ZP_PTR_SEG_32) = 0;
	*((byte*)ZP_PTR_SEG_64) = SPRITE_ALLOC_TABLE_SIZE - 2; //64 allocator starts two from the end

	*((byte*)ZP_PTR_WALL_32) = 0;
	*((byte*)ZP_PTR_WALL_64) = SPRITE_ALLOC_TABLE_SIZE - 2; //64 allocator starts two from the end
}

#ifdef TEST_ALLOCATE_SPRITE_MEMORY

extern unsigned long bEAllocateSpriteMemory32();
extern unsigned long bEAllocateSpriteMemory64();

void bETestOverlap(byte testNo, boolean outOfLoop)
{
	if (*((byte*)ZP_PTR_SEG_32) > *((byte*)ZP_PTR_SEG_64))
	{
		printf("fail on %d, ZP_SEG 32 has overlapped 64\n", testNo);
		printf("out of loop %d\n", outOfLoop);
	}

	if (*((byte*)ZP_PTR_WALL_32) > *((byte*)ZP_PTR_WALL_64))
	{
		printf("fail on %d, ZP_SEG Wall has overlapped 64\n", testNo);
		printf("out of loop %d\n", outOfLoop);
	}
}

void bETestSpriteAllocateSpriteMemory32()
{
	byte i;
	unsigned long expected = SPRITES_DATA_START, actual;

	printf("1. test allocate sprite nemory 32\n");

	//1. Should allocate every block in sequence
	for (i = 0; i < SPRITE_ALLOC_TABLE_SIZE - 2; i++) //Last two positions is where the 64 allocator starts don't check those
	{
		actual = bEAllocateSpriteMemory32();

		if (actual != expected)
		{
			printf("fail 1 on i = % d.The result was % lx\n", i, actual);
		}

		if (*((byte*)ZP_PTR_SEG_32) != *((byte*)ZP_PTR_WALL_32))
		{
			printf("fail on 1, 32 wall %d doesn't track the segment %d\n", *((byte*)ZP_PTR_WALL_32), *((byte*)ZP_PTR_SEG_32)); //Note we can only put this test here, because there won't be any reset to zero
		}

		bETestOverlap(1, FALSE);

		expected += SEGMENT_SMALL;
	}

	actual = bEAllocateSpriteMemory32(); 

	bETestOverlap(1, TRUE);

	////2. Don't allocate when full
	if (actual)
	{
		printf("Fail 2. Expected 0 got %lx\n", actual);
	}

	bEResetSpriteMemoryManager();
}

void bETestSpriteAllocateSpriteMemory64()
{
	byte i;
	unsigned long expected = (SPRITE_ALLOC_TABLE_SIZE - 2) * SEGMENT_SMALL + SPRITES_DATA_START, actual;

	printf("2 test allocate sprite nemory 64\n");

	//1. Should allocate every block in sequence
	for (i = 0; i < (byte) SPRITE_ALLOC_TABLE_SIZE - 1; i += 2) //First position is for 32 bit allocator
	{
		actual = bEAllocateSpriteMemory64();

		if (actual != expected)
		{
			printf("Fail 2 on i = %d. The result was %lx. We expected %lx\n", i, actual, expected);
		}

		if (*((byte*)ZP_PTR_SEG_64) != *((byte*)ZP_PTR_WALL_64) && i != SPRITE_ALLOC_TABLE_SIZE - 3)
		{
			printf("fail on 2 (i : %d), 64 wall %d doesn't track the segment %d\n", i, *((byte*)ZP_PTR_WALL_64), *((byte*)ZP_PTR_SEG_64)); //Note we can only put this test here, because there won't be any reset to zero
		}
		else if (i == SPRITE_ALLOC_TABLE_SIZE - 3 && (*((byte*)ZP_PTR_WALL_64) != 1 || *((byte*)ZP_PTR_SEG_64) != SPRITE_ALLOC_TABLE_SIZE - 2))
		{
			printf("fail on 2 (i : %d), 64 wall %d doesn't reset to 1 and segment %d isn't at %d\n", i, *((byte*)ZP_PTR_WALL_64), *((byte*)ZP_PTR_SEG_64), SPRITE_ALLOC_TABLE_SIZE - 2);
		}

		bETestOverlap(2, FALSE);

		expected -= SEGMENT_SMALL * 2;
	}

	actual = bEAllocateSpriteMemory64();


	bETestOverlap(2, TRUE);

	//2. Don't allocate when full
	if (actual)
	{
		printf("Fail 2. Expected 0 got %lx\n", actual);
	}

	bEResetSpriteMemoryManager();
}

void bETestSpriteAllocate64NotGoOver32Wall()
{
	byte i;
	unsigned long expected = (SPRITE_ALLOC_TABLE_SIZE - 2) * SEGMENT_SMALL + SPRITES_DATA_START, actual;

	//64 does not go over 32's wall

	printf("3 64 does not go over 32's wall. We will go %lx times. Divided by 2 is %lx\n", SPRITE_ALLOC_TABLE_SIZE - 2, (SPRITE_ALLOC_TABLE_SIZE - 2) /2);

	bEAllocateSpriteMemory32();
	bEAllocateSpriteMemory32();

	for (i = 0; i < (byte)SPRITE_ALLOC_TABLE_SIZE - 4; i += 2) //1 less than last test to account for space taken by 32
	{
		actual = bEAllocateSpriteMemory64();

		printf("i %d  segment %d\n", i, *((byte*)ZP_PTR_SEG_64));

		if (actual != expected)
		{
			printf("Fail 3 on i = %d. The result was %lx. We expected %lx\n", i, actual, expected);
		}

		if (*((byte*)ZP_PTR_SEG_64) != *((byte*)ZP_PTR_WALL_64))
		{
			printf("fail on 3, 64 wall %d doesn't track the segment %d\n", *((byte*)ZP_PTR_WALL_64), *((byte*)ZP_PTR_SEG_64)); //Note we can only put this test here, because there won't be any reset to zero
		}

		bETestOverlap(3, FALSE);

		expected -= SEGMENT_SMALL * 2;
	}
	bETestOverlap(3, TRUE);

	if (bEAllocateSpriteMemory64())
	{
		printf("fail 3, 64 has gone over 32's wall\n");
	}

	bEResetSpriteMemoryManager();
}

void bETestSpriteAllocate32NotGoOver64Wall()
{
	byte i;
	unsigned long expected = SPRITES_DATA_START, actual;

	//32 does not go over 64's wall

	printf("4 32 does not go over 64's wall. We will go %lx times. Divided by 2 is %lx\n", SPRITE_ALLOC_TABLE_SIZE - 2, (SPRITE_ALLOC_TABLE_SIZE - 2) / 2);

	bEAllocateSpriteMemory64();

	for (i = 0; i < SPRITE_ALLOC_TABLE_SIZE - 4; i++) //1 less than last test to account for space taken by 64
	{
		actual = bEAllocateSpriteMemory32();

		printf("i %d  segment %d\n", i, *((byte*)ZP_PTR_SEG_32));

		if (actual != expected)
		{
			printf("Fail 4 on i = %d. The result was %lx. We expected %lx\n", i, actual, expected);
		}

		if (*((byte*)ZP_PTR_SEG_32) != *((byte*)ZP_PTR_WALL_32))
		{
			printf("fail on 4, 32 wall %d doesn't track the segment %d\n", *((byte*)ZP_PTR_WALL_32), *((byte*)ZP_PTR_SEG_32)); //Note we can only put this test here, because there won't be any reset to zero
		}

		bETestOverlap(4, FALSE);

		expected += SEGMENT_SMALL;
	}
	bETestOverlap(4, TRUE);

	if (bEAllocateSpriteMemory32())
	{
		printf("fail 3, 32 has gone over 64's wall\n");
	}

	bEResetSpriteMemoryManager();
}

void bETestSpriteAllocateSpriteMemory()
{
	bETestSpriteAllocateSpriteMemory32();
	bETestSpriteAllocateSpriteMemory64();
	bETestSpriteAllocate64NotGoOver32Wall();
	bETestSpriteAllocate32NotGoOver64Wall();
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
	//asm("stp");
#endif //  TEST_ALLOCATE_SPRITE_MEMORY

}

#pragma code-name (pop)