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
int SAMPLE_table_callback();
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

		printf("0 callback\n");
		printf("1 table_callback\n");
		printf("2 9_times\n");
		printf("3 table\n");
		printf("4 list\n");
		printf("5 slice_run\n");
		printf("6 time_limit\n");
		printf("7 divide_by_zero\n");
		printf("8 delegate\n");
		printf("9 meta\n");
		printf("10 coroutine\n");

		printf("only enter to exit\n");
		printf("\npress the number and enter ...\n");

		std::string key = getKeyString();
		if (key == "")
			break;
		else if (key == "0")
			SAMPLE_callback();
		else if (key == "1")
			SAMPLE_table_callback();
		else if (key == "2")
			SAMPLE_9_times();
		else if (key == "3")
			SAMPLE_etc("table.neo", "main");
		else if (key == "4")
			SAMPLE_etc("list.neo", "main");
		else if (key == "5")
			SAMPLE_slice_run();
		else if (key == "6")
			SAMPLE_time_limit();
		else if (key == "7")
			SAMPLE_etc("etc.neo", "divide_by_zero");
		else if (key == "8")
			SAMPLE_etc("delegate.neo", "delegate");
		else if (key == "9")
			SAMPLE_etc("meta.neo", "meta");
		else if (key == "10")
			SAMPLE_etc("coroutine.neo", "test");
		system("pause");
	}
    return 0;
}

