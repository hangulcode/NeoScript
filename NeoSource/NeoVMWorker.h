#pragma once

#include <time.h>
#include "NeoConfig.h"
#include "NeoQueue.h"

typedef u8	OpType;
typedef u8	ArgFlag;

enum eNOperationSub : u8
{
	eOP_ADD,
	eOP_SUB,
	eOP_MUL,
	eOP_DIV,
	eOP_PER,
	eOP_LSH,
	eOP_RSH,
	eOP_AND,
	eOP__OR,
	eOP_XOR,
};

enum eNOperation : OpType
{
	NOP_MOV,
	NOP_MOVI,

	NOP_MOV_MINUS,
	NOP_ADD2,		// +
	NOP_SUB2,		// -
	NOP_MUL2,		// *
	NOP_DIV2,		// /
	NOP_PERSENT2,	// %
	NOP_LSHIFT2,	// <<
	NOP_RSHIFT2,	// >>
	NOP_AND2,		// &
	NOP_OR2,		// |
	NOP_XOR2,		// ^
	//NOP_NOT2,		// ~
	//NOP_LOG_NOT2,	// ! not support ( a != 0x01; if(a != b) ... bit / logic )

	NOP_VAR_CLEAR,
	NOP_INC,
	NOP_DEC,

	NOP_ADD3,
	NOP_SUB3,
	NOP_MUL3,
	NOP_DIV3,
	NOP_PERSENT3,
	NOP_LSHIFT3,
	NOP_RSHIFT3,
	NOP_AND3,
	NOP_OR3,
	NOP_XOR3,
	//NOP_NOT3,

	NOP_GREAT,		// >
	NOP_GREAT_EQ,	// >=
	NOP_LESS,		// <
	NOP_LESS_EQ,	// <=
	NOP_EQUAL2,		// ==
	NOP_NEQUAL,		// !=
	NOP_AND,		// &
	NOP_OR,			// |
	NOP_LOG_AND,	// &&
	NOP_LOG_OR,	// ||

	NOP_JMP,
	NOP_JMP_FALSE,
	NOP_JMP_TRUE,

	NOP_JMP_GREAT,		// >
	NOP_JMP_GREAT_EQ,	// >=
	NOP_JMP_LESS,		// <
	NOP_JMP_LESS_EQ,	// <=
	NOP_JMP_EQUAL2,	// ==
	NOP_JMP_NEQUAL,	// !=
	NOP_JMP_AND,	// &&
	NOP_JMP_OR,		// ||
	NOP_JMP_NAND,	// !(&&)
	NOP_JMP_NOR,	// !(||)
	NOP_JMP_FOR,	// for
	NOP_JMP_FOREACH,// foreach

	NOP_STR_ADD,	// ..

	NOP_TOSTRING,
	NOP_TOINT,
	NOP_TOFLOAT,
	NOP_TOSIZE,
	NOP_GETTYPE,
	NOP_SLEEP, // arg1 unuse

	NOP_FMOV1, // Function Info Load
	NOP_FMOV2, // Function Info Load

	NOP_CALL,
	NOP_PTRCALL,	// Multiple Call
	NOP_PTRCALL2,	// Native Call
	NOP_RETURN,
//	NOP_FUNEND,

	NOP_TABLE_ALLOC,
	NOP_CLT_READ,
	NOP_TABLE_REMOVE,
	NOP_CLT_MOV,	

	NOP_TABLE_ADD2,
	NOP_TABLE_SUB2,
	NOP_TABLE_MUL2,
	NOP_TABLE_DIV2,
	NOP_TABLE_PERSENT2,

	NOP_LIST_ALLOC,
	NOP_LIST_REMOVE,

	NOP_VERIFY_TYPE,
	NOP_CHANGE_INT,
	NOP_YIELD,
	NOP_IDLE,

	NOP_NONE,
	NOP_ERROR,
	NOP_MAX,
}; // Operation length

enum eYield_Type : s16
{
	YILED_RETURN,
};

#define CODE_TO_NOP(op) (eNOperation)(op)// >> 2)
#define CODE_TO_LEN(op) 3 //(op & 0x03)

#pragma pack(1)
struct SVMOperation
{
	eNOperation	  op;
	u8		argFlag;
	short	n1;

	union
	{
		struct
		{
			short	n2;
			short	n3;
		};
		int	n23;
	};
};
#pragma pack()

struct VarInfo;
struct SVarWrapper;
class CNeoVMWorker;
struct FunctionPtr;



struct SCallStack
{
	int		_iSP_Vars;
	int		_iSP_VarsMax;
	int		_iReturnOffset;
	VarInfo* _pReturnValue;
};

