#pragma once

#include <thread>
#include <time.h>

#include "NeoConfig.h"
#include "NeoVM.h"
//#include "NeoVMWorker.h"
#include "NeoVMMemoryPool.h"
#include "NeoQueue.h"

#include "NeoVMMap.h"
#include "NeoVMSet.h"
#include "NeoVMList.h"


namespace NeoScript
{
struct neo_libs;
struct neo_DCalllibs;;
class CNArchive;
class CNeoVMWorker;

// CNeoVM is intentionally incomplete in this header.  The non-interned
// string slow path uses this small bridge so MapInfo::FindString can remain
// inline for the usual (already canonical) key path.
StringInfo* FindCanonicalString(CNeoVM* pVM, StringInfo* pString);

struct SystemFun
{
	std::string fname;
	int			argCount;    // 파라미터 개수에서 유도. -1 = 가변("..." 포함 시)
	int			nativeIndex;
	std::string ret;                   // 리턴 타입 ("void","float","list" …)
	std::vector<std::string> params;   // "float x" 형태의 타입+이름. 개수가 곧 인자 수
};


enum eNeoDefaultString
{
	NDF_NULL,
	NDF_INT,
	NDF_FLOAT,
	NDF_BOOL,
	NDF_STRING,
	NDF_TABLE,
	NDF_LIST,
	NDF_SET,
	NDF_COROUTINE,
	NDF_FUNCTION,
	NDF_MODULE,
	NDF_ASYNC,

	NDF_TRUE,
	NDF_FALSE,

	NDF_SUSPENDED,
	NDF_RUNNING,
	NDF_DEAD,
	NDF_NORMAL,

	NDF_VEC2,
	NDF_VEC3,
	NDF_VEC4,
	NDF_QUAT,

	NDF_MAX
};

typedef u8	OpType;
typedef u8	ArgFlag;

#define NEOS_ARG_N1_IMMEDIATE	(1 << 5)
#define NEOS_ARG_N2_IMMEDIATE	(1 << 4)
#define NEOS_ARG_N3_IMMEDIATE	(1 << 3)
#define NEOS_ARG_N1_LOCAL		(1 << 2)
#define NEOS_ARG_N2_LOCAL		(1 << 1)
#define NEOS_ARG_N3_LOCAL		(1 << 0)
// 호출/복귀 op 전용: 반환값 슬롯을 쓰지 않음(=해당 오퍼랜드를 페치하지 않음).
// PatchLocalOps 도 이 비트를 봐야 해서 argFlag 비트 정의와 같은 곳에 둔다.
#define NEOS_OP_CALL_NORESULT	(1 << 7) // 0x80

// NOP_JMP_RANGE_* descriptor 전용 플래그. opcode의 argFlag와 분리해
// lower/upper가 각각 stack인지 global/static인지와 경계 포함 여부를 보관한다.
#define NEOS_RANGE_LOWER_LOCAL		(1 << 0)
#define NEOS_RANGE_UPPER_LOCAL		(1 << 1)
#define NEOS_RANGE_LOWER_INCLUSIVE	(1 << 2)
#define NEOS_RANGE_UPPER_INCLUSIVE	(1 << 3)
#define NEOS_RANGE_UPPER_FIRST		(1 << 4)

enum eNOperation : OpType
{
	NOP_MOV,
	NOP_MOV_L,
	NOP_MOVI,
	NOP_MOVI_L,
	NOP_MOVF,
	NOP_MOVF_L,

	NOP_MOV_MINUS,
	NOP_MOV_MINUS_L,
	NOP_LOG_NOT,	// !
	NOP_ADD2,		// +
	NOP_ADD2_L,
	NOP_SUB2,		// -
	NOP_MUL2,		// *
	NOP_DIV2,		// /
	NOP_PERSENT2,	// %
	NOP_LSHIFT2,	// <<
	NOP_RSHIFT2,	// >>
	NOP_AND2,		// &
	NOP_OR2,		// |
	NOP_XOR2,		// ^
	//NOP_NOT2,		// ~
	//NOP_LOG_NOT2,	// ! not support ( a != 0x01; if(a != b) ... bit / logic )

	NOP_VAR_CLEAR,
	NOP_INC,
	NOP_DEC,

	NOP_ADD3,
	NOP_ADD3_L,
	NOP_SUB3,
	NOP_SUB3_L,
	NOP_MUL3,
	NOP_MUL3_L,
	NOP_DIV3,
	NOP_PERSENT3,
	NOP_LSHIFT3,
	NOP_RSHIFT3,
	NOP_AND3,
	NOP_OR3,
	NOP_XOR3,
	//NOP_NOT3,

