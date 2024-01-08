#ifndef LRU_CACHE_H
#define LRU_CACHE_H
#include "general.h"
#include "memoryManager.h"
#include "agifiles.h"

#ifdef  __CX16__
#include <cx16.h>
#include "helpers.h"
#endif

#define MAX_CACHE_SIZE 10

typedef void (*CacheEvictionCallback)(byte key);

typedef struct LRUCache {
	int size;
	unsigned char max_size;
	byte* keys;
	CacheEvictionCallback evictionCallback;
	byte evictionCallbackBank;
} LRUCache;

extern LRUCache _logicCache;
extern LRUCache _viewCache;

#pragma wrapped-call (push, trampoline, LRU_CACHE_LOGIC_BANK)
extern void b4LruCacheGet(int resType, byte key, AGIFilePosType* location, AGIFile* agiData);
extern void b4InitLruCaches(CacheEvictionCallback evictionCallbackLogic, CacheEvictionCallback evictionCallbackView);
#pragma wrapped-call (pop)

#ifdef _MSC_VER
extern void loadAGIFileTest(int resType, AGIFilePosType* location, AGIFile* AGIData);
#endif

#endif