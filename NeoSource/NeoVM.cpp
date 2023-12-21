#include <math.h>
#include <stdlib.h>
#include "NeoVMImpl.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"
#include "UTFString.h"

namespace NeoScript
{

INeoVM* 	INeoVM::CreateVM()
{
	CNeoVMImpl* p = new CNeoVMImpl();
	return p;
}
void		INeoVM::ReleaseVM(INeoVM* pVM)
{
	delete (CNeoVMImpl*)pVM;
}

FunctionPtrNative INeoVM::RegisterNative(Neo_NativeFunction func)
{
	FunctionPtrNative fun;
	CNeoVMWorker::neo_pushcclosureNative(&fun, func);
	return fun;
}
bool INeoVM::Call_TL() // Time Limit
{
	return ((CNeoVMWorker*)_pMainWorker)->CallN_TL();
}

VarInfo* INeoVM::GetVar(const std::string& name)
{
	return ((CNeoVMWorker*)_pMainWorker)->GetVar(name);
}

bool	INeoVM::RegisterTableCallBack(VarInfo* p, void* pUserData, Neo_NativeFunction func, Neo_NativeProperty property)
{
	if (p == nullptr || p->GetType() != VAR_MAP) return false;

	MapInfo* pTable = p->_tbl;
	pTable->_pUserData = pUserData;
	CNeoVMWorker::neo_pushcclosureNative(&pTable->_fun, func);
	CNeoVMWorker::neo_pushcclosureNative(&pTable->_fun, property);

	return true;
}

//void INeoVM::Var_AddRef(VarInfo* d)
//{
//	switch (d->GetType())
//	{
//	case VAR_STRING:
//		++d->_str->_refCount;
//		break;
//	case VAR_MAP:
//		++d->_tbl->_refCount;
//		break;
//	case VAR_LIST:
//		++d->_lst->_refCount;
//		break;
//	case VAR_SET:
//		++d->_set->_refCount;
//		break;
//	case VAR_COROUTINE:
//		++d->_cor->_refCount;
//		break;
//	case VAR_MODULE:
//		++((CNeoVMWorker*)(d->_module))->_refCount;
//		break;
//	case VAR_ASYNC:
//		++d->_async->_refCount;
//		break;
//	default:
//		break;
//	}
//}
//
//void INeoVM::Move_DestNoRelease(VarInfo* v1, VarInfo* v2)
//{
//	v1->SetType(v2->GetType());
//	switch (v2->GetType())
//	{
//	case VAR_NONE: break;
//	case VAR_INT: v1->_int = v2->_int; break;
//	case VAR_FLOAT: v1->_float = v2->_float; break;
//	case VAR_BOOL: v1->_bl = v2->_bl; break;
//	case VAR_FUN: v1->_fun_index = v2->_fun_index; break;
//	case VAR_FUN_NATIVE: v1->_funPtr = v2->_funPtr; break;
//	case VAR_CHAR: v1->_c = v2->_c; break;
//
//	case VAR_STRING: v1->_str = v2->_str; ++v1->_str->_refCount; break;
//	case VAR_MAP: v1->_tbl = v2->_tbl; ++v1->_tbl->_refCount; break;
//	case VAR_LIST: v1->_lst = v2->_lst; ++v1->_lst->_refCount; break;
//	case VAR_SET: v1->_set = v2->_set; ++v1->_set->_refCount; break;
//	case VAR_COROUTINE: v1->_cor = v2->_cor; ++v1->_cor->_refCount; break;
//	case VAR_MODULE: v1->_module = v2->_module; ++((CNeoVMWorker*)(v1->_module))->_refCount; break;
//	case VAR_ASYNC: v1->_async = v2->_async; ++v1->_async->_refCount; break;
//	default: break;
//	}
//}
void INeoVM::Var_ReleaseInternal(VarInfo* d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeString(d);
		d->_str = NULL;
		break;
	case VAR_MAP:
		if (--d->_tbl->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeTable(d->_tbl);
		d->_tbl = NULL;
		break;
	case VAR_LIST:
		if (--d->_lst->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeList(d->_lst);
		d->_lst = NULL;
		break;
	case VAR_SET:
		if (--d->_set->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeSet(d->_set);
		d->_set = NULL;
		break;
	case VAR_COROUTINE:
		if (--d->_cor->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeCoroutine(d);
		d->_cor = NULL;
		break;
	case VAR_MODULE:
		if (--((CNeoVMWorker*)(d->_module))->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeWorker((CNeoVMWorker*)d->_module);
		d->_module = NULL;
		break;
	case VAR_ASYNC:
		if (--d->_async->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeAsync(d);
		d->_async = NULL;
		break;
	default:
		break;
	}
	d->ClearType();
}
/// <summary>
/// INeoVMWorker 
/// </summary>
/// <param name="d"></param>


void INeoVMWorker::PushString(const char* p)
{
	std::string s(p);
	VarInfo d;
	d.SetType(VAR_STRING);
	d._str = ((CNeoVMImpl*)_pVM)->StringAlloc(s);
	_args->push_back(d);
}
void INeoVMWorker::PushNeoFunction(NeoFunction v)
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
		d._funPtr = ((CNeoVMImpl*)_pVM)->FunctionPtrAlloc(&v._fun);
	}
	else
		d.ClearType();
	_args->push_back(d);
}
const char* INeoVMWorker::PopString(VarInfo* V)
{
	if (V->GetType() == VAR_STRING)
		return V->_str->_str.c_str();
	else if (V->GetType() == VAR_CHAR)
		return V->_c.c;

	return NULL;
}
const std::string* INeoVMWorker::PopStlString(VarInfo* V)
{
	//		if (V->GetType() == VAR_STRING)
	//			return &V->_str->_str;

	return NULL;
}

bool VarInfo::MapInsertFloat(const std::string& pKey, NS_FLOAT value)
{
	if(_type != VAR_MAP) return false;
	return _tbl->Insert(pKey, value);
}
bool VarInfo::MapFindFloat(const std::string& pKey, NS_FLOAT& value)
{
	if (_type != VAR_MAP) return false;
	VarInfo* p = _tbl->Find(pKey);
	if(p == NULL) return false;
	if(p->GetType() != VAR_FLOAT) return false;
	value = p->_float;
	return true;
}

bool VarInfo::ListInsertFloat(int idx, NS_FLOAT value)
{
	if (_type != VAR_LIST) return false;
	return _lst->SetValue(idx, value);
}
bool VarInfo::ListFindFloat(int idx, NS_FLOAT& value)
{
	VarInfo* p = _lst->GetValue(idx);
	if(p == nullptr) return false;
	if (p->GetType() == VAR_INT)
	{
		value = (NS_FLOAT)p->_int;
		return true;
	}
	else if (p->GetType() == VAR_FLOAT)
	{
		value = p->_float;
		return true;
	}
	return false;
}
bool VarInfo::SetListIndexer(VMHash<int>* pIndexer)
{
	if (_type != VAR_LIST) return false;
	_lst->_pIndexer = pIndexer;
	return true;
}

};
