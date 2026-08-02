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


	// 워커는 게임 오브젝트 단위로 수천~수만 개가 살아있고 프레임당 수십~수백 개가
	// 생성/삭제된다. std::map 은 조회마다 트리를 log n 단계 타서(=캐시 미스) 불리하다.
	std::unordered_map<u32, CNeoVMWorker*> _sVMWorkers;
	std::thread*	_job = nullptr;
	NeoThreadSafeQueue<AsyncInfo*> _job_queue;
	bool						_job_end = false;
	NeoThreadSafeQueue<AsyncInfo*> _job_completed;
	SNeoVMAllocStats m_sAllocStats;
	SNeoVMAllocStats m_sPublishedAllocStats;
public:
	void PublishAllocStats();
	void GetAllocStats(SNeoVMAllocStats& outStats) const { outStats = m_sAllocStats; }
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

	VecInfo* VecAlloc();
	void FreeVec(VecInfo* p);
	// 공유 중이면 복제해서 단독 소유로 만든다(성분 쓰기 직전에 호출). 값 의미론 보존용.
	VecInfo* VecCopyOnWrite(VarInfo* d);

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
	AsyncInfo* Pop_AsyncInfo(CNeoVMWorker* pOwnerWorker);
	void DiscardOrphanedAsync();              // 주인이 사라진 완료 async 폐기
	// 워커 소멸 시 호출. 완료 큐의 것은 즉시 폐기하고, 아직 처리 중인 수를 고아 카운터에 넘긴다.
	void DiscardAsyncByWorker(u32 workerId, int pendingCount);
	// 주인이 사라진 채 아직 완료되지 않은 async 의 수.
	// 0 이면 고아가 존재할 수 없으므로 Pop_AsyncInfo 가 검사를 건너뛴다.
	// (워커를 자주 만들고 지워도, async 를 쓴 워커가 없으면 계속 0 이다)
	int _orphanAsyncCount = 0;



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

	CNVMAllocPool< VecInfo, 32> m_sPool_Vec;

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
	static int FindDefaultNativeIndex(const VMString* pStr);
	// 프로그램 로드 시 코드 패치용 — StringInfo 없이 상수 문자열로 조회한다.
	static int FindDefaultNativeIndex(const std::string& name);
	static int GetDefaultNativeIntrinsic(int nativeIndex);
	static bool CallDefaultNativeByIndex(int nativeIndex, CNeoVMWorker* pWorker, short args);
	void RegLibrary(VarInfo* pSystem, const char* pLibName);// , SNeoFunLib* pFuns);
	static void RegObjLibrary();
	static void InitLib();
	void SetError(const std::string& msg);
	virtual int FindFunction(const std::string& name);
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
	virtual INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, bool blMainWorker, bool init, int iStackSize);
	virtual bool PCall(int iModule);
};

};
