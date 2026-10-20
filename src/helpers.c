#include "helpers.h"
//#define VERBOSE
//#define VERBOSE_MEMSET_CHECK

boolean debugStop = FALSE;

byte _previousRomBank = 0;
byte _assmByte = 0; //Used as a value to load things in and out of the registers
byte _assmByte2 = 0; //Used as a value to load things in and out of the registers
byte _assmByte3 = 0; //Used as a value to load things in and out of the registers
unsigned int _assmUInt = 0;
unsigned long _assmULong;
long _assmLong = 0; //Used as a value to load things in and out of the registers
boolean enableHelpersDebugging = FALSE; //This is so you can debug helpers at a certain area and not be bogged down when they are called elsewhere.

#pragma code-name (push, "BANKRAM05")
void b5WaitOnKey()
{
	byte ch;

	do {
		GET_IN(ch);
	} while (!ch);
}


void b5RefreshBufferNonGolden(BufferStatus* bufferStatus, byte* buffer, int bufferSize)
{
	BufferStatus localBufferStatus;
	localBufferStatus = *bufferStatus;

	bufferStatus->bufferCounter++;

	memCpyBanked(buffer, localBufferStatus.bankedData + localBufferStatus.bufferCounter * bufferSize, localBufferStatus.bank, bufferSize); //If it overflows the bank it isn't a big deal, the picture data is terminated by 0xFF so the rubbish data following will never be executed.
}

byte* b5ReallocateBiggerMemoryBlock(byte** memoryBlock, unsigned int newBufferSize, unsigned int* oldBufferSize, byte* bank)
{
	byte oldBank, *oldMemoryBlock;

	oldMemoryBlock = *memoryBlock;
	oldBank = *bank;
	*memoryBlock = b10BankedAlloc(newBufferSize, bank);
	memCpyBankedBetween(*memoryBlock, *bank, oldMemoryBlock, oldBank, *oldBufferSize);
	b10BankedDealloc(oldMemoryBlock, oldBank);
	*oldBufferSize = newBufferSize;
}

void b5FlushBufferNonGolden(BufferStatus* bufferStatus, byte* buffer, int bufferSize, int amountToCopy)
{
	BufferStatus localBufferStatus;
	localBufferStatus = *bufferStatus;

	bufferStatus->bufferCounter++;


	//printf("we flush to %p (%p + %p * %p) from %p bank %p, size %d\n", localBufferStatus.bankedData + localBufferStatus.bufferCounter * bufferSize, localBufferStatus.bankedData, localBufferStatus.bufferCounter, bufferSize,buffer, localBufferStatus.bank, amountToCopy);
	memCpyBanked(localBufferStatus.bankedData + localBufferStatus.bufferCounter * bufferSize, buffer, localBufferStatus.bank, amountToCopy);
}

void b5FlushBuffer(BufferStatus* bufferStatus)
{
	BufferStatus localBufferStatus;
	localBufferStatus = *bufferStatus;

	bufferStatus->bufferCounter++;


	//printf("we flush to %p (%p + %p * %p) from %p bank %p, size %d\n", localBufferStatus.bankedData + localBufferStatus.bufferCounter * LOCAL_WORK_AREA_SIZE, localBufferStatus.bankedData, localBufferStatus.bufferCounter, LOCAL_WORK_AREA_SIZE,GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE);
	memCpyBanked(localBufferStatus.bankedData + localBufferStatus.bufferCounter * LOCAL_WORK_AREA_SIZE, GOLDEN_RAM_WORK_AREA, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE);
}

void b5RefreshBuffer(BufferStatus* bufferStatus)
{
	b5RefreshBufferNonGolden(bufferStatus, GOLDEN_RAM_WORK_AREA, LOCAL_WORK_AREA_SIZE);
}

byte convertAsciiByteToPetsciiByte(byte toConvert)
{
	if (toConvert == ASCIIDASH)
	{
		toConvert = PETSCIIDash;
#ifdef VERBOSE
		printf("Dash Converting %c to %c \n", toConvert, toConvert + DIFF_ASCII_PETSCII_CAPS);
#endif
	}
	else if (toConvert >= ASCIIA && toConvert <= ASCIIZ)
	{
#ifdef VERBOSE
		printf("Upper Converting %c to %c \n", toConvert, toConvert + DIFF_ASCII_PETSCII_CAPS);
#endif
		toConvert = toConvert + DIFF_ASCII_PETSCII_CAPS;
	}
	else if (toConvert >= ASCIIa && toConvert <= ASCIIz)
	{
		toConvert = toConvert + DIFF_ASCII_PETSCII_LOWER;
#ifdef VERBOSE
		printf("Lower Converting %c to %c \n", toConvert, toConvert + DIFF_ASCII_PETSCII_CAPS);
#endif
	}
	return toConvert;
}

//These methods are called from ASM where multiplication and division is a lot harder
int b5Multiply(int a, int b)
{
	return a * b;
}

int b5Divide(int a, int b)
{
	return a / b;
}

void b5WaitOnSpecificKeys(byte* keys, byte length)
{
	byte ch, i;
	boolean keyPressed = FALSE;

	do {
		GET_IN(ch);

		if (ch)
		{
			for (i = 0; i < length && !keyPressed; i++)
			{
				keyPressed = ch == keys[i];
			}
		}
	} while (!keyPressed);
}

#pragma code-name (pop);

void copyStringFromBanked(char* src, char* dest, int start, int chunk, byte sourceBank, boolean convertFromAsciiByteToPetscii)
{
	int i;
	byte previousRamBank = RAM_BANK;
	byte convertResult;

	RAM_BANK = sourceBank;

	for (i = start; i - start < chunk && (i == 0 || src[i - 1] != '\0'); i++)
	{
		dest[i - start] = src[i];
		if (convertFromAsciiByteToPetscii)
		{
			RAM_BANK = HELPERS_BANK;
			convertResult = convertAsciiByteToPetsciiByte(dest[i - start]);
			RAM_BANK = sourceBank;
			dest[i - start] = convertResult;
		}
	}

	RAM_BANK = previousRamBank;
}

int sprintfBanked(const char* buffer, byte bank, char const* const format, ...) {
	va_list list;
	int result;
	byte previousRamBank = RAM_BANK;

	RAM_BANK = bank;

	va_start(list, format);
	result = sprintf(buffer, list);

	va_end(list);

	RAM_BANK = previousRamBank;
}

#pragma code-name (push, "BANKRAM10")
void b10SetResourceDirectory(AGIFilePosType* newLogicDirectory, AGIFilePosType* logicDirectoryLocation)
{
	*logicDirectoryLocation = *newLogicDirectory;

}

void b10GetLogicDirectory(AGIFilePosType* returnedLogicDirectory, AGIFilePosType* logicDirectoryLocation)
{
	*returnedLogicDirectory = *logicDirectoryLocation;
}
#pragma code-name (pop)




