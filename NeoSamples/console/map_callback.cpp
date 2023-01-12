#include "stdafx.h"
#include "../../NeoSource/Neo.h"

class CA
{
public:
	bool FunSum(CNeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;

		double v1 = pN->GetArg_Double(1);
		double v2 = pN->GetArg_Double(2);

		pN->ReturnValue(v1 + v2);

		return true;
	}
	bool FunMul(CNeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;

		double v1 = pN->GetArg_Double(1);
		double v2 = pN->GetArg_Double(2);

		pN->ReturnValue(v1 * v2);

		return true;
	}
};

typedef bool (CA::*TYPE_FUN)(CNeoVMWorker* pN, short args);
std::map<std::string, TYPE_FUN> g_sTablesFun;

bool Fun(CNeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
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
	if (false == FileLoad("map_callback.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		printf("Compile Success. Code : %d bytes !!\n", pVM->GetBytesSize());

		CA* pClass = new CA();

		VarInfo* g_sData;
		pVM->Call<VarInfo*>(&g_sData, "GetMap");
		if (g_sData != NULL && g_sData->GetType() == VAR_TABLE)
		{
			TableInfo* pTable = g_sData->_tbl;
			pTable->_pUserData = pClass; // <-------------------
			pTable->_fun = CNeoVM::RegisterNative(Fun);

			g_sTablesFun["sum"] = &CA::FunSum;
			g_sTablesFun["mul"] = &CA::FunMul;
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
		CNeoVM::ReleaseVM(pVM);
	}
	else
	{
		printf(err.c_str());
	}
	delete[] pFileBuffer;
	g_sTablesFun.clear();

    return 0;
}

