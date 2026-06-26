#pragma once

#include <chrono>

#include "NeoVMInternal.h"

#include "NeoVMImpl.h"



namespace NeoScript
{

struct SNeoVMHeader
{
	u32		_dwFileType;
	u32		_dwNeoVersion;

	int		_iFunctionCount;
	int		_iStaticVarCount;
	int		_iGlobalVarCount;
	int		_iExportVarCount;

	int		_iMainFunctionOffset;
	int		_iCodeSize;
	int		m_iDebugCount;
	int		m_iDebugOffset;

	u32		_dwFlag;
};

#define NEO_HEADER_FLAG_DEBUG				0x00000001
#define NEO_HEADER_FLAG_SINGLE_PRECISION	0x00000002


enum FUNCTION_TYPE : u8
{
	FUNT_NORMAL = 0,
	FUNT_EXPORT,
	FUNT_ANONYMOUS,
	FUNT_BUILT_IN,
};


struct SFunctionTable
{
	int					_codePtr;
	short				_argsCount;
	short				_localTempMax;
	int					_localVarCount;
	int					_localAddCount; // No Save
	FUNCTION_TYPE		_funType;
	FunctionPtr			_fun;
};

//struct SNeoFunLib
//{
//	std::string pName;
//	FunctionPtrNative fn;
//};

struct SVarWrapper
{
	VarInfo* _var;
	CNeoVMWorker*	_vmw;
	inline SVarWrapper(CNeoVMWorker* p, VarInfo* var) { _var = var; _vmw = p; }

	void SetNone();
	void SetInt(int v);
	void SetFloat(NS_FLOAT v);
	void SetBool(bool v);
	void SetString(const char* str);
//	void SetTableFun(FunctionPtrNative fun);
};


#define NEOS_OP_CALL_NORESULT	(1 << 7) // 0x80


#ifdef _DEBUG
	#define NEOS_GLOBAL_VAR(idx) &(*m_pVarGlobal)[idx]
#else
	#define NEOS_GLOBAL_VAR(idx) m_pVarGlobal_Pointer + idx
#endif

class mRND 
{
public:
	void seed(unsigned int s) {
		_seed = s;
	}
	int rnd() // 0 ~ 0x7fff
	{
		_seed = (a * _seed + c) % m;
		return (_seed >> 16);
	}
	mRND() {}
protected:
	int a = 214013;
	int c = 2531011;
	unsigned int m = 2147483648;
	unsigned int _seed = 0;
};


struct neo_DCalllibs;
struct neo_libs;
class CNeoVMImpl;
class CNeoVMWorker : public INeoVMWorker, public AllocBase, public CoroutineBase
{
	friend		CNeoVMImpl;
	friend		SVarWrapper;
	friend		MapInfo;
	friend		neo_libs;
	friend		neo_DCalllibs;
private:
	u8 *					_pCodeBegin;
	int						_iCodeLen;

	int					_isErrorOPIndex = 0;

	bool					_isInitialized = false;
	int						_iRemainSleep = 0;
	std::chrono::steady_clock::time_point _preClock;

	int m_iTimeout = -1;
	int m_iCheckOpCount = NEO_DEFAULT_CHECKOP;
	int m_op_process = 0;
	int m_iBreakingCallStack = 0;

    enum EDebugRunMode
    {
        DBG_CONTINUE,
        DBG_STEP_INTO,
        DBG_STEP_OVER,
        DBG_STEP_OUT,
        DBG_PAUSED,
    };

    INeoVMDebugListener* m_pDebugListener = nullptr;
    std::vector<u8> m_sDebugBreakLineBits;
    std::vector<std::vector<u8>> m_sDebugBreakLocationBits;
    int m_iDebugBreakCount = 0;
    NeoDebugLocation m_sDebugLocation;
    EDebugRunMode m_eDebugRunMode = DBG_CONTINUE;
    bool m_bDebugPauseRequested = false;
    bool m_bDebugPaused = false;
    int m_iDebugSkipFile = -1;
    int m_iDebugSkipLine = -1;
    int m_iDebugSkipOpIndex = -1;
    int m_iDebugStepDepth = -1;
    int m_iDebugSuppressCount = 0;

    void ClearDebugBreakpoints();
    void SetDebugBreakLineBit(std::vector<u8>& bits, int line);
    bool IsDebugBreakLineBit(const std::vector<u8>& bits, int line) const;
    bool IsDebugBreakpoint(int file, int line) const;


//	inline void SetCheckTime() { m_op_process = 0; }
	void JumpAsyncMsg();

	void	SetCodeData(u8* p, int sz)
	{
		_pCodeBegin = p;
		_pCodeCurrent = (SVMOperation*)p;
		_iCodeLen = sz;
		//_iCodeOffset = 0;
	}

