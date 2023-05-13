#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <thread>
#include <chrono>
#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"
#include "NeoLibDCall.h"
#include "UTFString.h"

void	SetCompileError(const char*	lpszString, ...);

void SVarWrapper::SetNone() { _vmw->Var_SetNone(_var); }
void SVarWrapper::SetInt(int v) { _vmw->Var_SetInt(_var, v); }
void SVarWrapper::SetFloat(double v) { _vmw->Var_SetFloat(_var, v); }
void SVarWrapper::SetBool(bool v) { _vmw->Var_SetBool(_var, v); }
void SVarWrapper::SetString(const char* str) { _vmw->Var_SetString(_var, str); }
//void SVarWrapper::SetTableFun(FunctionPtrNative fun) { _vmw->Var_SetTableFun(_var, fun); }

static std::string g_meta_Add3 = "+";
static std::string g_meta_Sub3 = "-";
static std::string g_meta_Mul3 = "*";
static std::string g_meta_Div3 = "/";
static std::string g_meta_Per3 = "%";

static std::string g_meta_Add2 = "+=";
static std::string g_meta_Sub2 = "-=";
static std::string g_meta_Mul2 = "*=";
static std::string g_meta_Div2 = "/=";
static std::string g_meta_Per2 = "%=";



CNeoVMWorker::CNeoVMWorker(CNeoVM* pVM, u32 id, int iStackSize)
{
	_pVM = pVM;
	_idWorker = id;
	//SetCodeData(pVM->_pCodePtr, pVM->_iCodeLen);
	m_pVarGlobal = &m_sVarGlobal;

	m_pCur = &m_sDefault;
	m_pCur->_state = COROUTINE_STATE_RUNNING;
	m_pVarStack = &m_pCur->m_sVarStack;
	m_pCallStack = &m_pCur->m_sCallStack;

	ClearSP();

	_intA2.SetType(VAR_INT);
	_intA3.SetType(VAR_INT);
	_funA3.SetType(VAR_FUN);

	m_pVarStack->resize(iStackSize);
	m_pCallStack->reserve(1000);
}
CNeoVMWorker::~CNeoVMWorker()
{
	for (int i = 0; i < (int)m_sVarGlobal.size(); i++)
		Var_Release(&m_sVarGlobal[i]);
	m_sVarGlobal.clear();

	if (_pCodeBegin != NULL)
	{
		delete[] _pCodeBegin;
		_pCodeBegin = NULL;
	}
}
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
	case VAR_COROUTINE:
		++d->_cor->_refCount;
		break;
	default:
		break;
	}
}
void CNeoVMWorker::Var_Release(VarInfo *d)
{
	if (d->IsAllocType())
		_pVM->Var_ReleaseInternal(d);
	else
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

		d->ClearType();
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
void CNeoVMWorker::Var_SetFun(VarInfo* d, int fun_index)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_FUN);
	d->_fun_index = fun_index;
}
void CNeoVMWorker::Var_SetCoroutine(VarInfo *d, CoroutineInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_COROUTINE);
	d->_cor = p;
	++d->_cor->_refCount;
}
void CNeoVMWorker::Var_SetString(VarInfo *d, const char* str)
{
	Var_SetStringA(d, str);
}
void CNeoVMWorker::Var_SetStringA(VarInfo *d, const std::string& str)
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
	++p->_refCount;
}
void CNeoVMWorker::Var_SetList(VarInfo *d, ListInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_LIST);
	d->_lst = p;
	++p->_refCount;
}
void CNeoVMWorker::Var_SetSet(VarInfo *d, SetInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_SET);
	d->_set = p;
	++p->_refCount;
}
void CNeoVMWorker::Var_SetModule(VarInfo *d, CNeoVMWorker* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_MODULE);
	d->_module = p;
	++p->_refCount;
}

