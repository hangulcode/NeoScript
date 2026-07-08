#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

int SAMPLE_9_times(INeoLoader* pLoader, std::string filename)
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

	NeoExecContextPool* execPool = NeoExecContextPool_Create();
	NeoLoadVMParam vparam;
	vparam.execPool = execPool;
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param, &vparam);
	if (pVM != NULL)
	{
		for (int i = 1; i < 10; i++)
		{
			DWORD t1 = GetTickCount();
			pVM->CallN("Time9", i);
			DWORD t2 = GetTickCount();
			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
			}
			else
				printf("(Elapse:%d)\n", t2 - t1);
		}
		INeoVM::ReleaseVM(pVM);
	}
	NeoExecContextPool_Destroy(execPool);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);

    return 0;
}

