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


void bELruCacheGet(int resType, byte key, AGIFilePosType* location, AGIFile* agiData)
{
    int i, j;
    byte tmp;
    LRUCache* lruCache = NULL;
    byte previousRamBank = RAM_BANK;

    RAM_BANK = LRU_CACHE_LOGIC_BANK;

    if (resType == LOGIC)
    {
        lruCache = _logicCache;
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

void initLruCachesTrampoline(CacheEvictionCallback evictionCallbackLogic, CacheEvictionCallback evictionCallbackView)
{
    byte previousRamBank = RAM_BANK;
    
    RAM_BANK = LRU_CACHE_LOGIC_BANK;

    bEInitLruCaches(evictionCallbackLogic, evictionCallbackView);
   

    RAM_BANK = previousRamBank;
}