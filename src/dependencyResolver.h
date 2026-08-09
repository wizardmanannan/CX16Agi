
#ifndef DEPENDENCYRESOLVER_H
#define DEPENDENCYRESOLVER_H

#include "memoryManager.h"
#include <cbm.h>
#include "logic.h"
#pragma wrapped-call (push, trampoline, DEPENDENCY_RESOLVER_BANK)
void b4InitMetadata();
void b4LoadUnloadDependencies(byte scriptNumber, boolean shouldLoad, boolean forceLoadSubDependencies);
#pragma wrapped-call (pop)

#endif