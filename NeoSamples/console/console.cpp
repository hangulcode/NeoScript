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

int getKey()
{
	while (1)
	{
		int r = getchar();
		if (r > 0)
			return r;
	}
	return 0;
}

int SAMPLE_callback();
int SAMPLE_table_callback();
int SAMPLE_9_times();
int SAMPLE_table();
int SAMPLE_slice_run();
int SAMPLE_time_limit();

int main()
{
	bool blEnd = false;
	while (blEnd == false)
	{
		printf("\n");

		printf("0.exit\n");
		printf("1.callback\n");
		printf("2.table_callback\n");
		printf("3.9_times\n");
		printf("4.table\n");
		printf("5.slice_run\n");
		printf("6.time_limit\n");

		printf("\npress a key ...\n");

		int key = getKey();
		switch (key)
		{
		case '0':
			blEnd = true;
			break;
		case '1':
			SAMPLE_callback();
			system("pause");
			break;
		case '2':
			SAMPLE_table_callback();
			system("pause");
			break;
		case '3':
			SAMPLE_9_times();
			system("pause");
			break;
		case '4':
			SAMPLE_table();
			system("pause");
			break;
		case '5':
			SAMPLE_slice_run();
			system("pause");
			break;
		case '6':
			SAMPLE_time_limit();
			system("pause");
			break;
		}

		//system("pause");
	}

    return 0;
}

