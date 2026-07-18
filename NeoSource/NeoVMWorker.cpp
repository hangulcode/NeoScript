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

int& GetModuleRefCount(VarInfo* p)
{
	return ((CNeoVMWorker*)(p->_module))->_refCount;
}

CNeoVMWorker::CNeoVMWorker(INeoVM* pVM, u32 id, int iStackSize)
{
	_pVM = pVM;
	_idWorker = id;
	//SetCodeData(pVM->_pCodePtr, pVM->_iCodeLen);
	m_pVarGlobal = &m_sVarGlobal;
	m_pVarGlobal_Pointer = nullptr;

	// 실행 스택은 더 이상 워커가 소유하지 않는다. 실행 시 풀에서 컨텍스트를 대여해 바인딩한다.
	m_pCur = nullptr;
	m_pMainCtx = nullptr;
	m_pVarStack_Base = nullptr;
	m_pCallStack = nullptr;
	m_pVarStack_Pointer = nullptr;

	ClearSP();

	_funA3.SetType(VAR_FUN);
	(void)iStackSize;
}
CNeoVMWorker::~CNeoVMWorker()
{
	// 정지 상태로 남은(retain 된) 최상위 실행 컨텍스트를 풀로 반납.
	if (m_pMainCtx != nullptr)
		ReleaseExecution();

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
	if (iSleepTick < 0)
		iSleepTick = 0;

	if (iTimeout >= 0)
	{
		_iRemainSleep = iSleepTick;
		_preClock = std::chrono::steady_clock::now();
		return 0;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(iSleepTick));
	return 1;
}

//#ifdef NS_SINGLE_PRECISION
//	#define FLOAT_FORMAT	"%f"
//#else
//	#define FLOAT_FORMAT	"%lf"
//#endif

#ifdef NS_SINGLE_PRECISION
	#define FLOAT_FORMAT	"%g"
#else
	#define FLOAT_FORMAT	"%g"
#endif


std::string CNeoVMWorker::ToString(VarInfo* v1)
{
	char ch[256];
	switch (v1->GetType())
	{
	case VAR_INT:
#ifdef _WIN32
		snprintf(ch, _countof(ch), "%d", v1->_int);
#else
		sprintf(ch, "%d", v1->_int);
#endif
		return ch;
	case VAR_FLOAT:
		
#ifdef _WIN32
		snprintf(ch, _countof(ch), FLOAT_FORMAT, v1->_float);
#else
		sprintf(ch, FLOAT_FORMAT, v1->_float);
#endif
		return ch;
	case VAR_BOOL:
		return v1->_bl ? "true" : "false";
	case VAR_NONE:
		return "null";
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
	case VAR_INT:
		return v1->_int;
	case VAR_FLOAT:
		return (int)v1->_float;
	case VAR_BOOL:
		return v1->_bl ? 1 : 0;
	case VAR_NONE:
		return -1;
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
	case VAR_INT:
		return (NS_FLOAT)v1->_int;
	case VAR_FLOAT:
		return v1->_float;
	case VAR_BOOL:
		return v1->_bl ? (NS_FLOAT)1 : (NS_FLOAT)0;
	case VAR_NONE:
		return -1;
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
	case VAR_INT:
		return 0;
	case VAR_FLOAT:
		return 0;
	case VAR_BOOL:
		return 0;
	case VAR_NONE:
		return 0;
	case VAR_CHAR: // 문자열 길이
		return (v1->_c.c[0] == 0) ? 0 : 1;
	case VAR_STRING: // 문자열 길이
		return (int)v1->_str->_str.length();
	case VAR_MAP:
		return (int)v1->_tbl->_itemCount;
	case VAR_LIST:
		return (int)v1->_lst->_itemCount;
	case VAR_SET:
		return (int)v1->_set->_itemCount;
	default:
		break;
	}
	return 0;
}
VarInfo* CNeoVMWorker::GetType(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		return &GetVM()->m_sDefaultValue[NDF_INT];
	case VAR_FLOAT:
		return &GetVM()->m_sDefaultValue[NDF_FLOAT];
	case VAR_BOOL:
		return &GetVM()->m_sDefaultValue[NDF_BOOL];
	case VAR_NONE:
		return &GetVM()->m_sDefaultValue[NDF_NULL];
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
	if (!EnsureStackRange(_iSP_VarsMax, n2))
		return;

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
		// SetError 는 오류 opcode로 점프시키므로 코드 포인터는 복구하지 않는다.
		// 다만 native 호출 프레임으로 옮긴 스택 레지스터는 부모 프레임으로 되돌려야 한다.
		_iSP_Vars = iSave_SP_Vars;
		_iSP_VarsMax = iSave_SP_VarsMax;
		SetStackPointer(_iSP_Vars);
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
	// 호출 스택을 push하거나 현재 프레임을 변경하기 전에 새 프레임 전체를 검증한다.
	if (!EnsureStackRange(_iSP_VarsMax, fun._localAddCount - 1))
		return;
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

}

