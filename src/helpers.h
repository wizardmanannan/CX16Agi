#ifndef _HELPERS_H_
#define _HELPERS_H_
#include "general.h"
#include "memoryManager.h"
#include <stdarg.h>
#include "kernal.h"
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

extern int strcmpIgnoreSpace(const char* str1, const char* str2);
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
void b5WaitOnKey();
void b5WaitOnSpecificKeys(byte* keys, byte length);
void b5RefreshBufferNonGolden(BufferStatus* bufferStatus, byte* buffer, int bufferSize);
#pragma wrapped-call (pop)
extern void trampolineDebug(void (*trampolineDebug)()); //Only to be called when debugging is enabled (b5IsDebuggingEnabled), otherwise a crash is likely.


extern long opStopAt;
extern long opExitAt;
extern long opCounter;

extern byte _assmByte;
extern byte _assmByte2;
extern byte _assmByte3;
extern long _assmLong;
extern unsigned int _assmUInt;

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
        asm("sta %v", _assmByte); \
        byteVar = _assmByte; \
    } while(0) \

#define WRITE_BYTE_VAR_TO_ASSM(byteVar, address) \
    do {                                           \
        _assmByte = byteVar;  \
        asm("lda %v", _assmByte);                  \
        asm("sta %w", address);                  \
    } while(0)

#define WRITE_INT_VAR_TO_ASSM(intVar, address) \
    do {                                           \
        _assmUInt = intVar;  \
        asm("lda %v", _assmUInt);                  \
        asm("sta %w", address);                  \
        asm("lda %v + 1", _assmUInt);                  \
        asm("sta %w + 1", address);                  \
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
        asm("sta %v", _assmByte); \
        byteVar = _assmByte; \
    } while(0) \


#endif
