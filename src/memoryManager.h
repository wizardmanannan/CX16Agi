#ifndef _MEMORYMANAGER_H_
#define _MEMORYMANAGER_H_

#ifndef _MSC_VER
#include <stdio.h>
#include <cx16.h>
#include <string.h>
#endif

#ifndef _MSC_VER
#include <cx16.h>
#endif // !

#define NO_FLOOD_BANKS 0x0A
#define FIRST_FLOOD_BANK 0x27
#define NO_CODE_BANKS 17
#define BANK_SIZE (0xBFFF-0xA000 + 1)
#define FLOODBANKFILENAME "agi.cx16.flood"
#ifndef _MSC_VER
extern void _BANKRAM01_SIZE__[], _BANKRAM02_SIZE__[], _BANKRAM03_SIZE__[], _BANKRAM04_SIZE__[], _BANKRAM05_SIZE__[], _BANKRAM06_SIZE__[], _BANKRAM07_SIZE__[], _BANKRAM08_SIZE__[], _BANKRAM09_SIZE__[], _BANKRAM0A_SIZE__[], _BANKRAM0B_SIZE__[], _BANKRAM0C_SIZE__[], _BANKRAM0D_SIZE__[], _BANKRAM0E_SIZE__[], _BANKRAM0F_SIZE__[], _BANKRAM10_SIZE__[], _BANKRAM11_SIZE__[], _BANKRAMFLOOD_SIZE__[];
#endif // !_MSC_VER

void memoryMangerInit();

#endif

