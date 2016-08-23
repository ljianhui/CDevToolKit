#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "object_cache.h"

#define MAX_INT 0x7FFFFFFF

typedef struct CacheEntry
{
	char *key;
	void *obj;
	int typeID;
	unsigned int visitCnt;
	time_t expireStamps;
	time_t visitStamps;
	struct CacheEntry *next;
}CacheEntry;

typedef struct ObjectCacheMng
{
	CacheEntry **table;
	unsigned int sizeMask;
	unsigned int keyCnt;
	unsigned int maxKeyCnt;
	DumpFunc dump;
	ReleaseFunc release;
}ObjectCacheMng;

static unsigned int GenHashValue(const void *key, int len) 
{
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = 5381;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) 
    {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) 
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

static unsigned int GenSizeMask(unsigned int maxKeyCnt)
{
	if (maxKeyCnt >= MAX_INT)
	{
		return MAX_INT;
	}

	unsigned int i = 32;
	while(i - 1 < maxKeyCnt)
	{
		i *= 2;
	}
	return i - 1;
}

static ObjectCacheMng* ObjectCacheMngInstance()
{
	static ObjectCacheMng mng = {
		.table=NULL,
		.sizeMask = 0,
		.keyCnt = 0,
		.maxKeyCnt = 0,
		.dump = NULL,
		.release = NULL
	};
	return &mng;
}

static CacheEntry* CacheEntryCreate(const char *key, const void *obj, int typeID, 
	unsigned int expireTime, DumpFunc dump)
{
	CacheEntry *entry = (CacheEntry*)malloc(sizeof(CacheEntry));
	if (entry == NULL)
	{
		return NULL;
	}

	unsigned int len = strlen(key);
	entry->key = (char*)malloc(len + 1);
	if (entry->key == NULL)
	{
		free(entry);
		return NULL;
	}
	memcpy(entry->key, key, len + 1);

	entry->obj = dump(obj, typeID);
	if (entry->obj == NULL)
	{
		free(entry->key);
		free(entry);
		return NULL;
	}

	time_t now = time(NULL);
	entry->typeID = typeID;
	entry->visitCnt = 1;
	entry->expireStamps = now + expireTime;
	entry->visitStamps = now;
	entry->next = NULL;
	return entry;
}

static void CacheEntryDestory(CacheEntry *entry, ReleaseFunc release)
{
	if (entry->key != NULL)
	{
		free(entry->key);
	}

	if (entry->obj != NULL)
	{
		release(entry->obj, entry->typeID);
	}

	free(entry);
}

static int CacheEntrySet(CacheEntry *entry, const void *obj, int typeID, unsigned int expireTime,
	DumpFunc dump, ReleaseFunc release)
{
	void *newObj = dump(obj, entry->typeID);
	if (newObj == NULL)
	{
		return ERR_OUT_OF_MEM;
	}

	if (entry->obj != NULL)
	{
		release(entry->obj, entry->typeID);
	}

	entry->obj = newObj;
	entry->expireStamps = time(NULL) + expireTime;
	return 0;
}

static int ObjectCacheMngFindEntry(const ObjectCacheMng *mng, unsigned int index, const char *key,
	CacheEntry **preEntryPtr, CacheEntry **entryPtr)
{
	if (mng->table == NULL)
	{
		return -1;
	}

	CacheEntry *entry = mng->table[index];
	CacheEntry *preEntry = mng->table[index];
	while (entry != NULL)
	{
		if (strcmp(entry->key, key) == 0)
		{
			*preEntryPtr = preEntry;
			*entryPtr = entry;
			return 0;
		}
		preEntry = entry;
		entry = entry->next;
	}
	return -1;
}

static void ObjectCacheMngRemoveEntry(ObjectCacheMng *mng, unsigned int i, 
	CacheEntry *preEntry, CacheEntry *entry)
{
	if (entry == NULL || preEntry == NULL)
	{
		return;
	}

	if (entry == preEntry)
	{
		// 删除的是第一个entry
		mng->table[i] = entry->next;
	}
	else
	{
		// 删除的不是第一个entry
		preEntry->next = entry->next;
	}
	CacheEntryDestory(entry, mng->release);
	--mng->keyCnt;
}

static void ObjectCacheMngDieOut(ObjectCacheMng *mng)
{
	time_t now = time(NULL);

	CacheEntry *entry = NULL;
	CacheEntry *preEntry = NULL;
	CacheEntry *nruEntry = NULL;
	CacheEntry *preNruUseEntry = NULL;
	unsigned int nruIndex = 0;

	unsigned int i = 0;
	for (i = 0; i < mng->sizeMask; ++i)
	{
		entry = mng->table[i];
		preEntry = entry;

		while(entry != NULL)
		{
			if (entry->expireStamps < now)
			{
				// 数据过期，删除过期的项
				ObjectCacheMngRemoveEntry(mng, i, preEntry, entry);
				return;
			}
			else
			{
				// 数据未过期, 记录最近未使用的项
				if (nruEntry == NULL || 
					nruEntry->visitStamps > entry->visitStamps ||
					(nruEntry->visitStamps == entry->visitStamps && nruEntry->visitCnt > entry->visitCnt))
				{
					nruIndex = i;
					nruEntry = entry;
					preNruUseEntry = preEntry;
				}
			}
			preEntry = entry;
			entry = entry->next;
		} // end while
	}

	ObjectCacheMngRemoveEntry(mng, nruIndex, preNruUseEntry, nruEntry);
}

