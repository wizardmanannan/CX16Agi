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
#define MEDIUM_NO_SEGMENTS 12
#define LARGE_NO_SEGMENTS 6

//Warning: All of these sizes must be multiples of 8000, otherwise the program will crash
#define TINY_SIZE  100
#define EXTRA_SMALL_SIZE 160
#define SMALL_SIZE 1600
#define MEDIUM_SIZE 4000 
#define LARGE_SIZE 8000
#define MEMORY_MANAGER_BANK_SIZE 8000

#define TINY_NO_BANKS  1
#define EXTRA_SMALL_NO_BANKS 1
#define SMALL_NO_BANKS 6
#define MEDIUM_NO_BANKS 6
#define LARGE_NO_BANKS 6

#define TINY_FIRST_BANK  0x13
#define EXTRA_SMALL_FIRST_BANK 0x14
#define SMALL_FIRST_BANK 0x15
#define MEDIUM_FIRST_BANK 0x1B
#define LARGE_FIRST_BANK 0x21

#define ALLOCATION_BANK 60

#define NO_SIZES 5

#define BANK_SIZE (0xBFFF-0xA000 + 1)
#define BANK_MIN 0xA000
#define BANK_MAX 0xBFFF

#define TOTAL_SIZE_OF_DIR 1275
#define TOTAL_SIZE_OF_LOGIC_ENTRY 2040
#define TOTAL_SIZE_OF_LOGIC_DATA 7331
#define TOTAL_SIZE_OF_VIEWTAB_DATA 840
#define TOTAL_SIZE_OF_LOADED_VIEWS 1536
#define TOTAL_SIZE_OF_MEMORY_AREA 35
#define TOTAL_SIZE_LOGIC_ENTRY_ADDRESSES 512
#define TOTAL_SIZE_OF_PICTURES 1536
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
#define NO_VIEWS 256
#define MEMORY_AREA_START 8156
#define LOGIC_ENTRY_ADDRESSES_START 7679
#define DIR_SIZE 6
#define LOGIC_ENTRY_SIZE 8
#define LOGIC_FILE_SIZE 9
#define MEMORY_AREA_SIZE 7
#define PICTURE_SIZE 6

//RAM BANKS
#define DIRECTORY_BANK 60
#define LOGIC_BANK 0x3E
#define MENU_BANK 61
#define LOGIC_ENTRY_ADDRESSES_BANK 8
#define DEBUG_INIT_BANK 5
#define HELPERS_BANK 5
#define INSTRUCTION_HANDLER_BANK 5
#define IF_LOGIC_HANDLERS_BANK 5
#define VIEWTAB_BANK 9
#define LOADED_VIEW_BANK 0x11
#define PICTURE_BANK 0x3E
#define VIEW_DRAW_BUFFER_BANK 9
#define STRING_BANK 0x7
#define PARSER_BANK 0x7
#define WORD_BANK 0x12
#define LINE_DRAW_BANK 8
#define FILL_BANK 8
#define SPRITE_GARBAGE_BANK 0xA

#define FIRST_FLOOD_BANK 0x27
#define NO_FLOOD_BANKS 0x0A
#define LAST_FLOOD_BANK (FIRST_FLOOD_BANK + NO_FLOOD_BANKS - 1)

#define FIRST_DIVISION_BANK 0x31
#define NO_DIVISION_BANKS 0x0B
#define LAST_DIVISION_BANK (FIRST_DIVISION_BANK + NO_DIVISION_BANKS - 1)
#define DIVISION_AREA        ((byte *)0xA000)
#define divBankMetadata ((byte *)0xA000 + 7470)
#define divAddressMetadata ((byte *)0xA000 + 7637)
#define DIV_METADATA_BANK 0x31
#define DIV_BANK_METADATA_SIZE 167
#define DIV_ADDRESS_METADATA_SIZE 334

#define NO_CODE_BANKS 18

#define LRU_CACHE_LOGIC_DATA_SIZE 10
#define LRU_CACHE_LOGIC_VIEW_SIZE 20
#define PICTURE_START 0

#define FLOOD_QUEUE_START 0xA850

//Code Banks
#define TEXT_CODE_BANK 3
#define LOADING_SCREEN_CODE_BANK 6
#define LOAD_DIRS_BANK 0x6
#define FILE_LOADER_HELPERS 0x6
#define MEKA_BANK 0x6
#define LOGIC_CODE_BANK 0x6
#define VIEW_CODE_BANK_1 0x9
#define VIEW_CODE_BANK_2 0xA
#define VIEW_CODE_BANK_3 0xB
#define VIEW_CODE_BANK_4 0xC
#define VIEW_CODE_BANK_5 0xD
#define LRU_CACHE_LOGIC_BANK 0x4
#define SPRITE_METADATA_BANK 0xE
#define SPRITE_MEMORY_MANAGER_BANK 0xE
#define SPRITE_UPDATED_BANK 0xE
#define MEMORY_MANAGEMENT_BANK 0x10
#define PICTURE_CODE_BANK 0x11
#define PICTURE_CODE_OVERFLOW_BANK 0x4
#define IRQ_BANK 6
#define GRAPHICS_BANK 6
#define FLOAT_BANK 0x1
#define PALETTE_MANAGER_BANK 0xF
#define OBJECT_BANK 0xF
#define RANDOM_BANK 0x6
#define SOUND_BANK 0x1

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
#define VERA_ctrl 0x9F25
#define VERA_data0 0x9F23
#define VERA_data1 0x9F24
#define VERA_DCVIDEO 0x9F29


