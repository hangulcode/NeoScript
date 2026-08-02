//==============================================================================
// NeoScript v2 공개 API 구현 (shim) — 기존 INeoVM/INeoVMWorker/CNeoVMProgram 위에 얹음.
// 내부 VM 은 손대지 않는다. 공개 헤더(NeoScript.h)는 내부 타입을 노출하지 않으므로
// 이 .cpp 만 내부 헤더를 include 한다.
//
// [상태] 골격: 핸들 인프라 + Runtime 수명주기(확정 내부 API)는 구현. 마샬링/디스패치/
//       빌더 본문은 "TODO(build)" 로 표시하고 의도한 내부 호출을 주석에 못박음.
//==============================================================================

#include "NeoScript.h"

// 내부 헤더 (공개 헤더는 이걸 include 하지 않는다 — 여기서만)
#include "NeoVM.h"
#include "NeoVMMap.h"
#include "NeoVMList.h"
#include "NeoVMProgram.h" // CNeoVMProgram::FindFunction (FindFunction 구현)
#include "NeoArchive.h"   // CNArchive (CompileToBytecode: 소스→바이트코드)

#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NeoScript
{
class RuntimeImpl; // 전방선언 (ObjectBinding 이 참조)

namespace
{

//------------------------------------------------------------------------------
// generation 검증 핸들 테이블. id=슬롯+1(0은 무효), generation 으로 use-after-free 감지.
//------------------------------------------------------------------------------
template <typename T>
class HandleTable
{
public:
    struct Slot { T* ptr = nullptr; uint32_t generation = 1; bool live = false; };

    uint32_t add(T* ptr)
    {
        uint32_t slot;
        if (!m_free.empty()) { slot = m_free.back(); m_free.pop_back(); }
        else { slot = static_cast<uint32_t>(m_slots.size()); m_slots.emplace_back(); }
        m_slots[slot].ptr = ptr;
        m_slots[slot].live = true;
        return slot + 1; // id
    }
    T* get(uint32_t id, uint32_t generation) const
    {
        if (id == 0 || id > m_slots.size()) return nullptr;
        const Slot& s = m_slots[id - 1];
        if (!s.live || s.generation != generation) return nullptr;
        return s.ptr;
    }
    uint32_t generationOf(uint32_t id) const { return (id && id <= m_slots.size()) ? m_slots[id - 1].generation : 0; }
    void remove(uint32_t id, uint32_t generation)
    {
        if (id == 0 || id > m_slots.size()) return;
        Slot& s = m_slots[id - 1];
        if (!s.live || s.generation != generation) return;
        s.live = false; s.ptr = nullptr; ++s.generation; // 다음 재사용에서 옛 핸들 무효화
        m_free.push_back(id - 1);
    }

private:
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free;
};

//------------------------------------------------------------------------------
// 내부 표현
//------------------------------------------------------------------------------
struct ProgramRec
{
    CNeoVMProgram* program = nullptr; // refcount 공유 불변 이미지
};

struct DefineSetRec
{
    NeoCompileDefines defines; // 한 번 빌드해두고 여러 Compile 에서 재사용
};

// INeoVM::Initialize 는 프로세스-1회 불변 테이블(토큰 문자열/빌트인 lib) 구축이다.
// Runtime 수명에 refcount 하지 않고 call_once 로 최초 1회만 실행 → 스레드 안전.
// per-Runtime auto-Shutdown 은 폐지(마지막 dtor 가 전역을 wipe → 타 스레드 compile 과
// 레이스 나던 문제 제거). Shutdown 은 필요 시 앱 종료 시점에 명시적으로 1회.
std::once_flag g_vmInitOnce;

// print/error 훅: 내부 IO_Print 는 (const char*) 시그니처라 공개 (StringView) 콜백을
// 그대로 대입할 수 없다 → 트램폴린으로 감싼다. 전역이지만 g_vmInitOnce(최초 init) 안에서만
// 1회 write 하므로 매 Runtime ctor 마다 쓰던 레이스가 없다(call_once 가 안전 발행 보장).
void (*g_userPrint)(StringView) = nullptr;
void (*g_userError)(StringView) = nullptr;
void PrintTrampoline(const char* s) { if (g_userPrint) g_userPrint(StringView(s ? s : "")); }
void ErrorTrampoline(const char* s) { if (g_userError) g_userError(StringView(s ? s : "")); }

struct RegisteredObject
{
    std::string name;
    NativeMethod method = nullptr;
    NativeProperty property = nullptr;
    void* defaultUserData = nullptr;
    bool declareGlobal = false;   // FreezeBindings 에서 전역 심볼로 낼지(등록 완료 후 일괄 구성)
};

struct InstanceRec;

// (instance, object) 바인딩 1건. RegisterTableCallBack 의 void* userData 로 넘긴다.
// 트램폴린이 이걸로 Runtime/Instance/디스패처/userData 를 복원한다.
struct ObjectBinding
{
    RuntimeImpl*            runtime = nullptr;
    InstanceRec*            inst = nullptr;
    const RegisteredObject* obj = nullptr;
    void*                   userData = nullptr;
    InstanceHandle          handle;   // CreateInstance 완료 후 채움(트램폴린이 ctx.instance() 로 노출)
};

// 컬렉션 리더 impl (공개 MapReader/ListReader 의 m_impl). InstanceRec 가 결과 리더를
// 담아야 하므로(CallReadMap/List) InstanceRec 앞에 정의. 중첩 리더는 pool 에 추가된다.
struct MapReaderImpl;
struct ListReaderImpl;
struct MapReaderImpl  { INeoVMWorker* w = nullptr; MapInfo*  map = nullptr;  std::deque<MapReaderImpl>* mpool = nullptr; std::deque<ListReaderImpl>* lpool = nullptr; };
struct ListReaderImpl { INeoVMWorker* w = nullptr; ListInfo* list = nullptr; std::deque<MapReaderImpl>* mpool = nullptr; std::deque<ListReaderImpl>* lpool = nullptr; };

// 중첩 바인딩 dedup 키 (RegisteredObject*, userData) 해시 — O(1) 조회(unordered_map).
struct PtrPairHash
{
    std::size_t operator()(const std::pair<const void*, const void*>& p) const noexcept
    {
        std::size_t h1 = std::hash<const void*>()(p.first);
        std::size_t h2 = std::hash<const void*>()(p.second);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

struct InstanceRec
{
    INeoVMWorker* worker = nullptr;
    RuntimeImpl*  runtime = nullptr;   // 글로벌 훅에서 복원(스택 지역변수 dangling 방지)
    InstanceHandle handle;             // 자기 핸들(CreateInstance 에서 채움; 중첩 바인딩이 사용)
    ProgramHandle program;
    void* instanceUserData = nullptr;
    // 객체명 -> 이 인스턴스에서의 userData (BindObject). 미지정이면 RegisteredObject.userData.
    std::unordered_map<std::string, void*> objectUserData;
    // CallReadMap/List 결과 리더 보관(다음 이 인스턴스 호출 전까지 유효). 중첩 리더도 여기 쌓임.
    std::deque<MapReaderImpl>  readMapPool;
    std::deque<ListReaderImpl> readListPool;
    // 구조화 반환(map/list)은 반환 컨텍스트를 유지해야 리더가 유효하므로 EndHostCall 을
    // 다음 호출까지 지연한다. 그 지연 상태를 여기에 보관.
    bool             hostCallPending = false;
    NeoHostCallBegin pendingBegin = NeoHostCallBegin::Acquired;
    bool             pendingNested = false;
    // 이 인스턴스의 객체 바인딩. reserve(objectCount) 로 안정 주소 유지(트램폴린이 &bindings[i] 보관).
    std::vector<ObjectBinding> bindings;
    // setObject/pushObject 로 반환값에 중첩된 바인딩 객체들. 반환 맵/리스트가 호출 종료 후에도
    // 스크립트에 남아 메서드 호출될 수 있으므로 인스턴스 수명까지 유지(deque=주소 안정).
    // (ObjectType, userData) 기준 dedup → 같은 userData 를 서로 다른 타입으로 반환해도 각각 1개만
    // (userData 단독 키였을 때: 두 타입 교대 반환 시 재사용 실패로 deque 무한증가하던 버그 수정). O(1) 해시.
    std::deque<ObjectBinding> nestedBindings;
    std::unordered_map<std::pair<const void*, const void*>, ObjectBinding*, PtrPairHash> nestedByKey; // key=(RegisteredObject*, userData)

    // Invocation 스크래치 (한 인스턴스에 동시 1개 Invocation). 인자를 VM VarInfo 로 직접 축적.
    int                  callFID = -1;
    std::vector<VarInfo> callArgs;
    int32_t              callTimeoutMs = -1;
    uint32_t             callBudget = 0;
    RunStatus            callStatus = RunStatus::Failed;
    // "arm" = Call() 로 준비됐으나 아직 invoke()/파괴 안 됨. 이 상태에서 같은 인스턴스에 또 Call() 하면
    // callFID/callArgs 가 덮여 첫 호출이 깨진다 → 겹침 감지해 두 번째 Call 을 안전 거부(무효 Invocation).
    // invoke() 시작 시 해제 → 스크립트 실행 중 네이티브가 같은 인스턴스를 재호출(중첩)하는 건 허용.
    bool                 callArmed = false;
};

} // namespace

//------------------------------------------------------------------------------
// 타입드 직접 마샬링 (owning Value 없음 — 규칙 통일, zero-copy).
//------------------------------------------------------------------------------
static RunStatus mapExecStatus(int st)
{
    switch (st)
    {
    case NEOEXEC_COMPLETED: return RunStatus::Completed;
    case NEOEXEC_SUSPENDED: return RunStatus::Suspended;
    default:                return RunStatus::Failed;
    }
}

static void SetVarString(INeoVMWorker* w, VarInfo* out, StringView s)
{
    w->Var_SetStringA(out, std::string(s.data(), s.size()));
}

// CompileDefine[] → 내부 NeoCompileDefines. CreateDefineSet(1회) 와 인라인 경로가 공유.
static void BuildNeoDefines(Span<const CompileDefine> defs, NeoCompileDefines& out)
{
    for (std::size_t i = 0; i < defs.size(); ++i)
    {
        const CompileDefine& d = defs[i];
        NeoCompileDefineToken tok;
        switch (d.kind)
        {
        case DefineKind::Int:    tok.type = NEO_DEFINE_TOKEN_INT; break;
        case DefineKind::Float:  tok.type = NEO_DEFINE_TOKEN_FLOAT; break;
        case DefineKind::String: tok.type = NEO_DEFINE_TOKEN_STRING; break;
        case DefineKind::Bool:   tok.type = (d.text.size() == 4 && std::memcmp(d.text.data(), "true", 4) == 0) ? NEO_DEFINE_TOKEN_TRUE : NEO_DEFINE_TOKEN_FALSE; break;
        case DefineKind::Null:   tok.type = NEO_DEFINE_TOKEN_NULL; break;
        default:                 tok.type = NEO_DEFINE_TOKEN_IDENTIFIER; break;
        }
        tok.text = std::string(d.text.data(), d.text.size());
        out.values[std::string(d.name.data(), d.name.size())] = tok;
    }
}

//------------------------------------------------------------------------------
// pimpl 내부 표현 (공개 CallContext/빌더/리더의 m_impl 이 이걸 가리킨다) + 트램폴린 선언.
//------------------------------------------------------------------------------
namespace
{
struct CallContextImpl;

// (MapReaderImpl/ListReaderImpl 은 InstanceRec 앞에서 이미 정의됨)
struct MapBuilderImpl  { INeoVMWorker* w = nullptr; MapInfo*  map = nullptr;  CallContextImpl* ctx = nullptr; };
struct ListBuilderImpl { INeoVMWorker* w = nullptr; ListInfo* list = nullptr; CallContextImpl* ctx = nullptr; };

struct CallContextImpl
{
    INeoVMWorker*  worker = nullptr;
    IRuntime*      runtime = nullptr;
    InstanceHandle instance;
    void*          bindingUserData = nullptr;
    void*          instanceUserData = nullptr;
    short          argCount = 0;
    // 프로퍼티 접근용 오버라이드: get 은 retX 를 propertyVar 로, set 은 argX(0) 를 propertyVar 로.
    VarInfo*       propertyVar = nullptr;
    bool           propertyIsArg = false;   // true=set(arg0=propertyVar), false=get(ret=propertyVar)
    // 이 호출 동안 만들어지는 빌더/리더 impl 보관(안정 주소 위해 deque).
    std::deque<MapBuilderImpl>  mapB;
    std::deque<ListBuilderImpl> listB;
    std::deque<MapReaderImpl>   mapR;
    std::deque<ListReaderImpl>  listR;
};

static StringView VmName(const VMString* s)
{
    if (s == nullptr) return StringView();
    return StringView(s->_str); // VMString 은 std::string _str 보유
}

static ValueType VarTypeToValueType(VAR_TYPE t)
{
    switch (t)
    {
    case VAR_BOOL:   return ValueType::Bool;
    case VAR_INT:    return ValueType::Int;
    case VAR_FLOAT:  return ValueType::Float;
    case VAR_STRING: return ValueType::String;
    case VAR_VEC2:   return ValueType::Vec2;
    case VAR_VEC3:   return ValueType::Vec3;
    case VAR_VEC4:   return ValueType::Vec4;
    case VAR_QUAT:   return ValueType::Quat;
    case VAR_MAP:    return ValueType::Map;
    case VAR_LIST:   return ValueType::List;
    case VAR_SET:    return ValueType::Set;
    case VAR_FUN:
    case VAR_FUN_NATIVE: return ValueType::Function;
    default:         return ValueType::None;
    }
}
} // namespace

static bool MethodTrampoline(INeoVMWorker* N, void* pUserData, const VMString* pStr, short args);
static bool PropertyTrampoline(INeoVMWorker* N, void* pUserData, const VMString* pStr, VarInfo* p, bool get);

// 공개 pimpl 타입의 m_impl 을 셋/겟하는 내부 전용 접근자(헤더에서 friend 로 허용).
struct NeoScriptInternal
{
    static CallContext ctx (void* i)        { CallContext c;  c.m_impl = i; return c; }
    static MapBuilder  mapB(void* i)        { MapBuilder b;   b.m_impl = i; return b; }
    static ListBuilder listB(void* i)       { ListBuilder b;  b.m_impl = i; return b; }
    static MapReader   mapR(const void* i)  { MapReader r;    r.m_impl = i; return r; }
    static ListReader  listR(const void* i) { ListReader r;   r.m_impl = i; return r; }
    static Invocation  inv(void* i)         { Invocation v;   v.m_impl = i; return v; }
    static void*       invImpl(const Invocation& v) { return v.m_impl; }
};

class DebuggerImpl;                       // IDebugger shim (RuntimeImpl 뒤에서 정의)
static void DestroyDebugger(DebuggerImpl*); // 완전 타입 소멸(dtor 뒤 정의)

// 공개 ILoader → 내부 INeoLoader 어댑터. RuntimeDesc.loader 를 실제로 배선한다(Runtime별).
// import 해석 시 내부 VM 이 Load/Unload 를 호출한다(버퍼 소유: Load 가 malloc, Unload 가 free).
struct LoaderAdapter : INeoLoader
{
    ILoader* user = nullptr;
    std::string libPath;
    bool Load(const char* pFileName, void*& pBuffer, int& iLen) override
    {
        if (!user) return false;
        std::vector<uint8_t> data;
        if (!user->Load(StringView(pFileName ? pFileName : ""), data)) return false;
        pBuffer = std::malloc(data.size() ? data.size() : 1);
        if (!pBuffer) return false;
        std::memcpy(pBuffer, data.data(), data.size());
        iLen = static_cast<int>(data.size());
        return true;
    }
    void Unload(const char*, void* pBuffer, int) override { std::free(pBuffer); }
    const char* GetLibPath() override { libPath = user ? user->LibPath().str() : std::string(); return libPath.c_str(); }
};

//==============================================================================
// RuntimeImpl
//==============================================================================
class RuntimeImpl final : public IRuntime
{
    friend class DebuggerImpl;
public:
    explicit RuntimeImpl(const RuntimeDesc& desc);
    ~RuntimeImpl() override;

    //--- 초기 설정 ---
    bool RegisterFunction(const NativeFunctionDesc& desc) override;
    bool RegisterObject(const NativeObjectDesc& desc) override;
    void FreezeBindings() override;
    ObjectType GetObjectType(StringView name) override;
    // setObject/pushObject 내부용: 중첩 바인딩을 (type,userData) 로 dedup 재사용(주소 안정).
    ObjectBinding* AcquireNestedBinding(InstanceHandle handle, const RegisteredObject* ro, void* userData);

    //--- Program ---
    CompileResult Compile(const CompileDesc& desc) override;
    Error CompileToBytecode(const CompileDesc& desc, std::vector<uint8_t>& out) override;
    ProgramHandle LoadProgram(Span<const uint8_t> bytecode, Error* error) override;
    void DestroyProgram(ProgramHandle program) override;
    DefineSetHandle CreateDefineSet(Span<const CompileDefine> defines) override;
    void DestroyDefineSet(DefineSetHandle set) override;

    //--- Instance ---
    InstanceHandle CreateInstance(ProgramHandle program, const InstanceDesc& desc) override;
    void DestroyInstance(InstanceHandle instance) override;
    bool IsAlive(InstanceHandle instance) const override;
    void ResetInstance(InstanceHandle instance) override;
    InstanceState GetState(InstanceHandle instance) const override;
    bool BindObject(InstanceHandle instance, StringView name, void* userData) override;
    void BindGlobalObject(InstanceHandle instance, StringView globalName, ObjectType type, void* userData) override;
    void BindGlobalMapObject(InstanceHandle instance, StringView mapName, StringView key, ObjectType type, void* userData) override;
    RunStatus RunGlobalInit(InstanceHandle instance) override;

    //--- 호출 ---
    FunctionHandle FindFunction(ProgramHandle program, StringView name) const override;
    Invocation Call(InstanceHandle instance, FunctionHandle function) override;
    Invocation Call(InstanceHandle instance, StringView functionName) override;

    //--- 재개/전역/async ---
    RunStatus Resume(InstanceHandle instance, const ResumeDesc& desc) override;
    void Cancel(InstanceHandle instance) override;
    bool StartSliced(InstanceHandle instance, StringView functionName, int32_t timeoutMs, uint32_t budget) override;
    RunStatus UpdateSliced(InstanceHandle instance) override;
    bool IsRunning(InstanceHandle instance) const override;
    bool GetGlobalInt(InstanceHandle instance, StringView name, int32_t& out) const override;
    bool GetGlobalFloat(InstanceHandle instance, StringView name, float& out) const override;
    bool GetGlobalString(InstanceHandle instance, StringView name, StringView& out) const override;
    bool SetGlobalInt(InstanceHandle instance, StringView name, int32_t v) override;
    bool SetGlobalFloat(InstanceHandle instance, StringView name, float v) override;
    bool SetGlobalString(InstanceHandle instance, StringView name, StringView v) override;
    bool CompleteAsyncInt(AsyncToken token, int32_t v) override;
    bool FailAsync(AsyncToken token, const Error& error) override;

    bool TakeLastError(StringView& out) override;
    bool PeekLastError(StringView& out) const override;

    //--- 리플렉션/디버거 ---
    void GetBuiltins(std::vector<BuiltinInfo>& out) const override;
    IDebugger* GetDebugger() override;

    // 내부: 글로벌 인터페이스 훅에서 등록 객체를 워커 맵에 바인딩
    void BindRegisteredObjects(INeoVMWorker* worker, InstanceRec* inst);
    const std::vector<RegisteredObject>& registeredObjects() const { return m_objects; }
    // Invocation 이 쓰는 내부 훅
    InstanceRec* resolveInstance(InstanceHandle h) const { return m_instances.get(h.id, h.generation); }
    void FlushPendingCall(InstanceRec* inst);
    // 내부 전용: 인스턴스 워커/바인딩 헬퍼(BindGlobalObject/MapObject, retInstanceGlobal 이 사용).
    INeoVMWorker* instanceWorker(InstanceHandle h) const;
    void BindObjectInto(void* worker, void* mapVar, StringView key, ObjectType type, void* userData);
    void BindObjectToVar(void* worker, void* varInfo, ObjectType type, void* userData);

private:
    ProgramRec*  resolveProgram(ProgramHandle h) const { return m_programs.get(h.id, h.generation); }
    // CompileDesc → NeoCompilerParam. 백킹 스토리지(err/gtab/inlineDefines/dbgPath)는 호출자가 소유하고
    // 컴파일 호출까지 살아있어야 한다(param 이 그 주소를 가리킴). Compile / CompileToBytecode 공용.
    void SetupCompilerParam(const CompileDesc& desc, NeoCompilerParam& param, std::string& err,
                            NeoGlobalSymbolTable& gtab, NeoCompileDefines& inlineDefines, std::string& dbgPath);

    RuntimeDesc m_desc;
    LoaderAdapter m_loaderAdapter;   // 공개 ILoader* 배선(nativeLoader 없을 때 사용)
    INeoVM* m_vm = nullptr;
    NeoExecContextPool* m_pool = nullptr;
    bool m_frozen = false;
    DebuggerImpl* m_debugger = nullptr;                              // 지연 생성
    std::unordered_map<INeoVMWorker*, InstanceHandle> m_workerHandle; // worker→instance (디버거 역참조)
    std::string m_lastError;                                          // TakeLastError 반환 뷰 소유

    std::vector<RegisteredObject> m_objects;              // FreezeBindings 전 등록
    std::vector<NeoGlobalSymbol> m_globalSymbols;         // 컴파일에 넘길 전역 심볼
    mutable HandleTable<ProgramRec> m_programs;
    mutable HandleTable<InstanceRec> m_instances;
    mutable HandleTable<DefineSetRec> m_defineSets;
};

//------------------------------------------------------------------------------
// 수명주기
//------------------------------------------------------------------------------
RuntimeImpl::RuntimeImpl(const RuntimeDesc& desc) : m_desc(desc)
{
    // 프로세스-1회 전역 초기화. call_once → 멀티스레드에서 각 스레드가 Runtime 을 만들어도
    // 최초 1회만, 그 사이 다른 스레드는 완료까지 블록. print/error 훅도 여기서만 설정한다
    // (VM 생성마다 X → 전역 포인터 write 레이스 없음). loader/printFn/errorFn 모두 최초
    // 호출 Runtime 의 것이 프로세스 전체에 적용됨(스레드별 다른 import loader 는 별도 축).
    m_loaderAdapter.user = m_desc.loader; // 공개 ILoader* 배선(Compile 에서 nativeLoader 없으면 이걸 사용)
    std::call_once(g_vmInitOnce, [this] {
        INeoVM::Initialize();   // 토큰/빌트인 lib 1회 구축(loader 는 Compile 마다 param.loader 로 전달)
        // print/error 는 내부 VM 의 프로세스-전역 훅(Runtime별 아님). g_once 안에서만 설정 → write 레이스 없음.
        // 즉 최초 생성 Runtime 의 printFn/errorFn 이 프로세스 전체에 적용된다(헤더에 명시).
        if (m_desc.printFn) { g_userPrint = m_desc.printFn; INeoVM::m_pFunPrint = &PrintTrampoline; }
        if (m_desc.errorFn) { g_userError = m_desc.errorFn; INeoVM::m_pFunError = &ErrorTrampoline; }
    });
    m_vm = INeoVM::CreateVM();
    m_pool = NeoExecContextPool_Create();
}

RuntimeImpl::~RuntimeImpl()
{
    DestroyDebugger(m_debugger);
    if (m_vm) INeoVM::ReleaseVM(m_vm);
    if (m_pool) NeoExecContextPool_Destroy(m_pool);
    // 프로그램 refcount 반납은 DestroyProgram 에서. 남은 것 정리는 TODO(build).
    // INeoVM::Shutdown 은 프로세스 전역(토큰/lib) 파괴라 per-Runtime 에서 부르지 않는다
    // (타 스레드가 compile 중일 수 있음). 필요 시 앱 종료 시점에 명시적으로 1회.
}

//------------------------------------------------------------------------------
// 등록
//------------------------------------------------------------------------------
bool RuntimeImpl::RegisterFunction(const NativeFunctionDesc& /*desc*/)
{
    if (m_frozen) return false;
    // TODO(build): 단독 전역 네이티브 함수. Neo3D 는 전부 객체(RegisterObject) 경로라
    // 우선순위 낮음. NeoGlobalSymbol + 글로벌훅에서 FunctionPtr 값으로 GetVar 세팅 예정.
    return false;
}

bool RuntimeImpl::RegisterObject(const NativeObjectDesc& desc)
{
    if (m_frozen || desc.name.data() == nullptr) return false;
    RegisteredObject o;
    o.name = desc.name.str();
    o.method = desc.method;
    o.property = desc.property;
    o.defaultUserData = desc.userData;
    o.declareGlobal = desc.declareGlobal;
    m_objects.push_back(std::move(o));
    // 주의: 전역 심볼(NeoGlobalSymbol.name = c_str())은 FreezeBindings 에서 일괄 구성한다.
    // 여기서 m_objects.back().name.c_str() 을 저장하면 다음 RegisterObject 의 push_back 이
    // vector 를 재할당해 그 포인터가 dangling 된다(다중 등록 시 garbage 심볼명 버그).
    return true;
}

void RuntimeImpl::FreezeBindings()
{
    m_frozen = true;
    // 등록이 끝나 m_objects 가 더는 안 커지는 시점에 전역 심볼 구성(요소 std::string 주소 안정).
    // 스크립트가 `export var` 로 직접 선언하는 객체(declareGlobal=false)는 중복 방지 위해 제외.
    m_globalSymbols.clear();
    for (const RegisteredObject& o : m_objects)
        if (o.declareGlobal)
            m_globalSymbols.push_back(NeoGlobalSymbol{ o.name.c_str(), /*exported=*/true });
}

ObjectType RuntimeImpl::GetObjectType(StringView name)
{
    ObjectType t;
    // FreezeBindings 후엔 m_objects 가 더 안 커지므로 요소 주소 안정 → 포인터 반환 OK.
    for (const RegisteredObject& o : m_objects)
        if (o.name.size() == name.size() &&
            std::memcmp(o.name.data(), name.data(), name.size()) == 0)
        {
            t.m_impl = const_cast<RegisteredObject*>(&o);
            break;
        }
    return t;
}

ObjectBinding* RuntimeImpl::AcquireNestedBinding(InstanceHandle handle, const RegisteredObject* ro, void* userData)
{
    InstanceRec* inst = resolveInstance(handle);
    if (inst == nullptr) return nullptr;
    // (type,userData) dedup: 같은 (타입,객체) 가 반복 반환돼도 바인딩 1개만(살아있는 객체 수만큼 bounded).
    auto key = std::make_pair(static_cast<const void*>(ro), static_cast<const void*>(userData));
    auto it = inst->nestedByKey.find(key);
    if (it != inst->nestedByKey.end())
        return it->second;
    ObjectBinding b;
    b.runtime  = this;
    b.inst     = inst;
    b.obj      = ro;
    b.userData = userData;
    b.handle   = inst->handle;
    inst->nestedBindings.push_back(b);   // deque = 주소 안정(트램폴린이 포인터 보관)
    ObjectBinding* p = &inst->nestedBindings.back();
    inst->nestedByKey[key] = p;
    return p;
}

//------------------------------------------------------------------------------
// Program
//------------------------------------------------------------------------------
void RuntimeImpl::SetupCompilerParam(const CompileDesc& desc, NeoCompilerParam& param, std::string& err,
                                    NeoGlobalSymbolTable& gtab, NeoCompileDefines& inlineDefines, std::string& dbgPath)
{
    param.err = &err;
    param.debug = desc.includeDebugInfo;
    param.putASM = desc.emitAsm;   // ASM 덤프(진단): 내부 컴파일러가 OutAsm 으로 stdout 출력
    gtab = NeoGlobalSymbolTable{ m_globalSymbols.data(), static_cast<int>(m_globalSymbols.size()) };
    param.globalSymbols = m_globalSymbols.empty() ? nullptr : &gtab;
    // TODO(build): desc.sourceName -> param.debugSourcePath, optimize 플래그 매핑

    if (desc.defineSet)
    {
        // 재사용 집합: 미리 빌드된 맵을 그대로 가리킨다 → 매 Compile 재구성 0.
        if (DefineSetRec* ds = m_defineSets.get(desc.defineSet.id, desc.defineSet.generation))
            param.defines = &ds->defines;
    }
    else if (!desc.defines.empty())
    {
        BuildNeoDefines(desc.defines, inlineDefines);
        param.defines = &inlineDefines;
    }

    if (desc.debugSourcePath.data() != nullptr && desc.debugSourcePath.size() > 0)
    {
        dbgPath.assign(desc.debugSourcePath.data(), desc.debugSourcePath.size());
        param.debugSourcePath = dbgPath.c_str();
    }
    param.debugSourceFiles = desc.debugSourceFiles;
    // import 해석: 이 Runtime 의 loader(전역 아님). nativeLoader(interop) 우선, 없으면 공개 ILoader 어댑터.
    param.loader = m_desc.nativeLoader ? static_cast<INeoLoader*>(m_desc.nativeLoader)
                 : (m_desc.loader ? &m_loaderAdapter : nullptr);
}

CompileResult RuntimeImpl::Compile(const CompileDesc& desc)
{
    CompileResult r;
    // 백킹 스토리지(param 이 가리킴 — CompileToProgram 반환까지 생존).
    std::string err, dbgPath;
    NeoGlobalSymbolTable gtab{};
    NeoCompileDefines inlineDefines;
    NeoCompilerParam param(desc.source.data(), static_cast<int>(desc.source.size()));
    SetupCompilerParam(desc, param, err, gtab, inlineDefines, dbgPath);

    CNeoVMProgram* prog = INeoVM::CompileToProgram(param);
    if (prog == nullptr)
    {
        r.error.code = 1;
        r.error.message = err;
        r.error.sourceName = desc.sourceName.str();
        return r;
    }
    ProgramRec* rec = new ProgramRec();
    rec->program = prog; // refCount==1, 소유
    uint32_t id = m_programs.add(rec);
    r.program = ProgramHandle{ id, m_programs.generationOf(id) };
    return r;
}

Error RuntimeImpl::CompileToBytecode(const CompileDesc& desc, std::vector<uint8_t>& out)
{
    // 소스 → 바이트코드(빌드 산출물). Program 은 만들지 않는다. 저장/전송 후 LoadProgram 으로 로드.
    std::string err, dbgPath;
    NeoGlobalSymbolTable gtab{};
    NeoCompileDefines inlineDefines;
    NeoCompilerParam param(desc.source.data(), static_cast<int>(desc.source.size()));
    SetupCompilerParam(desc, param, err, gtab, inlineDefines, dbgPath);

    CNArchive arw;   // 자체 버퍼(성장). INeoVM::Compile 이 여기 바이트코드를 쓴다.
    if (!INeoVM::Compile(arw, param))
    {
        Error e; e.code = 1; e.message = err; e.sourceName = desc.sourceName.str();
        return e;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(arw.GetData());
    out.assign(bytes, bytes + arw.GetBufferOffset());
    return Error{};   // ok
}


ProgramHandle RuntimeImpl::LoadProgram(Span<const uint8_t> bytecode, Error* error)
{
    std::string err;
    CNeoVMProgram* prog = INeoVM::CreateProgram(bytecode.data(), static_cast<int>(bytecode.size()), &err);
    if (prog == nullptr)
    {
        if (error) { error->code = 1; error->message = err; }
        return {};
    }
    ProgramRec* rec = new ProgramRec();
    rec->program = prog;
    uint32_t id = m_programs.add(rec);
    return ProgramHandle{ id, m_programs.generationOf(id) };
}

DefineSetHandle RuntimeImpl::CreateDefineSet(Span<const CompileDefine> defines)
{
    DefineSetRec* rec = new DefineSetRec();
    BuildNeoDefines(defines, rec->defines); // 여기서 1회만 빌드
    uint32_t id = m_defineSets.add(rec);
    return DefineSetHandle{ id, m_defineSets.generationOf(id) };
}
void RuntimeImpl::DestroyDefineSet(DefineSetHandle h)
{
    DefineSetRec* rec = m_defineSets.get(h.id, h.generation);
    if (!rec) return;
    delete rec;
    m_defineSets.remove(h.id, h.generation);
}

void RuntimeImpl::DestroyProgram(ProgramHandle h)
{
    ProgramRec* rec = resolveProgram(h);
    if (!rec) return;
    INeoVM::ProgramRelease(rec->program); // 인스턴스가 AddRef 했으면 실제 free 는 지연
    delete rec;
    m_programs.remove(h.id, h.generation);
}

//------------------------------------------------------------------------------
// Instance
//------------------------------------------------------------------------------
// 글로벌 인터페이스 훅: LoadVM 이 새 워커에 대해 호출. param 에 (Runtime,Instance) 전달.
static void ShimGlobalInterface(INeoVMWorker* worker, void* param);

InstanceHandle RuntimeImpl::CreateInstance(ProgramHandle programHandle, const InstanceDesc& desc)
{
    ProgramRec* prog = resolveProgram(programHandle);
    if (!prog) return {};

    InstanceRec* inst = new InstanceRec();
    inst->runtime = this;
    inst->program = programHandle;
    inst->instanceUserData = desc.userData;

    // LoadVM 훅(ShimGlobalInterface→onInstanceBind→BindObjectInto)이 워커→핸들 조회를 하므로
    // 핸들을 LoadVM 전에 확보한다(과거엔 LoadVM 후 설정 → 중첩 바인딩이 핸들 못 찾던 버그).
    uint32_t id = m_instances.add(inst);
    InstanceHandle h{ id, m_instances.generationOf(id) };
    inst->handle = h;

    NeoLoadVMParam vparam;
    vparam.execPool = m_pool;
    vparam.NeoGlobalInterface = &ShimGlobalInterface;
    // 훅에서 Runtime+Instance 를 찾도록 heap-안정 InstanceRec 를 전달(스택 지역변수 금지).
    vparam.param = inst;

    // 인스턴스 = program 공유 비-메인 워커. desc.runGlobalInit -> init 플래그.
    INeoVMWorker* worker = m_vm->LoadVM(&vparam, prog->program, /*mainWorker=*/false, /*init=*/desc.runGlobalInit);
    if (worker == nullptr) { m_instances.remove(h.id, h.generation); delete inst; return {}; }
    inst->worker = worker;
    m_workerHandle[worker] = h;      // 디버거 역참조 + 이후 조회용(훅 중엔 BindRegisteredObjects 에서 이미 등록)
    return h;
}

void RuntimeImpl::BindRegisteredObjects(INeoVMWorker* worker, InstanceRec* inst)
{
    // 훅 시점(LoadVM 중)에 worker→handle 을 등록: 이 함수 끝의 onInstanceBind→BindObjectInto(중첩
    // 바인딩)가 워커로 인스턴스를 찾을 수 있어야 하기 때문. (CreateInstance 는 LoadVM 후 재설정)
    inst->worker = worker;
    m_workerHandle[worker] = inst->handle;

    // 각 등록 객체를 이 워커의 전역 맵에 바인딩(현 neo_globalinterface 모델).
    inst->bindings.clear();
    inst->bindings.reserve(m_objects.size()); // 주소 안정(트램폴린이 &bindings[i] 보관)
    for (const RegisteredObject& o : m_objects)
    {
        if (o.method == nullptr && o.property == nullptr) continue; // 선언 전용(디스패처 없음)
        VarInfo* pVar = worker->GetVar(o.name);
        if (!pVar) continue;
        if (!worker->ResetVarType(pVar, VAR_MAP)) continue;

        void* userData = o.defaultUserData;
        auto it = inst->objectUserData.find(o.name);
        if (it != inst->objectUserData.end()) userData = it->second;

        ObjectBinding binding;
        binding.runtime = this;
        binding.inst = inst;
        binding.obj = &o;
        binding.userData = userData;
        binding.handle = inst->handle;   // 훅 전에 확보한 핸들(트램폴린이 ctx.instance() 로 노출)
        inst->bindings.push_back(binding);
        ObjectBinding* b = &inst->bindings.back(); // reserve() 로 주소 안정
        INeoVM::RegisterTableCallBack(pVar, b,
            (o.method != nullptr) ? &MethodTrampoline : nullptr,
            (o.property != nullptr) ? &PropertyTrampoline : nullptr);
    }
    // 소비자의 전역 바인딩 훅 트리거. 훅은 인스턴스 핸들로 BindGlobalObject/BindGlobalMapObject 호출.
    if (m_desc.onInstanceBind)
        m_desc.onInstanceBind(this, inst->handle, inst->instanceUserData);
}

static void ShimGlobalInterface(INeoVMWorker* worker, void* param)
{
    InstanceRec* inst = static_cast<InstanceRec*>(param);
    if (inst && inst->runtime) inst->runtime->BindRegisteredObjects(worker, inst);
}

void RuntimeImpl::DestroyInstance(InstanceHandle h)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return;
    FlushPendingCall(inst); // 지연된 borrowed 컨텍스트 반납
    m_workerHandle.erase(inst->worker);
    m_vm->ReleaseWorker(inst->worker->GetWorkerID());
    delete inst;
    m_instances.remove(h.id, h.generation);
}

bool RuntimeImpl::IsAlive(InstanceHandle h) const
{
    return resolveInstance(h) != nullptr;
}

INeoVMWorker* RuntimeImpl::instanceWorker(InstanceHandle h) const   // 내부 전용
{
    InstanceRec* inst = resolveInstance(h);
    return inst ? inst->worker : nullptr;
}

void RuntimeImpl::BindGlobalObject(InstanceHandle h, StringView globalName, ObjectType type, void* userData)
{
    // 인스턴스 전역 변수에 v2 네이티브 객체 바인딩(워커는 shim 안에서만). onInstanceBind 훅용.
    InstanceRec* inst = resolveInstance(h);
    if (!inst || !inst->worker) return;
    VarInfo* pVar = inst->worker->GetVar(std::string(globalName.data(), globalName.size()));
    if (!pVar) return;
    BindObjectToVar(inst->worker, pVar, type, userData);
}

void RuntimeImpl::BindGlobalMapObject(InstanceHandle h, StringView mapName, StringView key, ObjectType type, void* userData)
{
    // 인스턴스 전역 VAR_MAP 에 key 로 v2 네이티브 객체 삽입(서브서비스).
    InstanceRec* inst = resolveInstance(h);
    if (!inst || !inst->worker) return;
    VarInfo* pMap = inst->worker->GetVar(std::string(mapName.data(), mapName.size()));
    if (!pMap) return;
    BindObjectInto(inst->worker, pMap, key, type, userData);
}

void RuntimeImpl::BindObjectInto(void* worker, void* mapVar, StringView key, ObjectType type, void* userData)
{
    // [전이용] 구 neo_globalinterface2 대체: mapVar(VAR_MAP)에 key 로 v2 바인딩 객체 삽입.
    INeoVMWorker* w = static_cast<INeoVMWorker*>(worker);
    VarInfo* pMap = static_cast<VarInfo*>(mapVar);
    const RegisteredObject* ro = static_cast<const RegisteredObject*>(type.m_impl);
    if (!w || !pMap || pMap->GetType() != VAR_MAP || !pMap->_tbl || !ro) return;
    auto it = m_workerHandle.find(w);   // 워커 → 인스턴스 핸들
    if (it == m_workerHandle.end()) return;
    ObjectBinding* bind = AcquireNestedBinding(it->second, ro, userData);
    if (!bind) return;
    VarInfo v{};
    w->ResetVarType(&v, VAR_MAP);
    INeoVM::RegisterTableCallBack(&v, bind,
        (ro->method)   ? &MethodTrampoline   : nullptr,
        (ro->property) ? &PropertyTrampoline : nullptr);
    pMap->_tbl->Insert(std::string(key.data(), key.size()), &v);
    w->Var_Release(&v);
}

void RuntimeImpl::BindObjectToVar(void* worker, void* varInfo, ObjectType type, void* userData)
{
    // [전이용] varInfo 를 그 자체로 v2 바인딩 객체(VAR_MAP + 디스패처)로. 구 neo_globalinterface2 대체.
    INeoVMWorker* w = static_cast<INeoVMWorker*>(worker);
    VarInfo* pVar = static_cast<VarInfo*>(varInfo);
    const RegisteredObject* ro = static_cast<const RegisteredObject*>(type.m_impl);
    if (!w || !pVar || !ro) return;
    auto it = m_workerHandle.find(w);
    if (it == m_workerHandle.end()) return;
    if (!w->ResetVarType(pVar, VAR_MAP)) return;
    ObjectBinding* bind = AcquireNestedBinding(it->second, ro, userData);
    if (!bind) return;
    INeoVM::RegisterTableCallBack(pVar, bind,
        (ro->method)   ? &MethodTrampoline   : nullptr,
        (ro->property) ? &PropertyTrampoline : nullptr);
}

void RuntimeImpl::ResetInstance(InstanceHandle) { /* TODO(build): 전역/스택 초기화 재실행 */ }

RunStatus RuntimeImpl::RunGlobalInit(InstanceHandle h)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return RunStatus::Failed;
    bool ok = m_vm->PCall(inst->worker->GetWorkerID()); // 최상위 실행(구 PCall)
    // 디버거 브레이크포인트/sleep 로 정지했으면 아직 working → Suspended.
    if (m_vm->IsWorking(inst->worker->GetWorkerID())) return RunStatus::Suspended;
    return ok ? RunStatus::Completed : RunStatus::Failed;
}

InstanceState RuntimeImpl::GetState(InstanceHandle h) const
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return InstanceState::Failed;
    switch (inst->worker->GetExecutionState())
    {
    case NeoExecutionState::Idle:              return InstanceState::Idle;
    case NeoExecutionState::Running:           return InstanceState::Running;
    case NeoExecutionState::SuspendedSleep:    return InstanceState::Suspended;
    case NeoExecutionState::SuspendedDebugger: return InstanceState::Suspended;
    default:                                   return InstanceState::Idle;
    }
}

bool RuntimeImpl::BindObject(InstanceHandle h, StringView name, void* userData)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    std::string n = name.str();
    inst->objectUserData[n] = userData;
    // 이미 등록된 바인딩(트램폴린이 &ObjectBinding 을 보관)의 userData 를 갱신.
    // RegisterTableCallBack 은 이 ObjectBinding 포인터를 그대로 되돌려주므로 재등록 불필요.
    for (ObjectBinding& b : inst->bindings)
        if (b.obj != nullptr && b.obj->name == n) { b.userData = userData; break; }
    return true;
}

