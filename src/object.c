/***************************************************************************
** object.c
**
** Routines to load the OBJECT file. Firstly it needs to determine whether
** the file is encrypted or not. This is to accommodate some of the early
** AGIv2 games that didn't bother about having it encrypted.
**
** (c) 1997 Lance Ewing - Inital code (26 Aug 97)
***************************************************************************/

#include <stdio.h>
#include <cbm.h>

#include "general.h"
#include "object.h"

extern byte* var;

#pragma code-name (push, "BANKRAM0D")

#pragma rodata (push, "BANKRAM0D")
const char BD_OBJECT_FILE_NAME[] = "object";
const char BD_CANNOT_OPEN[] = "no object file\n";
#pragma rodata (pop)

#pragma bss-name (push, "BANKRAM0D")
int bDNumObjects;
objectType bDObjects[MAX_OBJECTS];
byte bDObjData[OBJ_NAME_CACHE_SIZE];
byte bDObjectNameLengths[MAX_OBJECTS];
unsigned int bDObjectRowPaletteAddresses[MAX_OBJECT_ROWS];
byte bDScreenObjToGameObj[MAX_OBJECT_ROWS * 2];
objectType* bDSelectedObject, * bDUnselectedObject;
unsigned char bDSelectedScreenObjNum, bDUnselectedScreenObjNum;
#pragma bss-name (pop)

/**************************************************************************
** bDGetObject
**
** Purpose: Copy object objNum from the loaded table into the caller's
** struct.
**************************************************************************/
boolean bDGetObject(byte objNum, objectType* objectType)
{
    if (!objectType || bDNumObjects <= 0 || bDNumObjects > MAX_OBJECTS ||
        objNum >= MAX_OBJECTS || objNum >= bDNumObjects)
        return FALSE;

    *objectType = bDObjects[objNum];
    return TRUE;
}

/**************************************************************************
** bDSetObject
**
** Purpose: Write the caller's object struct back into slot objNum of
** the loaded table (e.g. after changing roomNum when picking up / dropping).
**************************************************************************/
boolean bDSetObject(byte objNum, objectType* objectType)
{
    if (!objectType || bDNumObjects <= 0 || bDNumObjects > MAX_OBJECTS ||
        objNum >= MAX_OBJECTS || objNum >= bDNumObjects)
        return FALSE;

    bDObjects[objNum] = *objectType;
    return TRUE;
}

/**************************************************************************
** isObjCrypt
**
** Purpose: Checks whether the OBJECT file is encrypted with Avis Durgan
** or not. I havn't fully tested this routine, but it seems to work with
** all the AGI games that I've tried it on. What it does is check the
** end of the OBJECT file which should be all text characters if it is
** not encrypted. On the other hand, if the OBJECT file is encrypted,
** there is usually a lot of characters less than 0x20.
**************************************************************************/
boolean bDIsObjCrypt(long fileLen, byte* objData)
{
    int i, checkLen;

    if (!objData || fileLen <= 0 || fileLen > OBJ_NAME_CACHE_SIZE)
        return FALSE;

    checkLen = ((fileLen < 20) ? 10 : 20);   /* inspect last 10 or 20 bytes */

    /* TODO: Needs a fix here for Mixed Up Mother Goose */
    // ->>>

    for (i = fileLen - 1; i >= 0 && i > (fileLen - checkLen); i--) {
        if (((objData[i] < 0x20) || (objData[i] > 0x7F)) && (objData[i] != 0))
            return TRUE;                    /* non-printable => encrypted */
    }

    return FALSE;
}

