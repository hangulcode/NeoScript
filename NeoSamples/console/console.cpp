#include "stdafx.h"
#include "console.h"
#include "../../NeoSource/Neo.h"
#include <conio.h>

BOOL        FileLoad(const char* pFileName, void*& pBuffer, int& iLen)
{
	FILE* fp = NULL;
	int error_t = fopen_s(&fp, pFileName, "rb");
	if (error_t != 0)
		return false;

	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	pBuffer = new BYTE[iFileSize + 2];
	fread(pBuffer, iFileSize, 1, fp);
	fclose(fp);

	iLen = iFileSize;
	return true;
}

std::string getKeyString()
{
	std::string str;
	while (true)
	{
		int r = getchar();
		if (r == '\n') // Enter
			return str;

		str += (char)r;
	}
	return 0;
}

int SAMPLE_callback();
int SAMPLE_map_callback();
int SAMPLE_9_times();
int SAMPLE_slice_run();
int SAMPLE_time_limit();
int SAMPLE_etc(const char*pFileName, const char* pFunctionName);

int main()
{
	bool blEnd = false;
	while (blEnd == false)
	{
		printf("\n");

		printf("0 hello\n");
		printf("1 callback\n");
		printf("2 map_callback\n");
		printf("3 9_times\n");
		printf("4 string\n");
		printf("5 map\n");
		printf("6 list\n");
		printf("7 slice_run\n");
		printf("8 time_limit\n");
		printf("9 divide_by_zero\n");
		printf("10 delegate\n");
		printf("11 meta\n");
		printf("12 coroutine\n");
		printf("13 module\n");

		printf("only enter to exit\n");
		printf("\npress the number and enter ...\n");

		std::string key = getKeyString();

		if (key == "") break;
		else if (key ==  "0") SAMPLE_etc("hello.neo", "main");
		else if (key ==  "1") SAMPLE_callback();
		else if (key ==  "2") SAMPLE_map_callback();
		else if (key ==  "3") SAMPLE_9_times();
		else if (key ==  "4") SAMPLE_etc("string.neo", "main");
		else if (key ==  "5") SAMPLE_etc("map.neo", "main");
		else if (key ==  "6") SAMPLE_etc("list.neo", "main");
		else if (key ==  "7") SAMPLE_slice_run();
		else if (key ==  "8") SAMPLE_time_limit();
		else if (key ==  "9") SAMPLE_etc("etc.neo", "divide_by_zero");
		else if (key == "10") SAMPLE_etc("delegate.neo", "delegate");
		else if (key == "11") SAMPLE_etc("meta.neo", "meta");
		else if (key == "12") SAMPLE_etc("coroutine.neo", "test");
		else if (key == "13") SAMPLE_etc("module.neo", "main");
		
		system("pause");
	}
    return 0;
}

