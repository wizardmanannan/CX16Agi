#include "helpers.h"
//#define VERBOSE
//#define VERBOSE_CPY_CHECK
//#define VERBOSE_MEMSET_CHECK

boolean debugStop = FALSE;

byte _previousRomBank = 0;
byte _assm = 0; //Used as a value to load things in and out of the registers
long _assmLong = 0; //Used as a value to load things in and out of the registers

#pragma code-name (push, "BANKRAM05")
void b5RefreshBuffer(BufferStatus* bufferStatus)
{
	BufferStatus localBufferStatus;
	localBufferStatus = *bufferStatus;

	bufferStatus->bufferCounter++;
   printf("Called %p %d %p\n", bufferStatus->bankedData, bufferStatus->bank, bufferStatus->bufferCounter);
	

	memCpyBanked(GOLDEN_RAM_WORK_AREA, localBufferStatus.bankedData + localBufferStatus.bufferCounter * LOCAL_WORK_AREA_SIZE, localBufferStatus.bank, LOCAL_WORK_AREA_SIZE); //If it overflows the bank it isn't a big deal, the picture data is terminated by 0xFF so the rubbish data following will never be executed.
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
#pragma code-name (pop);

char* strcpyBanked(char* dest, const char* src, byte bank)
{
	char* result;
	byte previousRamBank = RAM_BANK;

	RAM_BANK = bank;

    strcpy(dest, src);

	RAM_BANK = previousRamBank;
	
	return result;
}

void* memCpyBanked(byte* dest, byte* src, byte bank, size_t len)
{
	byte previousRamBank = RAM_BANK;
	void* returnVal;

	RAM_BANK = bank;
	
#ifdef VERBOSE_CPY_CHECK
	printf("Attempting to copy to %p from %p on bank %d length %d and the first byte is %d\n", dest, src, bank, len, *src);
#endif 


	returnVal = memcpy(dest, src, len);

	RAM_BANK = previousRamBank;

	return returnVal;
}

void memCpyBankedBetween(byte* dest, byte bankDst, byte* src, byte bankSrc, size_t len)
{
#define BUFFER_SIZE 50
	int i;
	int copyAmount = 0;
	byte buffer[BUFFER_SIZE];

	for (i = 0; i < len; i += BUFFER_SIZE)
	{
		copyAmount = (i + BUFFER_SIZE <= len) ? BUFFER_SIZE : len - i;

		memCpyBanked(buffer, src + i, bankSrc, copyAmount);
		memCpyBanked(dest + i, buffer, bankDst, copyAmount);
	}
}

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

int sprintfBanked(const char* buffer, byte bank, char const* const format,  ...) {
	va_list list;
	int result;
	byte previousRamBank = RAM_BANK;

	RAM_BANK = bank;

	va_start(list, format);
	result = sprintf(buffer, list);

	va_end(list);

	RAM_BANK = previousRamBank;
}

size_t strLenBanked(char* string, int bank)
{
	byte previousRamBank = RAM_BANK;
	size_t len;
	RAM_BANK = bank;

	len = strlen(string);

	RAM_BANK = previousRamBank;

	return len;
}

void setResourceDirectory(AGIFilePosType* newLogicDirectory, AGIFilePosType* logicDirectoryLocation)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = DIRECTORY_BANK;

	*logicDirectoryLocation = *newLogicDirectory;

	RAM_BANK = previousRamBank;
}

void getLogicDirectory(AGIFilePosType* returnedLogicDirectory, AGIFilePosType* logicDirectoryLocation)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = DIRECTORY_BANK;

	*returnedLogicDirectory = *logicDirectoryLocation;

#ifdef VERBOSE
	printf("Retrieving file no: %d, location %p\n", logicDirectoryLocation->fileNum, logicDirectoryLocation->filePos);
#endif // VERBOSE

	RAM_BANK = previousRamBank;
}






