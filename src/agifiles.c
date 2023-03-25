/***************************************************************************
** AGIFILES.C
**
** Routines to handle AGI file system. These functions should enable you
** to load individual LOGIC, VIEW, PIC, and SOUND files into memory. The
** data is stored in a structure of type AGIFile. There is no code that
** is specific to the above types of data file though.
**
** (c) 1997 Lance Ewing
***************************************************************************/
//#define VERBOSE_DISPLAY_FILEOFFSETS
//#define VERBOSE_DISPLAY_MESSAGES
//#define VERBOSE_DISPLAY_OFFSETS
//#define VERBOSE
//#define VERBOSE_VIEW_LOAD_DEBUG
//define VERBOSE_LOGIC_LOAD_DEBUG
#include <stdint.h>
#include <cbm.h>


#include "general.h"
#include "agifiles.h"
#include "decomp.h"
#include "memoryManager.h"


byte avisDurgan[11] = { 0x41, 0x76, 0x69, 0x73, 0x20, 0x44, 0x75, 0x72, 0x67, 0x61, 0x6E };//https://www.liquisearch.com/what_is_avis_durgan
#define FILE_OPEN_ADDRESS 2
#define NO_BYTES_PER_MESSAGE 2
#define FILE_NAME_SIZE 10

AGIFilePosType* logdir = (AGIFilePosType*)&BANK_RAM[LOGDIR_START];
AGIFilePosType* picdir = (AGIFilePosType*)&BANK_RAM[PICDIR_START];
AGIFilePosType* viewdir = (AGIFilePosType*)&BANK_RAM[VIEWDIR_START];
AGIFilePosType* snddir = (AGIFilePosType*)&BANK_RAM[SOUNDDIR_START];

int numLogics, numPictures, numViews, numSounds;
boolean version3 = FALSE;


/***************************************************************************
** initFiles
**
** Purpose: To load the AGI resource directories and to determine whether
** the AGI files are for an AGIv3 game or for an AGIv2 game. It will also
** initialize the game signature in the case of a version 3 game.
***************************************************************************/

#define getMessageSectionSize AGIData->totalSize - AGIData->codeSize - 5 - AGIData->noMessages * 2

#ifdef VERBOSE_DISPLAY_MESSAGES
void printMessages(AGIFile* AGIData)
{
	int i;
	byte previousRamBank = RAM_BANK;
	RAM_BANK = AGIData->messageBank;

	for (i = 0; i < getMessageSectionSize; i++)
	{
		printf("%c", AGIData->messageData[i]);

		if (!AGIData->messageData[i])
		{
			printf("\n");
		}
	}

	RAM_BANK = previousRamBank;
}
#endif 

#ifdef VERBOSE_DISPLAY_OFFSETS
void printMessagesFromOffsets(AGIFile* AGIData)
{
	int i;
	byte previousRamBank = RAM_BANK;

	RAM_BANK = AGIData->messageBank;
	for (i = 0; i < AGIData->noMessages; i++)
	{
		if (AGIData->messagePointers[i] > 0)
		{
			printf("%d Data Length: %d Address %p Bank %d, Message %s\n", i + 1, strlen((char*)AGIData->messagePointers[i]), AGIData->messagePointers[i], AGIData->messageBank, AGIData->messagePointers[i]);
		}
		else
		{
			printf("%d is skipped\n", i + 1);
		}
	}
	printf("\nYou have %d message", AGIData->noMessages);

	RAM_BANK = previousRamBank;
}
#endif

#pragma code-name (push, "BANKRAM06")
byte cbm_openForSeeking(char* fileName)
{
	const char* OPEN_FLAGS = ",S,R";
	byte lfn = SEQUENTIAL_LFN;
	byte dev = 8;

	char fileNameAndFlags[FILE_NAME_SIZE + 4];
	byte sec_addr = FILE_OPEN_ADDRESS;

	sprintf(&fileNameAndFlags[0], "%s%s", &fileName[0], OPEN_FLAGS);

#ifdef VERBOSE
	printf("Attempting to load file %s", fileNameAndFlags);
#endif // VERBOSE


	cbm_open(lfn, dev, sec_addr, fileNameAndFlags);

	return lfn;
}