//------------------------------------------------------------------------------
// 호출
//------------------------------------------------------------------------------
FunctionHandle RuntimeImpl::FindFunction(ProgramHandle h, StringView name) const
{
    // 함수 인덱스는 프로그램 불변 → 프로그램 함수테이블에서 직접 조회(인스턴스 불요).
    // 프로그램 로드 직후 1회 캐시해 프레임 호출은 정수 핸들만 쓰게 하는 핫패스용.
    ProgramRec* rec = resolveProgram(h);
    if (!rec || !rec->program) return {};
    int idx = rec->program->FindFunction(std::string(name.data(), name.size()));
    if (idx < 0) return {};
    return FunctionHandle{ static_cast<uint32_t>(idx), h.id, h.generation };
}

void RuntimeImpl::FlushPendingCall(InstanceRec* inst)
{
    if (!inst->hostCallPending) return;
    inst->hostCallPending = false;
    inst->worker->EndHostCall(inst->pendingBegin);   // 여기서 반환 컨텍스트 반납 → 이전 리더 무효
    if (inst->pendingNested) inst->worker->EndNestedScriptCall();
    inst->readMapPool.clear();
    inst->readListPool.clear();
}

Invocation RuntimeImpl::Call(InstanceHandle h, FunctionHandle fn)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst || !fn) return Invocation();
    // 핸들이 Program 식별자를 지니면(FindFunction/argFunction), 이 인스턴스의 Program 과 일치 검증
    // (타 프로그램에서 얻은 함수핸들을 다른 인스턴스에 넘기는 오용 차단). 0=미지정이면 생략(하위호환).
    if (fn.programId != 0 &&
        (inst->program.id != fn.programId || inst->program.generation != fn.programGeneration))
        return Invocation();
    // 겹침 방지: 이전 Invocation 이 arm 된 채(invoke/파괴 전)면 두 번째 Call 을 안전 거부(상태 파괴 회피).
    if (inst->callArmed) return Invocation();
    FlushPendingCall(inst);
    inst->callFID = static_cast<int>(fn.index);
    inst->callArgs.clear();
    inst->callTimeoutMs = -1;
    inst->callBudget = 0;
    inst->callStatus = RunStatus::Failed;
    inst->callArmed = true;
    return NeoScriptInternal::inv(inst);
}
Invocation RuntimeImpl::Call(InstanceHandle h, StringView functionName)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return Invocation();
    int iFID = inst->worker->FindFunction(functionName.str());
    if (iFID < 0) return Invocation();
    return Call(h, FunctionHandle{ static_cast<uint32_t>(iFID) });
}

