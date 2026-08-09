#pragma once

#include "NeoConfig.h"
#include "NeoVMHash.h"

namespace NeoScript
{

class CNArchive;

struct INeoVMWorker;
struct INeoVM;
class CNeoVMImpl;
class CNeoVMWorker;
class CNeoVMProgram;
struct FunctionPtr;
struct VarInfo;
struct NeoExecContextPool;
struct SFunctionLayer;
struct SFunctions;
NEOS_FORCEINLINE void Move_DestNoRelease(VarInfo* v1, VarInfo* v2);

// 실행 컨텍스트 풀 팩토리. 내부 동기화가 없으므로 엔진이 스레드별(thread_local)로 하나 만들어 주입한다.
// varStackSize: 각 컨텍스트의 var 스택 엔트리 수.
NeoExecContextPool* NeoExecContextPool_Create(int varStackSize = 50 * 1024);
void                NeoExecContextPool_Destroy(NeoExecContextPool* pool);

// 최상위 실행의 결과 상태.
enum NeoExecStatus
{
	NEOEXEC_COMPLETED = 0,   // 실행이 끝까지 완료됨 → 컨텍스트 반납됨
	NEOEXEC_SUSPENDED = 1,   // sleep/yield/브레이크로 정지 → 컨텍스트 retain(반납 안 됨), Resume 필요
	NEOEXEC_ERROR     = 2,   // 에러 → 컨텍스트 반납됨
};

// 등록된 빌트인 함수 1개의 정보 (자동완성/코드어시스트용 리플렉션).
// 원천은 AddSystemFun / g_sNeoFunLib_* 등록 그 자체이므로 손 유지가 필요없다.
struct NeoBuiltinInfo
{
	std::string module;    // "math","system","coroutine" (namespaced) 또는 "string","list","map" (타입 메서드)
	std::string name;      // 함수/메서드 이름
	int         argCount = -1;  // 인자 수. -1 = 가변("..." 포함)/미상(타입 메서드)
	std::string ret;       // 리턴 타입("void","float","list" …). 비어있으면 미상(타입 메서드)
	std::vector<std::string> params;  // "float x" 형태의 타입+이름 목록(등록 나열 그대로). 마지막이 "..." 이면 가변.
};

// 워커의 현재 실행 상태. 별도 플래그를 저장하지 않고 워커의 실제 컨텍스트/정지 상태에서 계산한다.
enum class NeoExecutionState : u8
{
	Idle,
	Running,
	SuspendedSleep,
	SuspendedDebugger,
};

// 호스트가 Script 함수를 시작하려 할 때의 컨텍스트 상태.
// Nested는 실행 중인 Script의 native callback에서만 현재 컨텍스트를 재사용한다.
enum class NeoHostCallBegin : u8
{
	Acquired,
	Nested,
	Suspended,
	NoPool,
	InvalidState,
};

enum NeoCompileDefineTokenType
{
	NEO_DEFINE_TOKEN_IDENTIFIER,
	NEO_DEFINE_TOKEN_INT,
	NEO_DEFINE_TOKEN_FLOAT,
	NEO_DEFINE_TOKEN_STRING,
	NEO_DEFINE_TOKEN_TRUE,
	NEO_DEFINE_TOKEN_FALSE,
	NEO_DEFINE_TOKEN_NULL,
};

struct NeoCompileDefineToken
{
	NeoCompileDefineTokenType type = NEO_DEFINE_TOKEN_IDENTIFIER;
	std::string text;
};

struct NeoCompileDefines
{
	std::unordered_map<std::string, NeoCompileDefineToken> values;

	void clear() { values.clear(); }
	bool empty() const { return values.empty(); }
};

struct INeoLoader
{
	virtual bool Load(const char* pFileName, void*& pBuffer, int& iLen) = 0;
	virtual void Unload(const char* pFileName, void* pBuffer, int iLen) = 0;
	virtual const char* GetLibPath() = 0;
};


typedef int(*Neo_CFunction) (INeoVMWorker* N, FunctionPtr* pFun, short args);
typedef bool(*Neo_NativeFunction) (INeoVMWorker* N, void* pUserData, const VMString* pStr, short args);
typedef bool(*Neo_NativeProperty) (INeoVMWorker* N, void* pUserData, const VMString* pStr, VarInfo* p, bool get);

#define NEO_DEFAULT_CHECKOP		(500)

struct FunctionPtr
{
	u8							_argCount;
	Neo_CFunction				_fn;
	void* _func;
};

#pragma pack(1)
struct FunctionPtrNative
{
	Neo_NativeFunction			_func;
	Neo_NativeProperty			_property;
};
struct NeoFunction
{
	// Script Function
	INeoVMWorker* _pWorker;
	int			_fun_index;