enum COROUTINE_STATE
{
	COROUTINE_STATE_SUSPENDED,
	COROUTINE_STATE_RUNNING,
	COROUTINE_STATE_DEAD,
	COROUTINE_STATE_NORMAL,
};
enum COROUTINE_SUB_STATE
{
	COROUTINE_SUB_NORMAL,
	COROUTINE_SUB_START,
	COROUTINE_SUB_CLOSE,
};

enum ASYNC_STATE
{
	ASYNC_READY,
	ASYNC_PENDING,
	ASYNC_COMPLETED,
};

enum ASYNC_COMMAND
{
	ASYNC_GET,
	ASYNC_POST,
	ASYNC_POST_JSON,
};


struct AllocBase
{
	int _refCount;
};

struct CoroutineBase
{
	int			_iSP_Vars;
	int			_iSP_Vars_Max2;
	int			_iSP_VarsMax;
	SVMOperation* _pCodeCurrent;
	void ClearSP()
	{
		_iSP_Vars = 0;
		_iSP_Vars_Max2 = 0;
		_iSP_VarsMax = 0;
	}
};

struct CoroutineInfo : AllocBase
{
//	int	_CoroutineID;
	int	_fun_index;
	COROUTINE_STATE _state;
	COROUTINE_SUB_STATE _sub_state;

	CoroutineBase _info;

	std::vector<VarInfo>	m_sVarStack;
	std::vector<SCallStack>	m_sCallStack;
};

struct StringInfo : AllocBase
{
	std::string _str;
	int	_StringID;
	int _StringLen;
};


struct AsyncInfo : AllocBase
{
	std::string _request;
	std::string _body;
	std::vector< std::pair<std::string, std::string> > _headers;

	int			_fun_index;
	int			_timeout;

	ASYNC_STATE _state;
	ASYNC_COMMAND _type;
	bool		_success;
	std::string _resultValue;

	VarInfo		_LockReferance;
	NeoEvent	_event;
};


struct MapInfo;
struct MapNode;
struct ListInfo;
struct SetInfo;
struct SetNode;



#include "NeoVM.h"
#include "NeoVMMap.h"
#include "NeoVMSet.h"
#include "NeoVMList.h"


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

#define NEO_HEADER_FLAG_DEBUG		0x00000001


enum FUNCTION_TYPE : u8
{
	FUNT_NORMAL = 0,
	FUNT_EXPORT,
	FUNT_ANONYMOUS,
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

enum eNeoDefaultString
{
	NDF_NULL,
	NDF_BOOL,
	NDF_INT,
	NDF_FLOAT,
	NDF_STRING,
	NDF_TABLE,
	NDF_LIST,
	NDF_SET,
	NDF_COROUTINE,
	NDF_FUNCTION,
	NDF_MODULE,
	NDF_ASYNC,

	NDF_TRUE,
	NDF_FALSE,

	NDF_SUSPENDED,
	NDF_RUNNING,
	NDF_DEAD,
	NDF_NORMAL,

	NDF_MAX
};

#if defined(_MSC_VER) && !defined(_DEBUG)
#define NEOS_FORCEINLINE __forceinline
#elif defined(__GNUC__) && __GNUC__ >= 4 && defined(NDEBUG)
#define NEOS_FORCEINLINE __attribute__((always_inline))
#else
#define NEOS_FORCEINLINE
#endif


#ifdef _DEBUG
	#define NEOS_GLOBAL_VAR(idx) &(*m_pVarGlobal)[idx]
#else
	#define NEOS_GLOBAL_VAR(idx) m_pVarGlobal_Pointer + idx
#endif


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

	bool					_isSetup = false;
	int						_iRemainSleep = 0;
	clock_t					_preClock;

	int m_iTimeout = -1;
	int m_iCheckOpCount = NEO_DEFAULT_CHECKOP;
//	int m_op_process = 0;

//	inline void SetCheckTime() { m_op_process = 0; }
	void JumpAsyncMsg();

	void	SetCodeData(u8* p, int sz)
	{
		_pCodeBegin = p;
		_pCodeCurrent = (SVMOperation*)p;
		_iCodeLen = sz;
		//_iCodeOffset = 0;
	}

	inline SVMOperation*	GetOP()
	{
		return _pCodeCurrent++;
	}
	int GetDebugLine(int iOPIndex);
	NEOS_FORCEINLINE int GetCodeptr() { return (int)((u8*)_pCodeCurrent - _pCodeBegin); }
	NEOS_FORCEINLINE void SetCodePtr(int off) { _pCodeCurrent = (SVMOperation*)(_pCodeBegin + off); }
	NEOS_FORCEINLINE void SetCodeIncPtr(int off) { _pCodeCurrent = (SVMOperation*)((u8*)_pCodeCurrent + off); }

