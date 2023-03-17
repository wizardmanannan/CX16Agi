#ifndef LRU_CACHE_H
#define LRU_CACHE_H
#include "general.h"
#include "memoryManager.h"
#include "agifiles.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

extern LRUCache* _logicCache;
extern LRUCache* _viewCache;

extern void lruCacheGetTrampoline(int resType, byte key, AGIFilePosType* location, AGIFile* agiData);
extern void initLruCachesTrampoline(CacheEvictionCallback evictionCallbackLogic, CacheEvictionCallback evictionCallbackView);

#ifdef _MSC_VER
extern void loadAGIFileTest(int resType, AGIFilePosType* location, AGIFile* AGIData);
#endif

#endif