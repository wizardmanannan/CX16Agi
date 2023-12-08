#include "floatDivision.h"
//#define VERBOSE_LOAD_DIV
//#define TEST_DIVISION 

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
	const char* divFileName = "div%x.bin";
	const char* bankfileName = "divb.bin";
	const char* addressfileName = "diva.bin";

	for (bank = FIRST_DIVISION_BANK; bank <= LAST_DIVISION_BANK; bank++)
	{
		sprintf(&fileNameBuffer[0], divFileName, bank);

		printf("Loading division tables %d of %d \n", bank - FIRST_DIVISION_BANK + 1, LAST_DIVISION_BANK - FIRST_DIVISION_BANK + 1);
		if ((fp = fopen(&fileNameBuffer[0], "rb")) != NULL) {
			size_t bytesRead;
			i = 0;
			while ((bytesRead = fread(&GOLDEN_RAM_WORK_AREA[0], 1, LOCAL_WORK_AREA_SIZE, fp)) > 0) {
				memCpyBanked(DIVISION_AREA + LOCAL_WORK_AREA_SIZE * i++, &GOLDEN_RAM_WORK_AREA[0], bank, bytesRead);
			}

			fclose(fp);
		}
		else {
			printf("failed to division table file %s\n", &fileNameBuffer[0]);
		}
	}
	printf("Loading Division Metdata 1 of 2\n");
	b6LoadDivisionMetadata(bankfileName, DIV_BANK_METADATA_SIZE, &divBankMetadata[0]);
	printf("Loading Division Metdata 2 of 2\n");
	b6LoadDivisionMetadata(addressfileName, DIV_ADDRESS_METADATA_SIZE, &divAddressMetadata[0]);
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

#pragma code-name (push, "BANKRAM11")

fix32 DIV(int numerator, int denominator) {
	if (denominator == 0 || numerator == 0) {
		return (fix32)0;
	}
	else if (denominator == 1) {
		return fp_fromInt(numerator);
	}
	else if (numerator == denominator) {
		return fp_fromInt(1);
	}
	else {
		return floatDivision(numerator, denominator);
	}
}

#define ROUND_THRESHOLD_POS ((unsigned int) 0x7FBE)
#define ROUND_THRESHOLD_NEG ((unsigned int) 0x8041)
int round(fix32 aNumber, boolean isPos)
{
	if (isPos)
	{
#ifdef TEST_ROUND
		printf("%lu Pos True %d result %d\n", aNumber, getMantissa(aNumber), getMantissa(aNumber) < ROUND_THRESHOLD_POS ? floor_fix_32(aNumber) : ceil_fix_32(aNumber));
		printf("%u < %u = %d\n", getMantissa(aNumber), ROUND_THRESHOLD_POS, getMantissa(aNumber) < ROUND_THRESHOLD_POS);
		printf("The address of aNumber is %p", &aNumber);
#endif
		return getMantissa(aNumber) < ROUND_THRESHOLD_POS ? floor_fix_32(aNumber) : ceil_fix_32(aNumber);
	}
	else
	{
#ifdef TEST_ROUND
		printf("%lu Neg True %d result %p %d < %d\n", aNumber, getMantissa(aNumber), getMantissa(aNumber) <= ROUND_THRESHOLD_NEG ? floor_fix_32(aNumber) : ceil_fix_32(aNumber), getMantissa(aNumber), ROUND_THRESHOLD_POS);
		printf("%u < %u = %d\n", getMantissa(aNumber), ROUND_THRESHOLD_POS, getMantissa(aNumber) < ROUND_THRESHOLD_POS);
#endif
		return getMantissa(aNumber) <= ROUND_THRESHOLD_NEG ? floor_fix_32(aNumber) : ceil_fix_32(aNumber);
	}
}

#ifdef TEST_ROUND
void testRound()
{
	int result;
	result = round(0x0F7FBE, TRUE);

	if (result != 0x10)
	{
		printf("Fail Round 1 Pos (Equal). Expected %p got %p \n", 0x10, result);
	}

	result = round(0x0F7F7C, TRUE);

	if (result != 0x0F)
	{
		printf("Fail Round 2 Pos (Less). Expected %p got %p \n", 0xF, result);
	}

	result = round(0x0F8000, TRUE);

	if (result != 0x10)
	{
		printf("Fail Round 3 Pos (Greater). Expected %p got %p \n", 0x10, result);
	}

	result = round(0x0F8041, FALSE);

	if (result != 0xF)
	{
		printf("Fail Round 1 Neg (Equal). Expected %p got %p \n", 0xF, result);
	}

	result = round(0x0F8000, FALSE);

	if (result != 0xF)
	{
		printf("Fail Round 2 Neg (Less). Expected %p got %p \n", 0xF, result);
	}

	result = round(0x0F8083, FALSE);

	if (result != 0x10)
	{
		printf("Fail Round 3 Neg (Greater). Expected %p got %p \n", 0x10, result);
	}
}
#endif // TEST_ROUND


#ifdef TEST_DIVISION
void testDivision()
{
	long result;

	// Adjusted Tests

	result = DIV(0, 0xA7); // 0 and 167
	if (result != fp_fromInt(0))
	{
		printf("Fail Division 1. Expected %lx got %lx\n", fp_fromInt(0), result);
	}

	result = DIV(0xA7, 0); // 167 and 0
	if (result != fp_fromInt(0))
	{
		printf("Fail Division 2. Expected %lx got %lx\n", fp_fromInt(0), result);
	}

	result = DIV(0xA7, 1); // 167 and 1
	if (result != fp_fromInt(0xA7))
	{
		printf("Fail Division 3. Expected %lx got %lx\n", fp_fromInt(0xA7), result);
	}

	result = DIV(0xA7, 0xA7); // 167 and 167
	if (result != fp_fromInt(1))
	{
		printf("Fail Division 4. Expected %lx got %lx\n", fp_fromInt(1), result);
	}

	// New Tests

	result = DIV(1, 2); // 1 divided by 2
	if (result != fp_fromInt(0) + 0x8000) // should be 0.5 in fixed point format
	{
		printf("Fail Division 5. Expected %lx got %lx\n", fp_fromInt(0) + 0x8000, result);
	}

	result = DIV(1, 3); // 1 divided by 3
	if (result != fp_fromInt(0) + 0x5555) // should be 0.5 in fixed point format
	{
		printf("Fail Division 5. Expected %lx got %lx\n", fp_fromInt(0) + 0x553F, result);
	}

}
#endif // TEST_DIVISION