void CNeoVMWorker::CltInsert(VarInfo *pClt, VarInfo *pKey, VarInfo *pValue)
{
	switch (pClt->GetType())
	{
	case VAR_TABLE:
		if (pValue->GetType() == VAR_NONE)
		{
			TableRemove(pClt, pKey);
			return;
		}
		pClt->_tbl->Insert(pKey, pValue);
		break;
	case VAR_LIST:
		if (pKey->GetType() != VAR_INT)
		{
			SetError("Collision Insert Error");
			return;
		}
		pClt->_lst->SetValue(pKey->_int, pValue);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}
void CNeoVMWorker::CltInsert(VarInfo *pClt, int key, VarInfo *v)
{
	switch (pClt->GetType())
	{
	case VAR_TABLE:
		pClt->_tbl->Insert(key, v);
		break;
	case VAR_LIST:
		pClt->_lst->SetValue(key, v);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}

void CNeoVMWorker::CltInsert(VarInfo *pClt, VarInfo *pKey, int v)
{
	switch (pClt->GetType())
	{
	case VAR_TABLE:
		pClt->_tbl->Insert(pKey, v);
		break;
	case VAR_LIST:
		pClt->_lst->SetValue(pKey->_int, v);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}
void CNeoVMWorker::CltInsert(VarInfo *pClt, int key, int v)
{
	switch (pClt->GetType())
	{
	case VAR_TABLE:
		pClt->_tbl->Insert(key, v);
		break;
	case VAR_LIST:
		pClt->_lst->SetValue(key, v);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}
VarInfo* CNeoVMWorker::GetTableItem(VarInfo *pTable, VarInfo *pKey)
{
	if (pTable->GetType() != VAR_TABLE)
	{
		SetError("TableRead Error");
		return NULL;
	}

	return pTable->_tbl->Find(pKey);
}
VarInfo* CNeoVMWorker::GetTableItemValid(VarInfo *pTable, VarInfo *pKey)
{
	VarInfo* r = GetTableItem(pTable, pKey);
	if (r != NULL)
		return r;
	SetError("Table Key Not Found");
	return NULL;
}
VarInfo* CNeoVMWorker::GetTableItemValid(VarInfo *pTable, int Array)
{
	VarInfo Key(Array);
	VarInfo* r = GetTableItem(pTable, &Key);
	if (r != NULL)
		return r;
	SetError("Table Key Not Found");
	return NULL;
}

void CNeoVMWorker::CltRead(VarInfo *pClt, VarInfo *pKey, VarInfo *pValue)
{
	VarInfo *pFind;
	switch (pClt->GetType())
	{
	case VAR_TABLE:
		pFind = GetTableItem(pClt, pKey);
		if (pFind)
			Move(pValue, pFind);
		else
			Var_Release(pValue);
		break;
	case VAR_LIST:
		if (pKey->GetType() != VAR_INT)
		{
			SetError("Collision Read Error");
			return;
		}
		if(false == pClt->_lst->GetValue(pKey->_int, pValue))
			SetError("Collision Read Error");
		break;
	default:
		SetError("Collision Read Error");
		return;
	}
}

void CNeoVMWorker::TableRemove(VarInfo *pTable, VarInfo *pKey)
{
	if (pTable->GetType() != VAR_TABLE)
	{
		SetError("TableRead Error");
		return;
	}

	pTable->_tbl->Remove(pKey);
}

void CNeoVMWorker::Swap(VarInfo* v1, VarInfo* v2)
{
	VarInfo* p = v1;
	v1 = v2;
	v2 = p;
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
void CNeoVMWorker::Add2(eNOperationSub op, VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: r->_int += v2->_int; break;
			case eOP_SUB: r->_int -= v2->_int; break;
			case eOP_MUL: r->_int *= v2->_int; break;
			case eOP_DIV: r->_int /= v2->_int; break;
			case eOP_PER: r->_int %= v2->_int; break;
			case eOP_LSH: r->_int <<= v2->_int; break;
			case eOP_RSH: r->_int >>= v2->_int; break;
			case eOP_AND: r->_int &= v2->_int; break;
			case eOP__OR: r->_int |= v2->_int; break;
			case eOP_XOR: r->_int ^= v2->_int; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			switch (op)
			{
			case eOP_ADD: r->_float = (double)r->_int + v2->_float; break;
			case eOP_SUB: r->_float = (double)r->_int - v2->_float; break;
			case eOP_MUL: r->_float = (double)r->_int * v2->_float; break;
			case eOP_DIV: r->_float = (double)r->_int / v2->_float; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: r->_float += v2->_int; break;
			case eOP_SUB: r->_float -= v2->_int; break;
			case eOP_MUL: r->_float *= v2->_int; break;
			case eOP_DIV: r->_float /= v2->_int; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			switch (op)
			{
			case eOP_ADD: r->_float += v2->_float; break;
			case eOP_SUB: r->_float -= v2->_float; break;
			case eOP_MUL: r->_float *= v2->_float; break;
			case eOP_DIV: r->_float /= v2->_float; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, r->_str->_str + v2->_str->_str); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_TABLE:
		switch (op)
		{
		case eOP_ADD: if (Call_MetaTable(r, g_meta_Add2, r, r, v2)) return; break;
		case eOP_SUB: if (Call_MetaTable(r, g_meta_Sub2, r, r, v2)) return; break;
		case eOP_MUL: if (Call_MetaTable(r, g_meta_Div2, r, r, v2)) return; break;
		case eOP_DIV: if (Call_MetaTable(r, g_meta_Per2, r, r, v2)) return; break;
		default: SetError("operator Error"); break;
		}
		break;
	}
	SetError("+= Error");
}

void CNeoVMWorker::MoveMinusI(VarInfo* v1, int v)
{
	Var_SetInt(v1, -v);
}
void CNeoVMWorker::Add2(eNOperationSub op, VarInfo* r, int v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		switch (op)
		{
		case eOP_ADD: r->_int += v2; break;
		case eOP_SUB: r->_int -= v2; break;
		case eOP_MUL: r->_int *= v2; break;
		case eOP_DIV: r->_int /= v2; break;
		case eOP_PER: r->_int %= v2; break;
		case eOP_LSH: r->_int <<= v2; break;
		case eOP_RSH: r->_int >>= v2; break;
		case eOP_AND: r->_int &= v2; break;
		case eOP__OR: r->_int |= v2; break;
		case eOP_XOR: r->_int ^= v2; break;
		default: SetError("operator Error"); break;
		}
		return;
	case VAR_FLOAT:
		switch (op)
		{
		case eOP_ADD: r->_float += v2; break;
		case eOP_SUB: r->_float -= v2; break;
		case eOP_MUL: r->_float *= v2; break;
		case eOP_DIV: r->_float /= v2; break;
		default: SetError("operator Error"); break;
		}
		return;
	case VAR_STRING:
		break;
	case VAR_TABLE:
		switch (op)
		{
		case eOP_ADD: if (Call_MetaTableI(r, g_meta_Add2, r, r, v2)) return; break;
		case eOP_SUB: if (Call_MetaTableI(r, g_meta_Sub2, r, r, v2)) return; break;
		case eOP_MUL: if (Call_MetaTableI(r, g_meta_Mul2, r, r, v2)) return; break;
		case eOP_DIV: if (Call_MetaTableI(r, g_meta_Div2, r, r, v2)) return; break;
		case eOP_PER: if (Call_MetaTableI(r, g_meta_Per2, r, r, v2)) return; break;
		default: SetError("operator Error"); break;
		}
		break;
	}
	SetError("+= Error");
}


void CNeoVMWorker::And(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_BOOL:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, (int)v1->_bl & v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetBool(r, v1->_bl & v2->_bl);
			return;
		}
		break;
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int & v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetInt(r, v1->_int & (int)v2->_bl);
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_TABLE:
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		if (neo_DCalllibs::Set_And(this, r, v1, v2))
			return;
		break;
	}
	SetError("& Error");
}
void CNeoVMWorker::Or(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_BOOL:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, (int)v1->_bl | v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetBool(r, v1->_bl | v2->_bl);
			return;
		}
		break;
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int | v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetInt(r, v1->_int | (int)v2->_bl);
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_TABLE:
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		if (neo_DCalllibs::Set_Or(this, r, v1, v2))
			return;
		break;
	}
	SetError("| Error");
}
void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetInt(r, v1->_int + v2->_int); break;
			case eOP_SUB: Var_SetInt(r, v1->_int - v2->_int); break;
			case eOP_MUL: Var_SetInt(r, v1->_int * v2->_int); break;
			case eOP_DIV: Var_SetInt(r, v1->_int / v2->_int); break;
			case eOP_PER: Var_SetInt(r, v1->_int % v2->_int); break;
			case eOP_LSH: Var_SetInt(r, v1->_int << v2->_int); break;
			case eOP_RSH: Var_SetInt(r, v1->_int >> v2->_int); break;
			case eOP_AND: Var_SetInt(r, v1->_int & v2->_int); break;
			case eOP__OR: Var_SetInt(r, v1->_int | v2->_int); break;
			case eOP_XOR: Var_SetInt(r, v1->_int ^ v2->_int); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetFloat(r, v1->_int + v2->_float); break;
			case eOP_SUB: Var_SetFloat(r, v1->_int - v2->_float); break;
			case eOP_MUL: Var_SetFloat(r, v1->_int * v2->_float); break;
			case eOP_DIV: Var_SetFloat(r, v1->_int / v2->_float); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetFloat(r, v1->_float + v2->_int); break;
			case eOP_SUB: Var_SetFloat(r, v1->_float - v2->_int); break;
			case eOP_MUL: Var_SetFloat(r, v1->_float * v2->_int); break;
			case eOP_DIV: Var_SetFloat(r, v1->_float / v2->_int); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetFloat(r, v1->_float + v2->_float); break;
			case eOP_SUB: Var_SetFloat(r, v1->_float - v2->_float); break;
			case eOP_MUL: Var_SetFloat(r, v1->_float * v2->_float); break;
			case eOP_DIV: Var_SetFloat(r, v1->_float / v2->_float); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, r->_str->_str + v2->_str->_str); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_TABLE:
		switch (op)
		{
		case eOP_ADD: 
			if (Call_MetaTable(v1, g_meta_Add3, r, v1, v2)) return;
			SetError("unsupported operand + Error"); break;
		case eOP_SUB: 
			if (Call_MetaTable(v1, g_meta_Sub3, r, v1, v2)) return;
			SetError("unsupported operand - Error"); break;
		case eOP_MUL: 
			if (Call_MetaTable(v1, g_meta_Mul3, r, v1, v2)) return;
			SetError("unsupported operand * Error"); break;
		case eOP_DIV: 
			if (Call_MetaTable(v1, g_meta_Div3, r, v1, v2)) return;
			SetError("unsupported operand / Error"); break;
		case eOP_PER: 
			if (Call_MetaTable(v1, g_meta_Per3, r, v1, v2)) return;
			SetError("unsupported operand % Error"); break;
		case eOP_LSH: SetError("unsupported operand << Error"); break;
		case eOP_RSH: SetError("unsupported operand >> Error"); break;
		}
		break;
	case VAR_LIST:
		switch (op)
		{
		case eOP_ADD:
			if (neo_DCalllibs::List_Add(this, r, v1, v2)) return;
			SetError("unsupported operand + Error"); break;
		default: SetError("operator Error"); break;
		}
		break;
	case VAR_SET:
		switch (op)
		{
		case eOP_ADD:
			SetError("unsupported operand + Error"); break;
		case eOP_SUB: 
			if (neo_DCalllibs::Set_Sub(this, r, v1, v2)) return;
			SetError("unsupported operand - Error"); break;
		default: SetError("operator Error"); break;
		}
		return;
		break;
	}
	SetError("unsupported operand Error");
}
void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, VarInfo* v1, int v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		switch (op)
		{
		case eOP_ADD: Var_SetInt(r, v1->_int + v2); break;
		case eOP_SUB: Var_SetInt(r, v1->_int - v2); break;
		case eOP_MUL: Var_SetInt(r, v1->_int * v2); break;
		case eOP_DIV: Var_SetInt(r, v1->_int / v2); break;
		case eOP_PER: Var_SetInt(r, v1->_int % v2); break;
		case eOP_LSH: Var_SetInt(r, v1->_int << v2); break;
		case eOP_RSH: Var_SetInt(r, v1->_int >> v2); break;
		case eOP_AND: Var_SetInt(r, v1->_int & v2); break;
		case eOP__OR: Var_SetInt(r, v1->_int | v2); break;
		case eOP_XOR: Var_SetInt(r, v1->_int ^ v2); break;
		default: SetError("operator Error"); break;
		}
		return;
	case VAR_FLOAT:
		switch (op)
		{
		case eOP_ADD: Var_SetFloat(r, v1->_float + v2); break;
		case eOP_SUB: Var_SetFloat(r, v1->_float - v2); break;
		case eOP_MUL: Var_SetFloat(r, v1->_float * v2); break;
		case eOP_DIV: Var_SetFloat(r, v1->_float / v2); break;
		default: SetError("operator Error"); break;
		}
		return;
	case VAR_STRING:
		break;
	case VAR_TABLE:
	{
		VarInfo vv2 = v2;
		switch (op)
		{
		case eOP_ADD:
			if (Call_MetaTable(v1, g_meta_Add3, r, v1, &vv2)) return;
			SetError("unsupported operand + Error"); break;
		case eOP_SUB:
			if (Call_MetaTable(v1, g_meta_Sub3, r, v1, &vv2)) return;
			SetError("unsupported operand - Error"); break;
		case eOP_MUL:
			if (Call_MetaTable(v1, g_meta_Mul3, r, v1, &vv2)) return;
			SetError("unsupported operand * Error"); break;
		case eOP_DIV:
			if (Call_MetaTable(v1, g_meta_Div3, r, v1, &vv2)) return;
			SetError("unsupported operand / Error"); break;
		case eOP_PER:
			if (Call_MetaTable(v1, g_meta_Per3, r, v1, &vv2)) return;
			SetError("unsupported operand % Error"); break;
		default: SetError("operator Error"); break;
		}
		break;
	}
	case VAR_LIST:
	{
		VarInfo vv2 = v2;
		switch (op)
		{
		case eOP_ADD:
			if (neo_DCalllibs::List_Add(this, r, v1, &vv2)) return;
			SetError("unsupported operand + Error"); break;
		default: SetError("operator Error"); break;
		}
		break;
	}
	case VAR_SET:
	{
		VarInfo vv2 = v2;
		switch (op)
		{
		case eOP_ADD:
			SetError("unsupported operand + Error"); break;
		case eOP_SUB:
			if (neo_DCalllibs::Set_Sub(this, r, v1, &vv2)) return;
			SetError("unsupported operand - Error"); break;
		default: SetError("operator Error"); break;
		}
		return;
	}
	}
	SetError("unsupported operand Error");
}
void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, int v1, VarInfo* v2)
{
	if (v2->GetType() == VAR_INT)
	{
		switch (op)
		{
		case eOP_ADD: Var_SetInt(r, v1 + v2->_int); break;
		case eOP_SUB: Var_SetInt(r, v1 - v2->_int); break;
		case eOP_MUL: Var_SetInt(r, v1 * v2->_int); break;
		case eOP_DIV: Var_SetInt(r, v1 / v2->_int); break;
		case eOP_PER: Var_SetInt(r, v1 % v2->_int); break;
		case eOP_LSH: Var_SetInt(r, v1 << v2->_int); break;
		case eOP_RSH: Var_SetInt(r, v1 >> v2->_int); break;
		case eOP_AND: Var_SetInt(r, v1 & v2->_int); break;
		case eOP__OR: Var_SetInt(r, v1 | v2->_int); break;
		case eOP_XOR: Var_SetInt(r, v1 ^ v2->_int); break;
		default: SetError("operator Error"); break;
		}
		return;
	}
	else if (v2->GetType() == VAR_FLOAT)
	{
		switch (op)
		{
		case eOP_ADD: Var_SetFloat(r, v1 + v2->_float); break;
		case eOP_SUB: Var_SetFloat(r, v1 - v2->_float); break;
		case eOP_MUL: Var_SetFloat(r, v1 * v2->_float); break;
		case eOP_DIV: Var_SetFloat(r, v1 / v2->_float); break;
		default: SetError("operator Error"); break;
		}
		return;
	}
	SetError("unsupported operand Error");
}
void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, int v1, int v2)
{
	switch (op)
	{
	case eOP_ADD: Var_SetInt(r, v1 + v2); return;
	case eOP_SUB: Var_SetInt(r, v1 - v2); return;
	case eOP_MUL: Var_SetInt(r, v1 * v2); return;
	case eOP_DIV: Var_SetInt(r, v1 / v2); return;
	case eOP_PER: Var_SetInt(r, v1 % v2); return;
	case eOP_LSH: Var_SetInt(r, v1 << v2); return;
	case eOP_RSH: Var_SetInt(r, v1 >> v2); return;
	case eOP_AND: Var_SetInt(r, v1 & v2); break;
	case eOP__OR: Var_SetInt(r, v1 | v2); break;
	case eOP_XOR: Var_SetInt(r, v1 ^ v2); break;
	default: SetError("operator Error"); break;
	}
	SetError("unsupported operand Error");
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
	switch (v1->GetType())
	{
	case VAR_NONE:
		if (v2->GetType() == VAR_NONE)
			return true;
		break;
	case VAR_BOOL:
		if (v2->GetType() == VAR_BOOL)
			return v1->_bl == v2->_bl;
		break;
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
			return v1->_int == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int == v2->_float;
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
			return v1->_float == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float == v2->_float;
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
			return v1->_str->_str == v2->_str->_str;
		break;
	}
	return false;
}
bool CNeoVMWorker::CompareGR(VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
			return v1->_int > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int > v2->_float;
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
			return v1->_float > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float > v2->_float;
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
			return v1->_str->_str > v2->_str->_str;
		break;
	}
	SetError("CompareGR Error");
	return false;
}
bool CNeoVMWorker::CompareGE(VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if(v2->GetType() == VAR_INT)
			return v1->_int >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int >= v2->_float;
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
			return v1->_float >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float >= v2->_float;
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
			return v1->_str->_str >= v2->_str->_str;
		break;
	}
	SetError("CompareGE Error");
	return false;
}
bool CNeoVMWorker::For(VarInfo* pCur)
{
	VarInfo* pCur_Inter = pCur + 1;
	VarInfo* pBegin		= pCur + 2;
	VarInfo* pEnd		= pCur + 3;
	VarInfo* pStep		= pCur + 4;

	Move(pCur, pCur_Inter);
	if (pCur->_int < pEnd->_int)
	{
		pCur_Inter->_int += pStep->_int;
		return true;
	}
	return false;
}
bool CNeoVMWorker::ForEach(VarInfo* pClt, VarInfo* pKey)
{
	VarInfo* pValue = pKey + 1;
	VarInfo* pIterator = pKey + 2;

	switch (pClt->GetType())
	{
	case VAR_STRING:
		{
			std::string* str = &pClt->_str->_str;
			if (pIterator->GetType() != VAR_ITERATOR)
			{
				Var_Release(pIterator);
				if (0 < (int)str->length())
				{
					pIterator->_it._iStringOffset = 0;
					pIterator->SetType(VAR_ITERATOR);
				}
				else
					return false;
			}

			if (pIterator->_it._iStringOffset < (int)str->length())
			{
				std::string s = utf_string::UTF8_ONE(*str, pIterator->_it._iStringOffset);
				Var_SetStringA(pKey, s);
				return true;
			}
			else
			{
				pIterator->ClearType();
				return false;
			}
			break;
		}
	case VAR_TABLE:
		{
			TableInfo* tbl = pClt->_tbl;
			if (pIterator->GetType() != VAR_ITERATOR)
			{
				Var_Release(pIterator);
				if (0 < tbl->GetCount())
				{
					pIterator->_it = tbl->FirstNode();
					pIterator->SetType(VAR_ITERATOR);
				}
				else
					return false;
			}
			else
				tbl->NextNode(pIterator->_it);

			TableNode* n = pIterator->_it._pTableNode;
			if (n)
			{
				Move(pKey, &n->key);
				Move(pValue, &n->value);
				return true;
			}
			else
			{
				pIterator->ClearType();
				return false;
			}
			break;
		}
	case VAR_LIST:
		{
			ListInfo* lst = pClt->_lst;
			if (pIterator->GetType() != VAR_ITERATOR)
			{
				Var_Release(pIterator);
				if (0 < lst->GetCount())
				{
					pIterator->_it._iListOffset = 0;
					pIterator->SetType(VAR_ITERATOR);
				}
				else
					return false;
			}
			else
				++pIterator->_it._iListOffset;

			if (pIterator->_it._iListOffset < lst->GetCount())
			{
				lst->GetValue(pIterator->_it._iListOffset, pKey);
				//Move(pValue, &n->value);
				return true;
			}
			else
			{
				pIterator->ClearType();
				return false;
			}
			break;
		}
	case VAR_SET:
		{
			SetInfo* set = pClt->_set;
			if (pIterator->GetType() != VAR_ITERATOR)
			{
				Var_Release(pIterator);
				if (0 < set->GetCount())
				{
					pIterator->_it = set->FirstNode();
					pIterator->SetType(VAR_ITERATOR);
				}
				else
					return false;
			}
			else
				set->NextNode(pIterator->_it);

			SetNode* n = pIterator->_it._pSetNode;
			if (n)
			{
				Move(pKey, &n->key);
				return true;
			}
			else
			{
				pIterator->ClearType();
				return false;
			}
			break;
		}
	}
	SetErrorFormat("error : foreach not support '%s'", GetDataType(pClt->GetType()).c_str());
	return false;

//	SetError("foreach table key Error");
//	return false;
}
// -1 : Error
//  0 : SlieceRun
//  1 : Continue
//  2 : Timeout - 
int CNeoVMWorker::Sleep(int iTimeout, VarInfo* v1)
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

	if (iTimeout >= 0)
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
		return "map";
	case VAR_LIST:
		return "list";
	case VAR_SET:
		return "set";
	case VAR_COROUTINE:
		return "coroutine";
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
		return (int)v1->_tbl->_itemCount;
	default:
		break;
	}
	return 0;
}
VarInfo* CNeoVMWorker::GetType(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return &_pVM->m_sDefaultValue[NDF_NULL];
	case VAR_BOOL:
		return &_pVM->m_sDefaultValue[NDF_BOOL];
	case VAR_INT:
		return &_pVM->m_sDefaultValue[NDF_INT];
	case VAR_FLOAT:
		return &_pVM->m_sDefaultValue[NDF_FLOAT];
	case VAR_STRING:
		return &_pVM->m_sDefaultValue[NDF_STRING];
	case VAR_TABLE:
		return &_pVM->m_sDefaultValue[NDF_TABLE];
	case VAR_COROUTINE:
		return &_pVM->m_sDefaultValue[NDF_COROUTINE];
	case VAR_FUN:
	case VAR_FUN_NATIVE:
		return &_pVM->m_sDefaultValue[NDF_FUNCTION];
	default:
		break;
	}
	return &_pVM->m_sDefaultValue[NDF_NULL];
}

