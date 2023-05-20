#pragma once

#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMMemoryPool.h"

struct neo_libs;
struct neo_DCalllibs;;
class CNArchive;
class CNeoVMImpl : public INeoVM
{
	friend					CNeoVMWorker;
	friend					TableInfo;
	friend					ListInfo;
	friend					SetInfo;
	friend					neo_libs;
	friend					neo_DCalllibs;
private:


	std::map<u32, ListInfo*> _sLists;
	std::map<u32, TableInfo*> _sTables;
	std::map<u32, SetInfo*> _sSets;
	std::map<u32, StringInfo*> _sStrings;
	u32 _dwLastIDVMWorker = 0;


	std::map<u32, CNeoVMWorker*> _sVMWorkers;

public:
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetStringA(VarInfo *d, const std::string& str);
	void Var_SetTable(VarInfo *d, TableInfo* p);


	CNeoVMWorker* WorkerAlloc(int iStackSize);
	void FreeWorker(CNeoVMWorker *d);
	CNeoVMWorker* FindWorker(int iModule);

	CoroutineInfo* CoroutineAlloc();
	void FreeCoroutine(VarInfo *d);

	StringInfo* StringAlloc(const std::string& str);
	void FreeString(VarInfo *d);

	TableInfo* TableAlloc(int cnt = 0);
	void FreeTable(TableInfo* tbl);

	ListInfo* ListAlloc(int cnt = 0);
	void FreeList(ListInfo* tbl);

	SetInfo* SetAlloc();
	void FreeSet(SetInfo* tbl);

	FunctionPtr* FunctionPtrAlloc(FunctionPtr* pOld);

	int	 Coroutine_Create(int iFID);
	int	 Coroutine_Resume(int iCID);
	int	 Coroutine_Destroy(int iCID);



	inline void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
			Var_Release(v1);
		INeoVM::Move_DestNoRelease(v1, v2);
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

	std::map<void*, FunctionPtr*> m_sCache_FunPtr;

	static bool _funInitLib;
	static FunctionPtrNative _funDefaultLib;
	static FunctionPtrNative _funLstLib;
	static FunctionPtrNative _funStrLib;
	static FunctionPtrNative _funTblLib;
public:
	inline CNeoVMWorker* GetMainWorker() { return (CNeoVMWorker*)_pMainWorker; }

	bool RunFunction(const std::string& funName);


	std::string _sErrorMsgDetail;
	std::string _pErrorMsg;

	static bool IsGlobalLibFun(std::string& FunName);
	void RegLibrary(VarInfo* pSystem, const char* pLibName);// , SNeoFunLib* pFuns);
	static void RegObjLibrary();
	static void InitLib();
	inline void SetError(const std::string& msg);
	virtual int FindFunction(const std::string& name) { return GetMainWorker()->FindFunction(name); }
	virtual bool SetFunction(int iFID, FunctionPtr& fun, int argCount) { return GetMainWorker()->SetFunction(iFID, fun, argCount); }
public:
	CNeoVMImpl();
	virtual ~CNeoVMImpl();

	virtual u32 CreateWorker(int iStackSize);
	virtual bool ReleaseWorker(u32 id);
	virtual bool BindWorkerFunction(u32 id, const std::string& funName);
	virtual bool SetTimeout(u32 id, int iTimeout, int iCheckOpCount);
	virtual bool IsWorking(u32 id);
	virtual bool UpdateWorker(u32 id);


	virtual const char* GetLastErrorMsg() { return _sErrorMsgDetail.c_str();  }
	virtual bool IsLastErrorMsg() { return (_sErrorMsgDetail.empty() == false); }
	virtual void ClearLastErrorMsg() { _bError = false; _sErrorMsgDetail.clear(); }

	virtual INeoVMWorker*	LoadVM(void* pBuffer, int iSize, bool blMainWorker, int iStackSize); // 0 is error
	virtual bool PCall(int iModule);
};

