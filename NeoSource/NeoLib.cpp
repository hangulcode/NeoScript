#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include "NeoVM.h"

#define MATH_PI				3.14159265358979323846f // Pi




void NVM_QuickSort(CNeoVMWorker* pN, int compare, std::vector<VarInfo*>& lst);


struct neo_libs
{
	static bool Str_sub(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 2) return false;

		std::string* p = &pVar->_str->_str;
		int p1 = pN->read<int>(1);
		int p2 = pN->read<int>(2);

		if (p1 < 0 || p1 >= (int)p->length()) return false;

		std::string sTempString = p->substr(p1, p2);
		pN->ReturnValue(sTempString.c_str());
		return true;
	}
	static bool Str_len(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string* p = &pVar->_str->_str;
		pN->ReturnValue((int)p->length());
		return true;
	}
	static bool Str_find(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 1) return false;

		std::string* p = &pVar->_str->_str;
		std::string* p2 = pN->read<std::string*>(1);
		if (p2 == NULL) return false;

		int iFind = (int)p->find(*p2);
		pN->ReturnValue((int)iFind);
		return true;
	}
	static bool Str_upper(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string str = pVar->_str->_str;
		std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_lower(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string str = pVar->_str->_str;
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_trim(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string drop = " ";
		std::string str = pVar->_str->_str;
		str = str.erase(str.find_last_not_of(drop) + 1);
		str = str.erase(0, str.find_first_not_of(drop));
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_ltrim(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string drop = " ";
		std::string str = pVar->_str->_str;
		str = str.erase(0, str.find_first_not_of(drop));
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_rtrim(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string drop = " ";
		std::string str = pVar->_str->_str;
		str = str.erase(str.find_last_not_of(drop) + 1);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_replace(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 2) return false;

		VarInfo *pFind = pN->GetStack(1);
		VarInfo *pReplace = pN->GetStack(2);
		if (pFind->GetType() != VAR_STRING) return false;
		if (pReplace->GetType() != VAR_STRING) return false;

		std::string str = pVar->_str->_str;
		str.replace(str.find(pFind->_str->_str), pFind->_str->_str.length(), pReplace->_str->_str);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_split(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 1) return false;

		VarInfo *pFind = pN->GetStack(1);
		if (pFind->GetType() != VAR_STRING) return false;
		std::string& findstr = pFind->_str->_str;
		std::string str = pVar->_str->_str;

		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pListR = pN->_pVM->ListAlloc();
		pN->Var_SetList(pRet, pListR); // Set Return Value

		size_t previous = 0, current;
		current = str.find(findstr);
		while (current != std::string::npos)
		{
			std::string substring = str.substr(previous, current - previous);
			pListR->InsertLast(substring);

			previous = current + 1;
			current = str.find(findstr, previous);
		}
		pListR->InsertLast(str.substr(previous, current - previous)); // Last
		return true;
	}


	static bool List_resize(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 1) return false;

		int size = pN->read<int>(1);
		pVar->_lst->Resize(size);
		pN->ReturnValue();
		return true;
	}
	static bool List_len(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 0) return false;

		pN->ReturnValue((int)pVar->_lst->GetCount());
		return true;
	}

	static bool Math_abs(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::abs(v));
		return true;
	}
	static bool Math_acos(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::acos(v));
		return true;
	}
	static bool Math_asin(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::asin(v));
		return true;
	}
	static bool Math_atan(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::atan(v));
		return true;
	}
	static bool Math_ceil(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::ceil(v));
		return true;
	}
	static bool Math_floor(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::floor(v));
		return true;
	}
	static bool Math_sin(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::sin(v));
		return true;
	}
	static bool Math_cos(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::cos(v));
		return true;
	}
	static bool Math_tan(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::tan(v));
		return true;
	}
	static bool Math_log(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::log(v));
		return true;
	}
	static bool Math_log10(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::log10(v));
		return true;
	}
	static bool Math_pow(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		double v1 = pN->read<double>(1);
		double v2 = pN->read<double>(2);
		pN->ReturnValue(::pow(v1, v2));
		return true;
	}
	static bool Math_deg(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double radian = pN->read<double>(1);
		pN->ReturnValue(((radian) * (180.0f / MATH_PI)));
		return true;
	}
	static bool Math_rad(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double degree = pN->read<double>(1);
		pN->ReturnValue(((degree) * (MATH_PI / 180.0f)));
		return true;
	}
	static bool Math_sqrt(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		double v = pN->read<double>(1);
		pN->ReturnValue(::sqrt(v));
		return true;
	}
	static bool	Math_srand(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		int init = pN->read<int>(1);
		::srand((u32)init);
		pN->ReturnValue();
		return true;
	}
	static bool	Math_rand(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		pN->ReturnValue((int)::rand());
		return true;
	}

	static bool util_meta(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false; // table, meta

		VarInfo *pTable = pN->GetStack(1);
		VarInfo *pMeta = pN->GetStack(2);
		if (pTable->GetType() != VAR_TABLE) return false;
		if (pMeta->GetType() != VAR_TABLE) return false;

		VarInfo var;
		var.ClearType();
		if (pTable->_tbl->_meta)
		{
			var.SetType(VAR_TABLE);
			var._tbl = pTable->_tbl->_meta;
		}

		pN->Move(&var, pMeta); // for Referance
		pTable->_tbl->_meta = pMeta->_tbl;
		pN->ReturnValue();
		return true;
	}
	static bool alg_sort(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false; // fun

		VarInfo *pFun = pN->GetStack(1);
		if (pVar->GetType() != VAR_TABLE) return false;

		if (pFun->GetType() != VAR_FUN) return false;

		std::vector<VarInfo*> lst;
		if (false == pVar->_tbl->ToList(lst)) return false;

		if (lst.size() >= 2)
		{
			std::vector<VarInfo*> lstSorted = lst;
			NVM_QuickSort(pN, pFun->_fun_index, lstSorted);

			std::vector<VarInfo> lst3;
			lst3.resize(lstSorted.size());
			for (int i = 0; i < lstSorted.size(); i++)
				lst3[i] = *lstSorted[i];

			for (int i = 0; i < lst.size(); i++)
				*lst[i] = lst3[i];
		}
		pN->ReturnValue();
		return true;
	}

	static bool io_print(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args == 1)
		{
			VarInfo* pArg1 = pN->GetStack(1);
			std::string str = CNeoVMWorker::ToString(pArg1);
			std::cout << str.c_str() << '\n';
			pN->ReturnValue();
			return true;
		}
		else if (args == 2)
		{
			VarInfo* pArg1 = pN->GetStack(1);
			VarInfo* pArg2 = pN->GetStack(2);
			std::string str1 = CNeoVMWorker::ToString(pArg1);
			std::string str2 = CNeoVMWorker::ToString(pArg2);
			std::cout << str1.c_str() << str2.c_str();
			pN->ReturnValue();
			return true;
		}
		return false;
	}

	static bool sys_clock(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		pN->ReturnValue(double((double)clock() / (double)CLOCKS_PER_SEC));
		return true;
	}
	static bool sys_coroutine_create(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_FUN)
			return false;

		CNeoVM* pVM = pN->GetVM();
		CoroutineInfo* pCI = pVM->CoroutineAlloc();
		pCI->_refCount = 0;
		pCI->_fun_index = v->_fun_index;
		pCI->_state = COROUTINE_STATE_SUSPENDED;

		pN->ReturnValue(pCI);
		return true;
	}
	static bool sys_coroutine_resume(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args < 1) return false; // param : index 1 ~ 

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_COROUTINE) return false;

		if (v->_cor->_state != COROUTINE_STATE_SUSPENDED) return false;

		pN->m_pRegisterActive = v->_cor;
		pN->ReturnValue();
		return true;
	}
	static bool sys_coroutine_status(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_COROUTINE) return false;

		CNeoVM* pVM = pN->GetVM();
		switch (v->_cor->_state)
		{
		case COROUTINE_STATE_SUSPENDED:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_SUSPENDED]);
			break;
		case COROUTINE_STATE_RUNNING:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_RUNNING]);
			break;
		case COROUTINE_STATE_DEAD:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_DEAD]);
			break;
		case COROUTINE_STATE_NORMAL:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_NORMAL]);
			break;
		default:
			pN->ReturnValue();
			return false;
		}

		return true;
	}

	static bool set(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* pRet = pN->GetStack(0);
		VarInfo* pArg1 = pN->GetStack(1);
		switch(pArg1->GetType())
		{
		case VAR_LIST:
			{
				SetInfo* pSetR = pN->_pVM->SetAlloc();
				pN->Var_SetSet(pRet, pSetR);

				ListInfo* pListV1 = pArg1->_lst;
				int sz = pListV1->GetCount();
				VarInfo* src = pListV1->GetDataUnsafe();
				for (int i = 0; i < sz; i++)
				{
					pSetR->Insert(&src[i]);
				}
				return true;
			}
		default:
			return false;
		}
		pN->ReturnValue();
		return false;
	}
};



