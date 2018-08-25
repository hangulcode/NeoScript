#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"

void	DebugLog(const char*	lpszString, ...);

void CNeoVMWorker::Var_AddRef(VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		++d->_str->_refCount;
		break;
	case VAR_TABLE:
		++d->_tbl->_refCount;
		break;
	}
}
void CNeoVMWorker::Var_Release(VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			_pVM->FreeString(d);
		d->_str = NULL;
		break;
	case VAR_TABLE:
		if (--d->_tbl->_refCount <= 0)
			_pVM->FreeTable(d);
		d->_tbl = NULL;
		break;
	}
	d->ClearType();
}

void CNeoVMWorker::Var_SetInt(VarInfo *d, int v)
{
	if (d->GetType() != VAR_INT)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_INT);
	}
	d->_int = v;
}

void CNeoVMWorker::Var_SetFloat(VarInfo *d, double v)
{
	if (d->GetType() != VAR_FLOAT)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_FLOAT);
	}
	d->_float = v;
}
void CNeoVMWorker::Var_SetBool(VarInfo *d, bool v)
{
	if (d->GetType() != VAR_BOOL)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_BOOL);
	}
	d->_bl = v;
}

void CNeoVMWorker::Var_SetString(VarInfo *d, const char* str)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = _pVM->StringAlloc(str);
	++d->_str->_refCount;
}
void CNeoVMWorker::Var_SetTable(VarInfo *d, TableInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_TABLE);
	d->_tbl = p;
	++d->_tbl->_refCount;
}


void CNeoVMWorker::TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
{
	if (pTable->GetType() != VAR_TABLE)
	{
		SetError("TableInsert Error");
		return;
	}

	if (pValue->GetType() == VAR_NONE)
	{
		TableRemove(pTable, pArray);
		return;
	}

	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			pTable->_tbl->_intMap[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			pTable->_tbl->_intMap[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			pTable->_tbl->_strMap[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	}
}
FunctionPtrNative* CNeoVMWorker::GetPtrFunction(VarInfo *pTable, VarInfo *pArray)
{
	if (pTable->GetType() != VAR_TABLE)
		return NULL;

	VarInfo* pValue;
	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = (int)pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			pValue = &(*it).second;
			if (pValue->GetType() == VAR_PTRFUN)
				return &pValue->_fun;
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			pValue = &(*it).second;
			if (pValue->GetType() == VAR_PTRFUN)
				return &pValue->_fun;
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			pValue = &(*it).second;
			if (pValue->GetType() == VAR_PTRFUN)
				return &pValue->_fun;
		}
		break;
	}
	}
	return NULL;
}
void CNeoVMWorker::TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
{
	if (pTable->GetType() != VAR_TABLE)
	{
		SetError("TableRead Error");
		return;
	}

	Var_Release(pValue);
	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = (int)pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	}
}

void CNeoVMWorker::TableRemove(VarInfo *pTable, VarInfo *pArray)
{
	if (pTable->GetType() != VAR_TABLE)
	{
		SetError("TableRead Error");
		return;
	}

	VarInfo *pValue;
	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = (int)pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			pValue = &(*it).second;
			Var_Release(pValue);
			pTable->_tbl->_intMap.erase(it);
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			pValue = &(*it).second;
			Var_Release(pValue);
			pTable->_tbl->_intMap.erase(it);
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			pValue = &(*it).second;
			Var_Release(pValue);
			pTable->_tbl->_strMap.erase(it);
		}
		break;
	}
	}
}

