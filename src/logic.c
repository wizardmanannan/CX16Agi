/**************************************************************************
** LOGIC.C
**
** These functions are used to load LOGIC files into a structure of type
** LOGICFile. It uses the general function LoadAGIFile to load the data
** from the VOL file itself.
**
** (c) 1997 Lance Ewing - Original code (2 July 97)
**                        Changed (26 Aug 97)
**************************************************************************/
//#define VERBOSE
#include <string.h>

#include "agifiles.h"
#include "general.h"
#include "logic.h"
#include "memoryManager.h"

/* The logics array is the array that holds all the information about the
** logic files. A boolean flag determines whether the logic is loaded or
** not. If it isn't loaded, then the data is not in memory. */
LOGICEntry* logics = (LOGICEntry*)&BANK_RAM[LOGIC_ENTRY_START];

LOGICEntry** logicEntryAddressesLow = (LOGICEntry**)&BANK_RAM[LOGIC_ENTRY_ADDRESSES_START];
LOGICEntry** logicEntryAddressesHigh = &((LOGICEntry**)&BANK_RAM[LOGIC_ENTRY_ADDRESSES_START])[128];
void getLogicFile(LOGICFile* logicFile, byte logicFileNo)
{
	byte previousBank = RAM_BANK;
	LOGICEntry logicEntry;

#ifdef VERBOSE
	printf("attempting to write to %p for logicfileno %d \n", logicFile, logicFileNo);
#endif // VERBOSE
	RAM_BANK = LOGIC_BANK;

	logicEntry = logics[logicFileNo];

	RAM_BANK = LOGIC_BANK;
	*logicFile = *logicEntry.data;

#ifdef VERBOSE
	printf("logic entry address is %p \n", logicEntry.data);
	printf("the code bank is %p\n", logicFile->codeBank);
#endif // VERBOSE

	RAM_BANK = previousBank;
}

void setLogicFile(LOGICFile* logicFile, byte logicFileNo)
{
	byte previousBank = RAM_BANK;
	LOGICEntry logicEntry;

	RAM_BANK = LOGIC_BANK;

	logicEntry = logics[logicFileNo];

	RAM_BANK = LOGIC_BANK;

	*logicFile;
	*(logicEntry.data) = *logicFile;

	RAM_BANK = previousBank;
}

void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo)
{
	byte previousBank = RAM_BANK;

	RAM_BANK = LOGIC_BANK;

	logics[logicFileNo] = *logicEntry;

	RAM_BANK = previousBank;
}

void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo)
{
	byte previousBank = RAM_BANK;

	RAM_BANK = LOGIC_BANK;
	*logicEntry = logics[logicFileNo];

	RAM_BANK = previousBank;
}

/***************************************************************************
** initLogics
**
** Purpose: To initialize the array that holds information on the loaded
** logic files and also to load LOGIC.0 into that array. All other logics
** are marked as unloaded. Make sure you call initFiles() before calling
** this function.
***************************************************************************/
#pragma code-name (push, "BANKRAM08")
void b8Dummy() {}
#pragma code-name (pop)
#pragma code-name (push, "BANKRAM06")
void b6InitLogics()
{
	int i;
	LOGICEntry logicEntry;
	for (i = 0; i < 256; i++) {
		getLogicEntry(&logicEntry, i);

		logicEntry.loaded = FALSE;
		logicEntry.entryPoint = 0;
		logicEntry.currentPoint = 0;
		logicEntry.data = &((LOGICFile*)&BANK_RAM[LOGIC_FILE_START])[i];
		setLogicEntry(&logicEntry, i);
#ifdef VERBOSE
		printf("%d: currentPoint: %p, data: %p, dataBank: %d, loaded %d  &logics[i] %p\n",i, logicEntry.currentPoint, logicEntry.data, logicEntry.dataBank, logicEntry.loaded, &logics[i]);
#endif // VERBOSE
	}
	b6LoadLogicFile(0);
}


/**************************************************************************
** loadLogicFile
**
** Purpose: To load a LOGIC file, decode the messages, and store in a
** suitable structure.
**************************************************************************/
void b6LoadLogicFile(byte logFileNum)
{
	AGIFile tempAGI;
	AGIFilePosType agiFilePosType;
	LOGICEntry logicEntry;
	LOGICFile logicData;

	int dummy;

	getLogicEntry(&logicEntry, logFileNum);
	getLogicDirectory(&agiFilePosType, &logdir[logFileNum]);

#ifdef VERBOSE
	printf("\n%d Retrieved file num %d, Offset %lu\n", logFileNum, agiFilePosType.filePos);
#endif // VERBOSE

	/* Load LOGIC file, calculate logic code length, and copy
	** logic code into tempLOGIC. */


#ifdef VERBOSE
	printf("Loading Logic %d\n", logFileNum);
#endif // VERBOSE
	lruCacheGetTrampoline(LOGIC, logFileNum, &agiFilePosType, &tempAGI);

	getLogicFile(&logicData, logFileNum);
	logicData.codeBank = tempAGI.codeBank;
	logicData.codeSize = tempAGI.codeSize;
	logicData.logicCode = tempAGI.code;
	logicData.messageBank = tempAGI.messageBank;
	logicData.messages = (byte**)tempAGI.messagePointers;
	logicData.numMessages = tempAGI.noMessages;

#ifdef VERBOSE
	printf("The codebank is %d, the code size is %d, the messageBank is %d, \n and the number of messages is %d, the code pointer is non zero and matched against temp agi %d the message pointer is non zero and matches temp agi %d \n",
		logicData.codeBank, logicData.codeSize, logicData.messageBank, logicData.numMessages
		, logicData.logicCode == tempAGI.code && logicData.logicCode
		, logicData.messages == (byte**)tempAGI.messagePointers && logicData.messages
	);

#ifdef VERBOSE
	printf("currentPoint: %p, data: %p, dataBank: %d, loaded %d \n", logicEntry.currentPoint, logicEntry.data, logicEntry.dataBank, logicEntry.loaded);
#endif

#endif // VERBOSE

	setLogicFile(&logicData, logFileNum);

	logicEntry.loaded = TRUE;

	setLogicEntry(&logicEntry, logFileNum);


}

/**************************************************************************
** discardLogicFile
**
** Purpose: To deallocate all the memory associated with a LOGICFile
** struct. This function can only be used with dynamically allocated
** LOGICFile structs which is what I plan to use.
**************************************************************************/
void b6DiscardLogicFile(byte logFileNum)
{
	int messNum;
	LOGICFile logicData;
	LOGICEntry logicEntry;

	getLogicFile(&logicData, logFileNum);
	getLogicEntry(&logicEntry, logFileNum);

	if (logicEntry.loaded) {

		if (logicEntry.loaded && !banked_deallocTrampoline((byte*)logicData.messages, logicData.messageBank))
		{
#ifdef VERBOSE
			printf("Failed to deallocate messages for logic %d in bank %d", logFileNum, logicData.codeBank);
#endif // VERBOSE
		}

		if (logicEntry.loaded && !banked_deallocTrampoline((byte*)logicData.logicCode, logicData.codeBank))
		{
#ifdef VERBOSE
			printf("Failed to deallocate messages for logic %d in bank %d", logFileNum, logicData.codeBank);
#endif
		}

		logicEntry.loaded = FALSE;
		setLogicEntry(&logicEntry, logFileNum);
	}
}

#pragma code-name (pop)