	NOP_GREAT,		// >
	NOP_GREAT_EQ,	// >=
	NOP_LESS,		// <
	NOP_LESS_EQ,	// <=
	NOP_EQUAL2,		// ==
	NOP_NEQUAL,		// !=
	NOP_AND,		// &
	NOP_AND_L,
	NOP_OR,			// |
	NOP_LOG_AND,	// &&
	NOP_LOG_OR,	// ||

	NOP_JMP,
	NOP_JMP_FALSE,
	NOP_JMP_TRUE,

	NOP_JMP_GREAT,		// >
	NOP_JMP_GREAT_L,
	NOP_JMP_GREAT_EQ,	// >=
	NOP_JMP_GREAT_EQ_L,
	NOP_JMP_LESS,		// <
	NOP_JMP_LESS_L,
	NOP_JMP_LESS_EQ,	// <=
	NOP_JMP_LESS_EQ_L,
	NOP_JMP_EQUAL2,	// ==
	NOP_JMP_EQUAL2_L,
	NOP_JMP_NEQUAL,	// !=
	NOP_JMP_NEQUAL_L,
	NOP_JMP_AND,	// &&
	NOP_JMP_OR,		// ||
	NOP_JMP_NAND,	// !(&&)
	NOP_JMP_NOR,	// !(||)
	NOP_JMP_FOR,	// for (step 부호를 런타임에 보고 방향 결정 — 양수/음수/변수 모두 처리)
	NOP_JMP_FOREACH,// foreach
	NOP_SWITCH,		// n1 = switch table index, n2 = 조건식 위치. 매칭 offset(op 단위)만큼 상대 점프

	NOP_STR_ADD,	// ..
	NOP_STR_ADD_L,

	NOP_TOSTRING,
	NOP_TOINT,
	NOP_TOINT_L,
	NOP_TOFLOAT,
	NOP_TOSIZE,
	NOP_GETTYPE,
	NOP_SLEEP, // arg1 unuse

	NOP_FMOV1, // Function Info Load
	NOP_FMOV2, // Function Info Load

	NOP_CALL,
	NOP_CALL_L,
	NOP_PTRCALL,	// Multiple Call
	NOP_PTRCALL_L,
	NOP_PTRCALL2,	// Native Call
	NOP_PTRCALL2_L,
	NOP_NATIVECALL,	// Native Call (patched at LoadVM)
	NOP_RETURN,
	NOP_RETURN_L,
	//	NOP_FUNEND,

	NOP_TABLE_ALLOC,
	NOP_CLT_READ,
	NOP_CLT_READ_L,
	NOP_TABLE_REMOVE,
	NOP_CLT_MOV,
	NOP_CLT_MOV_L,

	NOP_TABLE_ADD2,
	NOP_TABLE_SUB2,
	NOP_TABLE_MUL2,
	NOP_TABLE_DIV2,
	NOP_TABLE_PERSENT2,

	NOP_LIST_ALLOC,
	NOP_LIST_ALLOC_L,
	NOP_LIST_REMOVE,

	NOP_VERIFY_TYPE,
	NOP_CHANGE_INT,
	NOP_CHANGE_INT_L,
	NOP_YIELD,
	NOP_IDLE,

	// math.Vector2/3/4/Quaternion 생성 intrinsic. LoadVM 에서 PTRCALL2(#Vector*) 를 패치.
	// native 호출 프레임 없이 인자 슬롯에서 직접 벡터를 만든다(n2=인자수=성분수, n3=결과).
	// 성분 수가 타입이 아니라 데이터라 op 하나로 충분하다(넷을 두면 디스패치 루프만 커진다).
	NOP_VEC_MAKE,


	// 새 opcode는 끝에 붙여 기존 저장 스크립트 이미지의 opcode 값을 보존한다.
	// 로드 시 n2가 static 문자열 상수인 READ만 이 opcode로 치환한다.
	// 키 StringInfo는 워커별 static 슬롯에서 매번 얻으며 Map/value 포인터는 보관하지 않는다.
	NOP_CLT_READ_STATIC_STRING,
	// n1 = relative jump, n2 = 검사 값, n3 = Program range descriptor index.
	// _L 두 개는 Program load 시 검사 값과 양 경계가 모두 local일 때만 치환된다.
	NOP_JMP_RANGE_INSIDE,
	NOP_JMP_RANGE_OUTSIDE,
	NOP_JMP_RANGE_INSIDE_L,
	NOP_JMP_RANGE_OUTSIDE_L,

	NOP_NONE,
	NOP_ERROR,

	NOP_MAX,
}; // Operation length

enum eYield_Type : s16
{
	YILED_RETURN,
};

#define CODE_TO_NOP(op) (eNOperation)(op)// >> 2)
#define CODE_TO_LEN(op) 3 //(op & 0x03)

#pragma pack(1)
struct SVMOperation
{
	eNOperation	  op;
	u8		argFlag;
	short	n1;