bool CNeoVMWorker::Call_MetaTable(VarInfo* pTable, std::string& funName, VarInfo* r, VarInfo* a, VarInfo* b)
{
	if (pTable->_tbl->_meta == NULL)
		return false;
	VarInfo* pVarItem = pTable->_tbl->_meta->Find(funName);
	if (pVarItem == NULL)
		return false;
	if (pVarItem->GetType() != VAR_FUN)
		return false;

	int n3 = 2;
	if (!EnsureStackRange(_iSP_VarsMax, n3))
		return false;

	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 1], a);
	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 2], b);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

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
	if (pVarItem->GetType() != VAR_FUN)
		return false;

	int n3 = 2;
	if (!EnsureStackRange(_iSP_VarsMax, n3))
		return false;

	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 1], r);
	Move(&(*m_pVarStack_Base)[_iSP_VarsMax + 2], b);

	if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
		_iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

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
static u32 ReadCount(CNArchive& ar)
{
	u16 wCount = 0;
	ar >> wCount;
	if (wCount < 0xFFFF)
		return wCount;

	u32 dwCount = 0;
	ar >> dwCount;
	return dwCount;
}
bool CNeoVMWorker::Init(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, int iStackSize)
{
	// 실행 컨텍스트 풀: 명시 주입 우선, 없으면 VM(임플) 기본 풀 상속(모듈 로드 워커 등).
	if (vparam != nullptr && vparam->execPool != nullptr)
		m_pPool = vparam->execPool;
	else
		m_pPool = GetVM()->GetExecPool();

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

	bool IsDataSinglePrecision = (header ._dwFlag & NEO_HEADER_FLAG_SINGLE_PRECISION) ? true : false;
	if(IsDataSinglePrecision != INeoVM::IsSinglePrecision())
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

	int opCount = header._iCodeSize / (int)sizeof(SVMOperation);
	SVMOperation* pOps = (SVMOperation*)_pCodeBegin;
	for (int i = 0; i < opCount; ++i)
	{
		SVMOperation& op = pOps[i];
		if (op.op != NOP_PTRCALL2)
			continue;

		if (op.argFlag & NEOS_ARG_N1_LOCAL)
			continue;

		VarInfo* pFunName = NEOS_GLOBAL_VAR(op.n1);
		if (pFunName == nullptr || pFunName->GetType() != VAR_STRING)
			continue;

		int nativeIndex = CNeoVMImpl::FindDefaultNativeIndex(pFunName->_str);
		if (nativeIndex < 0 || nativeIndex > SHRT_MAX)
			continue;

		op.op = NOP_NATIVECALL;
		op.n1 = (short)nativeIndex;
	}

	if (header.m_iDebugCount > 0)
	{
		_DebugData.resize(header.m_iDebugCount);
		ar.Read(&_DebugData[0], sizeof(debug_info) * header.m_iDebugCount);
	}
	while (ar.GetBufferOffset() + (int)sizeof(u32) <= ar.GetBufferSize())
	{
		u32 magic = 0;
		int oldOffset = ar.GetBufferOffset();
		ar >> magic;
		if (magic == 0x4E445642)
		{
			u32 funCount = ReadCount(ar);
			for (u32 i = 0; i < funCount; ++i)
			{
				int funId = -1;
				ar >> funId;
				u32 nameCount = ReadCount(ar);
				std::map<int, std::string>& names = (funId == -1) ? m_sDebugGlobalNames : m_sDebugVarNames[funId];
				for (u32 n = 0; n < nameCount; ++n)
				{
					int slot = -1;
					ar >> slot;
					ReadString(ar, Name);
					names[slot] = Name;
				}
			}
		}
		else if (magic == 0x4E44464E)
		{
			u32 funCount = ReadCount(ar);
			for (u32 i = 0; i < funCount; ++i)
			{
				int funId = -1;
				ar >> funId;
				ReadString(ar, Name);
				m_sDebugFunctionNames[funId] = Name;
			}
		}
		else
		{
			ar.SetBufferOffset(oldOffset);
			break;
		}
	}
	if(vparam && vparam->NeoGlobalInterface)
	{
		vparam->NeoGlobalInterface(this, vparam->param);
	}
	return true;
}
int CNeoVMWorker::GetDebugLine(int iOPIndex)
{
//	int idx = int((u8*)_pCodeCurrent - _pCodeBegin - 1) / sizeof(SVMOperation);
	if ((int)_DebugData.size() <= iOPIndex || iOPIndex < 0) return -1;
	return _DebugData[iOPIndex]._lineseq;
}
int CNeoVMWorker::GetFunctionIndexFromCodeOffset(int codeOffset)
{
	int iFind = -1;
	int iFindCodePtr = -1;
	for (int i = 0; i < (int)m_sFunctionPtr.size(); ++i)
	{
		int codePtr = m_sFunctionPtr[i]._codePtr;
		if (codePtr <= codeOffset && codePtr >= iFindCodePtr)
		{
			iFind = i;
			iFindCodePtr = codePtr;
		}
	}
	return iFind;
}
std::string CNeoVMWorker::FormatStackTrace(int currentOpIndex)
{
	std::string trace;
	trace.reserve(512);
	trace += "\nScript Call Stack:";

	auto appendFrame = [&](int frameIndex, int opIndex, int stackBase)
	{
		int functionId = GetFunctionIndexFromCodeOffset(opIndex * (int)sizeof(SVMOperation));
		std::string functionName;
		if (functionId >= 0)
		{
			auto itName = m_sDebugFunctionNames.find(functionId);
			if (itName != m_sDebugFunctionNames.end())
				functionName = itName->second;
		}
		if (functionName.empty())
		{
			functionName = "function#";
			functionName += std::to_string(functionId);
		}

		int file = -1;
		int line = -1;
		if (opIndex >= 0 && opIndex < (int)_DebugData.size())
		{
			file = _DebugData[opIndex]._fileseq;
			line = _DebugData[opIndex]._lineseq;
		}

		trace += "\n  #";
		trace += std::to_string(frameIndex);
		trace += " ";
		trace += functionName;
		trace += " : IP(";
		trace += std::to_string(opIndex);
		trace += "), File(";
		trace += std::to_string(file);
		trace += "), Line(";
		trace += std::to_string(line);
		trace += "), SP(";
		trace += std::to_string(stackBase);
		trace += ")";

		if (functionId >= 0 && functionId < (int)m_sFunctionPtr.size())
		{
			SFunctionTable& fun = m_sFunctionPtr[functionId];
			trace += ", Args[";
			for (int argIndex = 0; argIndex < fun._argsCount; ++argIndex)
			{
				if (argIndex > 0)
					trace += ", ";

				int varIndex = stackBase + 1 + argIndex;
				std::string argName;
				auto itFunNames = m_sDebugVarNames.find(functionId);
				if (itFunNames != m_sDebugVarNames.end())
				{
					auto itName = itFunNames->second.find(1 + argIndex);
					if (itName != itFunNames->second.end())
						argName = itName->second;
				}
				if (argName.empty())
				{
					argName = "arg";
					argName += std::to_string(argIndex + 1);
				}

				trace += argName;
				trace += ":";
				if (varIndex >= 0 && varIndex < (int)m_pVarStack_Base->size())
					trace += GetDataType((*m_pVarStack_Base)[varIndex].GetType());
				else
					trace += "out_of_range";
			}
			trace += "]";
		}
	};

	if (currentOpIndex < 0)
		currentOpIndex = 0;
	appendFrame(0, currentOpIndex, _iSP_Vars);

	if (m_pCallStack == nullptr)
		return trace;

	for (int i = (int)m_pCallStack->size() - 1, frameIndex = 1; i >= 0; --i, ++frameIndex)
	{
		SCallStack& cs = (*m_pCallStack)[i];
		int opIndex = cs._iReturnOffset / (int)sizeof(SVMOperation);
		if (opIndex > 0)
			--opIndex;
		appendFrame(frameIndex, opIndex, cs._iSP_Vars);
	}
	return trace;
}
bool CNeoVMWorker::CheckDebugStop(int iOPIndex)
{
	if (m_bDebugPaused)
		return true;
	if (m_pDebugListener == nullptr && m_iDebugBreakCount <= 0 && m_eDebugRunMode == DBG_CONTINUE && m_bDebugPauseRequested == false)
		return false;
	if (IsDebugInfo() == false)
		return false;
	if ((int)_DebugData.size() <= iOPIndex || iOPIndex < 0)
		return false;

	const debug_info& info = _DebugData[iOPIndex];
	int file = info._fileseq;
	int line = info._lineseq;
	if (line <= 0)
		return false;
	int callDepth = m_pCallStack ? (int)m_pCallStack->size() : 0;

	if (m_iDebugSkipOpIndex >= 0)
	{
		if (iOPIndex == m_iDebugSkipOpIndex)
			return false;
		m_iDebugSkipOpIndex = -1;
	}

	if (m_iDebugSkipLine >= 0)
	{
		bool sameSourceLine = (line == m_iDebugSkipLine) && (m_iDebugSkipFile < 0 || file == m_iDebugSkipFile);
		if (sameSourceLine && m_bDebugPauseRequested == false)
		{
			if (m_eDebugRunMode == DBG_STEP_INTO)
			{
				if (callDepth <= m_iDebugStepDepth)
					return false;
			}
			else
			{
				return false;
			}
		}
		if (m_eDebugRunMode == DBG_STEP_OVER && callDepth > m_iDebugStepDepth)
			return false;
		m_iDebugSkipFile = -1;
		m_iDebugSkipLine = -1;
	}

	if (m_bDebugPauseRequested)
	{
		StopDebug(iOPIndex, NEO_DEBUG_STOP_PAUSE);
		return true;
	}
	if (m_eDebugRunMode == DBG_STEP_INTO)
	{
		StopDebug(iOPIndex, NEO_DEBUG_STOP_STEP);
		return true;
	}
	if (m_eDebugRunMode == DBG_STEP_OVER)
	{
		if (callDepth <= m_iDebugStepDepth)
		{
			StopDebug(iOPIndex, NEO_DEBUG_STOP_STEP);
			return true;
		}
	}
	if (m_eDebugRunMode == DBG_STEP_OUT)
	{
		if (callDepth < m_iDebugStepDepth)
		{
			StopDebug(iOPIndex, NEO_DEBUG_STOP_STEP);
			return true;
		}
	}
	if (m_iDebugBreakCount > 0 && IsDebugBreakpoint(file, line))
	{
		StopDebug(iOPIndex, NEO_DEBUG_STOP_BREAKPOINT);
		return true;
	}
	return false;
}
void CNeoVMWorker::StopDebug(int iOPIndex, NeoDebugStopReason reason)
{
	const debug_info& info = _DebugData[iOPIndex];
	m_sDebugLocation.opIndex = iOPIndex;
	m_sDebugLocation.file = info._fileseq;
	m_sDebugLocation.line = info._lineseq;
	m_sDebugLocation.callDepth = m_pCallStack ? (int)m_pCallStack->size() : 0;
	m_bDebugPaused = true;
	m_bDebugFaulted = (reason == NEO_DEBUG_STOP_EXCEPTION);
	m_bDebugPauseRequested = false;
	m_eDebugRunMode = DBG_PAUSED;
	if (m_pDebugListener)
		m_pDebugListener->OnNeoDebugStopped(this, m_sDebugLocation, reason);
}
void CNeoVMWorker::ResetFaultStateForNewExecution()
{
	if (m_bDebugFaulted == false)
		return;

	m_bDebugFaulted = false;
	_isErrorOPIndex = 0;
}
void CNeoVMWorker::SetError(const char* pErrMsg)
{
	if(_isErrorOPIndex == 0)
		_isErrorOPIndex = int((u8*)_pCodeCurrent - _pCodeBegin - 1) / sizeof(SVMOperation);
	SetCodePtr(0); // Jump To Error
	GetVM()->SetError(std::string(pErrMsg));
}
void CNeoVMWorker::SetErrorUnsupport(const char* pErrMsg, VarInfo* p)
{
	char buff[1024];
#ifdef _WIN32
	snprintf(buff, _countof(buff), pErrMsg, GetDataType(p->GetType()).c_str());
#else
	sprintf(buff, pErrMsg, GetDataType(p->GetType()).c_str());
#endif
	SetError(buff);
}
void CNeoVMWorker::SetErrorFormat(const char* fmt, ...)
{
	char buff[1024];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buff, sizeof(buff), fmt, ap);
	va_end(ap);

	buff[sizeof(buff)-1] = '\0';
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
	bool result = Run();
	GetVM()->PublishAllocStats();
	return result;
}


