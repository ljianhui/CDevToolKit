#ifndef _CONFIG_READER_H
#define _CONFIG_READER_H

#define ERR_CONFIG_NULL -1001
#define ERR_OPEN_CONFIG -1002
#define ERR_CONFIG_FORMAT -1003

#define ERR_MALLOC_FAILED -2001

typedef struct ConfigKeyValue
{
	const char* key;	// key的文本的地址
	const char* value;	// value的文本的地址
}ConfigKeyValue;

typedef struct ConfigSection
{
	const char* name; 		// []的文本的地址
	ConfigKeyValue *kvs;	// []对应Key-values起始地址
	int kvCount;			// kv的数量, ConfigKeyValue的元素个数
}ConfigSection;

typedef struct ConfigReader
{
	ConfigSection *sections;	// section的起始地址
	ConfigKeyValue *allKvs;  	// 所有kvs的起始地址
	char *text;             	// 所有文本的起始地址
	int sectionCount;       	// section的个数
}ConfigReader;

#define ConfigReaderGetSectionCount(reader) ((reader)->sectionCount)
#define ConfigReaderGetSection(reader, index) ((reader)->sections[index])
#define ConfigReaderGetSectionKvCount(section) ((section)->kvCount)
#define ConfigReaderGetSectionKv(section) ((section)->kvs[index])

ConfigReader* ConfigReaderCreate(const char *fileName, int *errNo);
void ConfigReaderDestory(ConfigReader *reader);

const char* ConfigReaderGetValue(const ConfigReader *reader,
	const char *sectionName, const char *key);

void ConfigReaderPrint(const ConfigReader *configReader);

#endif
