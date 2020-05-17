#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"

void	SetCompileError(const char*	lpszString, ...);

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
	default:
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
	default:
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
void CNeoVMWorker::Var_SetNone(VarInfo *d)
{
	if (d->GetType() != VAR_NONE)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_NONE);
	}
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

void CNeoVMWorker::Var_SetFun(VarInfo* d, int fun_index)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_FUN);
	d->_fun_index = fun_index;
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
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			table[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			table[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		std::map<std::string, VarInfo>& table = pTable->_tbl->_strMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			table[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	default:
		break;
	}
}
VarInfo* CNeoVMWorker::GetTableItem(VarInfo *pTable, VarInfo *pArray)
{
	if (pTable->GetType() != VAR_TABLE)
		return NULL;

	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = (int)pArray->_int;
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
			return &(*it).second;
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
			return &(*it).second;
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		std::map<std::string, VarInfo>& table = pTable->_tbl->_strMap;
		auto it = table.find(n);
		if (it != table.end())
			return &(*it).second;
		break;
	}
	default:
		break;
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
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		std::map<std::string, VarInfo>& table = pTable->_tbl->_strMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	default:
		break;
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
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			pValue = &(*it).second;
			Var_Release(pValue);
			table.erase(it);
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		std::map<int, VarInfo>& table = pTable->_tbl->_intMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			pValue = &(*it).second;
			Var_Release(pValue);
			table.erase(it);
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		std::map<std::string, VarInfo>& table = pTable->_tbl->_strMap;
		auto it = table.find(n);
		if (it != table.end())
		{
			pValue = &(*it).second;
			Var_Release(pValue);
			table.erase(it);
		}
		break;
	}
	default:
		break;
	}
}

void CNeoVMWorker::Move(VarInfo* v1, VarInfo* v2)
{
	switch (v2->GetType())
	{
	case VAR_NONE:
		Var_SetNone(v1);
		break;
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
	case VAR_FUN:
		Var_Release(v1);
		v1->SetType(v2->GetType());
		v1->_fun_index = v2->_fun_index;
		break;
	default:
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
	default:
		break;
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
	default:
		break;
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
	default:
		break;
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
	case VAR_TABLEFUN:
		return "function";
	default:
		break;
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
	case VAR_TABLEFUN:
		return -1;
	default:
		break;
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
	case VAR_TABLEFUN:
		return -1;
	default:
		break;
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
	case VAR_TABLEFUN:
		return 0;
	default:
		break;
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
	case VAR_TABLEFUN:
		return "table_function";
	case VAR_FUN:
		return "function";
	default:
		break;
	}
	return "null";
}