//Char Set 
#define CHAR_SET_ROM ((byte *)0xC800)
#define CHARSET_ROM 6
#define SIZE_PER_CHAR_CHAR_SET_ROM 8


//Zero Page Values 
#define ZP_PTR_TMP_2 0xAD
#define ZP_PTR_TMP_3 0xAF
#define ZP_PTR_TMP_4 0xB1
#define ZP_PTR_TMP_5 0xB3

#define ZP_PTR_TMP_20 0xDD
#define ZP_PTR_TMP_21 0xDF
#define ZP_PTR_TMP_22 0xE1
#define ZP_PTR_TMP_23 0xE3
#define ZP_PTR_TMP_24 0xE5
#define  ZP_PTR_TMP_25 0xCD
#define ZP_PTR_TMP_26 0xCF
#define ZP_PTR_TMP_27 0xD1
#define ZP_PTR_TMP_28 0xD3
#define ZP_PTR_TMP_29 = 0xD5
#define ZP_PTR_TMP_30 = 0xD7
#define ZP_PTR_TMP_31 = 0xD9

//Sprite Allocation
#define ZP_PTR_SEG_32 0xF7
#define ZP_PTR_SEG_64 0xF8
#define  ZP_PTR_HIGH_BYTE_START 0xF9
#define ZP_PTR_WALL_32 0xFA
#define ZP_PTR_WALL_64 0xFB

//Float Division Zero Page
#define ZP_DIV_AREA 0xE3
#define ZP_DIV_BANK 0xE5
#define ZP_DIV_ADDR 0xFC


#define NO_ZERO_PAGE_ENTRIES 87
#define FIRST_ZERO_PAGE_ENTRY 0xA9

//Vera
#define TILEBASE 0xD000
#define MAPBASE 0xDA00
#define BITMAP_START 0x0
#define BITMAP_END 0x95FF
#define SPRITES_DATA_START 0xEA00
#define SPRITES_DATA_END 0x1F9BE
#define SPRITE_ATTRIBUTES_START 0x1FC00
#define VERA_END 0x1FFFF
#define SPRITES_PIXELS_PER_BYTE 0x2
#define PALETTE_START 0x1FA00
#define PRIORITY_START (((unsigned long)BITMAP_WIDTH * BITMAP_HEIGHT) / 2)
#define PRIORITY_SIZE (((unsigned long) PICTURE_WIDTH * PICTURE_HEIGHT) / 2)
#define SIZE_OF_SPRITE_ATTRIBUTE 8

#ifdef _MSC_VER //Used for testing under windows
extern byte* banked;
#endif 

#define GOLDEN_RAM        ((unsigned char *)0x0400)
#define GOLDEN_RAM_WORK_AREA        ((unsigned char *)0x0400 + LOCAL_WORK_AREA_START)
#define GOLDEN_RAM_PARAMS_AREA &GOLDEN_RAM[PARAMETERS_START]

extern int _noSegments;

#ifndef _MSC_VER
extern void _BANKRAM01_SIZE__[], _BANKRAM02_SIZE__[], _BANKRAM03_SIZE__[], _BANKRAM04_SIZE__[], _BANKRAM05_SIZE__[], _BANKRAM06_SIZE__[], _BANKRAM07_SIZE__[], _BANKRAM08_SIZE__[], _BANKRAM09_SIZE__[], _BANKRAM0A_SIZE__[], _BANKRAM0B_SIZE__[], _BANKRAM0C_SIZE__[], _BANKRAM0D_SIZE__[], _BANKRAM0E_SIZE__[], _BANKRAM0F_SIZE__[], _BANKRAM10_SIZE__[], _BANKRAM11_SIZE__[], _BANKRAM12_SIZE__[], _BANKRAMFLOOD_SIZE__[];
#endif // !_MSC_VER

typedef struct {          /* DIR entry structure */
	byte firstBank;
	byte noBanks;
	int segmentSize;
	byte noSegments;
	byte* start;
} MemoryArea;

void b10InitDynamicMemory();
void b10InitZeroPage();

void memoryMangerInit();

extern void trampoline(); //DOTO: Really should come from helpers but there would be a circular dependency if I included it. Split helpers up to make this possible

#pragma wrapped-call (push, trampoline, MEMORY_MANAGEMENT_BANK)
byte* b10BankedAlloc(int size, byte* bank);
boolean b10BankedDealloc(byte* ptr, byte bank);
#pragma wrapped-call (pop)

#endif

