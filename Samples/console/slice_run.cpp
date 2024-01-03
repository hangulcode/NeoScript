#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

int SAMPLE_slice_run()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("slice_run.ns", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}


	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	param.putASM = true;
	param.debug = true;

	INeoVM* pVM = INeoVM::CompileAndLoadVM(param);
	if (pVM != NULL)
	{
		// Alloc Worker Stack
		u32 id = pVM->GetMainWorkerID();
		if (id == 0)
		{
			printf("Error - GetMainWorkerID %s", "slice_fun\n");
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

		pVM->SetTimeout(id, 200, 1000);

		DWORD dwPre = GetTickCount();

		// Run ...
		int i = 0;
		while(pVM->IsWorking(id))
		{
			DWORD t1 = GetTickCount();
			bool r = pVM->UpdateWorker(id);
			DWORD t2 = GetTickCount();
			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
			}
			else
			{
				DWORD dwNext = GetTickCount();
				if (dwNext - dwPre > 500)
				{
					printf("Slide Run %d\n(Elapse:%d)\n", i++, t2 - t1);
					dwPre = dwNext;
				}
			}
			Sleep(10);
		}
		pVM->ReleaseWorker(id);
		INeoVM::ReleaseVM(pVM);
	}
	delete[] pFileBuffer;

    return 0;
}