int8_t cx16_fseek(uint8_t channel, uint32_t offset) {
	int8_t result = 0, status = 0, chkin = 0;
#define SETNAM 0xFFBD
	static struct cmd {
		char p;
		uint8_t lfn;
		uint32_t offset;
	} cmd;

#ifdef VERBOSE

	printf("Attempting to seek at channel %d offset %lu\n", channel, offset);

#endif

	// open command channel to DOS and send P command.
	// P u8 u32 (no spaces) u8 is LFN to seek(), and u32 = offset.
	cmd.p = 'p';
	cmd.lfn = channel;
	cmd.offset = offset;
	// can't call cbm_open because the P command is binary and may
	// contain zeros, which C would interpret as null terminator.
	//
	// Roll-your-own inline asm call to SETNAM:
	__asm__("lda #6");
	__asm__("ldx #<%v", cmd);
	__asm__("ldy #>%v", cmd);
	__asm__("jsr %w", SETNAM);
	cbm_k_setlfs(15, 8, 15);
	cbm_k_open(); // this sends the CMD bytes..

	cbm_k_close(15); // close the command channel

	return 0;
	// TODO: ERROR HANDLING!!!!!
}

/***************************************************************************
** loadAGIDir
**
** Purpose: To read in an individual AGI directory file. This function
** should only be called by loadAGIDirs() below.
***************************************************************************/
void b6LoadAGIDir(int dirNum, char* fName, int* count)
{
	FILE* fp;
	unsigned char byte1, byte2, byte3;
	unsigned char address[4] = { 0,0,0,0 };
	AGIFilePosType tempPos;
	int value;

	if ((fp = fopen(fName, "rb")) == NULL) {
		printf("no file : %s.\n", fName);
		exit(0);
	}

	while (!feof(fp)) {
		byte1 = fgetc(fp);
		byte2 = fgetc(fp);
		byte3 = fgetc(fp);

		tempPos.fileNum = ((byte1 & 0xF0) >> 4);

		address[3] = 0;
		address[2] = byte1 & 0x0F;
		address[1] = byte2;
		address[0] = byte3;


		memcpy(&tempPos.filePos, &address[0], 4);

		value = tempPos.filePos;

		switch (dirNum) {
		case 0:
		{
			setResourceDirectory(&tempPos, &logdir[*count]);
#ifdef VERBOSE_DISPLAY_FILEOFFSETS
			printf("\n%d Logic File Name %s, Offset %lu\n", *count, tempPos.fileName, logdir[*count].filePos);
#endif // VERBOSE_DISPLAY_FILEOFFSETS


			break;
		}
		case 1:
		{
			setResourceDirectory(&tempPos, &picdir[*count]);
#ifdef VERBOSE_DISPLAY_FILEOFFSETS
			printf("\n%d Pic File Name %s, Offset %lu\n", *count, picdir[*count].fileName, picdir[*count].filePos);
#endif // VERBOSE_DISPLAY_FILEOFFSETS
		}
		break;
		case 2:
		{
			setResourceDirectory(&tempPos, &viewdir[*count]);
#ifdef VERBOSE_DISPLAY_FILEOFFSETS
			printf("\n%d View File Name %s, Offset %lu\n", *count, viewdir[*count].fileName, viewdir[*count].filePos);
#endif // VERBOSE_DISPLAY_FILEOFFSETS
			break;
		}
		case 3:
			setResourceDirectory(&tempPos, &snddir[*count]);

#ifdef VERBOSE_DISPLAY_FILEOFFSETS
			printf("\n%d sound File Name %s, Offset %lu\n", *count, snddir[*count].fileName, snddir[*count].filePos);
#endif // VERBOSE_DISPLAY_FILEOFFSETS
			break;
		}

		(*count)++;
	}

	fclose(fp);
}

