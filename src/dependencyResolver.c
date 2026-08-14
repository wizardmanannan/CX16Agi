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
const char b4LogicIdsFileName[] = "%s%s-logic-ids.%s%s";
const char b4SoundIdsFileName[] = "%s%s-sound-ids.%s%s";
const char b4ViewIdsFileName[] = "%s%s-view-ids.%s%s";
// const char* b4IdsLoadOrder[] = { &b4ScriptIdsFileName, &b4SoundIdsFileName, &b4ViewIdsFileName };
// const char b4Extensions[][] = {{"idx"}, {"bin"}};


const char b4IndexExtension[] = "idx";
const char b4DataExtension[] = "bin";
const char b4Folder[] = "meta/";
const char b4FileFlags[] = ",S,R";

#pragma rodata-name (pop)

#pragma bss-name (push, "BANKRAM04")
#pragma data-name (push, "BANKRAM04")
boolean b4IsInited = FALSE;
boolean b4IsHandlingZeroOrDependencies = FALSE;
#pragma data-name (pop)
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM04")
boolean b4OpenMetadataFile(char* fileName, byte* buffer, int size)
{
    byte fileOpenResult, result = FALSE;
    int bytes;

    fileOpenResult = cbm_open(SEQUENTIAL_LFN, FILE_DEVICE, FILE_OPEN_ADDRESS, fileName);

    if (!fileOpenResult) //This should be not, because cbm_open returns 0 for success and something else for an error code
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

    b4IsInited = TRUE;
}

void b4LoadUnloadLogics(byte scriptNumber, boolean shouldLoad, boolean forceLoadSubDependencies, DEPENDENCY_TYPE dependencyType)
{

    int index = 0, size = 0;
    byte scriptIndex = scriptNumber * 3, i, logicToLoad;
    LOGICEntry localLogicEntry;

    // if(shouldLoad)
      // {
      //     printf("loading %d room %d\n", scriptNumber, *((byte*)0x400));
      // }
      // else
      // {
      //     printf("unloading %d\n", scriptNumber);
      // }

    size = b4LogicIndex[scriptIndex + 2];
    //printf("your size is %d\n", size);
    if (size > 0)
    {

        switch (dependencyType)
        {
        case DEPENDENCY_LOGIC:
            index = b4LogicIndex[scriptIndex] + (b4LogicIndex[scriptIndex + 1] << 8);
            break;
        }

        //printf("your index is %d which is %d + (%d << 8 (%d)) = %d \n", index, b4LogicIndex[scriptIndex], b4LogicIndex[scriptIndex + 1], b4LogicIndex[scriptIndex + 1] << 8, b4LogicIndex[scriptIndex] + (b4LogicIndex[scriptIndex + 1] << 8));

        for (i = 0; i < size; i++, index++)
        {
            switch (dependencyType)
            {
            case DEPENDENCY_LOGIC:
                logicToLoad = b4LogicMetadata[index];
                break;
            }


            if (shouldLoad)
            {

                switch (dependencyType)
                {
                case DEPENDENCY_LOGIC:
                    b6LoadLogicFile(logicToLoad, forceLoadSubDependencies);
                    break;
                }


                if (b4IsHandlingZeroOrDependencies)
                {
                    switch (dependencyType)
                    {
                    case DEPENDENCY_LOGIC:
                        b5GetLogicEntry(&localLogicEntry, scriptNumber);
                        localLogicEntry.isLogicZeroOrDependency = TRUE;
                        b5SetLogicEntry(&localLogicEntry, scriptNumber);
                        break;
                    }
                }

            }
            else
            {
                switch (dependencyType)
                {
                case DEPENDENCY_LOGIC:
                    b6DiscardLogicFile(logicToLoad);
                    break;
                }
            }
        }
    }
}

//Using this last parameter you can force a search for script 0's dependencies even when it itself is already loaded. We need it because script 0 will be called before the dependency resolver is ready eg. The b4Set_game_id  -> b4InitMetadata call is complete
//Note: It is usually script 0 that calls b4Set_game_id, but some games have a different script; in this case both the dependencies of both it and script 0 need to be force loaded.
void b4LoadUnloadDependencies(byte scriptNumber, boolean shouldLoad, boolean forceLoadSubDependencies, DEPENDENCY_TYPE dependencyType)
{
    if (b4IsInited)
    {
        if (scriptNumber == 0)
        {
            b4IsHandlingZeroOrDependencies = TRUE;
        }

        b4LoadUnloadLogics(scriptNumber, shouldLoad, forceLoadSubDependencies, dependencyType);

        if (scriptNumber == 0)
        {
            b4IsHandlingZeroOrDependencies = FALSE;
        }
    }
}

#pragma code-name (pop)