#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <atomic>
#include "NeoVMInternal.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"
#include "UTFString.h"

#ifdef _WIN32
#include "HttpRequest.h"
#pragma comment(lib, "ws2_32.lib")
#endif

namespace NeoScript
{

CNeoVM* 	NeoVMSystem::CreateVM()
{
	return new CNeoVM();
}
void		NeoVMSystem::ReleaseVM(CNeoVM* pVM)
{
	delete pVM;
}

FunctionPtrNative NeoVMSystem::RegisterNative(Neo_NativeFunction func)
{
	FunctionPtrNative fun;
	CNeoVMWorker::neo_pushcclosureNative(&fun, func);
	return fun;
}
bool	NeoVMSystem::RegisterTableCallBack(VarInfo* p, void* pUserData, Neo_NativeFunction func, Neo_NativeProperty property)
{
	// 일반 VAR_MAP을 변환하지 않는다. VAR_FP_NATIVE를 생성한 뒤에만 바인딩한다.
	if (p == nullptr || p->GetType() != VAR_FP_NATIVE || p->_fpNative == nullptr) return false;
	FunctionPropertyInfo* fp = p->_fpNative;

	fp->_pUserData = pUserData;
	CNeoVMWorker::neo_pushcclosureNative(&fp->_fun, func);
	CNeoVMWorker::neo_pushcclosureNative(&fp->_fun, property);

	return true;
}

//void NeoVMSystem::Var_AddRef(VarInfo* d)
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
//void NeoVMSystem::Move_DestNoRelease(VarInfo* v1, VarInfo* v2)
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
void CNeoVM::Var_ReleaseInternal(VarInfo* d)
{
	// case 순서 = VAR_TYPE 열거 순서 (리프 STRING/VEC/FP_NATIVE 먼저, 그다음 컨테이너).
	// 이 함수는 인터프리터 핫패스에 인라인된다. 어긋나면 코드 생성이 나빠져 벤치 전체가
	// 눈에 띄게 느려진다(실측 +5%). 열거를 바꾸면 여기도 반드시 같이 바꿀 것.
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			FreeString(d);
		d->_str = NULL;
		break;
	case VAR_VEC:
		if (--d->_vec->_refCount <= 0)
			FreeVec(d->_vec);
		d->_vec = NULL;
		break;
	case VAR_FP_NATIVE:
		if (--d->_fpNative->_refCount <= 0)
			FreeFunctionProperty(d->_fpNative);
		d->_fpNative = NULL;
		break;
	case VAR_MAP:
		if (--d->_tbl->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (d->_tbl->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
		d->_tbl = NULL;
		break;
	case VAR_LIST:
		if (--d->_lst->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (d->_lst->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
		d->_lst = NULL;
		break;
	case VAR_SET:
		if (--d->_set->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (d->_set->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
		d->_set = NULL;
		break;
	case VAR_COROUTINE:
		if (--d->_cor->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (d->_cor->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
		d->_cor = NULL;
		break;
	case VAR_MODULE:
		if (--((CNeoVMWorker*)(d->_module))->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (((CNeoVMWorker*)(d->_module))->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
		d->_module = NULL;
		break;
	case VAR_ASYNC:
		if (--d->_async->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (d->_async->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
		d->_async = NULL;
		break;
	case VAR_CLOSURE:
		if (--d->_closure->_refCount <= 0)
			QueueContainerForDestroy(*d);
		else if (d->_closure->_cycleState._mayContainContainerChild)
			QueueContainerForCycleCheck(*d);
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
	d._str = _pVM->StringAlloc(s);
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
		d._funPtr = _pVM->FunctionPtrAlloc(&v._fun);
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


static std::atomic<int> g_iNeoVMAllocStrings{ 0 };
static std::atomic<int> g_iNeoVMAllocMaps{ 0 };
static std::atomic<int> g_iNeoVMAllocLists{ 0 };
static std::atomic<int> g_iNeoVMAllocSets{ 0 };
static std::atomic<int> g_iNeoVMAllocCoroutines{ 0 };
static std::atomic<int> g_iNeoVMAllocModules{ 0 };
static std::atomic<int> g_iNeoVMAllocAsyncs{ 0 };
static std::atomic<int> g_iNeoVMAllocClosures{ 0 };
static std::atomic<int> g_iNeoVMAllocVectors{ 0 };
static std::atomic<long long> g_iNeoVMPoolBytes{ 0 };       // 모든 VM 의 오브젝트 풀 합계
static std::atomic<long long> g_iNeoVMStringIdleBytes{ 0 };  // 유휴 문자열 노드가 붙든 문자 버퍼
static std::atomic<long long> g_iNeoVMExecPoolBytes{ 0 };   // 스레드별 실행 컨텍스트 풀 합계

static void PublishNeoVMAllocStatValue(std::atomic<int>& target, int& published, int current)
{
	int delta = current - published;
	if (delta != 0)
	{
		target.fetch_add(delta, std::memory_order_relaxed);
		published = current;
	}
}
static void PublishNeoVMAllocStatValue(std::atomic<long long>& target, long long& published, long long current)
{
	long long delta = current - published;
	if (delta != 0)
	{
		target.fetch_add(delta, std::memory_order_relaxed);
		published = current;
	}
}

// 실행 컨텍스트 풀(스레드별, 엔진 소유)이 자기 확보량 변화를 전역에 반영할 때 부른다.
void NeoExecPool_PublishBytes(long long delta)
{
	if (delta != 0)
		g_iNeoVMExecPoolBytes.fetch_add(delta, std::memory_order_relaxed);
}

void GetNeoVMAllocStats(SNeoVMAllocStats& outStats)
{
	outStats.strings = g_iNeoVMAllocStrings.load(std::memory_order_relaxed);
	outStats.maps = g_iNeoVMAllocMaps.load(std::memory_order_relaxed);
	outStats.lists = g_iNeoVMAllocLists.load(std::memory_order_relaxed);
	outStats.sets = g_iNeoVMAllocSets.load(std::memory_order_relaxed);
	outStats.coroutines = g_iNeoVMAllocCoroutines.load(std::memory_order_relaxed);
	outStats.modules = g_iNeoVMAllocModules.load(std::memory_order_relaxed);
	outStats.asyncs = g_iNeoVMAllocAsyncs.load(std::memory_order_relaxed);
	outStats.closures = g_iNeoVMAllocClosures.load(std::memory_order_relaxed);
	outStats.vectors = g_iNeoVMAllocVectors.load(std::memory_order_relaxed);
	outStats.poolBytes = g_iNeoVMPoolBytes.load(std::memory_order_relaxed)
	                   + g_iNeoVMExecPoolBytes.load(std::memory_order_relaxed);
	outStats.stringIdleBytes = g_iNeoVMStringIdleBytes.load(std::memory_order_relaxed);
}

bool GetNeoVMAllocStats(CNeoVM* pVM, SNeoVMAllocStats& outStats)
{
	if (pVM == nullptr)
		return false;

	pVM->GetAllocStats(outStats);
	return true;
}

// 빈 페이지 회수.
//  force=false : 보유 시간이 지난 페이지를 "한 번에 m_iTrimPagesPerCall 장까지"만
//                돌려준다. 호스트가 필요할 때 조금씩 반납하는 용도다.
//  force=true  : 보유 시간도 예산도 무시하고 지금 비어 있는 페이지를 전부 회수한다.
//                맵 전환처럼 "지금 확실히 정리" 가 필요한 시점용.
//
// 호출 빈도는 호스트의 메모리 반환 정책이다. 풀은 빈 페이지를 FIFO로 들고 있어
// (1) 만료 판정은 head 하나를 보는 O(1), (2) 해제는 페이지 예산만큼이므로
// 호출 한 번의 비용에는 상한이 있다.
//
// [보유 시계 기준점]
// "페이지가 실제로 빈 순간" 이 아니라 "Collect 가 빈 걸 처음 본 순간" 부터 잰다.
// Confer 에서 시계를 읽지 않으려고 기록을 여기까지 미룬다. 호출이 늦을수록
// 실제 유지 시간은 더 길어지는 방향이라 안전하다.
long long CNeoVM::CollectEmptyPages(bool force)
{
	// 순환 참조 회수는 VM 안전 지점에서 자동으로 수행한다.
	// 이 함수는 pool 페이지 반환만 담당한다.
	//
	// 페이지 반환 비용은 "장수 예산"으로 호출당 상한이 있다.
	// force=false는 보유 시간이 지난 페이지만 예산만큼 반환한다. force=true는
	// 보유 시간과 예산을 무시하는 명시적 메모리 압박/씬 전환 경로다.
	//
	// 빈 페이지가 하나도 없으면 시계도 읽지 않고 나간다(풀 포인터 비교뿐).
	if (force == false && AnyEmptyPages() == false)
		return 0;

	const NeoPoolClock::time_point now = NeoPoolClock::now();
	const int hold = force ? 0 : m_iEmptyPageHoldMs;   // force 는 보유 시간도 무시한다
	int budget = (force || m_iTrimPagesPerCall <= 0) ? INT_MAX : m_iTrimPagesPerCall;

	long long freed = 0;
	for (int i = 0; i < kTrimPoolCount && budget > 0; ++i)
		freed += (long long)CollectPoolAt((m_iTrimPoolCursor + i) % kTrimPoolCount, now, hold, budget);

	// 다음 호출은 다음 풀부터 — 앞쪽 풀이 예산을 다 먹어 뒤쪽이 굶는 것을 막는다.
	if (force == false)
		m_iTrimPoolCursor = (m_iTrimPoolCursor + 1) % kTrimPoolCount;

	if (force)
	{
		// 강제 정리는 맵 전환처럼 드물게 부르는 명시적 시점이다. 여기서만 유휴 문자열
		// 버퍼를 다시 재서 전역 통계를 맞춰 둔다 — 매 프레임 경로에서는 절대 하지 않는다.
		m_sAllocStats.stringIdleBytes = StringIdleBytes();
	}
	if (freed != 0 || force)
		PublishAllocStats();
	return freed;
}

// 예산을 나눠 주고 라운드로빈 하려면 풀을 인덱스로 다뤄야 한다. 타입이 전부 달라서
// (템플릿 인자가 다르다) 공통 기반 클래스 대신 switch 로 편다 — vtable 이 안 생긴다.
size_t CNeoVM::CollectPoolAt(int idx, NeoPoolClock::time_point now, int holdMs, int& pageBudget)
{
	switch (idx)
	{
	case 0:  return m_sPool_TableData.Collect(now, holdMs, pageBudget);
	case 1:  return m_sPool_TableInfo.Collect(now, holdMs, pageBudget);
	case 2:  return m_sPool_FunctionProperty.Collect(now, holdMs, pageBudget);
	case 3:  return m_sPool_SetData.Collect(now, holdMs, pageBudget);
	case 4:  return m_sPool_SetInfo.Collect(now, holdMs, pageBudget);
	case 5:  return m_sPool_ListInfo.Collect(now, holdMs, pageBudget);
	case 6:  return m_sPool_Vec.Collect(now, holdMs, pageBudget);
	case 7:  return m_sPool_Async.Collect(now, holdMs, pageBudget);
	case 8:  return m_sPool_String.Collect(now, holdMs, pageBudget);
	default: return m_sPool_Closure.Collect(now, holdMs, pageBudget);
	}
}

long long CNeoVM::PoolBytes() const
{
	return (long long)m_sPool_TableData.ReservedBytes()
	     + (long long)m_sPool_TableInfo.ReservedBytes()
	     + (long long)m_sPool_FunctionProperty.ReservedBytes()
	     + (long long)m_sPool_SetData.ReservedBytes()
	     + (long long)m_sPool_SetInfo.ReservedBytes()
	     + (long long)m_sPool_ListInfo.ReservedBytes()
	     + (long long)m_sPool_Vec.ReservedBytes()
	     + (long long)m_sPool_Async.ReservedBytes()
	     + (long long)m_sPool_String.ReservedBytes()
	     + (long long)m_sPool_Closure.ReservedBytes();
	// _pExecPool 은 스레드별 공유(엔진 소유)라 여기서 세면 VM 수만큼 중복된다 → 자기가 직접 publish.
}

// 반납된(놀고 있는) 문자열 노드가 아직 붙들고 있는 문자 버퍼의 합계.
// 풀 페이지(PoolBytes)에는 안 잡히는 값이라 따로 센다. free 리스트를 훑으므로
// 매 프레임이 아니라 통계 조회 시점에만 부른다.
long long CNeoVM::StringIdleBytes()
{
	long long total = 0;
	m_sPool_String.ForEachFree([&total](StringInfo& s)
	{
		const size_t capa = s._str.capacity();
		if (capa > 15)   // SSO 범위는 힙을 안 쓴다
			total += (long long)capa;
	});
	return total;
}

// 조회 시점에만 유휴 문자열 버퍼를 다시 재고 전역에 반영한다.
void CNeoVM::GetAllocStats(SNeoVMAllocStats& outStats)
{
	m_sAllocStats.stringIdleBytes = StringIdleBytes();
	PublishNeoVMAllocStatValue(g_iNeoVMStringIdleBytes,
		m_sPublishedAllocStats.stringIdleBytes, m_sAllocStats.stringIdleBytes);
	outStats = m_sAllocStats;
	outStats.poolBytes = PoolBytes();
}

// 주의: 여기서 stringIdleBytes 를 다시 재지 않는다.
// StringIdleBytes() 는 문자열 free 리스트 전수 순회라 O(유휴 노드) 인데, 이 함수는
// 실행이 끝날 때마다 그리고 증분 회수가 도는 동안에는 매 프레임 불린다.
// 그러면 "한 호출의 비용에 상한을 둔다" 는 증분 회수의 전제가 깨진다.
// 갱신은 GetAllocStats(조회) 에서만 하고, 여기서는 마지막 값을 그대로 다시 publish 한다
// (델타 0 이라 전역은 변하지 않는다).
void CNeoVM::PublishAllocStats()
{
	m_sAllocStats.poolBytes = PoolBytes();
	PublishNeoVMAllocStatValue(g_iNeoVMPoolBytes, m_sPublishedAllocStats.poolBytes, m_sAllocStats.poolBytes);
	PublishNeoVMAllocStatValue(g_iNeoVMStringIdleBytes, m_sPublishedAllocStats.stringIdleBytes, m_sAllocStats.stringIdleBytes);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocStrings, m_sPublishedAllocStats.strings, m_sAllocStats.strings);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocMaps, m_sPublishedAllocStats.maps, m_sAllocStats.maps);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocLists, m_sPublishedAllocStats.lists, m_sAllocStats.lists);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocSets, m_sPublishedAllocStats.sets, m_sAllocStats.sets);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocCoroutines, m_sPublishedAllocStats.coroutines, m_sAllocStats.coroutines);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocModules, m_sPublishedAllocStats.modules, m_sAllocStats.modules);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocAsyncs, m_sPublishedAllocStats.asyncs, m_sAllocStats.asyncs);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocClosures, m_sPublishedAllocStats.closures, m_sAllocStats.closures);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocVectors, m_sPublishedAllocStats.vectors, m_sAllocStats.vectors);
}

NeoExecContextPool* NeoExecContextPool_Create(int varStackSize)
{
	return new NeoExecContextPool(varStackSize);
}
void NeoExecContextPool_Destroy(NeoExecContextPool* pool)
{
	delete pool;
}

// 살아있는 객체 추적용 intrusive 이중연결 리스트 헬퍼 (List/Map/Set 공용).
// _liveNext/_livePrev 필드를 가진 타입이면 동작한다.
template<typename T>
static NEOS_FORCEINLINE void LiveList_Insert(T*& head, T* p)
{
	p->_livePrev = nullptr;
	p->_liveNext = head;
	if (head) head->_livePrev = p;
	head = p;
}
template<typename T>
static NEOS_FORCEINLINE void LiveList_Remove(T*& head, T* p)
{
	if (p->_livePrev) p->_livePrev->_liveNext = p->_liveNext;
	else             head = p->_liveNext;
	if (p->_liveNext) p->_liveNext->_livePrev = p->_livePrev;
}

// 순환 후보는 객체 자신이 FIFO 노드다. _cyclePrev == owner 는 큐에 없다는 표식이다.
template<typename T>
static NEOS_FORCEINLINE bool CycleList_Append(T*& head, T*& tail, T* p)
{
	CycleState<T>& state = p->_cycleState;
	if (state.IsQueued(p))
		return false;

	state._cyclePrev = tail;
	state._cycleNext = nullptr;
	if (tail) tail->_cycleState._cycleNext = p;
	else      head = p;
	tail = p;
	return true;
}
template<typename T>
static NEOS_FORCEINLINE bool CycleList_Remove(T*& head, T*& tail, T* p)
{
	CycleState<T>& state = p->_cycleState;
	if (state.IsQueued(p) == false)
		return false;

	T* prev = state._cyclePrev;
	T* next = state._cycleNext;
	if (prev) prev->_cycleState._cycleNext = next;
	else      head = next;
	if (next) next->_cycleState._cyclePrev = prev;
	else      tail = prev;
	state._cycleNext = nullptr;
	state._cyclePrev = p;
	return true;
}
template<typename T>
static NEOS_FORCEINLINE T* CycleList_PopFront(T*& head, T*& tail)
{
	T* p = head;
	if (p)
		CycleList_Remove(head, tail, p);
	return p;
}

void CNeoVM::Var_SetString(VarInfo *d, const char* str)
{
	std::string s(str);
	Var_SetStringA(d, s);
}
void CNeoVM::Var_SetStringA(VarInfo *d, const std::string& str)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = StringAlloc(str);
	++d->_str->_refCount;
}
void CNeoVM::Var_SetTable(VarInfo *d, MapInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_MAP);
	d->_tbl = p;
	++d->_tbl->_refCount;
}


CNeoVMWorker* CNeoVM::WorkerAlloc(int iStackSize)
{
	while (true)
	{
		if (++_dwLastIDVMWorker == 0)
			_dwLastIDVMWorker = 1;

		if (_sVMWorkers.end() == _sVMWorkers.find(_dwLastIDVMWorker))
		{
			break;
		}
	}

	CNeoVMWorker* p = new CNeoVMWorker(this, _dwLastIDVMWorker, iStackSize);
	p->_refCount = 0;
	p->_cycleState.Init(p);
	p->_cycleState._mayContainContainerChild = true;
	++m_sAllocStats.modules;

	_sVMWorkers[_dwLastIDVMWorker] = p;
	return p;
}
void CNeoVM::FreeWorker(CNeoVMWorker *d)
{
	if (d == nullptr || d->_cycleState._destroying)
		return;
	auto it = _sVMWorkers.find(d->GetWorkerID());
	if (it == _sVMWorkers.end())
		return;

	d->_cycleState._destroying = true;
	CancelCycleCandidate(VAR_MODULE, d);
	_sVMWorkers.erase(it);
	--m_sAllocStats.modules;
	delete d;
}
CNeoVMWorker* CNeoVM::FindWorker(int iModule)
{
	auto it = _sVMWorkers.find(iModule);
	if (it == _sVMWorkers.end())
		return NULL;

	return (*it).second;
}

CoroutineInfo* CNeoVM::CoroutineAlloc()
{
	// 코루틴 컨텍스트도 default 실행 컨텍스트와 동일한 공유 풀에서 대여한다.
	CoroutineInfo* p = _pExecPool->Acquire();
	p->_function.ClearType();
	p->_activeClosure = nullptr;
	++m_sAllocStats.coroutines;
	return p;
}
void CNeoVM::FreeCoroutine(VarInfo *d)
{
	CoroutineInfo* pCI = d->_cor;
	CancelCycleCandidate(VAR_COROUTINE, pCI);
	if (pCI->_cycleState._destroying)
		return;
	pCI->_cycleState._destroying = true;
	--m_sAllocStats.coroutines;
	// 공유 풀로 반납하기 전에 컨텍스트의 스택 참조를 정리한다.
	// 완료(DeadCoroutine)를 거친 코루틴은 _iSP_Vars_Max2 가 0 이라 무해하지만,
	// yield 된 채 버려진 코루틴은 live ref 가 남아있어 정리하지 않으면 다른 VM 재사용 시 손상된다.
	int n = pCI->_info._iSP_Vars_Max2;
	std::vector<VarInfo>& s = pCI->m_sVarStack;
	if (n > (int)s.size()) n = (int)s.size();
	for (int i = 0; i < n; i++)
		Var_Release(&s[i]);
	// coroutine이 일반 함수 안에서 yield하면 부모 closure의 실행 참조는 여기로
	// 옮겨진다. 풀 반납 전 반드시 풀어야 다음 Acquire의 clear()가 소유권을 잃지 않는다.
	for (int i = 0; i < (int)pCI->m_sClosureCallStack.size(); ++i)
	{
		SClosureCallState& state = pCI->m_sClosureCallStack[i];
		if (state._closure == nullptr)
			continue;
		VarInfo active(VAR_CLOSURE);
		active._closure = state._closure;
		state._closure = nullptr;
		Var_Release(&active);
	}
	pCI->m_sClosureCallStack.clear();
	Var_Release(&pCI->_function);
	if (pCI->_activeClosure)
	{
		VarInfo active(VAR_CLOSURE);
		active._closure = pCI->_activeClosure;
		pCI->_activeClosure = nullptr;
		Var_Release(&active);
	}
	pCI->_info.ClearSP();
	pCI->m_sAsyncResumeCodePtrs.clear();
	pCI->m_sAsyncWaitReturnStack.clear();
	_pExecPool->Release(pCI);
}

StringInfo* CNeoVM::StringAlloc(const std::string& str)
{
	// 일반 문자열은 임시 문자열 루프를 위해 인터너와 해시 계산을 건너뛴다.
	// 해시가 필요해지면 VMString::GetHash()가 그때 한 번 채운다.
	StringInfo* p = m_sPool_String.Receive();// new StringInfo();
	p->_hash = 0;
	p->_interned = false;
	p->_refCount = 0;

	p->_str = str;
	p->_StringLen = utf_string::UTF8_LENGTH(str);

	++m_sAllocStats.strings;
	return p;
}

StringInfo* CNeoVM::StringIntern(const std::string& str)
{
	const u32 hash = GetHashCode(str);
	if (StringInfo* existing = FindInternedString(str, hash))
		return existing;

	StringInfo* p = StringAlloc(str);
	p->_hash = hash;
	p->_interned = true;
	InsertInternedString(p);
	return p;
}

StringInfo* CNeoVM::FindInternedString(const std::string& str, u32 hash) const
{
	if (m_sStringIntern.empty())
		return nullptr;

	const int mask = (int)m_sStringIntern.size() - 1;
	for (int index = (int)(hash & mask); ; index = (index + 1) & mask)
	{
		StringInfo* candidate = m_sStringIntern[index];
		if (candidate == nullptr)
			return nullptr;
		if (candidate->_hash == hash && candidate->_str == str)
			return candidate;
	}
}

StringInfo* CNeoVM::StringFind(const std::string& str) const
{
	return FindInternedString(str, GetHashCode(str));
}

StringInfo* CNeoVM::StringFind(StringInfo* pString) const
{
	return pString ? FindInternedString(pString->_str, pString->GetHash()) : nullptr;
}

StringInfo* FindCanonicalString(CNeoVM* pVM, StringInfo* pString)
{
	return pVM ? pVM->StringFind(pString) : nullptr;
}

void CNeoVM::RehashStringIntern(int capacity)
{
	if (capacity < 64)
		capacity = 64;

	int powerOfTwo = 1;
	while (powerOfTwo < capacity)
		powerOfTwo <<= 1;

	std::vector<StringInfo*> oldSlots;
	oldSlots.swap(m_sStringIntern);
	m_sStringIntern.assign(powerOfTwo, nullptr);
	m_sStringInternCount = 0;

	const int mask = powerOfTwo - 1;
	for (StringInfo* p : oldSlots)
	{
		if (p == nullptr)
			continue;

		int index = (int)(p->_hash & mask);
		while (m_sStringIntern[index] != nullptr)
			index = (index + 1) & mask;
		m_sStringIntern[index] = p;
		++m_sStringInternCount;
	}
}

void CNeoVM::InsertInternedString(StringInfo* p)
{
	if (m_sStringIntern.empty()
		|| (m_sStringInternCount + 1) * 4 > (int)m_sStringIntern.size() * 3)
	{
		RehashStringIntern(m_sStringIntern.empty() ? 64 : (int)m_sStringIntern.size() << 1);
	}

	const int mask = (int)m_sStringIntern.size() - 1;
	int index = (int)(p->_hash & mask);
	while (m_sStringIntern[index] != nullptr)
		index = (index + 1) & mask;
	m_sStringIntern[index] = p;
	++m_sStringInternCount;
}

void CNeoVM::RemoveInternedString(StringInfo* p)
{
	if (m_sStringIntern.empty())
		return;

	const int mask = (int)m_sStringIntern.size() - 1;
	int index = (int)(p->_hash & mask);
	while (m_sStringIntern[index] != p)
	{
		if (m_sStringIntern[index] == nullptr)
			return;
		index = (index + 1) & mask;
	}

	m_sStringIntern[index] = nullptr;
	--m_sStringInternCount;

	// Fill the hole by reinserting the rest of this probe cluster. This avoids
	// tombstones, so temporary strings leave no progressively longer probes.
	for (int cursor = (index + 1) & mask; m_sStringIntern[cursor] != nullptr; cursor = (cursor + 1) & mask)
	{
		StringInfo* displaced = m_sStringIntern[cursor];
		m_sStringIntern[cursor] = nullptr;
		--m_sStringInternCount;

		int target = (int)(displaced->_hash & mask);
		while (m_sStringIntern[target] != nullptr)
			target = (target + 1) & mask;
		m_sStringIntern[target] = displaced;
		++m_sStringInternCount;
	}

	int desiredCapacity = 64;
	while (m_sStringInternCount * 4 > desiredCapacity * 3)
		desiredCapacity <<= 1;
	if (desiredCapacity < (int)m_sStringIntern.size() / 2)
		RehashStringIntern(desiredCapacity);
}
// 이 용량 이상이면 반납 시 문자 버퍼를 즉시 놓아준다(미만이면 재사용을 위해 유지).
// 풀은 노드를 재사용하므로 버퍼를 남겨두면 재할당을 아낀다. 다만 상한이 없으면
// 한 번 큰 문자열을 담았던 노드가 그 용량을 영원히 붙든다 — 맵 로딩처럼 대량 생성 후
// 전량 소멸하는 구간에서 이게 그대로 남는다. 상한을 두면 총 유지량이
// (유휴 노드 수 x 이 값) 미만으로 묶인다.
// MSVC x64 는 15자까지 SSO(힙 미사용)라, 이 값은 그보다 충분히 커야 의미가 있다.
static const size_t kStringReleaseCapacity = 32;

void CNeoVM::FreeString(VarInfo *d)
{
	StringInfo* p = d->_str;
	if (p->_interned)
		RemoveInternedString(p);

	--m_sAllocStats.strings;
	std::string& str = p->_str;
	const size_t capa = str.capacity();
	if (capa >= kStringReleaseCapacity)
	{
		// shrink_to_fit 은 표준상 비구속 요청이라 확정적으로 놓아주려면 swap 을 쓴다.
		std::string().swap(str);
	}
	m_sPool_String.Confer(p);
}
VecInfo* CNeoVM::VecAlloc()
{
	// MapNode 와 같은 CNVMAllocPool — 생성자를 부르지 않는 raw 블록이라 성분은 호출측이 채운다.
	VecInfo* p = m_sPool_Vec.Receive();
	p->_refCount = 0;
	++m_sAllocStats.vectors;
	return p;
}
void CNeoVM::FreeVec(VecInfo* p)
{
	--m_sAllocStats.vectors;
	m_sPool_Vec.Confer(p);
}
VecInfo* CNeoVM::VecCopyOnWrite(VarInfo* d)
{
	VecInfo* src = d->_vec;
	if (src->_refCount <= 1)
		return src;              // 단독 소유 → 그대로 쓴다

	VecInfo* dst = VecAlloc();   // 공유 중 → 복제해서 이 VarInfo 만 새 것을 갖는다
	dst->v[0] = src->v[0]; dst->v[1] = src->v[1];
	dst->v[2] = src->v[2]; dst->v[3] = src->v[3];
	--src->_refCount;
	dst->_refCount = 1;
	d->_vec = dst;
	return dst;
}
MapInfo* CNeoVM::TableAlloc(int cnt)
{
	MapInfo* pTable = m_sPool_TableInfo.Receive();
	pTable->_pVM = this;
	pTable->_refCount = 0;
	pTable->_cycleState.Init(pTable);
	pTable->_itemCount = 0;
	pTable->_mutationVersion = 0;
	pTable->_HashBase = 0;
	pTable->_BucketCapa = 0;
	pTable->_lastFree = -1;
	pTable->_Bucket = nullptr;

	LiveList_Insert(_sTableHead, pTable);
	if (cnt > 0) pTable->Reserve(cnt);
	++m_sAllocStats.maps;
	return pTable;
}
void CNeoVM::FreeTable(MapInfo* tbl)
{
	CancelCycleCandidate(VAR_MAP, tbl);
	if (tbl->_cycleState._destroying)
		return;
	tbl->_cycleState._destroying = true;
	LiveList_Remove(_sTableHead, tbl);
	tbl->Free();

	//delete tbl;
	m_sPool_TableInfo.Confer(tbl);
	--m_sAllocStats.maps;
}
FunctionPropertyInfo* CNeoVM::FunctionPropertyAlloc()
{
	FunctionPropertyInfo* fp = m_sPool_FunctionProperty.Receive();
	fp->_refCount = 0;
	fp->_fun._func = NULL;
	fp->_fun._property = NULL;
	fp->_pUserData = NULL;
	return fp;
}
void CNeoVM::FreeFunctionProperty(FunctionPropertyInfo* fp)
{
	fp->_fun._func = NULL;
	fp->_fun._property = NULL;
	fp->_pUserData = NULL;
	m_sPool_FunctionProperty.Confer(fp);
}
ListInfo* CNeoVM::ListAlloc(int cnt)
{
	ListInfo* pList = m_sPool_ListInfo.Receive();
	pList->_pVM = this;
	pList->_refCount = 0;
	pList->_cycleState.Init(pList);
	pList->_mutationVersion = 0;
	pList->_pUserData = NULL;
	pList->_pIndexer = nullptr;
	pList->InitInlineBucket();   // _Bucket=인라인, capa=4, itemCount=0 (작은 리스트는 힙 할당 없음)

	LiveList_Insert(_sListHead, pList);
	if (cnt > 0) pList->Resize(cnt);
	++m_sAllocStats.lists;
	return pList;
}
void CNeoVM::FreeList(ListInfo* lst)
{
	CancelCycleCandidate(VAR_LIST, lst);
	if (lst->_cycleState._destroying)
		return;
	lst->_cycleState._destroying = true;
	LiveList_Remove(_sListHead, lst);
	lst->Free();

	//delete tbl;
	m_sPool_ListInfo.Confer(lst);
	--m_sAllocStats.lists;
}
SetInfo* CNeoVM::SetAlloc()
{
	SetInfo* pSet = m_sPool_SetInfo.Receive();
	pSet->_pVM = this;
	pSet->_refCount = 0;
	pSet->_cycleState.Init(pSet);
	pSet->_itemCount = 0;
	pSet->_mutationVersion = 0;
	pSet->_HashBase = 0;
	pSet->_BucketCapa = 0;
	pSet->_lastFree = -1;
	pSet->_Bucket = nullptr;

	LiveList_Insert(_sSetHead, pSet);
	++m_sAllocStats.sets;
	return pSet;
}
void CNeoVM::FreeSet(SetInfo* set)
{
	CancelCycleCandidate(VAR_SET, set);
	if (set->_cycleState._destroying)
		return;
	set->_cycleState._destroying = true;
	LiveList_Remove(_sSetHead, set);
	set->Free();

	//delete tbl;
	m_sPool_SetInfo.Confer(set);
	--m_sAllocStats.sets;
}
// 용량이 임계값 이상이면 버퍼를 확정 해제한다(문자열 풀과 같은 규칙).
static NEOS_FORCEINLINE void ReleaseIfLarge(std::string& s)
{
	if (s.capacity() >= kStringReleaseCapacity)
		std::string().swap(s);
	else
		s.clear();
}

AsyncInfo* CNeoVM::AsyncAlloc()
{
	AsyncInfo* p = m_sPool_Async.Receive();
	p->_refCount = 0;
	p->_cycleState.Init(p);
	p->_ownerWorkerId = 0;
	p->_state = ASYNC_READY;
	// 풀에서 재사용된 노드는 이전 요청의 값을 그대로 들고 있다. 특히 _headers 는
	// async_add_header 가 push_back 만 하므로, 안 비우면 이전 요청 헤더가 그대로 따라간다.
	// (요청마다 반드시 새로 채워지는 _type/_timeout/_fun_index 는 호출부가 덮어쓴다)
	p->_headers.clear();
	p->_request.clear();
	p->_body.clear();
	p->_resultValue.clear();
	p->_success = false;
	p->_callback.ClearType();
	++m_sAllocStats.asyncs;
	return p;
}
void CNeoVM::FreeAsync(VarInfo* d)
{
	AsyncInfo* p = d->_async;
	CancelCycleCandidate(VAR_ASYNC, p);
	if (p->_cycleState._destroying)
		return;
	p->_cycleState._destroying = true;
	--m_sAllocStats.asyncs;
	// AsyncInfo 는 노드가 372B 로 가장 크고, 문자열 멤버 셋에 HTTP 본문까지 담긴다
	// (_resultValue 는 응답 전문이라 MB 단위도 가능). 문자열 풀과 같은 규칙으로 놓아준다.
	ReleaseIfLarge(p->_request);
	ReleaseIfLarge(p->_body);
	ReleaseIfLarge(p->_resultValue);
	// 헤더는 요청마다 새로 쌓이고 재사용 이득이 없다 — 배열째 돌려준다.
	std::vector< std::pair<std::string, std::string> >().swap(p->_headers);
	Var_Release(&p->_callback);
	m_sPool_Async.Confer(p);
}

ClosureInfo* CNeoVM::ClosureAlloc(int functionIndex, int captureCount)
{
	ClosureInfo* closure = m_sPool_Closure.Receive();
	closure->_pVM = this;
	closure->_funIndex = functionIndex;
	closure->_refCount = 0;
	closure->_cycleState.Init(closure);
	closure->_captures.clear();
	closure->_captures.resize(captureCount);
	LiveList_Insert(_sClosureHead, closure);
	++m_sAllocStats.closures;
	return closure;
}

void CNeoVM::FreeClosure(ClosureInfo* closure)
{
	if (closure == nullptr)
		return;
	CancelCycleCandidate(VAR_CLOSURE, closure);
	if (closure->_cycleState._destroying)
		return;
	closure->_cycleState._destroying = true;
	LiveList_Remove(_sClosureHead, closure);
	for (VarInfo& value : closure->_captures)
		Var_Release(&value);
	closure->_captures.clear();
	closure->_pVM = nullptr;
	m_sPool_Closure.Confer(closure);
	--m_sAllocStats.closures;
}

void CNeoVM::QueueContainerForDestroy(const VarInfo& value)
{
	// value 는 참조를 새로 잡지 않는 "파괴 권한"이다. refcount 가 이미 0 이하가 된
	// 객체만 들어오며, Free* 가 먼저 _destroying 을 세워 순환에서 다시 들어온 항목은
	// 무시한다.
	_sDestroyQueue.push_back(value);
	if (_bDrainingDestroyQueue)
		return;

	_bDrainingDestroyQueue = true;
	while (_sDestroyQueue.empty() == false)
	{
		VarInfo next = _sDestroyQueue.back();
		_sDestroyQueue.pop_back();
		switch (next.GetType())
		{
		case VAR_MAP:       FreeTable(next._tbl); break;
		case VAR_LIST:      FreeList(next._lst); break;
		case VAR_SET:       FreeSet(next._set); break;
		case VAR_COROUTINE: FreeCoroutine(&next); break;
		case VAR_MODULE:    FreeWorker((CNeoVMWorker*)next._module); break;
		case VAR_ASYNC:     FreeAsync(&next); break;
		case VAR_CLOSURE:   FreeClosure(next._closure); break;
		default: break;
		}
	}
	_bDrainingDestroyQueue = false;
}

static void* GetContainerObject(VarInfo value)
{
	switch (value.GetType())
	{
	case VAR_MAP:       return value._tbl;
	case VAR_LIST:      return value._lst;
	case VAR_SET:       return value._set;
	case VAR_COROUTINE: return value._cor;
	case VAR_MODULE:    return value._module;
	case VAR_ASYNC:     return value._async;
	case VAR_CLOSURE:   return value._closure;
	default:             return nullptr;
	}
}

static bool MayContainContainerChild(VAR_TYPE type, void* object)
{
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleState._mayContainContainerChild;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleState._mayContainContainerChild;
	case VAR_SET:       return ((SetInfo*)object)->_cycleState._mayContainContainerChild;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleState._mayContainContainerChild;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleState._mayContainContainerChild;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleState._mayContainContainerChild;
	case VAR_CLOSURE:   return ((ClosureInfo*)object)->_cycleState._mayContainContainerChild;
	default:             return false;
	}
}

static bool& GetCycleCollectingFlag(VAR_TYPE type, void* object)
{
	static bool invalid = false;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleState._cycleCollecting;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleState._cycleCollecting;
	case VAR_SET:       return ((SetInfo*)object)->_cycleState._cycleCollecting;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleState._cycleCollecting;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleState._cycleCollecting;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleState._cycleCollecting;
	case VAR_CLOSURE:   return ((ClosureInfo*)object)->_cycleState._cycleCollecting;
	default:             return invalid;
	}
}

enum CycleColor : u8
{
	CYCLE_COLOR_NONE = 0,
	CYCLE_COLOR_GRAY = 1,
	CYCLE_COLOR_WHITE = 2,
	CYCLE_COLOR_BLACK = 3,
	CYCLE_COLOR_STATE_MASK = 0x7f,
	CYCLE_COLOR_NATIVE_ROOT = 0x80,
};

static u8& GetCycleColor(VAR_TYPE type, void* object)
{
	static u8 invalid = CYCLE_COLOR_NONE;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleState._cycleColor;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleState._cycleColor;
	case VAR_SET:       return ((SetInfo*)object)->_cycleState._cycleColor;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleState._cycleColor;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleState._cycleColor;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleState._cycleColor;
	case VAR_CLOSURE:   return ((ClosureInfo*)object)->_cycleState._cycleColor;
	default:             return invalid;
	}
}

