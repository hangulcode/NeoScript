#pragma once

#include <chrono>

#include "NeoVMInternal.h"
#include "NeoVMError.h"

#include "NeoVMProgram.h"
#include "NeoVMImpl.h"



namespace NeoScript
{

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


// NEOS_OP_CALL_NORESULT 는 NeoVMInternal.h 의 argFlag 비트 정의로 이동
// JMP_FOREACH 전용: foreach(var k, v in ...) 2변수 형태. 대상이 list 인지는 런타임에만 알 수
// 있으므로, 컴파일러가 이 플래그만 실어 보내고 미지원 판정/에러는 ForEach 런타임에서 한다.
#define NEOS_OP_FOREACH_TWOVAR	(1 << 6) // 0x40


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
	// 컴파일 이미지(코드/함수테이블/디버그정보/상수)는 프로그램이 소유하고 워커들이 공유한다.
	CNeoVMProgram*			_pProgram = nullptr;
	const u8 *				_pCodeBegin = nullptr;   // = _pProgram->GetCodeBegin()

	int					_isErrorOPIndex = 0;

	bool					_isInitialized = false;
	int						_iRemainSleep = 0;
	// sleep 카운트다운 기준 시각. 슬라이스 타임아웃 측정(_preClock)과 반드시 분리해야 한다 —
	// 같은 변수를 쓰면 실행 루프가 타임아웃용으로 덮어써서 남은 sleep 이 밀린다.
	std::chrono::steady_clock::time_point _sleepClock;
	std::chrono::steady_clock::time_point _preClock;

	int m_iTimeout = -1;
	int m_iCheckOpCount = NEO_DEFAULT_CHECKOP;
	int m_op_process = 0;
	bool m_bSliceExpired = false; // 시간 제한으로 다음 ResumeTop까지 보류됨
	int m_iBreakingCallStack = 0;
	bool m_bTopExec = false;   // 최상위 실행/재개 중(=완료까지 실행)인지
	// std::bad_alloc이 실행 중 발생한 워커는 중간 상태의 안전성을 보장할 수 없다.
	// 이 플래그는 worker-local이며 VM 전체나 다른 인스턴스에는 영향을 주지 않는다.
	bool m_bOutOfMemoryPoisoned = false;
	// 인터프리터 루프(Run) 안인지. 중첩 Run 을 고려해 저장/복원한다.
	// 실행 중에는 실행 컨텍스트를 해제할 수 없으므로 CancelExecution 이 이 플래그로 거부한다.
	bool m_bInRun = false;

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
	bool m_bDebugFaulted = false;
    int m_iDebugSkipFile = -1;
    int m_iDebugSkipLine = -1;
    int m_iDebugSkipOpIndex = -1;
    int m_iDebugStepDepth = -1;
    int m_iDebugSuppressCount = 0;
    // Script A -> native API -> Script B 동기 재진입 범위에서만 증가한다.
    int m_iNativeScriptCallDepth = 0;
    // 이 워커가 요청했고 아직 회수하지 않은 async 수. 소멸 시 고아 판정에 쓴다.
    int _asyncPendingCount = 0;

    void ClearDebugBreakpoints();
    void SetDebugBreakLineBit(std::vector<u8>& bits, int line);
    bool IsDebugBreakLineBit(const std::vector<u8>& bits, int line) const;
    bool IsDebugBreakpoint(int file, int line) const;

//	inline void SetCheckTime() { m_op_process = 0; }
	void JumpAsyncMsg();

	NEOS_FORCEINLINE const SVMOperation*	GetOP()
	{
		return _pCodeCurrent++;
	}
	int GetDebugLine(int iOPIndex);
    bool CheckDebugStop(int iOPIndex);
    void StopDebug(int iOPIndex, NeoDebugStopReason reason);
	void ResetFaultStateForNewExecution();
    int GetFunctionIndexFromCodeOffset(int codeOffset);
	std::string FormatStackTrace(int currentOpIndex);
	NEOS_FORCEINLINE int GetCodeptr() { return (int)((const u8*)_pCodeCurrent - _pCodeBegin); }
	NEOS_FORCEINLINE void SetCodePtr(int off) { _pCodeCurrent = (const SVMOperation*)(_pCodeBegin + off); }
	// 점프 offset 은 op(SVMOperation) 단위. SVMOperation* 포인터 산술로 op 수만큼 이동.
	NEOS_FORCEINLINE void SetCodeIncPtr(int opOff) { _pCodeCurrent += opOff; }