//typedef bool (ClassName::*TYPE_NeoLib)(CNeoVMWorker* pN, short args);
//typedef bool(*TYPE_NeoLib)(CNeoVMWorker* pN, short args);
typedef bool(*TYPE_NeoLib)(CNeoVMWorker* pN, VarInfo* pVar, short args);
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLstLib;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunStrLib;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunTblLib;


static bool Fun(CNeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib.find(fun);
	if (it == g_sNeoFunLib.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)(pN, (VarInfo*)pUserData, args);
}
static bool FunStr(CNeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunStrLib.find(fun);
	if (it == g_sNeoFunStrLib.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)(pN, (VarInfo*)pUserData, args);
}
static bool FunLst(CNeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLstLib.find(fun);
	if (it == g_sNeoFunLstLib.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)(pN, (VarInfo*)pUserData, args);
}
static bool FunTbl(CNeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunTblLib.find(fun);
	if (it == g_sNeoFunTblLib.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)(pN, (VarInfo*)pUserData, args);
}
static void AddGlobalLibFun()
{
	if (g_sNeoFunLib.empty() == false)
		return;

	g_sNeoFunLib["abs"] = &neo_libs::Math_abs;
	g_sNeoFunLib["acos"] = &neo_libs::Math_acos;
	g_sNeoFunLib["asin"] = &neo_libs::Math_asin;
	g_sNeoFunLib["atan"] = &neo_libs::Math_atan;
	g_sNeoFunLib["ceil"] = &neo_libs::Math_ceil;
	g_sNeoFunLib["floor"] = &neo_libs::Math_floor;
	g_sNeoFunLib["sin"] = &neo_libs::Math_sin;
	g_sNeoFunLib["cos"] = &neo_libs::Math_cos;
	g_sNeoFunLib["tan"] = &neo_libs::Math_tan;
	g_sNeoFunLib["log"] = &neo_libs::Math_log;
	g_sNeoFunLib["log10"] = &neo_libs::Math_log10;
	g_sNeoFunLib["pow"] = &neo_libs::Math_pow;
	g_sNeoFunLib["deg"] = &neo_libs::Math_deg;
	g_sNeoFunLib["rad"] = &neo_libs::Math_rad;
	g_sNeoFunLib["sqrt"] = &neo_libs::Math_sqrt;
	g_sNeoFunLib["srand"] = &neo_libs::Math_srand;
	g_sNeoFunLib["rand"] = &neo_libs::Math_rand;

	g_sNeoFunLib["meta"] = &neo_libs::util_meta;
	g_sNeoFunLib["print"] = &neo_libs::io_print;
	g_sNeoFunLib["clock"] = &neo_libs::sys_clock;

	g_sNeoFunLib["set"] = &neo_libs::set;

	g_sNeoFunLib["coroutine_create"] = &neo_libs::sys_coroutine_create;
	g_sNeoFunLib["coroutine_resume"] = &neo_libs::sys_coroutine_resume;
	g_sNeoFunLib["coroutine_status"] = &neo_libs::sys_coroutine_status;
}
bool CNeoVM::IsGlobalLibFun(std::string& FunName)
{
	AddGlobalLibFun();
	auto it = g_sNeoFunLib.find(FunName);
	if(it == g_sNeoFunLib.end())
		return false;
	return true;
}
void CNeoVM::RegLibrary(VarInfo* pSystem, const char* pLibName)
{
	TableInfo* pTable = pSystem->_tbl;
	pTable->_fun = CNeoVM::RegisterNative(Fun);
	AddGlobalLibFun();

	_funLib = CNeoVM::RegisterNative(Fun);
}

