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
#define MEMORY_MANAGER_BANK_SIZE 8000

#define TINY_NO_BANKS  1
#define EXTRA_SMALL_NO_BANKS 1
#define SMALL_NO_BANKS 6
#define MEDIUM_NO_BANKS 8
#define LARGE_NO_BANKS 6

#define TINY_FIRST_BANK  0x13
#define EXTRA_SMALL_FIRST_BANK 0x14
#define SMALL_FIRST_BANK 0x15
#define MEDIUM_FIRST_BANK 0x1B
#define LARGE_FIRST_BANK 0x23

#define ALLOCATION_BANK 60

#define NO_SIZES 5

#define BANK_SIZE (0xBFFF-0xA000 + 1)

#define TOTAL_SIZE_OF_DIR 1275
#define TOTAL_SIZE_OF_LOGIC_ENTRY 2040
#define TOTAL_SIZE_OF_LOGIC_DATA 7331
#define TOTAL_SIZE_OF_VIEWTAB_DATA 840
#define TOTAL_SIZE_OF_LOADED_VIEWS 1536
#define TOTAL_SIZE_OF_MEMORY_AREA 35
#define TOTAL_SIZE_LOGIC_ENTRY_ADDRESSES 512
#define TOTAL_SIZE_OF_PICTURES 1536
#define TOTAL_SIZE_OF_BITMAP_WIDTH_PREMULT 167 * 2

#define ALLOCATION_ARRAY_START 7971
#define LOGDIR_START 186
#define PICDIR_START 1462
#define SOUNDDIR_START 2738
#define VIEWDIR_START 4014
#define LOGIC_ENTRY_START 1793
#define LOGIC_FILE_START 3834
#define MENU_START 0
#define MENU_CHILD_START 181
#define VIEWTAB_START 3782
#define LOADED_VIEW_START 4623
#define MEMORY_AREA_START 8156
#define LOGIC_ENTRY_ADDRESSES_START 7679
#define BITMAP_WIDTH_PREMULT_START ((0xBFFF - 0xA000 - 1) - TOTAL_SIZE_OF_BITMAP_WIDTH_PREMULT)

#define DIR_SIZE 6
#define LOGIC_ENTRY_SIZE 8
#define LOGIC_FILE_SIZE 9
#define MEMORY_AREA_SIZE 7
#define PICTURE_SIZE 6

#define DIRECTORY_BANK 60
#define LOGIC_BANK 0x3E
#define MENU_BANK 61
#define LOGIC_ENTRY_ADDRESSES_BANK 8
#define DEBUG_BANK 5
#define HELPERS_BANK 5
#define INSTRUCTION_HANDLER_BANK 5
#define IF_LOGIC_HANDLERS_BANK 5
#define VIEWTAB_BANK 61
#define LOADED_VIEW_BANK 61
#define PICTURE_BANK 0x3E
#define IRQ_BANK 6
#define GRAPHICS_BANK 6


#define FIRST_FLOOD_BANK 0x29
#define NO_FLOOD_BANKS 7

#define FLOODBANKFILENAME "agi.cx16.flood"

#define NO_CODE_BANKS 17

#define LRU_CACHE_LOGIC_STRUCT_START 8183
#define LRU_CACHE_LOGIC_DATA_START 8173
#define LRU_CACHE_VIEW_STRUCT_START 8165
#define LRU_CACHE_VIEW_DATA_START 8145
#define LRU_CACHE_LOGIC_DATA_SIZE 10
#define LRU_CACHE_LOGIC_VIEW_SIZE 20
#define PICTURE_START 0

#define FLOOD_QUEUE_START 0xA7D0

//Code Banks
#define LOAD_DIRS_BANK 0x6
#define FILE_LOADER_HELPERS 0x6
#define MEKA_BANK 0x6
#define LOGIC_CODE_BANK 0x6
#define VIEW_CODE_BANK_1 0x9
#define VIEW_CODE_BANK_2 0xA
#define VIEW_CODE_BANK_3 0xB
#define VIEW_CODE_BANK_4 0xC
#define VIEW_CODE_BANK_5 0xD
#define LRU_CACHE_LOGIC_BANK 0xE
#define MEMORY_MANAGEMENT_BANK 0x10
#define PICTURE_CODE_BANK 0x11

//Golden RAM
#define VARS_AREA_START 0
#define VARS_AREA_SIZE 256

#define FLAGS_AREA_START 257
#define FLAGS_AREA_SIZE 256

#define LOCAL_WORK_AREA_START 514
#define LOCAL_WORK_AREA_SIZE 500
#define PARAMETERS_START 1015   

//CX16 Constants
#define VERA_addr_bank 0x9F22
#define VERA_addr_high 0x9F21
#define VERA_addr_low 0x9F20
#define VERA_data0 0x9F23

//Zero Page Values 
#define ZP_PTR_TEMP 0x66
#define ZP_PTR_B1 0xFA //For Flood Queue Queue
#define ZP_PTR_B2 0xFC //For Flood Queue Serve

#ifdef _MSC_VER //Used for testing under windows
extern byte* banked;
#endif 

#define GOLDEN_RAM        ((unsigned char *)0x0400)
#define GOLDEN_RAM_WORK_AREA        ((unsigned char *)0x0400 + LOCAL_WORK_AREA_START)
#define GOLDEN_RAM_PARAMS_AREA &GOLDEN_RAM[PARAMETERS_START]

extern int _noSegments;

#ifndef _MSC_VER
extern void _BANKRAM01_SIZE__[], _BANKRAM02_SIZE__[], _BANKRAM03_SIZE__[], _BANKRAM04_SIZE__[], _BANKRAM05_SIZE__[], _BANKRAM06_SIZE__[], _BANKRAM07_SIZE__[], _BANKRAM08_SIZE__[], _BANKRAM09_SIZE__[], _BANKRAM0A_SIZE__[], _BANKRAM0B_SIZE__[], _BANKRAM0C_SIZE__[], _BANKRAM0D_SIZE__[], _BANKRAM0E_SIZE__[], _BANKRAM0F_SIZE__[], _BANKRAM10_SIZE__[], _BANKRAM11_SIZE__[], _BANKRAMFLOOD_SIZE__[];
#endif // !_MSC_VER

typedef struct {          /* DIR entry structure */
	byte firstBank;
	byte noBanks;
	int segmentSize;
	byte noSegments;
	byte* start;
} MemoryArea;

void b10InitDynamicMemory();

void memoryMangerInit();
byte* banked_allocTrampoline(int size, byte* bank);
boolean banked_deallocTrampoline(byte* ptr, byte bank);

#endif

