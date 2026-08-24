#pragma once

// INeoVMWorker 의 인라인 정의.
//
// 이 함수들은 VM 내부뿐 아니라 네이티브 라이브러리(NeoLib.cpp 등) 여러 TU 에서 호출된다.
// 따라서 정의가 호출하는 모든 TU 에 보여야 한다 — NeoVMWorker.inl 처럼 한 TU 에서만
// include 하면 inline 함수의 아웃오브라인 실체가 아무 데서도 생기지 않아 링크가 깨진다.
// (MSVC 는 LTCG 가 TU 경계를 넘어 찾아줘서 가려졌고, g++/ld 에서 undefined reference 로 드러났다)
// 그래서 이 파일은 NeoVMWorker.h 끝에서 include 한다.

// 대상이 이미 단독 소유(refCount<=1)인 같은 계열 벡터면 그 저장소를 재사용한다.
// `v = v + d` 처럼 결과를 자기 자신에 되쓰는 패턴에서 할당/해제를 없애기 위한 것이다.
//
// 벡터를 쓰는 모든 경로(생성 intrinsic, 성분 쓰기, 산술 결과)가 여기를 지나므로
// .cpp 의 아웃오브라인 정의로 두면 벡터 연산마다 호출이 하나 붙는다.
NEOS_FORCEINLINE VecInfo* INeoVMWorker::VecStoreFor(VarInfo* d, int count)
{
	if (d->IsVector() && d->_vec->_refCount <= 1)
	{
		d->SetVecType(count);    // 성분 수만 바뀌어도 저장소는 그대로 쓴다
		return d->_vec;
	}
	if (d->IsAllocType()) Var_Release(d);
	VecInfo* p = _pVM->VecAlloc();
	p->_refCount = 1;
	d->SetVecType(count);
	d->_vec = p;
	return p;
}

NEOS_FORCEINLINE bool INeoVMWorker::Var_ReleaseVecFast(VarInfo* d)
{
	VecInfo* vec = d->_vec;
	if (vec->_refCount <= 1)
		return false;

	--vec->_refCount;
	d->_vec = nullptr;
	d->ClearType();
	return true;
}

NEOS_FORCEINLINE bool INeoVMWorker::Var_ReleaseStringFast(VarInfo* d)
{
	StringInfo* str = d->_str;
	if (str->_refCount <= 1)
		return false;

	--str->_refCount;
	d->_str = nullptr;
	d->ClearType();
	return true;
}

NEOS_FORCEINLINE bool INeoVMWorker::Var_ReleaseListFast(VarInfo* d)
{
	ListInfo* list = d->_lst;
	// 컨테이너 자식이 있으면 감소 후 순환 후보 등록이 필요하다.
	if (list->_refCount <= 1 || list->_cycleState._mayContainContainerChild)
		return false;

	--list->_refCount;
	d->_lst = nullptr;
	d->ClearType();
	return true;
}

// 호출자가 alloc 여부로 이미 분기한 자리용. Move/MoveTake/MoveI/MoveF 는 IsAllocType 이
// 참인 가지에서만 해제하므로, Var_Release 로 들어가면 같은 비교를 한 번 더 한다.
NEOS_FORCEINLINE void INeoVMWorker::Var_ReleaseAlloc(VarInfo* d)
{
	// 게임 워크로드는 VAR_VEC 해제가 압도적이다. shared ref만 여기서 끝내고,
	// 마지막 참조 및 순환 후보는 기존 dispatcher가 모든 부수 처리를 맡는다.
	const VAR_TYPE type = d->GetType();
	if (type == VAR_VEC && Var_ReleaseVecFast(d))
		return;
	if (type == VAR_STRING && Var_ReleaseStringFast(d))
		return;
	if (type == VAR_LIST && Var_ReleaseListFast(d))
		return;

	_pVM->Var_ReleaseInternal(d);
}

NEOS_FORCEINLINE void INeoVMWorker::Var_Release(VarInfo* d)
{
	if (d->IsAllocType() == false)
	{
		d->ClearType();
		return;
	}
	Var_ReleaseAlloc(d);
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

NEOS_FORCEINLINE void INeoVMWorker::Var_SetStringA(VarInfo* d, const std::string& str)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = _pVM->StringAlloc(str);
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