RunStatus RuntimeImpl::Resume(InstanceHandle h, const ResumeDesc&)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return RunStatus::Failed;
    return mapExecStatus(inst->worker->ResumeTop());
}
void RuntimeImpl::Cancel(InstanceHandle) {} // TODO(build): 취소 경로(내부 API 확인 필요)

bool RuntimeImpl::StartSliced(InstanceHandle h, StringView functionName, int32_t timeoutMs, uint32_t budget)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    FlushPendingCall(inst);
    u32 id = inst->worker->GetWorkerID();
    if (timeoutMs >= 0 || budget > 0)
        m_vm->SetTimeout(id, timeoutMs, budget > 0 ? static_cast<int>(budget) : NEO_DEFAULT_CHECKOP);
    return m_vm->BindWorkerFunction(id, functionName.str());
}
RunStatus RuntimeImpl::UpdateSliced(InstanceHandle h)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return RunStatus::Failed;
    u32 id = inst->worker->GetWorkerID();
    m_vm->UpdateWorker(id);                       // 한 슬라이스 실행
    return m_vm->IsWorking(id) ? RunStatus::Suspended : RunStatus::Completed;
}
bool RuntimeImpl::IsRunning(InstanceHandle h) const
{
    InstanceRec* inst = resolveInstance(h);
    return inst ? m_vm->IsWorking(inst->worker->GetWorkerID()) : false;
}

