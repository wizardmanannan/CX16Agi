/*
 * dependencyResolver.c
 *
 * Resource dependency manager for the AGI interpreter (Commander X16 / banked memory).
 *
 * Loads per-game index + metadata files (logic / view / sound) from the meta/ folder
 * into BANKRAM04.  These tables describe which resources each script depends on.
 *
 * Provides the public API used by the rest of the interpreter to:
 *   - initialise the metadata tables once at start-up
 *   - load or unload a script’s dependencies (recursively for logics)
 *   - specially mark script-0 / forced dependencies so they stay resident
 *
 * All data and code for this module live in bank BANKRAM04.
 */

#include "dependencyResolver.h"

#pragma bss-name (push, "BANKRAM04")
#define INDEX_CACHE_SIZE 768 
#define LOGIC_METADATA_SIZE 700
#define SOUND_METADATA_SIZE 400
#define VIEW_METADATA_SIZE 700
byte b4LogicIndex[INDEX_CACHE_SIZE];      // Per-script index: [offset_lo, offset_hi, count] * N
byte b4SoundIndex[INDEX_CACHE_SIZE];
byte b4ViewIndex[INDEX_CACHE_SIZE];
byte b4LogicMetadata[LOGIC_METADATA_SIZE]; // Flat list of resource numbers that each script depends on
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
const char b4FileFlags[] = ",S,R";        // Commodore sequential read flags

#pragma rodata-name (pop)

#pragma bss-name (push, "BANKRAM04")
#pragma data-name (push, "BANKRAM04")
boolean b4IsInited = FALSE;                       // True once metadata files have been loaded
boolean b4IsHandlingZeroOrDependencies = FALSE;   // True while processing script 0 (or forced zero-deps)
byte b4LastRoomLoaded = 0;
#pragma data-name (pop)
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM04")

// Opens a metadata file (index or data) and reads its entire contents into the supplied buffer.
// Returns TRUE if at least one byte was successfully read.
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

// Builds the two filenames for a resource type (index + data) and loads both into the provided buffers.
void b4InitResourceMetadata(char* fileNameTemplate, byte* indexBuffer, byte* metadataBuffer, int metadataSize)
{
    byte fileName[32];

    sprintf(fileName, fileNameTemplate, b4Folder, gameId, b4IndexExtension, b4FileFlags);
    b4OpenMetadataFile(fileName, indexBuffer, INDEX_CACHE_SIZE);

    sprintf(fileName, fileNameTemplate, b4Folder, gameId, b4DataExtension, b4FileFlags);
    b4OpenMetadataFile(fileName, metadataBuffer, metadataSize);
}

// Loads all three pairs of index/metadata files (logic, sound, view) and marks the resolver as ready.
void b4InitMetadata()
{
    int result;

    b4InitResourceMetadata(b4LogicIdsFileName, b4LogicIndex, b4LogicMetadata, LOGIC_METADATA_SIZE);


    b4InitResourceMetadata(b4SoundIdsFileName, b4SoundIndex, b4SoundMetadata, SOUND_METADATA_SIZE);
    b4InitResourceMetadata(b4ViewIdsFileName, b4ViewIndex, b4ViewMetadata, VIEW_METADATA_SIZE);

    b4IsInited = TRUE;
}

