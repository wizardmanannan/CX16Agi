#include "helpers.h"
//#define VERBOSE
//#define VERBOSE_CPY_CHECK
//#define VERBOSE_MEMSET_CHECK

boolean debugStop = FALSE;

#pragma code-name (push, "BANKRAM05")
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

void trampoline_0(fnTrampoline_0 func, byte bank)
{
	byte previousRamBank = RAM_BANK;

	RAM_BANK = bank;

	func();

	RAM_BANK = previousRamBank;
}

void trampoline_1Int(fnTrampoline_1Int func, int data, byte bank)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = bank;
	func(data);
	RAM_BANK = previousRamBank;
}

byte trampoline_1ByteRByte(fnTrampoline_1ByteRByte func, byte data, byte bank)
{
	byte result;
	byte previousRamBank = RAM_BANK;
	RAM_BANK = bank;
	result = func(data);
	RAM_BANK = previousRamBank;

	return result;
}

void trampoline_3Int(fnTrampoline_3Int func, int data1, int data2, int data3, int bank)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = bank;
	func(data1, data2, data3);
	RAM_BANK = previousRamBank;
}

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

	RAM_BANK = bank;
	
#ifdef VERBOSE_CPY_CHECK
	printf("Attempting to copy to %p from %p on bank %d length %d and the first byte is %d\n", dest, src, bank, len, *src);
#endif


	memcpy(dest, src, len);

	RAM_BANK = previousRamBank;
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