bool RuntimeImpl::GetGlobalInt(InstanceHandle h, StringView name, int32_t& out) const
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    VarInfo* v = inst->worker->GetVar(name.str());
    if (!v || v->GetType() == VAR_NONE) return false;
    out = inst->worker->PopInt(v); return true;
}
bool RuntimeImpl::GetGlobalFloat(InstanceHandle h, StringView name, float& out) const
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    VarInfo* v = inst->worker->GetVar(name.str());
    if (!v || v->GetType() == VAR_NONE) return false;
    out = static_cast<float>(inst->worker->PopFloat(v)); return true;
}
bool RuntimeImpl::GetGlobalString(InstanceHandle h, StringView name, StringView& out) const
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    VarInfo* v = inst->worker->GetVar(name.str());
    if (!v || v->GetType() != VAR_STRING) return false;
    const char* s = inst->worker->PopString(v); out = StringView(s ? s : ""); return true;
}
bool RuntimeImpl::SetGlobalInt(InstanceHandle h, StringView name, int32_t vv)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    VarInfo* v = inst->worker->GetVar(name.str());
    if (!v) return false;
    inst->worker->Var_SetInt(v, vv); return true;
}
bool RuntimeImpl::SetGlobalFloat(InstanceHandle h, StringView name, float vv)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    VarInfo* v = inst->worker->GetVar(name.str());
    if (!v) return false;
    inst->worker->Var_SetFloat(v, vv); return true;
}
bool RuntimeImpl::SetGlobalString(InstanceHandle h, StringView name, StringView vv)
{
    InstanceRec* inst = resolveInstance(h);
    if (!inst) return false;
    VarInfo* v = inst->worker->GetVar(name.str());
    if (!v) return false;
    SetVarString(inst->worker, v, vv); return true;
}

