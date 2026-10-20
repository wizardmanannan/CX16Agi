#ifndef _LOGIC_TYPES_H_
#define _LOGIC_TYPES_H_

#include "general.h"

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
	boolean isLogicZeroOrDependency;
} LOGICEntry;

#endif
