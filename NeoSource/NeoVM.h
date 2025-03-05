#pragma once

#include "NeoConfig.h"
#include "NeoVMHash.h"

namespace NeoScript
{

class CNArchive;

struct INeoVMWorker;
struct FunctionPtr;
struct VarInfo;

struct INeoLoader
{
	virtual bool Load(const char* pFileName, void*& pBuffer, int& iLen) = 0;
	virtual void Unload(const char* pFileName, void* pBuffer, int iLen) = 0;
};


typedef int(*Neo_CFunction) (INeoVMWorker* N, FunctionPtr* pFun, short args);
typedef bool(*Neo_NativeFunction) (INeoVMWorker* N, void* pUserData, const VMString* pStr, short args);
typedef bool(*Neo_NativeProperty) (INeoVMWorker* N, void* pUserData, const VMString* pStr, VarInfo* p, bool get);

#define NEO_DEFAULT_CHECKOP		(500)

struct FunctionPtr
{
	u8							_argCount;
	Neo_CFunction				_fn;
	void* _func;
};

#pragma pack(1)
struct FunctionPtrNative
{
	Neo_NativeFunction			_func;
	Neo_NativeProperty			_property;
};
struct NeoFunction
{
	// Script Function
	INeoVMWorker* _pWorker;
	int			_fun_index;

	// C Function
	FunctionPtr _fun;
};
enum VAR_TYPE : u8
{
	VAR_INT,
	VAR_FLOAT,	// float or double 
	VAR_BOOL,
	VAR_NONE,
	VAR_FUN,

	VAR_ITERATOR,
	VAR_FUN_NATIVE,

	VAR_CHAR,

	VAR_STRING,	// Alloc
	VAR_MAP,
	VAR_LIST,
	VAR_SET,
	VAR_COROUTINE,
	VAR_MODULE,
	VAR_ASYNC,
};
struct CoroutineInfo;
struct StringInfo;
struct MapInfo;
struct ListInfo;
struct SetInfo;
struct AsyncInfo;
struct MapNode;
struct SetNode;
struct INeoVM;

#pragma pack(1)
struct CollectionIterator
{
	union
	{
		MapNode* _pTableNode;
		SetNode* _pSetNode;
		int			_iListOffset;
		int			_iStringOffset;
	};
};
#pragma pack()

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
		SUtf8One	_c;
		MapInfo* _tbl;
		ListInfo* _lst;
		SetInfo* _set;
		FunctionPtr* _funPtr; // C Native
		int			_int;
		NS_FLOAT	_float;
		int			_fun_index;
		INeoVMWorker* _module;
		AsyncInfo*	_async;
		CollectionIterator	_it;
	};

	NEOS_FORCEINLINE VarInfo() { _type = VAR_NONE; }
	NEOS_FORCEINLINE VarInfo(VAR_TYPE t) { _type = t; }
	NEOS_FORCEINLINE VarInfo(int v) { _type = VAR_INT; _int = v; }

	NEOS_FORCEINLINE VAR_TYPE GetType() { return _type; }
	NEOS_FORCEINLINE void SetType(VAR_TYPE t) { _type = t; }
	NEOS_FORCEINLINE void ClearType()
	{
		_type = VAR_NONE;
	}
	NEOS_FORCEINLINE bool IsAllocType()
	{
		return ((_type >= VAR_STRING));
	}
	NEOS_FORCEINLINE bool IsTrue()
	{
		if (VAR_BOOL == _type)
			return _bl;
		return false;
	}
	NEOS_FORCEINLINE bool IsNumber()
	{
		return (VAR_INT == _type || VAR_FLOAT == _type);
	}
	NEOS_FORCEINLINE NS_FLOAT GetFloatNumber()
	{
		if (VAR_INT == _type) return (NS_FLOAT)_int;
		if (VAR_FLOAT == _type) return (NS_FLOAT)_float;
		return 0;
	}

	bool MapInsertFloat(const std::string& pKey, NS_FLOAT value);
	bool MapFindFloat(const std::string& pKey, NS_FLOAT& value);

	bool ListInsertFloat(int idx, NS_FLOAT value);
	bool ListFindFloat(int idx, NS_FLOAT& value);
	bool SetListIndexer(VMHash<int>* pIndexer);
};

