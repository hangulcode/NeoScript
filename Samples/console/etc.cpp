#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

static std::string preObejct = "GameObject";
static NeoGlobalSymbol g_globalSyms[] = { { "GameObject" } };
static NeoGlobalSymbolTable g_globalSymTable = { g_globalSyms, (int)_countof(g_globalSyms) };
static NeoCompileDefines defines;

static void neo_globalinterface(INeoVMWorker* pWorker, void* This)
{
	VarInfo* g_sGameObject = pWorker->GetVar(preObejct);
}

int SAMPLE_etc(INeoLoader* pLoader, std::string filename, const char* pFunctionName)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == pLoader->Load(filename.c_str(), pFileBuffer, iFileLen))
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
	defines.clear();
	defines.values["KEY_LEFT"] = { NEO_DEFINE_TOKEN_INT, "37" };

	param.globalSymbols = &g_globalSymTable;
	param.defines = &defines;
	NeoLoadVMParam vparam;
	vparam.NeoGlobalInterface = neo_globalinterface;
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param, &vparam);
#else
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param);
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