void CNeoVM::RegObjLibrary()
{
	_funStrLib = CNeoVM::RegisterNative(FunStr);
	g_sNeoFunStrLib["sub"] = &neo_libs::Str_sub;
	g_sNeoFunStrLib["len"] = &neo_libs::Str_len;
	g_sNeoFunStrLib["find"] = &neo_libs::Str_find;
	g_sNeoFunStrLib["upper"] = &neo_libs::Str_upper;
	g_sNeoFunStrLib["lower"] = &neo_libs::Str_lower;
	g_sNeoFunStrLib["trim"] = &neo_libs::Str_trim;
	g_sNeoFunStrLib["ltrim"] = &neo_libs::Str_ltrim;
	g_sNeoFunStrLib["rtrim"] = &neo_libs::Str_rtrim;
	g_sNeoFunStrLib["replace"] = &neo_libs::Str_replace;
	g_sNeoFunStrLib["split"] = &neo_libs::Str_split;

	_funLstLib = CNeoVM::RegisterNative(FunLst);
	g_sNeoFunLstLib["resize"] = &neo_libs::List_resize;
	g_sNeoFunLstLib["len"] = &neo_libs::List_len;

	_funTblLib = CNeoVM::RegisterNative(FunTbl);
	g_sNeoFunTblLib["sort"] = &neo_libs::alg_sort;
}

void CNeoVM::InitLib()
{
	//VarInfo* pSystem = GetVarPtr(-1);
	VarInfo* pSystem = &m_sVarGlobal[0];
	Var_SetTable(pSystem, TableAlloc());

	//SetTable(, TableAlloc());

	RegLibrary(pSystem, "sys");
	RegObjLibrary();
}