/***************************************************************************
** loadAGIv3Dir
***************************************************************************/
void b6LoadAGIv3Dir()
{
	//FILE *dirFile;
	//unsigned char dirName[15], *marker, *dirData, *endPos, *dataPos;
	//int resType=0, resNum=0, dirLength;
	//AGIFilePosType tempPos;

	//sprintf(dirName, "%sDIR", gameSig);
	//if ((dirFile = fopen(dirName, "rb")) == NULL) {
	//   printf("File not found : %s\n", dirName);
	//   exit(1);
	//}

	////fseek(dirFile, 0, SEEK_END);
	//dirLength = //ftell(dirFile);
	////fseek(dirFile, 0, SEEK_SET);
	//dirData = (char *)malloc(sizeof(char)*dirLength);
	//fread(dirData, sizeof(char), dirLength, dirFile);
	//fclose(dirFile);
	//marker = dirData;

	//for (resType=0, marker=dirData; resType<4; resType++, marker+=2) {
	//   dataPos = dirData + (*marker + *(marker+1)*256);
	//   endPos = ((resType<3)? (dirData + (*(marker+2) + *(marker+3)*256))
	//      :(dirData+dirLength));
	//   resNum = 0;
	//   for (; dataPos<endPos; dataPos+=3, resNum++) {
	//      tempPos.fileName = (char *)malloc(10);
	//      sprintf(tempPos.fileName, "%sVOL.%d", gameSig,
	//         ((dataPos[0] & 0xF0) >> 4));
	//      tempPos.filePos = ((long)((dataPos[0] & 0x0F) << 16) +
	//                         (long)((dataPos[1] & 0xFF) << 8) +
	//                         (long)(dataPos[2] & 0xFF));

	//      switch (resType) {
	//         case 0: logdir[resNum] = tempPos; break;
	//         case 1: picdir[resNum] = tempPos; break;
	//         case 2: viewdir[resNum] = tempPos; break;
	//         case 3: snddir[resNum] = tempPos; break;
	//      }
	//   }
	//   if (resNum > 256) {
	//      printf("Error loading directory file.\n");
	//      printf("Too many resources.\n");
	//      exit(1);
	//   }

	//   switch (resType) {
	//      case 0: numLogics = resNum; break;
	//      case 1: numPictures = resNum; break;
	//      case 2: numViews = resNum; break;
	//      case 3: numSounds = resNum; break;
	//   }
	//}

	//free(dirData);
}

/***************************************************************************
** loadAGIDirs
**
** Purpose: To read the AGI directory files LOGDIR, PICDIR, VIEWDIR, and
** SNDDIR and store the information in a usable format. This function must
** be called once at the start of the interpreter.
***************************************************************************/
void b6LoadAGIDirs()
{
	numLogics = numPictures = numViews = numSounds = 0;
	if (version3) {
		b6LoadAGIv3Dir();
	}
	else {
#ifdef VERBOSE
		printf("Loading Indexes");
#endif // VERBOSE
		b6LoadAGIDir(0, "logdir", &numLogics);
		b6LoadAGIDir(1, "picdir", &numPictures);
		b6LoadAGIDir(2, "viewdir", &numViews);
		b6LoadAGIDir(3, "snddir", &numSounds);
#ifdef VERBOSE
		printf("Indexs Loaded\n");
#endif
	}
}

void b6InitFiles()
{
	//loadGameSig(gameSig);
	//if (strlen(gameSig) > 0) version3 = TRUE;
	b6LoadAGIDirs();
}

byte* b6ReadFileContentsIntoBankedRam(int size, byte* bank)
{
	byte* result;
	int i;
	int copySize;
	result = banked_allocTrampoline(size, bank);

#ifdef VERBOSE
	printf("Attempting to code data of size %d to %p\n", size, result);
#endif

	for (i = 0; i < size; i = i + LOCAL_WORK_AREA_SIZE)
	{
		if (i + LOCAL_WORK_AREA_SIZE > size)
		{
			copySize = size - i;

#ifdef VERBOSE
			printf("CopySize is %d, size is %d and i is %d \n", copySize, size, i);
#endif
		}
		else
		{
			copySize = LOCAL_WORK_AREA_SIZE;
#ifdef VERBOSE
			printf("CopySize is %d\n", copySize);
#endif
		}

		cbm_read(SEQUENTIAL_LFN, &GOLDEN_RAM[LOCAL_WORK_AREA_START], copySize);

#ifdef VERBOSE
		printf("result + i %p. i %d\n", result + i, i);
#endif
		memCpyBanked(result + i, &GOLDEN_RAM[LOCAL_WORK_AREA_START], *bank, copySize);
	}

#ifdef VERBOSE
	printf("Data is in bank %d at address %p and the first byte is %p and the size is %d \n", *bank, result, result[0], size);
#endif // VERBOSE

	return result;
}