static int ObjectCacheMngInsert(ObjectCacheMng *mng, unsigned int index, 
	const char *key, const void *obj, int typeID, unsigned int expireTime)
{
	CacheEntry *entry = CacheEntryCreate(key, obj, typeID, expireTime, mng->dump);
	if (entry == NULL)
	{
		return ERR_OUT_OF_MEM;
	}

	if (mng->keyCnt >= mng->maxKeyCnt)
	{
		ObjectCacheMngDieOut(mng);
	}

	entry->next = mng->table[index];
	mng->table[index] = entry;
	++mng->keyCnt;
	return 0;
}

int ObjectCacheInit(unsigned int maxKeyCnt, DumpFunc dump, ReleaseFunc release)
{
	if (maxKeyCnt == 0 || dump == NULL || release == NULL)
	{
		return ERR_PARAM_INVALID;
	}

	ObjectCacheMng *mng = ObjectCacheMngInstance();
	if (mng->table)
	{
		return ERR_REINIT;
	}

	unsigned int sizeMask = GenSizeMask(maxKeyCnt);
	CacheEntry **table = (CacheEntry**)malloc(sizeof(CacheEntry*) * sizeMask);
	if (table == NULL)
	{
		return ERR_OUT_OF_MEM;
	}

	memset(table, 0, sizeof(CacheEntry*) * sizeMask);
	mng->table = table;
	mng->sizeMask = sizeMask;
	mng->keyCnt = 0;
	mng->maxKeyCnt = maxKeyCnt;
	mng->dump = dump;
	mng->release = release;
	return 0;
}

void ObjectCacheClear()
{
	ObjectCacheMng *mng = ObjectCacheMngInstance();
	if (mng->table == NULL)
	{
		return;
	}

	unsigned int i = 0;
	for (i = 0; i < mng->sizeMask; ++i)
	{
		if (mng->table[i] == NULL)
		{
			continue;
		}

		CacheEntry *entry1 = mng->table[i];
		CacheEntry *entry2 = entry1;
		while (entry1 != NULL)
		{
			entry2 = entry1->next;
			CacheEntryDestory(entry1, mng->release);
			entry1 = entry2;
		}
		mng->table[i] = NULL;
	}

	mng->keyCnt = 0;
}

void ObjectCacheDestory()
{
	ObjectCacheMng *mng = ObjectCacheMngInstance();
	if (mng->table == NULL)
	{
		return;
	}

	ObjectCacheClear();
	free(mng->table);
	mng->table = NULL;
	mng->sizeMask = 0;
	mng->maxKeyCnt = 0;
	mng->dump = NULL;
	mng->release = NULL;
}

void* ObjectCacheGet(const char *key)
{
	if (key == NULL)
	{
		return NULL;
	}

	ObjectCacheMng *mng = ObjectCacheMngInstance();
	if (mng->table == NULL)
	{
		return NULL;
	}

	unsigned int index = GenHashValue(key, strlen(key)) & mng->sizeMask;
	CacheEntry *entry = NULL;
	CacheEntry *preEntry = NULL;
	int ret = ObjectCacheMngFindEntry(mng, index, key, &preEntry, &entry);
	if (ret != 0)
	{
		return NULL;
	}

	time_t now = time(NULL);
	if (entry->expireStamps > now)
	{
		// key 没有过期
		++entry->visitCnt;
		entry->visitStamps = now;
		return entry->obj;
	}
	else
	{
		// key 已经过期，删除该entry
		ObjectCacheMngRemoveEntry(mng, index, preEntry, entry);
		return NULL;
	}
	return NULL;
}

int ObjectCacheInsert(const char *key, const void *obj, int typeID, unsigned int expireTime)
{
	if (key == NULL || obj == NULL || expireTime == 0)
	{
		return ERR_PARAM_INVALID;
	}

	ObjectCacheMng *mng = ObjectCacheMngInstance();
	if (mng->table == NULL)
	{
		return ERR_NOT_INIT;
	}

	unsigned int index = GenHashValue(key, strlen(key)) & mng->sizeMask;
	CacheEntry *entry = NULL;
	CacheEntry *preEntry = NULL;
	int ret = ObjectCacheMngFindEntry(mng, index, key, &preEntry, &entry);
	if (ret != 0)
	{
		// key 不存在
		return ObjectCacheMngInsert(mng, index, key, obj, typeID, expireTime);
	}
	else
	{
		return CacheEntrySet(entry, obj, typeID, expireTime, mng->dump, mng->release);
	}

	return 0;
}