	// C Function
	FunctionPtr _fun;
};
enum VAR_TYPE : u8
{
	VAR_INT,
	VAR_FLOAT,	// float or double 
	VAR_BOOL,
	VAR_NONE,
	VAR_FUN,

	VAR_ITERATOR,
	VAR_FUN_NATIVE,

	VAR_CHAR,

	VAR_STRING,	// Alloc
	VAR_MAP,
	VAR_LIST,
	VAR_SET,
	VAR_COROUTINE,
	VAR_MODULE,
	VAR_ASYNC,

	// 벡터 값타입 (alloc: VecInfo 를 풀에서 받아 refcount 로 공유).
	// IsAllocType 경계(VAR_STRING) 뒤에 둔다 — 성분 4개를 VarInfo 유니온에 인라인하면
	// VarInfo 가 16→24바이트가 되어 스택/리스트/맵 엔트리(MapData) 전체가 50% 커진다(실측: map_str
	// -12%, particles -13%). 값 의미론은 copy-on-write 로 보존한다(VecCopyOnWrite).
	//
	// 성분 수(1~4)는 타입이 아니라 VarInfo::_vecCount 가 들고 있다. 쿼터니언도 별도 타입이
	// 아니다 — 저장은 4성분이고 wxyz 해석은 사용자 규약이라 VM 이 구분할 이유가 없다.
	VAR_VEC,

	// 네이티브 Function/Property 객체. map 저장소를 갖지 않는 독립 alloc 타입이다.
	VAR_FP_NATIVE,
};

struct SNeoVMAllocStats
{
	int strings = 0;
	int maps = 0;
	int lists = 0;
	int sets = 0;
	int coroutines = 0;
	int modules = 0;
	int asyncs = 0;
	int vectors = 0;
	// VM 메모리풀이 OS 에서 확보해 들고 있는 총 바이트(사용중 + 여유). 풀은 반납해도 페이지를
	// 돌려주지 않으므로 이 값이 곧 실제 점유량이다. 전역 조회면 모든 VM 의 오브젝트 풀 +
	// 스레드별 실행 컨텍스트 풀(var 스택 포함) 합계, VM 단위 조회면 그 VM 의 오브젝트 풀만.
	// 컬렉션이 따로 잡는 힙(ListInfo/MapInfo 의 _Bucket, StringInfo 의 문자열 본문)은 풀 밖이라 제외.
	long long poolBytes = 0;
	// 반납되어 놀고 있는 문자열 노드가 아직 붙들고 있는 문자 버퍼 합계.
	// poolBytes 에는 안 잡히는 값이다(풀 밖 힙). 이게 계속 크면 유지 임계값을 낮춰야 한다.
	long long stringIdleBytes = 0;
};

struct INeoVM;
void GetNeoVMAllocStats(SNeoVMAllocStats& outStats);
bool GetNeoVMAllocStats(INeoVM* pVM, SNeoVMAllocStats& outStats);

struct CoroutineInfo;
struct StringInfo;
struct MapInfo;
struct ListInfo;
struct SetInfo;
struct AsyncInfo;
struct MapNode;
struct SetNode;
struct VecInfo;

// 네이티브 객체의 Function/Property와 사용자 데이터. MapInfo와 의도적으로
// 분리되어 일반 map/set 탐색과 네이티브 객체가 서로의 메모리 비용을 만들지 않는다.
struct FunctionPropertyInfo
{
	int				_refCount;
	FunctionPtrNative	_fun;
	void*				_pUserData;
};

#pragma pack(1)
struct CollectionIterator
{
	union
	{
		MapNode* _pTableNode;
		SetNode* _pSetNode;
		int			_iListOffset;
		int			_iStringOffset;
	};
};
#pragma pack()

struct VarInfo
{
private:
	VAR_TYPE	_type;
	// VAR_VEC 의 성분 수(1~4). _type 뒤 패딩에 들어가므로 크기를 늘리지 않는다
	// (u8 이라 x64 의 7바이트, Win32 의 3바이트 패딩 양쪽에 모두 맞는다).
	// 설정은 VecStoreFor() 한 곳뿐이고, 복사는 Move_DestNoRelease 가 _vec 과 함께 옮긴다.
	u8			_vecCount;
	NEOS_FORCEINLINE void SetType(VAR_TYPE t) { _type = t; }
	NEOS_FORCEINLINE void SetVecType(int count) { _type = VAR_VEC; _vecCount = (u8)count; }
	NEOS_FORCEINLINE void ClearType()
	{
		_type = VAR_NONE;
	}

