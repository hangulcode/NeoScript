#pragma once

#include "NeoVMWorker.h"

class CNeoVM
{
	friend					CNeoVMWorker;
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

	std::map<u32, TableInfo*> _sTables;
	std::map<u32, StringInfo*> _sStrings;
	u32 _dwLastIDTable = 0;
	u32 _dwLastIDString = 0;
	u32 _dwLastIDVMWorker = 0;

	SNeoVMHeader			_header;
	std::map<std::string, int> m_sImExportTable;

	CNeoVMWorker*			_pMainWorker;
	std::map<u32, CNeoVMWorker*> _sVMWorkers;

	inline VarInfo* GetVarPtr(short n)
	{
		if (n >= 0)
		{
			return NULL;
		}
		return &m_sVarGlobal[-n - 1];
	}


	void Var_AddRef(VarInfo *d);
	void Var_Release(VarInfo *d);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetTable(VarInfo *d, TableInfo* p);


	void TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	FunctionPtr* GetPtrFunction(VarInfo *pTable, VarInfo *pArray);

	CNeoVMWorker* WorkerAlloc(int iStackSize);
	void FreeWorker(CNeoVMWorker *d);

	StringInfo* StringAlloc(const char* str);
	void FreeString(VarInfo *d);

	TableInfo* TableAlloc();
	void FreeTable(VarInfo *d);

public:

	bool RunFunction(const std::string& funName);


	std::string _sErrorMsgDetail;
	const char* _pErrorMsg = NULL;

	void RegLibrary(VarInfo* pSystem, const char* pLibName, SFunLib* pFuns);
	void InitLib();
	bool Init(void* pBuffer, int iSize);
	inline void SetError(const char* pErrMsg);
public:
	CNeoVM();
	virtual ~CNeoVM();

	template<typename RVal>
	static void push_functorNative(FunctionPtr* pOut, RVal(*func)(CNeoVMWorker*))
	{
		CNeoVMWorker::neo_pushcclosure(pOut, CNeoVMWorker::functorNative<RVal>::invoke, (void*)func);
	}

	template<typename RVal, typename ... Types>
	static int push_functor(FunctionPtr* pOut, RVal(*func)(Types ... args))
	{
		CNeoVMWorker::neo_pushcclosure(pOut, CNeoVMWorker::functor<RVal, Types ...>::invoke, (void*)func);
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
		fun._argCount = iArgCnt;
		if (m_sFunctionPtr[iFID]._argsCount != iArgCnt)
			return false;

		m_sFunctionPtr[iFID]._fun = fun;
		return true;
	}


	template<typename F>
	static FunctionPtr RegisterNative(F func, u8 argCount)
	{
		FunctionPtr fun;
		push_functorNative(&fun, func);
		fun._argCount = argCount;
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
	RVal Call(const std::string& funName, Types ... args)
	{
		return _pMainWorker->Call<RVal>(funName, args...);
	}

	u32 CreateWorker();
	bool ReleaseWorker(u32 id);
	bool BindWorkerFunction(u32 id, const std::string& funName);
	bool IsWorking(u32 id);
	bool UpdateWorker(u32 id, int iTimeout = -1, int iCheckOpCount = 1000);


	inline const char* GetLastErrorMsg() { return _sErrorMsgDetail.c_str();  }
	inline bool IsLastErrorMsg() { return (_sErrorMsgDetail.empty() == false); }
	void ClearLastErrorMsg() { _pErrorMsg = NULL; _sErrorMsgDetail.clear(); }

	static CNeoVM*	LoadVM(void* pBuffer, int iSize);
	static void		ReleaseVM(CNeoVM* pVM);
	static bool		Compile(void* pBufferSrc, int iLenSrc, void* pBufferCode, int iLenCode, int* pLenCode, bool putASM = false, bool allowGlobalInitLogic = true);
};

