#pragma once

#include "NeoVMWorker.h"
class CNArchive;
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
	int	_BytesSize;


	void Var_AddRef(VarInfo *d);
	void Var_Release(VarInfo *d);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetTable(VarInfo *d, TableInfo* p);


	CNeoVMWorker* WorkerAlloc(int iStackSize);
	void FreeWorker(CNeoVMWorker *d);

	StringInfo* StringAlloc(const char* str);
	void FreeString(VarInfo *d);

	TableInfo* TableAlloc();
	void FreeTable(VarInfo *d);

public:

	bool RunFunction(const std::string& funName);
	inline int GetBytesSize() { return _BytesSize; }


	std::string _sErrorMsgDetail;
	const char* _pErrorMsg = NULL;

	void RegLibrary(VarInfo* pSystem, const char* pLibName, SFunLib* pFuns);
	void InitLib();
	bool Init(void* pBuffer, int iSize, int iStackSize);
	inline void SetError(const char* pErrMsg);
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
	bool Call_TL(int iTimeout, int iCheckOpCount, RVal* r, const std::string& funName, Types ... args) // Time Limit
	{
		int iFID = -1;
		auto it = m_sImExportTable.find(funName);
		if (it == m_sImExportTable.end())
			return false;

		iFID = (*it).second;

		return _pMainWorker->Call_TL<RVal>(iTimeout, iCheckOpCount, r, iFID, args...);
	}

	template<typename ... Types>
	bool CallN_TL(int iTimeout, int iCheckOpCount, const std::string& funName, Types ... args) // Time Limit
	{
		int iFID = -1;
		auto it = m_sImExportTable.find(funName);
		if (it == m_sImExportTable.end())
			return false;

		iFID = (*it).second;

		return _pMainWorker->CallN_TL(iTimeout, iCheckOpCount, iFID, args...);
	}

	u32 CreateWorker(int iStackSize = 50 * 1024);
	bool ReleaseWorker(u32 id);
	bool BindWorkerFunction(u32 id, const std::string& funName);
	bool IsWorking(u32 id);
	bool UpdateWorker(u32 id, int iTimeout = -1, int iCheckOpCount = 1000);


	inline const char* GetLastErrorMsg() { return _sErrorMsgDetail.c_str();  }
	inline bool IsLastErrorMsg() { return (_sErrorMsgDetail.empty() == false); }
	void ClearLastErrorMsg() { _pErrorMsg = NULL; _sErrorMsgDetail.clear(); }

	static CNeoVM*	LoadVM(void* pBuffer, int iSize, int iStackSize = 50 * 1024);
	static void		ReleaseVM(CNeoVM* pVM);
	static bool		Compile(void* pBufferSrc, int iLenSrc, CNArchive& arw, std::string& err, bool putASM = false, bool allowGlobalInitLogic = true);

	static CNeoVM*	CompileAndLoadVM(void* pBufferSrc, int iLenSrc, std::string& err, bool putASM = false, bool allowGlobalInitLogic = true, int iStackSize = 50 * 1024);
};

