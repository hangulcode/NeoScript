#pragma once

// INeoVMWorker 의 인라인 정의.
//
// 이 함수들은 VM 내부뿐 아니라 네이티브 라이브러리(NeoLib.cpp 등) 여러 TU 에서 호출된다.
// 따라서 정의가 호출하는 모든 TU 에 보여야 한다 — NeoVMWorker.inl 처럼 한 TU 에서만
// include 하면 inline 함수의 아웃오브라인 실체가 아무 데서도 생기지 않아 링크가 깨진다.
// (MSVC 는 LTCG 가 TU 경계를 넘어 찾아줘서 가려졌고, g++/ld 에서 undefined reference 로 드러났다)
// 그래서 이 파일은 NeoVMWorker.h 끝에서 include 한다.

NEOS_FORCEINLINE void INeoVMWorker::Var_Release(VarInfo* d)
{
	if (d->IsAllocType())
		_pVM->Var_ReleaseInternal(d);
	else
		d->ClearType();
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetInt(VarInfo* d, int v)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_INT);
	d->_int = v;
}

NEOS_FORCEINLINE void INeoVMWorker::Var_SetFloat(VarInfo* d, NS_FLOAT v)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_FLOAT);
	d->_float = v;
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetNone(VarInfo* d)
{
	if (d->GetType() != VAR_NONE)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->ClearType();
	}
}

NEOS_FORCEINLINE void INeoVMWorker::Var_SetBool(VarInfo* d, bool v)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_BOOL);
	d->_bl = v;
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetFun(VarInfo* d, int fun_index)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_FUN);
	d->_fun_index = fun_index;
}
inline void INeoVMWorker::Var_SetCoroutine(VarInfo* d, CoroutineInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_COROUTINE);
	d->_cor = p;
	++d->_cor->_refCount;
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetString(VarInfo* d, const char* str)
{
	Var_SetStringA(d, str);
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetString(VarInfo* d, SUtf8One c)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_CHAR);
	d->_c = c;
}

NEOS_FORCEINLINE void INeoVMWorker::Var_SetStringA(VarInfo* d, const std::string& str)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = ((CNeoVMImpl*)_pVM)->StringAlloc(str);
	++d->_str->_refCount;
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetTable(VarInfo* d, MapInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_MAP);
	d->_tbl = p;
	++p->_refCount;
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetList(VarInfo* d, ListInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_LIST);
	d->_lst = p;
	++p->_refCount;
}
NEOS_FORCEINLINE void INeoVMWorker::Var_SetSet(VarInfo* d, SetInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_SET);
	d->_set = p;
	++p->_refCount;
}
inline void INeoVMWorker::Var_SetModule(VarInfo* d, INeoVMWorker* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_MODULE);
	d->_module = p;
	++((CNeoVMWorker*)p)->_refCount;
}
inline void INeoVMWorker::Var_SetAsync(VarInfo* d, AsyncInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_ASYNC);
	d->_async = p;
	++p->_refCount;
}

inline bool INeoVMWorker::GetArg_StlString(int idx, std::string& r)
{
	VarInfo* p = GetStackVar(idx);
	if (p == nullptr) return false;
	switch (p->GetType())
	{
	case VAR_CHAR:
		r = p->_c.c;
		return true;
	case VAR_STRING:
		r = p->_str->_str;
		return true;
	default:
		break;
	}
	return false;
}
inline bool INeoVMWorker::GetArg_Int(int idx, int& r)
{
	VarInfo* p = GetStackVar(idx);
	if (p == nullptr) return false;
	switch (p->GetType())
	{
	case VAR_INT:
		r = p->_int;
		return true;
	case VAR_FLOAT:
		r = (int)p->_float;
		return true;
	default:
		break;
	}
	return false;
}
inline bool INeoVMWorker::GetArg_Float(int idx, NS_FLOAT& r)
{
	VarInfo* p = GetStackVar(idx);
	if (p == nullptr) return false;
	switch (p->GetType())
	{
	case VAR_INT:
		r = (NS_FLOAT)p->_int;
		return true;
	case VAR_FLOAT:
		r = (NS_FLOAT)p->_float;
		return true;
	default:
		break;
	}
	return false;
}
inline bool INeoVMWorker::GetArg_Bool(int idx, bool& r)
{
	VarInfo* p = GetStackVar(idx);
	if (p == nullptr) return false;
	if (p->GetType() == VAR_BOOL)
	{
		r = p->_bl;
		return true;
	}
	return false;
}