static int& GetCycleScratchRefCount(VAR_TYPE type, void* object)
{
	static int invalid = 0;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleState._cycleScratchRefCount;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleState._cycleScratchRefCount;
	case VAR_SET:       return ((SetInfo*)object)->_cycleState._cycleScratchRefCount;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleState._cycleScratchRefCount;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleState._cycleScratchRefCount;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleState._cycleScratchRefCount;
	case VAR_CLOSURE:   return ((ClosureInfo*)object)->_cycleState._cycleScratchRefCount;
	default:             return invalid;
	}
}

static NEOS_FORCEINLINE u8 GetCycleColorState(VAR_TYPE type, void* object)
{
	return GetCycleColor(type, object) & CYCLE_COLOR_STATE_MASK;
}

static NEOS_FORCEINLINE void SetCycleColorState(VAR_TYPE type, void* object, u8 state)
{
	u8& color = GetCycleColor(type, object);
	color = (color & CYCLE_COLOR_NATIVE_ROOT) | state;
}

// native root가 잡은 자식은 refcount 그래프 밖의 사용처를 하나 가진다. 따라서
// 수집 그래프에 native root의 자식을 전개하지 않아도, 그 자식이 다른 경로에서
// 발견되면 scratch RC가 양수로 남아 black 처리된다.
static NEOS_FORCEINLINE bool IsCycleNativeRoot(VAR_TYPE type, void* object)
{
	if (type == VAR_MODULE)
		return true;
	if (type == VAR_ASYNC)
		return ((AsyncInfo*)object)->_state != ASYNC_READY;
	return type == VAR_COROUTINE
		&& (GetCycleColor(type, object) & CYCLE_COLOR_NATIVE_ROOT) != 0;
}