// Core dependency walker.
// For a given script and resource type, walks the dependency list and either loads or discards each resource.
// When the type is LOGIC it first recursively processes the same script's VIEW and SOUND dependencies.
void b4LoadUnloadResources(byte scriptNumber, boolean shouldLoad, boolean forceLoadSubDependencies, DEPENDENCY_TYPE dependencyType)
{

    int index = 0, size = 0, scriptIndex = scriptNumber * 3;
    byte i, resourceToLoad;
    LOGICEntry localLogicEntry;
    View localView;
    Loop localLoop;

    byte* indexData;


    b5GetLogicEntry(&localLogicEntry, scriptNumber);

    switch (dependencyType)
    {
    case DEPENDENCY_LOGIC:
        indexData = b4LogicIndex;
        break;
    case DEPENDENCY_VIEW:
        indexData = b4ViewIndex;
        break;
    case DEPENDENCY_SOUND:
        indexData = b4SoundIndex;
        break;
    }

    size = indexData[scriptIndex + 2];

    if (dependencyType == DEPENDENCY_LOGIC)
    {
        // Ensure view + sound dependencies of this logic are handled first
        b4LoadUnloadResources(scriptNumber, shouldLoad, FALSE, DEPENDENCY_VIEW);
        b4LoadUnloadResources(scriptNumber, shouldLoad, FALSE, DEPENDENCY_SOUND);
    }


    if (size > 0)
    {

        // Reconstruct 16-bit offset into the metadata array
        index = indexData[scriptIndex] + (indexData[scriptIndex + 1] << 8);

        for (i = 0; i < size; i++, index++)
        {
            switch (dependencyType)
            {
            case DEPENDENCY_LOGIC:
                resourceToLoad = b4LogicMetadata[index];
                break;
            case DEPENDENCY_VIEW:
                resourceToLoad = b4ViewMetadata[index];
                break;
            case DEPENDENCY_SOUND:
                resourceToLoad = b4SoundMetadata[index];
            }


            if (shouldLoad)
            {

                switch (dependencyType)
                {
                case DEPENDENCY_LOGIC:
                    b6LoadLogicFile(resourceToLoad, forceLoadSubDependencies);
                    break;
                case DEPENDENCY_VIEW:
                    b9LoadViewFile(resourceToLoad);

                    break;
                case DEPENDENCY_SOUND:
                    bBLoadSoundFile(resourceToLoad);
                    break;
                }


                // While handling script 0 (or forced zero-deps) mark the resource so the rest of
                // the system knows it must stay resident / be treated specially.
                if (b4IsHandlingZeroOrDependencies)
                {
                    //Skip views here they are reloaded every time as palettes changes room to room.TODO: Investigate a way to refresh palette without reloading the whole view
                    switch (dependencyType)
                    {
                    case DEPENDENCY_LOGIC:
                        b5GetLogicEntry(&localLogicEntry, resourceToLoad);
                        localLogicEntry.isLogicZeroOrDependency = TRUE;
                        b5SetLogicEntry(&localLogicEntry, resourceToLoad);
                        break;
                    case DEPENDENCY_SOUND:
                        bBMarkSoundAsAZeroDependency(resourceToLoad);
                        break;
                    }
                }

            }
            else
            {
                switch (dependencyType)
                {
                case DEPENDENCY_LOGIC:
                    b6DiscardLogicFile(resourceToLoad);
                    break;
                case DEPENDENCY_VIEW:
                    b9DiscardView(resourceToLoad);
                    break;
                case DEPENDENCY_SOUND:
                    bBDiscardSoundFile(resourceToLoad);
                    break;
                }
            }
        }
    }
}

//Using this last parameter you can force a search for script 0's dependencies even when it itself is already loaded. We need it because script 0 will be called before the dependency resolver is ready eg. The b4Set_game_id  -> b4InitMetadata call is complete
//Note: It is usually script 0 that calls b4Set_game_id, but some games have a different script; in this case both the dependencies of both it and script 0 need to be force loaded.
void b4LoadUnloadDependencies(byte scriptNumber, boolean shouldLoad, boolean forceLoadSubDependencies)
{
    if (b4IsInited)
    {

        if (scriptNumber == 0)
        {
            b4IsHandlingZeroOrDependencies = TRUE;
        }

        b4LoadUnloadResources(scriptNumber, shouldLoad, forceLoadSubDependencies, DEPENDENCY_LOGIC);


        if (scriptNumber == 0)
        {
            b4IsHandlingZeroOrDependencies = FALSE;
        }
    }
}

void b4SetLastRoomLoaded(byte scriptNumber)
{
    b4LastRoomLoaded = scriptNumber;
}

byte b4GetLastRoomLoaded()
{
    return b4LastRoomLoaded;
}

#pragma code-name (pop)