bool RuntimeImpl::CompleteAsyncInt(AsyncToken, int32_t) { return false; } // TODO(build)
bool RuntimeImpl::FailAsync(AsyncToken, const Error&) { return false; }   // TODO(build)

bool RuntimeImpl::TakeLastError(StringView& out)
{
    if (!m_vm || !m_vm->IsLastErrorMsg()) return false;
    const char* s = m_vm->GetLastErrorMsg();
    m_lastError = s ? s : "";
    m_vm->ClearLastErrorMsg();
    out = StringView(m_lastError);
    return true;
}
bool RuntimeImpl::PeekLastError(StringView& out) const
{
    if (!m_vm || !m_vm->IsLastErrorMsg()) return false;
    const char* s = m_vm->GetLastErrorMsg();
    out = StringView(s ? s : "");   // VM 소유 문자열 뷰(Clear 전까지 유효)
    return true;
}

void RuntimeImpl::GetBuiltins(std::vector<BuiltinInfo>& out) const
{
    std::vector<NeoBuiltinInfo> src;
    INeoVM::GetBuiltins(src);
    out.clear(); out.reserve(src.size());
    for (const NeoBuiltinInfo& b : src)
    {
        BuiltinInfo o;
        o.module = b.module; o.name = b.name; o.argCount = b.argCount; o.ret = b.ret; o.params = b.params;
        out.push_back(std::move(o));
    }
}

//==============================================================================
// 팩토리
//==============================================================================
IRuntime* CreateRuntime(const RuntimeDesc& desc) { return new RuntimeImpl(desc); }
void      DestroyRuntime(IRuntime* runtime) { delete runtime; }

// 전역 로그 싱크 — 내부 INeoVM::m_pFunPrint/m_pFunError(IO_Print=void(*)(const char*)) 를 직접 설정.
// 콜백 시그니처가 동일해 트램폴린 없이 그대로 연결(널 종료 보장). RuntimeDesc.printFn 과 같은 전역을
// 공유하므로 last-writer-wins(호스트 전역 로깅과 런타임별 print 를 섞지 말 것).
void SetLogHandler(void (*print)(const char*), void (*error)(const char*))
{
    INeoVM::m_pFunPrint = print;
    INeoVM::m_pFunError = error;
}

void GetAllocStats(AllocStats& out)
{
    SNeoVMAllocStats s;
    GetNeoVMAllocStats(s);   // 내부 전역 통계(NeoVM.h)
    out.strings = s.strings; out.maps = s.maps; out.lists = s.lists; out.sets = s.sets;
    out.coroutines = s.coroutines; out.modules = s.modules; out.asyncs = s.asyncs;
}

//==============================================================================
// CallContext (타입드, zero-copy). 프로퍼티 접근 시 arg0/ret 를 propertyVar 로 라우팅.
//==============================================================================
static CallContextImpl* Ctx(void* p) { return static_cast<CallContextImpl*>(p); }
static VarInfo* CtxArgVar(CallContextImpl* c, std::size_t i)
{
    if (c->propertyVar && c->propertyIsArg && i == 0) return c->propertyVar;
    return c->worker->GetStackVar(static_cast<int>(i) + 1); // 인자 1-based
}
static VarInfo* CtxRetVar(CallContextImpl* c)
{
    if (c->propertyVar && !c->propertyIsArg) return c->propertyVar;
    return c->worker->GetReturnVar();
}
static void ReadVec(VarInfo* v, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    if (!v) return;
    NS_FLOAT a, b, c, d;
    switch (v->GetType())
    {
    case VAR_VEC2: if (v->GetVec2(a, b))       { out[0]=(float)a; out[1]=(float)b; } break;
    case VAR_VEC3: if (v->GetVec3(a, b, c))    { out[0]=(float)a; out[1]=(float)b; out[2]=(float)c; } break;
    case VAR_VEC4: if (v->GetVec4(a, b, c, d)) { out[0]=(float)a; out[1]=(float)b; out[2]=(float)c; out[3]=(float)d; } break;
    case VAR_QUAT: if (v->GetQuat(a, b, c, d)) { out[0]=(float)a; out[1]=(float)b; out[2]=(float)c; out[3]=(float)d; } break;
    default: break;
    }
}

