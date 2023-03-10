#ifndef _COMMANDS_H_
#define _COMMANDS_H_
#include "helpers.h"
#include "memoryManager.h"
#include "lruCache.h"
#include <string.h>
#include "logic.h"

void executeLogic(int logNum);
void b1Call(unsigned char** data);

extern boolean _b1Greatern();

extern void commandLoop(LOGICFile* currentLogicFile);
extern byte loadAndIncWinCode();
extern void afterLogicCommand();
extern void initCommands();
extern void incCodeBy(int jumpAmount);

extern byte codeBank;
#endif