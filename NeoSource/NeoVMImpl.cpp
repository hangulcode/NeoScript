#include <math.h>
#include <stdlib.h>
#include <limits.h>   // INT_MAX (회수 예산 무제한 표기)
#include <atomic>
#include "NeoVMImpl.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"
#include "UTFString.h"

#ifdef _WIN32
//https://github.com/elnormous/HTTPRequest
#include "HttpRequest.h"
#pragma comment(lib, "ws2_32.lib")
#endif


namespace NeoScript
{

static std::atomic<int> g_iNeoVMAllocStrings{ 0 };
static std::atomic<int> g_iNeoVMAllocMaps{ 0 };
static std::atomic<int> g_iNeoVMAllocLists{ 0 };
static std::atomic<int> g_iNeoVMAllocSets{ 0 };
static std::atomic<int> g_iNeoVMAllocCoroutines{ 0 };
static std::atomic<int> g_iNeoVMAllocModules{ 0 };
static std::atomic<int> g_iNeoVMAllocAsyncs{ 0 };
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
	outStats.vectors = g_iNeoVMAllocVectors.load(std::memory_order_relaxed);
	outStats.poolBytes = g_iNeoVMPoolBytes.load(std::memory_order_relaxed)
	                   + g_iNeoVMExecPoolBytes.load(std::memory_order_relaxed);
	outStats.stringIdleBytes = g_iNeoVMStringIdleBytes.load(std::memory_order_relaxed);
}

bool GetNeoVMAllocStats(INeoVM* pVM, SNeoVMAllocStats& outStats)
{
	if (pVM == nullptr)
		return false;

	((CNeoVMImpl*)pVM)->GetAllocStats(outStats);
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
long long CNeoVMImpl::CollectEmptyPages(bool force)
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
size_t CNeoVMImpl::CollectPoolAt(int idx, NeoPoolClock::time_point now, int holdMs, int& pageBudget)
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
	default: return m_sPool_String.Collect(now, holdMs, pageBudget);
	}
}

long long CNeoVMImpl::PoolBytes() const
{
	return (long long)m_sPool_TableData.ReservedBytes()
	     + (long long)m_sPool_TableInfo.ReservedBytes()
	     + (long long)m_sPool_FunctionProperty.ReservedBytes()
	     + (long long)m_sPool_SetData.ReservedBytes()
	     + (long long)m_sPool_SetInfo.ReservedBytes()
	     + (long long)m_sPool_ListInfo.ReservedBytes()
	     + (long long)m_sPool_Vec.ReservedBytes()
	     + (long long)m_sPool_Async.ReservedBytes()
	     + (long long)m_sPool_String.ReservedBytes();
	// _pExecPool 은 스레드별 공유(엔진 소유)라 여기서 세면 VM 수만큼 중복된다 → 자기가 직접 publish.
}

// 반납된(놀고 있는) 문자열 노드가 아직 붙들고 있는 문자 버퍼의 합계.
// 풀 페이지(PoolBytes)에는 안 잡히는 값이라 따로 센다. free 리스트를 훑으므로
// 매 프레임이 아니라 통계 조회 시점에만 부른다.
long long CNeoVMImpl::StringIdleBytes()
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
void CNeoVMImpl::GetAllocStats(SNeoVMAllocStats& outStats)
{
	m_sAllocStats.stringIdleBytes = StringIdleBytes();
	PublishNeoVMAllocStatValue(g_iNeoVMStringIdleBytes,
		m_sPublishedAllocStats.stringIdleBytes, m_sAllocStats.stringIdleBytes);
	outStats = m_sAllocStats;
	outStats.poolBytes = PoolBytes();
}

// 주의: 여기서 stringIdleBytes 를 다시 재지 않는다.
// StringIdleBytes() 는 문자열 free 리스트 전수 순회라 O(유휴 노드) 인데, 이 함수는
// RunFunction 이 끝날 때마다 그리고 증분 회수가 도는 동안에는 매 프레임 불린다.
// 그러면 "한 호출의 비용에 상한을 둔다" 는 증분 회수의 전제가 깨진다.
// 갱신은 GetAllocStats(조회) 에서만 하고, 여기서는 마지막 값을 그대로 다시 publish 한다
// (델타 0 이라 전역은 변하지 않는다).
void CNeoVMImpl::PublishAllocStats()
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

