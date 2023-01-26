#ifndef _MEMORYMANAGER_H_
#define _MEMORYMANAGER_H_

#ifndef _MSC_VER
#include <cx16.h>
#endif

#include "general.h"

#ifndef _MSC_VER
#include <cx16.h>
#endif // !_MSC_VER

#define TINY_SEG_ORDER  0
#define EXTRA_SMALL_SEG_ORDER  1
#define SMALL_SEG_ORDER  2
#define MEDIUM_SEG_ORDER  3
#define LARGE_SEG_ORDER  4

#define TINY_NO_SEGMENTS  80
#define EXTRA_SMALL_NO_SEGMENTS 50
#define SMALL_NO_SEGMENTS 30
#define MEDIUM_NO_SEGMENTS 20
#define LARGE_NO_SEGMENTS 5

#define TINY_SIZE  100
#define EXTRA_SMALL_SIZE 160
#define SMALL_SIZE 1600
#define MEDIUM_SIZE 3200
#define LARGE_SIZE 8000

#define TINY_NO_BANKS  1
#define EXTRA_SMALL_NO_BANKS 1
#define SMALL_NO_BANKS 7
#define MEDIUM_NO_BANKS 9
#define LARGE_NO_BANKS 7

#define TINY_FIRST_BANK  0x10
#define EXTRA_SMALL_FIRST_BANK 0x11
#define SMALL_FIRST_BANK 0x12
#define MEDIUM_FIRST_BANK 0x19
#define LARGE_FIRST_BANK 0x22

#define ALLOCATION_BANK 60

#define NO_SIZES 5

#define BANK_SIZE 8000

#define ALLOCATION_ARRAY_START 0
#define LOGDIR_START 186
#define PICDIR_START 1462
#define SOUNDDIR_START 2738
#define VIEWDIR_START 4014
#define LOGIC_ENTRY_START 5290
#define LOGIC_FILE_START 7331
#define MENU_START 0
#define MENU_CHILD_START 181
#define VIEWTAB_START 3782
#define LOADED_VIEW_START 4623

#define DIR_SIZE 6
#define LOGIC_ENTRY_SIZE 8
#define LOGIC_FILE_SIZE 2

#define TOTAL_SIZE_OF_DIR 1275
#define TOTAL_SIZE_OF_LOGIC_ENTRY 2040
#define TOTAL_SIZE_OF_LOGIC_DATA 7331
#define TOTAL_SIZE_OF_VIEWTAB_DATA 840
#define TOTAL_SIZE_OF_LOADED_VIEWS 1536

#define DIRECTORY_BANK 60
#define LOGIC_ENTRY_BANK 60
#define LOGIC_FILE_BANK 60
#define MENU_BANK 61
#define INSTRUCTION_HANDLER_BANK 5
#define IF_LOGIC_HANDLERS_BANK 5
#define VIEWTAB_BANK 61
#define LOADED_VIEW_BANK 61

#define FIRST_CODE_BANK 0x1
#define LAST_CODE_BANK 0x5
#define NO_CODE_BANKS 14

#define LRU_CACHE_LOGIC_BANK 0xE
#define LRU_CACHE_LOGIC_STRUCT_START 8183
#define LRU_CACHE_LOGIC_DATA_START 8173
#define LRU_CACHE_VIEW_STRUCT_START 8165
#define LRU_CACHE_VIEW_DATA_START 8145
#define LRU_CACHE_LOGIC_DATA_SIZE 10
#define LRU_CACHE_LOGIC_VIEW_SIZE 20



//Code Banks
#define LOAD_DIRS_BANK 0x6
#define FILE_LOADER_HELPERS 0x6
#define MEKA_BANK 0x7
#define LOGIC_CODE_BANK 0x8
#define VIEW_CODE_BANK_1 0x9
#define VIEW_CODE_BANK_2 0xA
#define VIEW_CODE_BANK_3 0xB
#define VIEW_CODE_BANK_4 0xC
#define VIEW_CODE_BANK_5 0xD

//Golden RAM
#define VARS_AREA_START 0
#define VARS_AREA_SIZE 256

#define FLAGS_AREA_START 257
#define FLAGS_AREA_SIZE 256

#define LOCAL_WORK_AREA_START 514
#define LOCAL_WORK_AREA_SIZE 500
#define PARAMETERS_START 1015

#ifdef _MSC_VER //Used for testing under windows
extern byte* banked;
#endif 

#define GOLDEN_RAM        ((unsigned char *)0x0400)
#define GOLDEN_RAM_PARAMS_AREA &GOLDEN_RAM[PARAMETERS_START]

extern int _noSegments;

#ifndef _MSC_VER
extern void _BANKRAM01_SIZE__[], _BANKRAM02_SIZE__[], _BANKRAM03_SIZE__[], _BANKRAM04_SIZE__[], _BANKRAM05_SIZE__[], _BANKRAM06_SIZE__[], _BANKRAM07_SIZE__[], _BANKRAM08_SIZE__[], _BANKRAM09_SIZE__[], _BANKRAM0A_SIZE__[], _BANKRAM0B_SIZE__[], _BANKRAM0C_SIZE__[], _BANKRAM0D_SIZE__[], _BANKRAM0E_SIZE__[];
#endif // !_MSC_VER

typedef struct {          /* DIR entry structure */
	byte firstBank;
	byte noBanks;
	int segmentSize;
	byte noSegments;
	byte* start;
} MemoryArea;

void initDynamicMemory();

void memoryMangerInit();
byte* banked_alloc(int size, byte* bank);
boolean banked_dealloc(byte* ptr, byte bank);
void initSegments(byte segOrder, byte noBanks, int segmentSize, byte noSegments, byte firstBank);
byte getFirstSegment(byte size);

#endif

