/***************************************************************************
** AGIFILES.C
**
** Routines to load AGI resource directories and volume data. The active
** resource loader accepts uncompressed AGIv2 data; unsupported compressed
** resources and AGIv3 directory files are rejected.
**
** (c) 1997 Lance Ewing
***************************************************************************/
//#define VERBOSE_DISPLAY_FILEOFFSETS
//#define VERBOSE_DISPLAY_MESSAGES
//#define VERBOSE_DISPLAY_OFFSETS
//#define VERBOSE
//#define VERBOSE_VIEW_LOAD_DEBUG
//#define VERBOSE_PICTURE_LOAD_DEBUG
//define VERBOSE_LOGIC_LOAD_DEBUG
#include <cbm.h>


#include "general.h"
#include "agifiles.h"
#include "memoryManager.h"

#pragma rodata-name (push, "BANKRAM06")
const char B6_OPEN_FLAGS[] = ",S,R";
const char B6_FILE_AND_FLAGS[] = "%s%s";
const char B6_FILE_FLAGS[] = "rb";
const char B6_NO_FILE[] = "no file : %s.\n";
const char B6_INVALID_DIR[] = "Invalid resource directory: %s\n";
const char B6_LOG_DIR[] = "logdir";
const char B6_PIC_DIR[] = "picdir";
const char B6_VIEW_DIR[] = "viewdir";
const char B6_SND_DIR[] = "snddir";
const char B6_VOL[] = "vol.%d";
#pragma rodata-name (pop)

byte avisDurgan[11] = { 0x41, 0x76, 0x69, 0x73, 0x20, 0x44, 0x75, 0x72, 0x67, 0x61, 0x6E };//https://www.liquisearch.com/what_is_avis_durgan


#pragma bss-name (push, "BANKRAM10")
AGIFilePosType logdir[NO_DIRECTORY_ENTRYS];
AGIFilePosType picdir[NO_DIRECTORY_ENTRYS];
AGIFilePosType viewdir[NO_DIRECTORY_ENTRYS];
AGIFilePosType snddir[NO_DIRECTORY_ENTRYS];
#pragma bss-name (pop)

/* Stage records in resident RAM until the complete directory is validated. */
static byte directoryReadBuffer[NO_DIRECTORY_ENTRYS * 3 + 1];

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

	printf("Attempting to display messages at %p on bank %p\n", &AGIData->messageData[i], RAM_BANK);

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
byte b6Cbm_openForSeeking(const char* fileName)
{
	byte lfn = SEQUENTIAL_LFN;

	char fileNameAndFlags[FILE_NAME_SIZE + 4];
	byte sec_addr = FILE_OPEN_ADDRESS;

	sprintf(&fileNameAndFlags[0], B6_FILE_AND_FLAGS, &fileName[0], B6_OPEN_FLAGS);

#ifdef VERBOSE
	printf("Attempting to load file %s", fileNameAndFlags);
#endif // VERBOSE


	if (cbm_open(lfn, FILE_DEVICE, sec_addr, fileNameAndFlags) != 0)
		return 0;

	return lfn;
}

int8_t b6Cx16_fseek(uint8_t channel, uint32_t offset) {
	int8_t result;
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
	result = cbm_k_open(); // this sends the CMD bytes..

	cbm_k_close(15); // close the command channel

	return result == 0 ? 0 : -1;
}

/***************************************************************************
** loadAGIDir
**
** Purpose: To read in an individual AGI directory file. This function
** should only be called by loadAGIDirs() below.
***************************************************************************/
static boolean b6ReadAGIDir(const char* fName, unsigned int* entryCount)
{
	FILE* fp;
	size_t bytesRead;

	if (!fName || !entryCount)
		return FALSE;

	fp = fopen(fName, B6_FILE_FLAGS);
	if (!fp) {
		printf(B6_NO_FILE, fName);
		return FALSE;
	}

	bytesRead = fread(directoryReadBuffer, 1, sizeof(directoryReadBuffer), fp);
	if (ferror(fp) || bytesRead > NO_DIRECTORY_ENTRYS * 3 || bytesRead % 3 != 0) {
		fclose(fp);
		printf(B6_INVALID_DIR, fName);
		return FALSE;
	}
	if (fclose(fp) != 0) {
		printf(B6_INVALID_DIR, fName);
		return FALSE;
	}

	*entryCount = bytesRead / 3;
	return TRUE;
}

