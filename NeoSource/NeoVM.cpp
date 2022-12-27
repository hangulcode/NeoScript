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
		if (++_dwLastIDString == 0)
			_dwLastIDString = 1;

		if (_sStrings.end() == _sStrings.find(_dwLastIDString))
		{
			break;
		}
	}

	p->_StringID = _dwLastIDString;
	p->_refCount = 0;

	p->_str = str;

	_sStrings[_dwLastIDString] = p;
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
	//TableInfo* pTable = new TableInfo();
	while (true)
	{
		if (++_dwLastIDTable == 0)
			_dwLastIDTable = 1;

		if (_sTables.end() == _sTables.find(_dwLastIDTable))
		{
			break;
		}
	}
	pTable->_pVM = this;
	pTable->_TableID = _dwLastIDTable;
	pTable->_refCount = 0;
	pTable->_itemCount = 0;
	pTable->_HashBase = 0;
	pTable->_BucketCapa = 0;
	pTable->_pUserData = NULL;
	pTable->_meta = NULL;
	pTable->_fun._func = NULL;

	_sTables[_dwLastIDTable] = pTable;
	return pTable;
}
void CNeoVM::FreeTable(TableInfo* tbl)
{
	auto it = _sTables.find(tbl->_TableID);
	if (it == _sTables.end())
		return; // Error

	_sTables.erase(it);

	//for (auto it2 = tbl->_intMap.begin(); it2 != tbl->_intMap.end(); it2++)
	//{
	//	VarInfo& v = (*it2).second;
	//	if(v.IsAllocType())
	//		Var_Release(&v);
	//}
	//tbl->_intMap.clear();

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

void CNeoVM::FreeTable(VarInfo *d)
{
	FreeTable(d->_tbl);
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
	_pCodePtr = NULL;
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
		case NDF_TABLE: Var_SetStringA(&m_sDefaultValue[i], "table"); break;
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


	if (_pCodePtr != NULL)
	{
		delete [] _pCodePtr;
		_pCodePtr = NULL;
	}
}

void CNeoVM::SetError(const char* pErrMsg)
{
	_pErrorMsg = pErrMsg;
	_bError = (pErrMsg != NULL);
}



static void ReadString(CNArchive& ar, std::string& str)
{
	short nLen;
	ar >> nLen;
	str.resize(nLen);

	ar.Read((char*)str.data(), nLen);
}

void		CNeoVM::ReleaseVM(CNeoVM* pVM)
{
	delete pVM;
}
bool CNeoVM::Init(void* pBuffer, int iSize, int iStackSize)
{
	_BytesSize = iSize;
	CNArchive ar(pBuffer, iSize);
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));
	ar >> header;
	_header = header;

	if (header._dwFileType != FILE_NEOS)
	{
		return false;
	}
	if (header._dwNeoVersion != NEO_VER)
	{
		return false;
	}


	u8* pCode = new u8[header._iCodeSize];
	SetCodeData(pCode, header._iCodeSize);
	ar.Read(pCode, header._iCodeSize);

	m_sFunctionPtr.resize(header._iFunctionCount);

	int iID;
	SFunctionTable fun;
	std::string funName;
	for (int i = 0; i < header._iFunctionCount; i++)
	{
		memset(&fun, 0, sizeof(SFunctionTable));

		ar >> iID >> fun._codePtr >> fun._argsCount >> fun._localTempMax >> fun._localVarCount >> fun._funType;
		if (fun._funType != FUNT_NORMAL && fun._funType != FUNT_ANONYMOUS)
		{
			ReadString(ar, funName);
			m_sImExportTable[funName] = iID;
		}

		fun._localAddCount = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;
		m_sFunctionPtr[iID] = fun;
	}

	std::string tempStr;
	int iMaxVar = header._iStaticVarCount + header._iGlobalVarCount;
	m_sVarGlobal.resize(iMaxVar);
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = m_sVarGlobal[i];
		Var_Release(&vi);

		VAR_TYPE type;
		ar >> type;
		vi.SetType(type);
		switch (type)
		{
		case VAR_INT:
			ar >> vi._int;
			break;
		case VAR_FLOAT:
			ar >> vi._float;
			break;
		case VAR_BOOL:
			ar >> vi._bl;
			break;
		case VAR_STRING:
			ReadString(ar, tempStr);
			vi._str = StringAlloc(tempStr);
			vi._str->_refCount = 1;
			break;
		case VAR_FUN:
			ar >> vi._fun_index;
			break;
		default:
			SetError("Error Invalid VAR Type");
			return false;
		}
	}
	for (int i = header._iStaticVarCount; i < iMaxVar; i++)
	{
		m_sVarGlobal[i].ClearType();
	}

	if (header.m_iDebugCount > 0)
	{
		_DebugData.resize(header.m_iDebugCount);
		ar.Read(&_DebugData[0], sizeof(debug_info) * header.m_iDebugCount);
	}


	_pMainWorker = WorkerAlloc(iStackSize);

	InitLib();
	std::vector<VarInfo> _args;
	_pMainWorker->Start(0, _args);
	return true;
}
CNeoVM* CNeoVM::LoadVM(void* pBuffer, int iSize, int iStackSize)
{
	CNeoVM* pVM = new CNeoVM();
	if (false == pVM->Init(pBuffer, iSize, iStackSize))
	{
		delete pVM;
		return NULL;
	}

	return pVM;
}



bool CNeoVM::RunFunction(const std::string& funName)
{
	auto it = m_sImExportTable.find(funName);
	if (it == m_sImExportTable.end())
		return false;

	std::vector<VarInfo> _args;
	int iID = (*it).second;
	_pMainWorker->Start(iID, _args);

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
	auto it = m_sImExportTable.find(funName);
	if (it == m_sImExportTable.end())
		return false;

	auto it2 = _sVMWorkers.find(id);
	if (it2 == _sVMWorkers.end())
		return false;

	std::vector<VarInfo> _args;
	auto pWorker = (*it2).second;
	return pWorker->Setup((*it).second, _args);
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
	if (NULL != _pErrorMsg)
		return false;

	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	return pWorker->Run();// iTimeout >= 0, iTimeout, iCheckOpCount);
}
