/**************************************************************************
** LOGIC.H
**************************************************************************/

#ifndef _LOGIC_H_
#define _LOGIC_H_

#include "helpers.h"
#include "logicTypes.h"
#include "dependencyResolver.h"

//extern LOGICEntry logics;
void b6InitLogics();

#pragma wrapped-call (push, trampoline, LOGIC_CODE_BANK)
void b6LoadLogicFile(byte logFileNum, boolean forceLoadSubDependencies);
void b6DiscardLogicFile(byte logFileNum);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, trampoline, LOGIC_BANK)
extern void b5GetLogicFile(LOGICFile* logicFile, byte logicFileNo);
extern void b5SetLogicFile(LOGICFile* logicFile, byte logicFileNo);
extern void b5GetLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
extern void b5SetLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
#pragma wrapped-call (pop)

#endif  /* _LOGIC_H_ */
