#include <math.h>
#include <stdlib.h>
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

long long CNeoVMImpl::PoolBytes() const
{
	return (long long)m_sPool_TableNode.ReservedBytes()
	     + (long long)m_sPool_TableInfo.ReservedBytes()
	     + (long long)m_sPool_SetNode.ReservedBytes()
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

void CNeoVMImpl::PublishAllocStats()
{
	m_sAllocStats.poolBytes = PoolBytes();
	m_sAllocStats.stringIdleBytes = StringIdleBytes();
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
	++m_sAllocStats.modules;

	_sVMWorkers[_dwLastIDVMWorker] = p;
	return p;
}
void CNeoVMImpl::FreeWorker(CNeoVMWorker *d)
{
	auto it = _sVMWorkers.find(d->GetWorkerID());
	if (it == _sVMWorkers.end())
		return;

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
	--m_sAllocStats.coroutines;
	CoroutineInfo* pCI = d->_cor;
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
	// String 은 CNVMInstPool(소멸자 지원)이라 종료 시 개별 정리가 불필요 → 레지스트리 없음
	StringInfo* p = m_sPool_String.Receive();// new StringInfo();
	p->_hash = 0;
	p->_container = nullptr;
	p->_containerVersion = 0;
	p->_refCount = 0;
	p->_value = nullptr;

	p->_str = str;
	p->_StringLen = utf_string::UTF8_LENGTH(str);

	++m_sAllocStats.strings;
	return p;
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
	--m_sAllocStats.strings;
	std::string& str = d->_str->_str;
	const size_t capa = str.capacity();
	if (capa >= kStringReleaseCapacity)
	{
		// shrink_to_fit 은 표준상 비구속 요청이라 확정적으로 놓아주려면 swap 을 쓴다.
		std::string().swap(str);
	}
	m_sPool_String.Confer(d->_str);
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
	pTable->_itemCount = 0;
	pTable->_mutationVersion = 0;
	pTable->_HashBase = 0;
	pTable->_BucketCapa = 0;
	pTable->_pUserData = NULL;
	pTable->_meta = NULL;
	pTable->_fun._func = NULL;
	pTable->_fun._property = NULL;

	LiveList_Insert(_sTableHead, pTable);
	if (cnt > 0) pTable->Reserve(cnt);
	++m_sAllocStats.maps;
	return pTable;
}
void CNeoVMImpl::FreeTable(MapInfo* tbl)
{
	LiveList_Remove(_sTableHead, tbl);

	if (tbl->_meta)
	{
		if (--tbl->_meta->_refCount <= 0)
		{
			FreeTable(tbl->_meta);
		}
		tbl->_meta = NULL;
	}
	tbl->_fun._func = NULL;
	tbl->_fun._property = NULL;

	tbl->Free();

	//delete tbl;
	m_sPool_TableInfo.Confer(tbl);
	--m_sAllocStats.maps;
}
ListInfo* CNeoVMImpl::ListAlloc(int cnt)
{
	ListInfo* pList = m_sPool_ListInfo.Receive();
	pList->_pVM = this;
	pList->_refCount = 0;
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
	pSet->_itemCount = 0;
	pSet->_mutationVersion = 0;
	pSet->_HashBase = 0;
	pSet->_BucketCapa = 0;
	pSet->_pUserData = NULL;
	pSet->_meta = NULL;
	pSet->_fun._func = NULL;
	pSet->_fun._property = NULL;

	LiveList_Insert(_sSetHead, pSet);
	++m_sAllocStats.sets;
	return pSet;
}
void CNeoVMImpl::FreeSet(SetInfo* set)
{
	LiveList_Remove(_sSetHead, set);
	if (set->_meta)
	{
		if (--set->_meta->_refCount <= 0)
		{
			FreeSet(set->_meta);
		}
		set->_meta = NULL;
	}
	set->_fun._func = NULL;
	set->_fun._property = NULL;

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
	--m_sAllocStats.asyncs;
	AsyncInfo* p = d->_async;
	// AsyncInfo 는 노드가 372B 로 가장 크고, 문자열 멤버 셋에 HTTP 본문까지 담긴다
	// (_resultValue 는 응답 전문이라 MB 단위도 가능). 문자열 풀과 같은 규칙으로 놓아준다.
	ReleaseIfLarge(p->_request);
	ReleaseIfLarge(p->_body);
	ReleaseIfLarge(p->_resultValue);
	// 헤더는 요청마다 새로 쌓이고 재사용 이득이 없다 — 배열째 돌려준다.
	std::vector< std::pair<std::string, std::string> >().swap(p->_headers);
	m_sPool_Async.Confer(p);
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
	_job_end = true;
	if(nullptr != _job)
	{
		_job->join();
		delete _job;
		_job = nullptr;
	}
	for(auto it = _sVMWorkers.begin(); it != _sVMWorkers.end(); it++)
	{
		CNeoVMWorker* d = (*it).second;
		--m_sAllocStats.modules;
		delete d;
	}
	_sVMWorkers.clear();

	for (int i = 0; i < NDF_MAX; i++)
		Var_Release(&m_sDefaultValue[i]);

	// 살아남은 List/Map/Set 의 _Bucket 을 해제 (intrusive live 리스트 순회).
	// String 은 CNVMInstPool 소멸자가 std::str 을 정리하므로 별도 처리 없음.
	// Free() 는 내부 항목을 Var_Release 하므로 중첩 컬렉션 해제 시 재귀로
	// LiveList_Remove 가 호출될 수 있다. 먼저 p 를 완전히 언링크한 뒤 Free 한다.
	while (_sTableHead)
	{
		MapInfo* p = _sTableHead;
		LiveList_Remove(_sTableHead, p);
		p->Free();
		--m_sAllocStats.maps;
	}
	while (_sListHead)
	{
		ListInfo* p = _sListHead;
		LiveList_Remove(_sListHead, p);
		p->Free();
		--m_sAllocStats.lists;
	}
	while (_sSetHead)
	{
		SetInfo* p = _sSetHead;
		LiveList_Remove(_sSetHead, p);
		p->Free();
		--m_sAllocStats.sets;
	}

	for (auto it = m_sCache_FunPtr.begin(); it != m_sCache_FunPtr.end(); it++)
		delete (*it).second;
	m_sCache_FunPtr.clear();
	PublishAllocStats();
	// 풀 페이지는 곧 멤버 소멸자가 free 한다. 지금 빼두지 않으면 이 VM 몫이 전역 집계에 영원히 남는다.
	PublishNeoVMAllocStatValue(g_iNeoVMPoolBytes, m_sPublishedAllocStats.poolBytes, 0);
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
	PublishAllocStats();
	return result;
}

};

