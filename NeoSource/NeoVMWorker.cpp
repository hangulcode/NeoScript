#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <thread>
#include <chrono>
#include "NeoVMImpl.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"
#include "NeoLibDCall.h"
#include "UTFString.h"

namespace NeoScript
{

void	SetCompileError(const char*	lpszString, ...);

void SVarWrapper::SetNone() { _vmw->Var_SetNone(_var); }
void SVarWrapper::SetInt(int v) { _vmw->Var_SetInt(_var, v); }
void SVarWrapper::SetFloat(NS_FLOAT v) { _vmw->Var_SetFloat(_var, v); }
void SVarWrapper::SetBool(bool v) { _vmw->Var_SetBool(_var, v); }
void SVarWrapper::SetString(const char* str) { _vmw->Var_SetString(_var, str); }
//void SVarWrapper::SetTableFun(FunctionPtrNative fun) { _vmw->Var_SetTableFun(_var, fun); }

static std::string g_meta_Add3 = "+";
static std::string g_meta_Sub3 = "-";
static std::string g_meta_Mul3 = "*";
static std::string g_meta_Div3 = "/";
static std::string g_meta_Per3 = "%";

static std::string g_meta_Add2 = "+=";
static std::string g_meta_Sub2 = "-=";
static std::string g_meta_Mul2 = "*=";
static std::string g_meta_Div2 = "/=";
static std::string g_meta_Per2 = "%=";

#include "NeoVMWorker.inl"


CNeoVMWorker::CNeoVMWorker(INeoVM* pVM, u32 id, int iStackSize)
{
	_pVM = pVM;
	_idWorker = id;
	//SetCodeData(pVM->_pCodePtr, pVM->_iCodeLen);
	m_pVarGlobal = &m_sVarGlobal;
	m_pVarGlobal_Pointer = nullptr;

	m_pCur = &m_sDefault;
	m_pCur->_state = COROUTINE_STATE_RUNNING;
	m_pVarStack_Base = &m_pCur->m_sVarStack;
	m_pCallStack = &m_pCur->m_sCallStack;

	ClearSP();

	//_intA1.SetType(VAR_INT);
	//_intA2.SetType(VAR_INT);
	//_intA3.SetType(VAR_INT);
	_funA3.SetType(VAR_FUN);

	m_pVarStack_Base->resize(iStackSize);
	m_pCallStack->reserve(1000);
	m_pVarStack_Pointer = &(*m_pVarStack_Base)[0];
}
CNeoVMWorker::~CNeoVMWorker()
{
	for (int i = 0; i < (int)m_sVarGlobal.size(); i++)
		Var_Release(&m_sVarGlobal[i]);
	m_sVarGlobal.clear();
	m_pVarGlobal_Pointer = nullptr;

	if (_pCodeBegin != NULL)
	{
		delete[] _pCodeBegin;
		_pCodeBegin = NULL;
	}
}




// -1 : Error
//  0 : SlieceRun
//  1 : Continue
//  2 : Timeout - 
int CNeoVMWorker::Sleep(int iTimeout, VarInfo* v1)
{
	int iSleepTick;
	switch (v1->GetType())
	{
	case VAR_INT:
		iSleepTick = v1->_int;
		break;
	case VAR_FLOAT:
		iSleepTick = (int)v1->_float;
		break;
	default:
		SetError("Sleep Value Error");
		return -1;
	}

	if (iTimeout >= 0)
	{
		_iRemainSleep = iSleepTick;
		return 0;
	}
	else
	{
		if (iTimeout >= 0)
		{
			auto cur = clock();
			auto delta = cur - _preClock;
			if (iTimeout > delta)
			{
				auto sleepAble = iTimeout - delta;
				if (iSleepTick > sleepAble)
					iSleepTick = sleepAble;
				std::this_thread::sleep_for(std::chrono::milliseconds(iSleepTick));
				return 1;
			}
			return 2;
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(iSleepTick));
			return 1;
		}
		return 1;
	}
	return 1;
}