void CNeoVMWorker::Move(VarInfo* v1, VarInfo* v2)
{
	switch (v2->GetType())
	{
	case VAR_BOOL:
		Var_SetBool(v1, v2->_bl);
		break;
	case VAR_INT:
		Var_SetInt(v1, v2->_int);
		break;
	case VAR_FLOAT:
		Var_SetFloat(v1, v2->_float);
		break;
	case VAR_STRING:
		Var_Release(v1);
		v1->SetType(v2->GetType());
		v1->_str = v2->_str;
		++v1->_str->_refCount;
		break;
	case VAR_TABLE:
		Var_Release(v1);
		v1->SetType(v2->GetType());
		v1->_tbl = v2->_tbl;
		++v1->_tbl->_refCount;
		break;
	}
}
void CNeoVMWorker::MoveMinus(VarInfo* v1, VarInfo* v2)
{
	switch (v2->GetType())
	{
	case VAR_INT:
		Var_SetInt(v1, -v2->_int);
		return;
	case VAR_FLOAT:
		Var_SetFloat(v1, -v2->_float);
		return;
	}
	SetError("Minus Error");
}
void CNeoVMWorker::Add2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_int += v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int + v2->_float;
			return;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_float += v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->_float += v2->_float;
			return;
		}
	}
	else if (r->GetType() == VAR_STRING)
	{
		if (v2->GetType() == VAR_STRING)
		{
			Var_SetString(r, (r->_str->_str + v2->_str->_str).c_str());
			return;
		}
	}
	SetError("+= Error");
}

void CNeoVMWorker::Sub2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_int -= v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int - v2->_float;
			return;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_float -= v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->_float -= v2->_float;
			return;
		}
	}
	SetError("-= Error");
}

void CNeoVMWorker::Mul2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_int *= v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int * v2->_float;
			return;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_float *= v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->_float *= v2->_float;
			return;
		}
	}
	SetError("*= Error");
}

void CNeoVMWorker::Div2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_int /= v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int / v2->_float;
			return;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_float /= v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->_float /= v2->_float;
			return;
		}
	}
	SetError("/= Error");
}
void CNeoVMWorker::Per2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			r->_int %= v2->_int;
			return;
		}
	}
	SetError("%= Error");
}
void CNeoVMWorker::Add(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int + v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_int + v2->_float);
			return;
		}
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetFloat(r, v1->_float + v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_float + v2->_float);
			return;
		}
	}
	else if (v1->GetType() == VAR_STRING)
	{
		if (v2->GetType() == VAR_STRING)
		{
			Var_SetString(r, (r->_str->_str + v2->_str->_str).c_str());
			return;
		}
	}
	SetError("+ Error");
}

void CNeoVMWorker::Sub(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int - v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_int - v2->_float);
			return;
		}
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetFloat(r, v1->_float - v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_float - v2->_float);
			return;
		}
	}
	SetError("- Error");
}

void CNeoVMWorker::Mul(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int * v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_int * v2->_float);
			return;
		}
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetFloat(r, v1->_float * v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_float * v2->_float);
			return;
		}
	}
	SetError("* Error");
}

void CNeoVMWorker::Div(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int / v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_int / v2->_float);
			return;
		}
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetFloat(r, v1->_float / v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			Var_SetFloat(r, v1->_float / v2->_float);
			return;
		}
	}
	SetError("/ Error");
}
void CNeoVMWorker::Per(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int % v2->_int);
			return;
		}
	}
	SetError("/ Error");
}
void CNeoVMWorker::Inc(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		++v1->_int;
		return;
	case VAR_FLOAT:
		++v1->_float;
		return;
	}
	SetError("++ Error");
}
void CNeoVMWorker::Dec(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		--v1->_int;
		return;
	case VAR_FLOAT:
		--v1->_float;
		return;
	}
	SetError("-- Error");
}

