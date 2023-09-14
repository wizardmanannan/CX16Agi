#ifndef _COMMANDS_H_
#define _COMMANDS_H_
#include "helpers.h"
#include "memoryManager.h"
#include "lruCache.h"
#include "logic.h"
#include "debugHelper.h"
#include "picture.h"

void executeLogic(LOGICEntry* logicEntry, int logNum);
void b1Call(unsigned char** data);

extern boolean _b1Greatern();

extern void commandLoop(byte logNum);
extern byte loadAndIncWinCode();
extern void afterLogicCommand();
extern void initCommands();
extern void incCodeBy(int jumpAmount);

extern byte codeBank;
#endif