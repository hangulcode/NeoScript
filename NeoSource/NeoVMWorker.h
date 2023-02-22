#pragma once

#include <time.h>
#include "NeoConfig.h"

enum VAR_TYPE : u8
{
	VAR_NONE,
	VAR_BOOL,
	VAR_INT,
	VAR_FLOAT,	// double
	VAR_FUN,

	VAR_ITERATOR,
	VAR_FUN_NATIVE,

	VAR_STRING,	// Alloc
	VAR_TABLE,
	VAR_LIST,
	VAR_SET,
	VAR_COROUTINE,
	VAR_MODULE,
};

typedef u8	OpType;
typedef u8	ArgFlag;

enum eNOperationSub : u8
{
	eOP_ADD,
	eOP_SUB,
	eOP_MUL,
	eOP_DIV,
	eOP_PER,
};

enum eNOperation : OpType
{
	NOP_MOV,

	NOP_MOV_MINUS,
	NOP_ADD2,
	NOP_SUB2,
	NOP_MUL2,
	NOP_DIV2,
	NOP_PERSENT2,

	NOP_VAR_CLEAR,
	NOP_INC,
	NOP_DEC,

	NOP_ADD3,
	NOP_SUB3,
	NOP_MUL3,
	NOP_DIV3,
	NOP_PERSENT3,

	NOP_GREAT,		// >
	NOP_GREAT_EQ,	// >=
	NOP_LESS,		// <
	NOP_LESS_EQ,	// <=
	NOP_EQUAL2,		// ==
	NOP_NEQUAL,		// !=
	NOP_AND,		// &
	NOP_OR,			// |
	NOP_AND2,		// &&
	NOP_OR2,		// ||

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
	NOP_FUNEND,

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
	NOP_YIELD,

	NOP_NONE,
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
typedef int(*Neo_CFunction) (CNeoVMWorker *N, FunctionPtr* pFun, short args);
typedef bool(*Neo_NativeFunction) (CNeoVMWorker *N, void* pUserData, const std::string& fun, short args);

struct FunctionPtr
{
	u8							_argCount;
	Neo_CFunction				_fn;
	void*						_func;
};


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

struct AllocBase
{
	int _refCount;
};

struct CoroutineInfo : AllocBase
{
//	int	_CoroutineID;
	int	_fun_index;
	COROUTINE_STATE _state;


	int						_iSP_Vars;
	int						_iSP_Vars_Max2;
	int						iSP_VarsMax;
	std::vector<VarInfo>	m_sVarStack;
	std::vector<SCallStack>	m_sCallStack;

	u8 *					_pCodeCurrent;
};

struct StringInfo : AllocBase
{
	std::string _str;
	int	_StringID;
};

//struct SNeoMeta
//{
//	virtual  bool GetFunction(std::string& fname, SVarWrapper*) =0;
//	virtual  bool GetVariable(std::string& fname, SVarWrapper*) =0;
//};





struct TableInfo;
struct TableNode;
struct ListInfo;
struct SetInfo;
struct SetNode;

#pragma pack(1)
struct CollectionIterator
{
	union
	{
		TableNode*	_pTableNode;
		SetNode*	_pSetNode;
		int			_iListOffset;
		int			_iStringOffset;
	};
};
#pragma pack()

#pragma pack(1)
struct FunctionPtrNative
{
	Neo_NativeFunction			_func;
};
struct NeoFunction
{
	// Script Function
	CNeoVMWorker* _pWorker;
	int			_fun_index;

	// C Function
	FunctionPtr _fun;
};
struct VarInfo
{
private:
	VAR_TYPE	_type;
public:
	union
	{
		bool		_bl;
		CoroutineInfo* _cor;
		StringInfo* _str;
		TableInfo*	_tbl;
		ListInfo*	_lst;
		SetInfo*	_set;
		FunctionPtr* _funPtr; // C Native
		int			_int;
		double		_float;
		int			_fun_index;
		CNeoVMWorker*	_module;
		CollectionIterator	_it;
	};

	inline VarInfo() { _type = VAR_NONE; }
	inline VarInfo(VAR_TYPE t) { _type = t; }
	inline VarInfo(int v) { _type = VAR_INT; _int = v; }

	inline VAR_TYPE GetType() { return _type; }
	inline void SetType(VAR_TYPE t) { _type = t; }
	inline void ClearType()
	{
		_type = VAR_NONE;
	}
	inline bool IsAllocType()
	{
		return ((_type >= VAR_STRING));
	}
	inline bool IsTrue()
	{
		if (VAR_BOOL == _type)
			return _bl;
		return false;
	}
};
#pragma pack()

//struct TableData
//{
//	VarInfo	key;
//	VarInfo	value;
//};

#include "NeoVMTable.h"
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
	void SetFloat(double v);
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