	friend struct INeoVMWorker;
	friend struct INeoVM;
	friend class CNeoVMImpl;
	friend class CNeoVMWorker;
	friend struct MapInfo;
	friend struct ListInfo;
	friend struct SetInfo;
	friend struct SFunctionLayer;
	friend struct SFunctions;
	friend void Move_DestNoRelease(VarInfo* v1, VarInfo* v2);
public:
	union
	{
		bool		_bl;
		CoroutineInfo* _cor;
		StringInfo* _str;
		SUtf8One	_c;
		MapInfo* _tbl;
		ListInfo* _lst;
		SetInfo* _set;
		FunctionPtr* _funPtr; // C Native
		FunctionPropertyInfo* _fpNative; // VAR_FP_NATIVE
		int			_int;
		NS_FLOAT	_float;
		int			_fun_index;
		INeoVMWorker* _module;
		AsyncInfo*	_async;
		CollectionIterator	_it;
		VecInfo*	_vec; // VAR_VEC 성분(유효 성분 수는 _vecCount)
	};

	NEOS_FORCEINLINE VarInfo() { _type = VAR_NONE; }
	NEOS_FORCEINLINE VarInfo(VAR_TYPE t) { _type = t; }
	NEOS_FORCEINLINE VarInfo(int v) { _type = VAR_INT; _int = v; }

	NEOS_FORCEINLINE VAR_TYPE GetType() { return _type; }
	NEOS_FORCEINLINE bool IsAllocType()
	{
		return ((_type >= VAR_STRING));
	}
	NEOS_FORCEINLINE bool IsTrue()
	{
		if (VAR_BOOL == _type)
			return _bl;
		return false;
	}
	NEOS_FORCEINLINE bool IsNumber()
	{
		return (VAR_INT == _type || VAR_FLOAT == _type);
	}
	NEOS_FORCEINLINE bool IsVector()
	{
		return (_type == VAR_VEC);
	}
	// VAR_VEC 일 때만 유효. 다른 타입에서는 값이 의미 없으므로 IsVector() 로 먼저 거른다.
	NEOS_FORCEINLINE int VectorComponentCount()
	{
		return (_type == VAR_VEC) ? _vecCount : 0;
	}
	NEOS_FORCEINLINE NS_FLOAT GetFloatNumber()
	{
		if (VAR_INT == _type) return (NS_FLOAT)_int;
		if (VAR_FLOAT == _type) return (NS_FLOAT)_float;
		return 0;
	}

	bool MapInsertFloat(const std::string& pKey, NS_FLOAT value);
	bool MapFindFloat(const std::string& pKey, NS_FLOAT& value);

	bool ListInsertFloat(int idx, NS_FLOAT value);
	bool ListFindFloat(int idx, NS_FLOAT& value);
	bool SetListIndexer(VMHash<int>* pIndexer);

	// 벡터 Get (호스트/엔진용). VAR_VEC* 전용 — 리스트 폴백 없음.
	// Quaternion 은 엔진 컨벤션대로 wxyz 순서, Vector4 는 xyzw.
	// Set 은 여기 없다: 벡터가 alloc 타입이라 저장소를 만들려면 VM(풀)이 필요하다.
	// 세팅은 INeoVMWorker::Var_SetVec2/3/4/Quat 를 쓴다.
	// 성공/실패 모두 해당 성분 수만 쓴다. 남는 레인은 호출자 몫이다.
	bool GetVec2(float out[2]);
	bool GetVec3(float out[3]);
	bool GetVec4(float out[4]); // xyzw
	bool GetQuat(float out[4]); // wxyz
};

// 유니온 최대 멤버가 포인터(8바이트)라 x64는 16바이트, Win32는 12바이트다.
// 벡터를 인라인(float[4])하던 시절엔 24/20바이트였다 — 이 8바이트가 스택·리스트·맵노드
// 전부에 곱해져서 실측으로 map_str 12%, particles 13% 를 먹었다.
static_assert(sizeof(VarInfo) == (sizeof(void*) == 8 ? 16 : 8), "Unexpected VarInfo size");

enum NeoDebugStopReason
{
	NEO_DEBUG_STOP_NONE = 0,
	NEO_DEBUG_STOP_BREAKPOINT,
	NEO_DEBUG_STOP_STEP,
	NEO_DEBUG_STOP_PAUSE,
	NEO_DEBUG_STOP_EXCEPTION,
};

struct NeoDebugLocation
{
	int opIndex = -1;
	int file = -1;
	int line = -1;
	int callDepth = 0;
};

struct NeoDebugBreakpoint
{
	int file = -1;
	int line = -1;
};

struct NeoDebugStackFrame
{
	int frameId = 0;
	int functionId = -1;
	std::string functionName;
	int file = -1;
	int line = -1;
	int opIndex = -1;
	int stackBase = 0;
	int argsCount = 0;
	int localCount = 0;
	int tempCount = 0;
};

struct NeoDebugVariable
{
	std::string name;
	std::string type;
	std::string value;
	int stackIndex = -1;
	std::vector<NeoDebugVariable> children;
};

struct INeoVMDebugListener
{
	virtual void OnNeoDebugStopped(INeoVMWorker* worker, const NeoDebugLocation& location, NeoDebugStopReason reason) = 0;
};

struct INeoVMWorker
{
protected:
	std::vector<VarInfo>* _args = NULL;
	INeoVM* _pVM;

