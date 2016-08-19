#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config_reader.h"

#define IS_SPACE_CHAR(c) ((c) == ' '||(c) == '\n'||(c) == '\t'||(c) == '\r')
#define MAX_BUF_LEN 1024

typedef int(*CmpFunc)(const void *x, const void *y);

static int ConfigSectionCmp(const void *x, const void *y)
{
	return strcmp(((ConfigSection*)x)->name, ((ConfigSection*)y)->name);
}

static int ConfigKeyValueCmp(const void *x, const void *y)
{
	return strcmp(((ConfigKeyValue*)x)->key, ((ConfigKeyValue*)y)->key);
}

/*
 * 删除字符串两端的空白字符
 */
static void TrimString(char *str)
{
	char *pc = NULL;
	for (pc = str + strlen(str) - 1; pc >= str && IS_SPACE_CHAR(*pc); --pc)
	{
		*pc = '\0';
	}

	for (pc = str; *pc != '\0' && IS_SPACE_CHAR(*pc); ++pc);
	if (pc != str)
	{
		char *dst = NULL;
		for (dst = str; *pc; ++pc, ++dst)
		{
			*dst = *pc;
		}
		*dst = '\0';
	}
}

/*
 * 对读取的文件行数据，进行预处理
 * @return 非0，需要进一步处理，0无需进一步处理
 */
static int PreHandleInput(char *line)
{
	TrimString(line);
	// 注释行或空行，无需要进一步处理
	if (line[0] == '\0' || line[0] == '#' || strncmp(line, "//", 2) == 0) 
	{
		return 0;
	}

	// 行内注释，删除注释
	char *comment = strstr(line, "//");
	if (comment != NULL)
	{
		*comment = '\0';
		TrimString(line);
	}

	comment = strchr(line, '#');
	if (comment != NULL)
	{
		*comment = '\0';
		TrimString(line);
	}

	return 1;
}

/*
 * 分析配置文件conf的内容,计算文件中的总section数,kv数和有效的总字节数
 */
static int AnalyseConfig(const char *conf, int *sectionCount, int *kvCount, int *bytes)
{
	FILE *fconf = fopen(conf, "r");
	if (fconf == NULL)
	{
		return ERR_OPEN_CONFIG;
	}

	*sectionCount = 0;
	*kvCount = 0;
	*bytes = 0;
	char buffer[MAX_BUF_LEN] = {0};
	while (fgets(buffer, sizeof(buffer) - 1, fconf))
	{
		if (PreHandleInput(buffer) == 0) 
		{
			continue;
		}

		char *equal = strchr(buffer, '=');
		char *leftBracket = strchr(buffer, '[');
		char *rightBracket = strchr(buffer, ']');
		if (leftBracket == buffer && rightBracket != NULL && equal == NULL)
		{
			++(*sectionCount);
			// [xxx] => xxx\0
			*bytes += (strlen(buffer) - 1);
		}
		else if (sectionCount > 0 && leftBracket == NULL && rightBracket == NULL && equal != NULL)
		{
			++(*kvCount);
			// xxx=yyy => xxx\0yyy\0
			*bytes += (strlen(buffer) + 1);
		}
		else
		{
			fclose(fconf);
			return ERR_CONFIG_FORMAT;
		}
	}

	fclose(fconf);
	return 0;
}

static ConfigReader* ConfigReaderCreateObj(int sectionCount, int kvCount, int bytes)
{
	ConfigReader *reader = (ConfigReader*)malloc(sizeof(ConfigReader));
	if (reader == NULL)
	{
		return NULL;
	}

	ConfigSection *sections = (ConfigSection*)malloc(sizeof(ConfigSection) * sectionCount);
	if (sections == NULL)
	{
		free(reader);
		return NULL;
	}

	ConfigKeyValue *kvs = (ConfigKeyValue*)malloc(sizeof(ConfigKeyValue) * kvCount);
	if (kvs == NULL)
	{
		free(sections);
		free(reader);
		return NULL;
	}

	char *text = (char*)malloc(bytes);
	if (text == NULL)
	{
		free(kvs);
		free(sections);
		free(reader);
		return NULL;
	}

	reader->sections = sections;
	reader->allKvs = kvs;
	reader->text = text;
	reader->sectionCount = sectionCount;
	return reader;
}

