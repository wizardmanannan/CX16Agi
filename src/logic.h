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

extern LOGICEntry logics[MAX_RESOURCE_NUMBER];
void b6InitLogics();

#pragma wrapped-call (push, trampoline, LOGIC_CODE_BANK)
void b6LoadLogicFile(byte logFileNum);
void b6DiscardLogicFile(byte logFileNum);
#pragma wrapped-call (pop)

extern void getLogicFile(LOGICFile* logicFile, byte logicFileNo);
extern void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
extern void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);

#endif  /* _LOGIC_H_ */