static int& GetContainerRefCountRef(VAR_TYPE type, void* object)
{
	static int invalid = 0;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_refCount;
	case VAR_LIST:      return ((ListInfo*)object)->_refCount;
	case VAR_SET:       return ((SetInfo*)object)->_refCount;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_refCount;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_refCount;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_refCount;
	case VAR_CLOSURE:   return ((ClosureInfo*)object)->_refCount;
	default:             return invalid;
	}
}

static int GetContainerRefCount(VAR_TYPE type, void* object)
{
	return GetContainerRefCountRef(type, object);
}

static VarInfo MakeContainerVar(VAR_TYPE type, void* object)
{
	VarInfo value(type);
	switch (type)
	{
	case VAR_MAP:       value._tbl = (MapInfo*)object; break;
	case VAR_LIST:      value._lst = (ListInfo*)object; break;
	case VAR_SET:       value._set = (SetInfo*)object; break;
	case VAR_COROUTINE: value._cor = (CoroutineInfo*)object; break;
	case VAR_MODULE:    value._module = (CNeoVMWorker*)object; break;
	case VAR_ASYNC:     value._async = (AsyncInfo*)object; break;
	case VAR_CLOSURE:   value._closure = (ClosureInfo*)object; break;
	default: break;
	}
	return value;
}