/**************************************************************************
** bDLoadFile
**
** Purpose: Open "object" via the CBM seek helper and read it byte-by-byte
** into buffer. fileLen is set to the number of bytes read. Exits if the
** file is missing. Returns zero after closing the file if a read fails or
** the input exceeds OBJ_NAME_CACHE_SIZE.
**
** Returns: the logical file number that was used (already closed).
**************************************************************************/
/* BANKRAM0D has no room for the bounds-checked reader. */
#pragma code-name (push, "CODE")
byte bDLoadFile(int* fileLen, byte* buffer)
{
	byte lfn;
	byte readByte;
	int bytesRead;

	if (!fileLen || !buffer)
	{
		if (fileLen)
			*fileLen = -1;
		return 0;
	}

	*fileLen = 0;
	lfn = b6Cbm_openForSeeking(BD_OBJECT_FILE_NAME);

    if (lfn == NULL) {
        printf("Cannot find file : object\n");
        exit(1);
    }

	while ((bytesRead = cbm_read(lfn, &readByte, 1)) != 0)
    {
		if (bytesRead != 1 || *fileLen >= OBJ_NAME_CACHE_SIZE)
        {
			cbm_close(lfn);
			*fileLen = -1;
			return 0;
        }

		buffer[*fileLen] = readByte;
		++*fileLen;
    }

    cbm_close(lfn);

    return lfn;
}
#pragma code-name (pop)

/**************************************************************************
** loadObjectFile
**
** Purpose: Load the names of the inventory items from the OBJECT file and
** their starting rooms.
**************************************************************************/
boolean bDParseObjectFileData(int fileLen)
{
    int objNum, nameLen;
    byte* marker;
    word tableSize, tableEnd, nameOffset;

    bDNumObjects = 0;
    if (fileLen < 3 || fileLen > OBJ_NAME_CACHE_SIZE)
        return FALSE;

    /* Header is the byte size of the three-byte object record table. */
    tableSize = (word)bDObjData[0] | ((word)bDObjData[1] << 8);
    if ((tableSize % 3) != 0 || tableSize > (MAX_OBJECTS * 3) ||
        (tableSize + 3) > fileLen)
        return FALSE;

    tableEnd = tableSize + 3;
    marker = (byte*)bDObjData + 3;

    /* Validate every name before publishing a count to object commands. */
    for (objNum = 0; objNum < (tableSize / 3); ++objNum, marker += 3) {
        nameOffset = (word)marker[0] | ((word)marker[1] << 8);
        nameOffset += 3;
        if (nameOffset < tableEnd || nameOffset >= fileLen)
            return FALSE;

        nameLen = 0;
        while ((nameOffset + nameLen) < fileLen &&
               bDObjData[nameOffset + nameLen] != 0)
            ++nameLen;
        if ((nameOffset + nameLen) >= fileLen || nameLen > 255)
            return FALSE;
    }

    marker = (byte*)bDObjData + 3;
    for (objNum = 0; objNum < (tableSize / 3); ++objNum, marker += 3) {
        nameOffset = (word)marker[0] | ((word)marker[1] << 8);
        nameOffset += 3;
        bDObjects[objNum].name = (char*)&bDObjData[nameOffset];
        bDObjects[objNum].roomNum = marker[2];
        bDObjects[objNum].objectNum = objNum;
        nameLen = 0;
        while (bDObjData[nameOffset + nameLen] != 0)
            ++nameLen;
        bDObjectNameLengths[objNum] = nameLen;
    }

    bDNumObjects = tableSize / 3;

    return TRUE;
}

void bDLoadObjectFile()
{
    byte lfn;
    int avisPos = 0, i, fileLen;

    lfn = bDLoadFile(&fileLen, bDObjData);
	if (lfn == NULL)
	{
		bDNumObjects = 0;
		printf(BD_CANNOT_OPEN);
		exit(1);
	}

    if (fileLen < 3 || fileLen > OBJ_NAME_CACHE_SIZE) {
        bDNumObjects = 0;
        return;
    }

    if (bDIsObjCrypt(fileLen, bDObjData))
    {
        for (i = 0; i < fileLen; i++)
        {
            bDObjData[i] ^= avisDurgan[avisPos++ % 11];  /* Avis Durgan cycle */
        }
    }

    bDParseObjectFileData(fileLen);
}

void bDInitObjects()
{
    byte i, rowCounter;

    bDLoadObjectFile();

    for (i = 0; i < MAX_ROWS_DOWN;i++)
    {
        bDObjectRowPaletteAddresses[i] = MAPBASE + (i + 1) * TILE_LAYER_WIDTH * 2 + 1; //+ 1 to i as the first row is 'you are carrying', + 1 on the end to get to the palette not the tile byte
    }
}