	union
	{
		struct
		{
			short	n2;
			short	n3;
		};
		int	n23;
	};
};
#pragma pack()

struct VarInfo;
struct SVarWrapper;
class CNeoVMWorker;
struct FunctionPtr;



struct SCallStack
{
	int		_iSP_Vars;
	int		_iSP_VarsMax;
	u32		_ReturnIP;
	// >= 0: 절대 실행 스택 인덱스, -1: 반환값 무시,
	// <= -2: 전역 인덱스(-_iReturnValueIndex - 2).
	int	 _iReturnValueIndex = -1;
};

static constexpr u32 FLAG_CLOSURE = 1u << 31;  // 호출자가 closure였음 → 보조스택에서 복원
static constexpr u32 FLAG_ASYNCWAIT = 1u << 30; // async 콜백 프레임 → wait() 반환슬롯 복원
static constexpr u32 IP_MASK = 0x3fffffffu;

// 일반 호출 프레임에는 closure 포인터를 넣지 않는다. 캡처 함수가 다른 함수를
// 호출할 때만 이 보조 스택에 부모 프레임을 보관한다.
struct SClosureCallState
{
	ClosureInfo* _closure = nullptr;
	int		_iSP_Vars = 0;
};

// async.wait가 시작한 callback 프레임만 원래 native 반환 슬롯을 되돌린다.
// 일반 호출 프레임의 공통 필드가 아니라 실행 컨텍스트별 LIFO 보관함에 둔다.
struct SAsyncWaitReturnState
{
	VarInfo* _pReturnValue = nullptr;
	bool	 _returnValue = false;
};

enum COROUTINE_STATE
{
	COROUTINE_STATE_SUSPENDED,
	COROUTINE_STATE_RUNNING,
	COROUTINE_STATE_DEAD,
	COROUTINE_STATE_NORMAL,
};
enum COROUTINE_SUB_STATE
{
	COROUTINE_SUB_NORMAL,
	COROUTINE_SUB_START,
	COROUTINE_SUB_CLOSE,
};

enum ASYNC_STATE
{
	ASYNC_READY,
	ASYNC_PENDING,
	ASYNC_COMPLETED,
};

enum ASYNC_COMMAND
{
	ASYNC_GET,
	ASYNC_POST,
	ASYNC_POST_JSON,
};



struct CoroutineBase
{
	int			_iSP_Vars;
	int			_iSP_Vars_Max2;
	int			_iSP_VarsMax;
	// 코드는 CNeoVMProgram 이 소유하는 불변 이미지다. IP 는 읽기 전용 포인터.
	const SVMOperation* _pCodeCurrent;
	void ClearSP()
	{
		_iSP_Vars = 0;
		_iSP_Vars_Max2 = 0;
		_iSP_VarsMax = 0;
	}
};

struct AsyncResumeInfo
{
	int		codePtr = 0;
	VarInfo*	pReturnValue = nullptr;
	bool		returnValue = false;
};

struct CoroutineInfo : AllocBase
{
	//	int	_CoroutineID;
	int	_fun_index;
	// coroutine.create가 받은 호출 가능 값. VAR_CLOSURE면 캡처 보관함의 소유권도 가진다.
	VarInfo	_function;
	// yield로 컨텍스트를 바꿀 때 현재 값-캡처 프레임 보유분을 함께 옮긴다.
	ClosureInfo*	_activeClosure = nullptr;
	COROUTINE_STATE _state;
	COROUTINE_SUB_STATE _sub_state;

	CoroutineBase _info;
	// async.wait가 Idle dispatcher를 거쳐 돌아갈 원래 IP. 실행 컨텍스트마다
	// 보관해 중첩 async 대기가 공유 바이트코드를 패치하지 않도록 한다.
	std::vector<AsyncResumeInfo>	m_sAsyncResumeCodePtrs;

	std::vector<VarInfo>	m_sVarStack;
	SimpleVector<SCallStack>	m_sCallStack;
	SimpleVector<SClosureCallState>	m_sClosureCallStack;
	SimpleVector<SAsyncWaitReturnState>	m_sAsyncWaitReturnStack;

