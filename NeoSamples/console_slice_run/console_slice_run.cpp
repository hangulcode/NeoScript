#include "stdafx.h"
#include "console_slice_run.h"
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
		printf("Comile Success %d bytes !!\n", iCodeLen);
		CNeoVM* pVM = CNeoVM::LoadVM(pCodeTemp, iCodeLen);
		if (NULL != pVM)
		{
			// Allock Worker Stack
			u32 id = pVM->CreateWorker();
			if (id == 0)
			{
				printf("\nError - CeateWorker %s", "slice_fun");
				return - 1;
			}

			// Worker & NeoFunction Bind
			if (false == pVM->IsWorking(id))
			{
				if (false == pVM->BindWorkerFunction(id, "slice_fun"))
				{
					printf("\nError - BindWorkerFunction %s", "slice_fun");
					return -1;
				}
			}

			// Run ...
			int i = 0;
			while(pVM->IsWorking(id))
			{
				DWORD t1 = GetTickCount();
				bool r = pVM->UpdateWorker(id, 1000, 1000);
				DWORD t2 = GetTickCount();
				if (pVM->IsLastErrorMsg())
				{
					printf("\nError - VM Call : %s (Elapse:%d)", pVM->GetLastErrorMsg(), t2 - t1);
					pVM->ClearLastErrorMsg();
				}
				else
					printf("\nSlide Run %d (Elapse:%d)", i++, t2 - t1);
				Sleep(10);
			}
			pVM->ReleaseWorker(id);
			CNeoVM::ReleaseVM(pVM);
		}
	}
	delete[] pFileBuffer;
	delete[] pCodeTemp;

    return 0;
}