	// 워커 고유 상태 = 전역 변수 슬롯([static | global] 단일 배열).
	// static 구간은 로드 시 프로그램의 상수 서술자에서 이 VM 의 할당자로 실체화한다.
	std::vector<VarInfo>	m_sVarGlobal;

	NEOS_FORCEINLINE const std::vector<SFunctionTable>& Functions() const { return _pProgram->functions; }
	// switch table 은 Program 소유(읽기 전용 공유). 매칭 실패/지원 외 타입이면 default offset
	// 을 돌려주는 것이 정상 동작이다. n1 은 short 저장이지만 인덱스는 부호 없이 해석한다(0~65535).
	// 테이블 인덱스가 범위를 벗어나는 건 손상된 이미지/VM 버그뿐이므로 호출측에서 에러 처리한다.
	NEOS_FORCEINLINE bool TryGetSwitchJumpOffset(u16 tableIndex, VarInfo* pKey, int& outOffset) const
	{
		const std::vector<ProgramSwitchTable>& t = _pProgram->switchTables;
		if ((size_t)tableIndex >= t.size())
			return false;
		outOffset = t[tableIndex].Find(pKey);
		return true;
	}
	NEOS_FORCEINLINE const std::vector<debug_info>& DebugData() const { return _pProgram->debugData; }

	inline bool IsDebugInfo() { return _pProgram->IsDebugInfo(); }

	virtual int FindFunction(const std::string& name)
	{
		return _pProgram->FindFunction(name);
	}

	std::vector<VarInfo>*	m_pVarStack_Base;
	VarInfo*				m_pVarStack_Pointer;
	SimpleVector<SCallStack>* m_pCallStack;

	std::vector<VarInfo>*	m_pVarGlobal;
	VarInfo* m_pVarGlobal_Pointer;

	std::list< CoroutineInfo*> m_sCoroutines;

	NeoExecContextPool* m_pPool = nullptr;      // 실행 컨텍스트 풀(로드 시 주입, 실행 시 대여/반납)
	CoroutineInfo* m_pMainCtx = NULL;           // 현재/정지된 최상위 실행의 컨텍스트. idle 이면 NULL
	CoroutineInfo* m_pCur = NULL;
	CoroutineInfo* m_pRegisterActive = NULL;

	void    BindContext(CoroutineInfo* ctx);            // 컨텍스트를 활성 스택으로 바인딩
	void    CleanupContextVars(CoroutineInfo* ctx, int usedMax);  // 반납 전 VarInfo 참조 정리
	void    ReleaseExecution();                         // 최상위+코루틴 컨텍스트 전부 풀로 반납
	int     RunSettle();                                // Run() 후 완료/정지/에러 판정 (NeoExecStatus)
	void    PoisonOutOfMemory() noexcept { m_bOutOfMemoryPoisoned = true; }

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

	bool	IsMainCoroutine(CoroutineInfo* p) { return (m_pMainCtx == p); }
	virtual bool	Setup(int iFunctionID, std::vector<VarInfo>& _args);
	virtual bool	Run();
	virtual int	ExecuteTop(int iFunctionID, std::vector<VarInfo>& _args);
	virtual int	ResumeTop();
	virtual NeoExecutionState GetExecutionState();
	virtual bool IsOutOfMemoryPoisoned() const { return m_bOutOfMemoryPoisoned; }
	virtual bool IsSuspended();
	virtual NeoHostCallBegin BeginHostCall();
	virtual void EndHostCall(NeoHostCallBegin begin);
	virtual void BeginNestedScriptCall();
	virtual void EndNestedScriptCall();
	virtual int RunHostCall(int iFunctionID, std::vector<VarInfo>& _args);
	virtual bool CancelExecution();

	template<bool TIMEOUT, bool DEBUG>
	bool	RunInternal(int iBreakingCallStack);

	bool	StopCoroutine(bool doDead = true);
	void	DeadCoroutine(CoroutineInfo* pCI);

	//VarInfo _intA1;
	//VarInfo _intA2, _intA3;
	VarInfo _funA3;