	// 파괴 재진입 방지와 순환 후보 intrusive FIFO 링크. 실행 스택의 저장 경로가
	// 여러 곳에 분산돼 있어 대여 시 _mayContainContainerChild는 보수적으로 true로 둔다.
	CycleState<CoroutineInfo> _cycleState;
};

// 실행 컨텍스트(파이버) = var 스택 + 콜 스택 + SP/IP 레지스터.
// default(메인) 실행과 코루틴이 동일 타입·동일 풀을 공유한다.
typedef CoroutineInfo NeoExecContext;

// 실행 컨텍스트 풀. 호스트(엔진)가 스레드별로 소유(thread_local)하고 VM 로드 시 주입한다.
// 스레드 귀속 사용 전제라 내부 동기화가 없다. 재사용 fast path는 free-list pop/push이며, 확장은 할당할 수 있다.
// 실행 컨텍스트 풀이 확보한 총 바이트를 전역에 반영한다(풀은 스레드별이라 각자 델타를 publish).
void NeoExecPool_PublishBytes(long long delta);

struct NeoExecContextPool
{
	// CoroutineInfo 132B x 16 = 2.1KB. 실행 컨텍스트는 동시 실행 수만큼만 필요해 작게 잡는다
	// (var 스택 800KB 는 Acquire 때 resize 되므로 페이지 생성 자체는 가볍다).
	CNVMInstPool<CoroutineInfo, 16> _pool;
	int _varStackSize;
	// 이 풀이 한 번이라도 대여해 준 컨텍스트들. 풀 페이지는 소멸까지 해제되지 않으므로 포인터가 안정적이다.
	// 컨텍스트 1개의 var 스택만 50K * sizeof(VarInfo) 라 풀 노드 자체보다 훨씬 크다 — 따로 세야 한다.
	std::vector<NeoExecContext*> _warmCtx;
	long long _publishedBytes = 0;

	// Acquire 중 vector 확장이 실패해도 Receive한 슬롯을 되돌린다. 예외 경계는
	// 정상 경로의 코드 배치를 키우므로, catch 대신 소멸자에서 처리한다.
	struct AcquiredContextGuard
	{
		CNVMInstPool<CoroutineInfo, 16>* pool;
		NeoExecContext* context;

		AcquiredContextGuard(CNVMInstPool<CoroutineInfo, 16>* p, NeoExecContext* c)
			: pool(p), context(c) {}
		~AcquiredContextGuard()
		{
			if (context != NULL)
				pool->Confer(context);
		}
		AcquiredContextGuard(const AcquiredContextGuard&) = delete;
		AcquiredContextGuard& operator=(const AcquiredContextGuard&) = delete;

		NeoExecContext* Release()
		{
			NeoExecContext* result = context;
			context = NULL;
			return result;
		}
	};

	NeoExecContextPool(int varStackSize = 50 * 1024) : _varStackSize(varStackSize < 100 ? 100 : varStackSize) {}
	~NeoExecContextPool()
	{
		NeoExecPool_PublishBytes(-_publishedBytes);
	}

	NeoExecContext* Acquire()
	{
		NeoExecContext* p = _pool.Receive();
		AcquiredContextGuard guard(&_pool, p);
		const bool cold = p->m_sVarStack.capacity() == 0;   // 처음 대여되는 노드 = 스택 힙이 지금 잡힌다
		// cold 컨텍스트는 성공 시 반드시 warm 목록에도 등록돼야 한다. 용량 확보를
		// 먼저 해 두면 아래 모든 스택 할당이 성공한 뒤 push_back은 던지지 않는다.
		if (cold)
			_warmCtx.reserve(_warmCtx.size() + 1);

		// 실행 컨텍스트 풀은 VM 사이에서도 재사용된다. 이전 코루틴의 약한 순환
		// 후보/파괴 상태가 남아 있으면 새 대여자가 stale ticket을 건드릴 수 있으므로
		// 대여 경계에서 항상 초기화한다.
		p->_cycleState.Init(p);
		p->_cycleState._mayContainContainerChild = true;
		p->_info._pCodeCurrent = NULL;
		p->_info.ClearSP();
		p->m_sAsyncResumeCodePtrs.clear();
		p->_state = COROUTINE_STATE_RUNNING;
		p->_sub_state = COROUTINE_SUB_NORMAL;
		p->m_sCallStack.reserve(1000);
		p->m_sCallStack.clear();
		p->m_sClosureCallStack.reserve(8);
		p->m_sClosureCallStack.clear();
		p->m_sAsyncWaitReturnStack.reserve(8);
		p->m_sAsyncWaitReturnStack.clear();
		p->m_sVarStack.resize(_varStackSize);
		if (cold)
		{
			// reserve를 위에서 끝냈으므로 포인터 삽입은 no-throw다.
			_warmCtx.push_back(p);
			PublishBytes();
		}
		return guard.Release();
	}
	// 반납 전 사용 슬롯의 VarInfo 참조 정리는 호출측(워커)이 수행한다(Var_Release 가 워커 멤버라서).
	void Release(NeoExecContext* p)
	{
		_pool.Confer(p);
	}

