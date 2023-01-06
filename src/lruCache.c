#include "lruCache.h"

LRUCache* _logicCache;
LRUCache* _viewCache;

#ifdef _MSC_VER
byte _bankedRam[8196];
#define BANK_RAM _bankedRam
#endif

#pragma code-name (push, "BANKRAM0E")

void bENewLruCache(byte max_size, byte* keys, LRUCache* cache, CacheEvictionCallback evictionCallback, byte evictionCallbackBank) {
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

void bEInitLruCaches(CacheEvictionCallback evictionCallbackLogic, CacheEvictionCallback evictionCallbackView)
{
    _logicCache = (LRUCache*)&BANK_RAM[LRU_CACHE_LOGIC_STRUCT_START];
    bENewLruCache(LRU_CACHE_LOGIC_DATA_SIZE, &BANK_RAM[LRU_CACHE_VIEW_STRUCT_START], (LRUCache*)&BANK_RAM[LRU_CACHE_LOGIC_STRUCT_START], evictionCallbackLogic, LOGIC_CODE_BANK);
}


void bELruCacheGet(int resType, LRUCache* cache, byte key, AGIFilePosType* location, AGIFile* AGIData) {
    int i, j;
    byte tmp;
    for (i = 0; i < cache->size; i++) {
        if (cache->keys[i] == key) {
            // move this entry to the front
            tmp = cache->keys[i];
            for (j = i; j > 0; j--) {
                cache->keys[j] = cache->keys[j - 1];
            }
            cache->keys[0] = tmp;
            return;
        }
    }
    // insert new entry at front
    if (cache->size < cache->max_size) {
        cache->size++;
    }
    else {
        // cache is full, delete least recently used entry
        if (cache->evictionCallback) {
#ifdef  __CX16__
            trampoline_1Int(&cache->evictionCallback, cache->keys[cache->size - 1], cache->evictionCallbackBank);
#endif
#ifdef _MSC_VER
            cache->evictionCallback(cache->keys[cache->size - 1]);
#endif // _MSC_VER
        }
    }
    for (i = cache->size - 1; i > 0; i--) {
        cache->keys[i] = cache->keys[i - 1];
    }
    cache->keys[0] = key;
#ifdef _MSC_VER
    loadAGIFileTest(resType, location, AGIData);
#endif // __CX16__

#ifdef __CX16__
    loadAGIFile(resType, location, AGIData);
#endif // __CX16__

}


#pragma code-name (pop)

void lruCacheGetTrampoline(int resType, byte key, AGIFilePosType* location, AGIFile* agiData)
{
#ifdef  __CX16__
    byte previousRamBank = RAM_BANK;
#endif
    LRUCache* lruCache = NULL;

#ifdef  __CX16__
    RAM_BANK = 0xE;
#endif

    if (resType == LOGIC)
    {
        lruCache = _logicCache;
    }

    bELruCacheGet(resType, lruCache, key, location, agiData);

#ifdef  __CX16__
    RAM_BANK = previousRamBank;
#endif
}