bool CNeoVMWorker::CompareEQ(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_int == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int == v2->_float;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float == v2->_float;
	}
	else if (v1->GetType() == VAR_BOOL)
	{
		if (v2->GetType() == VAR_BOOL)
			return v1->_bl == v2->_bl;
	}
	else if (v1->GetType() == VAR_NONE)
	{
		if (v2->GetType() == VAR_NONE)
			return true;
	}
	return false;
}
bool CNeoVMWorker::CompareGR(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_int > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int > v2->_float;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float > v2->_float;
	}
	SetError("CompareGR Error");
	return false;
}
bool CNeoVMWorker::CompareGE(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if(v2->GetType() == VAR_INT)
			return v1->_int >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int >= v2->_float;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float >= v2->_float;
	}
	SetError("CompareGE Error");
	return false;
}
bool CNeoVMWorker::ForEach(VarInfo* pTable, VarInfo* pKey, VarInfo* pValue)
{
	if (pTable->GetType() != VAR_TABLE)
	{
		SetError("foreach table Error");
		return false;
	}
	TableInfo* tbl = pTable->_tbl;
	if (pKey->GetType() == VAR_INT)
	{
		auto it = tbl->_intMap.find(pKey->_int);
		if (it == tbl->_intMap.end())
			return false;
		else
		{
			if (++it == tbl->_intMap.end())
			{
				if (false == tbl->_strMap.empty())
				{
					auto it = tbl->_strMap.begin();
					Var_SetString(pKey, (*it).first.c_str());
					Move(pValue, &(*it).second);
					return true;
				}
				return false;
			}
			else
			{
				Var_SetInt(pKey, (*it).first);
				Move(pValue, &(*it).second);
				return true;
			}
		}
	}
	else if (pKey->GetType() == VAR_STRING)
	{
		auto it = tbl->_strMap.find(pKey->_str->_str);
		if (it == tbl->_strMap.end())
			return false;
		else
		{
			if (++it == tbl->_strMap.end())
				return false;
			else
			{
				Var_SetString(pKey, (*it).first.c_str());
				Move(pValue, &(*it).second);
				return true;
			}
		}
	}
	else if (pKey->GetType() == VAR_NONE)
	{
		if (false == tbl->_intMap.empty())
		{
			auto it = tbl->_intMap.begin();
			Var_SetInt(pKey, (*it).first);
			Move(pValue, &(*it).second);
			return true;
		}
		if (false == tbl->_strMap.empty())
		{
			auto it = tbl->_strMap.begin();
			Var_SetString(pKey, (*it).first.c_str());
			Move(pValue, &(*it).second);
			return true;
		}
		return false;
	}
	SetError("foreach table key Error");
	return false;
}
// -1 : Error
//  0 : SlieceRun
//  1 : Continue
//  2 : Timeout - 
int CNeoVMWorker::Sleep(bool isSliceRun, int iTimeout, VarInfo* v1)
{
	int iSleepTick;
	switch (v1->GetType())
	{
	case VAR_INT:
		iSleepTick = v1->_int;
		break;
	case VAR_FLOAT:
		iSleepTick = (int)v1->_float;
		break;
	default:
		SetError("Sleep Value Error");
		return -1;
	}

	if (isSliceRun)
	{
		_iRemainSleep = iSleepTick;
		return 0;
	}
	else
	{
		if (iTimeout >= 0)
		{
			auto cur = clock();
			auto delta = cur - _preClock;
			if (iTimeout > delta)
			{
				auto sleepAble = iTimeout - delta;
				if (iSleepTick > sleepAble)
					iSleepTick = sleepAble;
				std::this_thread::sleep_for(std::chrono::milliseconds(iSleepTick));
				return 1;
			}
			return 2;
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(iSleepTick));
			return 1;
		}
		return 1;
	}
	return 1;
}