	u32						_idWorker;
	int	_BytesSize = 0;
public:
	inline void* GetVMPointer() { return _pVM; }
	inline u32 GetWorkerID() { return _idWorker; }
	inline int GetBytesSize() { return _BytesSize; }

	virtual bool RunFunctionResume(int iFID, std::vector<VarInfo>& _args) = 0;
	virtual bool RunFunction(int iFID, std::vector<VarInfo>& _args) =0;
	virtual bool RunFunction(const std::string& funName, std::vector<VarInfo>& _args) =0;
	virtual void GC() =0;
	virtual VarInfo* GetReturnVar() =0;
	virtual VarInfo* GetStackVar(int idx) =0;
	virtual bool ResetVarType(VarInfo* p, VAR_TYPE type, int capa = 0) =0;
	virtual void DebugSetListener(INeoVMDebugListener* listener) = 0;
	virtual void DebugSetBreakpoints(const std::vector<int>& lines) = 0;
	virtual void DebugSetBreakpoints(const std::vector<NeoDebugBreakpoint>& breakpoints) = 0;
	virtual void DebugContinue() = 0;
	virtual void DebugStepInto() = 0;
	virtual void DebugStepOver() = 0;
	virtual void DebugStepOut() = 0;
	virtual void DebugPause() = 0;
	virtual bool DebugIsPaused() = 0;
	virtual NeoDebugLocation DebugGetLocation() = 0;
	virtual void DebugGetStackTrace(std::vector<NeoDebugStackFrame>& frames) = 0;
	virtual void DebugGetFrameVariables(int frameId, std::vector<NeoDebugVariable>& vars) = 0;
	virtual void DebugGetExecutableLines(std::vector<int>& lines) = 0;
	virtual void DebugGetExecutableLocations(std::vector<NeoDebugLocation>& locations) = 0;

	static void neo_pushcclosureNative(FunctionPtrNative* pOut, Neo_NativeFunction pFun)
	{
		pOut->_func = pFun;
	}
	static void neo_pushcclosureNative(FunctionPtrNative* pOut, Neo_NativeProperty property)
	{
		pOut->_property = property;
	}
	static void neo_pushcclosure(FunctionPtr* pOut, Neo_CFunction fn, void* pFun)
	{
		pOut->_fn = fn;
		pOut->_func = pFun;
	}
	void Var_Release(VarInfo* d);
	void Var_SetNone(VarInfo* d);

	virtual void Var_Move(VarInfo* v1, VarInfo* v2) =0;


	void PushInt(int v)
	{
		VarInfo d;
		d.SetType(VAR_INT);
		d._int = v;
		_args->push_back(d);
	}
	void PushFloat(NS_FLOAT v)
	{
		VarInfo d;
		d.SetType(VAR_FLOAT);
		d._float = v;
		_args->push_back(d);
	}
	void PushString(const char* p);
	void PushBool(bool b)
	{
		VarInfo d;
		d.SetType(VAR_BOOL);
		d._bl = b;
		_args->push_back(d);
	}
	void PushNeoFunction(NeoFunction v);