void CNeoVMWorker::Call(FunctionPtr* fun, int n2, VarInfo* pReturnValue)
{
	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);

	if (fun->_func == NULL)
	{	// Error
		SetError("Ptr Call is null");
		return;
	}

	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;

	if ((*fun->_fn)(this, fun, n2) < 0)
	{
		SetError("Ptr Call Argument Count Error");
		return;
	}
	_iSP_Vars = iSave;
}

void CNeoVMWorker::Call(int n1, int n2, VarInfo* pReturnValue)
{
	SFunctionTable& fun = m_sFunctionPtr[n1];
	// n2 is Arg Count not use
	SCallStack callStack;
	callStack._iReturnOffset = GetCodeptr();
	callStack._iSP_Vars = _iSP_Vars;
	callStack._iSP_VarsMax = _iSP_VarsMax;
	callStack._pReturnValue = pReturnValue;
	m_pCallStack->push_back(callStack);

	SetCodePtr(fun._codePtr);
	_iSP_Vars = _iSP_VarsMax;
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
	if (_iSP_Vars_Max2 < _iSP_VarsMax)
		_iSP_Vars_Max2 = _iSP_VarsMax;
}

bool CNeoVMWorker::Call_MetaTable(VarInfo* pTable, std::string& funName, VarInfo* r, VarInfo* a, VarInfo* b)
{
	if (pTable->_tbl->_meta == NULL)
		return false;
	VarInfo* pVarItem = pTable->_tbl->_meta->Find(funName);
	if (pVarItem == NULL)
		return false;

	int n3 = 2;

	Move(&(*m_pVarStack)[_iSP_VarsMax + 1], a);
	Move(&(*m_pVarStack)[_iSP_VarsMax + 2], b);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

	if (pVarItem->GetType() == VAR_FUN)
		Call(pVarItem->_fun_index, n3, r);

	//Move(r, &(*m_pVarStack)[iSP_VarsMax]); ???
	return true;
}