std::string CNeoVMWorker::ToString(VarInfo* v1)
{
	char ch[256];
	switch (v1->GetType())
	{
	case VAR_NONE:
		return "null";
	case VAR_BOOL:
		return v1->_bl ? "true" : "false";
	case VAR_INT:
#ifdef _WIN32
		sprintf_s(ch, _countof(ch), "%d", v1->_int);
#else
		sprintf(ch, "%d", v1->_int);
#endif
		return ch;
	case VAR_FLOAT:
#ifdef _WIN32
		sprintf_s(ch, _countof(ch), "%lf", v1->_float);
#else
		sprintf(ch, "%lf", v1->_float);
#endif
		return ch;
	case VAR_STRING:
		return v1->_str->_str;
	case VAR_TABLE:
		return "table";
	case VAR_PTRFUN:
		return "function";
	}
	return "null";
}
int CNeoVMWorker::ToInt(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return -1;
	case VAR_BOOL:
		return v1->_bl ? 1 : 0;
	case VAR_INT:
		return v1->_int;
	case VAR_FLOAT:
		return (int)v1->_float;
	case VAR_STRING:
		return ::atoi(v1->_str->_str.c_str());
	case VAR_TABLE:
		return -1;
	case VAR_PTRFUN:
		return -1;
	}
	return -1;
}
double CNeoVMWorker::ToFloat(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return -1;
	case VAR_BOOL:
		return v1->_bl ? 1 : 0;
	case VAR_INT:
		return v1->_int;
	case VAR_FLOAT:
		return v1->_float;
	case VAR_STRING:
		return atof(v1->_str->_str.c_str());
	case VAR_TABLE:
		return -1;
	case VAR_PTRFUN:
		return -1;
	}
	return -1;
}
int CNeoVMWorker::ToSize(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return 0;
	case VAR_BOOL:
		return 0;
	case VAR_INT:
		return 0;
	case VAR_FLOAT:
		return 0;
	case VAR_STRING:
		return (int)v1->_str->_str.length();
	case VAR_TABLE:
		return (int)v1->_tbl->_intMap.size() + (int)v1->_tbl->_strMap.size();
	case VAR_PTRFUN:
		return 0;
	}
	return 0;
}
std::string CNeoVMWorker::GetType(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return "null";
	case VAR_BOOL:
		return "bool";
	case VAR_INT:
		return "int";
	case VAR_FLOAT:
		return "float";
	case VAR_STRING:
		return "string";
	case VAR_TABLE:
		return "table";
	case VAR_PTRFUN:
		return "function";
	}
	return "null";
}


CNeoVMWorker::CNeoVMWorker(CNeoVM* pVM, u32 id, int iStackSize)
{
	_pVM = pVM;
	_idWorker = id;
	SetCodeData(pVM->_pCodePtr, pVM->_iCodeLen);
	m_pVarGlobal = &pVM->m_sVarGlobal;
	m_sVarStack.resize(iStackSize);
	m_sCallStack.reserve(1000);
}
CNeoVMWorker::~CNeoVMWorker()
{

}

void CNeoVMWorker::SetError(const char* pErrMsg)
{
	_pVM->_pErrorMsg = pErrMsg;
}
bool	CNeoVMWorker::Start(int iFunctionID)
{
	if(false == Setup(iFunctionID))
		return false;

	return Run(false);
}

bool	CNeoVMWorker::Setup(int iFunctionID)
{
	SFunctionTable fun = _pVM->m_sFunctionPtr[iFunctionID];
	int iArgs = (int)_args.size();
	if (iArgs != fun._argsCount)
		return false;

	SetCodePtr(fun._codePtr);

	_iSP_Vars = 0;// _header._iStaticVarCount;
	iSP_VarsMax = _iSP_Vars + fun._localAddCount;

	_iSP_Vars_Max2 = iSP_VarsMax;

	if (iArgs > fun._argsCount)
		iArgs = fun._argsCount;

	int iCur;
	for (iCur = 0; iCur < iArgs; iCur++)
		m_sVarStack[1 + _iSP_Vars + iCur] = _args[iCur];
	for (; iCur < fun._argsCount; iCur++)
		Var_Release(&m_sVarStack[1 + _iSP_Vars + iCur]);

	_isSetup = true;
	return true;
}

#pragma pack(1)
struct SVMOperation
{
	debug_info dbg;
	NOP_TYPE   op;
	short n1, n2, n3;
};
#pragma pack()