	SNeoVMHeader			_header;
//	u8 *					_pCodePtr = NULL;
//	int						_iCodeLen;

	std::map<std::string, int> m_sImExportTable;
	std::map<std::string, int> m_sImportVars;
	std::vector<debug_info>	_DebugData;

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
	std::vector<SCallStack>* m_pCallStack;

	std::vector<VarInfo>*	m_pVarGlobal;
	VarInfo* m_pVarGlobal_Pointer;

	std::list< CoroutineInfo*> m_sCoroutines;

	CoroutineInfo m_sDefault;
	CoroutineInfo* m_pCur = NULL;
	CoroutineInfo* m_pRegisterActive = NULL;

	virtual bool RunFunctionResume(int iFID, std::vector<VarInfo>& _args);
	virtual bool RunFunction(int iFID, std::vector<VarInfo>& _args);
	virtual bool RunFunction(const std::string& funName, std::vector<VarInfo>& _args);

	bool	IsMainCoroutine(CoroutineInfo* p) { return (&m_sDefault == p); }
	virtual bool	Setup(int iFunctionID, std::vector<VarInfo>& _args);
	virtual bool	Start(int iFunctionID, std::vector<VarInfo>& _args);
	virtual bool IsWorking();
	virtual bool	Run(int iBreakingCallStack = 0);

	bool	RunInternal(int iBreakingCallStack);

	bool	StopCoroutine(bool doDead = true);
	void	DeadCoroutine(CoroutineInfo* pCI);

	VarInfo _intA1;
	VarInfo _intA2, _intA3;
	VarInfo _funA3;
	NEOS_FORCEINLINE VarInfo* GetVarPtr1Safe(const SVMOperation& OP)
	{
//		if (OP.argFlag & (1 << 5)) { _intA1 = OP.n1; return &_intA1; }
		if (OP.argFlag & (1 << 2)) return m_pVarStack_Pointer + OP.n1;
		else return NEOS_GLOBAL_VAR(OP.n1);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtrF1(const SVMOperation& OP)
	{
		//if (OP.argFlag & (1 << 5)) { _intA1 = OP.n1; return &_intA1; }
		if (OP.argFlag & (1 << 2)) return m_pVarStack_Pointer + OP.n1;
		else return NEOS_GLOBAL_VAR(OP.n1);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr2(const SVMOperation& OP)
	{
//		if (OP.argFlag & (1 << 4)) { _intA2._int = OP.n2; return &_intA2; }
		if (OP.argFlag & (1 << 1)) return m_pVarStack_Pointer + OP.n2;
		else return NEOS_GLOBAL_VAR(OP.n2);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr3(const SVMOperation& OP)
	{
//		if (OP.argFlag & (1 << 3)) { _intA3._int = OP.n3; return &_intA3; }
		if (OP.argFlag & (1 << 0)) return m_pVarStack_Pointer + OP.n3;
		else return NEOS_GLOBAL_VAR(OP.n3);
	}
	NEOS_FORCEINLINE VarInfo* GetVarPtr_L(short n) { return m_pVarStack_Pointer + n; }
	NEOS_FORCEINLINE VarInfo* GetVarPtr_G(short n) { return NEOS_GLOBAL_VAR(n); }

	NEOS_FORCEINLINE void SetStackPointer(short n) { m_pVarStack_Pointer = &(*m_pVarStack_Base)[n]; }
public:
	inline CNeoVMImpl* GetVM() { return (CNeoVMImpl*)_pVM;  }
	virtual void SetTimeout(int iTimeout, int iCheckOpCount) {
		m_iTimeout = iTimeout;
		m_iCheckOpCount = iCheckOpCount;
	}
	virtual bool BindWorkerFunction(const std::string& funName);
private:



public:
	virtual void Var_Move(VarInfo* v1, VarInfo* v2)
	{
		Move(v1, v2);
	}
	NEOS_FORCEINLINE void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
			Var_Release(v1);
		INeoVM::Move_DestNoRelease(v1, v2);
	}
	NEOS_FORCEINLINE void MoveI(VarInfo* v1, int v)
	{
		if (v1->IsAllocType())
			Var_Release(v1);

		v1->SetType(VAR_INT);
		v1->_int = v;
	}

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

	bool CallNative(FunctionPtrNative functionPtrNative, void* pUserData, const std::string& fname, int n3);
	bool PropertyNative(FunctionPtrNative functionPtrNative, void* pUserData, const std::string& fname, VarInfo* pRet, bool get);

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
	virtual bool ChangeVarType(VarInfo* p, VAR_TYPE type, int capa);

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
		if (_isSetup == false)
			return true;

		Run();
		if (_isSetup == true)
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

	bool Init(void* pBuffer, int iSize, int iStackSize);
	
};
extern std::string GetDataType(VAR_TYPE t);