bool CNeoVMWorker::Call_MetaTable2(VarInfo* pTable, std::string& funName, VarInfo* r, VarInfo* b)
{
	if (pTable->_tbl->_meta == NULL)
		return false;
	VarInfo* pVarItem = pTable->_tbl->_meta->Find(funName);
	if (pVarItem == NULL)
		return false;

	int n3 = 2;

	Move(&(*m_pVarStack)[_iSP_VarsMax + 1], r);
	Move(&(*m_pVarStack)[_iSP_VarsMax + 2], b);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

	if (pVarItem->GetType() == VAR_FUN)
		Call(pVarItem->_fun_index, n3, NULL);

	return true;
}
bool CNeoVMWorker::Call_MetaTableI(VarInfo* pTable, std::string& funName, VarInfo* r, VarInfo* a, int b)
{
	if (pTable->_tbl->_meta == NULL)
		return false;
	VarInfo* pVarItem = pTable->_tbl->_meta->Find(funName);
	if (pVarItem == NULL)
		return false;

	int n3 = 2;

	VarInfo tmp;
	tmp.SetType(VAR_INT);
	tmp._int = b;

	Move(&(*m_pVarStack)[_iSP_VarsMax + 1], a);
	Move(&(*m_pVarStack)[_iSP_VarsMax + 2], &tmp);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

	if (pVarItem->GetType() == VAR_FUN)
		Call(pVarItem->_fun_index, n3, r);

	//Move(r, &(*m_pVarStack)[iSP_VarsMax]); ???
	return true;
}


