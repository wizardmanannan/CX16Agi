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
const char BD_CANNOT_OPEN[] = "Cannot find file : object\n";
#pragma rodata (pop)

#pragma bss-name (push, "BANKRAM0D")
int bFNumObjects;
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

    marker = (byte*) bDObjData + 3;
                
    if (bDIsObjCrypt(fileLen, bDObjData))
    {
        for (i = 0; i < fileLen; i++)
        {
           bDObjData[i] ^= avisDurgan[avisPos++ % 11];
        }
    }

    bFNumObjects = (((bDObjData[1] * 256) + bDObjData[0]) / 3);

    for (objNum = 0; objNum < bFNumObjects; objNum++, strPos = 0, marker += 3) {
        index = *(marker)+256 * (*(marker + 1)) + 3;
        bDObjects[objNum].name = (char*) &bDObjData[index];
        bDObjects[objNum].roomNum = *(marker + 2);
    }
}
#pragma code-name (pop)

