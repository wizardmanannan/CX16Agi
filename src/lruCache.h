#ifndef LRU_CACHE_H
#define LRU_CACHE_H
#include "general.h"
#include "memoryManager.h"
#include "agifiles.h"
#include "helpers.h"
#include <cx16.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
extern void bEInitLruCaches(CacheEvictionCallback evictionCallbackLogic, CacheEvictionCallback evictionCallbackView);

#endif