std::string CNeoVMWorker::ToString(VarInfo* v1)
{
	char ch[256];
	switch (v1->GetType())
	{
	case VAR_NONE:
		return "null";
	case VAR_INT:
#ifdef _WIN32
		sprintf_s(ch, _countof(ch), "%d", v1->_int);
#else
		sprintf(ch, "%d", v1->_int);
#endif
		return ch;
	case VAR_FLOAT:
#ifdef _WIN32
		sprintf_s(ch, _countof(ch), "%lf", v1->_float);
#else
		sprintf(ch, "%lf", v1->_float);
#endif
		return ch;
	case VAR_BOOL:
		return v1->_bl ? "true" : "false";
	case VAR_CHAR:
		return std::string(v1->_c.c);
	case VAR_STRING:
		return v1->_str->_str;
	case VAR_MAP:
		return "map";
	case VAR_LIST:
		return "list";
	case VAR_SET:
		return "set";
	case VAR_COROUTINE:
		return "coroutine";
	case VAR_MODULE:
		return "module";
	case VAR_ASYNC:
		return "async";
	default:
		break;
	}
	return "null";
}
int CNeoVMWorker::ToInt(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return -1;
	case VAR_INT:
		return v1->_int;
	case VAR_FLOAT:
		return (int)v1->_float;
	case VAR_BOOL:
		return v1->_bl ? 1 : 0;
	case VAR_CHAR:
		return ::atoi(v1->_c.c);
	case VAR_STRING:
		return ::atoi(v1->_str->_str.c_str());
	case VAR_MAP:
		return -1;
	default:
		break;
	}
	return -1;
}
NS_FLOAT CNeoVMWorker::ToFloat(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return -1;
	case VAR_INT:
		return (NS_FLOAT)v1->_int;
	case VAR_FLOAT:
		return v1->_float;
	case VAR_BOOL:
		return v1->_bl ? (NS_FLOAT)1 : (NS_FLOAT)0;
	case VAR_CHAR:
		return (NS_FLOAT)atof(v1->_c.c);
	case VAR_STRING:
		return (NS_FLOAT)atof(v1->_str->_str.c_str());
	case VAR_MAP:
		return -1;
	default:
		break;
	}
	return -1;
}
int CNeoVMWorker::ToSize(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return 0;
	case VAR_INT:
		return 0;
	case VAR_FLOAT:
		return 0;
	case VAR_BOOL:
		return 0;
	case VAR_CHAR:
		return (v1->_c.c[0] == 0) ? 0 : 1;
	case VAR_STRING:
		return (int)v1->_str->_str.length();
	case VAR_MAP:
		return (int)v1->_tbl->_itemCount;
	default:
		break;
	}
	return 0;
}
VarInfo* CNeoVMWorker::GetType(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return &GetVM()->m_sDefaultValue[NDF_NULL];
	case VAR_INT:
		return &GetVM()->m_sDefaultValue[NDF_INT];
	case VAR_FLOAT:
		return &GetVM()->m_sDefaultValue[NDF_FLOAT];
	case VAR_BOOL:
		return &GetVM()->m_sDefaultValue[NDF_BOOL];
	case VAR_FUN:
		return &GetVM()->m_sDefaultValue[NDF_FUNCTION];
	case VAR_ITERATOR:
		break;
	case VAR_FUN_NATIVE:
		return &GetVM()->m_sDefaultValue[NDF_FUNCTION];
	case VAR_CHAR:
		return &GetVM()->m_sDefaultValue[NDF_STRING];
	case VAR_STRING:
		return &GetVM()->m_sDefaultValue[NDF_STRING];
	case VAR_MAP:
		return &GetVM()->m_sDefaultValue[NDF_TABLE];
	case VAR_LIST:
		return &GetVM()->m_sDefaultValue[NDF_LIST];
	case VAR_SET:
		return &GetVM()->m_sDefaultValue[NDF_SET];
	case VAR_COROUTINE:
		return &GetVM()->m_sDefaultValue[NDF_COROUTINE];
	case VAR_MODULE:
		return &GetVM()->m_sDefaultValue[NDF_MODULE];
	case VAR_ASYNC:
		return &GetVM()->m_sDefaultValue[NDF_ASYNC];
	default:
		break;
	}
	return &GetVM()->m_sDefaultValue[NDF_NULL];
}

void CNeoVMWorker::Call(FunctionPtr* fun, int n2, VarInfo* pReturnValue)
{
	if (_iSP_VarsMax + n2 >= (int)m_pVarStack_Base->size())
	{
		SetError("Call Stack Overflow");
		return;
	}

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);

	if (fun->_func == NULL)
	{	// Error
		SetError("Ptr Call is null");
		return;
	}
	int op = GetCodeptr();
	int iSave_SP_Vars = _iSP_Vars;
	int iSave_SP_VarsMax = _iSP_VarsMax;

	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);

	if ((*fun->_fn)(this, fun, n2) < 0)
	{
		SetError("Ptr Call Argument Count Error");
		return;
	}
	_iSP_Vars = iSave_SP_Vars;
	SetStackPointer(_iSP_Vars);
	SetCodePtr(op);
	_iSP_VarsMax = iSave_SP_VarsMax;
}

void CNeoVMWorker::Call(int n1, int n2, VarInfo* pReturnValue)
{
	SFunctionTable& fun = m_sFunctionPtr[n1];
	// n2 is Arg Count not use
#if _DEBUG
	SCallStack callStack;
	callStack._iReturnOffset = GetCodeptr();
	callStack._iSP_Vars = _iSP_Vars;
	callStack._iSP_VarsMax = _iSP_VarsMax;
	callStack._pReturnValue = pReturnValue;
	m_pCallStack->push_back(callStack);
#else
	SCallStack& callStack = m_pCallStack->push_back();
	callStack._iReturnOffset = GetCodeptr();
	callStack._iSP_Vars = _iSP_Vars;
	callStack._iSP_VarsMax = _iSP_VarsMax;
	callStack._pReturnValue = pReturnValue;
#endif

	SetCodePtr(fun._codePtr);
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
	if (_iSP_Vars_Max2 < _iSP_VarsMax)
		_iSP_Vars_Max2 = _iSP_VarsMax;

	if (_iSP_VarsMax >= (int)m_pVarStack_Base->size())
	{
		SetCodePtr(callStack._iReturnOffset); // Error Line Finder
		SetError("Call Stack Overflow");
		return;
	}
}

bool CNeoVMWorker::Call_MetaTable(VarInfo* pTable, std::string& funName, VarInfo* r, VarInfo* a, VarInfo* b)
{
	if (pTable->_tbl->_meta == NULL)
		return false;
	VarInfo* pVarItem = pTable->_tbl->_meta->Find(funName);
	if (pVarItem == NULL)
		return false;

	int n3 = 2;
	if (_iSP_VarsMax + n3 >= (int)m_pVarStack_Base->size())
	{
		SetError("Call Stack Overflow");
		return false;
	}

	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 1], a);
	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 2], b);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

	if (pVarItem->GetType() == VAR_FUN)
		Call(pVarItem->_fun_index, n3, r);

	//Move(r, &(*m_pVarStack)[iSP_VarsMax]); ???
	return true;
}