	NEOS_FORCEINLINE VarInfo* GetVarPtrF1(const SVMOperation& OP)
	{
		if (OP.argFlag & NEOS_ARG_N1_LOCAL) return GetVarPtr_L(OP.n1);
		return NEOS_GLOBAL_VAR(OP.n1);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr2(const SVMOperation& OP)
	{
		if (OP.argFlag & NEOS_ARG_N2_LOCAL) return GetVarPtr_L(OP.n2);
		return NEOS_GLOBAL_VAR(OP.n2);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr3(const SVMOperation& OP)
	{
		if (OP.argFlag & NEOS_ARG_N3_LOCAL) return GetVarPtr_L(OP.n3);
		return NEOS_GLOBAL_VAR(OP.n3);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr_L(short n) { return m_pVarStack_Pointer + n; }
	NEOS_FORCEINLINE VarInfo* GetVarPtr_G(short n) { return NEOS_GLOBAL_VAR(n); }

	NEOS_FORCEINLINE void SetStackPointer(int n) { m_pVarStack_Pointer = &(*m_pVarStack_Base)[n]; }
	// base부터 lastOffset까지(반환값 슬롯은 offset 0) 접근 가능한지 확인한다.
	bool EnsureStackRange(int base, int lastOffset);
public:
	NEOS_FORCEINLINE CNeoVMImpl* GetVM() { return (CNeoVMImpl*)_pVM;  }
	bool IsNativeScriptCallActive() const { return m_iNativeScriptCallDepth > 0; }
	virtual void SetTimeout(int iTimeout, int iCheckOpCount) {
		m_iTimeout = iTimeout;
		m_iCheckOpCount = iCheckOpCount;
	}
private:



public:
	mRND m_sRand;

	virtual void Var_Move(VarInfo* v1, VarInfo* v2)
	{
		Move(v1, v2);
	}
	void Move(VarInfo* v1, VarInfo* v2);
	void MoveI(VarInfo* v1, int v);
	void MoveF(VarInfo* v1, int bits);


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
	NEOS_NOINLINE void Add3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2);
	NEOS_NOINLINE void Sub3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2);
	NEOS_NOINLINE void Mul3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2);
	NEOS_NOINLINE void Div3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2);
	NEOS_NOINLINE void Add2Rare(VarInfo* r, VarInfo* v2);
	NEOS_NOINLINE void Sub2Rare(VarInfo* r, VarInfo* v2);
	NEOS_NOINLINE void Mul2Rare(VarInfo* r, VarInfo* v2);
	NEOS_NOINLINE void Div2Rare(VarInfo* r, VarInfo* v2);
	void Sub3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Mul3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Div3(VarInfo* r, VarInfo* v1, VarInfo* v2);
	// 벡터 산술: v1 은 VAR_VEC. op 0=+ 1=- 2=* 3=/. v2 는 성분 수가 같은 벡터(성분별) 또는 스칼라(*,/ 만).
	bool VecArith(VarInfo* r, VarInfo* v1, VarInfo* v2, int op);
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
	// 숫자 외 비교는 interpreter switch 안에 복제하지 않는다. 문자열/벡터/오류 처리는
	// 드문 경로라 noinline으로 두어 숫자 비교 opcode의 기계 코드가 안정적으로 유지된다.
	NEOS_NOINLINE bool CompareEQRare(VarInfo* v1, VarInfo* v2);
	NEOS_NOINLINE bool CompareGRRare(VarInfo* v1, VarInfo* v2);
	NEOS_NOINLINE bool CompareGERare(VarInfo* v1, VarInfo* v2);
	bool For(VarInfo* v1);
	bool ForRare(VarInfo* v1, int step);   // step<0 / step==0 (드문 경로, For 에서 분리)
	// bTwoVar: foreach(var k, v in ...) 형태(컴파일러가 op argFlag 로 전달).
	// list 는 2변수 순회를 지원하지 않으므로 런타임에 여기서 에러를 낸다.
	bool ForEach(VarInfo* v1, VarInfo* v2, bool bTwoVar);
	int Sleep(int iTimeout, VarInfo* v1);
	void Call(FunctionPtr* fun, int n2, VarInfo* pReturnValue = NULL);
	void Call(int n1, int n2, VarInfo* pReturnValue = NULL);
	bool Call_MetaTable(VarInfo* pTable, std::string&, VarInfo* r, VarInfo* a, VarInfo* b);
	bool Call_MetaTable2(VarInfo* pTable, std::string&, VarInfo* a, VarInfo* b);