static int ConfigReaderLoadConfig(ConfigReader *reader, const char *conf)
{
	FILE *fconf = fopen(conf, "r");
	if (fconf == NULL)
	{
		return ERR_OPEN_CONFIG;
	}

	ConfigSection *section = reader->sections;
	ConfigKeyValue *keyValue = reader->allKvs;
	char *text = reader->text;

	char buffer[MAX_BUF_LEN] = {0};
	char *pc = '\0';
	unsigned len = 0;
	int isFirstSection = 1;
	int i = 0;
	while (fgets(buffer, sizeof(buffer)-1, fconf))
	{
		if (PreHandleInput(buffer) == 0)
		{
			continue;
		}

		if (buffer[0] == '[')
		{
			pc = strchr(buffer + 1, ']');
			if (pc == NULL)
			{
				continue;
			}

			if (!isFirstSection)
			{
				++section;
			}
			section->kvs = keyValue;
			section->kvCount = 0;

			*pc = '\0';
			TrimString(buffer+1);
			len = strlen(buffer+1);
			memcpy(text, buffer + 1, len + 1);
			section->name = text;
			text += (len + 1);
			isFirstSection = 0;
		}
		else
		{
			pc = strchr(buffer, '=');
			if (pc == NULL)
			{
				continue;
			}

			*pc = '\0';
			char *strKey = buffer;
			char *strValue = pc + 1;
			TrimString(strKey);
			TrimString(strValue);

			len = strlen(strKey);
			memcpy(text, strKey, len + 1);
			keyValue->key = text;
			text += (len + 1);

			len = strlen(strValue);
			memcpy(text, strValue, len + 1);
			keyValue->value = text;
			text += (len + 1);

			++section->kvCount;
			++i;
			++keyValue;
		}
	}
	fclose(fconf);
	return 0;
}

static void ElemSwap(void *x, void *y, unsigned int size, void *swapBuf)
{
	memcpy(swapBuf, x, size);
	memcpy(x, y, size);
	memcpy(y, swapBuf, size);
}

static void QuickSort(void *elems, int begin, int end, unsigned int elemSize, 
	CmpFunc compare, void *swapBuf)
{
	if (elems == NULL || begin >= end)
	{
		return;
	}

	//随机选取一个元素作为枢纽，并与最后一个元素交换
	char *celems = (char*)elems;
	int ic = rand()%(end - begin + 1) + begin;
	ElemSwap(celems + ic * elemSize, celems + end * elemSize, elemSize, swapBuf);

	void *centre = (void*)(celems + end * elemSize);
	int i = begin;
	int j = end - 1;
	while(1)
	{
		//从前向后扫描，找到第一个小于枢纽的值，
		//在到达数组末尾前，必定结束循环,因为最后一个值为centre
		while (compare(celems + i * elemSize, centre) < 0)
			++i;
		//从后向前扫描，此时要检查下标，防止数组越界
		while(j >= begin && compare(celems + j * elemSize, centre) > 0)
			--j;
		//如果没有完成一趟交换，则交换
		if(i < j)
			ElemSwap(celems + elemSize * i++, celems + elemSize * j--, elemSize, swapBuf);
		else
			break;
	}
	//把枢纽放在正确的位置
	ElemSwap(celems + i * elemSize, celems + end * elemSize, elemSize, swapBuf);
	QuickSort(elems, begin, i - 1, elemSize, compare, swapBuf);
	QuickSort(elems, i + 1, end, elemSize, compare, swapBuf);
}

static int Sort(void *elems, int count, unsigned int elemSize, CmpFunc compare)
{
	if (elemSize <= MAX_BUF_LEN)
	{
		char swapBuf[MAX_BUF_LEN] = {0};
		QuickSort(elems, 0, count - 1, elemSize, compare, swapBuf);
	}
	else
	{
		void *swapBuf = malloc(elemSize);
		if (swapBuf == NULL)
		{
			return ERR_MALLOC_FAILED;
		}

		QuickSort(elems, 0, count - 1, elemSize, compare, swapBuf);
		free(swapBuf);
	}
	return 0;
}

