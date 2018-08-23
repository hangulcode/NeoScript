#include "stdafx.h"
#include "console_table.h"
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


	int iCodeTempLen = 1 * 1024 * 1024;
	BYTE* pCodeTemp = new BYTE[iCodeTempLen];

	int iCodeLen = 0;
	if (CNeoVM::Compile(pFileBuffer, iFileLen, pCodeTemp, iCodeTempLen, &iCodeLen, true) == TRUE)
	{
		printf("Comile Success. Code : %d bytes !!\n", iCodeLen);
		CNeoVM* pVM = CNeoVM::LoadVM(pCodeTemp, iCodeLen);
		if (NULL != pVM)
		{
			for (int i = 0; i < 1; i++)
			{
				DWORD t1 = GetTickCount();
				pVM->Call<void>("main");
				DWORD t2 = GetTickCount();
				if (pVM->IsLastErrorMsg())
				{
					printf("\nError - VM Call : %s (Elapse:%d)", pVM->GetLastErrorMsg(), t2 - t1);
					pVM->ClearLastErrorMsg();
				}
				else
					printf("\n(Elapse:%d)", t2 - t1);
			}
			CNeoVM::ReleaseVM(pVM);
		}
	}
	delete[] pFileBuffer;
	delete[] pCodeTemp;

    return 0;
}