bool CNeoVMWorker::Call_MetaTable2(VarInfo* pTable, std::string& funName, VarInfo* r, VarInfo* b)
{
	if (pTable->_tbl->_meta == NULL)
		return false;
	VarInfo* pVarItem = pTable->_tbl->_meta->Find(funName);
	if (pVarItem == NULL)
		return false;

	int n3 = 2;
	if (_iSP_VarsMax + n3 >= (int)m_pVarStack_Base->size())
	{
		SetError("Call Stack Overflow");
		return false;
	}

	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 1], r);
	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 2], b);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

	if (pVarItem->GetType() == VAR_FUN)
		Call(pVarItem->_fun_index, n3, NULL);

	return true;
}


static void ReadString(CNArchive& ar, std::string& str)
{
	short nLen;
	ar >> nLen;
	str.resize(nLen);

	ar.Read((char*)str.data(), nLen);
}
bool CNeoVMWorker::Init(void* pBuffer, int iSize, int iStackSize)
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
	std::string Name;
	for (int i = 0; i < header._iFunctionCount; i++)
	{
		memset(&fun, 0, sizeof(SFunctionTable));

		ar >> iID >> fun._codePtr >> fun._argsCount >> fun._localTempMax >> fun._localVarCount >> fun._funType;
		if (fun._funType != FUNT_NORMAL && fun._funType != FUNT_ANONYMOUS)
		{
			ReadString(ar, Name);
			m_sImExportTable[Name] = iID;
		}

		fun._localAddCount = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;
		m_sFunctionPtr[iID] = fun;
	}

	for (int i = 0; i < header._iExportVarCount; i++)
	{
		int idx;
		ar >> idx;
		ReadString(ar, Name);
		m_sImportVars[Name] = idx;
	}

	std::string tempStr;
	int iMaxVar = header._iStaticVarCount + header._iGlobalVarCount;
	m_sVarGlobal.resize(iMaxVar);
	if(m_sVarGlobal.empty()) m_pVarGlobal_Pointer = nullptr;
	else m_pVarGlobal_Pointer = &m_sVarGlobal[0];

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
			vi._str = GetVM()->StringAlloc(tempStr);
			vi._str->_refCount = 1;
			break;
		//case VAR_FUN:
		//	ar >> vi._fun_index;
		//	break;
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
	return true;
}
int CNeoVMWorker::GetDebugLine(int iOPIndex)
{
//	int idx = int((u8*)_pCodeCurrent - _pCodeBegin - 1) / sizeof(SVMOperation);
	if ((int)_DebugData.size() <= iOPIndex || iOPIndex < 0) return -1;
	return _DebugData[iOPIndex]._lineseq;
}
void CNeoVMWorker::SetError(const char* pErrMsg)
{
	if(_isErrorOPIndex == 0)
		_isErrorOPIndex = int((u8*)_pCodeCurrent - _pCodeBegin - 1) / sizeof(SVMOperation);
	SetCodePtr(0);
	GetVM()->SetError(std::string(pErrMsg));
}
void CNeoVMWorker::SetErrorUnsupport(const char* pErrMsg, VarInfo* p)
{
	char buff[1024];
#ifdef _WIN32
	sprintf_s(buff, _countof(buff), pErrMsg, GetDataType(p->GetType()).c_str());
#else
	sprintf(buff, pErrMsg, GetDataType(p->GetType()).c_str());
#endif
	SetError(buff);
}
void CNeoVMWorker::SetErrorFormat(const char* lpszString, ...)
{
	char buff[1024];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsprintf_s(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, 8, lpszString, arg_ptr);
#endif
	va_end(arg_ptr);

	SetError(buff);
}

bool	CNeoVMWorker::Initialize(int iFunctionID, std::vector<VarInfo>& _args)
{
	_iSP_Vars = 0;// _header._iStaticVarCount;
	_iSP_VarsMax = 0;
	_iSP_Vars_Max2 = 0;
	if (false == Setup(iFunctionID, _args))
		return false;
	_isInitialized = true;
	return Run();
}


bool	CNeoVMWorker::Start(int iFunctionID, std::vector<VarInfo>& _args)
{
	if(false == Setup(iFunctionID, _args))
		return false;

	return Run();
}

bool	CNeoVMWorker::Setup(int iFunctionID, std::vector<VarInfo>& _args)
{
	SFunctionTable& fun = m_sFunctionPtr[iFunctionID];
	int iArgs = (int)_args.size();
	if (iArgs != fun._argsCount)
		return false;

	SetCodePtr(fun._codePtr);

//	_iSP_Vars = 0;// _header._iStaticVarCount;
	SetStackPointer(_iSP_Vars);
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
	if(_iSP_Vars_Max2 < _iSP_VarsMax)
		_iSP_Vars_Max2 = _iSP_VarsMax;

	if (iArgs > fun._argsCount)
		iArgs = fun._argsCount;

	int iCur;
	for (iCur = 0; iCur < iArgs; iCur++)
		(*m_pVarStack_Base)[1 + _iSP_Vars + iCur] = _args[iCur];
	for (; iCur < fun._argsCount; iCur++)
		Var_Release(&(*m_pVarStack_Base)[1 + _iSP_Vars + iCur]);

	return true;
}
bool CNeoVMWorker::IsWorking()
{
	//return _isInitialized;
	return _iSP_VarsMax > 0;
}

