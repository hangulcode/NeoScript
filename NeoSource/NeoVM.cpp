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
	default:
		break;
	}
}
void CNeoVM::Var_Release(VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			FreeString(d);
		d->_str = NULL;
		break;
	case VAR_TABLE:
		if (--d->_tbl->_refCount <= 0)
			FreeTable(d);
		d->_tbl = NULL;
		break;
	default:
		break;
	}
	d->ClearType();
}

void CNeoVM::Var_SetString(VarInfo *d, const char* str)
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

StringInfo* CNeoVM::StringAlloc(const char* str)
{
	StringInfo* p = new StringInfo();
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
	delete d->_str;
}
TableInfo* CNeoVM::TableAlloc()
{
	TableInfo* pTable = new TableInfo();
	while (true)
	{
		if (++_dwLastIDTable == 0)
			_dwLastIDTable = 1;

		if (_sTables.end() == _sTables.find(_dwLastIDTable))
		{
			break;
		}
	}
	pTable->_TableID = _dwLastIDTable;
	pTable->_refCount = 0;
	pTable->_pUserData = NULL;

	_sTables[_dwLastIDTable] = pTable;
	return pTable;
}
void CNeoVM::FreeTable(VarInfo *d)
{
	TableInfo*	tbl = d->_tbl;
	auto it = _sTables.find(tbl->_TableID);
	if (it == _sTables.end())
		return; // Error

	_sTables.erase(it);

	for (auto it2 = tbl->_intMap.begin(); it2 != tbl->_intMap.end(); it2++)
	{
		VarInfo& v = (*it2).second;
		if(v.IsAllocType())
			Var_Release(&v);
	}
	tbl->_intMap.clear();

	for (auto it2 = tbl->_strMap.begin(); it2 != tbl->_strMap.end(); it2++)
	{
		VarInfo& v = (*it2).second;
		if (v.IsAllocType())
			Var_Release(&v);
	}
	tbl->_strMap.clear();

	delete d->_tbl;
}


CNeoVM::CNeoVM()
{
}
CNeoVM::~CNeoVM()
{
	if (_pCodePtr != NULL)
	{
		delete [] _pCodePtr;
		_pCodePtr = NULL;
	}
}

void CNeoVM::SetError(const char* pErrMsg)
{
	_pErrorMsg = pErrMsg;
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
		if (fun._funType != FUNT_NORMAL)
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
			vi._str = StringAlloc(tempStr.c_str());
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

	_pMainWorker = WorkerAlloc(iStackSize);

	InitLib();
	_pMainWorker->Start(0);
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

	int iID = (*it).second;
	_pMainWorker->Start(iID);

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

	auto pWorker = (*it2).second;
	return pWorker->Setup((*it).second);
}
bool CNeoVM::IsWorking(u32 id)
{
	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	return pWorker->_isSetup;
}

bool CNeoVM::UpdateWorker(u32 id, int iTimeout, int iCheckOpCount)
{
	if (NULL != _pErrorMsg)
		return false;

	auto it = _sVMWorkers.find(id);
	if (it == _sVMWorkers.end())
		return false;
	auto pWorker = (*it).second;
	return pWorker->Run(iTimeout >= 0, iTimeout, iCheckOpCount);
}