bool	CNeoVMWorker::Start(int iFunctionID, std::vector<VarInfo>& _args)
{
	if (m_pMainCtx == nullptr)
		return ExecuteTop(iFunctionID, _args) != NEOEXEC_ERROR;

	if(false == Setup(iFunctionID, _args))
		return false;

	bool result = Run();
	GetVM()->PublishAllocStats();
	return result;
}

// ─── 실행 컨텍스트(풀) 기반 최상위 실행/재개 ──────────────────────────────
void CNeoVMWorker::BindContext(CoroutineInfo* ctx)
{
	m_pCur = ctx;
	m_pVarStack_Base = &ctx->m_sVarStack;
	m_pCallStack = &ctx->m_sCallStack;
	m_pVarStack_Pointer = &(*m_pVarStack_Base)[0];
}

// 반납 전, 사용된 스택 슬롯[0..usedMax)의 VarInfo 참조를 정리 (DeadCoroutine 과 동일 패턴).
void CNeoVMWorker::CleanupContextVars(CoroutineInfo* ctx, int usedMax)
{
	std::vector<VarInfo>& s = ctx->m_sVarStack;
	if (usedMax > (int)s.size()) usedMax = (int)s.size();
	for (int i = 0; i < usedMax; i++)
		Var_Release(&s[i]);
}

