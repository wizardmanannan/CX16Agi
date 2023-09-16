#ifndef _HELPERS_H_
#define _HELPERS_H_
#include "general.h"
#include "memoryManager.h"
#include <stdarg.h>
#ifdef  __CX16__
#include <cx16.h>
#endif
#include <string.h>

#define ASCIIDASH 95
#define ASCIIA 65
#define ASCIIZ 90
#define ASCIIa 97
#define ASCIIz 122

#define PETSCIIA 193
#define PETSCIIZ 218
#define PETSCIIa 65
#define PETSCIIz 90
#define PETSCIISpace 32
#define	PETSCIIPercent 37
#define PETSCIIDash 228

#define DIFF_ASCII_PETSCII_CAPS -128
#define DIFF_ASCII_PETSCII_LOWER -32

typedef void (*fnTrampoline_0)();

typedef void (*fnTrampoline_1BytePointerPointer)(byte** data);
typedef boolean (*fnTrampoline_1BytePointerPointerRetBool)(byte** data);
typedef void (*fnTrampoline_1Int)(int data);
typedef byte (*fnTrampoline_1ByteRByte)(byte data);
typedef void (*fnTrampoline_1Int)(int data);

typedef void (*fnTrampoline_2Int)(int data, int data2);
typedef void (*fnTrampoline_2Byte)(byte data, byte data2);

typedef void (*fnTrampoline_3Int)(int data1, int data2, int data3);

byte convertAsciiByteToPetsciiByte(byte toConvert);

extern void trampoline_0(fnTrampoline_0 func, byte bank);

extern void trampoline_1Int(fnTrampoline_1Int func, int data, byte bank);
extern void trampoline_1Int(fnTrampoline_1Int func, int data, byte bank);

extern void trampoline_2Int(fnTrampoline_2Int func, int data1, int data2, int bank);
extern void trampoline_3Int(fnTrampoline_3Int func, int data1, int data2, int data3, int bank);

extern byte trampoline_1ByteRByte(fnTrampoline_1ByteRByte func, byte data, byte bank);
extern void trampoline_2Byte(fnTrampoline_2Byte func, byte data1, byte data2, byte bank);

extern char* strcpyBanked(char* dest, const char* src, byte bank);

extern void* memCpyBanked(byte* dest, byte* src, byte bank, size_t len);

#define COPY_EVERYTHING 32767
extern void copyStringFromBanked(char* src, char* dest, int start, int chunk, byte sourceBank, boolean convertFromAsciiByteToPetscii);

extern int sprintfBanked(const char* buffer, byte bank, char const* const format, ...);

extern void getLogicDirectory(AGIFilePosType* returnedLogicDirectory, AGIFilePosType* logicDirectoryLocation);
extern void setResourceDirectory(AGIFilePosType* newLogicDirectory, AGIFilePosType* logicDirectoryLocation);

extern void debugPrint(byte toPrint);

extern long opStopAt;
extern long opExitAt;
extern long opCounter;


#define abs_val(a) ((a) < 0 ? -(a) : (a))

extern byte _previousRomBank;

//Used in case of code with ROM Bank Switched
#define PRINTF_ROM_BANK = 0;
#define PRINTF(format, ...) do {  \
    _previousRomBank = ROM_BANK; \
    ROM_BANK = 0; \
    printf(format, ##__VA_ARGS__); \
    ROM_BANK = _previousRomBank; \
} while (0)

#endif