	// 풀 페이지 + 각 컨텍스트가 따로 들고 있는 스택 힙의 총 확보 용량.
	long long ReservedBytes() const
	{
		long long n = (long long)_pool.ReservedBytes();
		for (NeoExecContext* p : _warmCtx)
		{
			n += (long long)p->m_sVarStack.capacity() * sizeof(VarInfo);
			n += (long long)p->m_sCallStack.capacity() * sizeof(SCallStack);
			n += (long long)p->m_sClosureCallStack.capacity() * sizeof(SClosureCallState);
			n += (long long)p->m_sAsyncWaitReturnStack.capacity() * sizeof(SAsyncWaitReturnState);
			n += (long long)p->m_sAsyncResumeCodePtrs.capacity() * sizeof(AsyncResumeInfo);
		}
		return n;
	}
	void PublishBytes()
	{
		long long cur = ReservedBytes();
		if (cur != _publishedBytes)
		{
			NeoExecPool_PublishBytes(cur - _publishedBytes);
			_publishedBytes = cur;
		}
	}
};

struct StringInfo : AllocBase, VMString
{
};

// 벡터 성분 저장소. 유효 성분 수는 VarInfo::_vecCount 가 결정한다.
// Quaternion 은 엔진 컨벤션대로 v[0..3] = w,x,y,z 순서.
//
// refcount 로 공유하되 값 의미론을 유지한다 — 대입은 공유(incref)만 하고, 성분을 쓰는
// 순간(v[i] = x) 공유 중이면 복제한다(copy-on-write). 그래서 `var b = a;` 뒤에 b 를
// 고쳐도 a 는 그대로다.
struct VecInfo : AllocBase
{
	float v[4];
};


struct AsyncInfo : AllocBase
{
	// 요청을 낸 워커의 ID. 포인터를 들면 워커가 먼저 소멸했을 때 dangling 이 되고,
	// 같은 주소에 새 워커가 생기면 남의 완료 async 를 자기 것으로 오식별한다.
	// ID 는 살아있는 워커와 절대 충돌하지 않고, FindWorker 로 생존 여부도 확인할 수 있다.
	u32				_ownerWorkerId = 0;
	std::string _request;
	std::string _body;
	std::vector< std::pair<std::string, std::string> > _headers;

	int			_fun_index;
	// 일반 함수와 캡처 lambda를 모두 보관한다. _fun_index는 작업 스레드가
	// 바이트코드를 식별할 때만 쓰고, 호출 시에는 이 값으로 실제 환경을 복원한다.
	VarInfo		_callback;
	int			_timeout;

	ASYNC_STATE _state;
	ASYNC_COMMAND _type;
	bool		_success;
	std::string _resultValue;

	VarInfo		_LockReferance;
	NeoEvent	_event;

