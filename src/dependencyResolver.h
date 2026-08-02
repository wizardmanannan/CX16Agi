
#ifndef DEPENDENCYRESOLVER_H
#define DEPENDENCYRESOLVER_H

#include "memoryManager.h"
#include <cbm.h>
//#pragma wrapped-call (push, trampoline, DEPENDENCY_RESOLVER_BANK)

//typedef void (*loadFn)(byte resourceNum);
//typedef void (*isLoadedFn)(byte resourceNum);

//void b4ResolveDependency(byte fileType, loadFn loadFn, isLoadedFn isLoadedFn);

void b4InitMetadata();
void b4LoadDependencies(byte scriptNumber);

//#pragma wrapped-call (pop)

#endif