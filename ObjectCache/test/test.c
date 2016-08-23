#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "object_cache.h"

#define TYPE_INT 1
#define TYPE_DOUBLE 2

void* DumpObj(const void *value, int typeID)
{
	if (typeID == TYPE_INT)
	{
		int *obj = (int*)malloc(sizeof(int));
		if (obj == NULL)
		{
			return NULL;
		}

		*obj = *(int*)value;
		return obj;
	}
	else if (typeID == TYPE_DOUBLE)
	{
		double *obj = (double*)malloc(sizeof(double));
		if (obj == NULL)
		{
			return NULL;
		}
		*obj = *(double*)value;
		return obj;
	}
	return NULL;
}

void ReleaseObj(void *value, int typeID)
{
	if (typeID == TYPE_INT || typeID == TYPE_DOUBLE)
	{
		free(value);
	}
}

int main()
{
	int ret = ObjectCacheInit(3, DumpObj, ReleaseObj);
	if (ret != 0)
	{
		printf("init local cache failed, ret[%d]\n", ret);
		return 0;
	}

	int n = 100;
	double d = 1000;
	ObjectCacheInsert("intX", &n, TYPE_INT, 2);
	ObjectCacheInsert("doubleX", &d, TYPE_DOUBLE, 10);
	sleep(1);

	// intX = 100
	// doubleX = 1000.000000
	void *value = ObjectCacheGet("intX");
	if (value == NULL)
	{
		printf("intX no data\n");
	}
	else
	{
		printf("intX = %d\n", *(int*)value);
	}
	value = ObjectCacheGet("doubleX");
	if (value == NULL)
	{
		printf("doubleX no data\n");
	}
	else
	{
		printf("doubleX = %lf\n", *(double*)value);
	}
	sleep(1);

	// intX no data
	// doubleX = 1000.000000
	value = ObjectCacheGet("intX");
	if (value == NULL)
	{
		printf("intX no data\n");
	}
	else
	{
		printf("intX = %d\n", *(int*)value);
	}
	value = ObjectCacheGet("doubleX");
	if (value == NULL)
	{
		printf("doubleX no data\n");
	}
	else
	{
		printf("doubleX = %lf\n", *(double*)value);
	}
	sleep(1);

	++n;
	ObjectCacheInsert("intY", &n, TYPE_INT, 10);
	++n;
	ObjectCacheInsert("intZ", &n, TYPE_INT, 10);
	++n;
	ObjectCacheInsert("intA", &n, TYPE_INT, 10);
	value = ObjectCacheGet("doubleX");
	if (value == NULL)
	{
		printf("doubleX no data\n");
	}
	else
	{
		printf("doubleX = %lf\n", *(double*)value);
	}

	ObjectCacheDestory();
	
	return 0;


}