void CNeoVMImpl::Var_SetString(VarInfo *d, const char* str)
{
	std::string s(str);
	Var_SetStringA(d, s);
}
void CNeoVMImpl::Var_SetStringA(VarInfo *d, const std::string& str)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = StringAlloc(str);
	++d->_str->_refCount;
}
void CNeoVMImpl::Var_SetTable(VarInfo *d, MapInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_MAP);
	d->_tbl = p;
	++d->_tbl->_refCount;
}


CNeoVMWorker* CNeoVMImpl::WorkerAlloc(int iStackSize)
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
	p->_cycleTicket = nullptr;
	p->_destroying = false;
	p->_cycleQueued = false;
	p->_cycleCollecting = false;
	p->_mayContainContainerChild = true;
	++m_sAllocStats.modules;

	_sVMWorkers[_dwLastIDVMWorker] = p;
	return p;
}
void CNeoVMImpl::FreeWorker(CNeoVMWorker *d)
{
	if (d == nullptr || d->_destroying)
		return;
	auto it = _sVMWorkers.find(d->GetWorkerID());
	if (it == _sVMWorkers.end())
		return;

	d->_destroying = true;
	CancelCycleCandidate(VAR_MODULE, d);
	_sVMWorkers.erase(it);
	--m_sAllocStats.modules;
	delete d;
}
CNeoVMWorker* CNeoVMImpl::FindWorker(int iModule)
{
	auto it = _sVMWorkers.find(iModule);
	if (it == _sVMWorkers.end())
		return NULL;

	return (*it).second;
}

int CNeoVMImpl::FindFunction(const std::string& name) { return GetMainWorker()->FindFunction(name); }


CoroutineInfo* CNeoVMImpl::CoroutineAlloc()
{
	// 코루틴 컨텍스트도 default 실행 컨텍스트와 동일한 공유 풀에서 대여한다.
	CoroutineInfo* p = _pExecPool->Acquire();
	++m_sAllocStats.coroutines;
	return p;
}
void CNeoVMImpl::FreeCoroutine(VarInfo *d)
{
	CoroutineInfo* pCI = d->_cor;
	CancelCycleCandidate(VAR_COROUTINE, pCI);
	if (pCI->_destroying)
		return;
	pCI->_destroying = true;
	--m_sAllocStats.coroutines;
	// 공유 풀로 반납하기 전에 컨텍스트의 스택 참조를 정리한다.
	// 완료(DeadCoroutine)를 거친 코루틴은 _iSP_Vars_Max2 가 0 이라 무해하지만,
	// yield 된 채 버려진 코루틴은 live ref 가 남아있어 정리하지 않으면 다른 VM 재사용 시 손상된다.
	int n = pCI->_info._iSP_Vars_Max2;
	std::vector<VarInfo>& s = pCI->m_sVarStack;
	if (n > (int)s.size()) n = (int)s.size();
	for (int i = 0; i < n; i++)
		Var_Release(&s[i]);
	pCI->_info.ClearSP();
	pCI->m_sAsyncResumeCodePtrs.clear();
	_pExecPool->Release(pCI);
}

StringInfo* CNeoVMImpl::StringAlloc(const std::string& str)
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

StringInfo* CNeoVMImpl::StringIntern(const std::string& str)
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

StringInfo* CNeoVMImpl::FindInternedString(const std::string& str, u32 hash) const
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

StringInfo* CNeoVMImpl::StringFind(const std::string& str) const
{
	return FindInternedString(str, GetHashCode(str));
}

StringInfo* CNeoVMImpl::StringFind(StringInfo* pString) const
{
	return pString ? FindInternedString(pString->_str, pString->GetHash()) : nullptr;
}

StringInfo* FindCanonicalString(CNeoVMImpl* pVM, StringInfo* pString)
{
	return pVM ? pVM->StringFind(pString) : nullptr;
}

void CNeoVMImpl::RehashStringIntern(int capacity)
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

void CNeoVMImpl::InsertInternedString(StringInfo* p)
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

void CNeoVMImpl::RemoveInternedString(StringInfo* p)
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

