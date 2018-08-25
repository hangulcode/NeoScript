#include "stdafx.h"
#include "console_time_limit.h"
#include "../../NeoSource/NeoVM.h"




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

int main()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("1.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, true);
	if (pVM != NULL)
	{
		for (int i = 1; i < 10; i++)
		{
			DWORD t1 = GetTickCount();
			double r = 0;
			bool bCompleted = pVM->Call_TL<double>(100, 1000, &r, "TimeTest");
			DWORD t2 = GetTickCount();
			if (false == bCompleted)
			{
				printf("Job Not Completed\n");
			}
			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s (Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
			}
			else
				printf("Sum %d + %d = %lf (Elapse:%d)\n", 100, i, r, t2 - t1);
		}
		CNeoVM::ReleaseVM(pVM);
	}
	delete[] pFileBuffer;

    return 0;
}