//	bool Call_MetaTableI(VarInfo* pTable, std::string&, VarInfo* r, VarInfo* a, int b);

	bool CallNative(FunctionPtrNative functionPtrNative, void* pUserData, StringInfo *pStr, int n3, VarInfo* pRet = nullptr);
	bool CallDefaultNativeByIndex(int nativeIndex, int n3, VarInfo* pRet = nullptr);
	bool PropertyNative(FunctionPtrNative functionPtrNative, void* pUserData, StringInfo* pStr, VarInfo* pRet, bool get);

	static std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	NS_FLOAT ToFloat(VarInfo* v1);
	int ToSize(VarInfo* v1);
	VarInfo* GetType(VarInfo* v1);

	void CltInsert(VarInfo *pClt, VarInfo *pArray, VarInfo *pValue);
	void CltInsertRare(VarInfo *pClt, VarInfo *pArray, VarInfo *pValue);
	void CltInsert(VarInfo *pClt, int array, VarInfo *v);
	void CltInsert(VarInfo *pClt, VarInfo *pArray, int v);
	void CltInsert(VarInfo *pClt, int key, int v);
	void CltRead(VarInfo *pClt, VarInfo *pArray, VarInfo *pValue);
	void CltReadRare(VarInfo *pClt, VarInfo *pArray, VarInfo *pValue);
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






	virtual VarInfo* GetVar(const std::string& name)
	{
		int idx = _pProgram->FindGlobalVar(name);
		if (idx < 0 || idx >= (int)m_sVarGlobal.size())
			return NULL;
		return &m_sVarGlobal[idx];
	}

	VarInfo* testCall(int iFID, VarInfo* args, int argc);
	bool StartCoroutione(int argSP_Vars, int n3);

	void SetError(const char* pErrMsg);
	void SetErrorUnsupport(const char* pErrMsg, VarInfo* p);
	void SetErrorFormat(const char* pErrMsg, ...);
	// 런타임 에러 테이블(NeoVMError.h) 기반. 새 코드는 이쪽을 쓴다.
	void SetError(ENeoRuntimeError e);
	void SetErrorFormat(int error, ...);
	void SetErrorOperator(const char* op, VarInfo* v1, VarInfo* v2 = nullptr);
public:
	CNeoVMWorker(INeoVM* pVM, u32 id, int iStackSize);
	virtual ~CNeoVMWorker();

	// pProgram 은 호출측이 소유권 지분을 넘기지 않는다. 성공 시 워커가 AddRef 한다.
	bool Init(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, int iStackSize);

	NEOS_FORCEINLINE static void Var_AddRef(VarInfo* d)
	{
		switch (d->GetType())
		{
		case VAR_STRING:
			++d->_str->_refCount;
			break;
		case VAR_VEC:
			// 짝인 Var_ReleaseInternal / Move_DestNoRelease 는 처리하는데 여기만 빠져 있었다.
			// alloc 타입은 하나도 빠짐없이 여기 있어야 한다.
			++d->_vec->_refCount;
			break;
		case VAR_FP_NATIVE:
			++d->_fpNative->_refCount;
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

	// 파괴 재진입 방지와 순환 후보 intrusive FIFO 링크. 전역/실행 컨텍스트가
	// 컨테이너를 들 수 있으므로 worker는 보수적으로 후보 가능으로 초기화한다.
	CycleState<CNeoVMWorker> _cycleState;

};

extern std::string GetDataType(VAR_TYPE t);

// INeoVMWorker 인라인 정의. 호출하는 모든 TU 에 정의가 보여야 한다(파일 상단 주석 참고).
#include "NeoVMWorker_Var.inl"

// NeoLib.cpp 에서도 직접 호출하는 concrete worker fast path. .cpp 전용 .inl 에 두면
// GCC/Clang 비-LTO 빌드에서 inline 정의를 찾지 못해 undefined reference가 난다.
#include "NeoVMWorker_Move.inl"
};
