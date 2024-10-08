#include "memoryManager.h"
MemoryArea* _memoryAreas;
int _noSegments;
//#define VERBOSE

#ifdef _MSC_VER //Used for testing under windows
byte* banked;
byte ramBank;
#define BANK_RAM banked
#define RAM_BANK ramBank
#endif 

#define MEM_MANAGER_STRINGS 6;
#pragma rodata-name (push, "BANKRAM06")
const char B6_CANNOT_OPN_FLOOD[] = "Cannot open flood bank file\n";
const char B6_OPEN_FLOOD_FILE_MESSAGE_STRING[] = "Opening flood file %d of %d\n";
#pragma rodata-name (pop)
#pragma rodata-name (push, "BANKRAM10")
const char B10_OUT_DYNAMIC[] = "Out of dynamic memory. Amount %d\n";
#pragma rodata-name (pop)
#ifdef  __CX16__
void bankedRamInit()
{
#define FILE_NAME_LENGTH 13
#define FLOOD_MESSAGE_LENGTH 30
	
	byte previousRamBank = RAM_BANK;
	
	int i, j = 0;
	FILE* fp;
	char fileName[FILE_NAME_LENGTH];
	char openFloodFileMessage[FLOOD_MESSAGE_LENGTH];

	unsigned char fileByte;
	int bankRamSizes[NO_CODE_BANKS] = {
		(int)_BANKRAM01_SIZE__,
		(int)_BANKRAM02_SIZE__,
		(int)_BANKRAM03_SIZE__,
		(int)_BANKRAM04_SIZE__,
		(int)_BANKRAM05_SIZE__,
		(int)_BANKRAM06_SIZE__,
		(int)_BANKRAM07_SIZE__,
		(int)_BANKRAM08_SIZE__,
		(int)_BANKRAM09_SIZE__,
		(int)_BANKRAM0A_SIZE__,
		(int)_BANKRAM0B_SIZE__,
		(int)_BANKRAM0C_SIZE__,
		(int)_BANKRAM0D_SIZE__,
		(int)_BANKRAM0E_SIZE__,
		(int)_BANKRAM0F_SIZE__,
		(int)_BANKRAM10_SIZE__,
		(int)_BANKRAM11_SIZE__,
		(int)_BANKRAM12_SIZE__
	};

	for (i = 1; i < get_numbanks(); i++) //Don't do bank zero as it as a system bank
	{
		RAM_BANK = i;
		memset(&BANK_RAM[0], 0, BANK_SIZE);
	}
	RAM_BANK = previousRamBank;

	for (i = 0; i < NO_CODE_BANKS; i++)
	{
		printf("Loading MEKA Resource %d of %d\n", i + 1, NO_CODE_BANKS);
#ifdef VERBOSE
		printf("The bank ram size %p is %d\n", i + 1, bankRamSizes[i]);
#endif // VERBOSE

		if (i < 15)
		{
			sprintf(fileName, "agi.cx16.0%x", i + 1);
		}
		else
		{
			sprintf(fileName, "agi.cx16.%x", i + 1);
		}


#ifdef VERBOSE
		printf("Loading file %s\n", fileName);
#endif // VERBOSE

		RAM_BANK = i + 1;
		if ((fp = fopen(fileName, "rb")) != NULL) {

			if (RAM_BANK < 0xa)
			{
				fgetc(fp);
				fgetc(fp);
			}

			fread(&BANK_RAM[0], 1, bankRamSizes[i], fp);

			fclose(fp);
		}
		else {
#ifdef VERBOSE
			printf("Cannot find file");
#endif // VERBOSE
		}
	}

	RAM_BANK = previousRamBank;
}
#endif //  __CX16__
#pragma code-name (push, "BANKRAM10")

void b10InitSegments(byte segOrder, byte noBanks, int segmentSize, byte noSegments, byte firstBank)
{
	if (segOrder > 0)
	{
		_memoryAreas[segOrder].start = _memoryAreas[segOrder - 1].start + _memoryAreas[segOrder - 1].noSegments;
		//printf("The address is %p \n", _segments[segOrder].start);
	}
	else {
		_memoryAreas[segOrder].start = &BANK_RAM[ALLOCATION_ARRAY_START];
	}

	_memoryAreas[segOrder].firstBank = firstBank;

	_memoryAreas[segOrder].noBanks = noBanks;

	_memoryAreas[segOrder].segmentSize = segmentSize;

	_memoryAreas[segOrder].noSegments = noSegments;

	//printf("Segments: banks %p, noBanks %d, segmentSize %d, allocationArray %p, noSegments %d\n", _segments[segOrder].banks, _segments[segOrder].noBanks, _segments[segOrder].segmentSize, _segments[segOrder].allocationArray, _segments[segOrder].noSegments);
}

void b10InitZeroPage()
{
	memset((byte*)FIRST_ZERO_PAGE_ENTRY, 0, NO_ZERO_PAGE_ENTRIES);
}

