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

static void PublishNeoVMAllocStatValue(std::atomic<int>& target, int& published, int current)
{
	int delta = current - published;
	if (delta != 0)
	{
		target.fetch_add(delta, std::memory_order_relaxed);
		published = current;
	}
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
}

bool GetNeoVMAllocStats(INeoVM* pVM, SNeoVMAllocStats& outStats)
{
	if (pVM == nullptr)
		return false;

	((CNeoVMImpl*)pVM)->GetAllocStats(outStats);
	return true;
}

void CNeoVMImpl::PublishAllocStats()
{
	PublishNeoVMAllocStatValue(g_iNeoVMAllocStrings, m_sPublishedAllocStats.strings, m_sAllocStats.strings);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocMaps, m_sPublishedAllocStats.maps, m_sAllocStats.maps);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocLists, m_sPublishedAllocStats.lists, m_sAllocStats.lists);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocSets, m_sPublishedAllocStats.sets, m_sAllocStats.sets);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocCoroutines, m_sPublishedAllocStats.coroutines, m_sAllocStats.coroutines);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocModules, m_sPublishedAllocStats.modules, m_sAllocStats.modules);
	PublishNeoVMAllocStatValue(g_iNeoVMAllocAsyncs, m_sPublishedAllocStats.asyncs, m_sAllocStats.asyncs);
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
bool CNeoVMImpl::SetFunction(int iFID, FunctionPtr& fun, int argCount) { return GetMainWorker()->SetFunction(iFID, fun, argCount); }


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
	_pExecPool->Release(d->_cor);
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
void CNeoVMImpl::FreeString(VarInfo *d)
{
	--m_sAllocStats.strings;
	m_sPool_String.Confer(d->_str);
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
AsyncInfo* CNeoVMImpl::AsyncAlloc()
{
	AsyncInfo* p = m_sPool_Async.Receive();
	p->_refCount = 0;
	p->_state = ASYNC_READY;
	++m_sAllocStats.asyncs;
	return p;
}
void CNeoVMImpl::FreeAsync(VarInfo* d)
{
	--m_sAllocStats.asyncs;
	m_sPool_Async.Confer(d->_async);
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

AsyncInfo* CNeoVMImpl::Pop_AsyncInfo()
{
	AsyncInfo* p = nullptr;
	if(_job_completed.TryPop(p))
		return p;
	return nullptr;
}


CNeoVMImpl::CNeoVMImpl()
{
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
		default:
			SetError("unknown Default Value");
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

INeoVMWorker* CNeoVMImpl::LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker, bool init, int iStackSize)
{
	if (vparam != nullptr && vparam->execPool != nullptr)
		_pExecPool = vparam->execPool;   // 이후 모듈 로드 워커들이 상속할 수 있도록 VM 에 보관
	if (_pExecPool == nullptr)
	{
		SetError("NeoExecContextPool is required.");
		return NULL;
	}

	CNeoVMWorker*pWorker = WorkerAlloc(iStackSize);
	if (false == pWorker->Init(vparam, pBuffer, iSize, iStackSize))
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
	auto it = _sVMWorkers.find(iModule);
	if (it == _sVMWorkers.end())
		return false;

	auto pWorker = (*it).second;
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
	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;

	auto pWorker = (*it).second;
	FreeWorker(pWorker);

	if (pWorker == _pMainWorker)
		_pMainWorker = NULL;
	PublishAllocStats();
	return true;
}
bool CNeoVMImpl::BindWorkerFunction(u32 id, const std::string& funName)
{
	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;

	CNeoVMWorker* pWorker = (*it).second;
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
		auto it = _sVMWorkers.find(id);
		if (it == _sVMWorkers.end())
			return false;
		pWorker = (*it).second;
	}
	pWorker->SetTimeout(iTimeout, iCheckOpCount);
	return true;
}

bool CNeoVMImpl::IsWorking(u32 id)
{
	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	return pWorker->IsWorking();
}

bool CNeoVMImpl::UpdateWorker(u32 id)
{
	if (_pErrorMsg.empty() == false)
		return false;

	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	bool result = pWorker->Run();// iTimeout >= 0, iTimeout, iCheckOpCount);
	PublishAllocStats();
	return result;
}

};