// 최상위 실행 종료: 메인 컨텍스트를 풀로 반납한다.
// (코루틴 컨텍스트는 각자의 VarInfo 가 GC/스택 해제될 때 FreeCoroutine 으로 반납된다.)
void CNeoVMWorker::ReleaseExecution()
{
	if (m_pMainCtx != nullptr && m_pPool != nullptr)
	{
		// 정상 완료 시 m_pCur == m_pMainCtx 이며 high-water 는 워커의 _iSP_Vars_Max2.
		int usedMax;
		if (m_pCur == m_pMainCtx)
		{
			usedMax = _iSP_Vars_Max2;
			// 호스트콜(Call/iCall)은 GC() 가 _iSP_Vars_Max2 를 _iSP_Vars 로 줄이고 리턴 슬롯[_iSP_Vars]을
			// 남긴다. GC 는 그 위를 이미 해제했으므로 리턴 슬롯까지 포함해 정리하면 컨텍스트가 완전히 clean.
			if (usedMax < _iSP_Vars + 1)
				usedMax = _iSP_Vars + 1;
		}
		else
		{
			usedMax = m_pMainCtx->_info._iSP_Vars_Max2;
		}
		CleanupContextVars(m_pMainCtx, usedMax);
		m_pPool->Release(m_pMainCtx);
	}
	m_pCur = nullptr;
	m_pMainCtx = nullptr;
	m_pRegisterActive = nullptr;
	m_sCoroutines.clear();
	ClearSP();
	_iRemainSleep = 0;
	m_pVarStack_Base = nullptr;
	m_pCallStack = nullptr;
	m_pVarStack_Pointer = nullptr;
}

// Run() 실행 후 완료/정지/에러를 판정. 정지가 아니면 컨텍스트를 반납한다.
int CNeoVMWorker::RunSettle()
{
	m_bTopExec = true;
	bool ok = Run();
	m_bTopExec = false;
	if (ok == false)
	{
		ReleaseExecution();
		return NEOEXEC_ERROR;
	}
	// 정지(retain) 조건: sleep 대기 또는 디버거 pause.
	if (_iRemainSleep > 0 || m_bDebugPaused)
		return NEOEXEC_SUSPENDED;

	ReleaseExecution();
	return NEOEXEC_COMPLETED;
}

// 최상위 실행: 풀에서 컨텍스트를 대여해 iFID 를 처음부터 실행.
int CNeoVMWorker::ExecuteTop(int iFunctionID, std::vector<VarInfo>& _args)
{
	if (m_pPool == nullptr)
		return NEOEXEC_ERROR;
	if (m_pMainCtx != nullptr)
		return NEOEXEC_ERROR;   // 이미 정지된 실행이 있음 → ResumeTop 을 써야 함

	m_pMainCtx = m_pPool->Acquire();
	BindContext(m_pMainCtx);
	m_pRegisterActive = nullptr;
	m_sCoroutines.clear();
	ResetFaultStateForNewExecution();
	_iSP_Vars = 0;
	_iSP_VarsMax = 0;
	_iSP_Vars_Max2 = 0;
	_iRemainSleep = 0;
	_isInitialized = true;

	if (Setup(iFunctionID, _args) == false)
	{
		ReleaseExecution();
		GetVM()->PublishAllocStats();
		return NEOEXEC_ERROR;
	}
	int status = RunSettle();
	GetVM()->PublishAllocStats();
	return status;
}

// 정지된 최상위 실행을 이어서 재개 (retain 된 컨텍스트/레지스터로 계속).
int CNeoVMWorker::ResumeTop()
{
	if (m_pMainCtx == nullptr)
		return NEOEXEC_COMPLETED;   // 정지 상태 아님
	int status = RunSettle();
	GetVM()->PublishAllocStats();
	return status;
}

// 호스트→스크립트 함수 호출(Call/CallN/iCall/iCallN)용 컨텍스트 대여.
// idle 이면 최상위 컨텍스트를 새로 대여(true), 이미 실행/정지 중이면 중첩으로 보고 재사용(false).
bool CNeoVMWorker::BeginHostCall()
{
	if (m_pMainCtx != nullptr)
		return false;
	if (m_pPool == nullptr)
		return false;

	m_pMainCtx = m_pPool->Acquire();
	BindContext(m_pMainCtx);
	m_pRegisterActive = nullptr;
	m_sCoroutines.clear();
	ResetFaultStateForNewExecution();
	_iSP_Vars = 0;
	_iSP_VarsMax = 0;
	_iSP_Vars_Max2 = 0;
	_iRemainSleep = 0;
	_isInitialized = true;
	return true;
}
void CNeoVMWorker::EndHostCall(bool acquired)
{
	if (acquired == false)
		return;
	// An error can also be reported as a debugger pause.  It is inspect-only:
	// never retain the failed execution context for a later F5/F10/F11 resume.
	if (m_bDebugFaulted)
	{
		ReleaseExecution();
		GetVM()->PublishAllocStats();
		return;
	}
	if (_iRemainSleep > 0 || m_bDebugPaused)
	{
		GetVM()->PublishAllocStats();
		return;   // 호출이 정지(sleep/브레이크)됐으면 컨텍스트 retain
	}
	ReleaseExecution();
	GetVM()->PublishAllocStats();
}

