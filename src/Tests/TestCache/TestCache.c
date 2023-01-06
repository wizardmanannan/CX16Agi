#include "../../lruCache.h"
#include <assert.h>

AGIFile testfile;
AGIFilePosType testLocation;

byte _evicted;
boolean _evictedCalled;

void cacheEvictionCallbackLogic(int key)
{
	_evicted == key;
	_evictedCalled = true;
}

void cacheEvictionCallbackView(int key)
{

}

void init()
{
	bEInitLruCaches(&cacheEvictionCallbackLogic, &cacheEvictionCallbackView);
}

void testCanAdd(LRUCache* cache)
{
	const byte TestVal = 5;
	lruCacheGetTrampoline(LOGIC, TestVal, &testLocation, &testfile);

	assert(_logicCache->keys[0] == 5);
	assert(_logicCache->size == 1);
}

void testCanFill(LRUCache* cache)
{

	for (int i = cache->max_size - 1; i >= 0; i--)
	{
		lruCacheGetTrampoline(LOGIC, i, &testLocation, &testfile);
	}

	for (int i = 0; i < cache->max_size; i++)
	{
		assert(_logicCache->keys[i] == i);
	}


	assert(_logicCache->size == cache->max_size);
}

void testCanEvict(LRUCache* cache)
{

	for (int i = cache->max_size; i >= 0; i--)
	{
		lruCacheGetTrampoline(LOGIC, i, &testLocation, &testfile);
	}

	for (int i = 0; i < cache->max_size; i++)
	{
		assert(_logicCache->keys[i] == i);
	}


	assert(_logicCache->size == cache->max_size);
	assert(_evicted == cache->max_size);
	assert(_evictedCalled);
}

void loadAGIFileTest(int resType, AGIFilePosType* location, AGIFile* agiData)
{
	assert(location == &testLocation);
	assert(agiData == &testfile);
}

void reset()
{
	init();
	_evictedCalled = false;
	_evicted = 0;
}

int main()
{
	reset();
	testCanAdd(_logicCache);

	reset();
	testCanFill(_logicCache);

	reset();
}



