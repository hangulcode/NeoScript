#include "stdafx.h"
#include "../../NeoSource/Neo.h"

class CA
{
public:
	bool FunSum(INeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;

		double v1, v2;
		if (pN->GetArg_Double(1, v1) == false) return false;
		if (pN->GetArg_Double(2, v2) == false) return false;

		pN->ReturnValue(v1 + v2);

		return true;
	}
	bool FunMul(INeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;

		double v1, v2;
		if (pN->GetArg_Double(1, v1) == false) return false;
		if (pN->GetArg_Double(2, v2) == false) return false;

		pN->ReturnValue(v1 * v2);

		return true;
	}
};

typedef bool (CA::*TYPE_FUN)(INeoVMWorker* pN, short args);
std::map<std::string, TYPE_FUN> g_sTablesFun;

bool Fun(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sTablesFun.find(fun);
	if (it == g_sTablesFun.end())
		return false;

	TYPE_FUN f = (*it).second;
	return (((CA*)pUserData)->*f)(pN, args);
}


int SAMPLE_map_callback()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("map_callback.ns", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	INeoVM* pVM = INeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		CA* pClass = new CA();

		VarInfo* g_sData;
		g_sData = pVM->GetVar("g_sData");
		if (g_sData != NULL && g_sData->GetType() == VAR_MAP)
		{
			if(INeoVM::RegisterTableCallBack(g_sData, pClass, Fun))
			{
				g_sTablesFun["sum"] = &CA::FunSum;
				g_sTablesFun["mul"] = &CA::FunMul;
			}
		}

		DWORD t1 = GetTickCount();
		double r;
		pVM->Call(&r, "update", 5, 15);
		DWORD t2 = GetTickCount();

		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("%lf\n(Elapse:%d)\n", r, t2 - t1);

		delete pClass;
		INeoVM::ReleaseVM(pVM);
	}
	delete[] pFileBuffer;
	g_sTablesFun.clear();

    return 0;
}