static void ReadString(CNArchive& ar, std::string& str)
{
	short nLen;
	ar >> nLen;
	str.resize(nLen);

	ar.Read((char*)str.data(), nLen);
}
bool CNeoVMWorker::Init(void* pBuffer, int iSize, int iStackSize)
{
	_BytesSize = iSize;
	CNArchive ar(pBuffer, iSize);
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));
	ar >> header;
	_header = header;

	if (header._dwFileType != FILE_NEOS)
	{
		return false;
	}
	if (header._dwNeoVersion != NEO_VER)
	{
		return false;
	}


	u8* pCode = new u8[header._iCodeSize];
	SetCodeData(pCode, header._iCodeSize);
	ar.Read(pCode, header._iCodeSize);

	m_sFunctionPtr.resize(header._iFunctionCount);

	int iID;
	SFunctionTable fun;
	std::string Name;
	for (int i = 0; i < header._iFunctionCount; i++)
	{
		memset(&fun, 0, sizeof(SFunctionTable));

		ar >> iID >> fun._codePtr >> fun._argsCount >> fun._localTempMax >> fun._localVarCount >> fun._funType;
		if (fun._funType != FUNT_NORMAL && fun._funType != FUNT_ANONYMOUS)
		{
			ReadString(ar, Name);
			m_sImExportTable[Name] = iID;
		}

		fun._localAddCount = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;
		m_sFunctionPtr[iID] = fun;
	}

	for (int i = 0; i < header._iExportVarCount; i++)
	{
		int idx;
		ar >> idx;
		ReadString(ar, Name);
		m_sImportVars[Name] = idx;
	}

	std::string tempStr;
	int iMaxVar = header._iStaticVarCount + header._iGlobalVarCount;
	m_sVarGlobal.resize(iMaxVar);
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = m_sVarGlobal[i];
		Var_Release(&vi);

		VAR_TYPE type;
		ar >> type;
		vi.SetType(type);
		switch (type)
		{
		case VAR_INT:
			ar >> vi._int;
			break;
		case VAR_FLOAT:
			ar >> vi._float;
			break;
		case VAR_BOOL:
			ar >> vi._bl;
			break;
		case VAR_STRING:
			ReadString(ar, tempStr);
			vi._str = _pVM->StringAlloc(tempStr);
			vi._str->_refCount = 1;
			break;
		//case VAR_FUN:
		//	ar >> vi._fun_index;
		//	break;
		default:
			SetError("Error Invalid VAR Type");
			return false;
		}
	}
	for (int i = header._iStaticVarCount; i < iMaxVar; i++)
	{
		m_sVarGlobal[i].ClearType();
	}

	if (header.m_iDebugCount > 0)
	{
		_DebugData.resize(header.m_iDebugCount);
		ar.Read(&_DebugData[0], sizeof(debug_info) * header.m_iDebugCount);
	}
	return true;
}
int CNeoVMWorker::GetDebugLine()
{
	int idx = int(_pCodeCurrent - _pCodeBegin - 1) / sizeof(SVMOperation);
	if ((int)_DebugData.size() <= idx || idx < 0) return -1;
	return _DebugData[idx]._lineseq;
}
void CNeoVMWorker::SetError(const char* pErrMsg)
{
	_pVM->SetError(std::string(pErrMsg));
}
void CNeoVMWorker::SetErrorUnsupport(const char* pErrMsg, VarInfo* p)
{
	char buff[1024];
	sprintf_s(buff, _countof(buff), pErrMsg, GetDataType(p->GetType()).c_str());
	_pVM->SetError(std::string(buff));
}
void CNeoVMWorker::SetErrorFormat(const char* lpszString, ...)
{
	char buff[1024];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsprintf_s(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, 8, lpszString, arg_ptr);
#endif
	va_end(arg_ptr);

	_pVM->SetError(std::string(buff));
}

bool	CNeoVMWorker::Start(int iFunctionID, std::vector<VarInfo>& _args)
{
	if(false == Setup(iFunctionID, _args))
		return false;

	return Run();
}

bool	CNeoVMWorker::Setup(int iFunctionID, std::vector<VarInfo>& _args)
{
	SFunctionTable& fun = m_sFunctionPtr[iFunctionID];
	int iArgs = (int)_args.size();
	if (iArgs != fun._argsCount)
		return false;

	SetCodePtr(fun._codePtr);

	_iSP_Vars = 0;// _header._iStaticVarCount;
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;

	_iSP_Vars_Max2 = _iSP_VarsMax;

	if (iArgs > fun._argsCount)
		iArgs = fun._argsCount;

	int iCur;
	for (iCur = 0; iCur < iArgs; iCur++)
		(*m_pVarStack)[1 + _iSP_Vars + iCur] = _args[iCur];
	for (; iCur < fun._argsCount; iCur++)
		Var_Release(&(*m_pVarStack)[1 + _iSP_Vars + iCur]);

	_isSetup = true;
	return true;
}
bool CNeoVMWorker::BindWorkerFunction(const std::string& funName)
{
	int iFID = FindFunction(funName);
	if (iFID == -1)
		return false;

	std::vector<VarInfo> _args;
	return Setup(iFID, _args);
}