static void b6PublishDirectory(int dirNum, unsigned int start, unsigned int count, const byte* data, byte bank)
{
	AGIFilePosType* directory;
	AGIFilePosType tempPos;
	byte record[3];
	unsigned int i;

	switch (dirNum) {
	case 0: directory = logdir; break;
	case 1: directory = picdir; break;
	case 2: directory = viewdir; break;
	default: directory = snddir; break;
	}

	for (i = 0; i < count; ++i) {
		if (bank == 0)
			memcpy(record, &data[i * 3], sizeof(record));
		else
			memCpyBanked(record, (byte*)&data[i * 3], bank, sizeof(record));
		tempPos.fileNum = record[0] >> 4;
		tempPos.filePos = ((unsigned long)(record[0] & 0x0F) << 16) |
			((unsigned long)record[1] << 8) | record[2];
		b10SetResourceDirectory(&tempPos, &directory[start + i]);
	}
}

boolean b6LoadAGIDir(int dirNum, const char* fName, int* count)
{
	AGIFilePosType* directory;
	unsigned int entryCount = 0, i;
	AGIFilePosType tempPos;
	byte* record;

	if (!count || dirNum < 0 || dirNum > 3 || *count < 0 || *count > NO_DIRECTORY_ENTRYS)
		return FALSE;

	switch (dirNum) {
	case 0: directory = logdir; break;
	case 1: directory = picdir; break;
	case 2: directory = viewdir; break;
	default: directory = snddir; break;
	}

	if (!b6ReadAGIDir(fName, &entryCount))
		return FALSE;
	if (entryCount > NO_DIRECTORY_ENTRYS - *count) {
		if (fName)
			printf(B6_INVALID_DIR, fName);
		return FALSE;
	}

	for (i = 0; i < entryCount; ++i) {
		record = &directoryReadBuffer[i * 3];
		tempPos.fileNum = record[0] >> 4;
		tempPos.filePos = ((unsigned long)(record[0] & 0x0F) << 16) |
			((unsigned long)record[1] << 8) | record[2];
		b10SetResourceDirectory(&tempPos, &directory[*count + i]);
	}
	*count += entryCount;
	return TRUE;
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
boolean b6LoadAGIDirs(void)
{
	const char* names[4] = { B6_LOG_DIR, B6_PIC_DIR, B6_VIEW_DIR, B6_SND_DIR };
	unsigned int entryCounts[4];
	byte stageBank;
	byte* stagedDirectories;
	unsigned int type;
	const unsigned int directoryBytes = NO_DIRECTORY_ENTRYS * 3;

	if (version3) {
		/* The AGIv3 directory loader and decompressor are not implemented. */
		return FALSE;
	} else {
#ifdef VERBOSE
		printf("Loading Indexes");
#endif // VERBOSE
		stagedDirectories = b10BankedAlloc((int)(4 * directoryBytes), &stageBank);
		if (!stagedDirectories)
			return FALSE;

		for (type = 0; type < 4; ++type) {
			if (!b6ReadAGIDir(names[type], &entryCounts[type])) {
				b10BankedDealloc(stagedDirectories, stageBank);
				return FALSE;
			}
			memCpyBanked(stagedDirectories + type * directoryBytes,
				directoryReadBuffer, stageBank, entryCounts[type] * 3);
		}

		/* No global directory or count changes until every source file is valid. */
		for (type = 0; type < 4; ++type)
			b6PublishDirectory(type, 0, entryCounts[type],
				stagedDirectories + type * directoryBytes, stageBank);

		numLogics = entryCounts[0];
		numPictures = entryCounts[1];
		numViews = entryCounts[2];
		numSounds = entryCounts[3];
		b10BankedDealloc(stagedDirectories, stageBank);
#ifdef VERBOSE
		printf("Indexs Loaded\n");
#endif
	}
	return TRUE;
}

void b6InitFiles(void)
{
	//loadGameSig(gameSig);
	//if (strlen(gameSig) > 0) version3 = TRUE;
	if (!b6LoadAGIDirs())
		exit(EXIT_FAILURE);
}

#pragma code-name (pop)
#pragma code-name (push, "CODE")

static boolean b6ReadVolumeBytes(byte* destination, unsigned int size)
{
	int bytesRead = cbm_read(SEQUENTIAL_LFN, destination, size);
	return bytesRead >= 0 && (unsigned int)bytesRead == size;
}

static byte* b6ReadBankedVolumeData(unsigned int size, byte* bank)
{
	byte* data;
	unsigned int offset = 0;
	unsigned int chunk;

	if (size > LARGE_SIZE)
		return NULL;

	data = b10BankedAlloc(size ? (int)size : 1, bank);
	if (!data)
		return NULL;

	while (offset < size) {
		chunk = size - offset;
		if (chunk > LOCAL_WORK_AREA_SIZE)
			chunk = LOCAL_WORK_AREA_SIZE;

		if (cbm_read(SEQUENTIAL_LFN, GOLDEN_RAM_WORK_AREA, chunk) != (int)chunk ||
			!memCpyBanked(data + offset, GOLDEN_RAM_WORK_AREA, *bank, chunk)) {
			b10BankedDealloc(data, *bank);
			return NULL;
		}
		offset += chunk;
	}

	return data;
}

static boolean b6DecryptLogicMessages(AGIFile* data, unsigned int sectionSize)
{
	unsigned int tableSize = data->noMessages * NO_BYTES_PER_MESSAGE;
	unsigned int textSize = sectionSize - tableSize;
	unsigned int textOffset = 0;
	unsigned int avisPos = 0;
	unsigned int i;
	unsigned int relativeOffset;
	uint16_t encodedOffset;
	uint16_t address;
	byte* work = GOLDEN_RAM_WORK_AREA;
	byte* offsets = (byte*)data->messageData;

	data->messagePointers = (byte**)offsets;
	data->messageData = offsets + tableSize;

	if (data->noMessages) {
		while (textOffset < textSize) {
			unsigned int chunk = textSize - textOffset;
			if (chunk > LOCAL_WORK_AREA_SIZE)
				chunk = LOCAL_WORK_AREA_SIZE;
			if (!memCpyBanked(work, data->messageData + textOffset, data->messageBank, chunk))
				return FALSE;

			for (i = 0; i < chunk; ++i) {
				work[i] ^= avisDurgan[avisPos];
				if (++avisPos == 11)
					avisPos = 0;
			}

			if (!memCpyBanked(data->messageData + textOffset, work, data->messageBank, chunk))
				return FALSE;
			textOffset += chunk;
		}
	}

	for (i = 0; i < data->noMessages; ++i) {
		memCpyBanked(&encodedOffset, offsets + i * NO_BYTES_PER_MESSAGE,
			data->messageBank, NO_BYTES_PER_MESSAGE);
		if (!encodedOffset) {
			address = 0;
		} else {
			byte foundTerminator = FALSE;
			unsigned int scanOffset, chunk, j;

			if (encodedOffset < 2 + tableSize)
				return FALSE;
			relativeOffset = encodedOffset - 2 - tableSize;
			if (relativeOffset >= textSize)
				return FALSE;
			scanOffset = relativeOffset;
			while (scanOffset < textSize && !foundTerminator) {
				chunk = textSize - scanOffset;
				if (chunk > LOCAL_WORK_AREA_SIZE)
					chunk = LOCAL_WORK_AREA_SIZE;
				if (!memCpyBanked(work, data->messageData + scanOffset,
					data->messageBank, chunk))
					return FALSE;
				for (j = 0; j < chunk; ++j) {
					if (!work[j]) {
						foundTerminator = TRUE;
						break;
					}
				}
				scanOffset += chunk;
			}
			if (!foundTerminator)
				return FALSE;
			address = (uint16_t)(uintptr_t)(data->messageData + relativeOffset);
		}
		memCpyBanked(offsets + i * NO_BYTES_PER_MESSAGE, (byte*)&address,
			data->messageBank, NO_BYTES_PER_MESSAGE);
	}
	return TRUE;
}

#pragma code-name (pop)
#pragma code-name (push, "BANKRAM06")

void b6XOrAvisDurgan(byte* toXOR, unsigned int* avisPos)
{
	*toXOR ^= avisDurgan[*avisPos];
	*avisPos = (*avisPos + 1) % 11;
}

/* Load an uncompressed volume resource and publish it only when complete. */
boolean b6LoadAGIFile(int resType, AGIFilePosType* location, AGIFile* AGIData)
{
	byte lfn = 0;
	byte volumeHeader[3], signature[2], logicHeader[3];
	uint16_t messagesSize;
	unsigned int sectionSize, tableSize;
	char fileName[FILE_NAME_SIZE];
	AGIFile loaded;
	byte* messageAllocation = NULL;
	boolean success = FALSE;

	if (!location || !AGIData || resType < LOGIC || resType > SOUND ||
		location->fileNum > 15 || location->filePos == EMPTY || location->filePos > 0xFFFFF)
		return FALSE;
	memset(&loaded, 0, sizeof(loaded));

	sprintf(fileName, B6_VOL, location->fileNum);
	lfn = b6Cbm_openForSeeking(fileName);
	if (!lfn)
		return FALSE;

	if (b6Cx16_fseek(lfn, (uint32_t)location->filePos) != 0 ||
		!b6ReadVolumeBytes(signature, sizeof(signature)) ||
		signature[0] != 0x12 || signature[1] != 0x34 ||
		!b6ReadVolumeBytes(volumeHeader, sizeof(volumeHeader)))
		goto done;

	/* Compression is marked by bit 7; no decompressor is enabled in this build. */
	if ((volumeHeader[0] & 0x80) || (volumeHeader[0] & 0x7F) != location->fileNum)
		goto done;

	loaded.totalSize = (unsigned int)volumeHeader[1] | ((unsigned int)volumeHeader[2] << 8);
	if (!loaded.totalSize || loaded.totalSize > LARGE_SIZE)
		goto done;

	if (resType == LOGIC) {
		if (loaded.totalSize < 5 || !b6ReadVolumeBytes(logicHeader, 2))
			goto done;
		loaded.codeSize = (unsigned int)logicHeader[0] | ((unsigned int)logicHeader[1] << 8);
		if (loaded.codeSize > loaded.totalSize - 5)
			goto done;

		loaded.code = b6ReadBankedVolumeData(loaded.codeSize, &loaded.codeBank);
		if (!loaded.code)
			goto done;

		if (!b6ReadVolumeBytes(&logicHeader[2], 1) ||
			!b6ReadVolumeBytes(signature, 2))
			goto done;
		loaded.noMessages = logicHeader[2];
		messagesSize = (uint16_t)signature[0] | ((uint16_t)signature[1] << 8);
		sectionSize = loaded.totalSize - loaded.codeSize - 5;
		tableSize = loaded.noMessages * NO_BYTES_PER_MESSAGE;
		if (messagesSize < 2 || messagesSize - 2 != sectionSize || tableSize > sectionSize)
			goto done;

		loaded.messagePointers = NULL;
		messageAllocation = b6ReadBankedVolumeData(sectionSize, &loaded.messageBank);
		loaded.messageData = messageAllocation;
		if (!messageAllocation)
			goto done;

		if (!b6DecryptLogicMessages(&loaded, sectionSize))
			goto done;
	} else {
		loaded.codeSize = loaded.totalSize;
		loaded.code = b6ReadBankedVolumeData(loaded.totalSize, &loaded.codeBank);
		if (!loaded.code)
			goto done;
	}

	success = TRUE;

done:
	cbm_close(lfn);
	if (!success) {
		if (messageAllocation)
			b10BankedDealloc(messageAllocation, loaded.messageBank);
		if (loaded.code)
			b10BankedDealloc(loaded.code, loaded.codeBank);
		return FALSE;
	}

	*AGIData = loaded;
	return TRUE;
}
#pragma code-name (pop)
