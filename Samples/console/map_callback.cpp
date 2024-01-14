#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

VMHash<int> g_sVector3Indexer;

class CA
{
public:
	bool FunSum(INeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;

		NS_FLOAT v1, v2;
		if (pN->GetArg_Float(1, v1) == false) return false;
		if (pN->GetArg_Float(2, v2) == false) return false;

		pN->ReturnValue(v1 + v2);

		return true;
	}
	bool FunMul(INeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;

		NS_FLOAT v1, v2;
		if (pN->GetArg_Float(1, v1) == false) return false;
		if (pN->GetArg_Float(2, v2) == false) return false;

		pN->ReturnValue(v1 * v2);

		return true;
	}
	NS_FLOAT _x = 0.1f;
	NS_FLOAT _y = 1.0f;
	NS_FLOAT _z = 10.0f;
#if 1
	bool PropertyTransform(INeoVMWorker* pN, VarInfo* pVar, bool get)
	{
		if(get)
		{
			if(pN->ResetVarType(pVar, VAR_LIST, 3))
			{
				pVar->SetListIndexer(&g_sVector3Indexer);
				pVar->ListInsertFloat(0, _x);
				pVar->ListInsertFloat(1, _y);
				pVar->ListInsertFloat(2, _z);
			}
		}
		else
		{
			if(pVar->GetType() == VAR_LIST)
			{
				if (false == pVar->ListFindFloat(0, _x)) return false;
				if (false == pVar->ListFindFloat(1, _y)) return false;
				if (false == pVar->ListFindFloat(2, _z)) return false;
			}
		}
		return true;
	}
#else
	bool PropertyTransform(INeoVMWorker* pN, VarInfo* pVar, bool get)
	{
		if (get)
		{
			if (pN->ResetVarType(pVar, VAR_MAP, 3))
			{
				pVar->MapInsertFloat("x", _x);
				pVar->MapInsertFloat("y", _y);
				pVar->MapInsertFloat("z", _z);
			}
		}
		else
		{
			if (pVar->GetType() == VAR_MAP)
			{
				if (false == pVar->MapFindFloat("x", _x)) return false;
				if (false == pVar->MapFindFloat("y", _y)) return false;
				if (false == pVar->MapFindFloat("z", _z)) return false;
			}
		}
		return true;
	}
#endif
};

typedef bool (CA::*TYPE_FUN)(INeoVMWorker* pN, short args);
typedef bool (CA::* TYPE_PRY)(INeoVMWorker* pN, VarInfo* pVar, bool get);
std::map<std::string, TYPE_FUN> g_sTablesFunction;
std::map<std::string, TYPE_PRY> g_sTablesProperty;


bool Fun(INeoVMWorker* pN, void* pUserData, const VMString* pStr, short args)
{
	auto it = g_sTablesFunction.find(pStr->_str);
	if (it == g_sTablesFunction.end())
		return false;

	TYPE_FUN f = (*it).second;
	return (((CA*)pUserData)->*f)(pN, args);
}
bool Property(INeoVMWorker* pN, void* pUserData, const VMString* pStr, VarInfo* p, bool get)
{
	auto it = g_sTablesProperty.find(pStr->_str);
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

	g_sVector3Indexer.Add("x", 0);
	g_sVector3Indexer.Add("y", 1);
	g_sVector3Indexer.Add("z", 2);

	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	param.putASM = true;
	param.debug = true;

	INeoVM* pVM = INeoVM::CompileAndLoadVM(param);
	if (pVM != NULL)
	{
		CA* pClass = new CA();

		VarInfo* g_sData;
		g_sData = pVM->GetVar("g_sData");
		if (g_sData != NULL && pVM->GetMainWorker()->ResetVarType(g_sData, VAR_MAP))
		{
			if(INeoVM::RegisterTableCallBack(g_sData, pClass, Fun, Property))
			{
				g_sTablesFunction["sum"] = &CA::FunSum;
				g_sTablesFunction["mul"] = &CA::FunMul;
				g_sTablesProperty["Transform"] = &CA::PropertyTransform;
			}
		}

		DWORD t1 = GetTickCount();
		NS_FLOAT r;
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
	FileUnoad(nullptr, pFileBuffer, iFileLen);
	g_sTablesFunction.clear();

    return 0;
}

