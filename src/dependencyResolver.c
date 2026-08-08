#include "dependencyResolver.h"

#pragma bss-name (push, "BANKRAM04")
#define INDEX_CACHE_SIZE 768 
#define LOGIC_METADATA_SIZE 700
#define SOUND_METADATA_SIZE 400
#define VIEW_METADATA_SIZE 700
byte b4LogicIndex[INDEX_CACHE_SIZE];
byte b4SoundIndex[INDEX_CACHE_SIZE];
byte b4ViewIndex[INDEX_CACHE_SIZE];
byte b4LogicMetadata[LOGIC_METADATA_SIZE];
byte b4SoundMetadata[SOUND_METADATA_SIZE];
byte b4ViewMetadata[VIEW_METADATA_SIZE];
#pragma bss-name (pop)


#pragma rodata-name (push, "BANKRAM04")
const char b4LogicIdsFileName [] = "%s%s-logic-ids.%s%s";
const char b4SoundIdsFileName [] = "%s%s-sound-ids.%s%s";
const char b4ViewIdsFileName [] = "%s%s-view-ids.%s%s";
// const char* b4IdsLoadOrder[] = { &b4ScriptIdsFileName, &b4SoundIdsFileName, &b4ViewIdsFileName };
// const char b4Extensions[][] = {{"idx"}, {"bin"}};


const char b4IndexExtension [] = "idx";
const char b4DataExtension [] = "bin";
const char b4Folder[] = "meta/";
const char b4FileFlags[] = ",S,R";

#pragma rodata-name (pop)

#pragma bss-name (push, "BANKRAM04")
#pragma data-name (push, "BANKRAM04")
boolean b4IsInited = FALSE;
#pragma data-name (pop)
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM04")
boolean b4OpenMetadataFile(char* fileName, byte* buffer, int size)
{
    byte fileOpenResult, result = FALSE;
    int bytes;

    fileOpenResult = cbm_open(SEQUENTIAL_LFN, FILE_DEVICE, FILE_OPEN_ADDRESS, fileName);

    if(!fileOpenResult) //This should be not, because cbm_open returns 0 for success and something else for an error code
    {
        bytes = cbm_read(SEQUENTIAL_LFN, buffer, size);
        if (bytes != 0) {
            result = TRUE;
        }       
                 
        cbm_close(SEQUENTIAL_LFN);
    }

    return result;
}

void b4InitResourceMetadata(char* fileNameTemplate, byte* indexBuffer, byte* metadataBuffer, int metadataSize)
{
    byte fileName[32];

    //printf("%p\n", fileNameTemplate);

    sprintf(fileName, fileNameTemplate, b4Folder, gameId, b4IndexExtension, b4FileFlags);
    b4OpenMetadataFile(fileName, indexBuffer, INDEX_CACHE_SIZE);

    //printf("fileName %p inded buffer %p fileNameTemplate %p\n", &fileName[0], &indexBuffer[0], &fileNameTemplate[0]);        
    //asm("stp");

    sprintf(fileName, fileNameTemplate, b4Folder, gameId, b4DataExtension, b4FileFlags);
    b4OpenMetadataFile(fileName, metadataBuffer, metadataSize);    
}

void b4InitMetadata()
{
   int result;

    //printf("%p\n", b4LogicIdsFileName);
    b4InitResourceMetadata(b4LogicIdsFileName, b4LogicIndex, b4LogicMetadata, LOGIC_METADATA_SIZE);


    b4InitResourceMetadata(b4SoundIdsFileName, b4SoundIndex, b4SoundMetadata, SOUND_METADATA_SIZE);
    b4InitResourceMetadata(b4ViewIdsFileName, b4ViewIndex, b4ViewMetadata, VIEW_METADATA_SIZE);

    //printf("li %p lm %p si %p sm %p vi %p vm %p\n", b4LogicIndex, b4LogicMetadata, b4SoundIndex, b4SoundMetadata, b4ViewIndex, b4ViewMetadata);

    //asm("stp");

    b4IsInited = TRUE;
}

#pragma wrapped-call (push, trampoline, LOGIC_CODE_BANK)
void b6LoadLogicFile(byte logFileNum);
void b6DiscardLogicFile(byte logFileNum);
#pragma wrapped-call (pop)

void b4LoadUnloadDependencies(byte scriptNumber, boolean shouldLoad)
{
    int index = 0, size = 0;
    byte scriptIndex = scriptNumber * 3, i, logicToLoad;

    // if(shouldLoad)
    // {
    //     printf("loading %d\n", scriptNumber);
    // }
    // else
    // {
    //     printf("unloading %d\n", scriptNumber);
    // }

    if(b4IsInited)
    {
        size = b4LogicIndex[scriptIndex + 2];
        //printf("your size is %d\n", size);
        if(size > 0)
        {
            index = b4LogicIndex[scriptIndex] + (b4LogicIndex[scriptIndex + 1] << 8);

        //printf("your index is %d which is %d + (%d << 8 (%d)) = %d \n", index, b4LogicIndex[scriptIndex], b4LogicIndex[scriptIndex + 1], b4LogicIndex[scriptIndex + 1] << 8, b4LogicIndex[scriptIndex] + (b4LogicIndex[scriptIndex + 1] << 8));

            for(i = 0; i < size; i++, index++)
            {
               logicToLoad = b4LogicMetadata[index];
               //printf("you are loading %d\n", logicToLoad);

               if(shouldLoad)
               {
                b6LoadLogicFile(logicToLoad);
               }
               else
               {
                b6DiscardLogicFile(logicToLoad);
               }
            }
        }
    }
}

#pragma code-name (pop)