#pragma rodata (push, "BANKRAM0D")
#include <ascii_charmap.h>
const char BD_YOU_ARE_CARRYING[] = "            You are carrying:"; //Spaces are on purpose to centre.
const char BD_NOTHING[] = "                  nothing";
const char BD_EXIT_INVENTORY[] = "    Press a key to return to the game";
const char BD_SHOW_INVENTORY[] = "   Press ENTER to select, ESC to cancel";
#pragma rodata (pop)
#include <cbm_petscii_charmap.h>



extern void bDWriteNext(byte toWrite);
extern void bDDisplayInventoryInnerNoCompare();
extern void bDDisplayInventoryInner(byte thisLength);
extern byte bDPadWordsWithSpaces(byte objectNumber, byte lastLength);
//extern void bDPadWordsWithSpacesNoCompare(byte objectNumber, byte lastLength);

#define NOTHING_TO_SELECT 0xFF
void bDShowObject(byte numObjs)
{
    byte ch, changed = FALSE;

    bDSelectedScreenObjNum = 0;
    bDUnselectedScreenObjNum = NOTHING_TO_SELECT;
    bDSelectedObject = &bDObjects[bDScreenObjToGameObj[bDSelectedScreenObjNum]];

    b6SetAndWaitForIrqState(HIGHLIGHT_INVENTORY_ROW);

    bDUnselectedScreenObjNum = bDSelectedScreenObjNum;
    bDUnselectedObject = bDSelectedObject;

    do
    {
        GET_IN(ch);
        // if(ch)                   //Wait for input
        // {
        //     printf("you pushed %d bDSelectedScreenObjNum %d\n", ch, bDSelectedScreenObjNum);
        // }
        switch (ch)
        {
        case KEY_UP:
            if (bDSelectedScreenObjNum - 2 >= 0)
            {
                bDSelectedScreenObjNum -= 2;
                changed = TRUE;
            }
            break;
        case KEY_DOWN:
            if (bDSelectedScreenObjNum + 2 < numObjs)
            {
                bDSelectedScreenObjNum += 2;
                changed = TRUE;
            }
            break;
        case KEY_LEFT:
            if (bDSelectedScreenObjNum - 1 >= 0)
            {
                bDSelectedScreenObjNum--;
                changed = TRUE;
            }
            break;
        case KEY_RIGHT:
            if (bDSelectedScreenObjNum + 1 < numObjs)
            {
                bDSelectedScreenObjNum++;
                changed = TRUE;
            }
        }

        if (changed)
        {
            bDSelectedObject = &bDObjects[bDScreenObjToGameObj[bDSelectedScreenObjNum]];
            b6SetAndWaitForIrqState(HIGHLIGHT_INVENTORY_ROW);
            changed = FALSE;

            bDUnselectedScreenObjNum = bDSelectedScreenObjNum;
            bDUnselectedObject = bDSelectedObject;
        }

    } while (ch != KEY_ENTER && ch != KEY_ESC);


    if (ch == KEY_ENTER)
    {
        var[25] = bDScreenObjToGameObj[bDSelectedScreenObjNum];
    }
    else if (ch == KEY_ESC)
    {
        var[25] = NOTHING_TO_SELECT;
    }
}

#define WRITE_ZP ZP_TMP_10
#define BUFFER_STATUS_ZP ZP_TMP_12
#define OBJECT_NAME_ZP ZP_TMP_13
#define WRITE_ZP_PTR ((byte**)WRITE_ZP)
#define BUFFER_ZP_PTR ((byte**)BUFFER_STATUS_ZP)
#define OBJECT_NAME_PTR ((byte**)OBJECT_NAME_ZP)