bool	CNeoVMWorker::Setup(int iFunctionID, std::vector<VarInfo>& _args)
{
	if (iFunctionID < 0 || iFunctionID >= (int)m_sFunctionPtr.size())
		return false;

	SFunctionTable& fun = m_sFunctionPtr[iFunctionID];
	int iArgs = (int)_args.size();
	if (iArgs != fun._argsCount)
		return false;
	if (!EnsureStackRange(_iSP_Vars, fun._localAddCount - 1))
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
		Move(&(*m_pVarStack_Base)[1 + _iSP_Vars + iCur], &_args[iCur]);
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

	// 시분할 워커(CreateWorker+UpdateWorker) 경로: 실행 컨텍스트를 풀에서 대여해 바인딩한다.
	// (엔진 스크립트는 ExecuteTop 경로를 쓰고 이 API 는 사용하지 않는다.)
	if (m_pMainCtx == nullptr && m_pPool != nullptr)
	{
		m_pMainCtx = m_pPool->Acquire();
		BindContext(m_pMainCtx);
		ResetFaultStateForNewExecution();
		_iSP_Vars = 0;
		_iSP_VarsMax = 0;
		_iSP_Vars_Max2 = 0;
		_isInitialized = true;
	}
	if (m_pMainCtx == nullptr)
		return false;

	std::vector<VarInfo> _args;
	return Setup(iFID, _args);
}
void CNeoVMWorker::DebugSetListener(INeoVMDebugListener* listener)
{
	m_pDebugListener = listener;
}
void CNeoVMWorker::ClearDebugBreakpoints()
{
	m_sDebugBreakLineBits.clear();
	m_sDebugBreakLocationBits.clear();
	m_iDebugBreakCount = 0;
}
void CNeoVMWorker::SetDebugBreakLineBit(std::vector<u8>& bits, int line)
{
	if (line <= 0)
		return;
	size_t byteIndex = (size_t)line >> 3;
	u8 mask = (u8)(1 << (line & 7));
	if (bits.size() <= byteIndex)
		bits.resize(byteIndex + 1, 0);
	if ((bits[byteIndex] & mask) == 0)
	{
		bits[byteIndex] |= mask;
		++m_iDebugBreakCount;
	}
}
bool CNeoVMWorker::IsDebugBreakLineBit(const std::vector<u8>& bits, int line) const
{
	if (line <= 0)
		return false;
	size_t byteIndex = (size_t)line >> 3;
	if (byteIndex >= bits.size())
		return false;
	u8 mask = (u8)(1 << (line & 7));
	return (bits[byteIndex] & mask) != 0;
}
bool CNeoVMWorker::IsDebugBreakpoint(int file, int line) const
{
	if (IsDebugBreakLineBit(m_sDebugBreakLineBits, line))
		return true;
	if (file < 0 || file >= (int)m_sDebugBreakLocationBits.size())
		return false;
	return IsDebugBreakLineBit(m_sDebugBreakLocationBits[file], line);
}
void CNeoVMWorker::DebugSetBreakpoints(const std::vector<int>& lines)
{
	ClearDebugBreakpoints();
	for (int line : lines)
	{
		SetDebugBreakLineBit(m_sDebugBreakLineBits, line);
	}
}
void CNeoVMWorker::DebugSetBreakpoints(const std::vector<NeoDebugBreakpoint>& breakpoints)
{
	ClearDebugBreakpoints();
	for (const NeoDebugBreakpoint& bp : breakpoints)
	{
		if (bp.line <= 0)
			continue;
		if (bp.file < 0)
		{
			SetDebugBreakLineBit(m_sDebugBreakLineBits, bp.line);
			continue;
		}
		if ((int)m_sDebugBreakLocationBits.size() <= bp.file)
			m_sDebugBreakLocationBits.resize((size_t)bp.file + 1);
		SetDebugBreakLineBit(m_sDebugBreakLocationBits[bp.file], bp.line);
	}
}
void CNeoVMWorker::DebugGetExecutableLines(std::vector<int>& lines)
{
	lines.clear();
	if (IsDebugInfo() == false)
		return;

	std::set<int> uniqueLines;
	for (const debug_info& info : _DebugData)
	{
		if (info._lineseq > 0)
			uniqueLines.insert(info._lineseq);
	}

	lines.assign(uniqueLines.begin(), uniqueLines.end());
}
void CNeoVMWorker::DebugGetExecutableLocations(std::vector<NeoDebugLocation>& locations)
{
	locations.clear();
	if (IsDebugInfo() == false)
		return;

	std::set<u32> uniqueLocations;
	for (int i = 0; i < (int)_DebugData.size(); ++i)
	{
		const debug_info& info = _DebugData[i];
		if (info._lineseq <= 0)
			continue;
		u32 key = (((u32)info._fileseq) << 16) | (u32)info._lineseq;
		if (uniqueLocations.insert(key).second)
		{
			NeoDebugLocation location;
			location.opIndex = i;
			location.file = info._fileseq;
			location.line = info._lineseq;
			locations.push_back(location);
		}
	}
}
void CNeoVMWorker::DebugContinue()
{
	m_bDebugPaused = false;
	m_bDebugPauseRequested = false;
	m_eDebugRunMode = DBG_CONTINUE;
	m_iDebugSkipFile = m_sDebugLocation.file;
	m_iDebugSkipLine = m_sDebugLocation.line;
	m_iDebugSkipOpIndex = m_sDebugLocation.opIndex;
	m_iDebugStepDepth = -1;
}
void CNeoVMWorker::DebugStepInto()
{
	m_bDebugPaused = false;
	m_bDebugPauseRequested = false;
	m_eDebugRunMode = DBG_STEP_INTO;
	m_iDebugSkipFile = m_sDebugLocation.file;
	m_iDebugSkipLine = m_sDebugLocation.line;
	m_iDebugSkipOpIndex = m_sDebugLocation.opIndex;
	m_iDebugStepDepth = m_sDebugLocation.callDepth;
}
void CNeoVMWorker::DebugStepOver()
{
	m_bDebugPaused = false;
	m_bDebugPauseRequested = false;
	m_eDebugRunMode = DBG_STEP_OVER;
	m_iDebugSkipFile = m_sDebugLocation.file;
	m_iDebugSkipLine = m_sDebugLocation.line;
	m_iDebugSkipOpIndex = m_sDebugLocation.opIndex;
	m_iDebugStepDepth = m_sDebugLocation.callDepth;
}
void CNeoVMWorker::DebugStepOut()
{
	m_bDebugPaused = false;
	m_bDebugPauseRequested = false;
	m_eDebugRunMode = DBG_STEP_OUT;
	m_iDebugSkipFile = m_sDebugLocation.file;
	m_iDebugSkipLine = m_sDebugLocation.line;
	m_iDebugSkipOpIndex = m_sDebugLocation.opIndex;
	m_iDebugStepDepth = m_sDebugLocation.callDepth;
}
void CNeoVMWorker::DebugPause()
{
	m_bDebugPauseRequested = true;
}
bool CNeoVMWorker::DebugIsPaused()
{
	return m_bDebugPaused;
}
NeoDebugLocation CNeoVMWorker::DebugGetLocation()
{
	return m_sDebugLocation;
}
static const int NEO_DEBUG_MAX_COLLECTION_DEPTH = 4;
static const int NEO_DEBUG_MAX_COLLECTION_ITEMS = 64;