void b10InitDynamicMemory()
{
#ifdef _MSC_VER
	banked = (byte*)malloc(512000);
#define BANK_RAM banked
#endif // _MSC_VER

	_noSegments = TINY_NO_SEGMENTS + EXTRA_SMALL_NO_SEGMENTS + SMALL_NO_SEGMENTS + MEDIUM_NO_SEGMENTS + LARGE_NO_SEGMENTS;

	_memoryAreas = (MemoryArea*)&BANK_RAM[MEMORY_AREA_START];

	b10InitSegments(TINY_SEG_ORDER, TINY_NO_BANKS, TINY_SIZE, TINY_NO_SEGMENTS, TINY_FIRST_BANK);
	b10InitSegments(EXTRA_SMALL_SEG_ORDER, EXTRA_SMALL_NO_BANKS, EXTRA_SMALL_SIZE, EXTRA_SMALL_NO_SEGMENTS, EXTRA_SMALL_FIRST_BANK);
	b10InitSegments(SMALL_SEG_ORDER, SMALL_NO_BANKS, SMALL_SIZE, SMALL_NO_SEGMENTS, SMALL_FIRST_BANK);
	b10InitSegments(MEDIUM_SEG_ORDER, MEDIUM_NO_BANKS, MEDIUM_SIZE, MEDIUM_NO_SEGMENTS, MEDIUM_FIRST_BANK);
	b10InitSegments(LARGE_SEG_ORDER, LARGE_NO_BANKS, LARGE_SIZE, LARGE_NO_SEGMENTS, LARGE_FIRST_BANK);

	memset(_memoryAreas[0].start, 0, _noSegments);
}

//int getMemoryAreaAllocationStartIndex(int memoryArea)
//{
//	return _memoryAreas[memoryArea].start - &[ALLOCATION_ARRAY_START_INDEX]
//}

byte* b10BankedAlloc(int size, byte* bank)
{

	byte i, j;
	byte* result = 0;
	byte* allocationByte = 0;

	for (i = 0; i < NO_SIZES && !result; i++)
	{
		if (size <= _memoryAreas[i].segmentSize)
		{
			for (j = 0; j < _memoryAreas[i].noSegments && !result; j++)
			{
				allocationByte = _memoryAreas[i].start + j;

#ifdef VERBOSE
				printf("The allocation byte %d (%p + %d) is %d\n", i, _memoryAreas[i].start, j, *allocationByte);
#endif

				if (!*(allocationByte))
				{
					*allocationByte = TRUE;

#ifdef VERBOSE

					printf("Bank Calc ((%d * %d) / %d + %d)\n", j, _memoryAreas[i].segmentSize, MEMORY_MANAGER_BANK_SIZE, _memoryAreas[i].firstBank);
#endif
					*bank = (byte)(((unsigned long)j * _memoryAreas[i].segmentSize) / MEMORY_MANAGER_BANK_SIZE + _memoryAreas[i].firstBank);

					//printf("Size of unsigned long long %d, size of unsigned long %d", sizeof(unsigned long long), sizeof(unsigned long));

#ifdef VERBOSE
					printf("Result calc: (%d * %d) mod %d + %p;\n", _memoryAreas[i].segmentSize, j, MEMORY_MANAGER_BANK_SIZE, &BANK_RAM[0]);
#endif
					result = ((unsigned long)_memoryAreas[i].segmentSize * j) % MEMORY_MANAGER_BANK_SIZE + &BANK_RAM[0];
#ifdef VERBOSE
					printf("The result is %p, on bank %d size: %d, segment %d\n", result, *bank, i, j);
#endif // VERBOSE
				}
				}
			}
		}

	if (!result)
	{
		printf(B10_OUT_DYNAMIC,size);
		exit(0);
	}

	return result;
}

extern long opCounter;
boolean b10BankedDealloc(byte* ptr, byte bank)
{
	int i;
	byte size = 0;
	byte segment = 0;
	byte result = FALSE;
	MemoryArea memoryArea;
	byte* allocationAddress;

	for (i = NO_SIZES - 1; i >= 0 && !size; i--)
	{
		if (bank >= _memoryAreas[i].firstBank) {
			size = i;
		}
	}

	if (bank == 0 || ptr == 0)
	{
#ifdef  VERBOSE
		printf("Zero deallocation detected ptr %p bank %p on %lu \n", ptr, bank,  opCounter);
#endif
		return FALSE;
	}

	memoryArea = _memoryAreas[size];

	allocationAddress = memoryArea.start + ((ptr - &BANK_RAM[0]) / memoryArea.segmentSize) + ((bank - memoryArea.firstBank) * (memoryArea.noSegments / memoryArea.noBanks));

#ifdef VERBOSE
    printf("allocationAddressCalc: %p + ((%p - %p) / %d) + ((%p - %p) * (%d / %d) ) = %p\n", memoryArea.start, ptr, &BANK_RAM[0], memoryArea.segmentSize, bank, memoryArea.firstBank, memoryArea.noSegments, memoryArea.noBanks, allocationAddress);
	
	printf("allocationAddressCalc: %p + ((%p - %p) / %d) + ((%p - %p) * %d)\n", memoryArea.start, ptr, &BANK_RAM[0], memoryArea.segmentSize, bank, memoryArea.firstBank, memoryArea.noSegments);
	printf("the allocation address is %p and the pointer is %p and the bank is %p\n", allocationAddress, ptr, bank);
#endif

	if (*(allocationAddress))
	{
		*(allocationAddress) = FALSE;

		result = TRUE;
	}

#ifdef VERBOSE
	printf("\n Deallocation segment (%p - %p)  %p \n", allocationAddress, &BANK_RAM[0], allocationAddress - &BANK_RAM[0]);
#endif // VERBOSE

	return result;
}

#pragma code-name (pop);
void memoryMangerInit()
{
	byte previousRamBank = RAM_BANK;
#ifdef __CX16__
	bankedRamInit();
#endif // __CX16__

	RAM_BANK = MEMORY_MANAGEMENT_BANK;

	b10InitDynamicMemory();
	b10InitZeroPage();

	RAM_BANK = previousRamBank;
}