	int PopInt(VarInfo* V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return V->_int;
		case VAR_FLOAT:
			return (int)V->_float;
		default:
			break;
		}
		return -1;
	}
	NS_FLOAT PopFloat(VarInfo* V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return (NS_FLOAT)V->_int;
		case VAR_FLOAT:
			return V->_float;
		default:
			break;
		}
		return -1;
	}
	const char* PopString(VarInfo* V);
	const std::string* PopStlString(VarInfo* V);
	bool PopBool(VarInfo* V)
	{
		if (V->GetType() == VAR_BOOL)
			return V->_bl;

		return false;
	}
	NeoFunction PopNeoFunction(VarInfo* V)
	{
		NeoFunction r;
		if (V->GetType() == VAR_FUN)
		{
			r._pWorker = this;
			r._fun_index = V->_fun_index;
		}
		else
		{
			r._pWorker = NULL;
			r._fun_index = -1;
		}
		return r;
	}

	bool GetArg_StlString(int idx, std::string &r);
	bool GetArg_Int(int idx, int& r);
	bool GetArg_Float(int idx, NS_FLOAT& r);
	bool GetArg_Bool(int idx, bool &r);

	inline void push(char ret) { PushInt(ret); }
	inline void push(unsigned char ret) { PushInt(ret); }
	inline void push(short ret) { PushInt(ret); }
	inline void push(unsigned short ret) { PushInt(ret); }
	inline void push(long ret) { PushInt(ret); }
	inline void push(unsigned long ret) { PushInt(ret); }
	inline void push(int ret) { PushInt(ret); }
	inline void push(unsigned int ret) { PushInt(ret); }
	inline void push(NS_FLOAT ret) { PushFloat(ret); }
	inline void push(char* ret) { PushString(ret); }
	inline void push(const char* ret) { PushString(ret); }
	inline void push(bool ret) { PushBool(ret); }
	inline void push(long long ret) { PushInt((int)ret); }
	inline void push(unsigned long long ret) { PushInt((int)ret); }
	inline void push(NeoFunction ret) { PushNeoFunction(ret); }

	inline void		_read(VarInfo* V, std::string*& r) { r = (std::string*)PopStlString(V); }
	inline void		_read(VarInfo* V, char*& r) { r = (char*)PopString(V); }
	inline void		_read(VarInfo* V, const char*& r) { r = PopString(V); }
	inline void		_read(VarInfo* V, char& r) { r = (char)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned char& r) { r = (unsigned char)PopInt(V); }
	inline void		_read(VarInfo* V, short& r) { r = (short)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned short& r) { r = (unsigned short)PopInt(V); }
	inline void		_read(VarInfo* V, long& r) { r = (long)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned long& r) { r = (unsigned long)PopInt(V); }
	inline void		_read(VarInfo* V, int& r) { r = (int)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned int& r) { r = (unsigned int)PopInt(V); }
	inline void		_read(VarInfo* V, NS_FLOAT& r) { r = (NS_FLOAT)PopFloat(V); }
	inline void		_read(VarInfo* V, bool& r) { r = PopBool(V); }
	inline void		_read(VarInfo* V) {}
	inline void		_read(VarInfo* V, long long& r) { r = (long long)PopInt(V); }
	inline void		_read(VarInfo* V, unsigned long long& r) { r = (unsigned long long)PopInt(V); }
	inline void		_read(VarInfo* V, VarInfo*& r) { r = V; }
	inline void		_read(VarInfo* V, NeoFunction& r) { r = PopNeoFunction(V); }

	void Var_SetInt(VarInfo* d, int v);
	void Var_SetFloat(VarInfo* d, NS_FLOAT v);
	void Var_SetBool(VarInfo* d, bool v);
	// 벡터 저장소 확보. 대상이 이미 단독 소유 벡터면 재사용하고, 아니면 풀에서 새로 받는다.
	VecInfo*	VecStoreFor(VarInfo* d, int count);
	// 각 setter는 실제 성분 수만 받거나 저장한다. VecInfo의 남는 레인은 건드리지 않는다.
	void Var_SetVec2(VarInfo* d, float x, float y);
	void Var_SetVec3(VarInfo* d, float x, float y, float z);
	void Var_SetVec4(VarInfo* d, float x, float y, float z, float w);
	void Var_SetQuat(VarInfo* d, float w, float x, float y, float z);
	void Var_SetCoroutine(VarInfo* d, CoroutineInfo* p);
	void Var_SetString(VarInfo* d, const char* str);
	void Var_SetString(VarInfo* d, SUtf8One c);
	void Var_SetStringA(VarInfo* d, const std::string& str);
	void Var_SetTable(VarInfo* d, MapInfo* p);
	void Var_SetList(VarInfo* d, ListInfo* p);
	void Var_SetSet(VarInfo* d, SetInfo* p);
	void Var_SetFun(VarInfo* d, int fun_index);
	void Var_SetModule(VarInfo* d, INeoVMWorker* p);
	void Var_SetAsync(VarInfo* d, AsyncInfo* p);

	inline void	ReturnValue() { Var_Release(GetReturnVar()); }
	inline void	ReturnValue(VarInfo* p) { Var_Move(GetReturnVar(), p); }
	inline void	ReturnValue(char* p) { Var_SetString(GetReturnVar(), p); }
	inline void	ReturnValue(const char* p) { Var_SetString(GetReturnVar(), p); }
	inline void	ReturnValue(char p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned char p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(short p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned short p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(long p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned long p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(int p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(unsigned int p) { Var_SetInt(GetReturnVar(), p); }
	inline void	ReturnValue(NS_FLOAT p) { Var_SetFloat(GetReturnVar(), p); }
	inline void	ReturnValue(bool p) { Var_SetBool(GetReturnVar(), p); }
	inline void	ReturnValue(long long p) { Var_SetInt(GetReturnVar(), (int)p); }
	inline void	ReturnValue(unsigned long long p) { Var_SetInt(GetReturnVar(), (int)p); }
	inline void	ReturnValue(CoroutineInfo* p) { Var_SetCoroutine(GetReturnVar(), p); }
	inline void	ReturnValue(AsyncInfo* p) { Var_SetAsync(GetReturnVar(), p); }

	inline void PushArgs() { }
	template<typename  T, typename ... Types>
	inline void PushArgs(T arg1, Types ... args)
	{
		push(arg1);
		PushArgs(args...);
	}
	inline void ReleaseArgs(std::vector<VarInfo>& args)
	{
		for (VarInfo& arg : args)
			Var_Release(&arg);
	}

	class ScopedNestedScriptCall
	{
		INeoVMWorker* m_pWorker = nullptr;
	public:
		ScopedNestedScriptCall(INeoVMWorker* worker, bool active)
		{
			if (active)
			{
				m_pWorker = worker;
				m_pWorker->BeginNestedScriptCall();
			}
		}
		~ScopedNestedScriptCall()
		{
			if (m_pWorker != nullptr)
				m_pWorker->EndNestedScriptCall();
		}
	};

	template<typename RVal, typename ... Types>
	bool iCall(RVal& r, int iFID, Types ... args)
	{
		NeoHostCallBegin begin = BeginHostCall();
		if (begin != NeoHostCallBegin::Acquired && begin != NeoHostCallBegin::Nested)
			return false;
		ScopedNestedScriptCall nestedScriptCall(this, begin == NeoHostCallBegin::Nested);
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		if (RunFunction(iFID, args_) == false)
		{
			ReleaseArgs(args_);
			EndHostCall(begin);
			return false;
		}
		GC();
		ReleaseArgs(args_);
		_read(GetReturnVar(), r);
		EndHostCall(begin);
		return true;
	}

	template<typename ... Types>
	bool iCallN(int iFID, Types ... args)
	{
		NeoHostCallBegin begin = BeginHostCall();
		if (begin != NeoHostCallBegin::Acquired && begin != NeoHostCallBegin::Nested)
			return false;
		ScopedNestedScriptCall nestedScriptCall(this, begin == NeoHostCallBegin::Nested);
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		if (RunFunction(iFID, args_) == false)
		{
			ReleaseArgs(args_);
			EndHostCall(begin);
			return false;
		}
		GC();
		ReleaseArgs(args_);
		ReturnValue();
		EndHostCall(begin);
		return true;
	}

	template<typename RVal, typename ... Types>
	bool Call(RVal& r, const std::string& funName, Types ... args)
	{
		NeoHostCallBegin begin = BeginHostCall();
		if (begin != NeoHostCallBegin::Acquired && begin != NeoHostCallBegin::Nested)
			return false;
		ScopedNestedScriptCall nestedScriptCall(this, begin == NeoHostCallBegin::Nested);
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		if (RunFunction(funName, args_) == false)
		{
			ReleaseArgs(args_);
			EndHostCall(begin);
			return false;
		}
		GC();
		ReleaseArgs(args_);
		_read(GetReturnVar(), r);
		EndHostCall(begin);
		return true;
	}

	template<typename ... Types>
	bool CallN(const std::string& funName, Types ... args)
	{
		NeoHostCallBegin begin = BeginHostCall();
		if (begin != NeoHostCallBegin::Acquired && begin != NeoHostCallBegin::Nested)
			return false;
		ScopedNestedScriptCall nestedScriptCall(this, begin == NeoHostCallBegin::Nested);
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		if (RunFunction(funName, args_) == false)
		{
			ReleaseArgs(args_);
			EndHostCall(begin);
			return false;
		}
		GC();
		ReleaseArgs(args_);
		ReturnValue();
		EndHostCall(begin);
		return true;
	}

	template<typename ... Types>
	bool Setup_TL(int iFID, Types ... args)
	{
		if (IsSuspended())
			return false;

		NeoHostCallBegin begin = BeginHostCall();
		if (begin != NeoHostCallBegin::Acquired)
			return false;

		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		if (false == Setup(iFID, args_))
		{
			ReleaseArgs(args_);
			EndHostCall(begin);
			return false;
		}
		ReleaseArgs(args_);
		return true;
	}

	// 최상위 실행(엔진 이벤트 진입). 풀에서 컨텍스트를 대여해 iFID 를 실행한다.
	// 반환: NeoExecStatus. SUSPENDED 면 컨텍스트를 retain 하고, 다음엔 ResumeTop() 으로 이어가야 한다.
	template<typename ... Types>
	int ExecuteN(int iFID, Types ... args)
	{
		std::vector<VarInfo> args_;
		_args = &args_;
		PushArgs(args...);
		_args = NULL;

		int st = ExecuteTop(iFID, args_);
		ReleaseArgs(args_);
		return st;
	}

	virtual int FindFunction(const std::string& name) = 0;
	virtual bool	Setup(int iFunctionID, std::vector<VarInfo>& _args) = 0;
	virtual bool	Start(int iFunctionID, std::vector<VarInfo>& _args) = 0;
	virtual bool IsWorking() = 0;
	virtual bool	Run() =0;
	// 최상위 실행/재개 (NeoExecStatus 반환). IsSuspended() 가 true 면 ResumeTop() 을 호출한다.
	virtual int	ExecuteTop(int iFunctionID, std::vector<VarInfo>& _args) = 0;
	virtual int	ResumeTop() = 0;
	virtual NeoExecutionState GetExecutionState() = 0;
	// SuspendedSleep 또는 SuspendedDebugger일 때만 true. Running 상태는 포함하지 않는다.
	virtual bool IsSuspended() = 0;
	// 호스트→스크립트 함수 호출(Call/CallN/iCall/iCallN)용. idle 이면 최상위 컨텍스트를 대여하고
	// (반환값=대여했는지), 완료 후 EndHostCall 에서 반납한다. 실행 중(중첩 호출)이면 현재 컨텍스트 재사용.
	virtual NeoHostCallBegin BeginHostCall() = 0;
	virtual void EndHostCall(NeoHostCallBegin begin) = 0;
	virtual void BeginNestedScriptCall() = 0;
	virtual void EndNestedScriptCall() = 0;
	// 정지(sleep/디버거)되었거나 시분할 바인딩된 실행을 버리고 컨텍스트를 풀로 반납한다.
	// 전역 변수는 보존된다(워커는 그대로 살아있고 idle 로 돌아감).
	// 인터프리터 실행 중(네이티브 콜백 안 등)에는 컨텍스트를 해제할 수 없으므로 false 를 반환한다.
	virtual bool CancelExecution() = 0;
	virtual void SetTimeout(int iTimeout, int iCheckOpCount) = 0;
	virtual VarInfo* GetVar(const std::string& name) = 0;
	virtual bool BindWorkerFunction(const std::string& funName) = 0;
};


// 호스트(게임 엔진)가 컴파일러에 넘기는 네이티브 전역 심볼 하나.
// 소스 텍스트(preCompileHeader) 주입 대신 구조화 테이블로 전역 변수를 사전 선언한다.
struct NeoGlobalSymbol
{
	const char* name;
	bool        exported = true;   // true 면 GetVar 로 호스트가 바인딩 가능
};
struct NeoGlobalSymbolTable
{
	const NeoGlobalSymbol* symbols = nullptr;
	int                    count = 0;
};

struct NeoCompilerParam
{
	const void* pBufferSrc;
	int iLenSrc;

	std::string* err = nullptr;
	bool putASM = false;
	bool debug = false;
	bool allowGlobalInitLogic = true;
	int iStackSize = 50 * 1024;

	const NeoGlobalSymbolTable* globalSymbols = nullptr;
	const char* debugSourcePath = nullptr;
	std::vector<std::string>* debugSourceFiles = nullptr;
	const NeoCompileDefines* defines = nullptr;
	INeoLoader* loader = nullptr;              // import 모듈 해석용 loader(이 Runtime 것). 전역 대신 컴파일마다 주입.

	NeoCompilerParam(const void* pSrc, int SrcLen)
	{
		pBufferSrc = pSrc;
		iLenSrc = SrcLen;
	}
};

typedef void (*NEO_GLOBALINTERFACE)(INeoVMWorker*, void*);
struct NeoLoadVMParam
{
	NEO_GLOBALINTERFACE NeoGlobalInterface = nullptr;
	void* param = nullptr;
	NeoExecContextPool* execPool = nullptr;   // 실행 컨텍스트 풀(필수). 워커가 실행 시 여기서 대여/반납한다.

	NeoLoadVMParam()
	{
	}
};


struct INeoVM
{
protected:
	INeoVMWorker* _pMainWorker = NULL;
	bool _bError = false;
public:
	inline bool IsLocalErrorMsg() { return _bError; }
	static FunctionPtrNative RegisterNative(Neo_NativeFunction func);
	virtual int FindFunction(const std::string& name) =0;

	// 프로세스 전역 print/error 훅. 최초 초기화(1회) 때만 설정한다(VM 생성마다 X).
	// io_print / 에러 리포트가 이걸 통해 호스트로 출력. null 이면 std::cout 로 fallback.
	typedef void(*IO_Print)(const char* pMsg);
	static IO_Print m_pFunPrint;
	static IO_Print m_pFunError;

//	void Var_AddRef(VarInfo* d);
//	static void Move_DestNoRelease(VarInfo* v1, VarInfo* v2);

	void Var_ReleaseInternal(VarInfo* d);

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

	template<typename ... Types>
	bool Setup_TL(const std::string& funName, Types ... args) // Setup Time Limit
	{
		int iFID = _pMainWorker->FindFunction(funName);
		if (iFID == -1)
			return false;

		return _pMainWorker->Setup_TL(iFID, args...);
	}

	bool Call_TL(); // Time Limit
	VarInfo* GetVar(const std::string& name);

	static bool	RegisterTableCallBack(VarInfo* p, void* pUserData, Neo_NativeFunction func, Neo_NativeProperty property);

	virtual u32 CreateWorker(int iStackSize = 50 * 1024) =0;
	virtual bool ReleaseWorker(u32 id) = 0;
	virtual bool BindWorkerFunction(u32 id, const std::string& funName) = 0;
	virtual bool SetTimeout(u32 id, int iTimeout = -1, int iCheckOpCount = NEO_DEFAULT_CHECKOP) = 0;
	virtual bool IsWorking(u32 id) = 0;
	virtual bool UpdateWorker(u32 id) = 0;

	inline INeoVMWorker* GetMainWorker() { return _pMainWorker; }
	int GetMainWorkerID() { return _pMainWorker == NULL ? 0 : _pMainWorker->GetWorkerID(); }
	inline int GetBytesSize() { return _pMainWorker->GetBytesSize(); }

	virtual  const char* GetLastErrorMsg() = 0;
	virtual  bool IsLastErrorMsg() = 0;
	virtual  void ClearLastErrorMsg() = 0;
	// 네이티브(호스트 바인딩 함수)가 자기 실패 사유를 남긴다. 이 뒤에 네이티브가 false 를 반환하면
	// CallNative/PropertyNative 가 에러 opcode 로 점프하고, 여기 남긴 메시지가 IP/Line/스택트레이스와
	// 함께 최종 에러 상세로 조립된다(메시지를 안 남기면 "invalid call" 로 뭉개짐).
	// 이미 에러가 잡혀 있으면(먼저 난 원인 보존) 무시된다.
	virtual  void SetLastErrorMsg(const char* msg) = 0;

	virtual  INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, void* pBuffer, int iSize, bool blMainWorker = true, bool init = false, int iStackSize = 50 * 1024) =0; // 0 is error
	// 미리 만들어 둔 프로그램으로 워커를 붙인다. 같은 스크립트를 쓰는 워커 N 개가
	// 코드/함수테이블/디버그정보를 공유하고, 파싱과 코드 패치는 프로그램 생성 시 1회만 일어난다.
	// 소유권은 넘어가지 않는다 — 워커가 자체적으로 AddRef 하므로 호출측은 자기 참조를 Release 하면 된다.
	virtual  INeoVMWorker*	LoadVM(const NeoLoadVMParam* vparam, CNeoVMProgram* pProgram, bool blMainWorker = true, bool init = false, int iStackSize = 50 * 1024) = 0;
	virtual  bool PCall(int iModule) = 0;

	static INeoVM* 	CreateVM();
	static void		ReleaseVM(INeoVM* pVM);
	static bool		Compile(CNArchive& arw, const NeoCompilerParam& param);

	// 컴파일 이미지 → 공유 가능한 프로그램. 실패하면 nullptr(err 에 사유).
	// 반환값의 refCount 는 1 이며, 호출측이 Release() 로 자기 참조를 반납할 책임이 있다.
	static CNeoVMProgram* CreateProgram(const void* pBuffer, int iSize, std::string* err = nullptr);
	// Compile + CreateProgram 을 한 번에.
	static CNeoVMProgram* CompileToProgram(const NeoCompilerParam& param);
	// 호스트가 내부 헤더를 포함하지 않고 프로그램 수명을 다루기 위한 접근자. nullptr 안전.
	static void ProgramAddRef(CNeoVMProgram* pProgram);
	static void ProgramRelease(CNeoVMProgram* pProgram);

	static bool		Initialize(INeoLoader* loader = nullptr);
	static bool		Shutdown();

	// 등록된 빌트인(math/system/coroutine 및 string/list/map 메서드) 전체를 열거한다.
	// Initialize() 이후 유효. 코드어시스트가 이 목록을 당겨 자동완성 소스로 쓴다(하드코딩 불필요).
	static void		GetBuiltins(std::vector<NeoBuiltinInfo>& out);

	static INeoVM*	CompileAndLoadVM(const NeoCompilerParam& param, const NeoLoadVMParam* vparam = nullptr);
	static INeoVM*  CompileAndLoadRunVM(const NeoCompilerParam& param, const NeoLoadVMParam* vparam = nullptr);

	static bool		IsSinglePrecision() 
	{
		return sizeof(NS_FLOAT) == 4;
	}
};

};