static int ConfigReaderSort(ConfigReader *reader)
{
	srand(time(NULL));

	int ret = Sort(reader->sections, reader->sectionCount, sizeof(ConfigSection), ConfigSectionCmp);
	if (ret != 0)
	{
		return ret;
	}

	int i = 0;
	ConfigSection *section = reader->sections;
	for (i = 0; i < reader->sectionCount; ++i)
	{
		ret = Sort(section->kvs, section->kvCount, sizeof(ConfigKeyValue), ConfigKeyValueCmp);
		if (ret != 0)
		{
			return ret;
		}
		++section;
	}
	return 0;
}

static int BinSearch(const void *elems, int count, unsigned int elemSize, 
	const void *key, CmpFunc compare)
{
	int begin = 0;
	int end = count - 1;
	const char *celems = (const char*)elems;
	while (begin <= end)
	{
		int middle = begin + (end - begin) / 2;
		int ret = compare(celems + elemSize * middle, key);
		if (ret == 0)
		{
			return middle;
		}
		else if (ret < 0)
		{
			begin = middle + 1;
		}
		else
		{
			end = middle - 1;
		}

	}
	return -1;
}

ConfigReader* ConfigReaderCreate(const char *fileName, int *errNo)
{
	int ret = 0;
	if (fileName == NULL)
	{
		if (errNo != NULL) *errNo = ERR_CONFIG_NULL;
		return NULL;
	}

	int sectionCount = 0;
	int kvCount = 0;
	int bytes = 0;
	ret = AnalyseConfig(fileName, &sectionCount, &kvCount, &bytes);
	if (ret != 0)
	{
		if (errNo != NULL) *errNo = ret;
		return NULL;
	}

	ConfigReader *reader = ConfigReaderCreateObj(sectionCount, kvCount, bytes);
	if (reader == NULL)
	{
		if (errNo != NULL) *errNo = ERR_MALLOC_FAILED;
		return NULL;
	}

	ret = ConfigReaderLoadConfig(reader, fileName);
	if (ret != 0)
	{
		if (errNo != NULL) *errNo = ret;
		ConfigReaderDestory(reader);
		return NULL;
	}

	ret = ConfigReaderSort(reader);
	if (ret != 0)
	{
		if (errNo != NULL) *errNo = ret;
		ConfigReaderDestory(reader);
		return NULL;
	}

	return reader;
}


void ConfigReaderDestory(ConfigReader *reader)
{
	if (reader == NULL)
	{
		return;
	}

	if (reader->sections != NULL)
	{
		free(reader->sections);
	}

	if (reader->allKvs != NULL)
	{
		free(reader->allKvs);
	}

	if (reader->text != NULL)
	{
		free(reader->text);
	}

	free(reader);
}

const char* ConfigReaderGetValue(const ConfigReader *reader,
	const char *sectionName, const char *key)
{
	if (reader == NULL || sectionName == NULL || key == NULL)
	{
		return "";
	}

	ConfigSection section = {sectionName, NULL, 0};
	ConfigKeyValue keyValue = {key, NULL};
	int i = BinSearch(reader->sections, reader->sectionCount, sizeof(ConfigSection), 
		&section, ConfigSectionCmp);
	if (i >= 0)
	{
		int j = BinSearch(reader->sections[i].kvs, reader->sections[i].kvCount, sizeof(ConfigKeyValue),
			&keyValue, ConfigKeyValueCmp);
		if (j >= 0)
		{
			return reader->sections[i].kvs[j].value;
		}
	}
	return "";
}

void ConfigReaderPrint(const ConfigReader *reader)
{
	if (reader == NULL)
	{
		return;
	}

	int i = 0;
	int j = 0;
	ConfigSection *section = NULL;
	ConfigKeyValue *keyValue = NULL;
	for (i = 0; i < reader->sectionCount; ++i)
	{
		section = reader->sections + i;
		printf("[%s]\n", section->name);
		for (j = 0; j < section->kvCount; ++j)
		{
			keyValue = section->kvs + j;
			printf("%s=%s\n", keyValue->key, keyValue->value);
		}
	}
}