void CNeoVMWorker::Call(int n1, int n2)
{
	SFunctionTable fun = _pVM->m_sFunctionPtr[n1];
	if (fun._funType != FUNT_IMPORT)
	{	// n2 is Arg Count not use
		SCallStack callStack;
		callStack._iReturnOffset = GetCodeptr();
		callStack._iSP_Vars = _iSP_Vars;
		callStack._iSP_VarsMax = iSP_VarsMax;
		m_sCallStack.push_back(callStack);

		SetCodePtr(fun._codePtr);
		_iSP_Vars = iSP_VarsMax;
		iSP_VarsMax = _iSP_Vars + fun._localAddCount;
		if (_iSP_Vars_Max2 < iSP_VarsMax)
			_iSP_Vars_Max2 = iSP_VarsMax;
	}
	else
	{
		if (_iSP_Vars_Max2 < iSP_VarsMax + (1 + n2))
			_iSP_Vars_Max2 = iSP_VarsMax + (1 + n2);

		fun = _pVM->m_sFunctionPtr[n1];
		if (fun._fun._func == NULL)
		{	// Error
			SetError("Ptr Call is null");
			return;
		}

		int iSave = _iSP_Vars;
		_iSP_Vars = iSP_VarsMax;

		if ((*fun._fun._fn)(this, &fun._fun, n2) < 0)
		{
			SetError("Ptr Call Argument Count Error");
			return;
		}
		_iSP_Vars = iSave;
	}
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

	FunctionPtrNative* pFunctionPtrNative;
	SCallStack callStack;
	int iTemp;
	char chMsg[256];
	SVMOperation OP;

	int op_process = 0;
	bool isTimeout = (iTimeout >= 0);
	if(isTimeout)
		_preClock = clock();

	bool blDebugInfo = _pVM->IsDebugInfo();
	//debug_info dbg;
	OP.dbg._lineseq = -1;

	try
	{
		while (true)
		{
			//if (blDebugInfo)
			//{
			//	dbg = *(debug_info*)GetCurPtr();
			//	SetCodePtr(GetCodeptr() + sizeof(debug_info));
			//}

			GetOP(blDebugInfo, &OP);
			switch (OP.op)
			{
			case NOP_NONE:
				break;
			case NOP_MOV:
				Move(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			case NOP_MOV_MINUS:
				MoveMinus(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			case NOP_ADD2:
				Add2(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			case NOP_SUB2:
				Sub2(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			case NOP_MUL2:
				Mul2(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			case NOP_DIV2:
				Div2(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			case NOP_PERSENT2:
				Per2(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;

			case NOP_VAR_CLEAR:
				Var_Release(GetVarPtr(OP.n1));
				break;
			case NOP_INC:
				Inc(GetVarPtr(OP.n1));
				break;
			case NOP_DEC:
				Dec(GetVarPtr(OP.n1));
				break;

			case NOP_ADD3:
				Add(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;
			case NOP_SUB3:
				Sub(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;
			case NOP_MUL3:
				Mul(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;
			case NOP_DIV3:
				Div(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;
			case NOP_PERSENT3:
				Per(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;

			case NOP_GREAT:		// >
				Var_SetBool(GetVarPtr(OP.n1), CompareGR(GetVarPtr(OP.n2), GetVarPtr(OP.n3)));
				break;
			case NOP_GREAT_EQ:	// >=
				Var_SetBool(GetVarPtr(OP.n1), CompareGE(GetVarPtr(OP.n2), GetVarPtr(OP.n3)));
				break;
			case NOP_LESS:		// <
				Var_SetBool(GetVarPtr(OP.n1), CompareGR(GetVarPtr(OP.n3), GetVarPtr(OP.n2)));
				break;
			case NOP_LESS_EQ:	// <=
				Var_SetBool(GetVarPtr(OP.n1), CompareGE(GetVarPtr(OP.n3), GetVarPtr(OP.n2)));
				break;
			case NOP_EQUAL2:	// ==
				Var_SetBool(GetVarPtr(OP.n1), CompareEQ(GetVarPtr(OP.n2), GetVarPtr(OP.n3)));
				break;
			case NOP_NEQUAL:	// !=
				Var_SetBool(GetVarPtr(OP.n1), !CompareEQ(GetVarPtr(OP.n2), GetVarPtr(OP.n3)));
				break;
			case NOP_AND:	// &&
				Var_SetBool(GetVarPtr(OP.n1), GetVarPtr(OP.n3)->IsTrue() && GetVarPtr(OP.n2)->IsTrue());
				break;
			case NOP_OR:	// ||
				Var_SetBool(GetVarPtr(OP.n1), GetVarPtr(OP.n2)->IsTrue() || GetVarPtr(OP.n3)->IsTrue());
				break;



			case NOP_JMP_GREAT:		// >
				if (true == CompareGR(GetVarPtr(OP.n2), GetVarPtr(OP.n3)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_GREAT_EQ:	// >=
				if (true == CompareGE(GetVarPtr(OP.n2), GetVarPtr(OP.n3)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_LESS:		// <
				if (true == CompareGR(GetVarPtr(OP.n3), GetVarPtr(OP.n2)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_LESS_EQ:	// <=
				if (true == CompareGE(GetVarPtr(OP.n3), GetVarPtr(OP.n2)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_EQUAL2:	// ==
				if (true == CompareEQ(GetVarPtr(OP.n2), GetVarPtr(OP.n3)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_NEQUAL:	// !=
				if (false == CompareEQ(GetVarPtr(OP.n2), GetVarPtr(OP.n3)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_AND:	// &&
				if (GetVarPtr(OP.n2)->IsTrue() && GetVarPtr(OP.n3)->IsTrue())
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_OR:		// ||
				if (GetVarPtr(OP.n2)->IsTrue() || GetVarPtr(OP.n3)->IsTrue())
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_NAND:	// !(&&)
				if (false == (GetVarPtr(OP.n2)->IsTrue() && GetVarPtr(OP.n3)->IsTrue()))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_NOR:	// !(||)
				if (false == (GetVarPtr(OP.n2)->IsTrue() || GetVarPtr(OP.n3)->IsTrue()))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_FOREACH:	// foreach
				if (ForEach(GetVarPtr(OP.n2), GetVarPtr(OP.n3), GetVarPtr(OP.n3 + 1)))
					SetCodeIncPtr(OP.n1);
				break;

			case NOP_STR_ADD:	// ..
				Var_SetString(GetVarPtr(OP.n1), (ToString(GetVarPtr(OP.n2)) + ToString(GetVarPtr(OP.n3))).c_str());
				break;

			case NOP_TOSTRING:
				Var_SetString(GetVarPtr(OP.n1), ToString(GetVarPtr(OP.n2)).c_str());
				break;
			case NOP_TOINT:
				Var_SetInt(GetVarPtr(OP.n1), ToInt(GetVarPtr(OP.n2)));
				break;
			case NOP_TOFLOAT:
				Var_SetFloat(GetVarPtr(OP.n1), ToFloat(GetVarPtr(OP.n2)));
				break;
			case NOP_TOSIZE:
				Var_SetInt(GetVarPtr(OP.n1), ToSize(GetVarPtr(OP.n2)));
				break;
			case NOP_GETTYPE:
				Var_SetString(GetVarPtr(OP.n1), GetType(GetVarPtr(OP.n2)).c_str());
				break;
			case NOP_SLEEP:
				{
					int r = Sleep(isSliceRun, iTimeout, GetVarPtr(OP.n1));
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
				SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_FALSE:
				if (false == GetVarPtr(OP.n1)->IsTrue())
					SetCodeIncPtr(OP.n2);
				break;
			case NOP_JMP_TRUE:
				if (true == GetVarPtr(OP.n1)->IsTrue())
					SetCodeIncPtr(OP.n2);
				break;

			case NOP_CALL:
				Call(OP.n1, OP.n2);
				if(_pVM->IsLocalErrorMsg())
					break;
				break;
			case NOP_PTRCALL:
			{
				VarInfo* pVar1 = GetVarPtr(OP.n1);
				if (-1 == OP.n2)
				{
					if (pVar1->GetType() != VAR_FUN)
					{
						SetError("Call Function Type Error");
						break;
					}
					Call(pVar1->_fun_index, OP.n3);
					break;
				}
				VarInfo* pVarItem = GetTableItem(pVar1, GetVarPtr(OP.n2));
				if (pVarItem == NULL)
				{
					SetError("Ptr Call Error");
					break;
				}
				short n3 = OP.n3;

				if (_iSP_Vars_Max2 < iSP_VarsMax + (1 + n3))
					_iSP_Vars_Max2 = iSP_VarsMax + (1 + n3);

				if(pVarItem->GetType() == VAR_FUN)
					Call(pVarItem->_fun_index, n3);
				else if (pVarItem->GetType() == VAR_TABLEFUN)
				{
					pFunctionPtrNative = &pVarItem->_fun;
					if (pFunctionPtrNative != NULL)
					{
						int iSave = _iSP_Vars;
						_iSP_Vars = iSP_VarsMax;

						_pCallTableInfo = pVar1->_tbl;
						if ((pFunctionPtrNative->_func)(this, n3) == false)
						{
							_pCallTableInfo = NULL;
							SetError("Ptr Call Error");
							break;
						}
						_pCallTableInfo = NULL;

						_iSP_Vars = iSave;
					}
					else
					{
						SetError("Ptr Call Not Found");
						break;
					}
				}
				break;
			}
			case NOP_RETURN:
				if (OP.n1 == 0)
					Var_Release(&m_sVarStack[_iSP_Vars]); // Clear
				else
					Move(&m_sVarStack[_iSP_Vars], GetVarPtr(OP.n1));

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
				Var_SetTable(GetVarPtr(OP.n1), _pVM->TableAlloc());
				break;
			case NOP_TABLE_INSERT:
			{
				TableInsert(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;
			}
			case NOP_TABLE_READ:
			{
				TableRead(GetVarPtr(OP.n1), GetVarPtr(OP.n2), GetVarPtr(OP.n3));
				break;
			}
			case NOP_TABLE_REMOVE:
			{
				TableRemove(GetVarPtr(OP.n1), GetVarPtr(OP.n2));
				break;
			}
			default:
				SetError("Unkonwn OP");
				break;
			}
			if (_pVM->_pErrorMsg != NULL)
			{
				m_sCallStack.clear();
				_iSP_Vars = 0;
#ifdef _WIN32
				sprintf_s(chMsg, _countof(chMsg), "%s : Line (%d)", _pVM->_pErrorMsg, OP.dbg._lineseq);
#else
				sprintf(chMsg, "%s : Line (%d)", _pVM->_pErrorMsg, OP.dbg._lineseq);
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
	}
	catch (...)
	{
		SetError("Exception");
#ifdef _WIN32
		sprintf_s(chMsg, _countof(chMsg), "%s : Line (%d)", _pVM->_pErrorMsg, OP.dbg._lineseq);
#else
		sprintf(chMsg, "%s : Line (%d)", _pVM->_pErrorMsg, OP.dbg._lineseq);
#endif
		_pVM->_sErrorMsgDetail = chMsg;
		return false;
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