bool	CNeoVMWorker::Run(int iBreakingCallStack)
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

	SCallStack callStack;
	int iTemp;
	char chMsg[256];
	SVMOperation OP;

	int op_process = 0;
	bool isTimeout = (m_iTimeout >= 0);
	if(isTimeout)
		_preClock = clock();

	bool blDebugInfo = IsDebugInfo();
	int _lineseq = -1;
	u8 flagNum = 0;

	try
	{
		while (true)
		{
			GetOP(&OP);
			switch (OP.op)
			{
			case NOP_MOV:
				Move(GetVarPtr1(OP), GetVarPtr2(OP)); break;
				//switch ((OP.argFlag >> 3) & 0x2)
				//{
				//case 0: Move(GetVarPtr1(OP), GetVarPtr2(OP)); break;
				//case 2: MoveI(GetVarPtr1(OP), OP.n2); break;
				//}
				//break;
			//case NOP_MOVI:
			//	MoveI(GetVarPtr1(OP), OP.n2);
			//	break;

			case NOP_MOV_MINUS:
				MoveMinus(GetVarPtr1(OP), GetVarPtr2(OP)); break;
				//switch ((OP.argFlag >> 3) & 0x2)
				//{
				//case 0: MoveMinus(GetVarPtr1(OP), GetVarPtr2(OP)); break;
				//case 2: MoveMinusI(GetVarPtr1(OP), OP.n2); break;
				//}
				//break;
			case NOP_ADD2:
			case NOP_SUB2:
			case NOP_MUL2:
			case NOP_DIV2:
			case NOP_PERSENT2:
			case NOP_LSHIFT2:
			case NOP_RSHIFT2:
			case NOP_AND2:
			case NOP_OR2:
			case NOP_XOR2:
				Add2((eNOperationSub)(OP.op - NOP_ADD2), GetVarPtr1(OP), GetVarPtr2(OP)); break;
				//switch ((OP.argFlag >> 3) & 0x2)
				//{
				//case 0: Add2((eNOperationSub)(OP.op - NOP_ADD2), GetVarPtr1(OP), GetVarPtr2(OP)); break;
				//case 2: Add2((eNOperationSub)(OP.op - NOP_ADD2), GetVarPtr1(OP), OP.n2); break;
				//}
				//break;

			case NOP_VAR_CLEAR:
				Var_Release(GetVarPtr1(OP));
				break;
			case NOP_INC:
				Inc(GetVarPtr1(OP));
				break;
			case NOP_DEC:
				Dec(GetVarPtr1(OP));
				break;

			case NOP_ADD3:
			case NOP_SUB3:
			case NOP_MUL3:
			case NOP_DIV3:
			case NOP_PERSENT3:
			case NOP_LSHIFT3:
			case NOP_RSHIFT3:
			case NOP_AND3:
			case NOP_OR3:
			case NOP_XOR3:
					Add((eNOperationSub)(OP.op - NOP_ADD3), GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;

			case NOP_GREAT:		// >
				Var_SetBool(GetVarPtr1(OP), CompareGR(GetVarPtr2(OP), GetVarPtr3(OP)));
				break;
			case NOP_GREAT_EQ:	// >=
				Var_SetBool(GetVarPtr1(OP), CompareGE(GetVarPtr2(OP), GetVarPtr3(OP)));
				break;
			case NOP_LESS:		// <
				Var_SetBool(GetVarPtr1(OP), CompareGR(GetVarPtr3(OP), GetVarPtr2(OP)));
				break;
			case NOP_LESS_EQ:	// <=
				Var_SetBool(GetVarPtr1(OP), CompareGE(GetVarPtr3(OP), GetVarPtr2(OP)));
				break;
			case NOP_EQUAL2:	// ==
				Var_SetBool(GetVarPtr1(OP), CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)));
				break;
			case NOP_NEQUAL:	// !=
				Var_SetBool(GetVarPtr1(OP), !CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)));
				break;
			case NOP_AND:		// &
				And(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_OR:		// |
				Or(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_LOG_AND:	// &&
				Var_SetBool(GetVarPtr1(OP), GetVarPtr3(OP)->IsTrue() && GetVarPtr2(OP)->IsTrue());
				break;
			case NOP_LOG_OR:	// ||
				Var_SetBool(GetVarPtr1(OP), GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue());
				break;

			case NOP_JMP:
				SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_FALSE:
				if (false == GetVarPtr2(OP)->IsTrue())
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_TRUE:
				if (true == GetVarPtr2(OP)->IsTrue())
					SetCodeIncPtr(OP.n1);
				break;


			case NOP_JMP_GREAT:		// >
				if (true == CompareGR(GetVarPtr2(OP), GetVarPtr3(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_GREAT_EQ:	// >=
				if (true == CompareGE(GetVarPtr2(OP), GetVarPtr3(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_LESS:		// <
				if (true == CompareGR(GetVarPtr3(OP), GetVarPtr2(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_LESS_EQ:	// <=
				if (true == CompareGE(GetVarPtr3(OP), GetVarPtr2(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_EQUAL2:	// ==
				if (true == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_NEQUAL:	// !=
				if (false == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_AND:	// &&
				if (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue())
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_OR:		// ||
				if (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue())
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_NAND:	// !(&&)
				if (false == (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue()))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_NOR:	// !(||)
				if (false == (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_FOR:	// for (cur, cur_protect, begin, end, step)
				//if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP), GetVarPtr(OP.n3 + 1)))
				if (For(GetVarPtr2(OP)))
					SetCodeIncPtr(OP.n1);
				break;
			case NOP_JMP_FOREACH:	// foreach
				//if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP), GetVarPtr(OP.n3 + 1)))
				if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP)))
					SetCodeIncPtr(OP.n1);
				break;

			case NOP_STR_ADD:	// ..
				Var_SetStringA(GetVarPtr1(OP), ToString(GetVarPtr2(OP)) + ToString(GetVarPtr3(OP)));
				break;

			case NOP_TOSTRING:
				Var_SetStringA(GetVarPtr1(OP), ToString(GetVarPtr2(OP)));
				break;
			case NOP_TOINT:
				Var_SetInt(GetVarPtr1(OP), ToInt(GetVarPtr2(OP)));
				break;
			case NOP_TOFLOAT:
				Var_SetFloat(GetVarPtr1(OP), ToFloat(GetVarPtr2(OP)));
				break;
			case NOP_TOSIZE:
				Var_SetInt(GetVarPtr1(OP), ToSize(GetVarPtr2(OP)));
				break;
			case NOP_GETTYPE:
				Move(GetVarPtr1(OP), GetType(GetVarPtr2(OP)));
				break;
			case NOP_SLEEP:
				{
					int r = Sleep(m_iTimeout, GetVarPtr2(OP));
					if (r == 0)
					{
						_preClock = clock();
						return true;
					}
					else if (r == 2)
					{
						op_process = m_iCheckOpCount;
					}
				}
				break;

			case NOP_FMOV1:
				Var_SetFun(GetVarPtr1(OP), OP.n2);
				break;
			case NOP_FMOV2:
				//Var_SetFun(&_funA3, OP.n3);
				_funA3._fun_index = OP.n3;
				CltInsert(GetVarPtr1(OP), GetVarPtr2(OP), &_funA3);
				break;

			case NOP_CALL:
				Call(OP.n1, OP.n2);
				if(_pVM->IsLocalErrorMsg())
					break;
				break;
			case NOP_PTRCALL:
			{
				VarInfo* pVar1 = GetVarPtr1(OP);
				short n3 = OP.n3;
				VarInfo* pFunName = nullptr;
				switch (pVar1->GetType())
				{
				case VAR_FUN:
					if ((OP.argFlag & 0x02) == 0 && 0 == OP.n2)
						Call(pVar1->_fun_index, OP.n3);
					break;
				case VAR_FUN_NATIVE:
					if ((OP.argFlag & 0x02) == 0 && 0 == OP.n2)
						Call(pVar1->_funPtr, OP.n3);
					break;
				case VAR_STRING:
					pFunName = GetVarPtr2(OP);
					CallNative(_pVM->_funStrLib, pVar1, pFunName->_str->_str, n3);
					break;
				case VAR_TABLE:
				{
					pFunName = GetVarPtr2(OP);
					VarInfo* pVarMeta = pVar1->_tbl->Find(pFunName);
					if (pVarMeta != NULL)
					{
						if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
							_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

						if (pVarMeta->GetType() == VAR_FUN)
						{
							Call(pVarMeta->_fun_index, n3);
							break;
						}
					}
					FunctionPtrNative fun = pVar1->_tbl->_fun;
					if (fun._func)
					{
						CallNative(fun, pVar1, pFunName->_str->_str, n3);
						break;
					}
					CallNative(_pVM->_funTblLib, pVar1, pFunName->_str->_str, n3);
					break;
				}
				case VAR_LIST:
					pFunName = GetVarPtr2(OP);
					CallNative(_pVM->_funLstLib, pVar1, pFunName->_str->_str, n3);
					break;
				case VAR_SET:
					break;
				default:
					SetError("Ptr Call Error");
					break;
				}
				break;
			}
			case NOP_PTRCALL2:
			{
				short n2 = OP.n2;
				VarInfo* pFunName = GetVarPtr1(OP);
				if (pFunName->GetType() == VAR_STRING)
				{
					CallNative(_pVM->_funDefaultLib, NULL, pFunName->_str->_str, n2);
//					SetError("Ptr Call Error");
					break;
				}

				if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
					_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);
				break;
			}			
			case NOP_RETURN:
			case NOP_FUNEND:
				if (OP.n1 == 0)
					Var_Release(&(*m_pVarStack)[_iSP_Vars]); // Clear
				else
					Move(&(*m_pVarStack)[_iSP_Vars], GetVarPtr1(OP));

				//if (m_sCallStack.empty())
				if(iBreakingCallStack == (int)m_pCallStack->size())
				{
					if (iBreakingCallStack == 0 && IsMainCoroutine(m_pCur) == false)
					{
						if(StopCoroutine(true) == true) // Other Coroutine Active (No Stop)
							break;
					}
					_isSetup = false;
					return true;
				}
				iTemp = (int)m_pCallStack->size() - 1;
				callStack = (*m_pCallStack)[iTemp];
				m_pCallStack->resize(iTemp);

				if(callStack._pReturnValue)
					Move(callStack._pReturnValue, &(*m_pVarStack)[_iSP_Vars]);

				SetCodePtr(callStack._iReturnOffset);
				_iSP_Vars = callStack._iSP_Vars;
				_iSP_VarsMax = callStack._iSP_VarsMax;
				break;
			//case NOP_FUNEND:
/*				Var_Release(&(*m_pVarStack)[_iSP_Vars]); // Clear
				if (iBreakingCallStack == (int)m_pCallStack->size())
				{
					if (iBreakingCallStack == 0 && IsMainCoroutine(m_pCur) == false)
					{
						if (StopCoroutine(true) == true) // Other Coroutine Active (No Stop)
							break;
					}
					_isSetup = false;
					return true;
				}
				iTemp = (int)m_pCallStack->size() - 1;
				callStack = (*m_pCallStack)[iTemp];
				m_pCallStack->resize(iTemp);

				if (callStack._pReturnValue)
					Move(callStack._pReturnValue, &(*m_pVarStack)[_iSP_Vars]);

				SetCodePtr(callStack._iReturnOffset);
				_iSP_Vars = callStack._iSP_Vars;
				iSP_VarsMax = callStack._iSP_VarsMax;
				break;*/
			case NOP_TABLE_ALLOC:
				Var_SetTable(GetVarPtr1(OP), _pVM->TableAlloc(OP.n23));
				break;
			case NOP_CLT_READ:
				CltRead(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_TABLE_REMOVE:
				TableRemove(GetVarPtr1(OP), GetVarPtr2(OP));
				break;
			case NOP_CLT_MOV:
				CltInsert(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
				//switch ((OP.argFlag >> 3) & 0x3)
				//{
				//case 0: CltInsert(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
				//case 1: CltInsert(GetVarPtr1(OP), GetVarPtr2(OP), OP.n3); break;
				//case 2: CltInsert(GetVarPtr1(OP), OP.n2, GetVarPtr3(OP)); break;
				//case 3: CltInsert(GetVarPtr1(OP), OP.n2, OP.n3); break;
				//}
				break;
			case NOP_TABLE_ADD2:
			case NOP_TABLE_SUB2:
			case NOP_TABLE_MUL2:
			case NOP_TABLE_DIV2:
			case NOP_TABLE_PERSENT2:
				TableAdd2((eNOperationSub)(OP.op - NOP_TABLE_ADD2), GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
				//switch ((OP.argFlag >> 3) & 0x3)
				//{
				//case 0: TableAdd2((eNOperationSub)(OP.op - NOP_TABLE_ADD2), GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
				//case 1: TableAdd2((eNOperationSub)(OP.op - NOP_TABLE_ADD2), GetVarPtr1(OP), GetVarPtr2(OP), OP.n3); break;
				//case 2: TableAdd2((eNOperationSub)(OP.op - NOP_TABLE_ADD2), GetVarPtr1(OP), OP.n2, GetVarPtr3(OP)); break;
				//case 3: TableAdd2((eNOperationSub)(OP.op - NOP_TABLE_ADD2), GetVarPtr1(OP), OP.n2, OP.n3); break;
				//}
				//break;

			case NOP_LIST_ALLOC:
				Var_SetList(GetVarPtr1(OP), _pVM->ListAlloc(OP.n23));
				break;
/*			case NOP_LIST_READ:
				TableRead(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;*/
			case NOP_LIST_REMOVE:
				TableRemove(GetVarPtr1(OP), GetVarPtr2(OP));
				break;
/*			case NOP_LIST_MOV:
				TableInsert(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_LIST_ADD2:
				TableAdd2(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_LIST_SUB2:
				TableSub2(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_LIST_MUL2:
				TableMul2(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_LIST_DIV2:
				TableDiv2(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;
			case NOP_LIST_PERSENT2:
				TablePer2(GetVarPtr1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
				break;*/

			case NOP_VERIFY_TYPE:
				VerifyType(GetVarPtr1(OP), (VAR_TYPE)OP.n2);
				break;
			case NOP_YIELD:
				if (StopCoroutine(false) == false)
					return true;
				break;
			case NOP_NONE:
				break;
			default:
				SetError("Unknown OP");
				break;
			}
			if (_pVM->_bError)
			{
				m_pCallStack->clear();
				_iSP_Vars = 0;
				if(blDebugInfo)
					_lineseq = GetDebugLine();
				int idx = int((_pCodeCurrent - _pCodeBegin) / 8 - 1);
#ifdef _WIN32
				sprintf_s(chMsg, _countof(chMsg), "%s : Index(%d), Line (%d)", _pVM->_pErrorMsg.c_str(), idx, _lineseq);
#else
				sprintf(chMsg, "%s : Index(%d), Line (%d)", _pVM->_pErrorMsg, idx, _lineseq);
#endif
				if(_pVM->_sErrorMsgDetail.empty())
					_pVM->_sErrorMsgDetail = chMsg;
				return false;
			}
			if (isTimeout)
			{
				if (++op_process >= m_iCheckOpCount)
				{
					op_process = 0;
					t2 = clock() - _preClock;
					if (t2 >= m_iTimeout || t2 < 0)
						break;
				}
			}
		}
	}
	catch (...)
	{
		SetError("Exception");
		if(blDebugInfo)
			_lineseq = GetDebugLine();
#ifdef _WIN32
		sprintf_s(chMsg, _countof(chMsg), "%s : Line (%d)", _pVM->_pErrorMsg.c_str(), _lineseq);
#else
		sprintf(chMsg, "%s : Line (%d)", _pVM->_pErrorMsg, _lineseq);
#endif
		_pVM->_sErrorMsgDetail = chMsg;
		return false;
	}
	return true;
}

bool CNeoVMWorker::RunFunction(const std::string& funName, std::vector<VarInfo>& _args)
{
	auto it = m_sImExportTable.find(funName);
	if (it == m_sImExportTable.end())
	{
		SetError("Function Not Found");
		_pVM->_sErrorMsgDetail = _pVM->_pErrorMsg;
		_pVM->_sErrorMsgDetail += "(";
		_pVM->_sErrorMsgDetail += funName;
		_pVM->_sErrorMsgDetail += ")";
		return false;
	}

	int iID = (*it).second;
	Start(iID, _args);

	return true;
}
void CNeoVMWorker::PushString(const char* p)
{
	std::string s(p);
	VarInfo d;
	d.SetType(VAR_STRING);
	d._str = _pVM->StringAlloc(s);
	_args->push_back(d);
}
void CNeoVMWorker::PushNeoFunction(NeoFunction v)
{
	VarInfo d;
	if (v._fun_index >= 0 && v._pWorker == this)
	{
		d.SetType(VAR_FUN);
		d._fun_index = v._fun_index;
	}
	else if (v._fun._func)
	{
		d.SetType(VAR_FUN_NATIVE);
		d._funPtr = _pVM->FunctionPtrAlloc(&v._fun);
	}
	else
		d.ClearType();
	_args->push_back(d);
}
void	CNeoVMWorker::DeadCoroutine(CoroutineInfo* pCI)
{
	pCI->_state = COROUTINE_STATE_DEAD;
	int iSP_Vars_Max2 = pCI->_info._iSP_Vars_Max2;
	std::vector<VarInfo>& sVarStack = pCI->m_sVarStack;
	for (int i = 0; i < iSP_Vars_Max2; i++)
		Var_Release(&(sVarStack)[i]);

	pCI->_info.ClearSP();
}
bool CNeoVMWorker::StopCoroutine(bool doDead)
{
	CoroutineBase* pThis = (CoroutineBase*)this;
	m_pCur->_info = *pThis;
	if (doDead)
		DeadCoroutine(m_pCur);
	else
		m_pCur->_state = COROUTINE_STATE_SUSPENDED;

	if (m_sCoroutines.empty() == true)
	{
		return false;
	}
	else
	{
		auto it = m_sCoroutines.begin();
		m_pCur = (*it);
		m_sCoroutines.erase(it);
		if(m_pCur->_state != COROUTINE_STATE_NORMAL)
		{
			SetError("Coroutine State Error");
			return false;
		}
		m_pCur->_state = COROUTINE_STATE_RUNNING;
		m_pVarStack = &m_pCur->m_sVarStack;
		m_pCallStack = &m_pCur->m_sCallStack;

		*pThis = m_pCur->_info;
	}
	return true;
}

bool CNeoVMWorker::StartCoroutione(int n3)
{
	CoroutineBase* pThis = (CoroutineBase*)this;
	if (m_pCur)
	{
		// Back up
		m_pCur->_info = *pThis;
		//m_pCur->_info._iSP_Vars = sp;// _iSP_Vars;
		m_pCur->_state = COROUTINE_STATE_NORMAL;
		m_sCoroutines.push_front(m_pCur);
		CoroutineInfo* pPre = m_pCur;

		m_pCur = m_pRegisterActive;
		m_pCur->_state = COROUTINE_STATE_RUNNING;
		m_pCur->_sub_state = COROUTINE_SUB_NORMAL;
		m_pVarStack = &m_pCur->m_sVarStack;
		m_pCallStack = &m_pCur->m_sCallStack;

		if (m_pCur->_info._pCodeCurrent == NULL) // first run
		{
			int iResumeParamCount = n3 - 1;
			SFunctionTable& fun = m_sFunctionPtr[m_pCur->_fun_index];
			for (int i = 0; i < fun._argsCount; i++)
			{
				if (i < iResumeParamCount)
					Move(&m_pCur->m_sVarStack[i + 1], &pPre->m_sVarStack[i + _iSP_Vars + 2]);
				else
					Var_Release(&m_pCur->m_sVarStack[i + 1]); // Zero index return value
			}
			SetCodePtr(fun._codePtr);
			_iSP_Vars = 0;// iSP_VarsMax;
			_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
			_iSP_Vars_Max2 = _iSP_VarsMax;
			m_pCur->_info._pCodeCurrent = _pCodeCurrent;
		}
		else
		{
			*pThis = m_pCur->_info;
		}
	}
	m_pRegisterActive = NULL;
	return true;
}


VarInfo* CNeoVMWorker::testCall(int iFID, VarInfo* args, int argc)
{
	if (_isSetup == false)
		return NULL;

	SFunctionTable& fun = m_sFunctionPtr[iFID];
	if (argc != fun._argsCount)
		return NULL;

	int save_Code = GetCodeptr();
	int save_iSP_Vars = _iSP_Vars;
	int save__iSP_VarsMax = _iSP_VarsMax;

	SetCodePtr(fun._codePtr);

//	_iSP_Vars = 0;// _header._iStaticVarCount;
	_iSP_Vars = _iSP_VarsMax;
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
	if (_iSP_Vars_Max2 < _iSP_VarsMax)
		_iSP_Vars_Max2 = _iSP_VarsMax;

	//_iSP_Vars_Max2 = iSP_VarsMax;

	if (argc > fun._argsCount)
		argc = fun._argsCount;

	int iCur;
	for (iCur = 0; iCur < argc; iCur++)
		Move(&(*m_pVarStack)[1 + _iSP_Vars + iCur], &args[iCur]);
	for (; iCur < fun._argsCount; iCur++)
		Var_Release(&(*m_pVarStack)[1 + _iSP_Vars + iCur]);

	Run((int)m_pCallStack->size());

//	_read(&(*m_pVarStack)[_iSP_Vars], *r);
	VarInfo* r = &(*m_pVarStack)[_iSP_Vars];

	SetCodePtr(save_Code);
	_iSP_Vars = save_iSP_Vars;
	_iSP_VarsMax = save__iSP_VarsMax;

	//GC();
	
	_isSetup = true;
	return r;
}
bool CNeoVMWorker::CallNative(FunctionPtrNative functionPtrNative, VarInfo* pFunObj, const std::string& fname, int n3)
{
	Neo_NativeFunction func = functionPtrNative._func;
	if (func == NULL)
	{
		SetError("Ptr Call Error");
		return false;
	}
	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;
	if (_iSP_Vars_Max2 < _iSP_VarsMax + 1 + n3) // ??????? 이렇게 하면 맞나? 흠...
		_iSP_Vars_Max2 = _iSP_VarsMax + 1 + n3;

	if ((func)(this, pFunObj, fname, n3) == false)
	{
		SetError("Ptr Call Error");
		return false;
	}
	_iSP_Vars = iSave;
	if (m_pRegisterActive != NULL)
	{
		switch(m_pRegisterActive->_sub_state)
		{ 
			case COROUTINE_SUB_START:
				StartCoroutione(n3);
				break;
			case COROUTINE_SUB_CLOSE:
				switch (m_pRegisterActive->_state)
				{
					case COROUTINE_STATE_SUSPENDED:
						DeadCoroutine(m_pRegisterActive);
						break;
					case COROUTINE_STATE_RUNNING:
						if (m_pCur != m_pRegisterActive)SetError("Coroutine Error 1");
						else							StopCoroutine(true);
						break;
					case COROUTINE_STATE_DEAD:
						break;
					case COROUTINE_STATE_NORMAL:
						DeadCoroutine(m_pRegisterActive);
						for(auto it = m_sCoroutines.begin(); it != m_sCoroutines.end(); it++)
						{
							if((*it) == m_pRegisterActive)
							{
								m_sCoroutines.erase(it);
								break;
							}
						}
						break;
				}
				m_pRegisterActive = NULL;
				break;
			default:
				SetError("Coroutine Error 1");
				break;
		}
	}

	return true;
}

bool CNeoVMWorker::VerifyType(VarInfo *p, VAR_TYPE t)
{
	if (p->GetType() == t)
		return true;
	char ch[256];
#ifdef _WIN32
	sprintf_s(ch, _countof(ch), "VerifyType (%s != %s)", GetDataType(p->GetType()).c_str(), GetDataType(t).c_str());
#else
	sprintf(ch, "VerifyType (%s != %s)", GetDataType(p->GetType()).c_str(), GetDataType(t).c_str());
#endif

	SetError(ch);
	return false;
}

std::string GetDataType(VAR_TYPE t)
{
	switch (t)
	{
	case VAR_NONE:
		return "none";
	case VAR_BOOL:
		return "bool";
	case VAR_INT:
		return "int";
	case VAR_FLOAT:
		return "float";
	case VAR_FUN:
		return "fun";
	case VAR_ITERATOR:
		return "iterator";
	case VAR_FUN_NATIVE:
		return "fun";
	case VAR_STRING:
		return "string";
	case VAR_TABLE:
		return "map";
	case VAR_LIST:
		return "list";
	case VAR_SET:
		return "set";
	case VAR_COROUTINE:
		return "coroutine";
	default:
		break;
	}
	return "unknown";
}