	// 파괴 재진입 방지와 순환 후보 intrusive FIFO 링크.
	CycleState<AsyncInfo> _cycleState;
};

// 값 캡처 익명 함수 인스턴스. _captures는 람다 생성 시 부모의 지역 VarInfo를
// 정식 AddRef 복사한 값이며, 호출 종료 시 람다 프레임의 capture 슬롯에서 다시
// 갱신된다. alloc 타입의 자식을 들 수 있어 순환 수집 대상이다.
struct ClosureInfo : AllocBase
{
	CNeoVM*	_pVM = nullptr;
	int			_funIndex = -1;
	std::vector<VarInfo> _captures;
	CycleState<ClosureInfo> _cycleState;
	ClosureInfo*	_livePrev = nullptr;
	ClosureInfo*	_liveNext = nullptr;
};


struct MapInfo;
struct MapNode;
struct ListInfo;
struct SetInfo;
struct SetNode;

int& GetModuleRefCount(VarInfo* p);

NEOS_FORCEINLINE void Move_DestNoRelease(VarInfo* v1, VarInfo* v2)
{
	v1->SetType(v2->GetType());
	// case 순서 = VAR_TYPE 열거 순서. Var_ReleaseInternal 과 같은 이유로 순서가 성능을 좌우한다.
	switch (v2->GetType())
	{
	case VAR_INT: v1->_int = v2->_int; break;
	case VAR_FLOAT: v1->_float = v2->_float; break;
	case VAR_BOOL: v1->_bl = v2->_bl; break;
	case VAR_NONE: break;
	case VAR_FUN: v1->_fun_index = v2->_fun_index; break;
	case VAR_FUN_NATIVE: v1->_funPtr = v2->_funPtr; break;
	case VAR_STRING: v1->_str = v2->_str; ++v1->_str->_refCount; break;
	case VAR_VEC:
		v1->_vec = v2->_vec; v1->_vecCount = v2->_vecCount; ++v1->_vec->_refCount; break;
	case VAR_FP_NATIVE: v1->_fpNative = v2->_fpNative; ++v1->_fpNative->_refCount; break;
	case VAR_MAP: v1->_tbl = v2->_tbl; ++v1->_tbl->_refCount; break;
	case VAR_LIST: v1->_lst = v2->_lst; ++v1->_lst->_refCount; break;
	case VAR_SET: v1->_set = v2->_set; ++v1->_set->_refCount; break;
	case VAR_COROUTINE: v1->_cor = v2->_cor; ++v1->_cor->_refCount; break;
	case VAR_MODULE: v1->_module = v2->_module; ++GetModuleRefCount(v1); break;
	case VAR_ASYNC: v1->_async = v2->_async; ++v1->_async->_refCount; break;
	case VAR_CLOSURE: v1->_closure = v2->_closure; ++v1->_closure->_refCount; break;
	default: break;
	}
}

// 문자열 키 전용 fast path.
// MapInfo::Find의 문자열 의미론을 인라인해 타입 switch / GetHashCode 디스패치를 생략한다.
NEOS_FORCEINLINE VarInfo* MapInfo::FindString(StringInfo* pKeyStr)
{
	if (pKeyStr == nullptr)
		return NULL;
	if (pKeyStr->_interned == false)
	{
		pKeyStr = FindCanonicalString(_pVM, pKeyStr);
		if (pKeyStr == nullptr)
			return NULL;
	}
	if (_BucketCapa <= 0)
		return NULL;
	// 여기 도달했다면 pKeyStr 은 정규 문자열이고, _hash 는 인터닝 시점에 이미 채워졌다.
	// GetHash() 의 "아직 계산 안 됨(0)" 분기는 인터닝되지 않은 문자열용이라 여기서는 죽은 분기다.
	u32 hash = pKeyStr->_hash;
	MapNode* node = _Bucket + (hash & _HashBase);
	while (node->next != MAP_NODE_EMPTY)
	{
		if (node->hash == hash && node->data->key.GetType() == VAR_STRING)
		{
			StringInfo* pCurStr = node->data->key._str;
			if (pCurStr == pKeyStr)
				return &node->data->value;
		}
		if (node->next == MAP_NODE_END)
			break;
		node = _Bucket + node->next;
	}
	return NULL;
}

// 정수 키 전용 fast path. 문자열 키와 대칭 — 없으면 CltReadRare -> GetTableItem ->
// MapInfo::Find -> GetHashCode(타입 switch) -> 일반 탐색으로
// 아웃오브라인 호출 4단을 타게 된다(실측: 문자열 키의 1.8배).
// 해시는 GetHashCode(VarInfo*) 의 VAR_INT 경로와 같아야 한다 = 값 그대로.
NEOS_FORCEINLINE VarInfo* MapInfo::FindInt(int key)
{
	if (_BucketCapa <= 0)
		return NULL;
	MapNode* node = _Bucket + ((u32)key & _HashBase);
	while (node->next != MAP_NODE_EMPTY)
	{
		if (node->data->key.GetType() == VAR_INT && node->data->key._int == key)
			return &node->data->value;
		if (node->next == MAP_NODE_END)
			break;
		node = _Bucket + node->next;
	}
	return NULL;
}


class CNeoVM final
{
//	friend					CNeoVMWorker;
	friend					MapInfo;
	friend					ListInfo;
	friend					SetInfo;
	friend					neo_libs;
	friend					neo_DCalllibs;
private:
	// VM 인스턴스 자체가 실행 상태와 마지막 오류를 소유한다. 별도 기반 클래스를 두지
	// 않아 수명과 소유자가 한 곳에 모인다.
	INeoVMWorker* _pMainWorker = nullptr;
	bool _bError = false;