void* CallContext::userData() const { return Ctx(m_impl)->bindingUserData; }
void* CallContext::instanceUserData() const { return Ctx(m_impl)->instanceUserData; }
IRuntime* CallContext::runtime() const { return Ctx(m_impl)->runtime; }
InstanceHandle CallContext::instance() const { return Ctx(m_impl)->instance; }
// (raw INeoVMWorker*/VarInfo* 접근자는 공개 API 에 없다 — 전부 shim 내부.)
std::size_t CallContext::argCount() const { return static_cast<std::size_t>(Ctx(m_impl)->argCount); }
ValueType CallContext::argType(std::size_t i) const { CallContextImpl* c=Ctx(m_impl); VarInfo* v=CtxArgVar(c,i); return v?VarTypeToValueType(v->GetType()):ValueType::None; }
int32_t CallContext::argInt(std::size_t i) const { CallContextImpl* c=Ctx(m_impl); return c->worker->PopInt(CtxArgVar(c,i)); }
float CallContext::argFloat(std::size_t i) const { CallContextImpl* c=Ctx(m_impl); return static_cast<float>(c->worker->PopFloat(CtxArgVar(c,i))); }
bool CallContext::argBool(std::size_t i) const { CallContextImpl* c=Ctx(m_impl); return c->worker->PopBool(CtxArgVar(c,i)); }
StringView CallContext::argString(std::size_t i) const { CallContextImpl* c=Ctx(m_impl); const char* s=c->worker->PopString(CtxArgVar(c,i)); return StringView(s?s:""); }
void CallContext::argVec(std::size_t i, float out[4]) const { CallContextImpl* c=Ctx(m_impl); ReadVec(CtxArgVar(c,i), out); }
FunctionHandle CallContext::argFunction(std::size_t i) const
{
    // [콜백] 스크립트 함수 인자 → FunctionHandle{_fun_index}. FunctionHandle.index 는 IRuntime::Call 의 iFID 와 동일 공간.
    // 캡처된 함수는 이 인스턴스의 Program 소속 → program 식별자를 채워 Call 교차검증 대상이 되게 한다.
    CallContextImpl* c=Ctx(m_impl); VarInfo* v=CtxArgVar(c,i);
    if (!v || v->GetType() != VAR_FUN) return FunctionHandle{};
    FunctionHandle fh; fh.index = static_cast<uint32_t>(v->_fun_index);
    if (c->runtime)
        if (InstanceRec* inst = static_cast<RuntimeImpl*>(c->runtime)->resolveInstance(c->instance))
        { fh.programId = inst->program.id; fh.programGeneration = inst->program.generation; }
    return fh;
}
bool CallContext::argAsMap(std::size_t i, MapReader& out) const
{
    CallContextImpl* c=Ctx(m_impl); VarInfo* v=CtxArgVar(c,i);
    if (!v || v->GetType()!=VAR_MAP || v->_tbl==nullptr) return false;
    c->mapR.push_back(MapReaderImpl{ c->worker, v->_tbl, &c->mapR, &c->listR });
    out = NeoScriptInternal::mapR(&c->mapR.back()); return true;
}
bool CallContext::argAsList(std::size_t i, ListReader& out) const
{
    CallContextImpl* c=Ctx(m_impl); VarInfo* v=CtxArgVar(c,i);
    if (!v || v->GetType()!=VAR_LIST || v->_lst==nullptr) return false;
    c->listR.push_back(ListReaderImpl{ c->worker, v->_lst, &c->mapR, &c->listR });
    out = NeoScriptInternal::listR(&c->listR.back()); return true;
}
void* CallContext::argObjectUserData(std::size_t i) const
{
    CallContextImpl* c=Ctx(m_impl); VarInfo* v=CtxArgVar(c,i);
    if (!v || v->GetType()!=VAR_MAP || v->_tbl==nullptr) return nullptr;
    void* ud = v->_tbl->_pUserData;
    // v2 로 바인딩된 객체는 _pUserData 가 ObjectBinding* → .userData 를 푼다.
    // old(neo_globalinterface2) 바인딩은 _pUserData 가 userData(eventID) 그대로.
    if (v->_tbl->_fun._func == &MethodTrampoline && ud != nullptr)
        return static_cast<ObjectBinding*>(ud)->userData;
    return ud;
}
void CallContext::retInt(int32_t v) { CallContextImpl* c=Ctx(m_impl); c->worker->Var_SetInt(CtxRetVar(c), v); }
void CallContext::retFloat(float v) { CallContextImpl* c=Ctx(m_impl); c->worker->Var_SetFloat(CtxRetVar(c), v); }
void CallContext::retBool(bool v) { CallContextImpl* c=Ctx(m_impl); c->worker->Var_SetBool(CtxRetVar(c), v); }
void CallContext::retString(StringView v) { CallContextImpl* c=Ctx(m_impl); SetVarString(c->worker, CtxRetVar(c), v); }
void CallContext::retVec2(float x, float y) { CallContextImpl* c=Ctx(m_impl); c->worker->Var_SetVec2(CtxRetVar(c), x, y); }
void CallContext::retInstanceGlobal(InstanceHandle instance, StringView name)
{
    CallContextImpl* c=Ctx(m_impl);
    INeoVMWorker* src = c->runtime ? static_cast<RuntimeImpl*>(c->runtime)->instanceWorker(instance) : nullptr;
    VarInfo* v = src ? src->GetVar(std::string(name.data(), name.size())) : nullptr;
    if (v) c->worker->ReturnValue(v);          // Var_Move → 반환 슬롯(구 ReturnValue(GetVar) 재현)
    else   c->worker->Var_Release(CtxRetVar(c)); // None
}
void CallContext::retVec3(float x, float y, float z) { CallContextImpl* c=Ctx(m_impl); c->worker->Var_SetVec3(CtxRetVar(c), x, y, z); }
MapBuilder CallContext::retMap() { CallContextImpl* c=Ctx(m_impl); VarInfo* r=CtxRetVar(c); c->worker->ResetVarType(r, VAR_MAP); c->mapB.push_back(MapBuilderImpl{ c->worker, r->_tbl, c }); return NeoScriptInternal::mapB(&c->mapB.back()); }
ListBuilder CallContext::retList() { CallContextImpl* c=Ctx(m_impl); VarInfo* r=CtxRetVar(c); c->worker->ResetVarType(r, VAR_LIST); c->listB.push_back(ListBuilderImpl{ c->worker, r->_lst, c }); return NeoScriptInternal::listB(&c->listB.back()); }
void CallContext::retObject(ObjectType type, void* userData) {
    CallContextImpl* c=Ctx(m_impl);
    const RegisteredObject* ro=static_cast<const RegisteredObject*>(type.m_impl);
    VarInfo* r=CtxRetVar(c); if(!ro||!r) return;
    c->worker->ResetVarType(r, VAR_MAP);
    RuntimeImpl* rt=static_cast<RuntimeImpl*>(c->runtime);
    ObjectBinding* b=rt->AcquireNestedBinding(c->instance, ro, userData); if(!b) return;
    INeoVM::RegisterTableCallBack(r, b, (ro->method)?&MethodTrampoline:nullptr, (ro->property)?&PropertyTrampoline:nullptr);
}
void CallContext::retNull() { CallContextImpl* c=Ctx(m_impl); c->worker->ResetVarType(CtxRetVar(c), VAR_NONE); }
AsyncToken CallContext::beginAsync() { return 0; }                        // TODO(build)
void CallContext::fail(int32_t code, StringView message) { (void)code; (void)message; } // TODO(build)

//==============================================================================
// MapBuilder / ListBuilder (타입드) — VM 소유 컬렉션 조립
//==============================================================================
void MapBuilder::setInt(StringView key, int32_t v)    { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetInt(&it, v);   b->map->Insert(std::string(key.data(),key.size()), &it); }
void MapBuilder::setFloat(StringView key, float v)    { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetFloat(&it, v); b->map->Insert(std::string(key.data(),key.size()), &it); }
void MapBuilder::setBool(StringView key, bool v)      { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetBool(&it, v);  b->map->Insert(std::string(key.data(),key.size()), &it); }
void MapBuilder::setString(StringView key, StringView v) { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo it; SetVarString(b->w, &it, v); b->map->Insert(std::string(key.data(),key.size()), &it); }
void MapBuilder::setVec3(StringView key, float x, float y, float z) { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetVec3(&it, x, y, z); b->map->Insert(std::string(key.data(),key.size()), &it); }
MapBuilder MapBuilder::setMap(StringView key) { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo k; b->w->Var_SetStringA(&k, std::string(key.data(),key.size())); VarInfo* slot=b->map->Insert(&k); b->w->ResetVarType(slot, VAR_MAP); b->ctx->mapB.push_back(MapBuilderImpl{ b->w, slot->_tbl, b->ctx }); return NeoScriptInternal::mapB(&b->ctx->mapB.back()); }
ListBuilder MapBuilder::setList(StringView key) { MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl); VarInfo k; b->w->Var_SetStringA(&k, std::string(key.data(),key.size())); VarInfo* slot=b->map->Insert(&k); b->w->ResetVarType(slot, VAR_LIST); b->ctx->listB.push_back(ListBuilderImpl{ b->w, slot->_lst, b->ctx }); return NeoScriptInternal::listB(&b->ctx->listB.back()); }
void MapBuilder::setObject(StringView key, ObjectType type, void* userData) {
    MapBuilderImpl* b=static_cast<MapBuilderImpl*>(m_impl);
    const RegisteredObject* ro=static_cast<const RegisteredObject*>(type.m_impl); if(!ro) return;
    VarInfo k; b->w->Var_SetStringA(&k, std::string(key.data(),key.size()));
    VarInfo* slot=b->map->Insert(&k); b->w->ResetVarType(slot, VAR_MAP);
    RuntimeImpl* rt=static_cast<RuntimeImpl*>(b->ctx->runtime);
    ObjectBinding* bind=rt->AcquireNestedBinding(b->ctx->instance, ro, userData); if(!bind) return;
    INeoVM::RegisterTableCallBack(slot, bind, (ro->method)?&MethodTrampoline:nullptr, (ro->property)?&PropertyTrampoline:nullptr);
}