	NDF_TRUE,
	NDF_FALSE,

	NDF_SUSPENDED,
	NDF_RUNNING,
	NDF_DEAD,
	NDF_NORMAL,

	NDF_MAX
};

struct neo_DCalllibs;
struct neo_libs;
class CNeoVM;
class CNeoVMWorker : AllocBase
{
	friend		CNeoVM;
	friend		SVarWrapper;
	friend		TableInfo;
	friend		neo_libs;
	friend		neo_DCalllibs;
private:
	u8 *					_pCodeCurrent;
	u8 *					_pCodeBegin;
	int						_iCodeLen;


	CNeoVM*					_pVM;
	u32						_idWorker;
	bool					_isSetup = false;
	int						_iRemainSleep = 0;
	clock_t					_preClock;

	int m_iTimeout = -1;
	int m_iCheckOpCount = 1000;

	void	SetCodeData(u8* p, int sz)
	{
		_pCodeBegin = _pCodeCurrent = p;
		_iCodeLen = sz;
		//_iCodeOffset = 0;
	}

	inline void	GetOP(SVMOperation* op)
	{
		*op = *(SVMOperation*)(_pCodeCurrent);
		_pCodeCurrent += sizeof(SVMOperation);
	}
	int GetDebugLine();
	inline int GetCodeptr() { return (int)(_pCodeCurrent - _pCodeBegin); }
	inline void SetCodePtr(int off) { _pCodeCurrent = _pCodeBegin + off; }
	inline void SetCodeIncPtr(int off) { _pCodeCurrent += off; }

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
	int	_BytesSize;

	inline bool IsDebugInfo() { return (_header._dwFlag & NEO_HEADER_FLAG_DEBUG) != 0; }
	inline int GetBytesSize() { return _BytesSize; }

	int FindFunction(const std::string& name)
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

	int						_iSP_Vars;
	int						_iSP_Vars_Max2;
	int						iSP_VarsMax;
	std::vector<VarInfo>*	m_pVarStack;
	std::vector<SCallStack>* m_pCallStack;

	std::vector<VarInfo>*	m_pVarGlobal;

	std::list< CoroutineInfo*> m_sCoroutines;

	CoroutineInfo m_sDefault;
	CoroutineInfo* m_pCur = NULL;
	CoroutineInfo* m_pRegisterActive = NULL;

	bool RunFunction(int iFID, std::vector<VarInfo>& _args)
	{
		//Start(iFID, _args);
		VarInfo r;
		testCall(iFID, _args.empty() ? NULL : &_args[0], (int)_args.size());
		return true;
	}
	bool RunFunction(const std::string& funName, std::vector<VarInfo>& _args);

	bool	IsMainCoroutine() { return (&m_sDefault == m_pCur); }
	bool	Setup(int iFunctionID, std::vector<VarInfo>& _args);
	bool	Start(int iFunctionID, std::vector<VarInfo>& _args);
	bool	Run(int iBreakingCallStack = 0);
	bool	StopCoroutine();

