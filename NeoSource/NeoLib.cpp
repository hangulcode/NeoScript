#include <math.h>
#include <stdlib.h>
#include <iostream>

#include "NeoVM.h"

#define MATH_PI				3.14159265358979323846f // Pi




void NVM_QuickSort(CNeoVMWorker* pN, int compare, std::vector<VarInfo*>& lst);


struct neo_libs
{
	static bool Str_Substring(CNeoVMWorker* pN, short args)
	{
		if (args != 3)
			return false;
		std::string* p = pN->read<std::string*>(1);
		if (p == NULL)
			return false;

		int start = pN->read<int>(2);
		int len = pN->read<int>(3);

		std::string sTempString = p->substr(start, len);
		pN->ReturnValue(sTempString.c_str());
		return true;
	}
	static bool Str_len(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		std::string* p = pN->read<std::string*>(1);
		if (p == NULL)
			return false;

		pN->ReturnValue((int)p->length());
		return true;
	}
	static bool Str_find(CNeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;
		std::string* p = pN->read<std::string*>(1);
		if (p == NULL)
			return false;

		std::string* p2 = pN->read<std::string*>(2);
		if (p2 == NULL)
			return false;

		int iFind = (int)p->find(*p2);

		pN->ReturnValue((int)iFind);
		return true;
	}

	static bool Math_abs(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::abs(v));
		return true;
	}
	static bool Math_acos(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::acos(v));
		return true;
	}
	static bool Math_asin(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::asin(v));
		return true;
	}
	static bool Math_atan(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::atan(v));
		return true;
	}
	static bool Math_ceil(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::ceil(v));
		return true;
	}
	static bool Math_floor(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::floor(v));
		return true;
	}
	static bool Math_sin(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::sin(v));
		return true;
	}
	static bool Math_cos(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::cos(v));
		return true;
	}
	static bool Math_tan(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::tan(v));
		return true;
	}
	static bool Math_log(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::log(v));
		return true;
	}
	static bool Math_log10(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::log10(v));
		return true;
	}
	static bool Math_pow(CNeoVMWorker* pN, short args)
	{
		if (args != 2)
			return false;
		double v1 = pN->read<double>(1);
		double v2 = pN->read<double>(2);
		pN->ReturnValue(::pow(v1, v2));
		return true;
	}
	static bool Math_deg(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double radian = pN->read<double>(1);
		pN->ReturnValue(((radian) * (180.0f / MATH_PI)));
		return true;
	}
	static bool Math_rad(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double degree = pN->read<double>(1);
		pN->ReturnValue(((degree) * (MATH_PI / 180.0f)));
		return true;
	}
	static bool Math_sqrt(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		double v = pN->read<double>(1);
		pN->ReturnValue(::sqrt(v));
		return true;
	}
	static bool	Math_srand(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		int init = pN->read<int>(1);
		::srand((u32)init);
		return true;
	}
	static bool	Math_rand(CNeoVMWorker* pN, short args)
	{
		if (args != 0)
			return false;
		pN->ReturnValue((int)::rand());
		return true;
	}

	static bool util_meta(CNeoVMWorker* pN, short args)
	{
		if (args != 2) // table, meta
			return false;
		VarInfo *pTable = pN->GetStack(1);
		VarInfo *pMeta = pN->GetStack(2);

		if (pTable->GetType() != VAR_TABLE)
			return false;

		if (pMeta->GetType() != VAR_TABLE)
			return false;

		VarInfo var;
		var.ClearType();
		if (pTable->_tbl->_meta)
		{
			var.SetType(VAR_TABLE);
			var._tbl = pTable->_tbl->_meta;
		}

		pN->Move(&var, pMeta); // for Referance
		pTable->_tbl->_meta = pMeta->_tbl;

		return true;
	}
	static bool alg_sort(CNeoVMWorker* pN, short args)
	{
		if (args != 2) // table, fun
			return false;
		VarInfo *pTable = pN->GetStack(1);
		VarInfo *pFun = pN->GetStack(2);

		if (pTable->GetType() != VAR_TABLE)
			return false;

		if (pFun->GetType() != VAR_FUN)
			return false;

		std::vector<VarInfo*> lst;
		if (false == pTable->_tbl->ToList(lst))
			return false;
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
		return true;
	}

	static bool io_print(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		std::string* p = pN->read<std::string*>(1);
		if (p == NULL)
			return false;

		std::cout << p->c_str();
		pN->ReturnValue();
		return true;
	}

	static bool sys_clock(CNeoVMWorker* pN, short args)
	{
		if (args != 0)
			return false;

		pN->ReturnValue(double((double)clock() / (double)CLOCKS_PER_SEC));
		return true;
	}
	static bool sys_coroutine_create(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_FUN)
			return false;

		CNeoVM* pVM = pN->GetVM();
		//pVM->CreateWorker();
		//pVM->Coroutine_Create(v->_fun_index);

		CoroutineInfo* pCI = pVM->CoroutineAlloc();
		pCI->_fun_index = v->_fun_index;
		pCI->_state = COROUTINE_STATE_SUSPENDED;

		pN->ReturnValue(pCI);
		return true;
	}
	static bool sys_coroutine_resume(CNeoVMWorker* pN, short args)
	{
		if (args < 1) // param : index 1 ~ 
			return false;
		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_COROUTINE)
			return false;

		if (v->_cor->_state != COROUTINE_STATE_SUSPENDED)
			return false;

		pN->m_pRegisterActive = v->_cor;
		pN->ReturnValue();
		return true;
	}
	static bool sys_coroutine_status(CNeoVMWorker* pN, short args)
	{
		if (args != 1)
			return false;
		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_COROUTINE)
			return false;

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
};



//typedef bool (ClassName::*TYPE_NeoLib)(CNeoVMWorker* pN, short args);
typedef bool(*TYPE_NeoLib)(CNeoVMWorker* pN, short args);
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib;


static bool Fun(CNeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib.find(fun);
	if (it == g_sNeoFunLib.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)(pN, args);
}

void CNeoVM::RegLibrary(VarInfo* pSystem, const char* pLibName)
{
	TableInfo* pTable = pSystem->_tbl;
	pTable->_fun = CNeoVM::RegisterNative(Fun);

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

	g_sNeoFunLib["str_sub"] = &neo_libs::Str_Substring;
	g_sNeoFunLib["str_len"] = &neo_libs::Str_len;
	g_sNeoFunLib["str_find"] = &neo_libs::Str_find;

	g_sNeoFunLib["sort"] = &neo_libs::alg_sort;
	g_sNeoFunLib["meta"] = &neo_libs::util_meta;
	g_sNeoFunLib["print"] = &neo_libs::io_print;
	g_sNeoFunLib["clock"] = &neo_libs::sys_clock;

	g_sNeoFunLib["coroutine_create"] = &neo_libs::sys_coroutine_create;
	g_sNeoFunLib["coroutine_resume"] = &neo_libs::sys_coroutine_resume;
	g_sNeoFunLib["coroutine_status"] = &neo_libs::sys_coroutine_status;
}


void CNeoVM::InitLib()
{
	//VarInfo* pSystem = GetVarPtr(-1);
	VarInfo* pSystem = &m_sVarGlobal[0];
	Var_SetTable(pSystem, TableAlloc());

	//SetTable(, TableAlloc());

	RegLibrary(pSystem, "sys");
}

