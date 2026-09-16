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
    }
}

#pragma rodata (push, "BANKRAM0D")

#include <ascii_charmap.h>
const char BD_YOU_ARE_CARRYING[] = "You are carrying:";
#include <cbm_petscii_charmap.h>
#pragma rodata (pop)
void bDDisplayInventory(boolean showObject)
{
    byte ch, lastLength, thisLength;
    byte inventoryPaletteByte = 0x20;
    byte** data, * dataPtr;
    BufferStatus bufferStatus;
    boolean isFirstLetterOfWord;

    objectType object;
    char* objectName;
    int i, j;
    boolean evenObj = TRUE, foundObject = FALSE;

    bDObjects[1].roomNum = 255;
    bDObjects[2].roomNum = 255;
    bDObjects[3].roomNum = 255;
    bDObjects[4].roomNum = 255;

    memCpyBanked(&b3TextModeTileByte, &inventoryPaletteByte, TEXT_CODE_BANK, 1);

    // inputLineDisplayed = FALSE;
    // statusLineDisplayed = FALSE;

    b6SetBackgroundColour(PALETTE_COLOR_WHITE);
    b6TextMode();


    bufferStatus.bank = SPLIT_BANK;
    bufferStatus.bankedData = bCSplitBuffer; //Its a safer better to use the split buffer. As when the buffer flushes at the end it will always write the full buffer size out even if there's not data. There is a terminator so we know where the legitimate data ends, but it must not overflow
    bufferStatus.bufferCounter = 0;

    // printf("1. splitbuffer %p buffer status %p\n", bCSplitBuffer, &bufferStatus);;
    // asm("stp");

    //printf("%p\n", bufferStatus.bankedData);

    dataPtr = GOLDEN_RAM_WORK_AREA;
    data = &dataPtr;

    memset(GOLDEN_RAM_WORK_AREA, 0, LOCAL_WORK_AREA_SIZE);


    // i=0;
    // ch = BD_YOU_ARE_CARRYING[i];
    // while(ch)
    // {
    //     WRITE_NEXT(ch);
    //     ch = BD_YOU_ARE_CARRYING[i];
    //     i++;
    // }

    strcpy(GOLDEN_RAM_WORK_AREA, BD_YOU_ARE_CARRYING);
    (*data) += strlen(BD_YOU_ARE_CARRYING);
    WRITE_NEXT(NEW_LINE);

    for (i = 0; i < bDNumObjects; i++)
    {

        //printf("2. splitbuffer %p buffer status %p\n", bCSplitBuffer, &bufferStatus);;
        // asm("stp");

        object.name = bDObjects[i].name;
        object.roomNum = bDObjects[i].roomNum;
        if (object.roomNum == HAS_OBJ)
        {

            //printf("3. splitbuffer %p buffer status %p\n", bCSplitBuffer, &bufferStatus);;
            // asm("stp");
            objectName = object.name;
            thisLength = strlen(objectName);
            j = 0;
            
            if(!evenObj && foundObject)
            {
                for(j = lastLength + thisLength; j < TILES_ACROSS; j++)
                {
                     ch = SPACE;
                     WRITE_NEXT(ch);
                }
            }
            else if(foundObject)
            {
                 WRITE_NEXT(NEW_LINE);
            }
            else
            {
                foundObject = TRUE;
            }

   

            // printf("4. splitbuffer %p buffer status %p\n", bCSplitBuffer, &bufferStatus);;
            // asm("stp");

            j = 0;
            isFirstLetterOfWord = TRUE;
            ch = objectName[j];
            while (ch)
            {
                // printf("5. splitbuffer %p buffer status %p\n", bCSplitBuffer, &bufferStatus);;
                // asm("stp");
                //printf("%d >= 'a' (%d) && %d <= 'z' (%d) && %d (%d)", ch, ch >= 'a', ch, ch <= 'z', isFirstLetterOfWord , ch >= 'a' && ch <= 'z' && isFirstLetterOfWord);
                if(ch >= 97 && ch <= 122 && isFirstLetterOfWord) //Between lower a and lower z
                {
                    ch -= 32;
                }
                WRITE_NEXT(ch);
                //asm("stp");
                //printf("object name is %p %d %p objName[j] is %d data %p dataptr %p\n", &objectName, j, GOLDEN_RAM_WORK_AREA, objectName[j], data, dataPtr);
                if(ch == SPACE)
                {
                    isFirstLetterOfWord = TRUE;
                    asm("stp");
                }
                else
                {
                    isFirstLetterOfWord = FALSE;
                }
                
                
                j++;
                ch = objectName[j];
            }
            //    WRITE_NEXT(data, 0, localBufferStatus);

            lastLength = thisLength;
            
            evenObj = !evenObj;
        }
    }

    WRITE_NEXT(NEW_LINE);

    b5FlushBuffer(&bufferStatus);

    b3DisplayMessageBox(bCSplitBuffer, SPLIT_BANK, 0, 0, INVENTORY_PALETTE_NUMBER, 0, FALSE);

    // printf("6. splitbuffer %p buffer status %p\n", bCSplitBuffer, &bufferStatus);;
    // asm("stp");
    // asm("nop");

    do
    {
        GET_IN(ch);                     // Get keyboard input
    } while (!ch);

    b6SetBackgroundColour(PALETTE_COLOR_BLACK);
    b6GraphicsMode();
}

#pragma code-name (pop)