	//VarInfo _intA1;
	VarInfo _intA2, _intA3;
	VarInfo _funA3;
	inline VarInfo* GetVarPtr1(SVMOperation& OP)
	{
		//if (OP.argFlag & (1 << 5)) { _intA1 = OP.n1; return &_intA1; }
		if (OP.argFlag & (1 << 2)) return &(*m_pVarStack)[_iSP_Vars + OP.n1];
		else return &(*m_pVarGlobal)[OP.n1];
	}
	inline VarInfo* GetVarPtr2(SVMOperation& OP)
	{
		if (OP.argFlag & (1 << 4)) { _intA2._int = OP.n2; return &_intA2; }
		if (OP.argFlag & (1 << 1)) return &(*m_pVarStack)[_iSP_Vars + OP.n2];
		else return &(*m_pVarGlobal)[OP.n2];
	}
	inline VarInfo* GetVarPtr3(SVMOperation& OP)
	{
		if (OP.argFlag & (1 << 3)) { _intA3._int = OP.n3; return &_intA3; }
		if (OP.argFlag & (1 << 0)) return &(*m_pVarStack)[_iSP_Vars + OP.n3];
		else return &(*m_pVarGlobal)[OP.n3];
	}
	inline VarInfo* GetVarPtr_L(short n) { return &(*m_pVarStack)[_iSP_Vars + n]; }
	inline VarInfo* GetVarPtr_G(short n) { return &(*m_pVarGlobal)[n]; }

public:
	void Var_AddRef(VarInfo *d);
	void Var_Release(VarInfo *d);
	void Var_SetNone(VarInfo *d);
	CNeoVM* GetVM() { return _pVM;  }
private:
	void Var_SetInt(VarInfo *d, int v);
	void Var_SetFloat(VarInfo *d, double v);
	void Var_SetBool(VarInfo *d, bool v);
	void Var_SetCoroutine(VarInfo *d, CoroutineInfo* p);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetStringA(VarInfo *d, const std::string& str);
	void Var_SetTable(VarInfo *d, TableInfo* p);
	void Var_SetList(VarInfo *d, ListInfo* p);
	void Var_SetSet(VarInfo *d, SetInfo* p);
	void Var_SetFun(VarInfo* d, int fun_index);
	void Var_SetModule(VarInfo *d, CNeoVMWorker* p);


public:
	inline void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
			Var_Release(v1);
		Move_DestNoRelease(v1, v2);
	}
	inline void MoveI(VarInfo* v1, int v)
	{
		if (v1->IsAllocType())
			Var_Release(v1);

		v1->SetType(VAR_INT);
		v1->_int = v;
	}
	inline static void Move_DestNoRelease(VarInfo* v1, VarInfo* v2)
	{
		v1->SetType(v2->GetType());
		switch (v2->GetType())
		{
		case VAR_NONE: break;
		case VAR_BOOL: v1->_bl = v2->_bl; break;
		case VAR_INT: v1->_int = v2->_int; break;
		case VAR_FLOAT: v1->_float = v2->_float; break;
		case VAR_FUN: v1->_fun_index = v2->_fun_index; break;
		case VAR_FUN_NATIVE: v1->_funPtr = v2->_funPtr; break;
		case VAR_STRING: v1->_str = v2->_str; ++v1->_str->_refCount; break;
		case VAR_TABLE: v1->_tbl = v2->_tbl; ++v1->_tbl->_refCount; break;
		case VAR_LIST: v1->_lst = v2->_lst; ++v1->_lst->_refCount; break;
		case VAR_SET: v1->_set = v2->_set; ++v1->_set->_refCount; break;
		case VAR_COROUTINE: v1->_cor = v2->_cor; ++v1->_cor->_refCount; break;
		case VAR_MODULE: v1->_module = v2->_module; ++v1->_module->_refCount; break;
		default: break;
		}
	}
	void Swap(VarInfo* v1, VarInfo* v2);
private:
	void MoveMinus(VarInfo* v1, VarInfo* v2);
	void Add2(eNOperationSub op, VarInfo* r, VarInfo* v2);
	void Add2(eNOperationSub op, VarInfo* r, int v2);

	void MoveMinusI(VarInfo* v1, int);

	void And(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Or(VarInfo* r, VarInfo* v1, VarInfo* v2);
	
	void Add(eNOperationSub op, VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Add(eNOperationSub op, VarInfo* r, VarInfo* v1, int v2);
	void Add(eNOperationSub op, VarInfo* r, int v1, VarInfo* v2);
	void Add(eNOperationSub op, VarInfo* r, int v1, int v2);

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
	bool Call_MetaTableI(VarInfo* pTable, std::string&, VarInfo* r, VarInfo* a, int b);

	bool CallNative(FunctionPtrNative functionPtrNative, VarInfo* pFunObj, const std::string& fname, int n3);

	static std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	double ToFloat(VarInfo* v1);
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

	void TableAdd2(eNOperationSub op, VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Add2(op, p, pValue);
	}
	void TableAdd2(eNOperationSub op, VarInfo *pTable, VarInfo *pArray, int v)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Add2(op, p, v);
	}
	void TableAdd2(eNOperationSub op, VarInfo *pTable, int Array, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, Array);
		if (p) Add2(op, p, pValue);
	}
	void TableAdd2(eNOperationSub op, VarInfo *pTable, int Array, int v)
	{
		VarInfo* p = GetTableItemValid(pTable, Array);
		if (p) Add2(op, p, v);
	}


	bool VerifyType(VarInfo *p, VAR_TYPE t);


	//void ClearArgs()
	//{
	//	_args.clear();
	//}
	void PushInt(int v)
	{
		VarInfo d;
		d.SetType(VAR_INT);
		d._int = v;
		_args->push_back(d);
	}
	void PushFloat(double v)
	{
		VarInfo d;
		d.SetType(VAR_FLOAT);
		d._float = v;
		_args->push_back(d);
	}
	void PushString(const char* p);
	void PushBool(bool b)
	{
		VarInfo d;
		d.SetType(VAR_BOOL);
		d._bl = b;
		_args->push_back(d);
	}
	void PushNeoFunction(NeoFunction v);

	int PopInt(VarInfo *V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return V->_int;
		case VAR_FLOAT:
			return (int)V->_float;
		default:
			break;
		}
		return -1;
	}
	double PopFloat(VarInfo *V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return V->_int;
		case VAR_FLOAT:
			return V->_float;
		default:
			break;
		}
		return -1;
	}
	const char* PopString(VarInfo *V)
	{
		if (V->GetType() == VAR_STRING)
			return V->_str->_str.c_str();

		return NULL;
	}
	const std::string* PopStlString(VarInfo *V)
	{
		if (V->GetType() == VAR_STRING)
			return &V->_str->_str;

		return NULL;
	}
	bool PopBool(VarInfo *V)
	{
		if (V->GetType() == VAR_BOOL)
			return V->_bl;

		return false;
	}
	NeoFunction PopNeoFunction(VarInfo* V)
	{
		NeoFunction r;
		if (V->GetType() == VAR_FUN)
		{
			r._pWorker = this;
			r._fun_index = V->_fun_index;
		}
		else
		{
			r._pWorker = NULL;
			r._fun_index = -1;
		}
		return r;
	}
