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

// CNeoVMImpl is intentionally incomplete in this header.  The non-interned
// string slow path uses this small bridge so MapInfo::FindString can remain
// inline for the usual (already canonical) key path.
StringInfo* FindCanonicalString(CNeoVMImpl* pVM, StringInfo* pString);

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

enum eNOperationSub : u8
{
	eOP_ADD,
	eOP_SUB,
	eOP_MUL,
	eOP_DIV,
	eOP_PER,
	eOP_LSH,
	eOP_RSH,
	eOP_AND,
	eOP__OR,
	eOP_XOR,
};

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
	int		_iReturnOffset;
	VarInfo* _pReturnValue;
	// async.wait가 callback 실행 전에 기록한 native 반환값을 callback return 직전에 복원한다.
	VarInfo* _pAsyncWaitReturnValue = nullptr;
	bool	 _asyncWaitReturnValue = false;
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
	COROUTINE_STATE _state;
	COROUTINE_SUB_STATE _sub_state;

	CoroutineBase _info;
	// async.wait가 Idle dispatcher를 거쳐 돌아갈 원래 IP. 실행 컨텍스트마다
	// 보관해 중첩 async 대기가 공유 바이트코드를 패치하지 않도록 한다.
	std::vector<AsyncResumeInfo>	m_sAsyncResumeCodePtrs;

	std::vector<VarInfo>	m_sVarStack;
	SimpleVector<SCallStack>	m_sCallStack;
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

	NeoExecContextPool(int varStackSize = 50 * 1024) : _varStackSize(varStackSize < 100 ? 100 : varStackSize) {}
	~NeoExecContextPool()
	{
		NeoExecPool_PublishBytes(-_publishedBytes);
	}

	NeoExecContext* Acquire()
	{
		NeoExecContext* p = _pool.Receive();
		const bool cold = p->m_sVarStack.capacity() == 0;   // 처음 대여되는 노드 = 스택 힙이 지금 잡힌다
		p->_info._pCodeCurrent = NULL;
		p->_info.ClearSP();
		p->m_sAsyncResumeCodePtrs.clear();
		p->_state = COROUTINE_STATE_RUNNING;
		p->_sub_state = COROUTINE_SUB_NORMAL;
		p->m_sCallStack.reserve(1000);
		p->m_sCallStack.clear();
		p->m_sVarStack.resize(_varStackSize);
		if (cold)
		{
			// 재계산은 컨텍스트가 새로 늘어날 때만 한다(대여 fast path 에 부담을 주지 않으려고).
			// 실행 중 콜스택이 reserve 를 넘겨 자라는 만큼은 다음 신규 컨텍스트 때 반영된다.
			_warmCtx.push_back(p);
			PublishBytes();
		}
		return p;
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
	int			_timeout;

	ASYNC_STATE _state;
	ASYNC_COMMAND _type;
	bool		_success;
	std::string _resultValue;

	VarInfo		_LockReferance;
	NeoEvent	_event;
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
	switch (v2->GetType())
	{
	case VAR_INT: v1->_int = v2->_int; break;
	case VAR_FLOAT: v1->_float = v2->_float; break;
	case VAR_BOOL: v1->_bl = v2->_bl; break;
	case VAR_NONE: break;
	case VAR_FUN: v1->_fun_index = v2->_fun_index; break;
	case VAR_FUN_NATIVE: v1->_funPtr = v2->_funPtr; break;
	case VAR_CHAR: v1->_c = v2->_c; break;

	// 벡터: 여기서는 공유(incref)만 한다. 값 의미론은 성분을 쓰는 쪽에서 copy-on-write 로
	// 지킨다(VecCopyOnWrite) — 대입마다 복제하면 할당 비용이 그대로 살아나기 때문이다.
	case VAR_VEC:
		// 성분 수는 저장소가 아니라 VarInfo 에 있으므로 포인터와 함께 반드시 옮긴다.
		v1->_vec = v2->_vec; v1->_vecCount = v2->_vecCount; ++v1->_vec->_refCount; break;

	case VAR_STRING: v1->_str = v2->_str; ++v1->_str->_refCount; break;
	case VAR_MAP: v1->_tbl = v2->_tbl; ++v1->_tbl->_refCount; break;
	case VAR_LIST: v1->_lst = v2->_lst; ++v1->_lst->_refCount; break;
	case VAR_SET: v1->_set = v2->_set; ++v1->_set->_refCount; break;
	case VAR_COROUTINE: v1->_cor = v2->_cor; ++v1->_cor->_refCount; break;
	case VAR_MODULE: v1->_module = v2->_module; ++GetModuleRefCount(v1); break;
	case VAR_ASYNC: v1->_async = v2->_async; ++v1->_async->_refCount; break;
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

};
