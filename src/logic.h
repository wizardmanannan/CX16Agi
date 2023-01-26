/**************************************************************************
** LOGIC.H
**************************************************************************/

#ifndef _LOGIC_H_
#define _LOGIC_H_

#include <stdlib.h>
#include "helpers.h"
#include "lruCache.h"

typedef struct {
	word codeSize;
	byte* logicCode;
	byte numMessages;
	byte** messages;
	byte codeBank;
	byte messageBank;

} LOGICFile;

typedef struct { 
	boolean loaded;
	word entryPoint;
	word currentPoint;
	LOGICFile* data;
	byte dataBank;
} LOGICEntry;

extern LOGICEntry* logics;

void b8InitLogics();
void b8LoadLogicFile(byte logFileNum);
void b8DiscardLogicFile(byte logFileNum);

extern void getLogicFile(LOGICFile* logicFile, byte logicFileNo);
extern void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
extern void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);


#endif  /* _LOGIC_H_ */
