#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

int SAMPLE_slice_run(INeoLoader* pLoader, std::string filename)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == pLoader->Load(filename.c_str(), pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}


	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	param.putASM = true;
	param.debug = true;

	int result = 0;
	NeoExecContextPool* execPool = NeoExecContextPool_Create();
	NeoLoadVMParam vparam;
	vparam.execPool = execPool;
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param, &vparam);
	if (pVM != NULL)
	{
		// Alloc Worker Stack
		u32 id = pVM->GetMainWorkerID();
		if (id == 0)
		{
			printf("Error - GetMainWorkerID %s", "slice_fun\n");
			result = -1;
			goto cleanup;
		}

		// Worker & NeoFunction Bind
		if (false == pVM->IsWorking(id))
		{
			if (false == pVM->BindWorkerFunction(id, "slice_fun"))
			{
				printf("Error - BindWorkerFunction %s\n", "slice_fun");
				result = -1;
				goto cleanup;
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
		pVM = NULL;
	}
cleanup:
	if (pVM != NULL)
		INeoVM::ReleaseVM(pVM);
	NeoExecContextPool_Destroy(execPool);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);

    return result;
}