struct INeoVMWorker
{
protected:
	std::vector<VarInfo>* _args = NULL;
	INeoVM* _pVM;

	u32						_idWorker;
	int	_BytesSize = 0;
public:
	inline u32 GetWorkerID() { return _idWorker; }
	inline int GetBytesSize() { return _BytesSize; }

	virtual bool RunFunctionResume(int iFID, std::vector<VarInfo>& _args) = 0;
	virtual bool RunFunction(int iFID, std::vector<VarInfo>& _args) =0;
	virtual bool RunFunction(const std::string& funName, std::vector<VarInfo>& _args) =0;
	virtual void GC() =0;
	virtual VarInfo* GetReturnVar() =0;
	virtual VarInfo* GetStackVar(int idx) =0;
	virtual bool ResetVarType(VarInfo* p, VAR_TYPE type, int capa = 0) =0;

	static void neo_pushcclosureNative(FunctionPtrNative* pOut, Neo_NativeFunction pFun)
	{
		pOut->_func = pFun;
	}
	static void neo_pushcclosureNative(FunctionPtrNative* pOut, Neo_NativeProperty property)
	{
		pOut->_property = property;
	}
	static void neo_pushcclosure(FunctionPtr* pOut, Neo_CFunction fn, void* pFun)
	{
		pOut->_fn = fn;
		pOut->_func = pFun;
	}
	void Var_Release(VarInfo* d);
	void Var_SetNone(VarInfo* d);

	virtual void Var_Move(VarInfo* v1, VarInfo* v2) =0;


	void PushInt(int v)
	{
		VarInfo d;
		d.SetType(VAR_INT);
		d._int = v;
		_args->push_back(d);
	}
	void PushFloat(NS_FLOAT v)
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