#define  NORMAL     0
#define  ALTERNATE  1

///**************************************************************************
//** convertPic
//**
//** Purpose: To convert an AGIv3 resource to the AGIv2 format.
//**************************************************************************/
//void convertPic(unsigned char* input, unsigned char* output, int dataLen)
//{
//	unsigned char data, oldData, outData;
//	int mode = NORMAL, i = 0;
//
//	while (i++ < dataLen) {
//		data = *input++;
//
//		if (mode == ALTERNATE)
//			outData = ((data & 0xF0) >> 4) + ((oldData & 0x0F) << 4);
//		else
//			outData = data;
//
//		if ((outData == 0xF0) || (outData == 0xF2)) {
//			*output++ = outData;
//			if (mode == NORMAL) {
//				data = *input++;
//				*output++ = ((data & 0xF0) >> 4);
//				mode = ALTERNATE;
//			}
//			else {
//				*output++ = (data & 0x0F);
//				mode = NORMAL;
//			}
//		}
//		else
//			*output++ = outData;
//
//		oldData = data;
//	}
//}

unsigned int b6GetPositionOfFirstMessage(AGIFile* AGIData)
{
	return AGIData->noMessages * NO_BYTES_PER_MESSAGE;
}

boolean b6SeekAndCheckSignature(char* fileName, AGIFilePosType* location)
{
	boolean result = TRUE, signatureValidationPassed;
	const byte EXPECT_SIG_1 = 0x12;
	const byte EXPECTED_SIG_2 = 0x34;

	byte currentByte;

#ifdef VERBOSE_DISPLAY_MESSAGES
	printf("----Attempting to open %s for seeking data\n", fileName);
#endif // VERBOSE_DISPLAY_MESSAGES

	cx16_fseek(FILE_OPEN_ADDRESS, location->filePos);

	cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
	signatureValidationPassed = currentByte == 0x12;

	cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
	signatureValidationPassed = signatureValidationPassed & currentByte == 0x34;

	if (!signatureValidationPassed) {  /* All AGI data files start with 0x1234 */
#ifdef VERBOSE
		printf("Fail Sig. Validation %s.\n", location->fileNum);
#endif // VERBOSE
		result = FALSE;
	}

#ifdef VERBOSE
	printf("PS\n");
#endif // VERBOSE

	return result;
}

byte b6SeekAndReadLogicIntoMemory(AGIFile* AGIData, int resType)
{
	byte currentByte, bank;
	unsigned char byte1, byte2, volNum;


	cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
	volNum = currentByte;

	cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
	byte1 = currentByte;

	cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
	byte2 = currentByte;

	AGIData->totalSize = (unsigned int)(byte1)+(unsigned int)(byte2 << 8);

	if (resType == LOGIC)
	{
#ifdef VERBOSE
		printf("volNum:%d byte1:%p, byte2:%p, size:%d\n", volNum, byte1, byte2, (unsigned int)(byte1)+(unsigned int)(byte2 << 8));
#endif // VERBOSE
		cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
		byte1 = currentByte;

		cbm_read(SEQUENTIAL_LFN, &currentByte, 1);
		byte2 = currentByte;

		AGIData->codeSize = byte1 + byte2 * 256;

		AGIData->code = b6ReadFileContentsIntoBankedRam(AGIData->codeSize, &bank);
	}
	else
	{
		AGIData->code = b6ReadFileContentsIntoBankedRam(AGIData->totalSize, &bank);
	}
	return bank;

}



