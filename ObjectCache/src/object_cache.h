#ifndef _OBJECT_CACHE_H
#define _OBJECT_CACHE_H

#define ERR_NOT_INIT -1001
#define ERR_REINIT -1002
#define ERR_PARAM_INVALID -1003
#define ERR_OUT_OF_MEM -1004

typedef void*(*DumpFunc)(const void *obj, int typeID);
typedef void(*ReleaseFunc)(void *obj, int typeID);

int ObjectCacheInit(unsigned int maxKeyCnt, DumpFunc dump, ReleaseFunc release);
void ObjectCacheClear();
void ObjectCacheDestory();

void* ObjectCacheGet(const char *key);
int ObjectCacheInsert(const char *key, const void *obj, int typeID, unsigned int expireTime);

#endif