	NEOS_FORCEINLINE SVMOperation*	GetOP()
	{
		return _pCodeCurrent++;
	}
	int GetDebugLine(int iOPIndex);
    bool CheckDebugStop(int iOPIndex);
    void StopDebug(int iOPIndex, NeoDebugStopReason reason);
    int GetFunctionIndexFromCodeOffset(int codeOffset);
	NEOS_FORCEINLINE int GetCodeptr() { return (int)((u8*)_pCodeCurrent - _pCodeBegin); }
	NEOS_FORCEINLINE void SetCodePtr(int off) { _pCodeCurrent = (SVMOperation*)(_pCodeBegin + off); }
	NEOS_FORCEINLINE void SetCodeIncPtr(int off) { _pCodeCurrent = (SVMOperation*)((u8*)_pCodeCurrent + off); }

	SNeoVMHeader			_header;
//	u8 *					_pCodePtr = NULL;
//	int						_iCodeLen;

	std::map<std::string, int> m_sImExportTable;
	std::map<std::string, int> m_sImportVars;
	std::vector<debug_info>	_DebugData;
	std::map<int, std::map<int, std::string>> m_sDebugVarNames;
	std::map<int, std::string> m_sDebugGlobalNames;
	std::map<int, std::string> m_sDebugFunctionNames;

	//void	SetCodeData(u8* p, int sz)
	//{
	//	_pCodePtr = p;
	//	_iCodeLen = sz;
	//}

	std::vector<VarInfo>	m_sVarGlobal;
	std::vector<SFunctionTable> m_sFunctionPtr;

	inline bool IsDebugInfo() { return (_header._dwFlag & NEO_HEADER_FLAG_DEBUG) != 0; }

	virtual int FindFunction(const std::string& name)
	{
		auto it = m_sImExportTable.find(name);
		if (it == m_sImExportTable.end())
			return -1;
		return (*it).second;
	}
	bool SetFunction(int iFID, FunctionPtr& fun, int argCount)
	{
		fun._argCount = argCount;
		if (m_sFunctionPtr[iFID]._argsCount != argCount)
			return false;

		m_sFunctionPtr[iFID]._fun = fun;
		return true;
	}

	std::vector<VarInfo>*	m_pVarStack_Base;
	VarInfo*				m_pVarStack_Pointer;
	SimpleVector<SCallStack>* m_pCallStack;

	std::vector<VarInfo>*	m_pVarGlobal;
	VarInfo* m_pVarGlobal_Pointer;

	std::list< CoroutineInfo*> m_sCoroutines;

	CoroutineInfo m_sDefault;
	CoroutineInfo* m_pCur = NULL;
	CoroutineInfo* m_pRegisterActive = NULL;

	bool	Initialize(int iFunctionID, std::vector<VarInfo>& _args);

	virtual bool RunFunctionResume(int iFID, std::vector<VarInfo>& _args);
	virtual bool RunFunction(int iFID, std::vector<VarInfo>& _args);
	virtual bool RunFunction(const std::string& funName, std::vector<VarInfo>& _args);
    virtual void DebugSetListener(INeoVMDebugListener* listener);
    virtual void DebugSetBreakpoints(const std::vector<int>& lines);
    virtual void DebugSetBreakpoints(const std::vector<NeoDebugBreakpoint>& breakpoints);
    virtual void DebugContinue();
    virtual void DebugStepInto();
    virtual void DebugStepOver();
    virtual void DebugStepOut();
    virtual void DebugPause();
    virtual bool DebugIsPaused();
    virtual NeoDebugLocation DebugGetLocation();
    virtual void DebugGetStackTrace(std::vector<NeoDebugStackFrame>& frames);
    virtual void DebugGetFrameVariables(int frameId, std::vector<NeoDebugVariable>& vars);
    virtual void DebugGetExecutableLines(std::vector<int>& lines);
    virtual void DebugGetExecutableLocations(std::vector<NeoDebugLocation>& locations);

	bool	IsMainCoroutine(CoroutineInfo* p) { return (&m_sDefault == p); }
	virtual bool	Setup(int iFunctionID, std::vector<VarInfo>& _args);
	virtual bool	Start(int iFunctionID, std::vector<VarInfo>& _args);
	virtual bool IsWorking();
	virtual bool	Run();

	template<bool TIMEOUT, bool DEBUG>
	bool	RunInternal(int iBreakingCallStack);

	bool	StopCoroutine(bool doDead = true);
	void	DeadCoroutine(CoroutineInfo* pCI);

	//VarInfo _intA1;
	//VarInfo _intA2, _intA3;
	VarInfo _funA3;

