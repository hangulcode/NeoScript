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
	// 컨테이너 파괴는 자식 Var_Release 로 다시 컨테이너 파괴를 유발한다. 호출 스택 대신
	// 이 대기열을 깊이 우선으로 비워서, 깊은 트리와 순환 모두 안전하게 처리한다.
	std::vector<VarInfo> _sDestroyQueue;
	bool _bDrainingDestroyQueue = false;
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
	// VM-local weak string interner. Open-addressed slots own pointers only:
	// a StringInfo remains here exactly while script values retain it, so unique
	// dynamic strings do not accumulate for the lifetime of the VM.
	std::vector<StringInfo*> m_sStringIntern;
	int m_sStringInternCount = 0;
	StringInfo* FindInternedString(const std::string& str, u32 hash) const;
	void InsertInternedString(StringInfo* p);
	void RemoveInternedString(StringInfo* p);
	void RehashStringIntern(int capacity);
public:
	void PublishAllocStats();
	// 완전히 빈 페이지를 OS 로 돌려준다. 반환값 = 돌려준 바이트.
	// 빈 페이지를 Collect 가 처음 본 뒤 m_iEmptyPageHoldMs 가 지나야 대상이 된다
	// (경계에서 free/malloc 반복 방지). 빈 시각 기록이 지연되는 이유는 .cpp 참고.
	// force=false 는 한 번에 m_iTrimPagesPerCall 장까지만 해제하고, 빈 페이지가 없으면
	// 시계도 안 읽고 빠진다 — 매 프레임 불러서 조금씩 돌려주는 용도다.
	long long CollectEmptyPages(bool force = false);
	// 어느 풀이든 완전히 빈 페이지가 있는가(포인터 비교 8번). 매 프레임 경로의 조기 반환용.
	bool AnyEmptyPages() const
	{
		return m_sPool_TableData.HasEmptyPages() || m_sPool_TableInfo.HasEmptyPages()
			|| m_sPool_FunctionProperty.HasEmptyPages()
			|| m_sPool_SetData.HasEmptyPages()
			|| m_sPool_SetInfo.HasEmptyPages()
			|| m_sPool_ListInfo.HasEmptyPages()  || m_sPool_Vec.HasEmptyPages()
			|| m_sPool_Async.HasEmptyPages()     || m_sPool_String.HasEmptyPages();
	}
	void SetEmptyPageHoldSeconds(float sec) { m_iEmptyPageHoldMs = (sec <= 0.0f) ? 0 : (int)(sec * 1000.0f); }
	float GetEmptyPageHoldSeconds() const { return m_iEmptyPageHoldMs / 1000.0f; }
	// 한 번의 CollectEmptyPages(false) 가 해제할 페이지 수 상한. 0 이하 = 상한 없음.
	void SetTrimPagesPerCall(int pages) { m_iTrimPagesPerCall = pages; }
	int GetTrimPagesPerCall() const { return m_iTrimPagesPerCall; }

	long long PoolBytes() const;   // 이 VM 의 오브젝트 풀 8개가 확보한 총 바이트
	// 유휴 문자열 노드가 붙들고 있는 문자 버퍼 합계(풀 페이지 밖의 힙).
	// free 리스트를 훑으므로 통계 조회 시점에만 부른다.
	long long StringIdleBytes();
	// 조회 시점. 여기서만 stringIdleBytes 를 새로 재고 전역에도 반영한다.
	void GetAllocStats(SNeoVMAllocStats& outStats);
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

	// 일반 문자열: 해시와 인터너 작업을 지연한다. 임시 문자열 핫패스용.
	StringInfo* StringAlloc(const std::string& str);
	// 정규 문자열: 프로그램 상수와 Map/Set 키용. 같은 VM 안에서 내용당 하나다.
	StringInfo* StringIntern(const std::string& str);
	// Finds an already-live canonical string without creating or retaining one.
	StringInfo* StringFind(const std::string& str) const;
	// Reuses an existing object's lazy hash cache for dynamic Map/Set keys.
	StringInfo* StringFind(StringInfo* pString) const;
	void FreeString(VarInfo *d);

	VecInfo* VecAlloc();
	void FreeVec(VecInfo* p);
	// 공유 중이면 복제해서 단독 소유로 만든다(성분 쓰기 직전에 호출). 값 의미론 보존용.
	VecInfo* VecCopyOnWrite(VarInfo* d);

	MapInfo* TableAlloc(int cnt = 0);
	void FreeTable(MapInfo* tbl);
	FunctionPropertyInfo* FunctionPropertyAlloc();
	void FreeFunctionProperty(FunctionPropertyInfo* fp);

	ListInfo* ListAlloc(int cnt = 0);
	void FreeList(ListInfo* tbl);

	SetInfo* SetAlloc();
	void FreeSet(SetInfo* tbl);
	void QueueContainerForDestroy(const VarInfo& value);

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
	
	// 페이지 크기는 고정이다(배증 없음 — 마지막 한 장이 필요량을 크게 넘겨 잡던 문제).
	// 실측 노드 크기(헤더 포함)를 기준으로 한 장이 대략 3~13KB 가 되게 잡았다.
	// 개수가 적은 타입은 더 작게 — 안 쓰는 타입이 페이지 한 장을 통째로 물지 않게 한다.
	//   MapData 32B x 256 = 8KB  SetData 16B x 256 = 4KB  MapInfo 100B x 64 = 6.4KB
	//   SetInfo  100B x 32 = 3.2KB
	//   SetInfo  100B x 32 = 3.2KB
	//   ListInfo 148B x 64 =  9.5KB  VecInfo   24B x 512 = 12.3KB
	//   StringInfo 100B x 128 = 12.8KB  AsyncInfo 372B x 16 = 6.0KB
	// MapNode/SetNode 자체는 컬렉션별 연속 배열에 있고, key/value는 안정 주소의 별도 풀에 둔다.
	CNVMAllocPool < MapData, 256> m_sPool_TableData;
	CNVMAllocPool< MapInfo, 64> m_sPool_TableInfo;
	CNVMAllocPool< FunctionPropertyInfo, 64> m_sPool_FunctionProperty;
	CNVMAllocPool < SetData, 256> m_sPool_SetData;
	CNVMAllocPool< SetInfo, 32> m_sPool_SetInfo;
	CNVMAllocPool< ListInfo, 64> m_sPool_ListInfo;

	CNVMAllocPool< VecInfo, 512> m_sPool_Vec;

	CNVMInstPool< AsyncInfo, 16> m_sPool_Async;
	CNVMInstPool< StringInfo, 128> m_sPool_String;

	// 빈 페이지 보유 시간(기본 5초).
	int m_iEmptyPageHoldMs = 5000;
	// 매 프레임 호출을 전제로 한 회수 예산. 4장/호출 = 60fps 에서 초당 240장이니
	// 9MB 규모(약 700장)도 3초 남짓에 다 돌아가면서 프레임당 비용은 상한선 안이다.
	int m_iTrimPagesPerCall = 4;
	// 라운드로빈 시작 풀. 앞쪽 풀이 예산을 독식해 뒤쪽이 굶는 것을 막는다.
	int m_iTrimPoolCursor = 0;
	static const int kTrimPoolCount = 9;
	size_t CollectPoolAt(int idx, NeoPoolClock::time_point now, int holdMs, int& pageBudget);
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
	// _pErrorMsg 까지 비운다: 이게 남아 있으면 SetError 가 계속 무시되고(첫 에러 고정),
	// UpdateWorker 가 영구히 false 를 반환한다(시분할 실행이 첫 에러 이후 죽어버림).
	virtual void ClearLastErrorMsg() { _bError = false; _sErrorMsgDetail.clear(); _pErrorMsg.clear(); }
	virtual void SetLastErrorMsg(const char* msg) { SetError(msg != nullptr ? std::string(msg) : std::string()); }

	virtual INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker, bool init, int iStackSize); // 0 is error
	virtual INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, bool blMainWorker, bool init, int iStackSize);
	virtual bool PCall(int iModule);
};

};