// Cycle collector의 gray/black 단계에서만 쓴다. 그래프를 별도 노드/children
// 벡터로 복제하지 않고 실제 컨테이너의 소유 간선을 직접 방문한다.
template<typename Visitor>
void CNeoVM::VisitCycleContainerChildren(VarInfo source, Visitor visitor)
{
	switch (source.GetType())
	{
	case VAR_MAP:
	{
		MapInfo* map = source._tbl;
		for (int i = 0; i < map->_BucketCapa; ++i)
		{
			MapNode& node = map->_Bucket[i];
			if (IsMapNodeUsed(node) && node.data)
			{
				VarInfo key = node.data->key;
				VarInfo value = node.data->value;
				if (key.IsContainerType()) visitor(key.GetType(), GetContainerObject(key));
				if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
			}
		}
		break;
	}
	case VAR_LIST:
	{
		ListInfo* list = source._lst;
		for (int i = 0; i < list->_itemCount; ++i)
		{
			VarInfo value = list->_Bucket[i];
			if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
		}
		break;
	}
	case VAR_SET:
	{
		SetInfo* set = source._set;
		for (int i = 0; i < set->_BucketCapa; ++i)
		{
			SetNode& node = set->_Bucket[i];
			if (IsSetNodeUsed(node) && node.data)
			{
				VarInfo key = node.data->key;
				if (key.IsContainerType()) visitor(key.GetType(), GetContainerObject(key));
			}
		}
		break;
	}
	case VAR_COROUTINE:
	{
		CoroutineInfo* coroutine = source._cor;
		int count = coroutine->_info._iSP_Vars_Max2;
		if (count < 0) count = 0;
		if (count > (int)coroutine->m_sVarStack.size()) count = (int)coroutine->m_sVarStack.size();
		for (int i = 0; i < count; ++i)
		{
			VarInfo value = coroutine->m_sVarStack[i];
			if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
		}
		VarInfo function = coroutine->_function;
		if (function.IsContainerType()) visitor(function.GetType(), GetContainerObject(function));
		if (coroutine->_activeClosure) visitor(VAR_CLOSURE, coroutine->_activeClosure);
		for (int i = 0; i < (int)coroutine->m_sClosureCallStack.size(); ++i)
		{
			const SClosureCallState& state = coroutine->m_sClosureCallStack[i];
			if (state._closure) visitor(VAR_CLOSURE, state._closure);
		}
		break;
	}
	case VAR_MODULE:
	{
		CNeoVMWorker* worker = (CNeoVMWorker*)source._module;
		for (VarInfo& value : worker->m_sVarGlobal)
		{
			if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
		}
		break;
	}
	case VAR_ASYNC:
	{
		VarInfo value = source._async->_LockReferance;
		if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
		value = source._async->_callback;
		if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
		break;
	}
	case VAR_CLOSURE:
	{
		for (VarInfo& value : source._closure->_captures)
			if (value.IsContainerType()) visitor(value.GetType(), GetContainerObject(value));
		break;
	}
	default:
		break;
	}
}

