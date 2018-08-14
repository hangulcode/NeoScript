#pragma once

#include "NeoConfig.h"

enum VAR_TYPE : u8
{
	VAR_NONE,
	VAR_BOOL,
	VAR_INT,
	VAR_FLOAT,	// double
	VAR_STRING,
	VAR_TABLE,
	VAR_PTRFUN,
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

	NOP_INC,
	NOP_DEC,

	NOP_ADD3,
	NOP_SUB3,
	NOP_MUL3,
	NOP_DIV3,

	NOP_GREAT,		// >
	NOP_GREAT_EQ,	// >=
	NOP_LESS,		// <
	NOP_LESS_EQ,	// <=
	NOP_EQUAL2,	// ==
	NOP_NEQUAL,	// !=
	NOP_AND,	// &&
	NOP_OR,		// ||

	NOP_STR_ADD,	// ..

	NOP_TOSTRING,
	NOP_TOINT,
	NOP_TOFLOAT,
	NOP_GETTYPE,

	NOP_JMP,
	NOP_JMP_FALSE,
	NOP_JMP_TRUE,

	NOP_CALL,
	NOP_FARCALL,
	NOP_PTRCALL,
	NOP_RETURN,

	NOP_TABLE_ALLOC,
	NOP_TABLE_INSERT,
	NOP_TABLE_READ,
};

struct VarInfo;
class CNeoVM;
typedef int(*Neo_CFunction) (CNeoVM *N, void* pFun, short args);

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
};

struct FunctionPtr
{
	Neo_CFunction				_fn;
	void*						_func;
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
		FunctionPtr _fun;
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
	inline bool IsTrue()
	{
		if (VAR_BOOL == _type)
			return _bl;
		return false;
	}
};

struct SNeoVMHeader
{
	u32		_dwFileType;
	u32		_dwNeoVersion;

	int		_iFunctionCount;
	int		_iStaticVarCount;
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
struct SFunLib
{
	const char* pName;
	FunctionPtr fn;
};
class CNeoVM
{
private:
	//CNArchive				m_sCode;
	u8 *					_pCodePtr;
	int						_iCodeLen;
	int						_iCodeOffset;

