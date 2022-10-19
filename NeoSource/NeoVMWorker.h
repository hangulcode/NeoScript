#pragma once

#include <time.h>
#include "NeoConfig.h"

enum VAR_TYPE : u8
{
	VAR_NONE,
	VAR_BOOL,
	VAR_INT,
	VAR_FLOAT,	// double
	VAR_STRING,
	VAR_TABLE,
//	VAR_TABLEFUN,
	VAR_FUN,

	VAR_ITERATOR,
};

typedef u8	OpType;
typedef u8	ArgFlag;

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
	NOP_EQUAL2,	// ==
	NOP_NEQUAL,	// !=
	NOP_AND,	// &&
	NOP_OR,		// ||

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
	NOP_JMP_FOREACH,// foreach

	NOP_STR_ADD,	// ..

	NOP_TOSTRING,
	NOP_TOINT,
	NOP_TOFLOAT,
	NOP_TOSIZE,
	NOP_GETTYPE,
	NOP_SLEEP,


	NOP_CALL,
	NOP_PTRCALL,
	NOP_RETURN,
	NOP_FUNEND,

	NOP_TABLE_ALLOC,
	NOP_TABLE_READ,
	NOP_TABLE_REMOVE,
	NOP_TABLE_MOV,
	NOP_TABLE_ADD2,
	NOP_TABLE_SUB2,
	NOP_TABLE_MUL2,
	NOP_TABLE_DIV2,
	NOP_TABLE_PERSENT2,

	NOP_NONE,
	NOP_MAX,
}; // Operation length

#define CODE_TO_NOP(op) (eNOperation)(op)// >> 2)
#define CODE_TO_LEN(op) 3 //(op & 0x03)

#pragma pack(1)
struct SVMOperation
{
	eNOperation	  op;
	u8		argFlag;
	short	n1, n2, n3;
};
#pragma pack()

struct VarInfo;
struct SVarWrapper;
class CNeoVMWorker;
struct FunctionPtr;
typedef int(*Neo_CFunction) (CNeoVMWorker *N, FunctionPtr* pFun, short args);
typedef bool(*Neo_NativeFunction) (CNeoVMWorker *N, void* pUserData, const std::string& fun, short args);

struct StringInfo
{
	std::string _str;
	int	_StringID;
	int _refCount;
};

//struct SNeoMeta
//{
//	virtual  bool GetFunction(std::string& fname, SVarWrapper*) =0;
//	virtual  bool GetVariable(std::string& fname, SVarWrapper*) =0;
//};



struct FunctionPtr
{
	u8							_argCount;
	Neo_CFunction				_fn;
	void*						_func;
};


struct TableInfo;
struct TableBocket3;


#pragma pack(1)
struct TableIterator
{
	u16			_hash1;
	u16			_hash2;
	u16			_hash3;

	u16			_offset;
	TableBocket3*	_bocket;
};
#pragma pack()

#pragma pack(1)
struct FunctionPtrNative
{
	Neo_NativeFunction			_func;
};
struct VarInfo
{
private:
	VAR_TYPE	_type;
public:
	union
	{
		bool		_bl;
		StringInfo* _str;
		TableInfo*	_tbl;
		int			_int;
		double		_float;
//		FunctionPtrNative _fun;
		int			_fun_index;
		TableIterator	_it;
	};
	//std::map<std::string, TableData>::iterator _it;

	VarInfo()
	{
		_type = VAR_NONE;
	}