//https://www.liquisearch.com/what_is_avis_durgan
void xOrAvisDurgan(byte* toXOR, unsigned int* avisPos)
{
	*toXOR ^= avisDurgan[*avisPos];
	*avisPos = (*avisPos + 1) % 11;
}

/**************************************************************************
** loadAGIFile
**
** Purpose: To read an AGI data file out of a VOL file and store the data
** and data size into the AGIFile structure whose pointer is passed in.
** This function handles AGIv2 and AGIv3. It makes sure that the resources
** are converted to a common format to deal with the differences between
** the two versions. The conversions needed are as follows:
**
**  - All encrypted LOGIC message sections are decrypted. This includes
**    uncompressed AGIv3 LOGICs as well as AGIv2 LOGICs.
**  - AGIv3 PICTUREs are decompressed.
**
** In both cases the format that is easier to deal with is returned.
**************************************************************************/
void b6LoadAGIFile(int resType, AGIFilePosType* location, AGIFile* AGIData)
{
#define SEPARATOR 0

	unsigned int avisPos = 0, i, j = 0, k = 1, bufferSize = 0, offset;
	unsigned char byte1, byte2;
	byte lfn;
	byte* wholeMessageSectionData;
	byte** offsetPointer;
	boolean lastCharacterSeparator = TRUE;
	char fileName[FILE_NAME_SIZE];

	if (location->filePos == EMPTY) {
#ifdef VERBOSE
		printf("Could not find requested AGI file, as the filePos is empty.\n");
#endif // VERBOSE
		exit(0);
	}

	sprintf(&fileName[0], "vol.%d", location->fileNum);

#ifdef VERBOSE
#ifdef VERBOSE_VIEW_LOAD_DEBUG
	if (resType == VIEW)
#endif // VERBOSE_VIEW_LOAD_DEBUG
#ifdef VERBOSE_LOGIC_LOAD_DEBUG
		if (resType == LOGIC)
#endif // VERBOSE_LOGIC_LOAD_DEBUG

		{
			printf("---The file name is %s", &fileName[0]);
		}
#endif // VERBOSE

	lfn = cbm_openForSeeking(&fileName[0]);

	b6SeekAndCheckSignature(&fileName[0], location);

	AGIData->codeBank = b6SeekAndReadLogicIntoMemory(AGIData, resType);

	if (resType == LOGIC) {
		cbm_read(SEQUENTIAL_LFN, &byte1, 1);
		AGIData->noMessages = byte1;

		cbm_read(SEQUENTIAL_LFN, &byte1, 1);
		cbm_read(SEQUENTIAL_LFN, &byte2, 1);

		wholeMessageSectionData = b6ReadFileContentsIntoBankedRam(AGIData->totalSize - AGIData->codeSize - 5, &AGIData->messageBank);
		AGIData->messagePointers = (byte**)&wholeMessageSectionData[0];

		AGIData->messageData = &wholeMessageSectionData[b6GetPositionOfFirstMessage(AGIData)];

#ifdef VERBOSE
		printf("\nTrying to iterate from %d to %d\n", getMessageSectionSize);
#endif
		offsetPointer = AGIData->messagePointers;

		for (i = 0; i < getMessageSectionSize; i = i + LOCAL_WORK_AREA_SIZE) {
			if (i + LOCAL_WORK_AREA_SIZE < getMessageSectionSize)
			{
				bufferSize = LOCAL_WORK_AREA_SIZE;
			}
			else
			{
				bufferSize = (getMessageSectionSize - i);
			}

			memCpyBanked(&GOLDEN_RAM_WORK_AREA[0], &AGIData->messageData[i], AGIData->messageBank, bufferSize);
			for (j = 0; j < bufferSize; j++)
			{
				xOrAvisDurgan(&GOLDEN_RAM_WORK_AREA[j], &avisPos);

				GOLDEN_RAM_WORK_AREA[j] = trampoline_1ByteRByte(&convertAsciiByteToPetsciiByte, GOLDEN_RAM_WORK_AREA[j], HELPERS_BANK);

#ifdef VERBOSE_DISPLAY_MESSAGES_PRE_COPY_BACK
				printf("%c", GOLDEN_RAM_WORK_AREA[j]);
				if (!GOLDEN_RAM_WORK_AREA[j]) {
					printf("\n");
				}
#endif 

				if (lastCharacterSeparator)
				{
					lastCharacterSeparator = FALSE;

					offset = (unsigned int)&AGIData->messageData[i + j];
					 memCpyBanked(offsetPointer, &offset, AGIData->messageBank, 2);
					
					do
					{
#ifdef VERBOSE_DISPLAY_OFFSETS

						printf("%d offsetPointer is %p and offsetPointer == true is %d \n", k, offset, offset > 0 == TRUE);
						k++;
#endif // VERBOSE_DISPLAY_OFFSETS
						offsetPointer++;

						memCpyBanked(&offset, offsetPointer, AGIData->messageBank, 2);

					} while (offset == 0 && offsetPointer < (byte**)&AGIData->messageData[0]); //So that null message offsets are skipped
				}

				lastCharacterSeparator = GOLDEN_RAM_WORK_AREA[j] == SEPARATOR;
			}
			memCpyBanked(&AGIData->messageData[i], &GOLDEN_RAM_WORK_AREA[0], AGIData->messageBank, bufferSize);
		}

#ifdef VERBOSE_DISPLAY_MESSAGES
		printMessages(AGIData);
#endif

#ifdef VERBOSE_DISPLAY_OFFSETS
		printMessagesFromOffsets(AGIData);
#endif
	}

	//if (version3) {
	//   byte1 = fgetc(fp);
	//   byte2 = fgetc(fp);
	//   compSize = (unsigned int)(byte1) + (unsigned int)(byte2 << 8);
	//   compBuf = (unsigned char *)malloc((compSize+10)*sizeof(char));
	//   fread(compBuf, sizeof(char), compSize, fp);

	//   //initLZW();

	//   if (volNum & 0x80) {
	//      convertPic(compBuf, AGIData->data, compSize);
	//   }
	//   else if (AGIData->size == compSize) {  /* Not compressed */
	//      memcpy(AGIData->data, compBuf, compSize);

	//      if (resType == LOGIC) {
	//         /* Uncompressed AGIv3 logic files have their message sections
	//            encrypted, so we decrypt it here */
	//         fileData = AGIData->data;
	//         startPos = *fileData + (*(fileData+1))*256 + 2;
	//         numMess = fileData[startPos];
	//         endPos = fileData[startPos+1] + fileData[startPos+2]*256;
	//         fileData += (startPos + 3);
	//         startPos = (numMess * 2) + 0;

	//         for (i=startPos; i<endPos; i++)
	   //          fileData[i] ^= AVIS_DURGAN[avisPos++ % 11];
	//      }

	//      free(compBuf);
	//   }
	//   else {
	//      expand(compBuf, AGIData->data, AGIData->size);
	//   }

	//   free(compBuf);
	//   //closeLZW();
	//}
	//else {
	//   fread(AGIData->data, AGIData->size, 1, fp);
	//   if (resType == LOGIC) {
	//      /* Decrypt message section */
	//      fileData = AGIData->data;
	//      startPos = *fileData + (*(fileData+1))*256 + 2;
	//      numMess = fileData[startPos];
	//      endPos = fileData[startPos+1] + fileData[startPos+2]*256;
	//      fileData += (startPos + 3);
	//      startPos = (numMess * 2) + 0;

	//      for (i=startPos; i<endPos; i++)
	   //       fileData[i] ^= AVIS_DURGAN[avisPos++ % 11];
	//   }
	//}
#ifdef VERBOSE
	printf("Now closing the file");
#endif // VERBOSE

	cbm_close(lfn);

#ifdef VERBOSE
	//printf("File closed");
#endif // VERBOSE

}
#pragma code-name (pop)
void loadAGIFileTrampoline(int resType, AGIFilePosType* location, AGIFile* AGIData)
{
	byte previousRamBank = RAM_BANK;
	RAM_BANK = FILE_LOADER_HELPERS;
	b6LoadAGIFile(resType, location, AGIData);
	RAM_BANK = previousRamBank;
}



