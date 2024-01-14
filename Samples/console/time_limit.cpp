#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

int SAMPLE_time_limit()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("time_limit.ns", pFileBuffer, iFileLen))
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
		pVM->SetTimeout(-1, 100, 1000); // -1 is main worker
		pVM->Setup_TL("TimeTest");
		for (int i = 1; i < 20; i++)
		{
			DWORD t1 = GetTickCount();
			bool bCompleted = pVM->Call_TL();
			DWORD t2 = GetTickCount();
			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
				break;
			}
			if (false == bCompleted)
			{
				printf("Job Not Completed (Elapse:%d)\n", t2 - t1);
			}
			else
				printf("(Elapse:%d)\n", t2 - t1);
		}
		INeoVM::ReleaseVM(pVM);
	}
	FileUnoad(nullptr, pFileBuffer, iFileLen);

    return 0;
}

