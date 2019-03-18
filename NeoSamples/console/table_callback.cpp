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

bool FunSum(CNeoVMWorker* pN, short args)
{
	return ((CA*)pN->GetCallTableInfo()->_pUserData)->FunSum(pN, args);
}
bool FunMul(CNeoVMWorker* pN, short args)
{
	return ((CA*)pN->GetCallTableInfo()->_pUserData)->FunMul(pN, args);
}


static SNeoFunLib g_sTableFun[] =
{
	{ "sum", CNeoVM::RegisterNative(FunSum) },
	{ "mul", CNeoVM::RegisterNative(FunMul) },
	{ NULL, FunctionPtrNative() },
};



int SAMPLE_table_callback()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("table_callback.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true);
	if (pVM != NULL)
	{
		printf("Compile Success. Code : %d bytes !!\n\n", pVM->GetBytesSize());

		CA* pClass = new CA();

		VarInfo* g_sData;
		pVM->Call<VarInfo*>(&g_sData, "GetTable");
		if (g_sData != NULL && g_sData->GetType() == VAR_TABLE)
		{
			TableInfo* pTable = g_sData->_tbl;
			pTable->_pUserData = pClass; // <-------------------

			VarInfo fun;
			fun.SetType(VAR_TABLEFUN);
			SNeoFunLib* pFuns = g_sTableFun;
			while (pFuns->pName != NULL)
			{
				fun._fun = pFuns->fn;
				pTable->_strMap[pFuns->pName] = fun;
				pFuns++;
			}
		}

		DWORD t1 = GetTickCount();
		double r;
		pVM->Call(&r, "update", 5, 15);
		DWORD t2 = GetTickCount();

		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s (Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("%lf (Elapse:%d)\n", r, t2 - t1);

		CNeoVM::ReleaseVM(pVM);
	}
	else
	{
		printf(err.c_str());
	}
	delete[] pFileBuffer;

    return 0;
}

