#include "../../lruCache.h"
#include <assert.h>

AGIFile testfile;
AGIFilePosType testLocation;

byte _evicted;
boolean _evictedCalled;
int _expectedResType;

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

	printf("Test Can Add\n");

	assert(_logicCache->keys[0] == 5);
	assert(_logicCache->size == 1);
}

void testCanReorder(LRUCache* cache)
{
	const byte TestVal1 = 5;
	const byte TestVal2 = 7;

	printf("Test Can Reorder\n");

	lruCacheGetTrampoline(LOGIC, TestVal1, &testLocation, &testfile);
	lruCacheGetTrampoline(LOGIC, TestVal2, &testLocation, &testfile);

	assert(_logicCache->keys[0] == TestVal2);
	assert(_logicCache->keys[1] == TestVal1);
	assert(_logicCache->size == 2);

	lruCacheGetTrampoline(LOGIC, TestVal1, &testLocation, &testfile);

	assert(_logicCache->keys[0] == TestVal1);
	assert(_logicCache->keys[1] == TestVal2);
	assert(_logicCache->size == 2);
}



void testCanFill(LRUCache* cache)
{

	printf("Test Can Fill\n");

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
	printf("Test Can Evict\n");

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
	assert(resType == _expectedResType);
	assert(location == &testLocation);
	assert(agiData == &testfile);
}

void reset()
{
	init();
	_evictedCalled = false;
	_evicted = 0;
}

void runTests(int resType)
{
	LRUCache* cache = NULL;
	_expectedResType = resType;

	reset();

	if (resType == LOGIC)
	{
		cache = _logicCache;
	}

	testCanAdd(cache);

	reset();
	testCanFill(cache);

	reset();
	testCanReorder(cache);
}

int main()
{
	int _ = 0;
	runTests(LOGIC, _logicCache);

	printf("Press any key");
	scanf("%d", _);
}