void CNeoVM::QueueContainerForCycleCheck(const VarInfo& source)
{
	// VM 소멸은 참조 그래프의 생존성을 판단하는 단계가 아니다. 아래의 live
	// registry 강제 해제가 최종 소유자이므로, 정리 중 새 약한 티켓을 만들면
	// 이를 다시 꺼내 delete할 기회가 없어 누수가 된다.
	if (_isTearingDown)
		return;

	VarInfo value = source;
	if (value.IsContainerType() == false)
		return;

	void* object = GetContainerObject(value);
	if (object == nullptr)
		return;

	const VAR_TYPE type = value.GetType();
	// Module은 VM worker registry가 raw 포인터로 수명 전체를 소유한다. 순환
	// 수집기가 회수할 대상이 아니며, FreeWorker가 유일한 정리 경로다.
	if (type == VAR_MODULE)
		return;
	if (MayContainContainerChild(type, object) == false)
		return;
	bool appended = false;
	switch (type)
	{
	case VAR_MAP:       appended = CycleList_Append(_sCycleMapHead, _sCycleMapTail, (MapInfo*)object); break;
	case VAR_LIST:      appended = CycleList_Append(_sCycleListHead, _sCycleListTail, (ListInfo*)object); break;
	case VAR_SET:       appended = CycleList_Append(_sCycleSetHead, _sCycleSetTail, (SetInfo*)object); break;
	case VAR_COROUTINE: appended = CycleList_Append(_sCycleCoroutineHead, _sCycleCoroutineTail, (CoroutineInfo*)object); break;
	case VAR_ASYNC:     appended = CycleList_Append(_sCycleAsyncHead, _sCycleAsyncTail, (AsyncInfo*)object); break;
	case VAR_CLOSURE:   appended = CycleList_Append(_sCycleClosureHead, _sCycleClosureTail, (ClosureInfo*)object); break;
	default:             break;
	}
	if (appended)
		++_cycleCandidateCount;
}

void CNeoVM::CancelCycleCandidate(VAR_TYPE type, void* object)
{
	if (object == nullptr)
		return;

	bool removed = false;
	switch (type)
	{
	case VAR_MAP:       removed = CycleList_Remove(_sCycleMapHead, _sCycleMapTail, (MapInfo*)object); break;
	case VAR_LIST:      removed = CycleList_Remove(_sCycleListHead, _sCycleListTail, (ListInfo*)object); break;
	case VAR_SET:       removed = CycleList_Remove(_sCycleSetHead, _sCycleSetTail, (SetInfo*)object); break;
	case VAR_COROUTINE: removed = CycleList_Remove(_sCycleCoroutineHead, _sCycleCoroutineTail, (CoroutineInfo*)object); break;
	case VAR_MODULE:    removed = CycleList_Remove(_sCycleModuleHead, _sCycleModuleTail, (CNeoVMWorker*)object); break;
	case VAR_ASYNC:     removed = CycleList_Remove(_sCycleAsyncHead, _sCycleAsyncTail, (AsyncInfo*)object); break;
	case VAR_CLOSURE:   removed = CycleList_Remove(_sCycleClosureHead, _sCycleClosureTail, (ClosureInfo*)object); break;
	default:             break;
	}
	if (removed && _cycleCandidateCount != 0)
		--_cycleCandidateCount;
}

