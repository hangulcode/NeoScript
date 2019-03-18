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
	VAR_TABLEFUN,
};


enum NOP_TYPE : u8
{
	NOP_NONE = 0,
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

	NOP_JMP,
	NOP_JMP_FALSE,
	NOP_JMP_TRUE,

	NOP_CALL,
	NOP_FARCALL,
	NOP_PTRCALL,
	NOP_RETURN,
	NOP_FUNEND,

	NOP_TABLE_ALLOC,
	NOP_TABLE_INSERT,
	NOP_TABLE_READ,
	NOP_TABLE_REMOVE,
};

#pragma pack(1)
struct SVMOperation
{
	NOP_TYPE   op;
	short n1, n2, n3;
};
#pragma pack()

struct VarInfo;
class CNeoVMWorker;
struct FunctionPtr;
typedef int(*Neo_CFunction) (CNeoVMWorker *N, FunctionPtr* pFun, short args);
typedef bool(*Neo_NativeFunction) (CNeoVMWorker *N, short args);

struct StringInfo
{
	std::string _str;
	int	_StringID;
	int _refCount;
};


struct TableInfo
{
	std::map<std::string, VarInfo>	_strMap;
	std::map<int, VarInfo>			_intMap;
	int	_TableID;
	int _refCount;
	void* _pUserData;
};

struct FunctionPtr
{
	u8							_argCount;
	Neo_CFunction				_fn;
	void*						_func;
};


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
		FunctionPtrNative _fun;
	};

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

struct SNeoVMHeader
{
	u32		_dwFileType;
	u32		_dwNeoVersion;

	int		_iFunctionCount;
	int		_iStaticVarCount;
	int		_iGlobalVarCount;
	int		_iMainFunctionOffset;
	int		_iCodeSize;
};

struct SCallStack
{
	int		_iSP_Vars;
	int		_iSP_VarsMax;
	int		_iReturnOffset;
};

enum FUNCTION_TYPE : u8
{
	FUNT_NORMAL = 0,
	FUNT_IMPORT,
	FUNT_EXPORT,
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

struct SNeoFunLib
{
	const char* pName;
	FunctionPtrNative fn;
};

class CNeoVM;
class CNeoVMWorker
{
	friend CNeoVM;
private:
	u8 *					_pCodePtr;
	int						_iCodeLen;
	int						_iCodeOffset;


	CNeoVM*					_pVM;
	u32						_idWorker;
	bool					_isSetup = false;
	int						_iRemainSleep = 0;
	clock_t					_preClock;
	TableInfo*				_pCallTableInfo = NULL;


	void	SetCodeData(u8* p, int sz)
	{
		_pCodePtr = p;
		_iCodeLen = sz;
		_iCodeOffset = 0;
	}

	inline void*			GetCurPtr()
	{
		return (_pCodePtr + _iCodeOffset);
	}
	inline void				NextOP(int n)
	{
		_iCodeOffset += 1 + (n * 2);
	}

	inline	u8				GetU8()
	{
		return *(_pCodePtr + _iCodeOffset++);
	}
	inline	s16				GetS16()
	{
		s16 r = *(s16*)(_pCodePtr + _iCodeOffset);
		_iCodeOffset += 2;
		return r;
	}
	inline	u16				GetU16()
	{
		u16 r = *(u16*)(_pCodePtr + _iCodeOffset);
		_iCodeOffset += 2;
		return r;
	}
	inline	u32				GetU32()
	{
		s32 r = *(s32*)(_pCodePtr + _iCodeOffset);
		_iCodeOffset += 4;
		return r;
	}
	inline int GetCodeptr() { return _iCodeOffset; }
	inline void SetCodePtr(int off)
	{
		_iCodeOffset = off;
	}
	inline void SetCodeIncPtr(int off)
	{
		_iCodeOffset += off;
	}
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
	bool	Run(bool isSliceRun, int iTimeout = -1, int iCheckOpCount = 1000);

	inline VarInfo* GetVarPtr(short n)
	{
		if (n >= 0)
		{
			return &m_sVarStack[_iSP_Vars + n];
		}
		return &(*m_pVarGlobal)[-n - 1];
	}


	void Var_AddRef(VarInfo *d);
	void Var_Release(VarInfo *d);
	void Var_SetNone(VarInfo *d);
	void Var_SetInt(VarInfo *d, int v);
	void Var_SetFloat(VarInfo *d, double v);
	void Var_SetBool(VarInfo *d, bool v);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetTable(VarInfo *d, TableInfo* p);


	void Move(VarInfo* v1, VarInfo* v2);
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
	bool ForEach(VarInfo* v1, VarInfo* v2, VarInfo* v3);
	int Sleep(bool isSliceRun, int iTimeout, VarInfo* v1);

	std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	double ToFloat(VarInfo* v1);
	int ToSize(VarInfo* v1);
	std::string GetType(VarInfo* v1);

	void TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRemove(VarInfo *pTable, VarInfo *pArray);
	FunctionPtrNative* GetPtrFunction(VarInfo *pTable, VarInfo *pArray);

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
	inline TableInfo* GetCallTableInfo() { return _pCallTableInfo; }

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
	bool Call_TL(int iTimeout, int iCheckOpCount, RVal* r, int iFID, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		//RunFunction(funName, );
		if (false == Setup(iFID))
			return false;

		Run(false, iTimeout, iCheckOpCount);
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
	bool CallN_TL(int iTimeout, int iCheckOpCount, int iFID, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		//RunFunction(funName, );
		if (false == Setup(iFID))
			return false;

		Run(false, iTimeout, iCheckOpCount);
		if (_isSetup == true)
		{	// yet ... not completed
			GC();
			return false;
		}

		GC();
		ReturnValue();
		return true;
	}
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


	inline void SetError(const char* pErrMsg);
public:
	CNeoVMWorker(CNeoVM* pVM, u32 id, int iStackSize);
	virtual ~CNeoVMWorker();

	inline u32 GetWorkerID() { return _idWorker; }
};
