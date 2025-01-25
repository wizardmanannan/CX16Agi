#include "lruCache.h"

#pragma bss-name (push, "BANKRAM04")
LRUCache _logicCache;
LRUCache _viewCache;
byte logicKeys[MAX_CACHE_SIZE];
#pragma bss-name (pop)

#ifdef _MSC_VER
byte _bankedRam[8196];
#define BANK_RAM _bankedRam
#endif

#pragma code-name (push, "BANKRAM04")

void b4NewLruCache(byte max_size, byte* keys, LRUCache* cache, CacheEvictionCallback evictionCallback, byte evictionCallbackBank) {
    int i;
    cache->size = 0;
    cache->max_size = max_size;
    cache->keys = keys;
    cache->evictionCallback = evictionCallback;
    cache->evictionCallbackBank = evictionCallbackBank;

    for (i = 0; i < max_size; i++) {
        cache->keys[i] = 0;
    }
}

void b4InitLruCaches(CacheEvictionCallback evictionCallbackLogic, CacheEvictionCallback evictionCallbackView)
{
    b4NewLruCache(LRU_CACHE_LOGIC_DATA_SIZE, &logicKeys[0], &_logicCache, evictionCallbackLogic, LOGIC_CODE_BANK);
}

void b4LruCacheGet(int resType, byte key, AGIFilePosType* location, AGIFile* agiData)
{
    int i, j;
    byte tmp;
    LRUCache* lruCache = NULL;
    byte previousRamBank = RAM_BANK;

    RAM_BANK = LRU_CACHE_LOGIC_BANK;

    if (resType == LOGIC)
    {
        lruCache = &_logicCache;
    }

    for (i = 0; i < lruCache->size; i++) {
        if (lruCache->keys[i] == key) {
            // move this entry to the front
            tmp = lruCache->keys[i];
            for (j = i; j > 0; j--) {
                lruCache->keys[j] = lruCache->keys[j - 1];
            }
            lruCache->keys[0] = tmp;
            return;
        }
    }
    // insert new entry at front
    if (lruCache->size < lruCache->max_size) {
        lruCache->size++;
    }
    else {
        // cache is full, delete least recently used entry
        if (lruCache->evictionCallback) {
            lruCache->evictionCallback(lruCache->keys[lruCache->size - 1]);
        }
    }
    for (i = lruCache->size - 1; i > 0; i--) {
        lruCache->keys[i] = lruCache->keys[i - 1];
    }
    lruCache->keys[0] = key;

#ifdef __CX16__
    b6LoadAGIFile(resType, location, agiData);
#endif // __CX16__

#ifdef  __CX16__
    RAM_BANK = previousRamBank;
#endif
}

#pragma code-name (pop)