void CNeoVMImpl::FreeString(VarInfo *d)
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
VecInfo* CNeoVMImpl::VecAlloc()
{
	// MapNode 와 같은 CNVMAllocPool — 생성자를 부르지 않는 raw 블록이라 성분은 호출측이 채운다.
	VecInfo* p = m_sPool_Vec.Receive();
	p->_refCount = 0;
	++m_sAllocStats.vectors;
	return p;
}
void CNeoVMImpl::FreeVec(VecInfo* p)
{
	--m_sAllocStats.vectors;
	m_sPool_Vec.Confer(p);
}
VecInfo* CNeoVMImpl::VecCopyOnWrite(VarInfo* d)
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
MapInfo* CNeoVMImpl::TableAlloc(int cnt)
{
	MapInfo* pTable = m_sPool_TableInfo.Receive();
	pTable->_pVM = this;
	pTable->_refCount = 0;
	pTable->_cycleTicket = nullptr;
	pTable->_destroying = false;
	pTable->_cycleQueued = false;
	pTable->_cycleCollecting = false;
	pTable->_mayContainContainerChild = false;
	pTable->_itemCount = 0;
	pTable->_mutationVersion = 0;
	pTable->_HashBase = 0;
	pTable->_BucketCapa = 0;
	pTable->_lastFree = -1;
	pTable->_Bucket = nullptr;
	pTable->_meta = NULL;

	LiveList_Insert(_sTableHead, pTable);
	if (cnt > 0) pTable->Reserve(cnt);
	++m_sAllocStats.maps;
	return pTable;
}
void CNeoVMImpl::FreeTable(MapInfo* tbl)
{
	CancelCycleCandidate(VAR_MAP, tbl);
	if (tbl->_destroying)
		return;
	tbl->_destroying = true;
	LiveList_Remove(_sTableHead, tbl);

	if (tbl->_meta)
	{
		if (--tbl->_meta->_refCount <= 0)
		{
			VarInfo meta(VAR_MAP);
			meta._tbl = tbl->_meta;
			QueueContainerForDestroy(meta);
		}
		tbl->_meta = NULL;
	}
	tbl->Free();

	//delete tbl;
	m_sPool_TableInfo.Confer(tbl);
	--m_sAllocStats.maps;
}
FunctionPropertyInfo* CNeoVMImpl::FunctionPropertyAlloc()
{
	FunctionPropertyInfo* fp = m_sPool_FunctionProperty.Receive();
	fp->_refCount = 0;
	fp->_fun._func = NULL;
	fp->_fun._property = NULL;
	fp->_pUserData = NULL;
	return fp;
}
void CNeoVMImpl::FreeFunctionProperty(FunctionPropertyInfo* fp)
{
	fp->_fun._func = NULL;
	fp->_fun._property = NULL;
	fp->_pUserData = NULL;
	m_sPool_FunctionProperty.Confer(fp);
}
ListInfo* CNeoVMImpl::ListAlloc(int cnt)
{
	ListInfo* pList = m_sPool_ListInfo.Receive();
	pList->_pVM = this;
	pList->_refCount = 0;
	pList->_cycleTicket = nullptr;
	pList->_destroying = false;
	pList->_cycleQueued = false;
	pList->_cycleCollecting = false;
	pList->_mayContainContainerChild = false;
	pList->_mutationVersion = 0;
	pList->_pUserData = NULL;
	pList->_pIndexer = nullptr;
	pList->InitInlineBucket();   // _Bucket=인라인, capa=4, itemCount=0 (작은 리스트는 힙 할당 없음)

	LiveList_Insert(_sListHead, pList);
	if (cnt > 0) pList->Resize(cnt);
	++m_sAllocStats.lists;
	return pList;
}
void CNeoVMImpl::FreeList(ListInfo* lst)
{
	CancelCycleCandidate(VAR_LIST, lst);
	if (lst->_destroying)
		return;
	lst->_destroying = true;
	LiveList_Remove(_sListHead, lst);
	lst->Free();

	//delete tbl;
	m_sPool_ListInfo.Confer(lst);
	--m_sAllocStats.lists;
}
SetInfo* CNeoVMImpl::SetAlloc()
{
	SetInfo* pSet = m_sPool_SetInfo.Receive();
	pSet->_pVM = this;
	pSet->_refCount = 0;
	pSet->_cycleTicket = nullptr;
	pSet->_destroying = false;
	pSet->_cycleQueued = false;
	pSet->_cycleCollecting = false;
	pSet->_mayContainContainerChild = false;
	pSet->_itemCount = 0;
	pSet->_mutationVersion = 0;
	pSet->_HashBase = 0;
	pSet->_BucketCapa = 0;
	pSet->_lastFree = -1;
	pSet->_Bucket = nullptr;
	pSet->_meta = NULL;

	LiveList_Insert(_sSetHead, pSet);
	++m_sAllocStats.sets;
	return pSet;
}
void CNeoVMImpl::FreeSet(SetInfo* set)
{
	CancelCycleCandidate(VAR_SET, set);
	if (set->_destroying)
		return;
	set->_destroying = true;
	LiveList_Remove(_sSetHead, set);
	if (set->_meta)
	{
		if (--set->_meta->_refCount <= 0)
		{
			VarInfo meta(VAR_SET);
			meta._set = set->_meta;
			QueueContainerForDestroy(meta);
		}
		set->_meta = NULL;
	}
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

AsyncInfo* CNeoVMImpl::AsyncAlloc()
{
	AsyncInfo* p = m_sPool_Async.Receive();
	p->_refCount = 0;
	p->_cycleTicket = nullptr;
	p->_destroying = false;
	p->_cycleQueued = false;
	p->_cycleCollecting = false;
	p->_mayContainContainerChild = false;
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
	++m_sAllocStats.asyncs;
	return p;
}
void CNeoVMImpl::FreeAsync(VarInfo* d)
{
	AsyncInfo* p = d->_async;
	CancelCycleCandidate(VAR_ASYNC, p);
	if (p->_destroying)
		return;
	p->_destroying = true;
	--m_sAllocStats.asyncs;
	// AsyncInfo 는 노드가 372B 로 가장 크고, 문자열 멤버 셋에 HTTP 본문까지 담긴다
	// (_resultValue 는 응답 전문이라 MB 단위도 가능). 문자열 풀과 같은 규칙으로 놓아준다.
	ReleaseIfLarge(p->_request);
	ReleaseIfLarge(p->_body);
	ReleaseIfLarge(p->_resultValue);
	// 헤더는 요청마다 새로 쌓이고 재사용 이득이 없다 — 배열째 돌려준다.
	std::vector< std::pair<std::string, std::string> >().swap(p->_headers);
	m_sPool_Async.Confer(p);
}

void CNeoVMImpl::QueueContainerForDestroy(const VarInfo& value)
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
		default: break;
		}
	}
	_bDrainingDestroyQueue = false;
}

