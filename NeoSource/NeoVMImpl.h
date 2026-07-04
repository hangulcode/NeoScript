#pragma once

#include <thread>

#include "NeoVMInternal.h"


namespace NeoScript
{


class CNeoVMImpl : public INeoVM
{
//	friend					CNeoVMWorker;
	friend					MapInfo;
	friend					ListInfo;
	friend					SetInfo;
	friend					neo_libs;
	friend					neo_DCalllibs;
private:


	// 살아있는 List/Map/Set 를 intrusive 이중연결 리스트로 추적 (종료 시 _Bucket 해제용).
	// 기존 std::map<ID,ptr> 레지스트리 대체 — 할당/해제당 트리 연산 2~3회를 O(1) 링크로 교체.
	// String 은 CNVMInstPool(소멸자 지원)이라 별도 추적 불필요 → 레지스트리 제거.
	ListInfo* _sListHead = nullptr;
	MapInfo* _sTableHead = nullptr;
	SetInfo* _sSetHead = nullptr;
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

	NeoExecContextPool* _pExecPool = nullptr;             // 이 VM 의 실행 컨텍스트 풀(로드 시 주입)
	NeoExecContextPool* GetExecPool() { return _pExecPool; }

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
		{
			if (v1 == v2)
				return;
			Var_ReleaseInternal(v1);
		}

		if (v2->IsAllocType() == false)
			*v1 = *v2;
		else
			Move_DestNoRelease(v1, v2);
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
	// 코루틴 컨텍스트는 공유 실행 컨텍스트 풀(_pExecPool)로 통합됨 → per-VM 풀 제거.

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
	virtual int FindFunction(const std::string& name);
	virtual bool SetFunction(int iFID, FunctionPtr& fun, int argCount);
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

	virtual INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker, bool init, int iStackSize); // 0 is error
	virtual bool PCall(int iModule);
};

};