void ListBuilder::reserve(int count) { static_cast<ListBuilderImpl*>(m_impl)->list->Reserve(count); }
void ListBuilder::resize(int count)  { static_cast<ListBuilderImpl*>(m_impl)->list->Resize(count); }
int  ListBuilder::count() const      { return static_cast<ListBuilderImpl*>(m_impl)->list->GetCount(); }
void ListBuilder::pushInt(int32_t v)    { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetInt(&it, v);   b->list->InsertLast(&it); }
void ListBuilder::pushFloat(float v)    { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetFloat(&it, v); b->list->InsertLast(&it); }
void ListBuilder::pushBool(bool v)      { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetBool(&it, v);  b->list->InsertLast(&it); }
void ListBuilder::pushString(StringView v) { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; SetVarString(b->w, &it, v); b->list->InsertLast(&it); }
void ListBuilder::pushVec3(float x, float y, float z) { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetVec3(&it, x, y, z); b->list->InsertLast(&it); }
void ListBuilder::setInt(int index, int32_t v)    { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetInt(&it, v);   b->list->SetValue(index, &it); }
void ListBuilder::setFloat(int index, float v)    { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; b->w->Var_SetFloat(&it, v); b->list->SetValue(index, &it); }
void ListBuilder::setString(int index, StringView v) { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); VarInfo it; SetVarString(b->w, &it, v); b->list->SetValue(index, &it); }
MapBuilder ListBuilder::pushMap()  { ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); int idx=b->list->GetCount(); b->list->Resize(idx+1); VarInfo* slot=b->list->GetValue(idx); b->w->ResetVarType(slot, VAR_MAP); b->ctx->mapB.push_back(MapBuilderImpl{ b->w, slot->_tbl, b->ctx }); return NeoScriptInternal::mapB(&b->ctx->mapB.back()); }
ListBuilder ListBuilder::pushList(){ ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl); int idx=b->list->GetCount(); b->list->Resize(idx+1); VarInfo* slot=b->list->GetValue(idx); b->w->ResetVarType(slot, VAR_LIST); b->ctx->listB.push_back(ListBuilderImpl{ b->w, slot->_lst, b->ctx }); return NeoScriptInternal::listB(&b->ctx->listB.back()); }
void ListBuilder::pushObject(ObjectType type, void* userData) {
    ListBuilderImpl* b=static_cast<ListBuilderImpl*>(m_impl);
    const RegisteredObject* ro=static_cast<const RegisteredObject*>(type.m_impl); if(!ro) return;
    int idx=b->list->GetCount(); b->list->Resize(idx+1);
    VarInfo* slot=b->list->GetValue(idx); b->w->ResetVarType(slot, VAR_MAP);
    RuntimeImpl* rt=static_cast<RuntimeImpl*>(b->ctx->runtime);
    ObjectBinding* bind=rt->AcquireNestedBinding(b->ctx->instance, ro, userData); if(!bind) return;
    INeoVM::RegisterTableCallBack(slot, bind, (ro->method)?&MethodTrampoline:nullptr, (ro->property)?&PropertyTrampoline:nullptr);
}

//==============================================================================
// MapReader / ListReader (타입드) — VM 소유 컬렉션 열람
//==============================================================================
static VarInfo* MapFind(const MapReaderImpl* r, StringView key) { return r->map->Find(std::string(key.data(), key.size())); }

bool MapReader::has(StringView key) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); return MapFind(r,key)!=nullptr; }
ValueType MapReader::type(StringView key) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); return v?VarTypeToValueType(v->GetType()):ValueType::None; }
bool MapReader::getInt(StringView key, int32_t& out) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v) return false; out=r->w->PopInt(v); return true; }
bool MapReader::getFloat(StringView key, float& out) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v) return false; out=static_cast<float>(r->w->PopFloat(v)); return true; }
bool MapReader::getBool(StringView key, bool& out) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v) return false; out=r->w->PopBool(v); return true; }
bool MapReader::getString(StringView key, StringView& out) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v||v->GetType()!=VAR_STRING) return false; const char* s=r->w->PopString(v); out=StringView(s?s:""); return true; }
bool MapReader::getVec(StringView key, float out[4]) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v) return false; ReadVec(v,out); return v->GetType()>=VAR_VEC2 && v->GetType()<=VAR_QUAT; }
bool MapReader::getMap(StringView key, MapReader& out) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v||v->GetType()!=VAR_MAP||!v->_tbl) return false; r->mpool->push_back(MapReaderImpl{r->w,v->_tbl,r->mpool,r->lpool}); out=NeoScriptInternal::mapR(&r->mpool->back()); return true; }
bool MapReader::getList(StringView key, ListReader& out) const { const MapReaderImpl* r=static_cast<const MapReaderImpl*>(m_impl); VarInfo* v=MapFind(r,key); if(!v||v->GetType()!=VAR_LIST||!v->_lst) return false; r->lpool->push_back(ListReaderImpl{r->w,v->_lst,r->mpool,r->lpool}); out=NeoScriptInternal::listR(&r->lpool->back()); return true; }

int ListReader::count() const { return static_cast<const ListReaderImpl*>(m_impl)->list->GetCount(); }
ValueType ListReader::type(int index) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); return v?VarTypeToValueType(v->GetType()):ValueType::None; }
bool ListReader::getInt(int index, int32_t& out) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v) return false; out=r->w->PopInt(v); return true; }
bool ListReader::getFloat(int index, float& out) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v) return false; out=static_cast<float>(r->w->PopFloat(v)); return true; }
bool ListReader::getBool(int index, bool& out) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v) return false; out=r->w->PopBool(v); return true; }
bool ListReader::getString(int index, StringView& out) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v||v->GetType()!=VAR_STRING) return false; const char* s=r->w->PopString(v); out=StringView(s?s:""); return true; }
bool ListReader::getVec(int index, float out[4]) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v) return false; ReadVec(v,out); return v->GetType()>=VAR_VEC2 && v->GetType()<=VAR_QUAT; }
bool ListReader::getMap(int index, MapReader& out) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v||v->GetType()!=VAR_MAP||!v->_tbl) return false; r->mpool->push_back(MapReaderImpl{r->w,v->_tbl,r->mpool,r->lpool}); out=NeoScriptInternal::mapR(&r->mpool->back()); return true; }
bool ListReader::getList(int index, ListReader& out) const { const ListReaderImpl* r=static_cast<const ListReaderImpl*>(m_impl); VarInfo* v=r->list->GetValue(index); if(!v||v->GetType()!=VAR_LIST||!v->_lst) return false; r->lpool->push_back(ListReaderImpl{r->w,v->_lst,r->mpool,r->lpool}); out=NeoScriptInternal::listR(&r->lpool->back()); return true; }

//==============================================================================
// Invocation (호스트→스크립트 호출 핸들) — 인자 VM 슬롯 직접, 반환 in-place zero-copy
//==============================================================================
static InstanceRec* Inv(void* p) { return static_cast<InstanceRec*>(p); }
static void InvFinish(void* p) { if (!p) return; InstanceRec* i=Inv(p); i->callArmed = false; if (i->runtime) i->runtime->FlushPendingCall(i); }

Invocation& Invocation::operator=(Invocation&& o) noexcept { if (this != &o) { InvFinish(m_impl); m_impl = o.m_impl; o.m_impl = nullptr; } return *this; }
Invocation::~Invocation() { InvFinish(m_impl); }

Invocation& Invocation::argInt(int32_t v)    { if (m_impl){ InstanceRec* i=Inv(m_impl); i->callArgs.emplace_back(); i->worker->Var_SetInt(&i->callArgs.back(), v); } return *this; }
Invocation& Invocation::argFloat(float v)    { if (m_impl){ InstanceRec* i=Inv(m_impl); i->callArgs.emplace_back(); i->worker->Var_SetFloat(&i->callArgs.back(), v); } return *this; }
Invocation& Invocation::argBool(bool v)      { if (m_impl){ InstanceRec* i=Inv(m_impl); i->callArgs.emplace_back(); i->worker->Var_SetBool(&i->callArgs.back(), v); } return *this; }
Invocation& Invocation::argString(StringView v) { if (m_impl){ InstanceRec* i=Inv(m_impl); i->callArgs.emplace_back(); SetVarString(i->worker, &i->callArgs.back(), v); } return *this; }
Invocation& Invocation::argVec3(float x, float y, float z) { if (m_impl){ InstanceRec* i=Inv(m_impl); i->callArgs.emplace_back(); i->worker->Var_SetVec3(&i->callArgs.back(), x, y, z); } return *this; }
Invocation& Invocation::argObject(ObjectType type, void* userData)
{
    // 바인딩된 네이티브 객체를 인자 슬롯(callArgs)에 VAR_MAP 으로 바인딩(구 neo_globalinterface2 arg 대체).
    if (m_impl)
    {
        InstanceRec* i = Inv(m_impl);
        const RegisteredObject* ro = static_cast<const RegisteredObject*>(type.m_impl);
        i->callArgs.emplace_back();
        VarInfo* pVar = &i->callArgs.back();
        if (ro && i->runtime && i->worker->ResetVarType(pVar, VAR_MAP))
        {
            ObjectBinding* bind = i->runtime->AcquireNestedBinding(i->handle, ro, userData);
            if (bind)
                INeoVM::RegisterTableCallBack(pVar, bind,
                    (ro->method)   ? &MethodTrampoline   : nullptr,
                    (ro->property) ? &PropertyTrampoline : nullptr);
        }
    }
    return *this;
}
Invocation& Invocation::timeout(int32_t ms)  { if (m_impl) Inv(m_impl)->callTimeoutMs = ms;  return *this; }
Invocation& Invocation::budget(uint32_t ops) { if (m_impl) Inv(m_impl)->callBudget = ops;     return *this; }

RunStatus Invocation::invoke()
{
    if (!m_impl) return RunStatus::Failed;
    InstanceRec* inst = Inv(m_impl);
    INeoVMWorker* w = inst->worker;
    inst->callArmed = false;               // 소비 시작 → 스크립트 실행 중 중첩 Call 허용
    inst->runtime->FlushPendingCall(inst); // 이전 borrowed 컨텍스트 반납
    NeoHostCallBegin begin = w->BeginHostCall();
    if (begin != NeoHostCallBegin::Acquired && begin != NeoHostCallBegin::Nested)
    {
        w->ReleaseArgs(inst->callArgs); inst->callArgs.clear();
        inst->callStatus = RunStatus::Failed; return RunStatus::Failed;
    }
    const bool nested = (begin == NeoHostCallBegin::Nested);
    if (nested) w->BeginNestedScriptCall();
    if (inst->callTimeoutMs >= 0 || inst->callBudget > 0)
        w->SetTimeout(inst->callTimeoutMs, inst->callBudget > 0 ? static_cast<int>(inst->callBudget) : NEO_DEFAULT_CHECKOP);
    if (w->RunFunction(inst->callFID, inst->callArgs)) { w->GC(); inst->callStatus = RunStatus::Completed; }
    else inst->callStatus = RunStatus::Failed;
    w->ReleaseArgs(inst->callArgs);
    inst->callArgs.clear();
    // 반환 읽기(retX)를 위해 컨텍스트 유지 — dtor/다음 Call 의 FlushPendingCall 에서 반납.
    inst->hostCallPending = true; inst->pendingBegin = begin; inst->pendingNested = nested;
    return inst->callStatus;
}
RunStatus Invocation::status() const { return m_impl ? Inv(m_impl)->callStatus : RunStatus::Failed; }
const Error& Invocation::error() const { static const Error e; return e; } // TODO(build): VM 에러 채우기

