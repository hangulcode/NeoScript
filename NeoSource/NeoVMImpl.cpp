#include <math.h>
#include <stdlib.h>
#include "NeoVMImpl.h"
#include "NeoArchive.h"
#include "UTFString.h"

#ifdef _WIN32
//https://github.com/elnormous/HTTPRequest
#include "HttpRequest.h"
#pragma comment(lib, "ws2_32.lib")
#endif


namespace NeoScript
{

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

	_sVMWorkers[_dwLastIDVMWorker] = p;
	return p;
}
void CNeoVMImpl::FreeWorker(CNeoVMWorker *d)
{
	auto it = _sVMWorkers.find(d->GetWorkerID());
	if (it == _sVMWorkers.end())
		return;

	_sVMWorkers.erase(it);
	delete d;
}
CNeoVMWorker* CNeoVMImpl::FindWorker(int iModule)
{
	auto it = _sVMWorkers.find(iModule);
	if (it == _sVMWorkers.end())
		return NULL;

	return (*it).second;
}

CoroutineInfo* CNeoVMImpl::CoroutineAlloc()
{
	CoroutineInfo* p = m_sPool_Coroutine.Receive();
	p->_info._pCodeCurrent = NULL;
	p->m_sCallStack.reserve(1000);
	p->m_sVarStack.resize(10000);
	return p;
}
void CNeoVMImpl::FreeCoroutine(VarInfo *d)
{
	CoroutineInfo* p = d->_cor;
	m_sPool_Coroutine.Confer(p);
}