bool CNeoVM::PopCycleCandidate(VAR_TYPE& type, void*& object)
{
	if (_cycleCandidateCount == 0)
		return false;

	for (int checked = 0; checked < 7; ++checked)
	{
		const int queueIndex = _cycleQueueRoundRobin;
		_cycleQueueRoundRobin = (_cycleQueueRoundRobin + 1) % 7;
		switch (queueIndex)
		{
		case 0:
			if (MapInfo* p = CycleList_PopFront(_sCycleMapHead, _sCycleMapTail)) { type = VAR_MAP; object = p; --_cycleCandidateCount; return true; }
			break;
		case 1:
			if (ListInfo* p = CycleList_PopFront(_sCycleListHead, _sCycleListTail)) { type = VAR_LIST; object = p; --_cycleCandidateCount; return true; }
			break;
		case 2:
			if (SetInfo* p = CycleList_PopFront(_sCycleSetHead, _sCycleSetTail)) { type = VAR_SET; object = p; --_cycleCandidateCount; return true; }
			break;
		case 3:
			if (CoroutineInfo* p = CycleList_PopFront(_sCycleCoroutineHead, _sCycleCoroutineTail)) { type = VAR_COROUTINE; object = p; --_cycleCandidateCount; return true; }
			break;
		case 4:
			if (CNeoVMWorker* p = CycleList_PopFront(_sCycleModuleHead, _sCycleModuleTail)) { type = VAR_MODULE; object = p; --_cycleCandidateCount; return true; }
			break;
		case 5:
			if (AsyncInfo* p = CycleList_PopFront(_sCycleAsyncHead, _sCycleAsyncTail)) { type = VAR_ASYNC; object = p; --_cycleCandidateCount; return true; }
			break;
		case 6:
			if (ClosureInfo* p = CycleList_PopFront(_sCycleClosureHead, _sCycleClosureTail)) { type = VAR_CLOSURE; object = p; --_cycleCandidateCount; return true; }
			break;
		}
	}
	return false;
}

void CNeoVM::ClearCycleCandidates()
{
	VAR_TYPE type = VAR_NONE;
	void* object = nullptr;
	while (PopCycleCandidate(type, object))
	{
	}
}

int CNeoVM::CollectUnreachableCycleCandidates(size_t rootBudget)
{
	_cycleWorkList.clear();
	bool activeCoroutineRootsMarked = false;
	auto setActiveCoroutineNativeRoot = [this](bool enabled)
	{
		auto setRoot = [enabled](CoroutineInfo* coroutine)
		{
			if (coroutine == nullptr)
				return;
			u8& color = GetCycleColor(VAR_COROUTINE, coroutine);
			if (enabled)
				color |= CYCLE_COLOR_NATIVE_ROOT;
			else
				color = (u8)(color & ~CYCLE_COLOR_NATIVE_ROOT);
		};

		for (const auto& pair : _sVMWorkers)
		{
			CNeoVMWorker* worker = pair.second;
			setRoot(worker->m_pMainCtx);
			setRoot(worker->m_pCur);
			setRoot(worker->m_pRegisterActive);
			for (CoroutineInfo* scheduled : worker->m_sCoroutines)
				setRoot(scheduled);
		}
	};

	// Gray 단계는 후보 루트 전체의 closure를 한 번만 만든다. color가 NONE인
	// 객체만 worklist에 넣으므로 타입+포인터 side table이 필요 없다.
	auto markGray = [&](VAR_TYPE type, void* object)
	{
		if (object == nullptr || GetCycleColorState(type, object) != CYCLE_COLOR_NONE)
			return;
		// active coroutine은 worker가 raw 포인터로 붙들고 있다. 처음 coroutine을
		// 만났을 때만 한 번 비트를 준비해 Gray 단계에서도 자식 전개를 생략한다.
		if (type == VAR_COROUTINE && activeCoroutineRootsMarked == false)
		{
			setActiveCoroutineNativeRoot(true);
			activeCoroutineRootsMarked = true;
		}
		SetCycleColorState(type, object, CYCLE_COLOR_GRAY);
		GetCycleScratchRefCount(type, object) = GetContainerRefCount(type, object);
		_cycleWorkList.push_back(MakeContainerVar(type, object));
	};

	int processed = 0;
	while (_cycleCandidateCount != 0 && (size_t)processed < rootBudget)
	{
		VAR_TYPE type = VAR_NONE;
		void* object = nullptr;
		if (PopCycleCandidate(type, object) == false)
			break;
		++processed;
		markGray(type, object);
	}
	if (_cycleWorkList.empty())
		return processed;

	for (size_t cursor = 0; cursor < _cycleWorkList.size(); ++cursor)
	{
		VarInfo source = _cycleWorkList[cursor]; // child 추가로 vector가 재할당될 수 있다.
		if (IsCycleNativeRoot(source.GetType(), GetContainerObject(source)))
			continue;
		VisitCycleContainerChildren(source, [&](VAR_TYPE type, void* object)
		{
			if (object == nullptr)
				return;
			// 컨테이너 자식을 담은 적 없는 객체는 순환의 구성원이 될 수 없다.
			// parent가 white이면 release 단계에서 이 참조를 정상 해제한다.
			if (MayContainContainerChild(type, object) == false)
				return;

			if (GetCycleColorState(type, object) == CYCLE_COLOR_NONE)
				markGray(type, object);
			// 같은 child에 여러 간선이 오면 모두 감소해야 한다. 자식 자체는 한 번만
			// worklist에 들어가지만, 이 감소는 간선마다 수행한다.
			--GetCycleScratchRefCount(type, object);
		});
	}

	const size_t touchedCount = _cycleWorkList.size();

	// scratchRC = refCount - internalIncoming 이다. 0이 아닌 값은 외부 사용처
	// (음수는 참조 장부 불일치)를 뜻하므로 모두 black으로 보수적으로 처리한다.
	const size_t blackBegin = _cycleWorkList.size();
	for (size_t i = 0; i < touchedCount; ++i)
	{
		VarInfo value = _cycleWorkList[i];
		const VAR_TYPE type = value.GetType();
		void* object = GetContainerObject(value);
		const bool nativeRoot = IsCycleNativeRoot(type, object);
		if (nativeRoot || GetCycleScratchRefCount(type, object) != 0)
		{
			SetCycleColorState(type, object, CYCLE_COLOR_BLACK);
			_cycleWorkList.push_back(value);
		}
	}
	for (size_t cursor = blackBegin; cursor < _cycleWorkList.size(); ++cursor)
	{
		VarInfo source = _cycleWorkList[cursor];
		if (IsCycleNativeRoot(source.GetType(), GetContainerObject(source)))
			continue;
		VisitCycleContainerChildren(source, [&](VAR_TYPE type, void* object)
		{
			if (object == nullptr || GetCycleColorState(type, object) != CYCLE_COLOR_GRAY)
				return;
			SetCycleColorState(type, object, CYCLE_COLOR_BLACK);
			_cycleWorkList.push_back(MakeContainerVar(type, object));
		});
	}

	// worklist의 앞쪽은 touched node, 뒤쪽은 black BFS다. White만 다시 뒤에 복사해
	// 기존과 같은 hold/release 3단계를 유지하면서도 별도 white vector를 만들지 않는다.
	const size_t whiteBegin = _cycleWorkList.size();
	for (size_t i = 0; i < touchedCount; ++i)
	{
		VarInfo value = _cycleWorkList[i];
		const VAR_TYPE type = value.GetType();
		void* object = GetContainerObject(value);
		if (GetCycleColorState(type, object) == CYCLE_COLOR_GRAY)
		{
			SetCycleColorState(type, object, CYCLE_COLOR_WHITE);
			_cycleWorkList.push_back(value);
		}
	}
	const size_t whiteEnd = _cycleWorkList.size();

	// White 전체를 먼저 hold한다. 이 뒤에 내부 간선을 지우는 동안에는 어느 white도
	// pool로 반납되지 않는다.
	for (size_t i = whiteBegin; i < whiteEnd; ++i)
	{
		VarInfo value = _cycleWorkList[i];
		void* object = GetContainerObject(value);
		CancelCycleCandidate(value.GetType(), object);
		GetCycleCollectingFlag(value.GetType(), object) = true;
		++GetContainerRefCountRef(value.GetType(), object);
	}

	// 이후 release 과정에서 black 객체가 먼저 죽을 수 있으므로, 살아 있는 모든
	// touched 객체의 임시 상태를 hold를 푸는 것보다 먼저 지운다.
	if (activeCoroutineRootsMarked)
		setActiveCoroutineNativeRoot(false);
	for (size_t i = 0; i < touchedCount; ++i)
	{
		VarInfo value = _cycleWorkList[i];
		void* object = GetContainerObject(value);
		GetCycleColor(value.GetType(), object) = CYCLE_COLOR_NONE;
		GetCycleScratchRefCount(value.GetType(), object) = 0;
	}

	if (whiteBegin == whiteEnd)
		return processed;

	auto releaseVar = [this](VarInfo& value)
	{
		if (value.IsContainerType())
		{
			const VAR_TYPE type = value.GetType();
			void* object = GetContainerObject(value);
			if (object != nullptr && GetCycleCollectingFlag(type, object))
			{
				--GetContainerRefCountRef(type, object);
				value.ClearType();
				return;
			}
		}
		Var_Release(&value);
	};

	// 모든 white 객체가 살아 있는 상태에서만 소유 간선을 제거한다. 이 단계가 끝나면
	// 컨테이너 사이에 dangling pointer가 남지 않으므로 이후에는 기존 Free*를 써도 된다.
	//
	// 이 루프만 _cycleWorkList 원소를 참조로 잡는다(다른 루프는 전부 복사한다). 아래
	// releaseVar 가 도는 Var_Release -> QueueContainerForDestroy/QueueContainerForCycleCheck
	// 경로는 _sDestroyQueue 와 후보 intrusive 링크만 건드리고 _cycleWorkList 에는
	// push 하지 않으며, 재진입은 _isCollectingCycles 가 막는다. 그래서 여기서는
	// 재할당이 일어나지 않는다 — 이 루프 안에서 워크리스트에 뭔가 넣게 되면
	// owner 가 즉시 dangling 이 되므로, 그때는 복사로 바꿔야 한다.
	for (size_t i = whiteBegin; i < whiteEnd; ++i)
	{
		VarInfo& owner = _cycleWorkList[i];
		switch (owner.GetType())
		{
		case VAR_MAP:
		{
			MapInfo* map = owner._tbl;
			for (int i = 0; i < map->_BucketCapa; ++i)
			{
				MapNode& node = map->_Bucket[i];
				if (IsMapNodeUsed(node) && node.data)
				{
					releaseVar(node.data->key);
					releaseVar(node.data->value);
				}
			}
			break;
		}
		case VAR_LIST:
		{
			ListInfo* list = owner._lst;
			for (int i = 0; i < list->_itemCount; ++i)
				releaseVar(list->_Bucket[i]);
			break;
		}
		case VAR_SET:
		{
			SetInfo* set = owner._set;
			for (int i = 0; i < set->_BucketCapa; ++i)
			{
				SetNode& node = set->_Bucket[i];
				if (IsSetNodeUsed(node) && node.data)
					releaseVar(node.data->key);
			}
			break;
		}
		case VAR_COROUTINE:
		{
			CoroutineInfo* coroutine = owner._cor;
			int count = coroutine->_info._iSP_Vars_Max2;
			if (count < 0) count = 0;
			if (count > (int)coroutine->m_sVarStack.size()) count = (int)coroutine->m_sVarStack.size();
			for (int i = 0; i < count; ++i)
				releaseVar(coroutine->m_sVarStack[i]);
			releaseVar(coroutine->_function);
			if (coroutine->_activeClosure)
			{
				VarInfo active(VAR_CLOSURE);
				active._closure = coroutine->_activeClosure;
				releaseVar(active);
				coroutine->_activeClosure = nullptr;
			}
			for (int i = 0; i < (int)coroutine->m_sClosureCallStack.size(); ++i)
			{
				SClosureCallState& state = coroutine->m_sClosureCallStack[i];
				if (state._closure == nullptr)
					continue;
				VarInfo active(VAR_CLOSURE);
				active._closure = state._closure;
				releaseVar(active);
				state._closure = nullptr;
			}
			coroutine->m_sClosureCallStack.clear();
			break;
		}
		case VAR_MODULE:
		{
			CNeoVMWorker* worker = (CNeoVMWorker*)owner._module;
			for (VarInfo& value : worker->m_sVarGlobal)
				releaseVar(value);
			break;
		}
		case VAR_ASYNC:
			releaseVar(owner._async->_LockReferance);
			releaseVar(owner._async->_callback);
			break;
		case VAR_CLOSURE:
			for (VarInfo& value : owner._closure->_captures)
				releaseVar(value);
			break;
		default:
			break;
		}
	}

	// 내부 간선이 모두 사라진 뒤 hold를 반납한다. 이제 각 객체는 안전하게 기존
	// 파괴 큐를 통해 Free*/Confer 경로로 들어간다.
	for (size_t i = whiteBegin; i < whiteEnd; ++i)
	{
		VarInfo value = _cycleWorkList[i];
		void* object = GetContainerObject(value);
		GetCycleCollectingFlag(value.GetType(), object) = false;
		if (--GetContainerRefCountRef(value.GetType(), object) <= 0)
			QueueContainerForDestroy(value);
		else
			QueueContainerForCycleCheck(value);
	}
	return processed;
}