ValueType Invocation::retType() const { if(!m_impl) return ValueType::None; VarInfo* rv=Inv(m_impl)->worker->GetReturnVar(); return rv?VarTypeToValueType(rv->GetType()):ValueType::None; }
int32_t Invocation::retInt() const { if(!m_impl) return 0; InstanceRec* i=Inv(m_impl); return i->worker->PopInt(i->worker->GetReturnVar()); }
float Invocation::retFloat() const { if(!m_impl) return 0.0f; InstanceRec* i=Inv(m_impl); return static_cast<float>(i->worker->PopFloat(i->worker->GetReturnVar())); }
bool Invocation::retBool() const { if(!m_impl) return false; InstanceRec* i=Inv(m_impl); return i->worker->PopBool(i->worker->GetReturnVar()); }
StringView Invocation::retString() const { if(!m_impl) return StringView(); InstanceRec* i=Inv(m_impl); const char* s=i->worker->PopString(i->worker->GetReturnVar()); return StringView(s?s:""); }
void Invocation::retVec(float out[4]) const { out[0]=out[1]=out[2]=out[3]=0.0f; if(!m_impl) return; ReadVec(Inv(m_impl)->worker->GetReturnVar(), out); }
bool Invocation::retMap(MapReader& out) const
{
    if(!m_impl) return false; InstanceRec* i=Inv(m_impl); VarInfo* rv=i->worker->GetReturnVar();
    if(!rv||rv->GetType()!=VAR_MAP||!rv->_tbl) return false;
    i->readMapPool.clear(); i->readListPool.clear();
    i->readMapPool.push_back(MapReaderImpl{ i->worker, rv->_tbl, &i->readMapPool, &i->readListPool });
    out = NeoScriptInternal::mapR(&i->readMapPool.back()); return true;
}
bool Invocation::retList(ListReader& out) const
{
    if(!m_impl) return false; InstanceRec* i=Inv(m_impl); VarInfo* rv=i->worker->GetReturnVar();
    if(!rv||rv->GetType()!=VAR_LIST||!rv->_lst) return false;
    i->readMapPool.clear(); i->readListPool.clear();
    i->readListPool.push_back(ListReaderImpl{ i->worker, rv->_lst, &i->readMapPool, &i->readListPool });
    out = NeoScriptInternal::listR(&i->readListPool.back()); return true;
}

//==============================================================================
// 트램폴린 — 공개 NativeMethod/NativeProperty ↔ 내부 Neo_NativeFunction/Property
//==============================================================================
static bool MethodTrampoline(INeoVMWorker* N, void* pUserData, const VMString* pStr, short args)
{
    ObjectBinding* b = static_cast<ObjectBinding*>(pUserData);
    if (!b || !b->obj || !b->obj->method) return false;
    CallContextImpl impl;
    impl.worker = N;
    impl.runtime = static_cast<IRuntime*>(b->runtime);
    impl.instance = b->handle;
    impl.bindingUserData = b->userData;
    impl.instanceUserData = b->inst ? b->inst->instanceUserData : nullptr;
    impl.argCount = args;
    CallContext ctx = NeoScriptInternal::ctx(&impl);
    return b->obj->method(ctx, VmName(pStr));
}
static bool PropertyTrampoline(INeoVMWorker* N, void* pUserData, const VMString* pStr, VarInfo* p, bool get)
{
    ObjectBinding* b = static_cast<ObjectBinding*>(pUserData);
    if (!b || !b->obj || !b->obj->property) return false;
    CallContextImpl impl;
    impl.worker = N;
    impl.runtime = static_cast<IRuntime*>(b->runtime);
    impl.instance = b->handle;
    impl.bindingUserData = b->userData;
    impl.instanceUserData = b->inst ? b->inst->instanceUserData : nullptr;
    impl.propertyVar = p;          // get: retX 가 p 에 씀 / set: argX(0) 가 p 를 읽음
    impl.propertyIsArg = !get;
    impl.argCount = get ? 0 : 1;
    CallContext ctx = NeoScriptInternal::ctx(&impl);
    return b->obj->property(ctx, VmName(pStr), get);
}

//==============================================================================
// IDebugger shim — 워커의 Debug* 메서드 래핑. 내부 리스너를 워커에 설치해
// OnNeoDebugStopped → 공개 IDebugListener::OnStopped 로 변환.
//==============================================================================
static DebugStopReason ToPublicReason(NeoDebugStopReason r)
{
    switch (r)
    {
    case NEO_DEBUG_STOP_BREAKPOINT: return DebugStopReason::Breakpoint;
    case NEO_DEBUG_STOP_STEP:       return DebugStopReason::Step;
    case NEO_DEBUG_STOP_PAUSE:      return DebugStopReason::Pause;
    case NEO_DEBUG_STOP_EXCEPTION:  return DebugStopReason::Exception;
    default:                        return DebugStopReason::Pause;
    }
}
static DebugLocation ToPublicLoc(const NeoDebugLocation& l)
{
    DebugLocation o; o.opIndex = l.opIndex; o.file = l.file; o.line = l.line; o.callDepth = l.callDepth; return o;
}
static void ToPublicVars(const std::vector<NeoDebugVariable>& src, std::vector<DebugVariable>& dst)
{
    dst.clear(); dst.reserve(src.size());
    for (const NeoDebugVariable& v : src)
    {
        DebugVariable o; o.name = v.name; o.type = v.type; o.value = v.value;
        ToPublicVars(v.children, o.children);
        dst.push_back(std::move(o));
    }
}

class DebuggerImpl final : public IDebugger
{
public:
    explicit DebuggerImpl(RuntimeImpl* rt) : m_rt(rt) { m_adapter.owner = this; }

    void SetListener(IDebugListener* listener) override
    {
        m_user = listener;
        for (auto& kv : m_rt->m_workerHandle)               // 현재 모든 워커에 어댑터 설치/해제
            kv.first->DebugSetListener(listener ? &m_adapter : nullptr);
    }
    void SetBreakpoints(InstanceHandle inst, Span<const DebugBreakpoint> bps) override
    {
        INeoVMWorker* w = worker(inst); if (!w) return;
        if (m_user) w->DebugSetListener(&m_adapter);        // 이 워커에 아직 미설치면
        std::vector<NeoDebugBreakpoint> v; v.reserve(bps.size());
        for (std::size_t i = 0; i < bps.size(); ++i) { NeoDebugBreakpoint n; n.file = bps[i].file; n.line = bps[i].line; v.push_back(n); }
        w->DebugSetBreakpoints(v);
    }
    void Continue(InstanceHandle i) override { if (INeoVMWorker* w = worker(i)) w->DebugContinue(); }
    void StepInto(InstanceHandle i) override { if (INeoVMWorker* w = worker(i)) w->DebugStepInto(); }
    void StepOver(InstanceHandle i) override { if (INeoVMWorker* w = worker(i)) w->DebugStepOver(); }
    void StepOut(InstanceHandle i) override  { if (INeoVMWorker* w = worker(i)) w->DebugStepOut(); }
    void Pause(InstanceHandle i) override    { if (INeoVMWorker* w = worker(i)) w->DebugPause(); }
    bool IsPaused(InstanceHandle i) override { INeoVMWorker* w = worker(i); return w ? w->DebugIsPaused() : false; }
    RunStatus Run(InstanceHandle i) override
    {
        INeoVMWorker* w = worker(i); if (!w) return RunStatus::Failed;
        bool ok = w->Run();
        if (w->DebugIsPaused()) return RunStatus::Suspended;
        return ok ? RunStatus::Completed : RunStatus::Failed;
    }
    DebugLocation GetLocation(InstanceHandle i) override { INeoVMWorker* w = worker(i); return w ? ToPublicLoc(w->DebugGetLocation()) : DebugLocation(); }
    void GetStackTrace(InstanceHandle i, std::vector<DebugStackFrame>& out) override
    {
        out.clear(); INeoVMWorker* w = worker(i); if (!w) return;
        std::vector<NeoDebugStackFrame> src; w->DebugGetStackTrace(src);
        out.reserve(src.size());
        for (const NeoDebugStackFrame& f : src)
        {
            DebugStackFrame o; o.frameId = f.frameId; o.functionId = f.functionId; o.functionName = f.functionName;
            o.file = f.file; o.line = f.line; o.opIndex = f.opIndex; out.push_back(std::move(o));
        }
    }
    void GetFrameVariables(InstanceHandle i, int frameId, std::vector<DebugVariable>& out) override
    {
        out.clear(); INeoVMWorker* w = worker(i); if (!w) return;
        std::vector<NeoDebugVariable> src; w->DebugGetFrameVariables(frameId, src);
        ToPublicVars(src, out);
    }
    void GetExecutableLines(ProgramHandle p, std::vector<int>& out) override
    {
        out.clear(); INeoVMWorker* w = anyWorkerOfProgram(p); if (!w) return;
        w->DebugGetExecutableLines(out);
    }
    void GetExecutableLocations(ProgramHandle p, std::vector<DebugLocation>& out) override
    {
        out.clear(); INeoVMWorker* w = anyWorkerOfProgram(p); if (!w) return;
        std::vector<NeoDebugLocation> src; w->DebugGetExecutableLocations(src);
        out.reserve(src.size());
        for (const NeoDebugLocation& l : src) out.push_back(ToPublicLoc(l));
    }

    void OnStop(INeoVMWorker* w, const NeoDebugLocation& loc, NeoDebugStopReason reason)
    {
        if (!m_user) return;
        auto it = m_rt->m_workerHandle.find(w);
        InstanceHandle h = (it != m_rt->m_workerHandle.end()) ? it->second : InstanceHandle();
        m_user->OnStopped(h, ToPublicLoc(loc), ToPublicReason(reason));
    }

private:
    struct Adapter : INeoVMDebugListener
    {
        DebuggerImpl* owner = nullptr;
        void OnNeoDebugStopped(INeoVMWorker* worker, const NeoDebugLocation& location, NeoDebugStopReason reason) override
        { if (owner) owner->OnStop(worker, location, reason); }
    };

    INeoVMWorker* worker(InstanceHandle h) { InstanceRec* r = m_rt->resolveInstance(h); return r ? r->worker : nullptr; }
    INeoVMWorker* anyWorkerOfProgram(ProgramHandle p)
    {
        for (auto& kv : m_rt->m_workerHandle)
        {
            InstanceRec* r = m_rt->resolveInstance(kv.second);
            if (r && r->program.id == p.id && r->program.generation == p.generation) return kv.first;
        }
        return nullptr;
    }

    RuntimeImpl*    m_rt = nullptr;
    IDebugListener* m_user = nullptr;
    Adapter         m_adapter;
};

IDebugger* RuntimeImpl::GetDebugger()
{
    if (!m_debugger) m_debugger = new DebuggerImpl(this);
    return m_debugger;
}
static void DestroyDebugger(DebuggerImpl* d) { delete d; }

} // namespace NeoScript