static void NeoDebugFormatValue(VarInfo* pVar, NeoDebugVariable& out, int collectionDepth = 0)
{
	char buf[128];
	switch (pVar->GetType())
	{
	case VAR_INT:
		out.type = "int";
		snprintf(buf, sizeof(buf), "%d", pVar->_int);
		out.value = buf;
		break;
	case VAR_FLOAT:
		out.type = "float";
		snprintf(buf, sizeof(buf), "%g", (double)pVar->_float);
		out.value = buf;
		break;
	case VAR_BOOL:
		out.type = "bool";
		out.value = pVar->_bl ? "true" : "false";
		break;
	case VAR_NONE:
		out.type = "none";
		out.value = "none";
		break;
	case VAR_FUN:
		out.type = "function";
		snprintf(buf, sizeof(buf), "#%d", pVar->_fun_index);
		out.value = buf;
		break;
	case VAR_FUN_NATIVE:
		out.type = "native_function";
		out.value = "native_function";
		break;
	case VAR_CHAR:
		out.type = "char";
		out.value = pVar->_c.c;
		break;
	case VAR_STRING:
		out.type = "string";
		out.value = pVar->_str ? pVar->_str->_str : "";
		break;
	case VAR_MAP:
		out.type = "map";
		{
			const int count = pVar->_tbl ? pVar->_tbl->GetCount() : 0;
			snprintf(buf, sizeof(buf), "map(%d)", count);
			if (pVar->_tbl != nullptr && collectionDepth < NEO_DEBUG_MAX_COLLECTION_DEPTH)
			{
				const int childCount = count < NEO_DEBUG_MAX_COLLECTION_ITEMS ? count : NEO_DEBUG_MAX_COLLECTION_ITEMS;
				out.children.reserve(childCount + (count > childCount ? 1 : 0));
				CollectionIterator iterator = pVar->_tbl->FirstNode();
				for (int i = 0; i < childCount && iterator._pTableNode != nullptr; ++i)
				{
					MapNode* node = iterator._pTableNode;
					NeoDebugVariable child;
					if (node->key.GetType() == VAR_STRING && node->key._str != nullptr)
						child.name = "[\"" + node->key._str->_str + "\"]";
					else
					{
						NeoDebugVariable key;
						NeoDebugFormatValue(&node->key, key, collectionDepth + 1);
						child.name = "[" + key.value + "]";
					}
					NeoDebugFormatValue(&node->value, child, collectionDepth + 1);
					out.children.push_back(std::move(child));
					pVar->_tbl->NextNode(iterator);
				}
				if (count > childCount)
				{
					NeoDebugVariable more;
					more.name = "...";
					more.type = "map";
					more.value = std::to_string(count - childCount) + " more items";
					out.children.push_back(std::move(more));
				}
			}
		}
		out.value = buf;
		break;
	case VAR_LIST:
		out.type = "list";
		{
			const int count = pVar->_lst ? pVar->_lst->GetCount() : 0;
			snprintf(buf, sizeof(buf), "list(%d)", count);
			if (pVar->_lst != nullptr && collectionDepth < NEO_DEBUG_MAX_COLLECTION_DEPTH)
			{
				const int childCount = count < NEO_DEBUG_MAX_COLLECTION_ITEMS ? count : NEO_DEBUG_MAX_COLLECTION_ITEMS;
				out.children.reserve(childCount + (count > childCount ? 1 : 0));
				for (int i = 0; i < childCount; ++i)
				{
					VarInfo* pChildValue = pVar->_lst->GetValue(i);
					if (pChildValue == nullptr)
						continue;

					NeoDebugVariable child;
					child.name = "[" + std::to_string(i) + "]";
					NeoDebugFormatValue(pChildValue, child, collectionDepth + 1);
					out.children.push_back(std::move(child));
				}
				if (count > childCount)
				{
					NeoDebugVariable more;
					more.name = "...";
					more.type = "list";
					more.value = std::to_string(count - childCount) + " more items";
					out.children.push_back(std::move(more));
				}
			}
		}
		out.value = buf;
		break;
	case VAR_SET:
		out.type = "set";
		snprintf(buf, sizeof(buf), "set(%d)", pVar->_set ? pVar->_set->GetCount() : 0);
		out.value = buf;
		break;
	case VAR_COROUTINE:
		out.type = "coroutine";
		out.value = "coroutine";
		break;
	case VAR_MODULE:
		out.type = "module";
		out.value = "module";
		break;
	case VAR_ASYNC:
		out.type = "async";
		out.value = "async";
		break;
	case VAR_ITERATOR:
		out.type = "iterator";
		out.value = "iterator";
		break;
	default:
		out.type = "unknown";
		out.value = "unknown";
		break;
	}
}
void CNeoVMWorker::DebugGetStackTrace(std::vector<NeoDebugStackFrame>& frames)
{
	frames.clear();

	int codeOffset = m_bDebugPaused ? m_sDebugLocation.opIndex * (int)sizeof(SVMOperation) : GetCodeptr();
	if (codeOffset < 0)
		codeOffset = 0;

	NeoDebugStackFrame cur;
	cur.frameId = 0;
	cur.functionId = GetFunctionIndexFromCodeOffset(codeOffset);
	if (cur.functionId >= 0)
	{
		auto itName = m_sDebugFunctionNames.find(cur.functionId);
		if (itName != m_sDebugFunctionNames.end())
			cur.functionName = itName->second;
	}
	cur.opIndex = codeOffset / (int)sizeof(SVMOperation);
	cur.file = (cur.opIndex >= 0 && cur.opIndex < (int)_DebugData.size()) ? _DebugData[cur.opIndex]._fileseq : -1;
	cur.line = GetDebugLine(cur.opIndex);
	cur.stackBase = _iSP_Vars;
	if (cur.functionId >= 0)
	{
		SFunctionTable& fun = m_sFunctionPtr[cur.functionId];
		cur.argsCount = fun._argsCount;
		cur.localCount = fun._localVarCount;
		cur.tempCount = fun._localTempMax;
	}
	frames.push_back(cur);

	if (m_pCallStack == nullptr)
		return;

	for (int i = (int)m_pCallStack->size() - 1; i >= 0; --i)
	{
		SCallStack& cs = (*m_pCallStack)[i];
		NeoDebugStackFrame frame;
		frame.frameId = (int)frames.size();
		frame.stackBase = cs._iSP_Vars;
		frame.opIndex = cs._iReturnOffset / (int)sizeof(SVMOperation);
		frame.functionId = GetFunctionIndexFromCodeOffset(cs._iReturnOffset);
		if (frame.functionId >= 0)
		{
			auto itName = m_sDebugFunctionNames.find(frame.functionId);
			if (itName != m_sDebugFunctionNames.end())
				frame.functionName = itName->second;
		}
		frame.file = (frame.opIndex >= 0 && frame.opIndex < (int)_DebugData.size()) ? _DebugData[frame.opIndex]._fileseq : -1;
		frame.line = GetDebugLine(frame.opIndex);
		if (frame.functionId >= 0)
		{
			SFunctionTable& fun = m_sFunctionPtr[frame.functionId];
			frame.argsCount = fun._argsCount;
			frame.localCount = fun._localVarCount;
			frame.tempCount = fun._localTempMax;
		}
		frames.push_back(frame);
	}
}
void CNeoVMWorker::DebugGetFrameVariables(int frameId, std::vector<NeoDebugVariable>& vars)
{
	vars.clear();
	std::vector<NeoDebugStackFrame> frames;
	DebugGetStackTrace(frames);
	if (frameId < 0 || frameId >= (int)frames.size())
		return;

	const NeoDebugStackFrame& frame = frames[frameId];
	int base = frame.stackBase;
	int total = 1 + frame.argsCount + frame.localCount + frame.tempCount;
	auto itFunNames = m_sDebugVarNames.find(frame.functionId);
	bool hasDebugVarNames = itFunNames != m_sDebugVarNames.end();
	for (int i = 0; i < total; ++i)
	{
		int stackIndex = base + i;
		if (stackIndex < 0 || stackIndex >= (int)m_pVarStack_Base->size())
			continue;

		NeoDebugVariable var;
		if (i == 0)
			var.name = "return";
		else
		{
			if (hasDebugVarNames)
			{
				auto itName = itFunNames->second.find(i);
				if (itName != itFunNames->second.end())
					var.name = itName->second;
			}
			if (hasDebugVarNames && var.name.empty())
				continue;
			if (var.name.empty())
			{
				if (i <= frame.argsCount)
					var.name = "arg" + std::to_string(i);
				else if (i <= frame.argsCount + frame.localCount)
					var.name = "local" + std::to_string(i - frame.argsCount);
				else
					var.name = "temp" + std::to_string(i - frame.argsCount - frame.localCount);
			}
		}

		var.stackIndex = stackIndex;
		NeoDebugFormatValue(&(*m_pVarStack_Base)[stackIndex], var);
		if (i == 0 && var.type == "none")
			continue;
		vars.push_back(var);
	}
	if (frameId == 0)
	{
		for (auto it = m_sDebugGlobalNames.begin(); it != m_sDebugGlobalNames.end(); ++it)
		{
			int globalIndex = it->first;
			if (globalIndex < 0 || globalIndex >= (int)m_sVarGlobal.size())
				continue;

			NeoDebugVariable var;
			var.name = it->second;
			var.stackIndex = globalIndex;
			NeoDebugFormatValue(&m_sVarGlobal[globalIndex], var);
			vars.push_back(var);
		}
	}
}
bool	CNeoVMWorker::Run()
{
	bool b = true;
#ifdef _WIN32
	try
	{
#endif
		bool debugActive = (m_iDebugSuppressCount == 0) &&
			(m_iDebugBreakCount > 0 ||
			 m_eDebugRunMode != DBG_CONTINUE ||
			 m_bDebugPauseRequested);
		// 최상위 실행/재개는 완전 완료(depth 0)까지. 이 플래그는 최외곽 Run 에만 적용되어야 하므로
		// 즉시 소비(reset)한다 → 실행 중 발생하는 중첩(C++→스크립트) Run 은 현재 깊이에서 복귀.
		bool topExec = m_bTopExec;
		m_bTopExec = false;
		int breakingCallStack = (topExec || debugActive) ? 0 : (int)m_pCallStack->size();
		if(m_iTimeout >= 0)
			b = debugActive ? RunInternal<true, true>(breakingCallStack) : RunInternal<true, false>(breakingCallStack);
		else
			b = debugActive ? RunInternal<false, true>(breakingCallStack) : RunInternal<false, false>(breakingCallStack);
		// handle_ERROR reports the source location to the debugger, but the VM result
		// must remain an error so RunSettle releases this failed execution context.
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
		snprintf(chMsg, _countof(chMsg), "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), _isErrorOPIndex, _lineseq);
#else
		sprintf(chMsg, "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#endif

		GetVM()->_sErrorMsgDetail = std::string(chMsg) + FormatStackTrace(_isErrorOPIndex);
		if (m_pDebugListener || m_iDebugBreakCount > 0 || m_eDebugRunMode != DBG_CONTINUE || m_bDebugPauseRequested)
		{
			if (_isErrorOPIndex >= 0 && _isErrorOPIndex < (int)_DebugData.size())
				StopDebug(_isErrorOPIndex, NEO_DEBUG_STOP_EXCEPTION);
			return false;
		}
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
template<bool TIMEOUT, bool DEBUG>
bool	CNeoVMWorker::RunInternal(int iBreakingCallStack)
{
	if (false == _isInitialized)
		return false;

	if (_iRemainSleep > 0)
	{
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _preClock).count();
		if (elapsed > 0)
			_iRemainSleep -= (int)elapsed;

		_preClock = now;

		if (_iRemainSleep > 0)
			return true;
	}

	m_iBreakingCallStack = iBreakingCallStack;

	//SCallStack callStack;
//	int iTemp;
//	char chMsg[256];

	m_op_process = m_iCheckOpCount;
	if(TIMEOUT)
		_preClock = std::chrono::steady_clock::now();

	VarInfo* pVarTemp = nullptr;
	while (true)
	{
		if(TIMEOUT)
		{
			if (--m_op_process <= 0)
			{
				m_op_process = m_iCheckOpCount;
//				if (isTimeout)
				{
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - _preClock).count();
					if (elapsed >= m_iTimeout || elapsed < 0)
						break;
				}
			}
		}

		if (DEBUG)
		{
			int iOPIndex = int((u8*)_pCodeCurrent - _pCodeBegin) / sizeof(SVMOperation);
			if (CheckDebugStop(iOPIndex))
				return true;
		}

		const SVMOperation& OP = *GetOP();
		switch (OP.op)
		{
		case NOP_MOV:           Move(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_MOVI:          MoveI(GetVarPtrF1(OP), OP.n23); break;
		case NOP_MOV_MINUS:     MoveMinus(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_LOG_NOT:       Var_SetBool(GetVarPtrF1(OP), !GetVarPtr2(OP)->IsTrue()); break;
		case NOP_ADD2:          Add2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_SUB2:          Sub2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_MUL2:          Mul2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_DIV2:          Div2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_PERSENT2:      Per2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_LSHIFT2:       LSh2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_RSHIFT2:       RSh2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_AND2:          And2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_OR2:           Or2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_XOR2:          Xor2(GetVarPtrF1(OP), GetVarPtr2(OP)); break;
		case NOP_VAR_CLEAR:     Var_Release(GetVarPtrF1(OP)); break;
		case NOP_INC:           Inc(GetVarPtrF1(OP)); break;
		case NOP_DEC:           Dec(GetVarPtrF1(OP)); break;
		case NOP_ADD3:          Add3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_SUB3:          Sub3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_MUL3:          Mul3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_DIV3:          Div3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_PERSENT3:      Per3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_LSHIFT3:       LSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_RSHIFT3:       RSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_AND3:          And3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_OR3:           Or3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_XOR3:          Xor3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_GREAT:         Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr2(OP), GetVarPtr3(OP))); break;
		case NOP_GREAT_EQ:      Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr2(OP), GetVarPtr3(OP))); break;
		case NOP_LESS:          Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr3(OP), GetVarPtr2(OP))); break;
		case NOP_LESS_EQ:       Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr3(OP), GetVarPtr2(OP))); break;
		case NOP_EQUAL2:        Var_SetBool(GetVarPtrF1(OP), CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP))); break;
		case NOP_NEQUAL:        Var_SetBool(GetVarPtrF1(OP), !CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP))); break;
		case NOP_AND:           And(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_OR:            Or(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); break;
		case NOP_LOG_AND:       Var_SetBool(GetVarPtrF1(OP), GetVarPtr3(OP)->IsTrue() && GetVarPtr2(OP)->IsTrue()); break;
		case NOP_LOG_OR:        Var_SetBool(GetVarPtrF1(OP), GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()); break;
		case NOP_JMP:           SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_FALSE:     if (false == GetVarPtr2(OP)->IsTrue()) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_TRUE:      if (true == GetVarPtr2(OP)->IsTrue()) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_GREAT:     if (true == CompareGR(GetVarPtr2(OP), GetVarPtr3(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_GREAT_EQ:  if (true == CompareGE(GetVarPtr2(OP), GetVarPtr3(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_LESS:      if (true == CompareGR(GetVarPtr3(OP), GetVarPtr2(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_LESS_EQ:   if (true == CompareGE(GetVarPtr3(OP), GetVarPtr2(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_EQUAL2:    if (true == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_NEQUAL:    if (false == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_AND:       if (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue()) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_OR:        if (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_NAND:      if (false == (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue())) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_NOR:       if (false == (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue())) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_FOR:       if (For(GetVarPtr_L(OP.n2))) SetCodeIncPtr(OP.n1); break;
		case NOP_JMP_FOREACH:   if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP))) SetCodeIncPtr(OP.n1); break;
		case NOP_STR_ADD:       handle_STR_ADD(OP); break;
		case NOP_TOSTRING:      handle_TOSTRING(OP); break;
		case NOP_TOINT:         handle_TOINT(OP); break;
		case NOP_TOFLOAT:       handle_TOFLOAT(OP); break;
		case NOP_TOSIZE:        handle_TOSIZE(OP); break;
		case NOP_GETTYPE:       handle_GETTYPE(OP); break;
		case NOP_SLEEP:
			if (handle_SLEEP(OP)) return true;
			break;
		case NOP_FMOV1:         handle_FMOV1(OP); break;
		case NOP_FMOV2:         handle_FMOV2(OP); break;
		case NOP_CALL:          handle_CALL(OP); break;
		case NOP_PTRCALL:       handle_PTRCALL(OP); break;
		case NOP_PTRCALL2:      handle_PTRCALL2(OP); break;
		case NOP_NATIVECALL:    handle_NATIVECALL(OP); break;
		case NOP_RETURN:
			if (handle_RETURN(OP)) return true;
			break;
		case NOP_TABLE_ALLOC:   handle_TABLE_ALLOC(OP); break;
		case NOP_CLT_READ:      handle_CLT_READ(OP); break;
		case NOP_TABLE_REMOVE:  handle_TABLE_REMOVE(OP); break;
		case NOP_CLT_MOV:       handle_CLT_MOV(OP); break;
		case NOP_TABLE_ADD2:    handle_TABLE_ADD2(OP); break;
		case NOP_TABLE_SUB2:    handle_TABLE_SUB2(OP); break;
		case NOP_TABLE_MUL2:    handle_TABLE_MUL2(OP); break;
		case NOP_TABLE_DIV2:    handle_TABLE_DIV2(OP); break;
		case NOP_TABLE_PERSENT2:handle_TABLE_PERSENT2(OP); break;
		case NOP_LIST_ALLOC:    handle_LIST_ALLOC(OP); break;
		case NOP_LIST_REMOVE:   handle_LIST_REMOVE(OP); break;
		case NOP_VERIFY_TYPE:   handle_VERIFY_TYPE(OP); break;
		case NOP_CHANGE_INT:    handle_CHANGE_INT(OP); break;
		case NOP_YIELD:
			if (handle_YIELD(OP)) return true;
			break;
		case NOP_IDLE:          handle_IDLE(OP); break;
		case NOP_NONE:          handle_NONE(OP); break;
		case NOP_ERROR:
			handle_ERROR(OP);
			return false;
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
	return Start(iFID, _args);
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
	return Start(iID, _args);
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
			if (!EnsureStackRange(0, fun._localAddCount - 1))
				return false;
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
	if (!EnsureStackRange(_iSP_VarsMax, fun._localAddCount - 1))
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

	++m_iDebugSuppressCount;
	Run();
	--m_iDebugSuppressCount;

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
	if (!EnsureStackRange(_iSP_VarsMax, n3))
		return false;
	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	if (_iSP_Vars_Max2 < _iSP_VarsMax + 1 + n3) // ??????? 이렇게 하면 맞나? 흠...
		_iSP_Vars_Max2 = _iSP_VarsMax + 1 + n3;

	if ((func)(this, pUserData, pStr, n3) == false)
	{
		_iSP_Vars = iSave;
		SetStackPointer(_iSP_Vars);
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
				if (!StartCoroutione(argSP_Vars, n3))
					return false;
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
bool CNeoVMWorker::CallDefaultNativeByIndex(int nativeIndex, int n3, VarInfo* pRet)
{
	if (!EnsureStackRange(_iSP_VarsMax, n3))
		return false;
	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	if (_iSP_Vars_Max2 < _iSP_VarsMax + 1 + n3)
		_iSP_Vars_Max2 = _iSP_VarsMax + 1 + n3;

	if (CNeoVMImpl::CallDefaultNativeByIndex(nativeIndex, this, (short)n3) == false)
	{
		_iSP_Vars = iSave;
		SetStackPointer(_iSP_Vars);
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
				if (!StartCoroutione(argSP_Vars, n3))
					return false;
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
	if (!EnsureStackRange(_iSP_VarsMax, 0))
		return false;
	int iSave = _iSP_Vars;
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	if (_iSP_Vars_Max2 < _iSP_VarsMax + 1)
		_iSP_Vars_Max2 = _iSP_VarsMax + 1;

	if ((func)(this, pUserData, pStr, pRet, get) == false)
	{
		_iSP_Vars = iSave;
		SetStackPointer(_iSP_Vars);
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
			if (!StartCoroutione(argSP_Vars, 0))
				return false;
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
	snprintf(ch, _countof(ch), "VerifyType (%s != %s)", GetDataType(p->GetType()).c_str(), GetDataType(t).c_str());
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
	snprintf(ch, _countof(ch), "ChangeNumber (%s != number)", GetDataType(p->GetType()).c_str());
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
	case VAR_INT:
		return "int";
	case VAR_FLOAT:
		return "float";
	case VAR_BOOL:
		return "bool";
	case VAR_NONE:
		return "null";
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