	void	SetCodeData(u8* p, int sz)
	{
		_pCodePtr = p;
		_iCodeLen = sz;
		_iCodeOffset = 0;
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
	std::vector<VarInfo>	m_sVarGlobal;
	std::vector<VarInfo>	m_sVarStack;
	std::vector<SCallStack>	m_sCallStack;
	std::vector<SFunctionTable> m_sFunctionPtr;
	std::map<int, TableInfo*> _sTables;
	std::map<int, StringInfo*> _sStrings;

	int iLastIDTable = 0;
	int iLastIDString = 0;

	SNeoVMHeader			_header;
	std::map<std::string, int> m_sImExportTable;

	bool	Run(int iFunctionID);

	inline VarInfo* GetVarPtr(short n)
	{
		if (n >= 0)
		{
			return &m_sVarStack[_iSP_Vars + n];
		}
		return &m_sVarGlobal[-n - 1];
	}


	void Var_AddRef(VarInfo *d);
	void Var_Release(VarInfo *d);
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
	void Add(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Sub(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Mul(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Div(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Inc(VarInfo* v1);
	void Dec(VarInfo* v1);
	bool CompareEQ(VarInfo* v1, VarInfo* v2);
	bool CompareGR(VarInfo* v1, VarInfo* v2);
	bool CompareGE(VarInfo* v1, VarInfo* v2);

	std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	double ToFloat(VarInfo* v1);
	std::string GetType(VarInfo* v1);

	void TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	FunctionPtr* GetPtrFunction(VarInfo *pTable, VarInfo *pArray);

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
	void PushString(const char* p)
	{
		VarInfo d;
		d.SetType(VAR_STRING);
		d._str = StringAlloc(p);
		_args.push_back(d);
	}
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
		return -987654321;
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
		return -987654321;
	}
	const char* PopString(VarInfo *V)
	{
		if (V->GetType() == VAR_STRING)
			return V->_str->_str.c_str();

		return NULL;
	}
	bool PopBool(VarInfo *V)
	{
		if (V->GetType() == VAR_BOOL)
			return V->_bl;

		return false;
	}


	StringInfo* StringAlloc(const char* str);
	void FreeString(VarInfo *d);

	TableInfo* TableAlloc();
	void FreeTable(VarInfo *d);



	template<typename T>
	T read(VarInfo *V) { T t; return r; }

	template<>	char*				read(VarInfo *V) { return (char*)PopString(V); }
	template<>	const char*			read(VarInfo *V) { return PopString(V); }
	template<>	char				read(VarInfo *V) { return (char)PopInt(V); }
	template<>	unsigned char		read(VarInfo *V) { return (unsigned char)PopInt(V); }
	template<>	short				read(VarInfo *V) { return (short)PopInt(V); }
	template<>	unsigned short		read(VarInfo *V) { return (unsigned short)PopInt(V); }
	template<>	long				read(VarInfo *V) { return (long)PopInt(V); }
	template<>	unsigned long		read(VarInfo *V) { return (unsigned long)PopInt(V); }
	template<>	int					read(VarInfo *V) { return (int)PopInt(V); }
	template<>	unsigned int		read(VarInfo *V) { return (unsigned int)PopInt(V); }
	template<>	float				read(VarInfo *V) { return (float)PopFloat(V); }
	template<>	double				read(VarInfo *V) { return (double)PopFloat(V); }
	template<>	bool				read(VarInfo *V) { return PopBool(V); }
	template<>	void				read(VarInfo *V) {}
	template<>	long long			read(VarInfo *V) { return (long long)PopInt(V); }
	template<>	unsigned long long	read(VarInfo *V) { return (unsigned long long)PopInt(V); }

	template<typename T>
	void write(VarInfo *V, T) { }

	template<>	void				write(VarInfo *V, char* p) { Var_SetString(V, p); }
	template<>	void				write(VarInfo *V, const char* p) { Var_SetString(V, p); }
	template<>	void				write(VarInfo *V, char p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, unsigned char p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, short p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, unsigned short p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, long p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, unsigned long p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, int p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, unsigned int p) { Var_SetInt(V, p); }
	template<>	void				write(VarInfo *V, float p) { Var_SetFloat(V, p); }
	template<>	void				write(VarInfo *V, double p) { Var_SetFloat(V, p); }
	template<>	void				write(VarInfo *V, bool p) { Var_SetBool(V, p); }
	template<>	void				write(VarInfo *V, long long p) { Var_SetInt(V, (int)p); }
	template<>	void				write(VarInfo *V, unsigned long long p) { Var_SetInt(V, (int)p); }

	template<typename T>
	T read(int idx) { return read<T>(&m_sVarStack[_iSP_Vars + idx]); }

	template<typename T>
	void push(T ret) {}

	template<>	void push(char ret) { PushInt(ret); }
	template<>	void push(unsigned char ret) { PushInt(ret); }
	template<>	void push(short ret) { PushInt(ret); }
	template<>	void push(unsigned short ret) { PushInt(ret); }
	template<>	void push(long ret) { PushInt(ret); }
	template<>	void push(unsigned long ret) { PushInt(ret); }
	template<>	void push(int ret) { PushInt(ret); }
	template<>	void push(unsigned int ret) { PushInt(ret); }
	template<>	void push(float ret) { PushFloat(ret); }
	template<>	void push(double ret) { PushFloat((double)ret); }
	template<>	void push(char* ret) { PushString(ret); }
	template<>	void push(const char* ret) { PushString(ret); }
	template<>	void push(bool ret) { PushBool(ret); }
	template<>	void push(long long ret) { PushInt((int)ret); }
	template<>	void push(unsigned long long ret) { PushInt((int)ret); }

	std::vector<VarInfo> _args;
	bool RunFunction(const std::string& funName);

	void GC()
	{
		for (int i = _iSP_Vars + 1; i < _iSP_Vars_Max2; i++)
			Var_Release(&m_sVarStack[i]);
		_iSP_Vars_Max2 = _iSP_Vars;
	}

	const char* _pErrorMsg = NULL;

	void RegLibrary(VarInfo* pSystem, const char* pLibName, SFunLib* pFuns, int iCount);
	void Init();
	void SetError(const char* pErrMsg);
public:
	CNeoVM(int iStackSize);
	virtual ~CNeoVM();


	inline void PushArgs() { }
	template<typename  T, typename ... Types>
	inline void PushArgs(T arg1, Types ... args)
	{
		push(arg1);
		PushArgs(args...);
	}

	template<typename RVal, typename ... Types>
	RVal Call(const std::string& funName, Types ... args)
	{
		ClearArgs();
		PushArgs(args...);
		RunFunction(funName);
		GC();
		return read<RVal>(&m_sVarStack[_iSP_Vars]);
	}


	static void neo_pushcclosure(FunctionPtr* pOut, Neo_CFunction fn, void* pFun)
	{
		pOut->_fn = fn;
		pOut->_func = pFun;
	}

	template<typename T>
	inline static T upvalue_(void* p)
	{
		return (T)p;
	}

	// 리턴값이 있는 Call
	template<typename RVal, typename T1 = void, typename T2 = void, typename T3 = void, typename T4 = void, typename T5 = void>
	struct functor
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 5)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2, T3, T4, T5)>(pfun)(N->read<T1>(1), N->read<T2>(2), N->read<T3>(3), N->read<T4>(4), N->read<T5>(5));
			N->write(&N->m_sVarStack[N->_iSP_Vars], a);
			return 1;
		}
	};
	template<typename RVal, typename T1, typename T2, typename T3, typename T4>
	struct functor<RVal, T1, T2, T3, T4>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 4)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2, T3, T4)>(pfun)(N->read<T1>(1), N->read<T2>(2), N->read<T3>(3), N->read<T4>(4));
			N->write(&N->m_sVarStack[N->_iSP_Vars], a);
			return 1;
		}
	};
	template<typename RVal, typename T1, typename T2, typename T3>
	struct functor<RVal, T1, T2, T3>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 3)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2, T3)>(pfun)(N->read<T1>(1), N->read<T2>(2), N->read<T3>(3));
			N->write(&N->m_sVarStack[N->_iSP_Vars], a);
			return 1;
		}
	};
	template<typename RVal, typename T1, typename T2>
	struct functor<RVal, T1, T2>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 2)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2)>(pfun)(N->read<T1>(1), N->read<T2>(2));
			N->write(&N->m_sVarStack[N->_iSP_Vars], a);
			return 1;
		}
	};
	template<typename RVal, typename T1>
	struct functor<RVal, T1>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 1)
				return -1;
			auto a = upvalue_<RVal(*)(T1)>(pfun)(N->read<T1>(1));
			N->write(&N->m_sVarStack[N->_iSP_Vars], a);
			return 1;
		}
	};
	template<typename RVal>
	struct functor<RVal>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 0)
				return -1;
			auto a = upvalue_<RVal(*)()>(pfun)();
			N->write(&N->m_sVarStack[N->_iSP_Vars], a);
			return 1;
		}
	};

	// 리턴값이 없는 Call
	template<typename T1, typename T2, typename T3, typename T4, typename T5>
	struct functor<void, T1, T2, T3, T4, T5>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{ 
			if (args != 5)
				return -1;
			upvalue_<void(*)(T1, T2, T3, T4, T5)>(pfun)(N->read<T1>(1), N->read<T2>(2), N->read<T3>(3), N->read<T4>(4), N->read<T5>(5));
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0; 
		}
	};
	template<typename T1, typename T2, typename T3, typename T4>
	struct functor<void, T1, T2, T3, T4>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 4)
				return -1;
			upvalue_<void(*)(T1, T2, T3, T4)>(pfun)(N->read<T1>(1), N->read<T2>(2), N->read<T3>(3), N->read<T4>(4));
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0;
		}
	};
	template<typename T1, typename T2, typename T3>
	struct functor<void, T1, T2, T3>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 3)
				return -1;
			upvalue_<void(*)(T1, T2, T3)>(pfun)(N->read<T1>(1), N->read<T2>(2), N->read<T3>(3));
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0;
		}
	};
	template<typename T1, typename T2>
	struct functor<void, T1, T2>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 2)
				return -1;
			upvalue_<void(*)(T1, T2)>(pfun)(N->read<T1>(1), N->read<T2>(2));
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0;
		}
	};
	template<typename T1>
	struct functor<void, T1>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 1)
				return -1;
			upvalue_<void(*)(T1)>(pfun)(N->read<T1>(1));
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0;
		}
	};
	template<>
	struct functor<void>
	{
		static int invoke(CNeoVM *N, void* pfun, short args)
		{
			if (args != 0)
				return -1;
			upvalue_<void(*)()>(pfun)();
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0;
		}
	};

	template<typename RVal, typename ... Types>
	static int push_functor(FunctionPtr* pOut, RVal(*func)(Types ... args))
	{
		neo_pushcclosure(pOut, functor<RVal, Types ...>::invoke, (void*)func);
		return sizeof ...(Types);
	}

	template<typename F>
	bool Register(const char* name, F func)
	{
		auto it = m_sImExportTable.find(name);
		if (it == m_sImExportTable.end())
			return false;

		int iFID = (*it).second;
		FunctionPtr fun;
		int iArgCnt = push_functor(&fun, func);
		if (m_sFunctionPtr[iFID]._argsCount != iArgCnt)
			return false;

		m_sFunctionPtr[iFID]._fun = fun;
		return true;
	}


	template<typename F>
	static FunctionPtr Register(F func)
	{
		FunctionPtr fun;
		push_functor(&fun, func);
		return fun;
	}

	const char* GetLastErrorMsg() { return _pErrorMsg;  }
	bool IsLastErrorMsg() { return (NULL != _pErrorMsg); }
	void ClearLastErrorMsg() { _pErrorMsg = NULL; }

	static CNeoVM*	LoadVM(void* pBuffer, int iSize);
	static void		ReleaseVM(CNeoVM* pVM);
	static bool		Compile(void* pBufferSrc, int iLenSrc, void* pBufferCode, int iLenCode, int* pLenCode, bool putASM);
};