	// 살아있는 List/Map/Set/Closure 를 intrusive 이중연결 리스트로 추적한다.
	// 기존 std::map<ID,ptr> 레지스트리 대체 — 할당/해제당 트리 연산 2~3회를 O(1) 링크로 교체.
	// String 은 CNVMInstPool(소멸자 지원)이라 별도 추적 불필요 → 레지스트리 제거.
	ListInfo* _sListHead = nullptr;
	MapInfo* _sTableHead = nullptr;
	SetInfo* _sSetHead = nullptr;
	ClosureInfo* _sClosureHead = nullptr;
	// 컨테이너 파괴는 자식 Var_Release 로 다시 컨테이너 파괴를 유발한다. 호출 스택 대신
	// 이 대기열을 깊이 우선으로 비워서, 깊은 트리와 순환 모두 안전하게 처리한다.
	std::vector<VarInfo> _sDestroyQueue;
	bool _bDrainingDestroyQueue = false;
	// CollectCycles는 객체 내부의 임시 color/scratch를 쓴다. 재진입하면 상태가
	// 겹치므로 수집 중에는 같은 VM의 중첩 호출을 무시한다.
	bool _isCollectingCycles = false;
	// VM 종료 중에는 참조 감소가 순환 후보를 새로 만들 필요가 없다. live registry를
	// 강제 순회해 전부 해제하므로, 후보 티켓만 남기지 않도록 이 구간을 분리한다.
	bool _isTearingDown = false;
	// 순환 후보 intrusive FIFO. 타입별 링크라 타입 태그나 별도 heap ticket이 필요 없다.
	// _cycleCandidateCount 는 예산 계산과 empty 판정을 O(1)로 유지한다.
	MapInfo* _sCycleMapHead = nullptr;
	MapInfo* _sCycleMapTail = nullptr;
	ListInfo* _sCycleListHead = nullptr;
	ListInfo* _sCycleListTail = nullptr;
	SetInfo* _sCycleSetHead = nullptr;
	SetInfo* _sCycleSetTail = nullptr;
	CoroutineInfo* _sCycleCoroutineHead = nullptr;
	CoroutineInfo* _sCycleCoroutineTail = nullptr;
	// Module 큐는 현재 항상 비어 있다. QueueContainerForCycleCheck 가 VAR_MODULE 을
	// 초입에서 걸러내기 때문이다 — module 은 worker registry 가 raw 포인터로 수명을
	// 소유하고 FreeWorker 가 유일한 정리 경로라, 수집기가 회수할 대상이 아니다.
	// (전개해봐야 native root 로 판정돼 결과가 바뀌지 않으면서 worker 의 전역 변수
	//  전체를 훑는 비용만 든다.) Pop/Cancel/Clear 의 VAR_MODULE 분기도 같은 이유로
	// 지금은 죽은 경로다. 타입별 링크 구조의 대칭을 깨지 않으려고 남겨둔다 —
	// module 을 다시 후보로 올리려면 Append 한 줄만 되살리면 된다.
	CNeoVMWorker* _sCycleModuleHead = nullptr;
	CNeoVMWorker* _sCycleModuleTail = nullptr;
	AsyncInfo* _sCycleAsyncHead = nullptr;
	AsyncInfo* _sCycleAsyncTail = nullptr;
	ClosureInfo* _sCycleClosureHead = nullptr;
	ClosureInfo* _sCycleClosureTail = nullptr;
	size_t _cycleCandidateCount = 0;
	int _cycleQueueRoundRobin = 0;
	// 후보 루트, graph node, black work, white set을 구간별로 재사용한다.
	// 워밍업 뒤에는 CollectCycles가 그래프용 힙을 새로 할당하지 않는다.
	std::vector<VarInfo> _cycleWorkList;
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
	// 시계도 안 읽고 빠진다. 순환 참조 수집은 호스트가 CollectCycles()로 별도 호출한다.
	long long CollectEmptyPages(bool force = false);
	// 순환 참조 수집. force=false면 max(16, 후보 전체의 2%)개만 처리하고,
	// force=true면 후보가 빌 때까지 모두 처리한다. 반환값은 처리한 후보 수.
	int CollectCycles(bool force = false);
	// 어느 풀이든 완전히 빈 페이지가 있는가. 매 프레임 경로의 조기 반환용.
	bool AnyEmptyPages() const
	{
		return m_sPool_TableData.HasEmptyPages() || m_sPool_TableInfo.HasEmptyPages()
			|| m_sPool_FunctionProperty.HasEmptyPages()
			|| m_sPool_SetData.HasEmptyPages()
			|| m_sPool_SetInfo.HasEmptyPages()
			|| m_sPool_ListInfo.HasEmptyPages()  || m_sPool_Vec.HasEmptyPages()
			|| m_sPool_Async.HasEmptyPages()     || m_sPool_String.HasEmptyPages()
			|| m_sPool_Closure.HasEmptyPages();
	}
	void SetEmptyPageHoldSeconds(float sec) { m_iEmptyPageHoldMs = (sec <= 0.0f) ? 0 : (int)(sec * 1000.0f); }
	float GetEmptyPageHoldSeconds() const { return m_iEmptyPageHoldMs / 1000.0f; }
	// 한 번의 CollectEmptyPages(false) 가 해제할 페이지 수 상한. 0 이하 = 상한 없음.
	void SetTrimPagesPerCall(int pages) { m_iTrimPagesPerCall = pages; }
	int GetTrimPagesPerCall() const { return m_iTrimPagesPerCall; }

