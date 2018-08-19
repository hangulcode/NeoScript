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
	NOP_FUNEND,

	NOP_TABLE_ALLOC,
	NOP_TABLE_INSERT,
	NOP_TABLE_READ,
};

struct VarInfo;
class CNeoVM;
class CNeoVMWorker;
typedef int(*Neo_CFunction) (CNeoVMWorker *N, void* pFun, short args);

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

struct SFunLib
{
	const char* pName;
	FunctionPtr fn;
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
	std::vector<VarInfo>	m_sVarStack;
	std::vector<SCallStack>	m_sCallStack;
	std::vector<VarInfo>*	m_pVarGlobal;


	std::vector<VarInfo> _args;
	bool RunFunction(const std::string& funName);

	bool	Run(int iFunctionID);

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
	bool PopBool(VarInfo *V)
	{
		if (V->GetType() == VAR_BOOL)
			return V->_bl;

		return false;
	}



public:


	void GC()
	{
		for (int i = _iSP_Vars + 1; i < _iSP_Vars_Max2; i++)
			Var_Release(&m_sVarStack[i]);
		_iSP_Vars_Max2 = _iSP_Vars;
	}

	template<typename T>
	T _read(VarInfo *V) { return T(); }



	template<typename T>
	void write(VarInfo *V, T) { }



	template<typename T>
	T read(int idx) { return _read<T>(&m_sVarStack[_iSP_Vars + idx]); }

	template<typename T>
	void push(T ret) {}

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
		return _read<RVal>(&m_sVarStack[_iSP_Vars]);
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
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
		static int invoke(CNeoVMWorker *N, void* pfun, short args)
		{
			if (args != 1)
				return -1;
			upvalue_<void(*)(T1)>(pfun)(N->read<T1>(1));
			N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
			return 0;
		}
	};

	std::string _sErrorMsgDetail;
	const char* _pErrorMsg = NULL;

//	bool Init(void* pBuffer, int iSize);
	inline void SetError(const char* pErrMsg);
public:
	CNeoVMWorker(CNeoVM* pVM, u32 id, int iStackSize);
	virtual ~CNeoVMWorker();

	inline u32 GetWorkerID() { return _idWorker; }
};

template<> inline char*			CNeoVMWorker::_read(VarInfo *V) { return (char*)PopString(V); }
template<> inline const char*		CNeoVMWorker::_read(VarInfo *V) { return PopString(V); }
template<> inline char				CNeoVMWorker::_read(VarInfo *V) { return (char)PopInt(V); }
template<> inline unsigned char	CNeoVMWorker::_read(VarInfo *V) { return (unsigned char)PopInt(V); }
template<> inline short			CNeoVMWorker::_read(VarInfo *V) { return (short)PopInt(V); }
template<> inline unsigned short	CNeoVMWorker::_read(VarInfo *V) { return (unsigned short)PopInt(V); }
template<> inline long				CNeoVMWorker::_read(VarInfo *V) { return (long)PopInt(V); }
template<> inline unsigned long	CNeoVMWorker::_read(VarInfo *V) { return (unsigned long)PopInt(V); }
template<> inline int				CNeoVMWorker::_read(VarInfo *V) { return (int)PopInt(V); }
template<> inline unsigned int		CNeoVMWorker::_read(VarInfo *V) { return (unsigned int)PopInt(V); }
template<> inline float			CNeoVMWorker::_read(VarInfo *V) { return (float)PopFloat(V); }
template<> inline double			CNeoVMWorker::_read(VarInfo *V) { return (double)PopFloat(V); }
template<> inline bool				CNeoVMWorker::_read(VarInfo *V) { return PopBool(V); }
template<> inline void				CNeoVMWorker::_read(VarInfo *V) {}
template<> inline long long		CNeoVMWorker::_read(VarInfo *V) { return (long long)PopInt(V); }
template<> inline unsigned long long CNeoVMWorker::_read(VarInfo *V) { return (unsigned long long)PopInt(V); }

template<> void	inline CNeoVMWorker::write(VarInfo *V, char* p) { Var_SetString(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, const char* p) { Var_SetString(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, char p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, unsigned char p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, short p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, unsigned short p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, long p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, unsigned long p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, int p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, unsigned int p) { Var_SetInt(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, float p) { Var_SetFloat(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, double p) { Var_SetFloat(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, bool p) { Var_SetBool(V, p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, long long p) { Var_SetInt(V, (int)p); }
template<> void	inline CNeoVMWorker::write(VarInfo *V, unsigned long long p) { Var_SetInt(V, (int)p); }

template<>	inline void CNeoVMWorker::push(char ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(unsigned char ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(short ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(unsigned short ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(long ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(unsigned long ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(int ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(unsigned int ret) { PushInt(ret); }
template<>	inline void CNeoVMWorker::push(float ret) { PushFloat(ret); }
template<>	inline void CNeoVMWorker::push(double ret) { PushFloat((double)ret); }
template<>	inline void CNeoVMWorker::push(char* ret) { PushString(ret); }
template<>	inline void CNeoVMWorker::push(const char* ret) { PushString(ret); }
template<>	inline void CNeoVMWorker::push(bool ret) { PushBool(ret); }
template<>	inline void CNeoVMWorker::push(long long ret) { PushInt((int)ret); }
template<>	inline void CNeoVMWorker::push(unsigned long long ret) { PushInt((int)ret); }

template<>
struct CNeoVMWorker::functor<void>
{
	static int invoke(CNeoVMWorker *N, void* pfun, short args)
	{
		if (args != 0)
			return -1;
		upvalue_<void(*)()>(pfun)();
		N->Var_Release(&N->m_sVarStack[N->_iSP_Vars]);
		return 0;
	}
};