bool CNeoVMWorker::BindWorkerFunction(const std::string& funName)
{
	int iFID = FindFunction(funName);
	if (iFID == -1)
		return false;

	std::vector<VarInfo> _args;
	return Setup(iFID, _args);
}
bool	CNeoVMWorker::Run()
{
	bool b = true;
#ifdef _WIN32
	try
	{
#endif
		if(m_iTimeout >= 0)
			b = RunInternal(0, (int)m_pCallStack->size());
		else
			b = RunInternal("abc", (int)m_pCallStack->size());
#ifdef _WIN32
	}
	catch (...)
	{
		//int idx = int((u8*)_pCodeCurrent - _pCodeBegin - 1) / sizeof(SVMOperation);
		SetError("Exception");
		bool blDebugInfo = IsDebugInfo();
		int _lineseq = -1;
		if (blDebugInfo)
			_lineseq = GetDebugLine(_isErrorOPIndex);

		char chMsg[256];

#ifdef _WIN32
		sprintf_s(chMsg, _countof(chMsg), "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), _isErrorOPIndex, _lineseq);
#else
		sprintf(chMsg, "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#endif

		GetVM()->_sErrorMsgDetail = chMsg;
		return false;
	}
#endif
	return b;
}
void CNeoVMWorker::JumpAsyncMsg()
{
	int dist = int((u8*)_pCodeCurrent - _pCodeBegin);
	if (dist < int(2 * sizeof(SVMOperation))) // 0 : Error, 1 : Idle(Already?)
		return;
	SetCodePtr(sizeof(SVMOperation));
	_pCodeCurrent->n23 = dist - int(sizeof(SVMOperation) * 2); // Idle
}
template<typename T>
bool	CNeoVMWorker::RunInternal(T slide, int iBreakingCallStack)
{
	if (false == _isInitialized)
		return false;

	clock_t t1, t2;
	if (_iRemainSleep > 0)
	{
		t1 = clock();
		t2 = t1 - _preClock;
		if (t2 > 0)
			_iRemainSleep -= t2;

		_preClock = t1;

		if (_iRemainSleep > 0)
			return true;
	}

	//SCallStack callStack;
	int iTemp;
	char chMsg[256];

	int op_process = m_iCheckOpCount;
	bool isTimeout = (m_iTimeout >= 0);
	if(isTimeout)
		_preClock = clock();

	VarInfo* pVarTemp = nullptr;
	while (true)
	{
		if(std::is_integral<T>::value)
		{
			if (--op_process <= 0)
			{
				op_process = m_iCheckOpCount;
//				if (isTimeout)
				{
					t2 = clock() - _preClock;
					if (t2 >= m_iTimeout || t2 < 0)
						break;
				}
			}
		}

		const SVMOperation& OP = *GetOP();
		switch (OP.op)
		{
		case NOP_MOV:
			Move(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_MOVI:
			MoveI(GetVarPtrF1(OP), OP.n23); break;

		case NOP_MOV_MINUS:
			MoveMinus(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_ADD2:
			Add2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_SUB2:
			Sub2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_MUL2:
			Mul2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_DIV2:
			Div2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_PERSENT2:
			Per2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_LSHIFT2:
			LSh2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_RSHIFT2:
			RSh2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_AND2:
			And2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_OR2:
			Or2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_XOR2:
			Xor2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;

		case NOP_VAR_CLEAR: Var_Release(GetVarPtrF1(OP)); break;
		case NOP_INC: Inc(GetVarPtrF1(OP)); break;
		case NOP_DEC: Dec(GetVarPtrF1(OP)); break;

		case NOP_ADD3: 
			Add3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_SUB3: 
			Sub3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_MUL3: 
			Mul3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_DIV3: 
			Div3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_PERSENT3: 
			Per3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_LSHIFT3: 
			LSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_RSHIFT3: 
			RSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_AND3: 
			And3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_OR3: 
			Or3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_XOR3: 
			Xor3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;

		case NOP_GREAT:		// >
			Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr2(OP), GetVarPtr3(OP)));
			break;
		case NOP_GREAT_EQ:	// >=
			Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr2(OP), GetVarPtr3(OP)));
			break;
		case NOP_LESS:		// <
			Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr3(OP), GetVarPtr2(OP)));
			break;
		case NOP_LESS_EQ:	// <=
			Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr3(OP), GetVarPtr2(OP)));
			break;
		case NOP_EQUAL2:	// ==
			Var_SetBool(GetVarPtrF1(OP), CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)));
			break;
		case NOP_NEQUAL:	// !=
			Var_SetBool(GetVarPtrF1(OP), !CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)));
			break;
		case NOP_AND:		// &
			And(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
			break;
		case NOP_OR:		// |
			Or(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
			break;
		case NOP_LOG_AND:	// &&
			Var_SetBool(GetVarPtrF1(OP), GetVarPtr3(OP)->IsTrue() && GetVarPtr2(OP)->IsTrue());
			break;
		case NOP_LOG_OR:	// ||
			Var_SetBool(GetVarPtrF1(OP), GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue());
			break;

		case NOP_JMP:
			SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_FALSE:
			if (false == GetVarPtr2(OP)->IsTrue())
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_TRUE:
			if (true == GetVarPtr2(OP)->IsTrue())
				SetCodeIncPtr(OP.n1);
			break;


		case NOP_JMP_GREAT:		// >
			if (true == CompareGR(GetVarPtr2(OP), GetVarPtr3(OP)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_GREAT_EQ:	// >=
			if (true == CompareGE(GetVarPtr2(OP), GetVarPtr3(OP)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_LESS:		// <
			if (true == CompareGR(GetVarPtr3(OP), GetVarPtr2(OP)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_LESS_EQ:	// <=
			if (true == CompareGE(GetVarPtr3(OP), GetVarPtr2(OP)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_EQUAL2:	// ==
			if (true == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_NEQUAL:	// !=
			if (false == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_AND:	// &&
			if (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue())
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_OR:		// ||
			if (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue())
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_NAND:	// !(&&)
			if (false == (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue()))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_NOR:	// !(||)
			if (false == (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_FOR:	// for (cur, cur_protect, begin, end, step)
			//if (For(GetVarPtr2(OP)))
			if (For(GetVarPtr_L(OP.n2)))
				SetCodeIncPtr(OP.n1);
			break;
		case NOP_JMP_FOREACH:	// foreach
			//if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP), GetVarPtr(OP.n3 + 1)))
			if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP)))
				SetCodeIncPtr(OP.n1);
			break;

		case NOP_STR_ADD:	// ..
			Var_SetStringA(GetVarPtrF1(OP), ToString(GetVarPtr2(OP)) + ToString(GetVarPtr3(OP)));
			break;

		case NOP_TOSTRING:
			Var_SetStringA(GetVarPtrF1(OP), ToString(GetVarPtr2(OP)));
			break;
		case NOP_TOINT:
			Var_SetInt(GetVarPtrF1(OP), ToInt(GetVarPtr2(OP)));
			break;
		case NOP_TOFLOAT:
			Var_SetFloat(GetVarPtrF1(OP), ToFloat(GetVarPtr2(OP)));
			break;
		case NOP_TOSIZE:
			Var_SetInt(GetVarPtrF1(OP), ToSize(GetVarPtr2(OP)));
			break;
		case NOP_GETTYPE:
			Move(GetVarPtrF1(OP), GetType(GetVarPtr2(OP)));
			break;
		case NOP_SLEEP:
			{
				int r = Sleep(m_iTimeout, GetVarPtr2(OP));
				if (r == 0)
				{
					_preClock = clock();
					return true;
				}
				else if (r == 2)
				{
					//SetCheckTime();
					op_process = 0;
				}
			}
			break;

		case NOP_FMOV1:
			Var_SetFun(GetVarPtrF1(OP), OP.n2);
			break;
		case NOP_FMOV2:
			//Var_SetFun(&_funA3, OP.n3);
			_funA3._fun_index = OP.n3;
			CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), &_funA3);
			break;

		case NOP_CALL:
			Call(OP.n1, OP.n2, (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr3(OP));
			break;
		case NOP_PTRCALL:
		{
			VarInfo* pVar1 = GetVarPtrF1(OP);
			short n3 = OP.n3;
			VarInfo* pFunName = nullptr;
			switch (pVar1->GetType())
			{
			case VAR_FUN:
				if ((OP.argFlag & 0x02) == 0 && 0 == OP.n2)
					Call(pVar1->_fun_index, OP.n3);
				break;
			case VAR_FUN_NATIVE:
				if ((OP.argFlag & 0x02) == 0 && 0 == OP.n2)
					Call(pVar1->_funPtr, OP.n3);
				break;
			case VAR_CHAR:
				Var_SetString(pVar1, pVar1->_c.c); // char -> string
			case VAR_STRING:
				pFunName = GetVarPtr2(OP);
				CallNative(GetVM()->_funLib_String, pVar1, pFunName->_str, n3);
				break;
			case VAR_MAP:
			{
				pFunName = GetVarPtr2(OP);
				VarInfo* pVarMeta = pVar1->_tbl->Find(pFunName);
				if (pVarMeta != NULL)
				{
					if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
						_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

					if (pVarMeta->GetType() == VAR_FUN)
					{
						Call(pVarMeta->_fun_index, n3);
						break;
					}
				}
				FunctionPtrNative fun = pVar1->_tbl->_fun;
				if (fun._func)
				{
					CallNative(fun, pVar1, pFunName->_str, n3);
					break;
				}
				CallNative(GetVM()->_funLib_Map, pVar1, pFunName->_str, n3);
				break;
			}
			case VAR_LIST:
				pFunName = GetVarPtr2(OP);
				CallNative(GetVM()->_funLib_List, pVar1, pFunName->_str, n3);
				break;
			case VAR_SET:
				break;
			case VAR_ASYNC:
				pFunName = GetVarPtr2(OP);
				CallNative(GetVM()->_funLib_Async, pVar1, pFunName->_str, n3);
				break;
			default:
				SetError("Ptr Call Error");
				break;
			}
			break;
		}
		case NOP_PTRCALL2:
		{
			short n2 = OP.n2;
			VarInfo* pFunName = GetVarPtrF1(OP);
			if (pFunName->GetType() == VAR_CHAR)
			{
				GetVM()->Var_SetStringA(pFunName, pFunName->_c.c);
			}				
			if (pFunName->GetType() == VAR_STRING)
			{
				CallNative(GetVM()->_funLib_Default, NULL, pFunName->_str, n2, (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr3(OP));
			}

			if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
				_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);
			break;
		}			
		case NOP_RETURN:
		{
			if (OP.n1 == 0 && (OP.argFlag & (1 << 5)) == 0)
				Var_Release(&(*m_pVarStack_Base)[_iSP_Vars]); // Clear
			else
				Move(&(*m_pVarStack_Base)[_iSP_Vars], GetVarPtrF1(OP));

			//if (m_sCallStack.empty())
			if(iBreakingCallStack == (int)m_pCallStack->size())
			{
				if (iBreakingCallStack == 0 && IsMainCoroutine(m_pCur) == false)
				{
					if(StopCoroutine(true) == true) // Other Coroutine Active (No Stop)
						break;
				}
//				_isInitialized = false;
				_iSP_VarsMax = _iSP_Vars;
				return true;
			}
			iTemp = (int)m_pCallStack->size() - 1;
			SCallStack &callStack = (*m_pCallStack)[iTemp];
			m_pCallStack->resize(iTemp);

			if(callStack._pReturnValue)
				Move(callStack._pReturnValue, &(*m_pVarStack_Base)[_iSP_Vars]);

			SetCodePtr(callStack._iReturnOffset);
			_iSP_Vars = callStack._iSP_Vars;
			SetStackPointer(_iSP_Vars);
			_iSP_VarsMax = callStack._iSP_VarsMax;
			break;
		}
		case NOP_TABLE_ALLOC:
			Var_SetTable(GetVarPtrF1(OP), GetVM()->TableAlloc(OP.n23));
			break;
		case NOP_CLT_READ:
			CltRead(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
			break;
		case NOP_TABLE_REMOVE:
			TableRemove(GetVarPtrF1(OP), GetVarPtr2(OP));
			break;
		case NOP_CLT_MOV:
			CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_TABLE_ADD2:
			pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
			if (pVarTemp) Add2(pVarTemp, GetVarPtr3(OP));
			break;
		case NOP_TABLE_SUB2:
			pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
			if (pVarTemp) Sub2(pVarTemp, GetVarPtr3(OP));
			break;
		case NOP_TABLE_MUL2:
			pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
			if (pVarTemp) Mul2(pVarTemp, GetVarPtr3(OP));
			break;
		case NOP_TABLE_DIV2:
			pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
			if (pVarTemp) Div2(pVarTemp, GetVarPtr3(OP));
			break;
		case NOP_TABLE_PERSENT2:
			pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
			if (pVarTemp) Per2(pVarTemp, GetVarPtr3(OP));
			break;

		case NOP_LIST_ALLOC:
			Var_SetList(GetVarPtrF1(OP), GetVM()->ListAlloc(OP.n23));
			break;
		case NOP_LIST_REMOVE:
			TableRemove(GetVarPtrF1(OP), GetVarPtr2(OP));
			break;

		case NOP_VERIFY_TYPE:
			VerifyType(GetVarPtrF1(OP), (VAR_TYPE)OP.n2);
			break;
		case NOP_CHANGE_INT:
			ChangeNumber(GetVarPtrF1(OP));
			break;
		case NOP_YIELD:
			if (StopCoroutine(false) == false)
				return true;
			break;
		case NOP_IDLE:
//			op_process = m_iCheckOpCount;
			SetCodeIncPtr(OP.n23);
			//if (isTimeout)
			//{
			//	t2 = clock() - _preClock;
			//	if (t2 >= m_iTimeout || t2 < 0)
			//		break;
			//}
			while (true)
			{
				AsyncInfo* p = GetVM()->Pop_AsyncInfo();
				if (p == nullptr) break;
				Var_SetBool(GetStackFromBase(_iSP_VarsMax + 1), p->_success);
				Var_SetStringA(GetStackFromBase(_iSP_VarsMax + 2), p->_resultValue);
				Call(p->_fun_index, 2); // no no no !!
				GetVM()->Var_Release(&p->_LockReferance);
			}
			break;
		case NOP_NONE:
			break;
		case NOP_ERROR:
			{
				int idx = _isErrorOPIndex;
				_isErrorOPIndex = 0;
				m_pCallStack->clear();
				_iSP_Vars = 0;
				SetStackPointer(_iSP_Vars);
				bool blDebugInfo = IsDebugInfo();
				int _lineseq = -1;
				if (blDebugInfo)
					_lineseq = GetDebugLine(idx);
#ifdef _WIN32
				sprintf_s(chMsg, _countof(chMsg), "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#else
				sprintf(chMsg, "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#endif
				if (GetVM()->_sErrorMsgDetail.empty())
					GetVM()->_sErrorMsgDetail = chMsg;
				return false;
			}
		default:
			SetError("Unknown OP");
			break;
		}
	}
	return true;
}

bool CNeoVMWorker::RunFunctionResume(int iFID, std::vector<VarInfo>& _args)
{
	//Start(iFID, _args);
	VarInfo r;
	testCall(iFID, _args.empty() ? NULL : &_args[0], (int)_args.size());
	return true;
}
bool CNeoVMWorker::RunFunction(int iFID, std::vector<VarInfo>& _args)
{
	Start(iFID, _args);
	return true;
}
bool CNeoVMWorker::RunFunction(const std::string& funName, std::vector<VarInfo>& _args)
{
	auto it = m_sImExportTable.find(funName);
	if (it == m_sImExportTable.end())
	{
		SetError("Function Not Found");
		GetVM()->_sErrorMsgDetail = GetVM()->_pErrorMsg;
		GetVM()->_sErrorMsgDetail += "(";
		GetVM()->_sErrorMsgDetail += funName;
		GetVM()->_sErrorMsgDetail += ")";
		return false;
	}

	int iID = (*it).second;
	Start(iID, _args);

	return true;
}

void	CNeoVMWorker::DeadCoroutine(CoroutineInfo* pCI)
{
	pCI->_state = COROUTINE_STATE_DEAD;
	int iSP_Vars_Max2 = pCI->_info._iSP_Vars_Max2;
	std::vector<VarInfo>& sVarStack = pCI->m_sVarStack;
	for (int i = 0; i < iSP_Vars_Max2; i++)
		Var_Release(&(sVarStack)[i]);

	pCI->_info.ClearSP();
}
bool CNeoVMWorker::StopCoroutine(bool doDead)
{
	CoroutineBase* pThis = (CoroutineBase*)this;
	m_pCur->_info = *pThis;
	if (doDead)
		DeadCoroutine(m_pCur);
	else
		m_pCur->_state = COROUTINE_STATE_SUSPENDED;

	if (m_sCoroutines.empty() == true)
	{
		return false;
	}
	else
	{
		auto it = m_sCoroutines.begin();
		m_pCur = (*it);
		m_sCoroutines.erase(it);
		if(m_pCur->_state != COROUTINE_STATE_NORMAL)
		{
			SetError("Coroutine State Error");
			return false;
		}
		m_pCur->_state = COROUTINE_STATE_RUNNING;
		m_pVarStack_Base = &m_pCur->m_sVarStack;
		m_pCallStack = &m_pCur->m_sCallStack;

		*pThis = m_pCur->_info;
		SetStackPointer(_iSP_Vars);
	}
	return true;
}

bool CNeoVMWorker::StartCoroutione(int argSP_Vars, int n3)
{
	CoroutineBase* pThis = (CoroutineBase*)this;
	if (m_pCur)
	{
		// Back up
		m_pCur->_info = *pThis;
		//m_pCur->_info._iSP_Vars = sp;// _iSP_Vars;
		m_pCur->_state = COROUTINE_STATE_NORMAL;
		m_sCoroutines.push_front(m_pCur);
		CoroutineInfo* pPre = m_pCur;

		m_pCur = m_pRegisterActive;
		m_pCur->_state = COROUTINE_STATE_RUNNING;
		m_pCur->_sub_state = COROUTINE_SUB_NORMAL;
		m_pVarStack_Base = &m_pCur->m_sVarStack;
		m_pCallStack = &m_pCur->m_sCallStack;

		if (m_pCur->_info._pCodeCurrent == NULL) // first run
		{
			int iResumeParamCount = n3 - 1;
			SFunctionTable& fun = m_sFunctionPtr[m_pCur->_fun_index];
			for (int i = 0; i < fun._argsCount; i++)
			{
				if (i < iResumeParamCount)
					Move(&m_pCur->m_sVarStack[i + 1], &pPre->m_sVarStack[i + argSP_Vars + 2]);
				else
					Var_Release(&m_pCur->m_sVarStack[i + 1]); // Zero index return value
			}
			SetCodePtr(fun._codePtr);
			_iSP_Vars = 0;// iSP_VarsMax;
			_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
			_iSP_Vars_Max2 = _iSP_VarsMax;
			m_pCur->_info._pCodeCurrent = _pCodeCurrent;
		}
		else
		{
			*pThis = m_pCur->_info;
		}
		SetStackPointer(_iSP_Vars);
	}
	m_pRegisterActive = NULL;
	return true;
}


VarInfo* CNeoVMWorker::testCall(int iFID, VarInfo* args, int argc)
{
	if (_isInitialized == false)
		return NULL;

	SFunctionTable& fun = m_sFunctionPtr[iFID];
	if (argc != fun._argsCount)
		return NULL;

	int save_Code = GetCodeptr();
	int save_iSP_Vars = _iSP_Vars;
	int save__iSP_VarsMax = _iSP_VarsMax;

	SetCodePtr(fun._codePtr);

//	_iSP_Vars = 0;// _header._iStaticVarCount;
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
	if (_iSP_Vars_Max2 < _iSP_VarsMax)
		_iSP_Vars_Max2 = _iSP_VarsMax;

	//_iSP_Vars_Max2 = iSP_VarsMax;

	if (argc > fun._argsCount)
		argc = fun._argsCount;

	int iCur;
	for (iCur = 0; iCur < argc; iCur++)
		Move(&(*m_pVarStack_Base)[1 + _iSP_Vars + iCur], &args[iCur]);
	for (; iCur < fun._argsCount; iCur++)
		Var_Release(&(*m_pVarStack_Base)[1 + _iSP_Vars + iCur]);

	Run();

//	_read(&(*m_pVarStack)[_iSP_Vars], *r);
	VarInfo* r = &(*m_pVarStack_Base)[_iSP_Vars];

	SetCodePtr(save_Code);
	_iSP_Vars = save_iSP_Vars;
	_iSP_VarsMax = save__iSP_VarsMax;
	SetStackPointer(_iSP_Vars);

	//GC();
	
	_isInitialized = true;
	return r;
}
bool CNeoVMWorker::CallNative(FunctionPtrNative functionPtrNative, void* pUserData, StringInfo* pStr, int n3, VarInfo* pRet)
{
	Neo_NativeFunction func = functionPtrNative._func;
	if (func == NULL)
	{
		SetError("Ptr Call Error");
		return false;
	}
	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	if (_iSP_Vars_Max2 < _iSP_VarsMax + 1 + n3) // ??????? 이렇게 하면 맞나? 흠...
		_iSP_Vars_Max2 = _iSP_VarsMax + 1 + n3;

	if ((func)(this, pUserData, pStr, n3) == false)
	{
		SetError("Ptr Call Error");
		return false;
	}
	if (nullptr != pRet)
		Move(pRet, m_pVarStack_Pointer);

	int argSP_Vars = _iSP_Vars;
	_iSP_Vars = iSave;
	SetStackPointer(_iSP_Vars);
	if (m_pRegisterActive != NULL)
	{
		switch(m_pRegisterActive->_sub_state)
		{ 
			case COROUTINE_SUB_START:
				StartCoroutione(argSP_Vars, n3);
				break;
			case COROUTINE_SUB_CLOSE:
				switch (m_pRegisterActive->_state)
				{
					case COROUTINE_STATE_SUSPENDED:
						DeadCoroutine(m_pRegisterActive);
						break;
					case COROUTINE_STATE_RUNNING:
						if (m_pCur != m_pRegisterActive)SetError("Coroutine Error 1");
						else							StopCoroutine(true);
						break;
					case COROUTINE_STATE_DEAD:
						break;
					case COROUTINE_STATE_NORMAL:
						DeadCoroutine(m_pRegisterActive);
						for(auto it = m_sCoroutines.begin(); it != m_sCoroutines.end(); it++)
						{
							if((*it) == m_pRegisterActive)
							{
								m_sCoroutines.erase(it);
								break;
							}
						}
						break;
				}
				m_pRegisterActive = NULL;
				break;
			default:
				SetError("Coroutine Error 1");
				break;
		}
	}

	return true;
}
bool CNeoVMWorker::PropertyNative(FunctionPtrNative functionPtrNative, void* pUserData, StringInfo* pStr, VarInfo* pRet, bool get)
{
	Neo_NativeProperty func = functionPtrNative._property;
	if (func == NULL)
	{
		SetError("Ptr Call Error");
		return false;
	}
	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	if (_iSP_Vars_Max2 < _iSP_VarsMax + 1)
		_iSP_Vars_Max2 = _iSP_VarsMax + 1;

	if ((func)(this, pUserData, pStr, pRet, get) == false)
	{
		SetError("Ptr Call Error");
		return false;
	}

//	Move(pRet, GetReturnVar());

	int argSP_Vars = _iSP_Vars;
	_iSP_Vars = iSave;
	SetStackPointer(_iSP_Vars);
	if (m_pRegisterActive != NULL)
	{
		switch (m_pRegisterActive->_sub_state)
		{
		case COROUTINE_SUB_START:
			StartCoroutione(argSP_Vars, 0);
			break;
		case COROUTINE_SUB_CLOSE:
			switch (m_pRegisterActive->_state)
			{
			case COROUTINE_STATE_SUSPENDED:
				DeadCoroutine(m_pRegisterActive);
				break;
			case COROUTINE_STATE_RUNNING:
				if (m_pCur != m_pRegisterActive)SetError("Coroutine Error 1");
				else							StopCoroutine(true);
				break;
			case COROUTINE_STATE_DEAD:
				break;
			case COROUTINE_STATE_NORMAL:
				DeadCoroutine(m_pRegisterActive);
				for (auto it = m_sCoroutines.begin(); it != m_sCoroutines.end(); it++)
				{
					if ((*it) == m_pRegisterActive)
					{
						m_sCoroutines.erase(it);
						break;
					}
				}
				break;
			}
			m_pRegisterActive = NULL;
			break;
		default:
			SetError("Coroutine Error 1");
			break;
		}
	}

	return true;
}

bool CNeoVMWorker::VerifyType(VarInfo *p, VAR_TYPE t)
{
	if (p->GetType() == t)
		return true;
	char ch[256];
#ifdef _WIN32
	sprintf_s(ch, _countof(ch), "VerifyType (%s != %s)", GetDataType(p->GetType()).c_str(), GetDataType(t).c_str());
#else
	sprintf(ch, "VerifyType (%s != %s)", GetDataType(p->GetType()).c_str(), GetDataType(t).c_str());
#endif

	SetError(ch);
	return false;
}

bool CNeoVMWorker::ChangeNumber(VarInfo* p)
{
	switch(p->GetType())
	{
		case VAR_INT:
			return true;
		case VAR_FLOAT:
			p->SetType(VAR_INT);
			p->_int = (int)p->_float;
			return true;
		default:
			break;
	}
	char ch[256];
#ifdef _WIN32
	sprintf_s(ch, _countof(ch), "ChangeNumber (%s != number)", GetDataType(p->GetType()).c_str());
#else
	sprintf(ch, "ChangeNumber (%s != number)", GetDataType(p->GetType()).c_str());
#endif

	SetError(ch);
	return false;
}


std::string GetDataType(VAR_TYPE t)
{
	switch (t)
	{
	case VAR_NONE:
		return "none";
	case VAR_INT:
		return "int";
	case VAR_FLOAT:
		return "float";
	case VAR_BOOL:
		return "bool";
	case VAR_FUN:
		return "fun";
	case VAR_ITERATOR:
		return "iterator";
	case VAR_FUN_NATIVE:
		return "fun";
	case VAR_CHAR:
	case VAR_STRING:
		return "string";
	case VAR_MAP:
		return "map";
	case VAR_LIST:
		return "list";
	case VAR_SET:
		return "set";
	case VAR_COROUTINE:
		return "coroutine";
	default:
		break;
	}
	return "unknown";
}

};
