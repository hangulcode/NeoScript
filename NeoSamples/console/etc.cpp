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
	INeoVM* pVM = INeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		DWORD dwCallTime = 0;
		if (pFunctionName != NULL)
		{
			DWORD t1 = GetTickCount();
			pVM->CallN(pFunctionName);
			dwCallTime = GetTickCount() - t1;
		}
		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), dwCallTime);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("(Elapse:%d)\n", dwCallTime);

		INeoVM::ReleaseVM(pVM);
	}
	delete[] pFileBuffer;

    return 0;
}