	NEOS_FORCEINLINE VarInfo* GetVarPtrF1(const SVMOperation& OP)
	{
		if (OP.argFlag & (1 << 2)) return m_pVarStack_Pointer + OP.n1;
		return NEOS_GLOBAL_VAR(OP.n1);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr2(const SVMOperation& OP)
	{
		if (OP.argFlag & (1 << 1)) return m_pVarStack_Pointer + OP.n2;
		return NEOS_GLOBAL_VAR(OP.n2);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr3(const SVMOperation& OP)
	{
		if (OP.argFlag & (1 << 0)) return m_pVarStack_Pointer + OP.n3;
		return NEOS_GLOBAL_VAR(OP.n3);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr_L(short n) { return m_pVarStack_Pointer + n; }
	NEOS_FORCEINLINE VarInfo* GetVarPtr_G(short n) { return NEOS_GLOBAL_VAR(n); }

	NEOS_FORCEINLINE void SetStackPointer(int n) { m_pVarStack_Pointer = &(*m_pVarStack_Base)[n]; }
public:
	NEOS_FORCEINLINE CNeoVMImpl* GetVM() { return (CNeoVMImpl*)_pVM;  }
	virtual void SetTimeout(int iTimeout, int iCheckOpCount) {
		m_iTimeout = iTimeout;
		m_iCheckOpCount = iCheckOpCount;
	}
	virtual bool BindWorkerFunction(const std::string& funName);
private:



public:
	mRND m_sRand;

	virtual void Var_Move(VarInfo* v1, VarInfo* v2)
	{
		Move(v1, v2);
	}
	void Move(VarInfo* v1, VarInfo* v2);
	void MoveI(VarInfo* v1, int v);


	void Swap(VarInfo* v1, VarInfo* v2);
private:
	void MoveMinus(VarInfo* v1, VarInfo* v2);
//	void Add2(eNOperationSub op, VarInfo* r, VarInfo* v2);

	void Add2(VarInfo* r, VarInfo* v2);
	void Sub2(VarInfo* r, VarInfo* v2);
	void Mul2(VarInfo* r, VarInfo* v2);
	void Div2(VarInfo* r, VarInfo* v2);
	void Per2(VarInfo* r, VarInfo* v2);
	void LSh2(VarInfo* r, VarInfo* v2);
	void RSh2(VarInfo* r, VarInfo* v2);
	void And2(VarInfo* r, VarInfo* v2);
	void Or2 (VarInfo* r, VarInfo* v2);
	void Xor2(VarInfo* r, VarInfo* v2);

	void MoveMinusI(VarInfo* v1, int);

	void And(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Or(VarInfo* r, VarInfo* v1, VarInfo* v2);

	void Add3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Sub3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Mul3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Div3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Per3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void LSh3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void RSh3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void And3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Or3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Xor3(VarInfo* r, VarInfo* v1, VarInfo* v2);

//	void Add(eNOperationSub op, VarInfo* r, VarInfo* v1, int v2);
//	void Add(eNOperationSub op, VarInfo* r, int v1, VarInfo* v2);
//	void Add(eNOperationSub op, VarInfo* r, int v1, int v2);

	void Inc(VarInfo* v1);
	void Dec(VarInfo* v1);
	bool CompareEQ(VarInfo* v1, VarInfo* v2);
	bool CompareGR(VarInfo* v1, VarInfo* v2);
	bool CompareGE(VarInfo* v1, VarInfo* v2);
	bool For(VarInfo* v1);
	bool ForEach(VarInfo* v1, VarInfo* v2);
	int Sleep(int iTimeout, VarInfo* v1);
	void Call(FunctionPtr* fun, int n2, VarInfo* pReturnValue = NULL);
	void Call(int n1, int n2, VarInfo* pReturnValue = NULL);
	bool Call_MetaTable(VarInfo* pTable, std::string&, VarInfo* r, VarInfo* a, VarInfo* b);
	bool Call_MetaTable2(VarInfo* pTable, std::string&, VarInfo* a, VarInfo* b);
//	bool Call_MetaTableI(VarInfo* pTable, std::string&, VarInfo* r, VarInfo* a, int b);

	bool CallNative(FunctionPtrNative functionPtrNative, void* pUserData, StringInfo *pStr, int n3, VarInfo* pRet = nullptr);
	bool PropertyNative(FunctionPtrNative functionPtrNative, void* pUserData, StringInfo* pStr, VarInfo* pRet, bool get);

	static std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	NS_FLOAT ToFloat(VarInfo* v1);
	int ToSize(VarInfo* v1);
	VarInfo* GetType(VarInfo* v1);

	void CltInsert(VarInfo *pClt, VarInfo *pArray, VarInfo *pValue);
	void CltInsert(VarInfo *pClt, int array, VarInfo *v);
	void CltInsert(VarInfo *pClt, VarInfo *pArray, int v);
	void CltInsert(VarInfo *pClt, int key, int v);
	void CltRead(VarInfo *pClt, VarInfo *pArray, VarInfo *pValue);
	void TableRemove(VarInfo *pTable, VarInfo *pArray);
	VarInfo* GetTableItem(VarInfo *pTable, VarInfo *pArray);
	VarInfo* GetTableItemValid(VarInfo *pTable, VarInfo *pArray);
	VarInfo* GetTableItemValid(VarInfo *pTable, int Array);

	//void TableAdd2(eNOperationSub op, VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	//{
	//	VarInfo* p = GetTableItemValid(pTable, pArray);
	//	if (p) Add2(op, p, pValue);
	//}
	//void TableAdd2(eNOperationSub op, VarInfo *pTable, VarInfo *pArray, int v)
	//{
	//	VarInfo* p = GetTableItemValid(pTable, pArray);
	//	VarInfo temp(v);
	//	if (p) Add2(op, p, &temp);
	//}
	//void TableAdd2(eNOperationSub op, VarInfo *pTable, int Array, VarInfo *pValue)
	//{
	//	VarInfo* p = GetTableItemValid(pTable, Array);
	//	if (p) Add2(op, p, pValue);
	//}
	//void TableAdd2(eNOperationSub op, VarInfo *pTable, int Array, int v)
	//{
	//	VarInfo* p = GetTableItemValid(pTable, Array);
	//	VarInfo temp(v);
	//	if (p) Add2(op, p, &temp);
	//}


	bool VerifyType(VarInfo *p, VAR_TYPE t);
	bool ChangeNumber(VarInfo* p);


	//void ClearArgs()
	//{
	//	_args.clear();
	//}

public:
//	virtual VarInfo* GetReturnVar() { return &(*m_pVarStack_Base)[_iSP_Vars]; }
	virtual VarInfo* GetReturnVar() { return m_pVarStack_Pointer; }
	virtual VarInfo* GetStackVar(int idx){ return GetStack (idx); }
	virtual bool ResetVarType(VarInfo* p, VAR_TYPE type, int capa);

	virtual void GC()
	{
		for (int i = _iSP_Vars + 1; i < _iSP_Vars_Max2; i++)
			Var_Release(&(*m_pVarStack_Base)[i]);
		_iSP_Vars_Max2 = _iSP_Vars;
	}

	NEOS_FORCEINLINE VarInfo *GetStack(int idx) { return m_pVarStack_Pointer + idx; }
	NEOS_FORCEINLINE VarInfo* GetStackFromBase(int idx) { return &(*m_pVarStack_Base)[idx]; }

	template<typename T>
	T read(int idx) { T r; _read(m_pVarStack_Pointer + idx, r); return r; }






	bool CallN_TL()
	{
		if (_iSP_Vars == _iSP_VarsMax)
			return true;

		Run();
		if (_iSP_Vars != _iSP_VarsMax)
		{	// yet ... not completed
			//GC();
			return false;
		}

		//GC();
		//ReturnValue();
		return true;
	}

	virtual VarInfo* GetVar(const std::string& name)
	{
		auto it = m_sImportVars.find(name);
		if (it == m_sImportVars.end())
			return NULL;
		return &m_sVarGlobal[(*it).second];
	}

	VarInfo* testCall(int iFID, VarInfo* args, int argc);
	bool StartCoroutione(int argSP_Vars, int n3);

	void SetError(const char* pErrMsg);
	void SetErrorUnsupport(const char* pErrMsg, VarInfo* p);
	void SetErrorFormat(const char* pErrMsg, ...);
public:
	CNeoVMWorker(INeoVM* pVM, u32 id, int iStackSize);
	virtual ~CNeoVMWorker();

	bool Init(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, int iStackSize);

	NEOS_FORCEINLINE static void Var_AddRef(VarInfo* d)
	{
		switch (d->GetType())
		{
		case VAR_STRING:
			++d->_str->_refCount;
			break;
		case VAR_MAP:
			++d->_tbl->_refCount;
			break;
		case VAR_LIST:
			++d->_lst->_refCount;
			break;
		case VAR_SET:
			++d->_set->_refCount;
			break;
		case VAR_COROUTINE:
			++d->_cor->_refCount;
			break;
		case VAR_MODULE:
			++((CNeoVMWorker*)(d->_module))->_refCount;
			break;
		case VAR_ASYNC:
			++d->_async->_refCount;
			break;
		default:
			break;
		}
	}


#include "NeoVMWorker_Handlers.inl"

};

extern std::string GetDataType(VAR_TYPE t);
};
