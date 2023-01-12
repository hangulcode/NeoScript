#include <math.h>
#include <stdlib.h>
#include "NeoVM.h"
#include "NeoArchive.h"

void CNeoVM::Var_AddRef(VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		++d->_str->_refCount;
		break;
	case VAR_TABLE:
		++d->_tbl->_refCount;
		break;
	case VAR_LIST:
		++d->_lst->_refCount;
		break;
	case VAR_SET:
		++d->_tbl->_refCount;
		break;
	case VAR_COROUTINE:
		++d->_cor->_refCount;
		break;
	default:
		break;
	}
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
void CNeoVM::Var_SetTable(VarInfo *d, TableInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_TABLE);
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
//	p->_refCount = 0;

	_sVMWorkers[_dwLastIDVMWorker] = p;
	return p;
}
void CNeoVM::FreeWorker(CNeoVMWorker *d)
{
	auto it = _sVMWorkers.find(d->GetWorkerID());
	if (it == _sVMWorkers.end())
		return;

	_sVMWorkers.erase(it);
	delete d;
}
CoroutineInfo* CNeoVM::CoroutineAlloc()
{
	CoroutineInfo* p = m_sPool_Coroutine.Receive();
	p->_pCodeCurrent = NULL;
	p->m_sCallStack.reserve(1000);
	p->m_sVarStack.resize(10000);
	return p;
}
void CNeoVM::FreeCoroutine(VarInfo *d)
{
	CoroutineInfo* p = d->_cor;
	m_sPool_Coroutine.Confer(p);
}

StringInfo* CNeoVM::StringAlloc(const std::string& str)
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

	_sStrings[m_sPool_String._dwLastID] = p;
	return p;
}
void CNeoVM::FreeString(VarInfo *d)
{
	auto it = _sStrings.find(d->_str->_StringID);
	if (it == _sStrings.end())
		return; // Error

	_sStrings.erase(it);
	//delete d->_str;
	m_sPool_String.Confer(d->_str);
}
TableInfo* CNeoVM::TableAlloc()
{
	TableInfo* pTable = m_sPool_TableInfo.Receive();
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

	_sTables[m_sPool_TableInfo._dwLastID] = pTable;
	return pTable;
}
void CNeoVM::FreeTable(TableInfo* tbl)
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

	tbl->Free();

	//delete tbl;
	m_sPool_TableInfo.Confer(tbl);
}
ListInfo* CNeoVM::ListAlloc()
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

	_sLists[m_sPool_ListInfo._dwLastID] = pList;
	return pList;
}
void CNeoVM::FreeList(ListInfo* lst)
{
	auto it = _sLists.find(lst->_ListID);
	if (it == _sLists.end())
		return; // Error

	_sLists.erase(it);
	lst->Free();

	//delete tbl;
	m_sPool_ListInfo.Confer(lst);
}
SetInfo* CNeoVM::SetAlloc()
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

	_sSets[m_sPool_SetInfo._dwLastID] = pSet;
	return pSet;
}
void CNeoVM::FreeSet(SetInfo* set)
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

	set->Free();

	//delete tbl;
	m_sPool_SetInfo.Confer(set);
}
int	 CNeoVM::Coroutine_Create(int iFID)
{
	return 0;
}
int	 CNeoVM::Coroutine_Resume(int iCID)
{
	return 0;
}
int	 CNeoVM::Coroutine_Destroy(int iCID)
{
	return 0;
}


CNeoVM::CNeoVM()
{
	for (int i = 0; i < NDF_MAX; i++)
	{
		m_sDefaultValue[i].ClearType();
		switch (i)
		{
		case NDF_NULL: Var_SetStringA(&m_sDefaultValue[i], "null"); break;
		case NDF_BOOL: Var_SetStringA(&m_sDefaultValue[i], "bool"); break;
		case NDF_INT: Var_SetStringA(&m_sDefaultValue[i], "int"); break;
		case NDF_FLOAT: Var_SetStringA(&m_sDefaultValue[i], "float"); break;
		case NDF_STRING: Var_SetStringA(&m_sDefaultValue[i], "string"); break;
		case NDF_TABLE: Var_SetStringA(&m_sDefaultValue[i], "map"); break;
		case NDF_LIST: Var_SetStringA(&m_sDefaultValue[i], "list"); break;
		case NDF_SET: Var_SetStringA(&m_sDefaultValue[i], "set"); break;
		case NDF_COROUTINE: Var_SetStringA(&m_sDefaultValue[i], "coroutine"); break;
		case NDF_FUNCTION: Var_SetStringA(&m_sDefaultValue[i], "function"); break;
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
CNeoVM::~CNeoVM()
{
	for(auto it = _sVMWorkers.begin(); it != _sVMWorkers.end(); it++)
	{
		CNeoVMWorker* d = (*it).second;
		delete d;
	}
	_sVMWorkers.clear();

	for (auto it = _sTables.begin(); it != _sTables.end(); it++)
	{
		TableInfo* p = (*it).second;
		p->Free();
	}
	_sTables.clear();

	_sStrings.clear();


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



CNeoVM* 	CNeoVM::CreateVM()
{
	return new CNeoVM();
}
void		CNeoVM::ReleaseVM(CNeoVM* pVM)
{
	delete pVM;
}

bool CNeoVM::LoadVM(void* pBuffer, int iSize, int iStackSize)
{
	if (_pMainWorker)
		return false;

	_pMainWorker = WorkerAlloc(iStackSize);
	if (false == _pMainWorker->Init(pBuffer, iSize, iStackSize))
	{
		FreeWorker(_pMainWorker);
		return false;
	}
	return true;
}

bool CNeoVM::RunFunction(const std::string& funName)
{
	int iFID = _pMainWorker->FindFunction(funName);
	if (iFID == -1)
		return false;

	std::vector<VarInfo> _args;
	_pMainWorker->Start(iFID, _args);
	return true;
}
u32 CNeoVM::CreateWorker(int iStackSize)
{
	auto pWorker = WorkerAlloc(iStackSize);
	return pWorker->GetWorkerID();
}
bool CNeoVM::ReleaseWorker(u32 id)
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
bool CNeoVM::BindWorkerFunction(u32 id, const std::string& funName)
{
	int iFID = _pMainWorker->FindFunction(funName);
	if (iFID == -1)
		return false;

	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;

	std::vector<VarInfo> _args;
	auto pWorker = (*it).second;
	return pWorker->Setup(iFID, _args);
}
bool CNeoVM::SetTimeout(u32 id, int iTimeout, int iCheckOpCount)
{
	CNeoVMWorker* pWorker;
	if (id == -1)
	{
		pWorker = _pMainWorker;
	}
	else
	{
		auto it = _sVMWorkers.find(id);
		if (it == _sVMWorkers.end())
			return false;
		pWorker = (*it).second;
	}
	pWorker->m_iTimeout = iTimeout;
	pWorker->m_iCheckOpCount = iCheckOpCount;
	return true;
}

bool CNeoVM::IsWorking(u32 id)
{
	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	return pWorker->_isSetup;
}

bool CNeoVM::UpdateWorker(u32 id)
{
	if (_pErrorMsg.empty() == false)
		return false;

	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	return pWorker->Run();// iTimeout >= 0, iTimeout, iCheckOpCount);
}
