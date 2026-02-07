/**************************************************************************
** LOGIC.H
**************************************************************************/

#ifndef _LOGIC_H_
#define _LOGIC_H_

#include "helpers.h"
#include "lruCache.h"

typedef struct {
	word codeSize; //0
	byte* logicCode; //2
	byte numMessages; //4
	byte** messages; // 5
	byte codeBank; //7
	byte messageBank; //8

} LOGICFile;

typedef struct {
	boolean loaded; //0
	word entryPoint; //1
	word currentPoint; //3
	LOGICFile* data; //5
	byte dataBank; //7
} LOGICEntry;

//extern LOGICEntry logics;
void b6InitLogics();

#pragma wrapped-call (push, trampoline, LOGIC_CODE_BANK)
void b6LoadLogicFile(byte logFileNum);
void b6DiscardLogicFile(byte logFileNum);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, LOGIC_BANK)
extern void b5GetLogicFile(LOGICFile* logicFile, byte logicFileNo);
extern void b5SetLogicFile(LOGICFile* logicFile, byte logicFileNo);
extern void b5GetLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
extern void b5SetLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
#pragma wrapped-call (pop)

#endif  /* _LOGIC_H_ */