struct CycleCandidate
{
	VAR_TYPE type;
	void* object;
};

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
	default:             return nullptr;
	}
}

static bool MayContainContainerChild(VAR_TYPE type, void* object)
{
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_mayContainContainerChild;
	case VAR_LIST:      return ((ListInfo*)object)->_mayContainContainerChild;
	case VAR_SET:       return ((SetInfo*)object)->_mayContainContainerChild;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_mayContainContainerChild;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_mayContainContainerChild;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_mayContainContainerChild;
	default:             return false;
	}
}

static bool& GetCycleQueuedFlag(VAR_TYPE type, void* object)
{
	static bool invalid = false;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleQueued;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleQueued;
	case VAR_SET:       return ((SetInfo*)object)->_cycleQueued;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleQueued;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleQueued;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleQueued;
	default:             return invalid;
	}
}

static bool& GetCycleCollectingFlag(VAR_TYPE type, void* object)
{
	static bool invalid = false;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleCollecting;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleCollecting;
	case VAR_SET:       return ((SetInfo*)object)->_cycleCollecting;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleCollecting;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleCollecting;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleCollecting;
	default:             return invalid;
	}
}

static CycleCandidate*& GetCycleTicket(VAR_TYPE type, void* object)
{
	static CycleCandidate* invalid = nullptr;
	switch (type)
	{
	case VAR_MAP:       return ((MapInfo*)object)->_cycleTicket;
	case VAR_LIST:      return ((ListInfo*)object)->_cycleTicket;
	case VAR_SET:       return ((SetInfo*)object)->_cycleTicket;
	case VAR_COROUTINE: return ((CoroutineInfo*)object)->_cycleTicket;
	case VAR_MODULE:    return ((CNeoVMWorker*)object)->_cycleTicket;
	case VAR_ASYNC:     return ((AsyncInfo*)object)->_cycleTicket;
	default:             return invalid;
	}
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
	default: break;
	}
	return value;
}

void CNeoVMImpl::QueueContainerForCycleCheck(const VarInfo& source)
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
	if (MayContainContainerChild(type, object) == false)
		return;
	bool& queued = GetCycleQueuedFlag(type, object);
	if (queued)
		return;

	CycleCandidate* ticket = new CycleCandidate{ type, object };
	queued = true;
	GetCycleTicket(type, object) = ticket;
	_sCycleCandidates.push_back(ticket);
}

