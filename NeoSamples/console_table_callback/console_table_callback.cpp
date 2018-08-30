#include "stdafx.h"
#include "console_table_callback.h"
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

template<typename t1>
bool TemSum(CNeoVMWorker* pN, short args)
{
	return ((t1*)pN->GetCallTableInfo()->_pUserData)->FunSum(pN, args);
}

bool FunSum(CNeoVMWorker* pN, short args)
{
	return ((CA*)pN->GetCallTableInfo()->_pUserData)->FunSum(pN, args);
}
bool FunMul(CNeoVMWorker* pN, short args)
{
	return ((CA*)pN->GetCallTableInfo()->_pUserData)->FunMul(pN, args);
}


static SFunLib g_sTableFun[] =
{
	{ "sum", CNeoVM::RegisterNative(TemSum<CA>) },
	{ "mul", CNeoVM::RegisterNative(FunMul) },
	{ NULL, FunctionPtrNative() },
};



BOOL        FileLoad(const char* pFileName, void*& pBuffer, int& iLen)
{
	FILE* fp = NULL;
	int error_t = fopen_s(&fp, pFileName, "rb");
	if (error_t != 0)
		return false;

	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	pBuffer = new BYTE[iFileSize + 2];
	fread(pBuffer, iFileSize, 1, fp);
	fclose(fp);

	iLen = iFileSize;
	return true;
}

int main()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("1.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true);
	if (pVM != NULL)
	{
		printf("Comile Success. Code : %d bytes !!\n\n", pVM->GetBytesSize());

		CA* pClass = new CA();

		VarInfo* g_sData;
		pVM->Call<VarInfo*>(&g_sData, "GetTable");
		if (g_sData != NULL && g_sData->GetType() == VAR_TABLE)
		{
			TableInfo* pTable = g_sData->_tbl;
			pTable->_pUserData = pClass; // <-------------------

			VarInfo fun;
			fun.SetType(VAR_TABLEFUN);
			SFunLib* pFuns = g_sTableFun;
			while (pFuns->pName != NULL)
			{
				fun._fun = pFuns->fn;
				pTable->_strMap[pFuns->pName] = fun;
				pFuns++;
			}
		}

		DWORD t1 = GetTickCount();
		double r;
		pVM->Call(&r, "update", 5, 5);
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