	long long PoolBytes() const;   // 이 VM 의 오브젝트 풀이 확보한 총 바이트
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
	void QueueContainerForCycleCheck(const VarInfo& value);
	void CancelCycleCandidate(VAR_TYPE type, void* object);
	bool PopCycleCandidate(VAR_TYPE& type, void*& object);
	void ClearCycleCandidates();
	template<typename Visitor>
	void VisitCycleContainerChildren(VarInfo source, Visitor visitor);
	int CollectUnreachableCycleCandidates(size_t rootBudget);

	AsyncInfo* AsyncAlloc();
	void FreeAsync(VarInfo* d);
	ClosureInfo* ClosureAlloc(int functionIndex, int captureCount);
	void FreeClosure(ClosureInfo* closure);

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

	NEOS_FORCEINLINE bool Var_ReleaseVecFast(VarInfo* d)
	{
		VecInfo* vec = d->_vec;
		if (vec->_refCount <= 1)
			return false;

		--vec->_refCount;
		d->_vec = nullptr;
		d->ClearType();
		return true;
	}

	NEOS_FORCEINLINE bool Var_ReleaseStringFast(VarInfo* d)
	{
		StringInfo* str = d->_str;
		if (str->_refCount <= 1)
			return false;

		--str->_refCount;
		d->_str = nullptr;
		d->ClearType();
		return true;
	}

	NEOS_FORCEINLINE bool Var_ReleaseListFast(VarInfo* d)
	{
		ListInfo* list = d->_lst;
		if (list->_refCount <= 1 || list->_cycleState._mayContainContainerChild)
			return false;

		--list->_refCount;
		d->_lst = nullptr;
		d->ClearType();
		return true;
	}



	NEOS_FORCEINLINE void Move(VarInfo* v1, VarInfo* v2)
	{
		if (v1->IsAllocType())
		{
			if (v1 == v2)
				return;
			Var_Release(v1);
		}

		if (v2->IsAllocType() == false)
			*v1 = *v2;
		else
			Move_DestNoRelease(v1, v2);
	}


	NEOS_FORCEINLINE void Var_Release(VarInfo *d)
	{
		if (d->IsAllocType() == false)
		{
			d->ClearType();
			return;
		}

		const VAR_TYPE type = d->GetType();
		if (type == VAR_VEC && Var_ReleaseVecFast(d))
			return;
		if (type == VAR_STRING && Var_ReleaseStringFast(d))
			return;
		if (type == VAR_LIST && Var_ReleaseListFast(d))
			return;

		Var_ReleaseInternal(d);
	}

	VarInfo m_sDefaultValue[NDF_MAX];

	// 페이지 크기는 고정이다(배증 없음 — 마지막 한 장이 필요량을 크게 넘겨 잡던 문제).
	// 아래 개수는 노드 크기(헤더 포함)를 기준으로 한 장이 대략 3~13KB 가 되게 잡은 값이다.
	// 개수가 적은 타입은 더 작게 — 안 쓰는 타입이 페이지 한 장을 통째로 물지 않게 한다.
	// (구조체 크기는 필드가 늘 때마다 바뀌므로 여기 적지 않는다. 필요하면 sizeof 로 확인할 것)
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
	CNVMInstPool< ClosureInfo, 64> m_sPool_Closure;

	// 빈 페이지 보유 시간(기본 5초).
	int m_iEmptyPageHoldMs = 5000;
	// 매 프레임 호출을 전제로 한 회수 예산. 4장/호출 = 60fps 에서 초당 240장이니
	// 9MB 규모(약 700장)도 3초 남짓에 다 돌아가면서 프레임당 비용은 상한선 안이다.
	int m_iTrimPagesPerCall = 4;
	// 라운드로빈 시작 풀. 앞쪽 풀이 예산을 독식해 뒤쪽이 굶는 것을 막는다.
	int m_iTrimPoolCursor = 0;
	static const int kTrimPoolCount = 10;
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
	int GetBytesSize() const;
	bool IsLocalErrorMsg() const { return _bError; }

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
public:
	CNeoVM();
	~CNeoVM();

	bool ReleaseWorker(INeoVMWorker* worker);


	const char* GetLastErrorMsg() { return _sErrorMsgDetail.c_str();  }
	bool IsLastErrorMsg() { return (_sErrorMsgDetail.empty() == false); }
	// _pErrorMsg 까지 비운다: 이게 남아 있으면 SetError 가 계속 무시되어 첫 에러가 고정된다.
	void ClearLastErrorMsg() { _bError = false; _sErrorMsgDetail.clear(); _pErrorMsg.clear(); }
	void SetLastErrorMsg(const char* msg) { SetError(msg != nullptr ? std::string(msg) : std::string()); }
	void Var_ReleaseInternal(VarInfo* d);

	INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker = true, bool init = false, int iStackSize = 50 * 1024); // 0 is error
	INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, bool blMainWorker = true, bool init = false, int iStackSize = 50 * 1024);
};

};