class CycleCollectingScope
{
public:
	explicit CycleCollectingScope(bool& flag) : _flag(flag) { _flag = true; }
	~CycleCollectingScope() { _flag = false; }
private:
	bool& _flag;
};

int CNeoVM::CollectCycles(bool force)
{
	if (_isTearingDown || _isCollectingCycles || _cycleCandidateCount == 0)
		return 0;
	CycleCollectingScope collectingScope(_isCollectingCycles);

	int processedTotal = 0;
	do
	{
		const size_t total = _cycleCandidateCount;
		const size_t budget = force ? total : std::max<size_t>(16, (total + 49) / 50);
		const int processed = CollectUnreachableCycleCandidates(budget);
		processedTotal += processed;
		if (processed == 0)
			break;
	} while (force && _cycleCandidateCount != 0);

	if (processedTotal != 0)
		PublishAllocStats();
	return processedTotal;
}


FunctionPtr* CNeoVM::FunctionPtrAlloc(FunctionPtr* pOld)
{
	auto it = m_sCache_FunPtr.find(pOld->_func);
	if (it != m_sCache_FunPtr.end())
		return (*it).second;

	FunctionPtr* pNew = new FunctionPtr();
	*pNew = *pOld;
	m_sCache_FunPtr[pNew->_func] = pNew;
	return pNew;
}


static void threadFunction(CNeoVM* p)
{
	p->ThreadFunction();
}
void CNeoVM::ThreadFunction()
{
	AsyncInfo* p;
	while(false == _job_end)
	{
		if(false == _job_queue.TryPop(p))
			continue;

#ifdef _WIN32
		switch(p->_type)
		{
		case ASYNC_GET:
			try
			{
				http::Request request{p->_request};

				//const auto response = request.send("GET");
				const auto response = request.send("GET", "", p->_headers, std::chrono::milliseconds{ p->_timeout });

				p->_success = true;
				p->_resultValue = std::string{ response.body.begin(), response.body.end() };
			}
			catch (const std::exception& e)
			{
				p->_success = false;
				p->_resultValue = e.what();
			}
			break;
		case ASYNC_POST: // POST request with form data
			try
			{
				http::Request request{p->_request};

				//const auto response = request.send("POST", p->_body, {{"Content-Type", "application/x-www-form-urlencoded"}});
				const auto response = request.send("POST", p->_body, p->_headers, std::chrono::milliseconds{ p->_timeout });

				p->_success = true;
				p->_resultValue = std::string{ response.body.begin(), response.body.end() };
			}
			catch (const std::exception& e)
			{
				p->_success = false;
				p->_resultValue = e.what();
			}
			break;
		case ASYNC_POST_JSON: // POST request with a JSON body
			try
			{
				http::Request request{p->_request};

				const auto response = request.send("POST", p->_body, { {"Content-Type", "application/json"} });

				p->_success = true;
				p->_resultValue = std::string{ response.body.begin(), response.body.end() };
			}
			catch (const std::exception& e)
			{
				p->_success = false;
				p->_resultValue = e.what();
			}
			break;
		}
#endif
		p->_state = ASYNC_COMPLETED;
		_job_completed.Push(p);
		p->_event.set();
	}
}

void CNeoVM::AddHttp_Request(AsyncInfo* p)
{
	if(nullptr == _job)
	{
#ifdef _WIN32
		WSAData wsaData;
		int code = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (code != 0)
		{
			fprintf(stderr, "shite. %d\n", code);
			return;
		}
#endif //_WIN32
		_job = new std::thread(threadFunction, this);
	}

	_job_queue.Push(p);
}

// 주인 워커가 이미 소멸한 완료 async 를 폐기한다.
// (워커가 요청을 남긴 채 사라지면 아무도 pop 하지 않아 큐와 _LockReferance 가 그대로 남는다)
void CNeoVM::DiscardOrphanedAsync()
{
	AsyncInfo* p = nullptr;
	while (_orphanAsyncCount > 0 && _job_completed.TryPopMatching(p,
		[this](AsyncInfo* candidate) { return FindWorker(candidate->_ownerWorkerId) == nullptr; }))
	{
		--_orphanAsyncCount;
		Var_Release(&p->_LockReferance);   // 참조가 풀리면 AsyncInfo 도 풀로 반납된다
	}
}

// 워커 소멸 직전 호출. 이 워커가 주인인 완료 async 를 폐기한다.
// 아직 처리 중인 요청은 나중에 완료 큐에 들어오며, DiscardOrphanedAsync 가 정리한다.
void CNeoVM::DiscardAsyncByWorker(u32 workerId, int pendingCount)
{
	int discarded = 0;
	AsyncInfo* p = nullptr;
	while (_job_completed.TryPopMatching(p,
		[workerId](AsyncInfo* candidate) { return candidate->_ownerWorkerId == workerId; }))
	{
		++discarded;
		Var_Release(&p->_LockReferance);
	}

	// 완료 큐에 없던 나머지는 아직 처리 중이다. 나중에 완료 큐로 들어오면
	// DiscardOrphanedAsync 가 회수하도록 수를 넘겨둔다.
	int stillRunning = pendingCount - discarded;
	if (stillRunning > 0)
		_orphanAsyncCount += stillRunning;
}

