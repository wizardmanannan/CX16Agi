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

typedef struct BufferStatus //Note: Also used in viewAsm.s b9ViewToVera. Don't change without adjusting this
{
    byte* bankedData;
    byte bank;
    byte bufferCounter;
} BufferStatus;


byte convertAsciiByteToPetsciiByte(byte toConvert);

extern char* strcpyBanked(char* dest, const char* src, byte bank);
extern size_t strLenBanked(char* string, int bank);

extern void* memCpyBanked(byte* dest, byte* src, byte bank, size_t len);
extern void memCpyBankedBetween(byte* dest, byte bankDst, byte* src, byte bankSrc, size_t len);
extern void memsetBanked(void* _Dst, int _Val, size_t _Size, byte bank);

#define COPY_EVERYTHING 32767
extern void copyStringFromBanked(char* src, char* dest, int start, int chunk, byte sourceBank, boolean convertFromAsciiByteToPetscii);

extern int sprintfBanked(const char* buffer, byte bank, char const* const format, ...);

extern void getLogicDirectory(AGIFilePosType* returnedLogicDirectory, AGIFilePosType* logicDirectoryLocation);
extern void setResourceDirectory(AGIFilePosType* newLogicDirectory, AGIFilePosType* logicDirectoryLocation);

extern void debugPrint(byte toPrint);
extern void trampoline();

#pragma wrapped-call (push, trampoline, HELPERS_BANK)
void b5RefreshBuffer(BufferStatus* bufferStatus);
#pragma wrapped-call (pop)


extern long opStopAt;
extern long opExitAt;
extern long opCounter;

extern byte _assm;
extern long _assmLong;

extern boolean enableHelpersDebugging;

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


#define READ_BYTE_VAR_FROM_ASSM(byteVar, address) \
    do {                                           \
        asm("lda %w", address); \
        asm("sta %v", _assm); \
        byteVar = _assm; \
    } while(0) \

#define WRITE_BYTE_VAR_TO_ASSM(byteVar, address) \
    do {                                           \
        _assm = byteVar;  \
        asm("lda %v", _assm);                  \
        asm("sta %w", address);                  \
    } while(0)

//For writing things defined with a #define
#define WRITE_BYTE_DEF_TO_ASSM(byteDef, address) \
    do {                                           \
        asm("lda #%w", byteDef);                  \
        asm("sta %w", address);                  \
    } while(0)

#define GET_NEXT(storeLocation)  \
    do {                              \
        if(*data >= GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE) \
		{ \
			b5RefreshBuffer(bufferStatus); \
			*data = GOLDEN_RAM_WORK_AREA; \
		} \
		 storeLocation = *((*data)++); \
		\
    } while(0);


#define READ_STACK_FROM_ASSM(byteVar) \
     do {                                           \
        asm("pla"); \
        asm("sta %v", _assm); \
        byteVar = _assm; \
    } while(0) \


#endif