	inline VAR_TYPE GetType() { return _type; }
	inline void SetType(VAR_TYPE t) { _type = t; }
	inline void ClearType()
	{
		_type = VAR_NONE;
	}
	inline bool IsAllocType()
	{
		return ((_type == VAR_STRING) || (_type == VAR_TABLE));
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


struct SNeoVMHeader
{
	u32		_dwFileType;
	u32		_dwNeoVersion;

	int		_iFunctionCount;
	int		_iStaticVarCount;
	int		_iGlobalVarCount;
	int		_iMainFunctionOffset;
	int		_iCodeSize;
	int		m_iDebugCount;
	int		m_iDebugOffset;

	u32		_dwFlag;
};

#define NEO_HEADER_FLAG_DEBUG		0x00000001

struct SCallStack
{
	int		_iSP_Vars;
	int		_iSP_VarsMax;
	int		_iReturnOffset;
	VarInfo* _pReturnValue;
};

enum FUNCTION_TYPE : u8
{
	FUNT_NORMAL = 0,
	FUNT_IMPORT,
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
	NDF_FUNCTION,

	NDF_TRUE,
	NDF_FALSE,

	NDF_MAX
};

class CNeoVM;
class CNeoVMWorker
{
	friend CNeoVM;
	friend SVarWrapper;
	friend TableInfo;
private:
	u8 *					_pCodeCurrent;
	u8 *					_pCodeBegin;
	int						_iCodeLen;
	//int						_iCodeOffset;


	CNeoVM*					_pVM;
	u32						_idWorker;
	bool					_isSetup = false;
	int						_iRemainSleep = 0;
	clock_t					_preClock;
//	TableInfo*				_pCallTableInfo = NULL;

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

	int						_iSP_Vars;
	int						_iSP_Vars_Max2;
	int						iSP_VarsMax;
	std::vector<VarInfo>	m_sVarStack;
	std::vector<SCallStack>	m_sCallStack;
	std::vector<VarInfo>*	m_pVarGlobal;


	std::vector<VarInfo> _args;
	bool RunFunction(const std::string& funName);

	bool	Setup(int iFunctionID);
	bool	Start(int iFunctionID);
	bool	Run(int iBreakingCallStack = 0);

	inline VarInfo* GetVarPtr1(SVMOperation& OP)
	{
		if (OP.argFlag & 0x4) return &m_sVarStack[_iSP_Vars + OP.n1];
		else return &(*m_pVarGlobal)[OP.n1];
	}
	inline VarInfo* GetVarPtr2(SVMOperation& OP)
	{
		if (OP.argFlag & 0x2) return &m_sVarStack[_iSP_Vars + OP.n2];
		else return &(*m_pVarGlobal)[OP.n2];
	}
	inline VarInfo* GetVarPtr3(SVMOperation& OP)
	{
		if (OP.argFlag & 0x1) return &m_sVarStack[_iSP_Vars + OP.n3];
		else return &(*m_pVarGlobal)[OP.n3];
	}
	inline VarInfo* GetVarPtr_S(short n) { return &m_sVarStack[_iSP_Vars + n]; }
	inline VarInfo* GetVarPtr_G(short n) { return &(*m_pVarGlobal)[n]; }

public:
	void Var_AddRef(VarInfo *d);
	void Var_ReleaseInternal(VarInfo *d);
	inline void Var_Release(VarInfo *d)
	{
		if (d->IsAllocType())
			Var_ReleaseInternal(d);
		else
			d->ClearType();
	}
	void Var_SetNone(VarInfo *d);
private:
	void Var_SetInt(VarInfo *d, int v);
	void Var_SetFloat(VarInfo *d, double v);
	void Var_SetBool(VarInfo *d, bool v);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetStringA(VarInfo *d, const std::string& str);
	void Var_SetTable(VarInfo *d, TableInfo* p);
	void Var_SetFun(VarInfo* d, int fun_index);
//	void Var_SetTableFun(VarInfo* d, FunctionPtrNative fun);


public:
	inline void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
			Var_Release(v1);

		v1->SetType(v2->GetType());
		switch (v2->GetType())
		{
		case VAR_NONE:
			break;
		case VAR_BOOL:
			v1->_bl = v2->_bl;
			break;
		case VAR_INT:
			v1->_int = v2->_int;
			break;
		case VAR_FLOAT:
			v1->_float = v2->_float;
			break;
		case VAR_STRING:
			v1->_str = v2->_str;
			++v1->_str->_refCount;
			break;
		case VAR_TABLE:
			v1->_tbl = v2->_tbl;
			++v1->_tbl->_refCount;
			break;
		//case VAR_TABLEFUN:
		//	v1->_fun = v2->_fun;
		//	break;
		case VAR_FUN:
			v1->_fun_index = v2->_fun_index;
			break;
		default:
			break;
		}
	}
	void Swap(VarInfo* v1, VarInfo* v2);
private:
	void MoveMinus(VarInfo* v1, VarInfo* v2);
	void Add2(VarInfo* r, VarInfo* v2);
	void Sub2(VarInfo* r, VarInfo* v2);
	void Mul2(VarInfo* r, VarInfo* v2);
	void Div2(VarInfo* r, VarInfo* v2);
	void Per2(VarInfo* r, VarInfo* v2);
	void Add(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Sub(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Mul(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Div(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Per(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Inc(VarInfo* v1);
	void Dec(VarInfo* v1);
	bool CompareEQ(VarInfo* v1, VarInfo* v2);
	bool CompareGR(VarInfo* v1, VarInfo* v2);
	bool CompareGE(VarInfo* v1, VarInfo* v2);
	bool ForEach(VarInfo* v1, VarInfo* v2);
	int Sleep(int iTimeout, VarInfo* v1);
	void Call(int n1, int n2, VarInfo* pReturnValue = NULL);
	bool Call_MetaTable(VarInfo* pTable, std::string&, VarInfo* r, VarInfo* a, VarInfo* b);
	bool Call_MetaTable2(VarInfo* pTable, std::string&, VarInfo* a, VarInfo* b);

	std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	double ToFloat(VarInfo* v1);
	int ToSize(VarInfo* v1);
	VarInfo* GetType(VarInfo* v1);

	void TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRemove(VarInfo *pTable, VarInfo *pArray);
	VarInfo* GetTableItem(VarInfo *pTable, VarInfo *pArray);
	VarInfo* GetTableItemValid(VarInfo *pTable, VarInfo *pArray);

	void TableAdd2(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Add2(p, pValue);
	}
	void TableSub2(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Sub2(p, pValue);
	}
	void TableMul2(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Mul2(p, pValue);
	}
	void TableDiv2(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Div2(p, pValue);
	}
	void TablePer2(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
	{
		VarInfo* p = GetTableItemValid(pTable, pArray);
		if (p) Per2(p, pValue);
	}

	void ClearArgs()
	{
		_args.clear();
	}
	void PushInt(int v)
	{
		VarInfo d;
		d.SetType(VAR_INT);
		d._int = v;
		_args.push_back(d);
	}
	void PushFloat(double v)
	{
		VarInfo d;
		d.SetType(VAR_FLOAT);
		d._float = v;
		_args.push_back(d);
	}
	void PushString(const char* p);
	void PushBool(bool b)
	{
		VarInfo d;
		d.SetType(VAR_BOOL);
		d._bl = b;
		_args.push_back(d);
	}

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
public:
	std::string				_sTempString;
	VarInfo* GetReturnVar() { return &m_sVarStack[_iSP_Vars]; }
//	inline TableInfo* GetCallTableInfo() { return _pCallTableInfo; }

	void GC()
	{
		for (int i = _iSP_Vars + 1; i < _iSP_Vars_Max2; i++)
			Var_Release(&m_sVarStack[i]);
		_iSP_Vars_Max2 = _iSP_Vars;
	}

	VarInfo *GetStack(int idx) { return &m_sVarStack[_iSP_Vars + idx]; }

	template<typename T>
	T read(int idx) { T r; _read(&m_sVarStack[_iSP_Vars + idx], r); return r; }

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

	inline void PushArgs() { }
	template<typename  T, typename ... Types>
	inline void PushArgs(T arg1, Types ... args)
	{
		push(arg1);
		PushArgs(args...);
	}

	template<typename RVal, typename ... Types>
	bool Call(RVal& r, const std::string& funName, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		RunFunction(funName);
		GC();
		_read(&m_sVarStack[_iSP_Vars], r);
		return true;
	}

	template<typename ... Types>
	bool CallN(const std::string& funName, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		RunFunction(funName);
		GC();
		ReturnValue();
		return true;
	}

	template<typename RVal, typename ... Types>
	bool Call_TL(RVal* r, int iFID, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		//RunFunction(funName, );
		if (false == Setup(iFID))
			return false;

		Run();
		if (_isSetup == true)
		{	// yet ... not completed
			GC();
			return false;
		}

		GC();
		_read(&m_sVarStack[_iSP_Vars], *r);
		return true;
	}

	template<typename ... Types>
	bool CallN_TL(int iFID, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		//RunFunction(funName, );
		if (false == Setup(iFID))
			return false;

		Run();
		if (_isSetup == true)
		{	// yet ... not completed
			GC();
			return false;
		}

		GC();
		ReturnValue();
		return true;
	}

	bool testCall(VarInfo** r, int iFID, VarInfo* args[], int argc);


	static void neo_pushcclosureNative(FunctionPtrNative* pOut, Neo_NativeFunction pFun)
	{
		pOut->_func = pFun;
	}
	static void neo_pushcclosure(FunctionPtr* pOut, Neo_CFunction fn, void* pFun)
	{
		pOut->_fn = fn;
		pOut->_func = pFun;
	}

	inline const std::string* GetArg_StlString(int idx) { return PopStlString(&m_sVarStack[_iSP_Vars + idx]); }
	inline const char* GetArg_CharPtr(int idx) { return (const char*)PopString(&m_sVarStack[_iSP_Vars + idx]); }
	inline int GetArg_Int(int idx) { return PopInt(&m_sVarStack[_iSP_Vars + idx]); }
	inline double GetArg_Double(int idx) { return PopFloat(&m_sVarStack[_iSP_Vars + idx]); }
	inline bool GetArg_Bool(int idx) { return PopBool(&m_sVarStack[_iSP_Vars + idx]); }


	inline void	ReturnValue() { Var_Release(&m_sVarStack[_iSP_Vars]);  }
	inline void	ReturnValue(char* p) { Var_SetString(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(const char* p) { Var_SetString(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(char p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned char p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(short p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned short p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(long p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned long p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(int p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(unsigned int p) { Var_SetInt(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(float p) { Var_SetFloat(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(double p) { Var_SetFloat(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(bool p) { Var_SetBool(&m_sVarStack[_iSP_Vars], p); }
	inline void	ReturnValue(long long p) { Var_SetInt(&m_sVarStack[_iSP_Vars], (int)p); }
	inline void	ReturnValue(unsigned long long p) { Var_SetInt(&m_sVarStack[_iSP_Vars], (int)p); }
//	inline void	ReturnValue(SNeoMeta* p) { Var_SetMeta(&m_sVarStack[_iSP_Vars], p); }


	inline void SetError(const char* pErrMsg);
public:
	CNeoVMWorker(CNeoVM* pVM, u32 id, int iStackSize);
	virtual ~CNeoVMWorker();

	inline u32 GetWorkerID() { return _idWorker; }
};