AsyncInfo* CNeoVM::Pop_AsyncInfo(CNeoVMWorker* pOwnerWorker)
{
	// 고아가 하나도 없으면(대부분의 경우) 검사 자체를 건너뛴다.
	if (_orphanAsyncCount > 0)
		DiscardOrphanedAsync();

	const u32 ownerId = pOwnerWorker->GetWorkerID();
	AsyncInfo* p = nullptr;
	if(_job_completed.TryPopMatching(p, [ownerId](AsyncInfo* candidate) { return candidate->_ownerWorkerId == ownerId; }))
	{
		if (pOwnerWorker->_asyncPendingCount > 0)
			--pOwnerWorker->_asyncPendingCount;
		return p;
	}
	return nullptr;
}


CNeoVM::CNeoVM()
{
	// 워커가 늘어나는 과정에서 rehash(전체 재해싱) 스파이크가 프레임에 걸리지 않게
	// 버킷을 미리 잡아둔다. 부족하면 그때부터 평소대로 확장된다.
	_sVMWorkers.reserve(4096);

	for (int i = 0; i < NDF_MAX; i++)
	{
		m_sDefaultValue[i].ClearType();
		switch (i)
		{
		case NDF_NULL: Var_SetStringA(&m_sDefaultValue[i], "null"); break;
		case NDF_INT: Var_SetStringA(&m_sDefaultValue[i], "int"); break;
		case NDF_FLOAT: Var_SetStringA(&m_sDefaultValue[i], "float"); break;
		case NDF_BOOL: Var_SetStringA(&m_sDefaultValue[i], "bool"); break;
		case NDF_STRING: Var_SetStringA(&m_sDefaultValue[i], "string"); break;
		case NDF_TABLE: Var_SetStringA(&m_sDefaultValue[i], "map"); break;
		case NDF_LIST: Var_SetStringA(&m_sDefaultValue[i], "list"); break;
		case NDF_SET: Var_SetStringA(&m_sDefaultValue[i], "set"); break;
		case NDF_COROUTINE: Var_SetStringA(&m_sDefaultValue[i], "coroutine"); break;
		case NDF_FUNCTION: Var_SetStringA(&m_sDefaultValue[i], "function"); break;
		case NDF_MODULE: Var_SetStringA(&m_sDefaultValue[i], "module"); break;
		case NDF_ASYNC: Var_SetStringA(&m_sDefaultValue[i], "asynchronous"); break;

		case NDF_TRUE: Var_SetStringA(&m_sDefaultValue[i], "true"); break;
		case NDF_FALSE: Var_SetStringA(&m_sDefaultValue[i], "false"); break;

		case NDF_SUSPENDED: Var_SetStringA(&m_sDefaultValue[i], "suspended"); break;
		case NDF_RUNNING: Var_SetStringA(&m_sDefaultValue[i], "running"); break;
		case NDF_DEAD: Var_SetStringA(&m_sDefaultValue[i], "dead"); break;
		case NDF_NORMAL: Var_SetStringA(&m_sDefaultValue[i], "normal"); break;
		case NDF_VEC2: Var_SetStringA(&m_sDefaultValue[i], "Vector2"); break;
		case NDF_VEC3: Var_SetStringA(&m_sDefaultValue[i], "Vector3"); break;
		case NDF_VEC4: Var_SetStringA(&m_sDefaultValue[i], "Vector4"); break;
		case NDF_QUAT: Var_SetStringA(&m_sDefaultValue[i], "Quaternion"); break;
		default:
			SetError(g_sNeoRuntimeErrors[RTE_DEFAULT_VALUE]);
			break;
		}
	}
//	InitLib();
}

int CNeoVM::GetBytesSize() const
{
	return _pMainWorker != nullptr ? _pMainWorker->GetBytesSize() : 0;
}

CNeoVM::~CNeoVM()
{
	// 이후의 Var_Release 는 VM 종료를 위한 부수 정리일 뿐이다. refcount가
	// 남았다는 이유로 순환 후보를 만들지 말고, 아래 live registry sweep으로
	// 모든 컨테이너를 강제 제거한다.
	_isTearingDown = true;
	_job_end = true;
	if(nullptr != _job)
	{
		_job->join();
		delete _job;
		_job = nullptr;
	}
	// 후보는 객체 자신이 들고 있는 intrusive 링크다. 실제 객체를 정리하기 전에
	// 모두 unlink해 뒤따르는 Free*가 이미 비워진 큐를 다시 건드리지 않게 한다.
	ClearCycleCandidates();
	while (_sVMWorkers.empty() == false)
		FreeWorker(_sVMWorkers.begin()->second);

	for (int i = 0; i < NDF_MAX; i++)
		Var_Release(&m_sDefaultValue[i]);

	// 살아남은 컨테이너와 closure의 내부 저장소를 해제 (intrusive live 리스트 순회).
	// String 은 CNVMInstPool 소멸자가 std::str 을 정리하므로 별도 처리 없음.
	// Free* 는 먼저 파괴 표식을 세우고 live 리스트에서 뺀다. 내부 항목의
	// Var_Release 가 같은 객체로 되돌아와도 재진입하지 않는다.
	while (_sClosureHead || _sTableHead || _sListHead || _sSetHead)
	{
		if (_sClosureHead)
		{
			FreeClosure(_sClosureHead);
			continue;
		}
		if (_sTableHead)
		{
			FreeTable(_sTableHead);
			continue;
		}
		if (_sListHead)
		{
			FreeList(_sListHead);
			continue;
		}
		FreeSet(_sSetHead);
	}

	// _isTearingDown guard 때문에 보통 비어 있다. 종료 경로가 확장돼도 객체가
	// 남긴 intrusive 링크를 풀기 위해 한 번 더 비운다.
	ClearCycleCandidates();

	for (auto it = m_sCache_FunPtr.begin(); it != m_sCache_FunPtr.end(); it++)
		delete (*it).second;
	m_sCache_FunPtr.clear();
	PublishAllocStats();
	// 풀 페이지는 곧 멤버 소멸자가 free 한다. 지금 빼두지 않으면 이 VM 몫이 전역 집계에 영원히 남는다.
	PublishNeoVMAllocStatValue(g_iNeoVMPoolBytes, m_sPublishedAllocStats.poolBytes, 0);
	// 유휴 문자열 버퍼도 같은 이유로 빼야 한다. 전역은 델타 누적이라, 죽는 VM 이 자기 몫을
	// 0 으로 되돌리지 않으면 되돌릴 기회가 영영 없다(기준선인 m_sPublishedAllocStats 가 같이 사라진다).
	PublishNeoVMAllocStatValue(g_iNeoVMStringIdleBytes, m_sPublishedAllocStats.stringIdleBytes, 0);
}

void CNeoVM::SetError(const std::string& msg)
{
	if (_bError)
	{	// already error msg
		return;
	}
	if (msg.empty() == false)
	{
		_pErrorMsg = msg;
		_bError = true;
	}
	else
	{
		_pErrorMsg.clear();
		_bError = false;
	}
}

// 바이트 이미지 직접 로드 — 이 워커 전용 프로그램을 1회 만들고 소유권을 넘긴다.
// 같은 스크립트를 여러 워커에 붙일 때는 CNeoVMProgram 을 캐시해 아래 오버로드를 쓴다.
INeoVMWorker* CNeoVM::LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker, bool init, int iStackSize)
{
	std::string err;
	CNeoVMProgram* pProgram = CNeoVMProgram::Create(pBuffer, iSize, &err);
	if (pProgram == nullptr)
	{
		SetError(err);
		return NULL;
	}

	INeoVMWorker* pWorker = LoadVM(vparam, pProgram, blMainWorker, init, iStackSize);
	pProgram->Release();   // 성공했으면 워커가 자기 참조를 들고 있다
	return pWorker;
}

INeoVMWorker* CNeoVM::LoadVM(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, bool blMainWorker, bool init, int iStackSize)
{
	if (pProgram == nullptr)
	{
		SetError(g_sNeoRuntimeErrors[RTE_PROGRAM_REQUIRED]);
		return NULL;
	}
	if (vparam != nullptr && vparam->execPool != nullptr)
		_pExecPool = vparam->execPool;   // 이후 모듈 로드 워커들이 상속할 수 있도록 VM 에 보관
	if (_pExecPool == nullptr)
	{
		SetError(g_sNeoRuntimeErrors[RTE_EXEC_POOL_REQUIRED]);
		return NULL;
	}

	CNeoVMWorker*pWorker = WorkerAlloc(iStackSize);
	if (false == pWorker->Init(vparam, pProgram, iStackSize))
	{
		FreeWorker(pWorker);
		return NULL;
	}
	if (blMainWorker && NULL == _pMainWorker)
		_pMainWorker = pWorker;
	if(init)
	{
		std::vector<VarInfo> _args;
		pWorker->ExecuteTop(0, _args);
	}
	PublishAllocStats();
	return pWorker;
}
bool CNeoVM::ReleaseWorker(INeoVMWorker* worker)
{
	CNeoVMWorker* pWorker = static_cast<CNeoVMWorker*>(worker);
	if (pWorker == nullptr)
		return false;
	auto it = _sVMWorkers.find(pWorker->GetWorkerID());
	if (it == _sVMWorkers.end() || it->second != pWorker)
		return false;
	const bool isMainWorker = (pWorker == _pMainWorker);
	FreeWorker(pWorker);
	if (isMainWorker)
		_pMainWorker = NULL;
	PublishAllocStats();
	return true;
}


};