void CNeoVMImpl::CancelCycleCandidate(VAR_TYPE type, void* object)
{
	if (object == nullptr)
		return;

	bool& queued = GetCycleQueuedFlag(type, object);
	if (queued == false)
		return;

	CycleCandidate*& ticket = GetCycleTicket(type, object);
	queued = false;
	if (ticket != nullptr)
		ticket->object = nullptr;
	ticket = nullptr;
}

struct CycleNodeKey
{
	VAR_TYPE type;
	void* object;

	bool operator==(const CycleNodeKey& rhs) const
	{
		return type == rhs.type && object == rhs.object;
	}
};

struct CycleNodeKeyHash
{
	size_t operator()(const CycleNodeKey& key) const
	{
		return ((size_t)(uintptr_t)key.object >> 3) ^ ((size_t)key.type * 0x9e3779b9u);
	}
};

struct CycleGraphNode
{
	CycleNodeKey key;
	int refCount = 0;
	int internalIncoming = 0;
	bool markedLive = false;
	bool nativeRoot = false;
	std::vector<int> children;
};

bool CNeoVMImpl::CollectUnreachableCycleCandidate(VAR_TYPE candidateType, void* candidateObject)
{
	std::vector<CycleGraphNode> nodes;
	std::unordered_map<CycleNodeKey, int, CycleNodeKeyHash> indices;

	auto isNativeRoot = [this](VAR_TYPE type, void* object)
	{
		// Module은 VM worker registry가 raw 포인터로 소유한다. 요청을 보낸 Async도
		// worker/completed queue가 소유하므로 refcount 그래프 밖의 사용처로 취급한다.
		if (type == VAR_MODULE)
			return true;
		if (type == VAR_ASYNC)
			return ((AsyncInfo*)object)->_state != ASYNC_READY;

		if (type != VAR_COROUTINE)
			return false;

		CoroutineInfo* coroutine = (CoroutineInfo*)object;
		for (const auto& pair : _sVMWorkers)
		{
			CNeoVMWorker* worker = pair.second;
			if (worker->m_pMainCtx == coroutine || worker->m_pCur == coroutine
				|| worker->m_pRegisterActive == coroutine)
				return true;
			for (CoroutineInfo* scheduled : worker->m_sCoroutines)
			{
				if (scheduled == coroutine)
					return true;
			}
		}
		return false;
	};

	auto addNode = [&](VAR_TYPE type, void* object)
	{
		const CycleNodeKey key{ type, object };
		auto found = indices.find(key);
		if (found != indices.end())
			return found->second;

		const int index = (int)nodes.size();
		CycleGraphNode node;
		node.key = key;
		node.refCount = GetContainerRefCount(type, object);
		node.nativeRoot = isNativeRoot(type, object);
		nodes.push_back(node);
		indices.emplace(key, index);
		return index;
	};

	addNode(candidateType, candidateObject);
	for (int source = 0; source < (int)nodes.size(); ++source)
	{
		auto addEdge = [&](VAR_TYPE type, void* object)
		{
			if (object == nullptr)
				return;
			const int child = addNode(type, object);
			++nodes[child].internalIncoming;
			nodes[source].children.push_back(child);
		};
		auto addVarEdge = [&](VarInfo& value)
		{
			if (value.IsContainerType())
				addEdge(value.GetType(), GetContainerObject(value));
		};
		auto addCoroutineVars = [&](CoroutineInfo* coroutine)
		{
			int count = coroutine->_info._iSP_Vars_Max2;
			if (count < 0) count = 0;
			if (count > (int)coroutine->m_sVarStack.size()) count = (int)coroutine->m_sVarStack.size();
			for (int i = 0; i < count; ++i)
				addVarEdge(coroutine->m_sVarStack[i]);
		};

		switch (nodes[source].key.type)
		{
		case VAR_MAP:
		{
			MapInfo* map = (MapInfo*)nodes[source].key.object;
			if (map->_meta) addEdge(VAR_MAP, map->_meta);
			for (int i = 0; i < map->_BucketCapa; ++i)
			{
				MapNode& node = map->_Bucket[i];
				if (IsMapNodeUsed(node) && node.data)
				{
					addVarEdge(node.data->key);
					addVarEdge(node.data->value);
				}
			}
			break;
		}
		case VAR_LIST:
		{
			ListInfo* list = (ListInfo*)nodes[source].key.object;
			for (int i = 0; i < list->_itemCount; ++i)
				addVarEdge(list->_Bucket[i]);
			break;
		}
		case VAR_SET:
		{
			SetInfo* set = (SetInfo*)nodes[source].key.object;
			if (set->_meta) addEdge(VAR_SET, set->_meta);
			for (int i = 0; i < set->_BucketCapa; ++i)
			{
				SetNode& node = set->_Bucket[i];
				if (IsSetNodeUsed(node) && node.data)
					addVarEdge(node.data->key);
			}
			break;
		}
		case VAR_COROUTINE:
			addCoroutineVars((CoroutineInfo*)nodes[source].key.object);
			break;
		case VAR_MODULE:
		{
			CNeoVMWorker* worker = (CNeoVMWorker*)nodes[source].key.object;
			for (VarInfo& value : worker->m_sVarGlobal)
				addVarEdge(value);
			break;
		}
		case VAR_ASYNC:
			addVarEdge(((AsyncInfo*)nodes[source].key.object)->_LockReferance);
			break;
		default:
			break;
		}
	}

	std::vector<int> work;
	for (int i = 0; i < (int)nodes.size(); ++i)
	{
		CycleGraphNode& node = nodes[i];
		// 내부 간선보다 refcount가 작으면 참조 장부가 이미 손상된 상태다. 안전을 위해
		// 외부 사용처가 있는 것으로 보수적으로 처리한다.
		if (node.refCount < node.internalIncoming || node.nativeRoot
			|| node.refCount > node.internalIncoming)
		{
			node.markedLive = true;
			work.push_back(i);
		}
	}
	for (size_t cursor = 0; cursor < work.size(); ++cursor)
	{
		for (int child : nodes[work[cursor]].children)
		{
			if (nodes[child].markedLive == false)
			{
				nodes[child].markedLive = true;
				work.push_back(child);
			}
		}
	}

	if (nodes.empty() || nodes[0].markedLive)
		return false;

	// white set 전체를 하나의 수집 단위로 잡는다. 후보 하나만 먼저 Free하면 그 자식이
	// 아직 부모를 가리킨 채 살아남아 pool에 반납된 부모를 역참조할 수 있다.
	std::vector<VarInfo> white;
	white.reserve(nodes.size());
	for (const CycleGraphNode& node : nodes)
	{
		if (node.markedLive == false)
			white.push_back(MakeContainerVar(node.key.type, node.key.object));
	}

	// ticket을 먼저 무효화하고 GC hold를 하나씩 더한다. 아래에서 내부 간선을 지우는
	// 동안에는 어떤 white 객체도 pool로 반납되지 않는다.
	for (VarInfo& value : white)
	{
		void* object = GetContainerObject(value);
		CancelCycleCandidate(value.GetType(), object);
		GetCycleCollectingFlag(value.GetType(), object) = true;
		++GetContainerRefCountRef(value.GetType(), object);
	}

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

	auto releaseMapMeta = [this](MapInfo*& meta)
	{
		if (meta == nullptr)
			return;
		if (GetCycleCollectingFlag(VAR_MAP, meta))
			--GetContainerRefCountRef(VAR_MAP, meta);
		else
		{
			VarInfo value = MakeContainerVar(VAR_MAP, meta);
			Var_Release(&value);
		}
		meta = nullptr;
	};

	auto releaseSetMeta = [this](SetInfo*& meta)
	{
		if (meta == nullptr)
			return;
		if (GetCycleCollectingFlag(VAR_SET, meta))
			--GetContainerRefCountRef(VAR_SET, meta);
		else
		{
			VarInfo value = MakeContainerVar(VAR_SET, meta);
			Var_Release(&value);
		}
		meta = nullptr;
	};

	// 모든 white 객체가 살아 있는 상태에서만 소유 간선을 제거한다. 이 단계가 끝나면
	// 컨테이너 사이에 dangling pointer가 남지 않으므로 이후에는 기존 Free*를 써도 된다.
	for (VarInfo& owner : white)
	{
		switch (owner.GetType())
		{
		case VAR_MAP:
		{
			MapInfo* map = owner._tbl;
			releaseMapMeta(map->_meta);
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
			releaseSetMeta(set->_meta);
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
			break;
		default:
			break;
		}
	}

	// 내부 간선이 모두 사라진 뒤 hold를 반납한다. 이제 각 객체는 안전하게 기존
	// 파괴 큐를 통해 Free*/Confer 경로로 들어간다.
	for (VarInfo& value : white)
	{
		void* object = GetContainerObject(value);
		GetCycleCollectingFlag(value.GetType(), object) = false;
		if (--GetContainerRefCountRef(value.GetType(), object) <= 0)
			QueueContainerForDestroy(value);
		else
			QueueContainerForCycleCheck(value);
	}
	return true;
}

int CNeoVMImpl::CollectCycles()
{
	// 다음 수집은 현재 VM의 모든 worker가 한 번씩 안전 지점에 도달한 뒤에 한다.
	// worker가 없는 직접 사용 VM은 0으로 설정되어 다음 안전 지점에서 바로 수집된다.
	_cycleSafePointRemainCount = (_sVMWorkers.size() > (size_t)INT_MAX) ? INT_MAX : (int)_sVMWorkers.size();
	_cycleLastCollectTime = std::chrono::steady_clock::now();

	if (_isTearingDown || _sCycleCandidates.empty())
		return 0;

	const size_t total = _sCycleCandidates.size();
	const size_t budget = std::max<size_t>(16, (total + 49) / 50); // max(16, 전체의 2%), 올림
	int processed = 0;
	while (_sCycleCandidates.empty() == false && (size_t)processed < budget)
	{
		CycleCandidate* ticket = _sCycleCandidates.front();
		_sCycleCandidates.pop_front();
		if (ticket->object != nullptr)
		{
			void* object = ticket->object;
			if (GetCycleQueuedFlag(ticket->type, object) && GetCycleTicket(ticket->type, object) == ticket)
			{
				if (CollectUnreachableCycleCandidate(ticket->type, object) == false)
				{
					CancelCycleCandidate(ticket->type, object);
				}
			}
		}
		delete ticket;
		++processed;
	}
	if (processed != 0)
		PublishAllocStats();
	return processed;
}

void CNeoVMImpl::OnVMSafePoint()
{
	if (_isTearingDown)
		return;

	// 후보가 없는 동안에도 worker 라운드는 진행하되, 0 아래로는 내리지 않는다.
	// 이후 후보가 생겼을 때 이미 라운드가 끝났다면 다음 안전 지점에서 바로 수집한다.
	if (_cycleSafePointRemainCount > 0)
		--_cycleSafePointRemainCount;

	if (_sCycleCandidates.empty())
		return;

	if (_cycleSafePointRemainCount <= 0)
	{
		CollectCycles();
		return;
	}

	// steady_clock 조회는 hot path에서 매번 하지 않는다. 남은 worker 수가 256의
	// 배수인 지점에서만 설정된 시간 fallback을 확인한다.
	constexpr int kTimeCheckRemainModulo = 256;
	if ((_cycleSafePointRemainCount % kTimeCheckRemainModulo) == 0
		&& std::chrono::steady_clock::now() - _cycleLastCollectTime >= std::chrono::milliseconds(_cycleCollectIntervalMs))
	{
		CollectCycles();
	}
}


FunctionPtr* CNeoVMImpl::FunctionPtrAlloc(FunctionPtr* pOld)
{
	auto it = m_sCache_FunPtr.find(pOld->_func);
	if (it != m_sCache_FunPtr.end())
		return (*it).second;

	FunctionPtr* pNew = new FunctionPtr();
	*pNew = *pOld;
	m_sCache_FunPtr[pNew->_func] = pNew;
	return pNew;
}


static void threadFunction(CNeoVMImpl* p) 
{
	p->ThreadFunction();
}
void CNeoVMImpl::ThreadFunction()
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

void CNeoVMImpl::AddHttp_Request(AsyncInfo* p)
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
void CNeoVMImpl::DiscardOrphanedAsync()
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
void CNeoVMImpl::DiscardAsyncByWorker(u32 workerId, int pendingCount)
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

AsyncInfo* CNeoVMImpl::Pop_AsyncInfo(CNeoVMWorker* pOwnerWorker)
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


CNeoVMImpl::CNeoVMImpl()
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
CNeoVMImpl::~CNeoVMImpl()
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
	// 후보 티켓은 객체를 소유하지 않는다. 실제 객체를 정리하기 전에 연결만 끊고
	// 티켓을 버린다. 그래야 뒤따르는 Free*가 이미 해제된 티켓을 건드리지 않는다.
	while (_sCycleCandidates.empty() == false)
	{
		CycleCandidate* ticket = _sCycleCandidates.front();
		_sCycleCandidates.pop_front();
		if (ticket->object != nullptr && GetCycleTicket(ticket->type, ticket->object) == ticket)
			CancelCycleCandidate(ticket->type, ticket->object);
		delete ticket;
	}
	while (_sVMWorkers.empty() == false)
		FreeWorker(_sVMWorkers.begin()->second);

	for (int i = 0; i < NDF_MAX; i++)
		Var_Release(&m_sDefaultValue[i]);

	// 살아남은 List/Map/Set 의 _Bucket 을 해제 (intrusive live 리스트 순회).
	// String 은 CNVMInstPool 소멸자가 std::str 을 정리하므로 별도 처리 없음.
	// Free* 는 먼저 파괴 표식을 세우고 live 리스트에서 뺀다. 내부 항목의
	// Var_Release 가 같은 객체로 되돌아와도 재진입하지 않는다.
	while (_sTableHead)
	{
		MapInfo* p = _sTableHead;
		FreeTable(p);
	}
	while (_sListHead)
	{
		ListInfo* p = _sListHead;
		FreeList(p);
	}
	while (_sSetHead)
	{
		SetInfo* p = _sSetHead;
		FreeSet(p);
	}

	// _isTearingDown guard 때문에 보통 비어 있어야 한다. 그래도 종료 경로가
	// 확장되더라도 heap 티켓만 남기지 않도록 마지막으로 포인터만 폐기한다.
	while (_sCycleCandidates.empty() == false)
	{
		CycleCandidate* ticket = _sCycleCandidates.front();
		_sCycleCandidates.pop_front();
		delete ticket;
	}

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

void CNeoVMImpl::SetError(const std::string& msg)
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
INeoVMWorker* CNeoVMImpl::LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker, bool init, int iStackSize)
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

