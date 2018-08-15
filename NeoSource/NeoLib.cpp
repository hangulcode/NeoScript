#include <math.h>
#include <stdlib.h>
#include <iostream>

#include "NeoVM.h"

#define MATH_PI				3.14159265358979323846f // Pi

std::string str2;
const char* Str_Substring(char* p, int start, int len)
{
	std::string str(p);
	str2 = str.substr(start, len);
	return str2.c_str();
}
int Str_len(char* p)
{
	return (int)strlen(p);
}

double Math_abs(double v)
{
	return ::abs(v);
}
double Math_acos(double v)
{
	return ::acos(v);
}
double Math_asin(double v)
{
	return ::asin(v);
}
double Math_atan(double v)
{
	return ::atan(v);
}
double Math_ceil(double v)
{
	return ::ceil(v);
}
double Math_floor(double v)
{
	return ::floor(v);
}
double Math_sin(double v)
{
	return ::sin(v);
}
double Math_cos(double v)
{
	return ::cos(v);
}
double Math_tan(double v)
{
	return ::tan(v);
}
double Math_log(double v)
{
	return ::log(v);
}
double Math_log10(double v)
{
	return ::log10(v);
}
double Math_pow(double v1, double v2)
{
	return ::pow(v1, v2);
}
double Math_deg(double radian)
{
	return ((radian) * (180.0f / MATH_PI));
}
double Math_rad(double degree)
{
	return ((degree) * (MATH_PI / 180.0f));
}
double Math_sqrt(double v)
{
	return ::sqrt(v);
}
int	Math_rand()
{
	return ::rand();
}

void io_print(const char* p)
{
	std::cout << p;
}




static SFunLib _Lib[] =
{
{ "abs", CNeoVM::Register(Math_abs) },
{ "acos", CNeoVM::Register(Math_acos) },
{ "asin", CNeoVM::Register(Math_asin) },
{ "atan", CNeoVM::Register(Math_atan) },
{ "ceil", CNeoVM::Register(Math_ceil) },
{ "floor", CNeoVM::Register(Math_floor) },
{ "sin", CNeoVM::Register(Math_sin) },
{ "cos", CNeoVM::Register(Math_cos) },
{ "tan", CNeoVM::Register(Math_tan) },
{ "log", CNeoVM::Register(Math_log) },
{ "log10", CNeoVM::Register(Math_log10) },
{ "pow", CNeoVM::Register(Math_pow) },
{ "deg", CNeoVM::Register(Math_deg) },
{ "rad", CNeoVM::Register(Math_rad) },
{ "sqrt", CNeoVM::Register(Math_sqrt) },
{ "rand", CNeoVM::Register(Math_rand) },

{ "str_sub", CNeoVM::Register(Str_Substring) },
{ "str_len", CNeoVM::Register(Str_len) },

{ "print", CNeoVM::Register(io_print) },

{ NULL, FunctionPtr() },
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


void CNeoVM::Init()
{
	VarInfo* pSystem = GetVarPtr(-1);
	Var_SetTable(pSystem, TableAlloc());
	pSystem->_tbl->_refCount = 1;

	//SetTable(, TableAlloc());

	RegLibrary(pSystem, "sys", _Lib);

	Run(0);
}