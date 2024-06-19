#pragma once

#include <thread>

#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMMemoryPool.h"
#include "NeoQueue.h"

namespace NeoScript
{
struct neo_libs;
struct neo_DCalllibs;;
class CNArchive;

struct SystemFun
{
	std::string fname;
	int			argCount;
};

class CNeoVMImpl : public INeoVM
{
	friend					CNeoVMWorker;
	friend					MapInfo;
	friend					ListInfo;
	friend					SetInfo;
	friend					neo_libs;
	friend					neo_DCalllibs;
private:


	std::map<u32, ListInfo*> _sLists;
	std::map<u32, MapInfo*> _sTables;
	std::map<u32, SetInfo*> _sSets;
	std::map<u32, StringInfo*> _sStrings;
	u32 _dwLastIDVMWorker = 0;


	std::map<u32, CNeoVMWorker*> _sVMWorkers;
	std::thread*	_job = nullptr;
	NeoThreadSafeQueue<AsyncInfo*> _job_queue;
	bool						_job_end = false;
	NeoThreadSafeQueue<AsyncInfo*> _job_completed;
public:
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetStringA(VarInfo *d, const std::string& str);
	void Var_SetTable(VarInfo *d, MapInfo* p);


	CNeoVMWorker* WorkerAlloc(int iStackSize);
	void FreeWorker(CNeoVMWorker *d);
	CNeoVMWorker* FindWorker(int iModule);

	CoroutineInfo* CoroutineAlloc();
	void FreeCoroutine(VarInfo *d);

	StringInfo* StringAlloc(const std::string& str);
	void FreeString(VarInfo *d);

	MapInfo* TableAlloc(int cnt = 0);
	void FreeTable(MapInfo* tbl);

	ListInfo* ListAlloc(int cnt = 0);
	void FreeList(ListInfo* tbl);

	SetInfo* SetAlloc();
	void FreeSet(SetInfo* tbl);

	AsyncInfo* AsyncAlloc();
	void FreeAsync(VarInfo* d);

	FunctionPtr* FunctionPtrAlloc(FunctionPtr* pOld);

	void ThreadFunction();
	void AddHttp_Request(AsyncInfo* p);
	AsyncInfo* Pop_AsyncInfo();



	NEOS_FORCEINLINE void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
			Var_ReleaseInternal(v1);

		if (v2->IsAllocType() == false)
			*v1 = *v2;
		else
			CNeoVMWorker::Move_DestNoRelease(v1, v2);
	}


	NEOS_FORCEINLINE void Var_Release(VarInfo *d)
	{
		if (d->IsAllocType())
			Var_ReleaseInternal(d);
		else
			d->ClearType();
	}

	VarInfo m_sDefaultValue[NDF_MAX];
	
	CNVMAllocPool < MapNode, 32> m_sPool_TableNode;
	CNVMAllocPool< MapInfo, 32> m_sPool_TableInfo;
	CNVMAllocPool < SetNode, 32> m_sPool_SetNode;
	CNVMAllocPool< SetInfo, 32> m_sPool_SetInfo;
	CNVMAllocPool< ListInfo, 32> m_sPool_ListInfo;

	CNVMInstPool< AsyncInfo, 32> m_sPool_Async;
	CNVMInstPool< StringInfo, 32> m_sPool_String;
	CNVMInstPool< CoroutineInfo, 32> m_sPool_Coroutine;

	std::map<void*, FunctionPtr*> m_sCache_FunPtr;

	static bool _funInitLib;
	static FunctionPtrNative _funLib_Default;
	static FunctionPtrNative _funLib_List;
	static FunctionPtrNative _funLib_String;
	static FunctionPtrNative _funLib_Map;
	static FunctionPtrNative _funLib_Async;
public:
	NEOS_FORCEINLINE CNeoVMWorker* GetMainWorker() { return (CNeoVMWorker*)_pMainWorker; }

	bool RunFunction(const std::string& funName);


	std::string _sErrorMsgDetail;
	std::string _pErrorMsg;

	static bool IsGlobalLibFun(std::string& FunName);
	static const std::list< SystemFun>* GetSystemModule(const std::string& module);
	void RegLibrary(VarInfo* pSystem, const char* pLibName);// , SNeoFunLib* pFuns);
	static void RegObjLibrary();
	static void InitLib();
	void SetError(const std::string& msg);
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

	virtual INeoVMWorker*	LoadVM(void* pBuffer, int iSize, bool blMainWorker, bool init, int iStackSize); // 0 is error
	virtual bool PCall(int iModule);
};

};