#include "stdafx.h"
#include "../../NeoSource/NeoVM.h"



int SAMPLE_etc(const char*pFileName, const char* pFunctionName)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad(pFileName, pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		printf("Compile Success. Code : %d bytes !!\n\n", pVM->GetBytesSize());

		DWORD t1 = GetTickCount();
		pVM->CallN(pFunctionName);
		DWORD t2 = GetTickCount();
		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s (Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("(Elapse:%d)\n", t2 - t1);

		CNeoVM::ReleaseVM(pVM);
	}
	else
	{
		printf(err.c_str());
	}	
	delete[] pFileBuffer;

    return 0;
}

