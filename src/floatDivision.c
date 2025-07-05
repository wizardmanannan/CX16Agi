#include "floatDivision.h"
//#define VERBOSE_LOAD_DIV
//#define TEST_DIVISION 

#pragma rodata (push, "BANKRAM06");
const char B6_FAILED_OPEN_DIVISION[] = "failed to division table file %s\n";
const char B6_LOAD_DIVISION[] = "Loading Division Metdata %d of 2\n";
const char B6_DIV_FILE_NAME[] = "div%x.bin";
const char B6_BANK_FILE_NAME[] = "divb.bin";
const char B6_ADDRESS_FILE_NAME[] = "diva.bin";
const char B6_LOAD_DIV_TABLES[] = "Loading division tables %d of %d \n";
const char B6_FAILED_TO_OPEN[] = "Failed to open %s\n";
#pragma rodata (pop)

#ifdef TEST_DIVISION
void testDivision()
{
	long result;

	// Adjusted Tests

	result = b12Div(0, 0xA7); // 0 and 167
	if (result != b12FpFromInt(0))
	{
		printf("Fail Division 1. Expected %lx got %lx\n", b12FpFromInt(0), result);
	}

	result = b12Div(0xA7, 0); // 167 and 0
	if (result != b12FpFromInt(0))
	{
		printf("Fail Division 2. Expected %lx got %lx\n", b12FpFromInt(0), result);
	}

	result = b12Div(0xA7, 1); // 167 and 1
	if (result != b12FpFromInt(0xA7))
	{
		printf("Fail Division 3. Expected %lx got %lx\n", b12FpFromInt(0xA7), result);
	}

	result = b12Div(0xA7, 0xA7); // 167 and 167
	if (result != b12FpFromInt(1))
	{
		printf("Fail Division 4. Expected %lx got %lx\n", b12FpFromInt(1), result);
	}

	// New Tests

	result = b12Div(1, 2); // 1 divided by 2
	if (result != b12FpFromInt(0) + 0x8000) // should be 0.5 in fixed point format
	{
		printf("Fail Division 5. Expected %lx got %lx\n", b12FpFromInt(0) + 0x8000, result);
	}

	result = b12Div(1, 3); // 1 divided by 3
	if (result != b12FpFromInt(0) + 0x5555) // should be 0.5 in fixed point format
	{
		printf("Fail Division 5. Expected %lx got %lx\n", b12FpFromInt(0) + 0x553F, result);
	}

	asm("stp");
}
#endif // TEST_DIVISION

#pragma code-name (push, "BANKRAM06")
void b6LoadDivisionMetadata(const char* fileName, int metadataSize, byte* metadataLocation)
{
	FILE* fp;
	char fileNameBuffer[30];
	size_t bytesRead;

#ifdef VERBOSE_LOAD_DIV
	printf("The filename is %s and the metadata size is %d\n", fileName, metadataSize);
#endif // VERBOSE


	if ((fp = fopen(fileName, "rb")) != NULL) {
		bytesRead = fread(&GOLDEN_RAM_WORK_AREA[0], 1, metadataSize, fp);

#ifdef VERBOSE_LOAD_DIV
		printf("Read %d bytes. The first byte is %p\n", bytesRead);
#endif // VERBOSE

		memCpyBanked(metadataLocation, &GOLDEN_RAM_WORK_AREA[0], DIV_METADATA_BANK, metadataSize);
#ifdef VERBOSE_LOAD_DIV
		printf("Copy %d bytes to location %p \n", bytesRead, metadataLocation);
#endif // VERBOSE

		fclose(fp);
	}
	else {
		printf("Failed to open %s\n", fileName);
	}
}


#define SWIDTH   640  /* Screen resolution */
#define SHEIGHT  480
#define PWIDTH   160  /* Picture resolution */
#define PHEIGHT  168
#define VWIDTH   640  /* Viewport size */
#define VHEIGHT  336

void b6LoadDivisionTables()
{
	FILE* fp;
	int bank, i;
	char fileNameBuffer[30];

	for (bank = FIRST_DIVISION_BANK; bank <= LAST_DIVISION_BANK; bank++)
	{
		sprintf(&fileNameBuffer[0], B6_DIV_FILE_NAME, bank);

		printf(B6_LOAD_DIV_TABLES, bank - FIRST_DIVISION_BANK + 1, LAST_DIVISION_BANK - FIRST_DIVISION_BANK + 1);
		if ((fp = fopen(&fileNameBuffer[0], "rb")) != NULL) {
			size_t bytesRead;
			i = 0;
			while ((bytesRead = fread(&GOLDEN_RAM_WORK_AREA[0], 1, LOCAL_WORK_AREA_SIZE, fp)) > 0) {
				memCpyBanked(DIVISION_AREA + LOCAL_WORK_AREA_SIZE * i++, &GOLDEN_RAM_WORK_AREA[0], bank, bytesRead);
			}

			fclose(fp);
		}
		else {
			printf(B6_FAILED_OPEN_DIVISION, &fileNameBuffer[0]);
		}
	}
	printf(B6_LOAD_DIVISION, 1);
	b6LoadDivisionMetadata(B6_BANK_FILE_NAME, DIV_BANK_METADATA_SIZE, &divBankMetadata[0]);
	printf(B6_LOAD_DIVISION, 2);
	b6LoadDivisionMetadata(B6_ADDRESS_FILE_NAME, DIV_ADDRESS_METADATA_SIZE, &divAddressMetadata[0]);
}


void b6InitFloatDivision()
{
	byte** zpDivisionArea = (byte**)ZP_DIV_AREA;
	byte** zpDivBankMetadata = (byte**)ZP_DIV_BANK;
	byte** zpDivAddressMetadata = (byte**)ZP_DIV_ADDR;

	*zpDivisionArea = &DIVISION_AREA[0];
	*zpDivBankMetadata = divBankMetadata;
	*zpDivAddressMetadata = divAddressMetadata;

	b6LoadDivisionTables();

#ifdef TEST_DIVISION
	testDivision();
#endif

#ifdef TEST_ROUND
	testRound();
#endif
}
#pragma code-name (pop)

#pragma code-name (push, "BANKRAM12")

extern fix32 floatDivision(byte numerator, byte denominator);

fix32 b12Div(int numerator, int denominator) {
	if (denominator == 0 || numerator == 0) {
		return (fix32)0;
	}
	else if (denominator == 1) {
		return b12FpFromInt(numerator);
	}
	else if (numerator == denominator) {
		return b12FpFromInt(1);
	}
	else {
		return floatDivision(numerator, denominator);
	}
}
#pragma code-name (pop)
