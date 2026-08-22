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
bool	INeoVM::RegisterTableCallBack(VarInfo* p, void* pUserData, Neo_NativeFunction func, Neo_NativeProperty property)
{
	// 일반 VAR_MAP을 변환하지 않는다. VAR_FP_NATIVE를 생성한 뒤에만 바인딩한다.
	if (p == nullptr || p->GetType() != VAR_FP_NATIVE || p->_fpNative == nullptr) return false;
	FunctionPropertyInfo* fp = p->_fpNative;

	fp->_pUserData = pUserData;
	CNeoVMWorker::neo_pushcclosureNative(&fp->_fun, func);
	CNeoVMWorker::neo_pushcclosureNative(&fp->_fun, property);

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
//	case VAR_INT: v1->_int = v2->_int; break;
//	case VAR_FLOAT: v1->_float = v2->_float; break;
//	case VAR_BOOL: v1->_bl = v2->_bl; break;
//	case VAR_NONE: break;
//	case VAR_FUN: v1->_fun_index = v2->_fun_index; break;
//	case VAR_FUN_NATIVE: v1->_funPtr = v2->_funPtr; break;
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
	// case 순서 = VAR_TYPE 열거 순서 (리프 STRING/VEC/FP_NATIVE 먼저, 그다음 컨테이너).
	// 이 함수는 인터프리터 핫패스에 인라인된다. 어긋나면 코드 생성이 나빠져 벤치 전체가
	// 눈에 띄게 느려진다(실측 +5%). 열거를 바꾸면 여기도 반드시 같이 바꿀 것.
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeString(d);
		d->_str = NULL;
		break;
	case VAR_VEC:
		if (--d->_vec->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeVec(d->_vec);
		d->_vec = NULL;
		break;
	case VAR_FP_NATIVE:
		if (--d->_fpNative->_refCount <= 0)
			((CNeoVMImpl*)this)->FreeFunctionProperty(d->_fpNative);
		d->_fpNative = NULL;
		break;
	case VAR_MAP:
		if (--d->_tbl->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (d->_tbl->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_tbl = NULL;
		break;
	case VAR_LIST:
		if (--d->_lst->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (d->_lst->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_lst = NULL;
		break;
	case VAR_SET:
		if (--d->_set->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (d->_set->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_set = NULL;
		break;
	case VAR_COROUTINE:
		if (--d->_cor->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (d->_cor->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_cor = NULL;
		break;
	case VAR_MODULE:
		if (--((CNeoVMWorker*)(d->_module))->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (((CNeoVMWorker*)(d->_module))->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_module = NULL;
		break;
	case VAR_ASYNC:
		if (--d->_async->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (d->_async->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_async = NULL;
		break;
	case VAR_CLOSURE:
		if (--d->_closure->_refCount <= 0)
			((CNeoVMImpl*)this)->QueueContainerForDestroy(*d);
		else if (d->_closure->_cycleState._mayContainContainerChild)
			((CNeoVMImpl*)this)->QueueContainerForCycleCheck(*d);
		d->_closure = NULL;
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
	++d._str->_refCount;
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

	return NULL;
}
const std::string* INeoVMWorker::PopStlString(VarInfo* V)
{
	if (V->GetType() == VAR_STRING)
		return &V->_str->_str;

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
	if (p == NULL) return false;
	if (p->GetType() == VAR_FLOAT) { value = p->_float; return true; }
	if (p->GetType() == VAR_INT) { value = (NS_FLOAT)p->_int; return true; }
	return false;
}

bool VarInfo::ListInsertFloat(int idx, NS_FLOAT value)
{
	if (_type != VAR_LIST) return false;
	return _lst->SetValue(idx, value);
}
bool VarInfo::ListFindFloat(int idx, NS_FLOAT& value)
{
	if (_type != VAR_LIST) return false;
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

// ---- 벡터 값타입 Get/Set ----
// Get: 벡터 값타입(VAR_VEC*) 전용. 리스트 폴백 없음 — 벡터 인자는 값타입이어야 한다.
// 성공/실패 모두 요청한 성분까지만 쓴다 — 남는 레인은 호출자 몫이다.
bool VarInfo::GetVec2(float out[2])
{
	if (IsVector() && VectorComponentCount() >= 2) { out[0] = _vec->v[0]; out[1] = _vec->v[1]; return true; }
	out[0] = out[1] = 0.0f;
	return false;
}
bool VarInfo::GetVec3(float out[3])
{
	if (IsVector() && VectorComponentCount() >= 3) { out[0] = _vec->v[0]; out[1] = _vec->v[1]; out[2] = _vec->v[2]; return true; }
	out[0] = out[1] = out[2] = 0.0f;
	return false;
}
bool VarInfo::GetVec4(float out[4])
{
	if (IsVector() && VectorComponentCount() >= 4) { out[0] = _vec->v[0]; out[1] = _vec->v[1]; out[2] = _vec->v[2]; out[3] = _vec->v[3]; return true; }
	out[0] = out[1] = out[2] = out[3] = 0.0f;
	return false;
}
bool VarInfo::GetQuat(float out[4])
{
	// wxyz 순서 (엔진 컨벤션). 저장이 v[0]=w,[1]=x,[2]=y,[3]=z 라 GetVec4 와 복사 순서는 같다.
	if (IsVector() && VectorComponentCount() >= 4) { out[0] = _vec->v[0]; out[1] = _vec->v[1]; out[2] = _vec->v[2]; out[3] = _vec->v[3]; return true; }
	out[0] = out[1] = out[2] = out[3] = 0.0f;
	return false;
}
// Set 은 VarInfo 멤버에서 없앴다 — 벡터가 alloc 타입이 되면서 VM(풀)이 있어야 저장소를
// 만들 수 있기 때문이다. 대신 INeoVMWorker::Var_SetVec2/3/4/Quat 를 쓴다.

};
