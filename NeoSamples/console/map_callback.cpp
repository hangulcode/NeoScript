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
	double _x = 0.1f;
	double _y = 1.0f;
	double _z = 10.0f;
	bool PryTransform(INeoVMWorker* pN, VarInfo* pVar, bool get)
	{
		if(get)
		{
			if(pN->ChangeVarType(pVar, VAR_MAP))
			{
				pVar->TableInsertFloat("x", _x);
				pVar->TableInsertFloat("y", _y);
				pVar->TableInsertFloat("z", _z);
			}
		}
		else
		{
			if(pVar->GetType() == VAR_MAP)
			{
				if (false == pVar->TableFindFloat("x", _x)) return false;
				if (false == pVar->TableFindFloat("y", _y)) return false;
				if (false == pVar->TableFindFloat("z", _z)) return false;
			}
		}
		return true;
	}
};

typedef bool (CA::*TYPE_FUN)(INeoVMWorker* pN, short args);
typedef bool (CA::* TYPE_PRY)(INeoVMWorker* pN, VarInfo* pVar, bool get);
std::map<std::string, TYPE_FUN> g_sTablesFunction;
std::map<std::string, TYPE_PRY> g_sTablesProperty;

bool Fun(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sTablesFunction.find(fun);
	if (it == g_sTablesFunction.end())
		return false;

	TYPE_FUN f = (*it).second;
	return (((CA*)pUserData)->*f)(pN, args);
}
bool Property(INeoVMWorker* pN, void* pUserData, const std::string& fun, VarInfo* p, bool get)
{
	auto it = g_sTablesProperty.find(fun);
	if (it == g_sTablesProperty.end())
		return false;

	TYPE_PRY f = (*it).second;
	return (((CA*)pUserData)->*f)(pN, p, get);
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
		if (g_sData != NULL && pVM->GetMainWorker()->ChangeVarType(g_sData, VAR_MAP))
		{
			if(INeoVM::RegisterTableCallBack(g_sData, pClass, Fun, Property))
			{
				g_sTablesFunction["sum"] = &CA::FunSum;
				g_sTablesFunction["mul"] = &CA::FunMul;
				g_sTablesProperty["Transform"] = &CA::PryTransform;
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
	g_sTablesFunction.clear();

    return 0;
}