INeoVMWorker* CNeoVMImpl::LoadVM(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, bool blMainWorker, bool init, int iStackSize)
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
bool CNeoVMImpl::PCall(int iModule)
{
	CNeoVMWorker* pFound = FindWorker(iModule);
	if (pFound == nullptr)
		return false;

	auto pWorker = pFound;
	std::vector<VarInfo> _args;
	int st = pWorker->ExecuteTop(0, _args);   // 모듈 본문(함수0)을 풀 컨텍스트로 최상위 실행
	return st != NEOEXEC_ERROR;
}

bool CNeoVMImpl::RunFunction(const std::string& funName)
{
	int iFID = _pMainWorker->FindFunction(funName);
	if (iFID == -1)
		return false;

	std::vector<VarInfo> _args;
	_pMainWorker->ExecuteTop(iFID, _args);
	return true;
}
u32 CNeoVMImpl::CreateWorker(int iStackSize)
{
	auto pWorker = WorkerAlloc(iStackSize);
	PublishAllocStats();
	return pWorker->GetWorkerID();
}
bool CNeoVMImpl::ReleaseWorker(u32 id)
{
	CNeoVMWorker* pFound = FindWorker(id);
	if (pFound == nullptr)
		return false;

	auto pWorker = pFound;
	FreeWorker(pWorker);

	if (pWorker == _pMainWorker)
		_pMainWorker = NULL;
	PublishAllocStats();
	return true;
}
bool CNeoVMImpl::BindWorkerFunction(u32 id, const std::string& funName)
{
	CNeoVMWorker* pFound = FindWorker(id);
	if (pFound == nullptr)
		return false;

	CNeoVMWorker* pWorker = pFound;
	return pWorker->BindWorkerFunction(funName);
}
bool CNeoVMImpl::SetTimeout(u32 id, int iTimeout, int iCheckOpCount)
{
	CNeoVMWorker* pWorker;
	if ((int)id == -1)
	{
		pWorker = (CNeoVMWorker*)_pMainWorker;
	}
	else
	{
		CNeoVMWorker* pFound = FindWorker(id);
		if (pFound == nullptr)
			return false;
		pWorker = pFound;
	}
	pWorker->SetTimeout(iTimeout, iCheckOpCount);
	return true;
}

bool CNeoVMImpl::IsWorking(u32 id)
{
	CNeoVMWorker* pFound = FindWorker(id);
	if (pFound == nullptr)
		return false;
	auto pWorker = pFound;
	return pWorker->IsWorking();
}

bool CNeoVMImpl::UpdateWorker(u32 id)
{
	if (_pErrorMsg.empty() == false)
		return false;

	CNeoVMWorker* pFound = FindWorker(id);
	if (pFound == nullptr)
		return false;
	auto pWorker = pFound;
	bool result = pWorker->Run();// iTimeout >= 0, iTimeout, iCheckOpCount);
	OnVMSafePoint();
	PublishAllocStats();
	return result;
}

};