	int PopInt(VarInfo* V)
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
	NS_FLOAT PopFloat(VarInfo* V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return (NS_FLOAT)V->_int;
		case VAR_FLOAT:
			return V->_float;
		default:
			break;
		}
		return -1;
	}
	const char* PopString(VarInfo* V);
	const std::string* PopStlString(VarInfo* V);
	bool PopBool(VarInfo* V)
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

	bool GetArg_StlString(int idx, std::string &r);
	bool GetArg_Int(int idx, int& r);
	bool GetArg_Float(int idx, NS_FLOAT& r);
	bool GetArg_Bool(int idx, bool &r);

	inline void push(char ret) { PushInt(ret); }
	inline void push(unsigned char ret) { PushInt(ret); }
	inline void push(short ret) { PushInt(ret); }
	inline void push(unsigned short ret) { PushInt(ret); }
	inline void push(long ret) { PushInt(ret); }
	inline void push(unsigned long ret) { PushInt(ret); }
	inline void push(int ret) { PushInt(ret); }
	inline void push(unsigned int ret) { PushInt(ret); }
	inline void push(NS_FLOAT ret) { PushFloat(ret); }
	inline void push(char* ret) { PushString(ret); }
	inline void push(const char* ret) { PushString(ret); }
	inline void push(bool ret) { PushBool(ret); }
	inline void push(long long ret) { PushInt((int)ret); }
	inline void push(unsigned long long ret) { PushInt((int)ret); }
	inline void push(NeoFunction ret) { PushNeoFunction(ret); }

	inline void		_read(VarInfo* V, std::string*& r) { r = (std::string*)PopStlString(V); }
	inline void		_read(VarInfo* V, char*& r) { r = (char*)PopString(V); }
	inline void		_read(VarInfo* V, const char*& r) { r = PopString(V); }
	inline void		_read(VarInfo* V, char& r) { r = (char)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned char& r) { r = (unsigned char)PopInt(V); }
	inline void		_read(VarInfo* V, short& r) { r = (short)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned short& r) { r = (unsigned short)PopInt(V); }
	inline void		_read(VarInfo* V, long& r) { r = (long)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned long& r) { r = (unsigned long)PopInt(V); }
	inline void		_read(VarInfo* V, int& r) { r = (int)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned int& r) { r = (unsigned int)PopInt(V); }
	inline void		_read(VarInfo* V, NS_FLOAT& r) { r = (NS_FLOAT)PopFloat(V); }
	inline void		_read(VarInfo* V, bool& r) { r = PopBool(V); }
	inline void		_read(VarInfo* V) {}
	inline void		_read(VarInfo* V, long long& r) { r = (long long)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned long long& r) { r = (unsigned long long)PopInt(V); }
	inline void		_read(VarInfo* V, VarInfo*& r) { r = V; }
	inline void		_read(VarInfo* V, NeoFunction& r) { r = PopNeoFunction(V); }

	void Var_SetInt(VarInfo* d, int v);
	void Var_SetFloat(VarInfo* d, NS_FLOAT v);
	void Var_SetBool(VarInfo* d, bool v);
	void Var_SetCoroutine(VarInfo* d, CoroutineInfo* p);
	void Var_SetString(VarInfo* d, const char* str);
	void Var_SetString(VarInfo* d, SUtf8One c);
	void Var_SetStringA(VarInfo* d, const std::string& str);
	void Var_SetTable(VarInfo* d, MapInfo* p);
	void Var_SetList(VarInfo* d, ListInfo* p);
	void Var_SetSet(VarInfo* d, SetInfo* p);
	void Var_SetFun(VarInfo* d, int fun_index);
	void Var_SetModule(VarInfo* d, INeoVMWorker* p);
	void Var_SetAsync(VarInfo* d, AsyncInfo* p);

	inline void	ReturnValue() { Var_Release(GetReturnVar()); }
	inline void	ReturnValue(VarInfo* p) { Var_Move(GetReturnVar(), p); }
	inline void	ReturnValue(char* p) { Var_SetString(GetReturnVar(), p); }
	inline void	ReturnValue(const char* p) { Var_SetString(GetReturnVar(), p); }
	inline void	ReturnValue(char p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned char p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(short p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned short p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(long p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned long p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(int p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned int p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(NS_FLOAT p) { Var_SetFloat(GetReturnVar(), p); }
	inline void	ReturnValue(bool p) { Var_SetBool(GetReturnVar(), p); }
	inline void	ReturnValue(long long p) { Var_SetInt(GetReturnVar(), (int)p); }
	inline void	ReturnValue(unsigned long long p) { Var_SetInt(GetReturnVar(), (int)p); }
	inline void	ReturnValue(CoroutineInfo* p) { Var_SetCoroutine(GetReturnVar(), p); }
	inline void	ReturnValue(AsyncInfo* p) { Var_SetAsync(GetReturnVar(), p); }

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
		_read(GetReturnVar(), r);
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
		_read(GetReturnVar(), r);
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

	virtual int FindFunction(const std::string& name) = 0;
	virtual bool	Setup(int iFunctionID, std::vector<VarInfo>& _args) = 0;
	virtual bool	Start(int iFunctionID, std::vector<VarInfo>& _args) = 0;
	virtual bool IsWorking() = 0;
	virtual bool	Run() =0;
	virtual void SetTimeout(int iTimeout, int iCheckOpCount) = 0;
	virtual VarInfo* GetVar(const std::string& name) = 0;
	virtual bool BindWorkerFunction(const std::string& funName) = 0;
};


struct NeoCompilerParam
{
	const void* pBufferSrc;
	int iLenSrc;

	std::string* err = nullptr;
	bool putASM = false;
	bool debug = false;
	bool allowGlobalInitLogic = true;
	int iStackSize = 50 * 1024;

	std::string* preCompileHeader = nullptr;

	NeoCompilerParam(const void* pSrc, int SrcLen)
	{
		pBufferSrc = pSrc;
		iLenSrc = SrcLen;
	}
};

typedef void (*NEO_GLOBALINTERFACE)(INeoVMWorker*, void*);
struct NeoLoadVMParam
{
	std::string* globalInterfaceName = nullptr;
	NEO_GLOBALINTERFACE NeoGlobalInterface = nullptr;
	void* param = nullptr;

	NeoLoadVMParam()
	{
	}
};


struct INeoVM
{
protected:
	INeoVMWorker* _pMainWorker = NULL;
	bool _bError = false;
public:
	inline bool IsLocalErrorMsg() { return _bError; }
	static FunctionPtrNative RegisterNative(Neo_NativeFunction func);
	virtual int FindFunction(const std::string& name) =0;
	virtual bool SetFunction(int iFID, FunctionPtr& fun, int argCount) =0;

	typedef void(*IO_Print)(const char* pMsg);
	static IO_Print m_pFunPrint;

//	void Var_AddRef(VarInfo* d);
//	static void Move_DestNoRelease(VarInfo* v1, VarInfo* v2);

	void Var_ReleaseInternal(VarInfo* d);

	template<typename F>
	static FunctionPtr Register(F func)
	{
		FunctionPtr fun;
		int argCount = push_functor(&fun, func);
		fun._argCount = argCount;
		return fun;
	}

	template<typename RVal, typename ... Types>
	bool Call(RVal* r, const std::string& funName, Types ... args)
	{
		return _pMainWorker->Call<RVal>(*r, funName, args...);
	}

	template<typename ... Types>
	bool CallN(const std::string& funName, Types ... args)
	{
		return _pMainWorker->CallN(funName, args...);
	}

	template<typename ... Types>
	bool Setup_TL(const std::string& funName, Types ... args) // Setup Time Limit
	{
		int iFID = _pMainWorker->FindFunction(funName);
		if (iFID == -1)
			return false;

		return _pMainWorker->Setup_TL(iFID, args...);
	}

	bool Call_TL(); // Time Limit
	VarInfo* GetVar(const std::string& name);

	static bool	RegisterTableCallBack(VarInfo* p, void* pUserData, Neo_NativeFunction func, Neo_NativeProperty property);

	virtual u32 CreateWorker(int iStackSize = 50 * 1024) =0;
	virtual bool ReleaseWorker(u32 id) = 0;
	virtual bool BindWorkerFunction(u32 id, const std::string& funName) = 0;
	virtual bool SetTimeout(u32 id, int iTimeout = -1, int iCheckOpCount = NEO_DEFAULT_CHECKOP) = 0;
	virtual bool IsWorking(u32 id) = 0;
	virtual bool UpdateWorker(u32 id) = 0;

	inline INeoVMWorker* GetMainWorker() { return _pMainWorker; }
	int GetMainWorkerID() { return _pMainWorker == NULL ? 0 : _pMainWorker->GetWorkerID(); }
	inline int GetBytesSize() { return _pMainWorker->GetBytesSize(); }

	virtual  const char* GetLastErrorMsg() = 0;
	virtual  bool IsLastErrorMsg() = 0;
	virtual  void ClearLastErrorMsg() = 0;

	virtual  INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker = true, bool init = false, int iStackSize = 50 * 1024) =0; // 0 is error
	virtual  bool PCall(int iModule) = 0;

	static INeoVM* 	CreateVM();
	static void		ReleaseVM(INeoVM* pVM);
	static bool		Compile(CNArchive& arw, const NeoCompilerParam& param);

	static bool		Initialize(INeoLoader* loader = nullptr);
	static bool		Shutdown();

	static INeoVM*	CompileAndLoadVM(const NeoCompilerParam& param, const NeoLoadVMParam* vparam = nullptr);

	static bool		IsSinglePrecision() 
	{
	#ifdef NS_SINGLE_PRECISION
		return true;
	#else
		return false;
	#endif
	}
};

};