StringInfo* CNeoVMImpl::StringAlloc(const std::string& str)
{
	StringInfo* p = m_sPool_String.Receive();// new StringInfo();
	while (true)
	{
		if (++m_sPool_String._dwLastID == 0)
			m_sPool_String._dwLastID = 1;

		if (_sStrings.end() == _sStrings.find(m_sPool_String._dwLastID))
		{
			break;
		}
	}

	p->_StringID = m_sPool_String._dwLastID;
	p->_refCount = 0;

	p->_str = str;
	p->_StringLen = utf_string::UTF8_LENGTH(str);

	_sStrings[m_sPool_String._dwLastID] = p;
	return p;
}
void CNeoVMImpl::FreeString(VarInfo *d)
{
	auto it = _sStrings.find(d->_str->_StringID);
	if (it == _sStrings.end())
		return; // Error

	_sStrings.erase(it);
	//delete d->_str;
	m_sPool_String.Confer(d->_str);
}
MapInfo* CNeoVMImpl::TableAlloc(int cnt)
{
	MapInfo* pTable = m_sPool_TableInfo.Receive();
	while (true)
	{
		if (++m_sPool_TableInfo._dwLastID == 0)
			m_sPool_TableInfo._dwLastID = 1;

		if (_sTables.end() == _sTables.find(m_sPool_TableInfo._dwLastID))
		{
			break;
		}
	}
	pTable->_pVM = this;
	pTable->_TableID = m_sPool_TableInfo._dwLastID;
	pTable->_refCount = 0;
	pTable->_itemCount = 0;
	pTable->_HashBase = 0;
	pTable->_BucketCapa = 0;
	pTable->_pUserData = NULL;
	pTable->_meta = NULL;
	pTable->_fun._func = NULL;
	pTable->_fun._property = NULL;

	_sTables[m_sPool_TableInfo._dwLastID] = pTable;
	if (cnt > 0) pTable->Reserve(cnt);
	return pTable;
}
void CNeoVMImpl::FreeTable(MapInfo* tbl)
{
	auto it = _sTables.find(tbl->_TableID);
	if (it == _sTables.end())
		return; // Error

	_sTables.erase(it);

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
}
ListInfo* CNeoVMImpl::ListAlloc(int cnt)
{
	ListInfo* pList = m_sPool_ListInfo.Receive();
	while (true)
	{
		if (++m_sPool_ListInfo._dwLastID == 0)
			m_sPool_ListInfo._dwLastID = 1;

		if (_sLists.end() == _sLists.find(m_sPool_ListInfo._dwLastID))
		{
			break;
		}
	}
	pList->_pVM = this;
	pList->_ListID = m_sPool_ListInfo._dwLastID;
	pList->_refCount = 0;
	pList->_itemCount = 0;
	pList->_BucketCapa = 0;
	pList->_pUserData = NULL;
	pList->_pIndexer = nullptr;

	_sLists[m_sPool_ListInfo._dwLastID] = pList;
	if (cnt > 0) pList->Resize(cnt);
	return pList;
}
void CNeoVMImpl::FreeList(ListInfo* lst)
{
	auto it = _sLists.find(lst->_ListID);
	if (it == _sLists.end())
		return; // Error

	_sLists.erase(it);
	lst->Free();

	//delete tbl;
	m_sPool_ListInfo.Confer(lst);
}
SetInfo* CNeoVMImpl::SetAlloc()
{
	SetInfo* pSet = m_sPool_SetInfo.Receive();
	while (true)
	{
		if (++m_sPool_SetInfo._dwLastID == 0)
			m_sPool_SetInfo._dwLastID = 1;

		if (_sSets.end() == _sSets.find(m_sPool_SetInfo._dwLastID))
		{
			break;
		}
	}
	pSet->_pVM = this;
	pSet->_SetID = m_sPool_SetInfo._dwLastID;
	pSet->_refCount = 0;
	pSet->_itemCount = 0;
	pSet->_HashBase = 0;
	pSet->_BucketCapa = 0;
	pSet->_pUserData = NULL;
	pSet->_meta = NULL;
	pSet->_fun._func = NULL;
	pSet->_fun._property = NULL;

	_sSets[m_sPool_SetInfo._dwLastID] = pSet;
	return pSet;
}
void CNeoVMImpl::FreeSet(SetInfo* set)
{
	auto it = _sSets.find(set->_SetID);
	if (it == _sSets.end())
		return; // Error

	_sSets.erase(it);
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
}
AsyncInfo* CNeoVMImpl::AsyncAlloc()
{
	AsyncInfo* p = m_sPool_Async.Receive();
	p->_refCount = 0;
	p->_state = ASYNC_READY;
	return p;
}
void CNeoVMImpl::FreeAsync(VarInfo* d)
{
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
	InitLib();
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
		delete d;
	}
	_sVMWorkers.clear();

	for (auto it = _sTables.begin(); it != _sTables.end(); it++)
	{
		MapInfo* p = (*it).second;
		p->Free();
	}
	_sTables.clear();

	_sStrings.clear();

	for (auto it = m_sCache_FunPtr.begin(); it != m_sCache_FunPtr.end(); it++)
		delete (*it).second;
	m_sCache_FunPtr.clear();
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

INeoVMWorker* CNeoVMImpl::LoadVM(void* pBuffer, int iSize, bool blMainWorker, int iStackSize)
{
	CNeoVMWorker*pWorker = WorkerAlloc(iStackSize);
	if (false == pWorker->Init(pBuffer, iSize, iStackSize))
	{
		FreeWorker(pWorker);
		return NULL;
	}
	if (blMainWorker && NULL == _pMainWorker)
		_pMainWorker = pWorker;
	return pWorker;
}
bool CNeoVMImpl::PCall(int iModule)
{
	auto it = _sVMWorkers.find(iModule);
	if (it == _sVMWorkers.end())
		return false;

	auto pWorker = (*it).second;
	std::vector<VarInfo> _args;
	pWorker->Initialize(0, _args);
	return true;
}

bool CNeoVMImpl::RunFunction(const std::string& funName)
{
	int iFID = _pMainWorker->FindFunction(funName);
	if (iFID == -1)
		return false;

	std::vector<VarInfo> _args;
	_pMainWorker->Start(iFID, _args);
	return true;
}
u32 CNeoVMImpl::CreateWorker(int iStackSize)
{
	auto pWorker = WorkerAlloc(iStackSize);
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
	return pWorker->Run();// iTimeout >= 0, iTimeout, iCheckOpCount);
}

};
