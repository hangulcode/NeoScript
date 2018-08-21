#include <math.h>
#include <stdlib.h>
#include <iostream>

#include "NeoVM.h"

#define MATH_PI				3.14159265358979323846f // Pi

bool Str_Substring(CNeoVMWorker* pN, short args)
{
	std::string* p = pN->read<std::string*>(1);
	if (p == NULL)
		return false;

	int start = pN->read<int>(2);
	int len = pN->read<int>(3);

	pN->_sTempString = p->substr(start, len);
	pN->write<char*>(pN->GetReturnVar(), (char*)pN->_sTempString.c_str());
	return true;
}
bool Str_len(CNeoVMWorker* pN, short args)
{
	std::string* p = pN->read<std::string*>(1);
	if (p == NULL)
		return false;

	pN->write<int>(pN->GetReturnVar(), (int)p->length());
	return true;
}

bool Math_abs(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::abs(v));
	return true;
}
bool Math_acos(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::acos(v));
	return true;
}
bool Math_asin(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::asin(v));
	return true;
}
bool Math_atan(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::atan(v));
	return true;
}
bool Math_ceil(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::ceil(v));
	return true;
}
bool Math_floor(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::floor(v));
	return true;
}
bool Math_sin(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::sin(v));
	return true;
}
bool Math_cos(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::cos(v));
	return true;
}
bool Math_tan(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::tan(v));
	return true;
}
bool Math_log(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::log(v));
	return true;
}
bool Math_log10(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::log10(v));
	return true;
}
bool Math_pow(CNeoVMWorker* pN, short args)
{
	double v1 = pN->read<double>(1);
	double v2 = pN->read<double>(2);
	pN->write<double>(pN->GetReturnVar(), ::pow(v1, v2));
	return true;
}
bool Math_deg(CNeoVMWorker* pN, short args)
{
	double radian = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ((radian) * (180.0f / MATH_PI)));
	return true;
}
bool Math_rad(CNeoVMWorker* pN, short args)
{
	double degree = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ((degree) * (MATH_PI / 180.0f)));
	return true;
}
bool Math_sqrt(CNeoVMWorker* pN, short args)
{
	double v = pN->read<double>(1);
	pN->write<double>(pN->GetReturnVar(), ::sqrt(v));
	return true;
}
bool	Math_rand(CNeoVMWorker* pN, short args)
{
	pN->write<int>(pN->GetReturnVar(), ::rand());
	return true;
}

bool io_print(CNeoVMWorker* pN, short args)
{
	std::string* p = pN->read<std::string*>(1);
	if (p == NULL)
		return false;

	std::cout << p->c_str();
//	pN->GetReturnVar()
	return true;
}




static SFunLib _Lib[] =
{
{ "abs", CNeoVM::RegisterNative(Math_abs, 1) },
{ "acos", CNeoVM::RegisterNative(Math_acos, 1) },
{ "asin", CNeoVM::RegisterNative(Math_asin, 1) },
{ "atan", CNeoVM::RegisterNative(Math_atan, 1) },
{ "ceil", CNeoVM::RegisterNative(Math_ceil, 1) },
{ "floor", CNeoVM::RegisterNative(Math_floor, 1) },
{ "sin", CNeoVM::RegisterNative(Math_sin, 1) },
{ "cos", CNeoVM::RegisterNative(Math_cos, 1) },
{ "tan", CNeoVM::RegisterNative(Math_tan, 1) },
{ "log", CNeoVM::RegisterNative(Math_log, 1) },
{ "log10", CNeoVM::RegisterNative(Math_log10, 1) },
{ "pow", CNeoVM::RegisterNative(Math_pow, 2) },
{ "deg", CNeoVM::RegisterNative(Math_deg, 1) },
{ "rad", CNeoVM::RegisterNative(Math_rad, 1) },
{ "sqrt", CNeoVM::RegisterNative(Math_sqrt, 1) },
{ "rand", CNeoVM::RegisterNative(Math_rand, 0) },

{ "str_sub", CNeoVM::RegisterNative(Str_Substring, 3) },
{ "str_len", CNeoVM::RegisterNative(Str_len, 1) },

{ "print", CNeoVM::RegisterNative(io_print, 1) },

{ NULL, FunctionPtrNative() },
};

void CNeoVM::RegLibrary(VarInfo* pSystem, const char* pLibName, SFunLib* pFuns)
{
	//VarInfo temp;
	//Var_SetTable(&temp, TableAlloc());
	//temp._tbl->_refCount = 1;
	//pSystem->_tbl->_strMap[pLibName] = temp;

	VarInfo fun;
	fun.SetType(VAR_PTRFUN);

	TableInfo* pTable = pSystem->_tbl;

	while (pFuns->pName != NULL)
	{
		fun._fun = pFuns->fn;
		//temp._tbl->_strMap[pFuns[i].pName] = fun;
		pTable->_strMap[pFuns->pName] = fun;
		pFuns++;
	}
}


void CNeoVM::InitLib()
{
	VarInfo* pSystem = GetVarPtr(-1);
	Var_SetTable(pSystem, TableAlloc());

	//SetTable(, TableAlloc());

	RegLibrary(pSystem, "sys", _Lib);

}

