#pragma once

#include "NeoVMWorker.h"
#include "NeoVMMemoryPool.h"

struct neo_libs;
struct neo_DCalllibs;;
class CNArchive;
class CNeoVM
{
	friend					CNeoVMWorker;
	friend					TableInfo;
	friend					ListInfo;
	friend					SetInfo;
	friend					neo_libs;
	friend					neo_DCalllibs;
private:
	u8 *					_pCodePtr;
	int						_iCodeLen;

	void	SetCodeData(u8* p, int sz)
	{
		_pCodePtr = p;
		_iCodeLen = sz;
	}

	std::vector<VarInfo>	m_sVarGlobal;
	std::vector<SFunctionTable> m_sFunctionPtr;

	std::map<u32, ListInfo*> _sLists;
	std::map<u32, TableInfo*> _sTables;
	std::map<u32, SetInfo*> _sSets;
	std::map<u32, StringInfo*> _sStrings;
	u32 _dwLastIDVMWorker = 0;

	SNeoVMHeader			_header;
	std::map<std::string, int> m_sImExportTable;
	std::vector<debug_info>	_DebugData;

	CNeoVMWorker*			_pMainWorker;
	std::map<u32, CNeoVMWorker*> _sVMWorkers;
	int	_BytesSize;

	inline bool IsDebugInfo() { return (_header._dwFlag & NEO_HEADER_FLAG_DEBUG) != 0; }

	void Var_AddRef(VarInfo *d);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetStringA(VarInfo *d, const std::string& str);
	void Var_SetTable(VarInfo *d, TableInfo* p);


	CNeoVMWorker* WorkerAlloc(int iStackSize);
	void FreeWorker(CNeoVMWorker *d);

	CoroutineInfo* CoroutineAlloc();
	void FreeCoroutine(VarInfo *d);

	StringInfo* StringAlloc(const std::string& str);
	void FreeString(VarInfo *d);

	TableInfo* TableAlloc();
	void FreeTable(TableInfo* tbl);

	ListInfo* ListAlloc();
	void FreeList(ListInfo* tbl);

	SetInfo* SetAlloc();
	void FreeSet(SetInfo* tbl);

	int	 Coroutine_Create(int iFID);
	int	 Coroutine_Resume(int iCID);
	int	 Coroutine_Destroy(int iCID);



	inline void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
			Var_Release(v1);
		CNeoVMWorker::Move_DestNoRelease(v1, v2);
	}
	void Var_ReleaseInternal(VarInfo *d)
	{
		switch (d->GetType())
		{
		case VAR_STRING:
			if (--d->_str->_refCount <= 0)
				FreeString(d);
			d->_str = NULL;
			break;
		case VAR_TABLE:
			if (--d->_tbl->_refCount <= 0)
				FreeTable(d->_tbl);
			d->_tbl = NULL;
			break;
		case VAR_LIST:
			if (--d->_lst->_refCount <= 0)
				FreeList(d->_lst);
			d->_lst = NULL;
			break;
		case VAR_SET:
			if (--d->_set->_refCount <= 0)
				FreeSet(d->_set);
			d->_set = NULL;
			break;
		case VAR_COROUTINE:
			if (--d->_cor->_refCount <= 0)
				FreeCoroutine(d);
			d->_cor = NULL;
			break;
		default:
			break;
		}
		d->ClearType();
	}

	inline void Var_Release(VarInfo *d)
	{
		if (d->IsAllocType())
			Var_ReleaseInternal(d);
		else
			d->ClearType();
	}

	VarInfo m_sDefaultValue[NDF_MAX];
	
	CNVMAllocPool < TableNode, 10> m_sPool_TableNode;
	CNVMAllocPool< TableInfo, 10 > m_sPool_TableInfo;
	CNVMAllocPool < SetNode, 10> m_sPool_SetNode;
	CNVMAllocPool< SetInfo, 10 > m_sPool_SetInfo;
	CNVMAllocPool< ListInfo, 10 > m_sPool_ListInfo;

	CNVMInstPool< StringInfo, 10 > m_sPool_String;
	CNVMInstPool< CoroutineInfo, 10 > m_sPool_Coroutine;

	FunctionPtrNative _funLib;
	FunctionPtrNative _funLstLib;
	FunctionPtrNative _funStrLib;
	FunctionPtrNative _funTblLib;
public:

	bool RunFunction(const std::string& funName);
	inline int GetBytesSize() { return _BytesSize; }


	std::string _sErrorMsgDetail;
	std::string _pErrorMsg;
	bool _bError = false;

	static bool IsGlobalLibFun(std::string& FunName);
	void RegLibrary(VarInfo* pSystem, const char* pLibName);// , SNeoFunLib* pFuns);
	void RegObjLibrary();
	void InitLib();
	void CallStatic(VarInfo* r, VarInfo* v1, VarInfo* v2);
	bool Init(void* pBuffer, int iSize, int iStackSize);
	inline void SetError(const std::string& msg);
	inline bool IsLocalErrorMsg() { return _bError; }
public:
	CNeoVM();
	virtual ~CNeoVM();

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

	static FunctionPtrNative RegisterNative(Neo_NativeFunction func)
	{
		FunctionPtrNative fun;
		CNeoVMWorker::neo_pushcclosureNative(&fun, func);
		return fun;
	}

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

	template<typename RVal, typename ... Types>
	bool Call_TL(RVal* r, const std::string& funName, Types ... args) // Time Limit
	{
		int iFID = -1;
		auto it = m_sImExportTable.find(funName);
		if (it == m_sImExportTable.end())
			return false;

		iFID = (*it).second;

		return _pMainWorker->Call_TL<RVal>(r, iFID, args...);
	}

	template<typename ... Types>
	bool CallN_TL(const std::string& funName, Types ... args) // Time Limit
	{
		int iFID = -1;
		auto it = m_sImExportTable.find(funName);
		if (it == m_sImExportTable.end())
			return false;

		iFID = (*it).second;

		return _pMainWorker->CallN_TL(iFID, args...);
	}

	u32 CreateWorker(int iStackSize = 50 * 1024);
	bool ReleaseWorker(u32 id);
	bool BindWorkerFunction(u32 id, const std::string& funName);
	bool SetTimeout(u32 id, int iTimeout = -1, int iCheckOpCount = 1000);
	bool IsWorking(u32 id);
	bool UpdateWorker(u32 id);


	inline const char* GetLastErrorMsg() { return _sErrorMsgDetail.c_str();  }
	inline bool IsLastErrorMsg() { return (_sErrorMsgDetail.empty() == false); }
	void ClearLastErrorMsg() { _bError = false; _sErrorMsgDetail.clear(); }

	static CNeoVM*	LoadVM(void* pBuffer, int iSize, int iStackSize = 50 * 1024);
	static void		ReleaseVM(CNeoVM* pVM);
	static bool		Compile(void* pBufferSrc, int iLenSrc, CNArchive& arw, std::string& err, bool putASM = false, bool debug = false, bool allowGlobalInitLogic = true);

	static CNeoVM*	CompileAndLoadVM(void* pBufferSrc, int iLenSrc, std::string& err, bool putASM = false, bool debug = false, bool allowGlobalInitLogic = true, int iStackSize = 50 * 1024);
};

