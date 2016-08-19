#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_reader.h"

int main(int argc, char *argv[])
{
	char configFile[256] = {'\0'};
	char path[256] = {'\0'};
	char *pc = strrchr(argv[0], '/');
	memcpy(path, argv[0], pc - argv[0] + 1);
	path[pc - argv[0] + 1] = '\0';
	snprintf(configFile, sizeof(configFile) - 1, "%s/test.ini", path);

	int errNo = 0;
	ConfigReader *reader = ConfigReaderCreate(configFile, &errNo);
	if (reader == NULL)
	{
		printf("config reader create failed, errNo[%d]\n", errNo);
		return 0;
	}

	ConfigReaderPrint(reader);
	printf("\nsection.key demo: \n");

	printf("websize.baidu=%s\n", ConfigReaderGetValue(reader, "website", "baidu"));
	printf("network.protocol=%s\n", ConfigReaderGetValue(reader, "network", "protocol"));
	printf("ext.py=%s\n", ConfigReaderGetValue(reader, "ext", "py"));
	printf("ext.txt=%s\n", ConfigReaderGetValue(reader, "ext", "txt"));

	ConfigReaderDestory(reader);
	return 0;

}