public:
	VarInfo* GetReturnVar() { return &(*m_pVarStack)[_iSP_Vars]; }

	void GC()
	{
		for (int i = _iSP_Vars + 1; i < _iSP_Vars_Max2; i++)
			Var_Release(&(*m_pVarStack)[i]);
		_iSP_Vars_Max2 = _iSP_Vars;
	}

	VarInfo *GetStack(int idx) { return &(*m_pVarStack)[_iSP_Vars + idx]; }

	template<typename T>
	T read(int idx) { T r; _read(&(*m_pVarStack)[_iSP_Vars + idx], r); return r; }

	std::vector<VarInfo>* _args = NULL;

	inline void push(char ret) { PushInt(ret); }
	inline void push(unsigned char ret) { PushInt(ret); }
	inline void push(short ret) { PushInt(ret); }
	inline void push(unsigned short ret) { PushInt(ret); }
	inline void push(long ret) { PushInt(ret); }
	inline void push(unsigned long ret) { PushInt(ret); }
	inline void push(int ret) { PushInt(ret); }
	inline void push(unsigned int ret) { PushInt(ret); }
	inline void push(float ret) { PushFloat(ret); }
	inline void push(double ret) { PushFloat((double)ret); }
	inline void push(char* ret) { PushString(ret); }
	inline void push(const char* ret) { PushString(ret); }
	inline void push(bool ret) { PushBool(ret); }
	inline void push(long long ret) { PushInt((int)ret); }
	inline void push(unsigned long long ret) { PushInt((int)ret); }
	inline void push(NeoFunction ret) { PushNeoFunction(ret); }

	inline void		_read(VarInfo *V, std::string*& r) { r = (std::string*)PopStlString(V); }
	inline void		_read(VarInfo *V, char*& r) { r = (char*)PopString(V); }
	inline void		_read(VarInfo *V, const char*& r) { r = PopString(V); }
	inline void		_read(VarInfo *V, char& r) { r = (char)PopInt(V); }
	inline void		_read(VarInfo *V, unsigned char& r) { r = (unsigned char)PopInt(V); }
	inline void		_read(VarInfo *V, short& r) { r = (short)PopInt(V); }
	inline void		_read(VarInfo *V, unsigned short& r) { r = (unsigned short)PopInt(V); }
	inline void		_read(VarInfo *V, long& r) { r = (long)PopInt(V); }
	inline void		_read(VarInfo *V, unsigned long& r) { r = (unsigned long)PopInt(V); }
	inline void		_read(VarInfo *V, int& r) { r = (int)PopInt(V); }
	inline void		_read(VarInfo *V, unsigned int& r) { r = (unsigned int)PopInt(V); }
	inline void		_read(VarInfo *V, float& r) { r = (float)PopFloat(V); }
	inline void		_read(VarInfo *V, double& r) { r = (double)PopFloat(V); }
	inline void		_read(VarInfo *V, bool& r) { r = PopBool(V); }
	inline void		_read(VarInfo *V) {}
	inline void		_read(VarInfo *V, long long& r) { r = (long long)PopInt(V); }
	inline void		_read(VarInfo *V, unsigned long long& r) { r = (unsigned long long)PopInt(V); }
	inline void		_read(VarInfo *V, VarInfo*& r) { r = V; }
	inline void		_read(VarInfo *V, NeoFunction& r) { r = PopNeoFunction(V); }

	inline void PushArgs() { }
	template<typename  T, typename ... Types>
	inline void PushArgs(T arg1, Types ... args)
	{
		push(arg1);
		PushArgs(args...);
	}

	template<typename RVal, typename ... Types>
	bool iCall(RVal& r, int iFID, Types ... args)
	{
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		RunFunction(iFID, args_);
		GC();
		_read(&(*m_pVarStack)[_iSP_Vars], r);
		return true;
	}

	template<typename ... Types>
	bool iCallN(int iFID, Types ... args)
	{
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		RunFunction(iFID, args_);
		GC();
		ReturnValue();
		return true;
	}

	template<typename RVal, typename ... Types>
	bool Call(RVal& r, const std::string& funName, Types ... args)
	{
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		RunFunction(funName, args_);
		GC();
		_read(&(*m_pVarStack)[_iSP_Vars], r);
		return true;
	}

	template<typename ... Types>
	bool CallN(const std::string& funName, Types ... args)
	{
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		RunFunction(funName, args_);
		GC();
		ReturnValue();
		return true;
	}

	template<typename ... Types>
	bool Setup_TL(int iFID, Types ... args)
	{
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		if (false == Setup(iFID, args_))
			return false;
		return true;
	}

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

	VarInfo* GetVar(const std::string& name)
	{
		auto it = m_sImportVars.find(name);
		if (it == m_sImportVars.end())
			return NULL;
		return &m_sVarGlobal[(*it).second];
	}

	VarInfo* testCall(int iFID, VarInfo* args, int argc);
	bool StartCoroutione(int sp, int n3);


	static void neo_pushcclosureNative(FunctionPtrNative* pOut, Neo_NativeFunction pFun)
	{
		pOut->_func = pFun;
	}
	static void neo_pushcclosure(FunctionPtr* pOut, Neo_CFunction fn, void* pFun)
	{
		pOut->_fn = fn;
		pOut->_func = pFun;
	}

	inline const std::string* GetArg_StlString(int idx) { return PopStlString(&(*m_pVarStack)[_iSP_Vars + idx]); }
	inline const char* GetArg_CharPtr(int idx) { return (const char*)PopString(&(*m_pVarStack)[_iSP_Vars + idx]); }
	inline int GetArg_Int(int idx) { return PopInt(&(*m_pVarStack)[_iSP_Vars + idx]); }
	inline double GetArg_Double(int idx) { return PopFloat(&(*m_pVarStack)[_iSP_Vars + idx]); }
	inline bool GetArg_Bool(int idx) { return PopBool(&(*m_pVarStack)[_iSP_Vars + idx]); }


	inline void	ReturnValue() { Var_Release(&(*m_pVarStack)[_iSP_Vars]);  }
	inline void	ReturnValue(VarInfo* p) { Move(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(char* p) { Var_SetString(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(const char* p) { Var_SetString(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(char p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned char p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(short p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned short p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(long p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned long p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(int p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned int p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(float p) { Var_SetFloat(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(double p) { Var_SetFloat(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(bool p) { Var_SetBool(&(*m_pVarStack)[_iSP_Vars], p); }
	inline void	ReturnValue(long long p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], (int)p); }
	inline void	ReturnValue(unsigned long long p) { Var_SetInt(&(*m_pVarStack)[_iSP_Vars], (int)p); }
	inline void	ReturnValue(CoroutineInfo* p) { Var_SetCoroutine(&(*m_pVarStack)[_iSP_Vars], p); }
//	inline void	ReturnValue(SNeoMeta* p) { Var_SetMeta(&(*m_pVarStack)[_iSP_Vars], p); }


	void SetError(const char* pErrMsg);
	void SetErrorUnsupport(const char* pErrMsg, VarInfo* p);
	void SetErrorFormat(const char* pErrMsg, ...);
public:
	CNeoVMWorker(CNeoVM* pVM, u32 id, int iStackSize);
	virtual ~CNeoVMWorker();

	bool Init(void* pBuffer, int iSize, int iStackSize);
	inline u32 GetWorkerID() { return _idWorker; }
};
extern std::string GetDataType(VAR_TYPE t);
