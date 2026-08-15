
#ifndef DEPENDENCYRESOLVER_H
#define DEPENDENCYRESOLVER_H

#include "memoryManager.h"
#include <cbm.h>
#include "logic.h"
#include "view.h"
#pragma wrapped-call (push, trampoline, DEPENDENCY_RESOLVER_BANK)

typedef enum {
	DEPENDENCY_LOGIC = 1,
    DEPENDENCY_VIEW = 2,
    DEPENDENCY_SOUND = 3
} DEPENDENCY_TYPE;

void b4InitMetadata();
void b4LoadUnloadDependencies(byte scriptNumber, boolean shouldLoad, boolean forceLoadSubDependencies, DEPENDENCY_TYPE dependencyType);
#pragma wrapped-call (pop)

#endif