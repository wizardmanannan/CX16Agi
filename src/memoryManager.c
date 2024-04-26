#include "memoryManager.h"
//#define VERBOSE
#define boolean unsigned char

#ifdef  __CX16__
void bankedRamInit()
{
#define FILE_NAME_LENGTH 13
#define FLOOD_MESSAGE_LENGTH 30
#define byte unsigned char

	byte previousRamBank = RAM_BANK;
	
	int i, j = 0;
	FILE* fp;
	char fileName[FILE_NAME_LENGTH];
	char openFloodFileMessage[FLOOD_MESSAGE_LENGTH];
	char* openFloodFileMessageString = "Opening flood file %d of %d";

	unsigned char fileByte;
	int bankRamSizes[NO_CODE_BANKS] = {
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		(int)_BANKRAM10_SIZE__,
		0
	};

	printf("Zeroing Banks\n");
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

	for (i = 0; i < NO_FLOOD_BANKS; i++)
	{
		if ((fp = fopen(FLOODBANKFILENAME, "rb")) != NULL) {
			sprintf(openFloodFileMessage, openFloodFileMessageString, i + 1, NO_FLOOD_BANKS);
			printf("%s\n", openFloodFileMessage);
			RAM_BANK = FIRST_FLOOD_BANK + i;
			fread(&BANK_RAM[0], 1, 1, fp);
		}
		else {
			printf("Cannot open flood bank file");
		}
		fclose(fp);
	}

	RAM_BANK = previousRamBank;
}

//#pragma bss-name (push, "BANKRAM10")
//void Dummy2()
//{
//
//}
//#pragma bss-name (pop)

#endif //  __CX16__
#pragma code-name (push, "BANKRAM10")

void b10InitDynamicMemory()
{
//#ifdef _MSC_VER
//	banked = (byte*)malloc(512000);
//#define BANK_RAM banked
//#endif // _MSC_VER
//
//	b10InitSegments(TINY_SEG_ORDER, TINY_NO_BANKS, TINY_SIZE, TINY_NO_SEGMENTS, TINY_FIRST_BANK);
//	b10InitSegments(EXTRA_SMALL_SEG_ORDER, EXTRA_SMALL_NO_BANKS, EXTRA_SMALL_SIZE, EXTRA_SMALL_NO_SEGMENTS, EXTRA_SMALL_FIRST_BANK);
//	b10InitSegments(SMALL_SEG_ORDER, SMALL_NO_BANKS, SMALL_SIZE, SMALL_NO_SEGMENTS, SMALL_FIRST_BANK);
//	b10InitSegments(MEDIUM_SEG_ORDER, MEDIUM_NO_BANKS, MEDIUM_SIZE, MEDIUM_NO_SEGMENTS, MEDIUM_FIRST_BANK);
//	b10InitSegments(LARGE_SEG_ORDER, LARGE_NO_BANKS, LARGE_SIZE, LARGE_NO_SEGMENTS, LARGE_FIRST_BANK);
//
//	memset(_memoryAreas[0].start, 0, NO_SEGMENTS);
   return;
}

//int getMemoryAreaAllocationStartIndex(int memoryArea)
//{
//	return _memoryAreas[memoryArea].start - &[ALLOCATION_ARRAY_START_INDEX]
//}

#pragma code-name (pop);

#pragma code-name (push, "BANKRAM07")
byte* b10BankedAlloc(int size, byte* bank)
{


}

extern long opCounter;
boolean b10BankedDealloc(byte* ptr, byte bank)
{
	
}
#pragma code-name (pop)

void memoryMangerInit()
{
	byte previousRamBank = RAM_BANK;
#ifdef __CX16__
	bankedRamInit();
#endif // __CX16__

	RAM_BANK = 0x10;

	printf("enter");
	b10InitDynamicMemory();
	printf("exit");
	asm("stp");

	//b10InitZeroPage();

	RAM_BANK = previousRamBank;
}