bool	CNeoVMWorker::Run(bool isSliceRun, int iTimeout, int iCheckOpCount)
{
	if (false == _isSetup)
		return false;

	clock_t t1, t2;
	if (_iRemainSleep > 0)
	{
		t1 = clock();
		t2 = t1 - _preClock;
		if (t2 > 0)
			_iRemainSleep -= t2;

		_preClock = t1;

		if (_iRemainSleep > 0)
			return true;
	}

	SFunctionTable fun;
	FunctionPtrNative* pFunctionPtrNative;
	SCallStack callStack;
	int iTemp;
	char chMsg[256];
	SVMOperation* pOP;

	int op_process = 0;
	bool isTimeout = (iTimeout >= 0);
	if(isTimeout)
		_preClock = clock();

	while (true)
	{
		pOP = (SVMOperation*)GetCurPtr();
		switch (pOP->op)
		{
		case NOP_MOV:
			NextOP(2);
			Move(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;
		case NOP_MOV_MINUS:
			NextOP(2);
			MoveMinus(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;

		case NOP_ADD2:
			NextOP(2);
			Add2(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;
		case NOP_SUB2:
			NextOP(2);
			Sub2(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;
		case NOP_MUL2:
			NextOP(2);
			Mul2(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;
		case NOP_DIV2:
			NextOP(2);
			Div2(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;
		case NOP_PERSENT2:
			NextOP(2);
			Per2(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;

		case NOP_VAR_CLEAR:
			NextOP(1);
			Var_Release(GetVarPtr(pOP->n1));
			break;
		case NOP_INC:
			NextOP(1);
			Inc(GetVarPtr(pOP->n1));
			break;
		case NOP_DEC:
			NextOP(1);
			Dec(GetVarPtr(pOP->n1));
			break;

		case NOP_ADD3:
			NextOP(3);
			Add(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;
		case NOP_SUB3:
			NextOP(3);
			Sub(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;
		case NOP_MUL3:
			NextOP(3);
			Mul(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;
		case NOP_DIV3:
			NextOP(3);
			Div(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;
		case NOP_PERSENT3:
			NextOP(3);
			Per(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;

		case NOP_GREAT:		// >
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), CompareGR(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)));
			break;
		case NOP_GREAT_EQ:	// >=
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), CompareGE(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)));
			break;
		case NOP_LESS:		// <
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), CompareGR(GetVarPtr(pOP->n3), GetVarPtr(pOP->n2)));
			break;
		case NOP_LESS_EQ:	// <=
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), CompareGE(GetVarPtr(pOP->n3), GetVarPtr(pOP->n2)));
			break;
		case NOP_EQUAL2:	// ==
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), CompareEQ(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)));
			break;
		case NOP_NEQUAL:	// !=
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), !CompareEQ(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)));
			break;
		case NOP_AND:	// &&
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), GetVarPtr(pOP->n3)->IsTrue() && GetVarPtr(pOP->n2)->IsTrue());
			break;
		case NOP_OR:	// ||
			NextOP(3);
			Var_SetBool(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2)->IsTrue() || GetVarPtr(pOP->n3)->IsTrue());
			break;



		case NOP_JMP_GREAT:		// >
			NextOP(3);
			if (true == CompareGR(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_GREAT_EQ:	// >=
			NextOP(3);
			if (true == CompareGE(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_LESS:		// <
			NextOP(3);
			if (true == CompareGR(GetVarPtr(pOP->n3), GetVarPtr(pOP->n2)))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_LESS_EQ:	// <=
			NextOP(3);
			if (true == CompareGE(GetVarPtr(pOP->n3), GetVarPtr(pOP->n2)))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_EQUAL2:	// ==
			NextOP(3);
			if (true == CompareEQ(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_NEQUAL:	// !=
			NextOP(3);
			if (false == CompareEQ(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3)))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_AND:	// &&
			NextOP(3);
			if (GetVarPtr(pOP->n2)->IsTrue() && GetVarPtr(pOP->n3)->IsTrue())
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_OR:		// ||
			NextOP(3);
			if (GetVarPtr(pOP->n2)->IsTrue() || GetVarPtr(pOP->n3)->IsTrue())
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_NAND:	// !(&&)
			NextOP(3);
			if (false == (GetVarPtr(pOP->n2)->IsTrue() && GetVarPtr(pOP->n3)->IsTrue()))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_NOR:	// !(||)
			NextOP(3);
			if (false == (GetVarPtr(pOP->n2)->IsTrue() || GetVarPtr(pOP->n3)->IsTrue()))
				SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_FOREACH:	// foreach
			NextOP(3);
			if(ForEach(GetVarPtr(pOP->n2), GetVarPtr(pOP->n3), GetVarPtr(pOP->n3+1)))
				SetCodeIncPtr(pOP->n1);
			break;

		case NOP_STR_ADD:	// ..
			NextOP(3);
			Var_SetString(GetVarPtr(pOP->n1), (ToString(GetVarPtr(pOP->n2)) + ToString(GetVarPtr(pOP->n3))).c_str());
			break;

		case NOP_TOSTRING:
			NextOP(2);
			Var_SetString(GetVarPtr(pOP->n1), ToString(GetVarPtr(pOP->n2)).c_str());
			break;
		case NOP_TOINT:
			NextOP(2);
			Var_SetInt(GetVarPtr(pOP->n1), ToInt(GetVarPtr(pOP->n2)));
			break;
		case NOP_TOFLOAT:
			NextOP(2);
			Var_SetFloat(GetVarPtr(pOP->n1), ToFloat(GetVarPtr(pOP->n2)));
			break;
		case NOP_TOSIZE:
			NextOP(2);
			Var_SetInt(GetVarPtr(pOP->n1), ToSize(GetVarPtr(pOP->n2)));
			break;
		case NOP_GETTYPE:
			NextOP(2);
			Var_SetString(GetVarPtr(pOP->n1), GetType(GetVarPtr(pOP->n2)).c_str());
			break;
		case NOP_SLEEP:
			NextOP(1);
			{
				int r = Sleep(isSliceRun, iTimeout, GetVarPtr(pOP->n1));
				if (r == 0)
				{
					_preClock = clock();
					return true;
				}
				else if (r == 2)
				{
					op_process = iCheckOpCount;
				}
			}
			break;

		case NOP_JMP:
			NextOP(1);
			SetCodeIncPtr(pOP->n1);
			break;
		case NOP_JMP_FALSE:
			NextOP(2);
			if (false == GetVarPtr(pOP->n1)->IsTrue())
				SetCodeIncPtr(pOP->n2);
			break;
		case NOP_JMP_TRUE:
			NextOP(2);
			if (true == GetVarPtr(pOP->n1)->IsTrue())
				SetCodeIncPtr(pOP->n2);
			break;

		case NOP_CALL:
			NextOP(2);
			// n2 is Arg Count not use
			callStack._iReturnOffset = GetCodeptr();
			callStack._iSP_Vars = _iSP_Vars;
			callStack._iSP_VarsMax = iSP_VarsMax;
			m_sCallStack.push_back(callStack);

			fun = _pVM->m_sFunctionPtr[pOP->n1];
			SetCodePtr(fun._codePtr);
			_iSP_Vars = iSP_VarsMax;
			iSP_VarsMax = _iSP_Vars + fun._localAddCount;
			if (_iSP_Vars_Max2 < iSP_VarsMax)
				_iSP_Vars_Max2 = iSP_VarsMax;
			break;
		case NOP_FARCALL:
		{
			NextOP(2);
			short n2 = pOP->n2;
			if (_iSP_Vars_Max2 < iSP_VarsMax + (1 + n2))
				_iSP_Vars_Max2 = iSP_VarsMax + (1 + n2);

			fun = _pVM->m_sFunctionPtr[pOP->n1];
			if (fun._fun._func == NULL)
			{	// Error
				SetError("Ptr Call is null");
				break;
			}

			int iSave = _iSP_Vars;
			_iSP_Vars = iSP_VarsMax;

			if((*fun._fun._fn)(this, &fun._fun, n2) < 0)
			{
				SetError("Ptr Call Argument Count Error");
				break;
			}

			_iSP_Vars = iSave;
			break;
		}
		case NOP_PTRCALL:
		{
			NextOP(3);
			short n3 = pOP->n3;

			if (_iSP_Vars_Max2 < iSP_VarsMax + (1 + n3))
				_iSP_Vars_Max2 = iSP_VarsMax + (1 + n3);

			pFunctionPtrNative = GetPtrFunction(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			if (pFunctionPtrNative != NULL)
			{
				int iSave = _iSP_Vars;
				_iSP_Vars = iSP_VarsMax;

				if ((pFunctionPtrNative->_func)(this, n3) == false)
				{
					SetError("Ptr Call Error");
					break;
				}

				_iSP_Vars = iSave;
			}
			else
			{
				SetError("Ptr Call Not Found");
				break;
			}
			break;
		}
		case NOP_RETURN:
			NextOP(1);
			if (pOP->n1 == 0)
				Var_Release(&m_sVarStack[_iSP_Vars]); // Clear
			else
				Move(&m_sVarStack[_iSP_Vars], GetVarPtr(pOP->n1));

			if (m_sCallStack.empty())
			{
				_isSetup = false;
				return true;
			}
			iTemp = (int)m_sCallStack.size() - 1;
			callStack = m_sCallStack[iTemp];
			m_sCallStack.resize(iTemp);

			SetCodePtr(callStack._iReturnOffset);
			_iSP_Vars = callStack._iSP_Vars;
			iSP_VarsMax = callStack._iSP_VarsMax;
			break;
		case NOP_FUNEND:
			NextOP(0);
			Var_Release(&m_sVarStack[_iSP_Vars]); // Clear
			if (m_sCallStack.empty())
			{
				_isSetup = false;
				return true;
			}
			iTemp = (int)m_sCallStack.size() - 1;
			callStack = m_sCallStack[iTemp];
			m_sCallStack.resize(iTemp);

			SetCodePtr(callStack._iReturnOffset);
			_iSP_Vars = callStack._iSP_Vars;
			iSP_VarsMax = callStack._iSP_VarsMax;
			break;
		case NOP_TABLE_ALLOC:
			NextOP(1);
			Var_SetTable(GetVarPtr(pOP->n1), _pVM->TableAlloc());
			break;
		case NOP_TABLE_INSERT:
		{
			NextOP(3);
			TableInsert(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;
		}
		case NOP_TABLE_READ:
		{
			NextOP(3);
			TableRead(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2), GetVarPtr(pOP->n3));
			break;
		}
		case NOP_TABLE_REMOVE:
		{
			NextOP(2);
			TableRemove(GetVarPtr(pOP->n1), GetVarPtr(pOP->n2));
			break;
		}
		default:
			NextOP(0);
			SetError("Unkonwn OP");
			break;
		}
		if (_pVM->_pErrorMsg != NULL)
		{
			m_sCallStack.clear();
			_iSP_Vars = 0;
#ifdef _WIN32
			sprintf_s(chMsg, _countof(chMsg), "%s : Line (%d)", _pVM->_pErrorMsg, pOP->dbg._lineseq);
#else
			sprintf(chMsg, "%s : Line (%d)", _pVM->_pErrorMsg, pOP->dbg._lineseq);
#endif
			_pVM->_sErrorMsgDetail = chMsg;
			return false;
		}
		if (iTimeout >= 0)
		{
			if (++op_process >= iCheckOpCount)
			{
				op_process = 0;
				t2 = clock() - _preClock;
				if (t2 >= iTimeout || t2 < 0)
					break;
			}
		}
	}
	return true;
}

bool CNeoVMWorker::RunFunction(const std::string& funName)
{
	auto it = _pVM->m_sImExportTable.find(funName);
	if (it == _pVM->m_sImExportTable.end())
	{
		SetError("Function Not Found");
		_pVM->_sErrorMsgDetail = _pVM->_pErrorMsg;
		_pVM->_sErrorMsgDetail += "(";
		_pVM->_sErrorMsgDetail += funName;
		_pVM->_sErrorMsgDetail += ")";
		return false;
	}

	int iID = (*it).second;
	Start(iID);

	return true;
}

void CNeoVMWorker::PushString(const char* p)
{
	VarInfo d;
	d.SetType(VAR_STRING);
	d._str = _pVM->StringAlloc(p);
	_args.push_back(d);
}