#include "stdafx.h"
#include "../../NeoSource/NeoVM.h"



int SAMPLE_slice_run()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("slice_run.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}


	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		printf("Compile Success. Code : %d bytes !!\n\n", pVM->GetBytesSize());

		// Alloc Worker Stack
		u32 id = pVM->CreateWorker();
		if (id == 0)
		{
			printf("Error - CeateWorker %s", "slice_fun\n");
			return - 1;
		}

		// Worker & NeoFunction Bind
		if (false == pVM->IsWorking(id))
		{
			if (false == pVM->BindWorkerFunction(id, "slice_fun"))
			{
				printf("Error - BindWorkerFunction %s\n", "slice_fun");
				return -1;
			}
		}

		DWORD dwPre = GetTickCount();

		// Run ...
		int i = 0;
		while(pVM->IsWorking(id))
		{
			DWORD t1 = GetTickCount();
			bool r = pVM->UpdateWorker(id, 1000, 1000);
			DWORD t2 = GetTickCount();
			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s (Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
			}
			else
			{
				DWORD dwNext = GetTickCount();
				if (dwNext - dwPre > 500)
				{
					printf("Slide Run %d (Elapse:%d)\n", i++, t2 - t1);
					dwPre = dwNext;
				}
			}
			Sleep(10);
		}
		pVM->ReleaseWorker(id);
		CNeoVM::ReleaseVM(pVM);
	}
	else
	{
		printf(err.c_str());
	}
	delete[] pFileBuffer;

    return 0;
}