/**************************************************************************
** bDDisplayInventory
**
** Purpose: Build the "You are carrying:" screen into the golden-RAM work
** area / split buffer, flush it, and show it as a text-mode message box.
** Objects are listed two per row; empty inventory shows "nothing".
** If showObject is false, pad to a full box and append the "press a key"
** prompt, then wait for a key before restoring graphics mode.
**************************************************************************/
void bDDisplayInventory(boolean showObject)
{
    byte ch, lastLength = 0, thisLength, rows = 0, numSpacesAdded; //Always at least one row since the text 'nothing' displays if you carry nothing
    byte inventoryPaletteByte = 0x10, numObjs = 0;
    byte* data;
    BufferStatus bufferStatus;
    char* exitMessage;

    objectType object;
    char* objectName;
    unsigned int i;
    boolean evenObj = TRUE, foundObject = FALSE;

    *WRITE_ZP_PTR = GOLDEN_RAM_WORK_AREA;    /* assembly WRITE_NEXT starts here */
    *BUFFER_ZP_PTR = &bufferStatus;

    memCpyBanked(&b3TextModeTileByte, &inventoryPaletteByte, TEXT_CODE_BANK, 1); //Text mode only sets the stuff below the menu bar, but since the menu bar is already the right color (white), we are going to not add any extra complexity

    inputLineDisplayed = FALSE;

    b6SetBackgroundColour(PALETTE_COLOR_WHITE);

    b6TextMode();

    bufferStatus.bank = SPLIT_BANK;
    bufferStatus.bankedData = bCSplitBuffer; //Its a safer better to use the split buffer. As when the buffer flushes at the end it will always write the full buffer size out even if there's not data. There is a terminator so we know where the legitimate data ends, but it must not overflow
    bufferStatus.bufferCounter = 0;

    strcpy(GOLDEN_RAM_WORK_AREA, BD_YOU_ARE_CARRYING);
    *WRITE_ZP_PTR += strlen(BD_YOU_ARE_CARRYING);

    bDWriteNext(NEW_LINE);

    for (i = 0; i < bDNumObjects; i++)
    {
        object.name = bDObjects[i].name;
        object.roomNum = bDObjects[i].roomNum;

        if (object.roomNum == HAS_OBJ)      /* carried by the player */
        {
            objectName = object.name;

            thisLength = bDObjectNameLengths[i];

            *OBJECT_NAME_PTR = object.name; /* for the asm name-copy helpers */

            if (!evenObj && foundObject)
            {
                numSpacesAdded = bDPadWordsWithSpaces(i, lastLength);  /* second column of the pair */
                rows++; //One row for every even row
                bDObjects[i].lengthOffset = lastLength + numSpacesAdded;
            }
            else if (foundObject)
            { //Add one extra for space
                bDObjects[i].lengthOffset = 0;
                bDWriteNext(NEW_LINE);      /* start a new row for an odd item */
            }
            else
            {
                foundObject = TRUE;         /* first carried object */
                bDObjects[i].lengthOffset = 0;
            }

            bDDisplayInventoryInner(thisLength);

            lastLength = thisLength;

            evenObj = !evenObj;
            bDScreenObjToGameObj[numObjs++] = i;
        }
    }
    bDWriteNext(NEW_LINE);

    if (!foundObject)
    {
        strcpy(*WRITE_ZP_PTR, BD_NOTHING);
        *WRITE_ZP_PTR += strlen(BD_NOTHING);
    }


    for (i = 0; i < 26 - rows; i++)
    {
        bDWriteNext(NEW_LINE);          /* pad remaining rows of the box */
    }

    if(showObject && foundObject)
    {
        exitMessage = BD_SHOW_INVENTORY;
    }
    else
    {
        exitMessage = BD_EXIT_INVENTORY;
    }

    i = 0;
    ch = exitMessage[i];
    while (ch)
    {
        bDWriteNext(ch);
        i++;
        ch = exitMessage[i];
    }


    bDWriteNext('\0');                      /* terminator for the flush path */

    b5FlushBuffer(&bufferStatus);
    b3DisplayMessageBox(bCSplitBuffer, SPLIT_BANK, 0, 0, INVENTORY_PALETTE_NUMBER, 0, FALSE, FIRST_OBJECT_ROW);

    if (showObject && foundObject)
    {
        bDShowObject(numObjs);
    }
    else
    {
        do
        {
            GET_IN(ch);                     //Wait for input
        } while (!ch);
    }

    b6SetBackgroundColour(PALETTE_COLOR_BLACK);
    b6GraphicsMode();
}

#pragma code-name (pop)
