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
#pragma bss-name (pop)

void bDGetObject(byte objNum, objectType* objectType)
{
    *objectType = bDObjects[objNum];
}

void bDSetObject(byte objNum, objectType* objectType)
{
    bDObjects[objNum] = *objectType;
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

    checkLen = ((fileLen < 20) ? 10 : 20);

    /* TODO: Needs a fix here for Mixed Up Mother Goose */
    // ->>>

    for (i = fileLen - 1; i > (fileLen - checkLen); i--) {
        if (((bDObjData[i] < 0x20) || (bDObjData[i] > 0x7F)) && (bDObjData[i] != 0))
            return TRUE;
    }

    return FALSE;
}

byte bDLoadFile(int* fileLen, byte* buffer)
{
    byte lfn = b6Cbm_openForSeeking(BD_OBJECT_FILE_NAME);

    if (lfn == NULL) {
        printf("Cannot find file : object\n");
        exit(1);
    }

    for (*fileLen = 0; cbm_read(lfn, buffer++, 1); *fileLen += 1)
    {
        if (*fileLen > OBJ_NAME_CACHE_SIZE)
        {
            printf(BD_CANNOT_OPEN);
            exit(0);
        }
    }

    cbm_close(lfn);

    return lfn;
}

/**************************************************************************
** loadObjectFile
**
** Purpose: Load the names of the inventory items from the OBJECT file and
** their starting rooms.
**************************************************************************/
void bDLoadObjectFile()
{
    byte lfn;
    int avisPos = 0, objNum, i, strPos = 0;
    int fileLen;
    byte* marker;
    word index;

    lfn = bDLoadFile(&fileLen, bDObjData);

    marker = (byte*)bDObjData + 3;

    if (bDIsObjCrypt(fileLen, bDObjData))
    {
        for (i = 0; i < fileLen; i++)
        {
            bDObjData[i] ^= avisDurgan[avisPos++ % 11];
        }
    }

    bDNumObjects = (((bDObjData[1] * 256) + bDObjData[0]) / 3);

    for (objNum = 0; objNum < bDNumObjects; objNum++, strPos = 0, marker += 3) {
        index = *(marker)+256 * (*(marker + 1)) + 3;
        bDObjects[objNum].name = (char*)&bDObjData[index];
        bDObjects[objNum].roomNum = *(marker + 2);
        bDObjectNameLengths[objNum] = strlen(bDObjects[objNum].name);
    }
}

#pragma rodata (push, "BANKRAM0D")
#include <ascii_charmap.h>
const char BD_YOU_ARE_CARRYING[] = "            You are carrying:"; //Spaces are on purpose to centre.
const char BD_NOTHING[] = "                  nothing";
const char BD_EXIT_INVENTORY[] = "    Press a key to return to the game";
#pragma rodata (pop)
#include <cbm_petscii_charmap.h>



extern void bDWriteNext(byte toWrite);
extern void bDDisplayInventoryInnerNoCompare();
extern void bDDisplayInventoryInner(byte thisLength);
extern void bDPadWordsWithSpaces(byte objectNumber, byte lastLength);
//extern void bDPadWordsWithSpacesNoCompare(byte objectNumber, byte lastLength);

#define WRITE_ZP ZP_TMP_10
#define BUFFER_STATUS_ZP ZP_TMP_12
#define OBJECT_NAME_ZP ZP_TMP_13
#define WRITE_ZP_PTR ((byte**)WRITE_ZP)
#define BUFFER_ZP_PTR ((byte**)BUFFER_STATUS_ZP)
#define OBJECT_NAME_PTR ((byte**)OBJECT_NAME_ZP)

void bDDisplayInventory(boolean showObject)
{
    byte ch, lastLength = 0, thisLength, rows = 1; //Always at least one row since the text 'nothing' displays if you carry nothing
    byte inventoryPaletteByte = 0x10;
    byte* data;
    BufferStatus bufferStatus;
    boolean isFirstLetterOfWord;

    objectType object;
    char* objectName;
    unsigned int i, j, inventoryInnerWriteAddr;
    boolean evenObj = TRUE, foundObject = FALSE;

    *WRITE_ZP_PTR = GOLDEN_RAM_WORK_AREA;
    *BUFFER_ZP_PTR = &bufferStatus;

    for (i = 1; i < 55; i++) //Uncomment this when you wants lots of inventory items for testing in kq3
    {
        if (i >= 44 && i <= 46)
        {
           continue;
        }

            // if(i == 12 || i == 23 || i == 7 || i == 1)
            // {
            //     continue;
            // }

        

        bDObjects[i].roomNum = 255;
    }

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

        if (object.roomNum == HAS_OBJ)
        {
            objectName = object.name;

            thisLength = bDObjectNameLengths[i];
            //printf("tl %d\n", thisLength);

            *OBJECT_NAME_PTR = object.name;
            
            j = 0;

            if (!evenObj && foundObject)
            {
                bDPadWordsWithSpaces(i, lastLength);
                rows++; //One row for every even row
            }
            else if (foundObject)
            {
                bDWriteNext(NEW_LINE);
            }
            else
            {
                foundObject = TRUE;
            }

            bDDisplayInventoryInner(thisLength);
            
            lastLength = thisLength;

            evenObj = !evenObj;
        }
    }
    bDWriteNext(NEW_LINE);

    if (!foundObject)
    {
        strcpy(*WRITE_ZP_PTR, BD_NOTHING);
        *WRITE_ZP_PTR += strlen(BD_NOTHING);
    }

    if (!showObject)
    {
        for (i = 0; i < 27 - rows; i++)
        {
            bDWriteNext(NEW_LINE);
        }

        i = 0;
        ch = BD_EXIT_INVENTORY[i];
        while (ch)
        {
            bDWriteNext(ch);
            i++;
            ch = BD_EXIT_INVENTORY[i];
        }
    }

    bDWriteNext('\0');

    b5FlushBuffer(&bufferStatus);
    b3DisplayMessageBox(bCSplitBuffer, SPLIT_BANK, 0, 0, INVENTORY_PALETTE_NUMBER, 0, FALSE, FIRST_OBJECT_ROW);
    //printf("the split buffer is on %p\n", bCSplitBuffer);

           
    do
    {
        GET_IN(ch);                     // Get keyboard input
    } while (!ch);

    b6SetBackgroundColour(PALETTE_COLOR_BLACK);
    b6GraphicsMode();

    //asm("stp");
    asm("nop");
}

#pragma code-name (pop)

