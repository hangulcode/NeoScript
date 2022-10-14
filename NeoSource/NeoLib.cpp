#include <math.h>
#include <stdlib.h>
#include <iostream>

#include "NeoVM.h"

#define MATH_PI				3.14159265358979323846f // Pi

bool Str_Substring(CNeoVMWorker* pN, short args)
{
	if (args != 3)
		return false;
	std::string* p = pN->read<std::string*>(1);
	if (p == NULL)
		return false;

	int start = pN->read<int>(2);
	int len = pN->read<int>(3);

	pN->_sTempString = p->substr(start, len);
	pN->ReturnValue((char*)pN->_sTempString.c_str());
	return true;
}
bool Str_len(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	std::string* p = pN->read<std::string*>(1);
	if (p == NULL)
		return false;

	pN->ReturnValue((int)p->length());
	return true;
}
bool Str_find(CNeoVMWorker* pN, short args)
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

bool Math_abs(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::abs(v));
	return true;
}
bool Math_acos(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::acos(v));
	return true;
}
bool Math_asin(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::asin(v));
	return true;
}
bool Math_atan(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::atan(v));
	return true;
}
bool Math_ceil(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::ceil(v));
	return true;
}
bool Math_floor(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::floor(v));
	return true;
}
bool Math_sin(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::sin(v));
	return true;
}
bool Math_cos(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::cos(v));
	return true;
}
bool Math_tan(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::tan(v));
	return true;
}
bool Math_log(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::log(v));
	return true;
}
bool Math_log10(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::log10(v));
	return true;
}
bool Math_pow(CNeoVMWorker* pN, short args)
{
	if (args != 2)
		return false;
	double v1 = pN->read<double>(1);
	double v2 = pN->read<double>(2);
	pN->ReturnValue(::pow(v1, v2));
	return true;
}
bool Math_deg(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double radian = pN->read<double>(1);
	pN->ReturnValue(((radian) * (180.0f / MATH_PI)));
	return true;
}
bool Math_rad(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double degree = pN->read<double>(1);
	pN->ReturnValue(((degree) * (MATH_PI / 180.0f)));
	return true;
}
bool Math_sqrt(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	double v = pN->read<double>(1);
	pN->ReturnValue(::sqrt(v));
	return true;
}
bool	Math_srand(CNeoVMWorker* pN, short args)
{
	if (args != 1)
		return false;
	int init = pN->read<int>(1);
	::srand((u32)init);
	return true;
}
bool	Math_rand(CNeoVMWorker* pN, short args)
{
	if (args != 0)
		return false;
	pN->ReturnValue((int)::rand());
	return true;
}

bool util_meta(CNeoVMWorker* pN, short args)
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


void quickSort(CNeoVMWorker* pN, int compare, std::vector<VarInfo*>& lst);
bool alg_sort(CNeoVMWorker* pN, short args)
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
	if(false == pTable->_tbl->ToList(lst))
		return false;
	if (lst.size() >= 2)
	{
		std::vector<VarInfo*> lstSorted = lst;
		quickSort(pN, pFun->_fun_index, lstSorted);

		std::vector<VarInfo> lst3;
		lst3.resize(lstSorted.size());
		for (int i = 0; i < lstSorted.size(); i++)
			lst3[i] = *lstSorted[i];

		for (int i = 0; i < lst.size(); i++)
			*lst[i] = lst3[i];
	}
	return true;
}

bool io_print(CNeoVMWorker* pN, short args)
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

bool sys_clock(CNeoVMWorker* pN, short args)
{
	if (args != 0)
		return false;

	pN->ReturnValue(double((double)clock() / (double)CLOCKS_PER_SEC));
	return true;
}



static SNeoFunLib _Lib[] =
{
{ "abs", CNeoVM::RegisterNative(Math_abs) },
{ "acos", CNeoVM::RegisterNative(Math_acos) },
{ "asin", CNeoVM::RegisterNative(Math_asin) },
{ "atan", CNeoVM::RegisterNative(Math_atan) },
{ "ceil", CNeoVM::RegisterNative(Math_ceil) },
{ "floor", CNeoVM::RegisterNative(Math_floor) },
{ "sin", CNeoVM::RegisterNative(Math_sin) },
{ "cos", CNeoVM::RegisterNative(Math_cos) },
{ "tan", CNeoVM::RegisterNative(Math_tan) },
{ "log", CNeoVM::RegisterNative(Math_log) },
{ "log10", CNeoVM::RegisterNative(Math_log10) },
{ "pow", CNeoVM::RegisterNative(Math_pow) },
{ "deg", CNeoVM::RegisterNative(Math_deg) },
{ "rad", CNeoVM::RegisterNative(Math_rad) },
{ "sqrt", CNeoVM::RegisterNative(Math_sqrt) },
{ "srand", CNeoVM::RegisterNative(Math_srand) },
{ "rand", CNeoVM::RegisterNative(Math_rand) },

{ "str_sub", CNeoVM::RegisterNative(Str_Substring) },
{ "str_len", CNeoVM::RegisterNative(Str_len) },
{ "str_find", CNeoVM::RegisterNative(Str_find) },

{ "sort", CNeoVM::RegisterNative(alg_sort) },
{ "meta", CNeoVM::RegisterNative(util_meta) },
{ "print", CNeoVM::RegisterNative(io_print) },
{ "clock", CNeoVM::RegisterNative(sys_clock) },

{ "", FunctionPtrNative() },
};

void CNeoVM::RegLibrary(VarInfo* pSystem, const char* pLibName, SNeoFunLib* pFuns)
{
	//VarInfo temp;
	//Var_SetTable(&temp, TableAlloc());
	//temp._tbl->_refCount = 1;
	//pSystem->_tbl->_strMap[pLibName] = temp;

	VarInfo value;
	value.SetType(VAR_TABLEFUN);
	//VarInfo fun;
	//fun.SetType(VAR_TABLEFUN);

	TableInfo* pTable = pSystem->_tbl;

	while (pFuns->pName.empty() == false)
	{
		value._fun = pFuns->fn;
		//temp._tbl->_strMap[pFuns[i].pName] = fun;
		pTable->Insert(this, pFuns->pName, &value);
		pFuns++;
	}
}


void CNeoVM::InitLib()
{
	//VarInfo* pSystem = GetVarPtr(-1);
	VarInfo* pSystem = &m_sVarGlobal[0];
	Var_SetTable(pSystem, TableAlloc());

	//SetTable(, TableAlloc());

	RegLibrary(pSystem, "sys", _Lib);

}

