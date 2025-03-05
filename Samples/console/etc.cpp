#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

static void neo_globalinterface(VarInfo* v, void* This)
{
}

int SAMPLE_etc(INeoLoader* pLoader, const char*pFileName, const char* pFunctionName)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == pLoader->Load(pFileName, pFileBuffer, iFileLen))
	{
		printf("file read error\n");
		return -1;
	}

	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	param.putASM = true;
	param.debug = true;
#if true
	static std::string preHeader = "export var gameObject;";
	static std::string preObejct = "gameObject";
	param.preCompileHeader = &preHeader;
	NeoLoadVMParam vparam;
	vparam.globalInterfaceName = &preObejct;
	vparam.NeoGlobalInterface = neo_globalinterface;
	INeoVM* pVM = INeoVM::CompileAndLoadVM(param, &vparam);
#else
	INeoVM* pVM = INeoVM::CompileAndLoadVM(param);
#endif

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
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);

    return 0;
}

