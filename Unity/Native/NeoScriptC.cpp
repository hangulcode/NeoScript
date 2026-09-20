//==============================================================================
// NeoScriptC.cpp — C ABI 구현. NeoScript.h 의 C++ 파사드를 감싼다.
//
// 여기서 해결해야 하는 임피던스 불일치가 셋 있다.
//
// 1) 디스패처에 컨텍스트가 없다.
//    NativeObjectDesc.method 는 맨 함수 포인터라 "어느 객체 타입이냐"를 실을 곳이 없다.
//    C++ 호스트는 타입마다 함수를 하나씩 쓰면 되지만 C#/Unity 는 그럴 수 없다.
//    → 슬롯마다 고정 썽크를 템플릿으로 찍어 두고(g_methodThunks), 썽크가 슬롯 번호를
//      호스트 디스패처에 넘긴다. 그래서 등록 가능한 타입 수에 상한이 있다.
//
// 2) 빌더/리더는 private 멤버 + friend 접근이라 raw void* 에서 되살릴 수 없다.
//    → 래퍼가 값을 소유한다. 스크래치 아레나에 담고 그 주소를 핸들로 내준다.
//      아레나는 프레임 단위로 되감기므로 문서화된 "빌린 수명"과 정확히 일치한다.
//
// 3) ObjectType 도 같은 이유로 왕복이 안 된다.
//    → 프로세스 전역 테이블에 인터닝하고 1-기반 인덱스를 핸들로 쓴다. 인덱스라서
//      빌더 함수처럼 런타임을 모르는 자리에서도 해소할 수 있다.
//==============================================================================

#include "NeoScriptC.h"
#include "NeoScript.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace NeoScript;

//------------------------------------------------------------------------------
// C 상수와 C++ enum 이 갈라지면 조용히 깨진다. 여기서 못을 박는다.
//------------------------------------------------------------------------------
static_assert((int)RunStatus::Completed == NS_RUN_COMPLETED, "RunStatus drift");
static_assert((int)RunStatus::Suspended == NS_RUN_SUSPENDED, "RunStatus drift");
static_assert((int)RunStatus::Yielded   == NS_RUN_YIELDED,   "RunStatus drift");
static_assert((int)RunStatus::Failed    == NS_RUN_FAILED,    "RunStatus drift");
static_assert((int)RunStatus::Cancelled == NS_RUN_CANCELLED, "RunStatus drift");

static_assert((int)InstanceState::Idle      == NS_STATE_IDLE,      "InstanceState drift");
static_assert((int)InstanceState::Running   == NS_STATE_RUNNING,   "InstanceState drift");
static_assert((int)InstanceState::Suspended == NS_STATE_SUSPENDED, "InstanceState drift");
static_assert((int)InstanceState::Failed    == NS_STATE_FAILED,    "InstanceState drift");

static_assert((int)ValueType::None     == NS_TYPE_NONE,     "ValueType drift");
static_assert((int)ValueType::Bool     == NS_TYPE_BOOL,     "ValueType drift");
static_assert((int)ValueType::Int      == NS_TYPE_INT,      "ValueType drift");
static_assert((int)ValueType::Float    == NS_TYPE_FLOAT,    "ValueType drift");
static_assert((int)ValueType::String   == NS_TYPE_STRING,   "ValueType drift");
static_assert((int)ValueType::Vec2     == NS_TYPE_VEC2,     "ValueType drift");
static_assert((int)ValueType::Vec3     == NS_TYPE_VEC3,     "ValueType drift");
static_assert((int)ValueType::Vec4     == NS_TYPE_VEC4,     "ValueType drift");
static_assert((int)ValueType::Map      == NS_TYPE_MAP,      "ValueType drift");
static_assert((int)ValueType::List     == NS_TYPE_LIST,     "ValueType drift");
static_assert((int)ValueType::Array    == NS_TYPE_ARRAY,    "ValueType drift");
static_assert((int)ValueType::Set      == NS_TYPE_SET,      "ValueType drift");
static_assert((int)ValueType::Function == NS_TYPE_FUNCTION, "ValueType drift");

static_assert((int)ArrayElementType::Bool  == NS_ELEM_BOOL,  "ArrayElementType drift");
static_assert((int)ArrayElementType::Int   == NS_ELEM_INT,   "ArrayElementType drift");
static_assert((int)ArrayElementType::Float == NS_ELEM_FLOAT, "ArrayElementType drift");

static_assert((int)DefineKind::Identifier == NS_DEFINE_IDENTIFIER, "DefineKind drift");
static_assert((int)DefineKind::Int        == NS_DEFINE_INT,        "DefineKind drift");
static_assert((int)DefineKind::Float      == NS_DEFINE_FLOAT,      "DefineKind drift");
static_assert((int)DefineKind::String     == NS_DEFINE_STRING,     "DefineKind drift");
static_assert((int)DefineKind::Bool       == NS_DEFINE_BOOL,       "DefineKind drift");
static_assert((int)DefineKind::Null       == NS_DEFINE_NULL,       "DefineKind drift");

//------------------------------------------------------------------------------
// 잡다한 변환
//------------------------------------------------------------------------------
namespace {

inline StringView SV(const char* p, int32_t len)
{
    if (p == nullptr || len < 0)
        return StringView();
    return StringView(p, (std::size_t)len);
}

inline StringView SV(const NsStr& s) { return SV(s.data, s.len); }

inline NsStr ToNsStr(StringView v)
{
    NsStr s;
    s.data = v.data();
    s.len  = (int32_t)v.size();
    s._pad = 0;
    return s;
}

inline NsStr ToNsStr(const std::string& v)
{
    NsStr s;
    s.data = v.c_str();
    s.len  = (int32_t)v.size();
    s._pad = 0;
    return s;
}

inline uint64_t Pack(uint32_t id, uint32_t generation)
{
    return (uint64_t)id | ((uint64_t)generation << 32);
}

inline ProgramHandle ToProgram(NsProgram v)
{
    ProgramHandle h;
    h.id = (uint32_t)(v & 0xFFFFFFFFu);
    h.generation = (uint32_t)(v >> 32);
    return h;
}

inline InstanceHandle ToInstance(NsInstance v)
{
    InstanceHandle h;
    h.id = (uint32_t)(v & 0xFFFFFFFFu);
    h.generation = (uint32_t)(v >> 32);
    return h;
}

inline DefineSetHandle ToDefineSet(NsDefineSet v)
{
    DefineSetHandle h;
    h.id = (uint32_t)(v & 0xFFFFFFFFu);
    h.generation = (uint32_t)(v >> 32);
    return h;
}

//------------------------------------------------------------------------------
// 스크래치 아레나 — 빌더/리더/뷰 값을 담아 두고 주소를 핸들로 내준다.
// 청크 단위로 잡아서 주소가 절대 움직이지 않는다(vector 재할당이면 내준 핸들이 썩는다).
// 되감기는 카운터만 줄이면 되므로 프레임 pop 이 공짜다.
//------------------------------------------------------------------------------
template <class T>
class Arena
{
public:
    static constexpr std::size_t kChunk = 32;

    T* Alloc()
    {
        const std::size_t chunk = m_count / kChunk;
        const std::size_t index = m_count % kChunk;
        while (m_chunks.size() <= chunk)
            m_chunks.push_back(std::unique_ptr<T[]>(new T[kChunk]));
        ++m_count;
        return &m_chunks[chunk][index];
    }

    std::size_t Mark() const { return m_count; }
    void Rewind(std::size_t mark) { if (mark < m_count) m_count = mark; }

private:
    std::vector<std::unique_ptr<T[]>> m_chunks;
    std::size_t m_count = 0;
};

struct ScratchMark
{
    std::size_t mapBuilder;
    std::size_t listBuilder;
    std::size_t mapReader;
    std::size_t listReader;
    std::size_t arrayView;
};

struct Scratch
{
    Arena<MapBuilder>  mapBuilder;
    Arena<ListBuilder> listBuilder;
    Arena<MapReader>   mapReader;
    Arena<ListReader>  listReader;
    Arena<ArrayView>   arrayView;

    ScratchMark Mark()
    {
        ScratchMark m;
        m.mapBuilder  = mapBuilder.Mark();
        m.listBuilder = listBuilder.Mark();
        m.mapReader   = mapReader.Mark();
        m.listReader  = listReader.Mark();
        m.arrayView   = arrayView.Mark();
        return m;
    }

    void Rewind(const ScratchMark& m)
    {
        mapBuilder.Rewind(m.mapBuilder);
        listBuilder.Rewind(m.listBuilder);
        mapReader.Rewind(m.mapReader);
        listReader.Rewind(m.listReader);
        arrayView.Rewind(m.arrayView);
    }
};

// 인스턴스가 스레드를 넘나들지 않는다는 전제(NeoScript.h 와 동일)라 thread_local 로 충분하다.
Scratch& TheScratch()
{
    static thread_local Scratch s;
    return s;
}

class ScratchFrame
{
public:
    ScratchFrame() : m_mark(TheScratch().Mark()) {}
    ~ScratchFrame() { TheScratch().Rewind(m_mark); }
    ScratchFrame(const ScratchFrame&) = delete;
    ScratchFrame& operator=(const ScratchFrame&) = delete;

private:
    ScratchMark m_mark;
};

// 값 → 핸들. 전부 "아레나에 복사하고 주소를 준다" 한 가지 패턴이다.
inline NsMapBuilder* Hand(MapBuilder v)
{
    MapBuilder* p = TheScratch().mapBuilder.Alloc();
    *p = v;
    return reinterpret_cast<NsMapBuilder*>(p);
}
inline NsListBuilder* Hand(ListBuilder v)
{
    ListBuilder* p = TheScratch().listBuilder.Alloc();
    *p = v;
    return reinterpret_cast<NsListBuilder*>(p);
}
inline NsMapReader* Hand(MapReader v)
{
    MapReader* p = TheScratch().mapReader.Alloc();
    *p = v;
    return reinterpret_cast<NsMapReader*>(p);
}
inline NsListReader* Hand(ListReader v)
{
    ListReader* p = TheScratch().listReader.Alloc();
    *p = v;
    return reinterpret_cast<NsListReader*>(p);
}
inline NsArrayView* Hand(ArrayView v)
{
    ArrayView* p = TheScratch().arrayView.Alloc();
    *p = v;
    return reinterpret_cast<NsArrayView*>(p);
}

inline MapBuilder*  Deref(NsMapBuilder* h)  { return reinterpret_cast<MapBuilder*>(h); }
inline ListBuilder* Deref(NsListBuilder* h) { return reinterpret_cast<ListBuilder*>(h); }
inline MapReader*   Deref(NsMapReader* h)   { return reinterpret_cast<MapReader*>(h); }
inline ListReader*  Deref(NsListReader* h)  { return reinterpret_cast<ListReader*>(h); }
inline ArrayView*   Deref(NsArrayView* h)   { return reinterpret_cast<ArrayView*>(h); }
inline CallContext* Deref(NsContext* h)     { return reinterpret_cast<CallContext*>(h); }

} // namespace

//------------------------------------------------------------------------------
// ObjectType 전역 인터닝. 인덱스로 쓰는 이유는 파일 상단 주석 3항 참고.
//------------------------------------------------------------------------------
namespace {

std::mutex& ObjectTypeLock()
{
    static std::mutex m;
    return m;
}

std::vector<ObjectType>& ObjectTypeTable()
{
    static std::vector<ObjectType> t;
    return t;
}

std::map<std::pair<const void*, std::string>, uint32_t>& ObjectTypeIndex()
{
    static std::map<std::pair<const void*, std::string>, uint32_t> m;
    return m;
}

bool LookupObjectType(NsObjectType handle, ObjectType& out)
{
    if (handle == 0)
        return false;
    std::lock_guard<std::mutex> guard(ObjectTypeLock());
    std::vector<ObjectType>& table = ObjectTypeTable();
    if (handle > table.size())
        return false;
    out = table[handle - 1];
    return true;
}

} // namespace

//------------------------------------------------------------------------------
// 런타임 구현체. 헤더의 불투명 NsRuntime 이 곧 이 타입이다.
//------------------------------------------------------------------------------
struct NsRuntime
{
    IRuntime*     rt = nullptr;
    NsRuntimeDesc desc;
    std::string   libPath;

    // 에러 문자열은 호출자가 복사할 때까지 살아 있어야 한다 → 여기에 보관한다.
    Error         lastCompileError;
    std::string   lastRuntimeError;

    std::vector<uint8_t> bytecode;   // NsCompileToBytecode 산출물

    NsRuntime() { std::memset(&desc, 0, sizeof(desc)); }
};

namespace {

struct LoaderBridge : ILoader
{
    NsRuntime* owner = nullptr;

    bool Load(StringView path, std::vector<uint8_t>& out) override
    {
        if (owner == nullptr || owner->desc.load == nullptr)
            return false;
        return owner->desc.load(owner->desc.user, path.data(), (int32_t)path.size(), &out) != 0;
    }

    StringView LibPath() const override
    {
        if (owner == nullptr)
            return StringView();
        return StringView(owner->libPath.c_str(), owner->libPath.size());
    }
};

// IRuntime* → NsRuntime*. onInstanceBind 훅과 CallContext::runtime() 이 IRuntime* 만
// 주기 때문에 역인덱스가 필요하다.
std::mutex& RuntimeLock()
{
    static std::mutex m;
    return m;
}

std::map<IRuntime*, NsRuntime*>& RuntimeIndex()
{
    static std::map<IRuntime*, NsRuntime*> m;
    return m;
}

NsRuntime* FindWrapper(IRuntime* rt)
{
    if (rt == nullptr)
        return nullptr;
    std::lock_guard<std::mutex> guard(RuntimeLock());
    std::map<IRuntime*, NsRuntime*>& index = RuntimeIndex();
    std::map<IRuntime*, NsRuntime*>::iterator it = index.find(rt);
    return it == index.end() ? nullptr : it->second;
}

// 런타임별 로더 브리지. ILoader 는 런타임 수명 내내 살아 있어야 한다.
std::map<NsRuntime*, std::unique_ptr<LoaderBridge>>& LoaderIndex()
{
    static std::map<NsRuntime*, std::unique_ptr<LoaderBridge>> m;
    return m;
}

//------------------------------------------------------------------------------
// 객체 디스패치 슬롯 + 썽크 테이블 (파일 상단 주석 1항)
//------------------------------------------------------------------------------
struct SlotRec
{
    NsRuntime* owner = nullptr;
};

SlotRec g_slots[NS_MAX_OBJECT_SLOTS];
std::mutex& SlotLock()
{
    static std::mutex m;
    return m;
}

bool DispatchMethod(int slot, CallContext& ctx, StringView method)
{
    NsRuntime* owner = g_slots[slot].owner;
    if (owner == nullptr || owner->desc.method == nullptr)
        return false;
    ScratchFrame frame;   // 이 호출 안에서 발급한 빌더/리더는 반환과 동시에 무효가 된다
    return owner->desc.method(owner->desc.user, slot,
                              reinterpret_cast<NsContext*>(&ctx),
                              method.data(), (int32_t)method.size()) != 0;
}

bool DispatchProperty(int slot, CallContext& ctx, StringView name, bool isGet)
{
    NsRuntime* owner = g_slots[slot].owner;
    if (owner == nullptr || owner->desc.property == nullptr)
        return false;
    ScratchFrame frame;
    return owner->desc.property(owner->desc.user, slot,
                                reinterpret_cast<NsContext*>(&ctx),
                                name.data(), (int32_t)name.size(), isGet ? 1 : 0) != 0;
}

template <int N>
bool MethodThunk(CallContext& ctx, StringView method) { return DispatchMethod(N, ctx, method); }

template <int N>
bool PropertyThunk(CallContext& ctx, StringView name, bool isGet) { return DispatchProperty(N, ctx, name, isGet); }

template <std::size_t... I>
std::array<NativeMethod, sizeof...(I)> MakeMethodThunks(std::index_sequence<I...>)
{
    return { { &MethodThunk<(int)I>... } };
}

template <std::size_t... I>
std::array<NativeProperty, sizeof...(I)> MakePropertyThunks(std::index_sequence<I...>)
{
    return { { &PropertyThunk<(int)I>... } };
}

const std::array<NativeMethod, NS_MAX_OBJECT_SLOTS> g_methodThunks =
    MakeMethodThunks(std::make_index_sequence<NS_MAX_OBJECT_SLOTS>{});
const std::array<NativeProperty, NS_MAX_OBJECT_SLOTS> g_propertyThunks =
    MakePropertyThunks(std::make_index_sequence<NS_MAX_OBJECT_SLOTS>{});

//------------------------------------------------------------------------------
// 전역 로그 훅. SetLogHandler 는 user 포인터를 받지 않으므로 여기 세워 둔다.
//------------------------------------------------------------------------------
NsLogFn g_printFn = nullptr;
NsLogFn g_errorFn = nullptr;
void*   g_logUser = nullptr;

void LogPrintBridge(const char* msg)
{
    if (g_printFn != nullptr && msg != nullptr)
        g_printFn(g_logUser, msg, (int32_t)std::strlen(msg));
}

void LogErrorBridge(const char* msg)
{
    if (g_errorFn != nullptr && msg != nullptr)
        g_errorFn(g_logUser, msg, (int32_t)std::strlen(msg));
}

void InstanceBindBridge(IRuntime* rt, InstanceHandle instance, void* instanceUserData)
{
    NsRuntime* wrapper = FindWrapper(rt);
    if (wrapper == nullptr || wrapper->desc.onInstanceBind == nullptr)
        return;
    ScratchFrame frame;
    wrapper->desc.onInstanceBind(wrapper->desc.user, wrapper,
                                 Pack(instance.id, instance.generation), instanceUserData);
}

void FillError(const Error& src, NsError* out)
{
    if (out == nullptr)
        return;
    out->code       = src.code;
    out->line       = src.line;
    out->column     = src.column;
    out->_pad       = 0;
    out->message    = ToNsStr(src.message);
    out->sourceName = ToNsStr(src.sourceName);
}

} // namespace

//------------------------------------------------------------------------------
// 소유하는 FunctionHandle
//------------------------------------------------------------------------------
struct NsFunction
{
    FunctionHandle handle;
};

//------------------------------------------------------------------------------
// 호스트→스크립트 호출 슬롯. Invocation 은 이동 전용이라 힙에 둔다.
//------------------------------------------------------------------------------
struct NsCall
{
    NsRuntime*  owner = nullptr;
    Invocation  invocation;
    CallResult  result;       // NsCallResult.str 이 가리키는 문자열의 주인
    ScratchMark mark;
    bool        marked = false;
};

namespace {

// NsCallEnd 는 LIFO 가 정상이다(중첩 네이티브→스크립트 호출이 그렇게 쌓인다).
// 어긋나게 끝내도 터지지 않게, 스택 맨 위일 때만 아레나를 되감는다.
// 바깥 호출이 끝날 때 어차피 전부 회수된다.
std::vector<NsCall*>& CallStack()
{
    static thread_local std::vector<NsCall*> s;
    return s;
}

NsCall* PushCall(NsRuntime* rt)
{
    NsCall* call = new NsCall();
    call->owner  = rt;
    call->mark   = TheScratch().Mark();
    call->marked = true;
    CallStack().push_back(call);
    return call;
}

} // namespace

//==============================================================================
// 프로세스 전역
//==============================================================================
extern "C" {

NS_API uint32_t NS_CALL NsGetAbiVersion(void)
{
    return NS_ABI_VERSION;
}

NS_API void NS_CALL NsSetLogHandler(NsLogFn print, NsLogFn error, void* user)
{
    g_printFn = print;
    g_errorFn = error;
    g_logUser = user;
    SetLogHandler(print != nullptr ? &LogPrintBridge : nullptr,
                  error != nullptr ? &LogErrorBridge : nullptr);
}

NS_API void NS_CALL NsGetAllocStats(NsAllocStats* out)
{
    if (out == nullptr)
        return;
    AllocStats s;
    GetAllocStats(s);
    out->strings         = s.strings;
    out->maps            = s.maps;
    out->lists           = s.lists;
    out->sets            = s.sets;
    out->coroutines      = s.coroutines;
    out->modules         = s.modules;
    out->asyncs          = s.asyncs;
    out->vectors         = s.vectors;
    out->arrays          = s.arrays;
    out->closures        = s.closures;
    out->bufferBytes     = s.bufferBytes;
    out->poolBytes       = s.poolBytes;
    out->stringIdleBytes = s.stringIdleBytes;
}

NS_API void NS_CALL NsSinkWrite(void* sink, const void* data, int32_t len)
{
    if (sink == nullptr || data == nullptr || len <= 0)
        return;
    std::vector<uint8_t>* out = static_cast<std::vector<uint8_t>*>(sink);
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + len);
}

//==============================================================================
// Runtime
//==============================================================================
NS_API NsRuntime* NS_CALL NsRuntimeCreate(const NsRuntimeDesc* desc)
{
    if (desc == nullptr || desc->structSize != (uint32_t)sizeof(NsRuntimeDesc))
        return nullptr;

    NsRuntime* wrapper = new NsRuntime();
    wrapper->desc = *desc;
    if (desc->libPath.data != nullptr && desc->libPath.len > 0)
        wrapper->libPath.assign(desc->libPath.data, (std::size_t)desc->libPath.len);

    std::unique_ptr<LoaderBridge> loader;
    if (desc->load != nullptr)
    {
        loader.reset(new LoaderBridge());
        loader->owner = wrapper;
    }

    RuntimeDesc rd;
    rd.loader         = loader.get();
    rd.onInstanceBind = desc->onInstanceBind != nullptr ? &InstanceBindBridge : nullptr;

    wrapper->rt = CreateRuntime(rd);
    if (wrapper->rt == nullptr)
    {
        delete wrapper;
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> guard(RuntimeLock());
        RuntimeIndex()[wrapper->rt] = wrapper;
        if (loader)
            LoaderIndex()[wrapper] = std::move(loader);
    }
    return wrapper;
}

NS_API void NS_CALL NsRuntimeDestroy(NsRuntime* rt)
{
    if (rt == nullptr)
        return;

    // 이 런타임이 쥔 디스패치 슬롯을 놓아 준다. 유니티 도메인 리로드처럼 런타임이
    // 수시로 다시 만들어지는 호스트에서 슬롯이 마르지 않게 하는 게 핵심이다.
    {
        std::lock_guard<std::mutex> guard(SlotLock());
        for (int i = 0; i < NS_MAX_OBJECT_SLOTS; ++i)
        {
            if (g_slots[i].owner == rt)
                g_slots[i].owner = nullptr;
        }
    }

    IRuntime* inner = rt->rt;
    {
        std::lock_guard<std::mutex> guard(RuntimeLock());
        RuntimeIndex().erase(inner);
        LoaderIndex().erase(rt);
    }

    if (inner != nullptr)
        DestroyRuntime(inner);
    delete rt;
}

NS_API void NS_CALL NsRuntimeSetLibPath(NsRuntime* rt, const char* path, int32_t len)
{
    if (rt == nullptr)
        return;
    if (path == nullptr || len <= 0)
        rt->libPath.clear();
    else
        rt->libPath.assign(path, (std::size_t)len);
}

NS_API int32_t NS_CALL NsRegisterObject(NsRuntime* rt, const NsObjectDesc* desc, int32_t* outSlot)
{
    if (rt == nullptr || rt->rt == nullptr || desc == nullptr)
        return 0;
    if (desc->structSize != (uint32_t)sizeof(NsObjectDesc))
        return 0;

    int slot = -1;
    {
        std::lock_guard<std::mutex> guard(SlotLock());
        for (int i = 0; i < NS_MAX_OBJECT_SLOTS; ++i)
        {
            if (g_slots[i].owner == nullptr)
            {
                g_slots[i].owner = rt;
                slot = i;
                break;
            }
        }
    }
    if (slot < 0)
        return 0;   // 슬롯 고갈 — NS_MAX_OBJECT_SLOTS 를 올릴 것

    NativeObjectDesc od;
    od.name          = SV(desc->name);
    od.method        = desc->hasMethod != 0 ? g_methodThunks[slot] : nullptr;
    od.property      = desc->hasProperty != 0 ? g_propertyThunks[slot] : nullptr;
    od.userData      = desc->userData;
    od.declareGlobal = desc->declareGlobal != 0;

    if (!rt->rt->RegisterObject(od))
    {
        std::lock_guard<std::mutex> guard(SlotLock());
        g_slots[slot].owner = nullptr;
        return 0;
    }

    if (outSlot != nullptr)
        *outSlot = slot;
    return 1;
}

NS_API void NS_CALL NsFreezeBindings(NsRuntime* rt)
{
    if (rt != nullptr && rt->rt != nullptr)
        rt->rt->FreezeBindings();
}

NS_API NsObjectType NS_CALL NsGetObjectType(NsRuntime* rt, const char* name, int32_t len)
{
    if (rt == nullptr || rt->rt == nullptr || name == nullptr || len <= 0)
        return 0;

    const std::string key(name, (std::size_t)len);
    const std::pair<const void*, std::string> indexKey(rt->rt, key);

    {
        std::lock_guard<std::mutex> guard(ObjectTypeLock());
        std::map<std::pair<const void*, std::string>, uint32_t>& index = ObjectTypeIndex();
        std::map<std::pair<const void*, std::string>, uint32_t>::iterator it = index.find(indexKey);
        if (it != index.end())
            return it->second;
    }

    ObjectType type = rt->rt->GetObjectType(SV(name, len));
    if (!type)
        return 0;

    std::lock_guard<std::mutex> guard(ObjectTypeLock());
    std::vector<ObjectType>& table = ObjectTypeTable();
    table.push_back(type);
    const uint32_t handle = (uint32_t)table.size();   // 1-기반
    ObjectTypeIndex()[indexKey] = handle;
    return handle;
}

NS_API int32_t NS_CALL NsTakeLastError(NsRuntime* rt, NsStr* out)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    StringView view;
    if (!rt->rt->TakeLastError(view))
        return 0;
    // 소비된 뒤에도 호출자가 읽을 수 있도록 래퍼가 복사해 들고 있는다.
    rt->lastRuntimeError.assign(view.data() != nullptr ? view.data() : "", view.size());
    if (out != nullptr)
        *out = ToNsStr(rt->lastRuntimeError);
    return 1;
}

NS_API int32_t NS_CALL NsPeekLastError(NsRuntime* rt, NsStr* out)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    StringView view;
    if (!rt->rt->PeekLastError(view))
        return 0;
    rt->lastRuntimeError.assign(view.data() != nullptr ? view.data() : "", view.size());
    if (out != nullptr)
        *out = ToNsStr(rt->lastRuntimeError);
    return 1;
}

NS_API int64_t NS_CALL NsTrimMemory(NsRuntime* rt, int32_t force)
{
    return (rt != nullptr && rt->rt != nullptr) ? rt->rt->TrimMemory(force != 0) : 0;
}

NS_API int32_t NS_CALL NsCollectCycles(NsRuntime* rt, int32_t force)
{
    return (rt != nullptr && rt->rt != nullptr) ? rt->rt->CollectCycles(force != 0) : 0;
}

NS_API void NS_CALL NsSetEmptyPageHoldSeconds(NsRuntime* rt, float sec)
{
    if (rt != nullptr && rt->rt != nullptr)
        rt->rt->SetEmptyPageHoldSeconds(sec);
}

NS_API float NS_CALL NsGetEmptyPageHoldSeconds(NsRuntime* rt)
{
    return (rt != nullptr && rt->rt != nullptr) ? rt->rt->GetEmptyPageHoldSeconds() : 0.0f;
}

NS_API void NS_CALL NsSetTrimPagesPerCall(NsRuntime* rt, int32_t pages)
{
    if (rt != nullptr && rt->rt != nullptr)
        rt->rt->SetTrimPagesPerCall(pages);
}

NS_API int32_t NS_CALL NsGetTrimPagesPerCall(NsRuntime* rt)
{
    return (rt != nullptr && rt->rt != nullptr) ? rt->rt->GetTrimPagesPerCall() : 0;
}

//==============================================================================
// Program
//==============================================================================
namespace {

// NsCompileDesc → CompileDesc. defines 벡터의 수명이 Compile 호출을 덮어야 하므로
// 호출자가 보관할 저장소를 받는다.
bool BuildCompileDesc(const NsCompileDesc* src, CompileDesc& out, std::vector<CompileDefine>& storage)
{
    if (src == nullptr || src->structSize != (uint32_t)sizeof(NsCompileDesc))
        return false;

    out.source           = SV(src->source);
    out.sourceName       = SV(src->sourceName);
    out.includeDebugInfo = src->includeDebugInfo != 0;
    out.emitAsm          = src->emitAsm != 0;
    out.debugSourcePath  = SV(src->debugSourcePath);

    if (src->defineSet != 0)
    {
        out.defineSet = ToDefineSet(src->defineSet);
    }
    else if (src->defines != nullptr && src->defineCount > 0)
    {
        storage.reserve((std::size_t)src->defineCount);
        for (int32_t i = 0; i < src->defineCount; ++i)
        {
            CompileDefine d;
            d.name = SV(src->defines[i].name);
            d.kind = (DefineKind)src->defines[i].kind;
            d.text = SV(src->defines[i].text);
            storage.push_back(d);
        }
        out.defines = Span<const CompileDefine>(storage.data(), storage.size());
    }
    return true;
}

} // namespace

NS_API NsProgram NS_CALL NsCompile(NsRuntime* rt, const NsCompileDesc* desc, NsError* outErr)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;

    CompileDesc cd;
    std::vector<CompileDefine> defines;
    if (!BuildCompileDesc(desc, cd, defines))
        return 0;

    CompileResult result = rt->rt->Compile(cd);
    rt->lastCompileError = result.error;
    FillError(rt->lastCompileError, outErr);
    if (!result.program)
        return 0;
    return Pack(result.program.id, result.program.generation);
}

NS_API int32_t NS_CALL NsCompileToBytecode(NsRuntime* rt, const NsCompileDesc* desc,
                                           NsStr* outBytes, NsError* outErr)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;

    CompileDesc cd;
    std::vector<CompileDefine> defines;
    if (!BuildCompileDesc(desc, cd, defines))
        return 0;

    rt->bytecode.clear();
    Error error = rt->rt->CompileToBytecode(cd, rt->bytecode);
    rt->lastCompileError = error;
    FillError(rt->lastCompileError, outErr);
    if (!error.ok())
        return 0;

    if (outBytes != nullptr)
    {
        outBytes->data = reinterpret_cast<const char*>(rt->bytecode.data());
        outBytes->len  = (int32_t)rt->bytecode.size();
        outBytes->_pad = 0;
    }
    return 1;
}

NS_API NsProgram NS_CALL NsLoadProgram(NsRuntime* rt, const void* bytes, int32_t len, NsError* outErr)
{
    if (rt == nullptr || rt->rt == nullptr || bytes == nullptr || len <= 0)
        return 0;

    Error error;
    ProgramHandle program = rt->rt->LoadProgram(
        Span<const uint8_t>(static_cast<const uint8_t*>(bytes), (std::size_t)len), &error);
    rt->lastCompileError = error;
    FillError(rt->lastCompileError, outErr);
    if (!program)
        return 0;
    return Pack(program.id, program.generation);
}

NS_API void NS_CALL NsDestroyProgram(NsRuntime* rt, NsProgram program)
{
    if (rt != nullptr && rt->rt != nullptr && program != 0)
        rt->rt->DestroyProgram(ToProgram(program));
}

NS_API NsDefineSet NS_CALL NsCreateDefineSet(NsRuntime* rt, const NsDefine* defines, int32_t count)
{
    if (rt == nullptr || rt->rt == nullptr || defines == nullptr || count <= 0)
        return 0;

    std::vector<CompileDefine> storage;
    storage.reserve((std::size_t)count);
    for (int32_t i = 0; i < count; ++i)
    {
        CompileDefine d;
        d.name = SV(defines[i].name);
        d.kind = (DefineKind)defines[i].kind;
        d.text = SV(defines[i].text);
        storage.push_back(d);
    }

    DefineSetHandle set = rt->rt->CreateDefineSet(Span<const CompileDefine>(storage.data(), storage.size()));
    if (!set)
        return 0;
    return Pack(set.id, set.generation);
}

NS_API void NS_CALL NsDestroyDefineSet(NsRuntime* rt, NsDefineSet set)
{
    if (rt != nullptr && rt->rt != nullptr && set != 0)
        rt->rt->DestroyDefineSet(ToDefineSet(set));
}

NS_API NsFunction* NS_CALL NsFindFunction(NsRuntime* rt, NsProgram program, const char* name, int32_t len)
{
    if (rt == nullptr || rt->rt == nullptr || program == 0)
        return nullptr;

    FunctionHandle handle = rt->rt->FindFunction(ToProgram(program), SV(name, len));
    if (!handle)
        return nullptr;

    NsFunction* fn = new NsFunction();
    fn->handle = handle;
    return fn;
}

NS_API void NS_CALL NsFunctionRelease(NsFunction* fn)
{
    delete fn;
}

NS_API int32_t NS_CALL NsFunctionValid(NsFunction* fn)
{
    return (fn != nullptr && (bool)fn->handle) ? 1 : 0;
}

//==============================================================================
// Instance
//==============================================================================
NS_API NsInstance NS_CALL NsCreateInstance(NsRuntime* rt, NsProgram program, const NsInstanceDesc* desc)
{
    if (rt == nullptr || rt->rt == nullptr || program == 0)
        return 0;
    if (desc != nullptr && desc->structSize != (uint32_t)sizeof(NsInstanceDesc))
        return 0;

    InstanceDesc id;
    if (desc != nullptr)
    {
        id.userData      = desc->userData;
        id.runGlobalInit = desc->runGlobalInit != 0;
    }

    InstanceHandle instance = rt->rt->CreateInstance(ToProgram(program), id);
    if (!instance)
        return 0;
    return Pack(instance.id, instance.generation);
}

NS_API void NS_CALL NsDestroyInstance(NsRuntime* rt, NsInstance inst)
{
    if (rt != nullptr && rt->rt != nullptr && inst != 0)
        rt->rt->DestroyInstance(ToInstance(inst));
}

NS_API int32_t NS_CALL NsIsAlive(NsRuntime* rt, NsInstance inst)
{
    return (rt != nullptr && rt->rt != nullptr && inst != 0 && rt->rt->IsAlive(ToInstance(inst))) ? 1 : 0;
}

NS_API int32_t NS_CALL NsResetInstance(NsRuntime* rt, NsInstance inst)
{
    return (rt != nullptr && rt->rt != nullptr && rt->rt->ResetInstance(ToInstance(inst))) ? 1 : 0;
}

NS_API int32_t NS_CALL NsGetState(NsRuntime* rt, NsInstance inst)
{
    if (rt == nullptr || rt->rt == nullptr)
        return NS_STATE_FAILED;
    return (int32_t)rt->rt->GetState(ToInstance(inst));
}

NS_API int32_t NS_CALL NsBindObject(NsRuntime* rt, NsInstance inst,
                                    const char* name, int32_t len, void* userData)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    return rt->rt->BindObject(ToInstance(inst), SV(name, len), userData) ? 1 : 0;
}

NS_API void NS_CALL NsBindGlobalObject(NsRuntime* rt, NsInstance inst,
                                       const char* globalName, int32_t len,
                                       NsObjectType type, void* userData)
{
    if (rt == nullptr || rt->rt == nullptr)
        return;
    ObjectType resolved;
    if (!LookupObjectType(type, resolved))
        return;
    rt->rt->BindGlobalObject(ToInstance(inst), SV(globalName, len), resolved, userData);
}

NS_API void NS_CALL NsBindGlobalMapObject(NsRuntime* rt, NsInstance inst,
                                          const char* mapName, int32_t mapLen,
                                          const char* key, int32_t keyLen,
                                          NsObjectType type, void* userData)
{
    if (rt == nullptr || rt->rt == nullptr)
        return;
    ObjectType resolved;
    if (!LookupObjectType(type, resolved))
        return;
    rt->rt->BindGlobalMapObject(ToInstance(inst), SV(mapName, mapLen), SV(key, keyLen), resolved, userData);
}

NS_API int32_t NS_CALL NsRunGlobalInit(NsRuntime* rt, NsInstance inst)
{
    if (rt == nullptr || rt->rt == nullptr)
        return NS_RUN_FAILED;
    return (int32_t)rt->rt->RunGlobalInit(ToInstance(inst));
}

NS_API int32_t NS_CALL NsResume(NsRuntime* rt, NsInstance inst, uint32_t instructionBudget, float deltaTime)
{
    if (rt == nullptr || rt->rt == nullptr)
        return NS_RUN_FAILED;
    ResumeDesc rd;
    rd.instructionBudget = instructionBudget;
    rd.deltaTime = deltaTime;
    return (int32_t)rt->rt->Resume(ToInstance(inst), rd);
}

NS_API int32_t NS_CALL NsCancel(NsRuntime* rt, NsInstance inst)
{
    return (rt != nullptr && rt->rt != nullptr && rt->rt->Cancel(ToInstance(inst))) ? 1 : 0;
}

NS_API int32_t NS_CALL NsStartSliced(NsRuntime* rt, NsInstance inst,
                                     const char* functionName, int32_t len,
                                     int32_t timeoutMs, uint32_t budget)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    return rt->rt->StartSliced(ToInstance(inst), SV(functionName, len), timeoutMs, budget) ? 1 : 0;
}

NS_API int32_t NS_CALL NsUpdateSliced(NsRuntime* rt, NsInstance inst)
{
    if (rt == nullptr || rt->rt == nullptr)
        return NS_RUN_FAILED;
    return (int32_t)rt->rt->UpdateSliced(ToInstance(inst));
}

NS_API int32_t NS_CALL NsIsRunning(NsRuntime* rt, NsInstance inst)
{
    return (rt != nullptr && rt->rt != nullptr && rt->rt->IsRunning(ToInstance(inst))) ? 1 : 0;
}

NS_API int32_t NS_CALL NsGetGlobalInt(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, int32_t* out)
{
    if (rt == nullptr || rt->rt == nullptr || out == nullptr)
        return 0;
    return rt->rt->GetGlobalInt(ToInstance(inst), SV(name, len), *out) ? 1 : 0;
}

NS_API int32_t NS_CALL NsGetGlobalFloat(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, float* out)
{
    if (rt == nullptr || rt->rt == nullptr || out == nullptr)
        return 0;
    return rt->rt->GetGlobalFloat(ToInstance(inst), SV(name, len), *out) ? 1 : 0;
}

NS_API int32_t NS_CALL NsGetGlobalString(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, NsStr* out)
{
    if (rt == nullptr || rt->rt == nullptr || out == nullptr)
        return 0;
    StringView view;
    if (!rt->rt->GetGlobalString(ToInstance(inst), SV(name, len), view))
        return 0;
    *out = ToNsStr(view);
    return 1;
}

NS_API int32_t NS_CALL NsSetGlobalInt(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, int32_t v)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    return rt->rt->SetGlobalInt(ToInstance(inst), SV(name, len), v) ? 1 : 0;
}

NS_API int32_t NS_CALL NsSetGlobalFloat(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, float v)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    return rt->rt->SetGlobalFloat(ToInstance(inst), SV(name, len), v) ? 1 : 0;
}

NS_API int32_t NS_CALL NsSetGlobalString(NsRuntime* rt, NsInstance inst, const char* name, int32_t len,
                                         const char* v, int32_t vlen)
{
    if (rt == nullptr || rt->rt == nullptr)
        return 0;
    return rt->rt->SetGlobalString(ToInstance(inst), SV(name, len), SV(v, vlen)) ? 1 : 0;
}

//==============================================================================
// 호스트 → 스크립트 호출
//==============================================================================
NS_API NsCall* NS_CALL NsCallBegin(NsRuntime* rt, NsInstance inst, NsFunction* fn)
{
    if (rt == nullptr || rt->rt == nullptr || fn == nullptr)
        return nullptr;
    NsCall* call = PushCall(rt);
    call->invocation = rt->rt->Call(ToInstance(inst), fn->handle);
    return call;
}

NS_API NsCall* NS_CALL NsCallBeginName(NsRuntime* rt, NsInstance inst, const char* name, int32_t len)
{
    if (rt == nullptr || rt->rt == nullptr)
        return nullptr;
    NsCall* call = PushCall(rt);
    call->invocation = rt->rt->Call(ToInstance(inst), SV(name, len));
    return call;
}

NS_API void NS_CALL NsCallEnd(NsCall* call)
{
    if (call == nullptr)
        return;

    // Invocation 소멸이 인스턴스의 실행 컨텍스트를 반납한다(EndHostCall).
    // 아레나를 되감기 전에 먼저 죽여야 한다.
    call->invocation = Invocation();

    std::vector<NsCall*>& stack = CallStack();
    const bool isTop = (!stack.empty() && stack.back() == call);
    for (std::size_t i = stack.size(); i > 0; --i)
    {
        if (stack[i - 1] == call)
        {
            stack.erase(stack.begin() + (std::ptrdiff_t)(i - 1));
            break;
        }
    }
    if (isTop && call->marked)
        TheScratch().Rewind(call->mark);

    delete call;
}

NS_API void NS_CALL NsCallArgInt(NsCall* call, int32_t v)
{
    if (call != nullptr) call->invocation.argInt(v);
}

NS_API void NS_CALL NsCallArgFloat(NsCall* call, float v)
{
    if (call != nullptr) call->invocation.argFloat(v);
}

NS_API void NS_CALL NsCallArgBool(NsCall* call, int32_t v)
{
    if (call != nullptr) call->invocation.argBool(v != 0);
}

NS_API void NS_CALL NsCallArgString(NsCall* call, const char* v, int32_t len)
{
    if (call != nullptr) call->invocation.argString(SV(v, len));
}

NS_API void NS_CALL NsCallArgVec3(NsCall* call, float x, float y, float z)
{
    if (call != nullptr) call->invocation.argVec3(x, y, z);
}

NS_API void NS_CALL NsCallArgObject(NsCall* call, NsObjectType type, void* userData)
{
    if (call == nullptr)
        return;
    ObjectType resolved;
    if (!LookupObjectType(type, resolved))
        return;
    call->invocation.argObject(resolved, userData);
}

NS_API void NS_CALL NsCallTimeout(NsCall* call, int32_t ms)
{
    if (call != nullptr) call->invocation.timeout(ms);
}

NS_API void NS_CALL NsCallBudget(NsCall* call, uint32_t ops)
{
    if (call != nullptr) call->invocation.budget(ops);
}

NS_API int32_t NS_CALL NsCallInvoke(NsCall* call)
{
    if (call == nullptr)
        return NS_RUN_FAILED;
    return (int32_t)call->invocation.invoke();
}

NS_API int32_t NS_CALL NsCallInvokeR(NsCall* call, NsCallResult* out)
{
    if (call == nullptr)
        return NS_RUN_FAILED;

    call->result = call->invocation.invokeR();
    const CallResult& r = call->result;
    if (out != nullptr)
    {
        out->status = (int32_t)r.status;
        out->type   = (int32_t)r.type();
        out->i      = r.asInt();
        out->_pad   = 0;
        r.asVec(out->f);
        out->str = ToNsStr(r.asString());
    }
    return (int32_t)r.status;
}

NS_API int32_t NS_CALL NsCallInvokeReadMap(NsCall* call, NsReadMapFn fn, void* user)
{
    if (call == nullptr)
        return NS_RUN_FAILED;
    if (fn == nullptr)
        return (int32_t)call->invocation.invoke();

    return (int32_t)call->invocation.invokeReadMap([fn, user](MapReader reader) {
        ScratchFrame frame;
        fn(user, Hand(reader));
    });
}

NS_API int32_t NS_CALL NsCallInvokeReadList(NsCall* call, NsReadListFn fn, void* user)
{
    if (call == nullptr)
        return NS_RUN_FAILED;
    if (fn == nullptr)
        return (int32_t)call->invocation.invoke();

    return (int32_t)call->invocation.invokeReadList([fn, user](ListReader reader) {
        ScratchFrame frame;
        fn(user, Hand(reader));
    });
}

NS_API int32_t NS_CALL NsCallInvokeReadArray(NsCall* call, NsReadArrayFn fn, void* user)
{
    if (call == nullptr)
        return NS_RUN_FAILED;
    if (fn == nullptr)
        return (int32_t)call->invocation.invoke();

    return (int32_t)call->invocation.invokeReadArray([fn, user](ArrayView view) {
        ScratchFrame frame;
        fn(user, Hand(view));
    });
}

NS_API int32_t NS_CALL NsCallStatus(NsCall* call)
{
    return call != nullptr ? (int32_t)call->invocation.status() : NS_RUN_FAILED;
}

NS_API void NS_CALL NsCallGetError(NsCall* call, NsError* out)
{
    if (out == nullptr)
        return;
    if (call == nullptr)
    {
        std::memset(out, 0, sizeof(*out));
        return;
    }
    // Error 는 Invocation 이 소유한 스냅샷이라 NsCallEnd 까지 유효하다.
    FillError(call->invocation.error(), out);
}

NS_API int32_t NS_CALL NsCallRetType(NsCall* call)
{
    return call != nullptr ? (int32_t)call->invocation.retType() : NS_TYPE_NONE;
}

NS_API int32_t NS_CALL NsCallRetInt(NsCall* call)
{
    return call != nullptr ? call->invocation.retInt() : 0;
}

NS_API float NS_CALL NsCallRetFloat(NsCall* call)
{
    return call != nullptr ? call->invocation.retFloat() : 0.0f;
}

NS_API int32_t NS_CALL NsCallRetBool(NsCall* call)
{
    return (call != nullptr && call->invocation.retBool()) ? 1 : 0;
}

NS_API void NS_CALL NsCallRetString(NsCall* call, NsStr* out)
{
    if (out == nullptr)
        return;
    if (call == nullptr)
    {
        std::memset(out, 0, sizeof(*out));
        return;
    }
    *out = ToNsStr(call->invocation.retString());
}

NS_API void NS_CALL NsCallRetVec(NsCall* call, float* out4)
{
    if (out4 == nullptr)
        return;
    if (call == nullptr)
    {
        out4[0] = out4[1] = out4[2] = out4[3] = 0.0f;
        return;
    }
    call->invocation.retVec(out4);
}

NS_API NsMapReader* NS_CALL NsCallRetMap(NsCall* call)
{
    if (call == nullptr)
        return nullptr;
    MapReader reader;
    if (!call->invocation.retMap(reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsListReader* NS_CALL NsCallRetList(NsCall* call)
{
    if (call == nullptr)
        return nullptr;
    ListReader reader;
    if (!call->invocation.retList(reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsArrayView* NS_CALL NsCallRetArray(NsCall* call)
{
    if (call == nullptr)
        return nullptr;
    ArrayView view;
    if (!call->invocation.retArray(view))
        return nullptr;
    return Hand(view);
}

//==============================================================================
// CallContext
//==============================================================================
NS_API void* NS_CALL NsCtxUserData(NsContext* ctx)
{
    return ctx != nullptr ? Deref(ctx)->userData() : nullptr;
}

NS_API void* NS_CALL NsCtxInstanceUserData(NsContext* ctx)
{
    return ctx != nullptr ? Deref(ctx)->instanceUserData() : nullptr;
}

NS_API NsRuntime* NS_CALL NsCtxRuntime(NsContext* ctx)
{
    return ctx != nullptr ? FindWrapper(Deref(ctx)->runtime()) : nullptr;
}

NS_API NsInstance NS_CALL NsCtxInstance(NsContext* ctx)
{
    if (ctx == nullptr)
        return 0;
    InstanceHandle h = Deref(ctx)->instance();
    return Pack(h.id, h.generation);
}

NS_API int32_t NS_CALL NsCtxArgCount(NsContext* ctx)
{
    return ctx != nullptr ? (int32_t)Deref(ctx)->argCount() : 0;
}

NS_API int32_t NS_CALL NsCtxArgType(NsContext* ctx, int32_t i)
{
    if (ctx == nullptr || i < 0)
        return NS_TYPE_NONE;
    return (int32_t)Deref(ctx)->argType((std::size_t)i);
}

NS_API int32_t NS_CALL NsCtxArgInt(NsContext* ctx, int32_t i)
{
    return (ctx != nullptr && i >= 0) ? Deref(ctx)->argInt((std::size_t)i) : 0;
}

NS_API float NS_CALL NsCtxArgFloat(NsContext* ctx, int32_t i)
{
    return (ctx != nullptr && i >= 0) ? Deref(ctx)->argFloat((std::size_t)i) : 0.0f;
}

NS_API int32_t NS_CALL NsCtxArgBool(NsContext* ctx, int32_t i)
{
    return (ctx != nullptr && i >= 0 && Deref(ctx)->argBool((std::size_t)i)) ? 1 : 0;
}

NS_API void NS_CALL NsCtxArgString(NsContext* ctx, int32_t i, NsStr* out)
{
    if (out == nullptr)
        return;
    if (ctx == nullptr || i < 0)
    {
        std::memset(out, 0, sizeof(*out));
        return;
    }
    *out = ToNsStr(Deref(ctx)->argString((std::size_t)i));
}

NS_API void NS_CALL NsCtxArgVec(NsContext* ctx, int32_t i, float* out4)
{
    if (out4 == nullptr)
        return;
    if (ctx == nullptr || i < 0)
    {
        out4[0] = out4[1] = out4[2] = out4[3] = 0.0f;
        return;
    }
    Deref(ctx)->argVec((std::size_t)i, out4);
}

NS_API NsMapReader* NS_CALL NsCtxArgMap(NsContext* ctx, int32_t i)
{
    if (ctx == nullptr || i < 0)
        return nullptr;
    MapReader reader;
    if (!Deref(ctx)->argAsMap((std::size_t)i, reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsListReader* NS_CALL NsCtxArgList(NsContext* ctx, int32_t i)
{
    if (ctx == nullptr || i < 0)
        return nullptr;
    ListReader reader;
    if (!Deref(ctx)->argAsList((std::size_t)i, reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsArrayView* NS_CALL NsCtxArgArray(NsContext* ctx, int32_t i)
{
    if (ctx == nullptr || i < 0)
        return nullptr;
    ArrayView view;
    if (!Deref(ctx)->argAsArray((std::size_t)i, view))
        return nullptr;
    return Hand(view);
}

NS_API void* NS_CALL NsCtxArgObjectUserData(NsContext* ctx, int32_t i)
{
    return (ctx != nullptr && i >= 0) ? Deref(ctx)->argObjectUserData((std::size_t)i) : nullptr;
}

NS_API NsFunction* NS_CALL NsCtxArgFunction(NsContext* ctx, int32_t i)
{
    if (ctx == nullptr || i < 0)
        return nullptr;
    FunctionHandle handle = Deref(ctx)->argFunction((std::size_t)i);
    if (!handle)
        return nullptr;
    NsFunction* fn = new NsFunction();
    fn->handle = handle;
    return fn;
}

NS_API void NS_CALL NsCtxRetInt(NsContext* ctx, int32_t v)
{
    if (ctx != nullptr) Deref(ctx)->retInt(v);
}

NS_API void NS_CALL NsCtxRetFloat(NsContext* ctx, float v)
{
    if (ctx != nullptr) Deref(ctx)->retFloat(v);
}

NS_API void NS_CALL NsCtxRetBool(NsContext* ctx, int32_t v)
{
    if (ctx != nullptr) Deref(ctx)->retBool(v != 0);
}

NS_API void NS_CALL NsCtxRetString(NsContext* ctx, const char* v, int32_t len)
{
    if (ctx != nullptr) Deref(ctx)->retString(SV(v, len));
}

NS_API void NS_CALL NsCtxRetVec2(NsContext* ctx, float x, float y)
{
    if (ctx != nullptr) Deref(ctx)->retVec2(x, y);
}

NS_API void NS_CALL NsCtxRetVec3(NsContext* ctx, float x, float y, float z)
{
    if (ctx != nullptr) Deref(ctx)->retVec3(x, y, z);
}

NS_API void NS_CALL NsCtxRetVec4(NsContext* ctx, float x, float y, float z, float w)
{
    if (ctx != nullptr) Deref(ctx)->retVec4(x, y, z, w);
}

NS_API void NS_CALL NsCtxRetNull(NsContext* ctx)
{
    if (ctx != nullptr) Deref(ctx)->retNull();
}

NS_API void NS_CALL NsCtxRetObject(NsContext* ctx, NsObjectType type, void* userData)
{
    if (ctx == nullptr)
        return;
    ObjectType resolved;
    if (!LookupObjectType(type, resolved))
        return;
    Deref(ctx)->retObject(resolved, userData);
}

NS_API void NS_CALL NsCtxRetInstanceGlobal(NsContext* ctx, NsInstance inst, const char* name, int32_t len)
{
    if (ctx != nullptr)
        Deref(ctx)->retInstanceGlobal(ToInstance(inst), SV(name, len));
}

NS_API NsMapBuilder* NS_CALL NsCtxRetMap(NsContext* ctx)
{
    return ctx != nullptr ? Hand(Deref(ctx)->retMap()) : nullptr;
}

NS_API NsListBuilder* NS_CALL NsCtxRetList(NsContext* ctx)
{
    return ctx != nullptr ? Hand(Deref(ctx)->retList()) : nullptr;
}

NS_API void NS_CALL NsCtxFail(NsContext* ctx, int32_t code, const char* msg, int32_t len)
{
    if (ctx != nullptr) Deref(ctx)->fail(code, SV(msg, len));
}

//==============================================================================
// 컬렉션 빌더
//==============================================================================
NS_API void NS_CALL NsMapSetInt(NsMapBuilder* m, const char* k, int32_t klen, int32_t v)
{
    if (m != nullptr) Deref(m)->setInt(SV(k, klen), v);
}

NS_API void NS_CALL NsMapSetFloat(NsMapBuilder* m, const char* k, int32_t klen, float v)
{
    if (m != nullptr) Deref(m)->setFloat(SV(k, klen), v);
}

NS_API void NS_CALL NsMapSetBool(NsMapBuilder* m, const char* k, int32_t klen, int32_t v)
{
    if (m != nullptr) Deref(m)->setBool(SV(k, klen), v != 0);
}

NS_API void NS_CALL NsMapSetString(NsMapBuilder* m, const char* k, int32_t klen, const char* v, int32_t vlen)
{
    if (m != nullptr) Deref(m)->setString(SV(k, klen), SV(v, vlen));
}

NS_API void NS_CALL NsMapSetVec3(NsMapBuilder* m, const char* k, int32_t klen, float x, float y, float z)
{
    if (m != nullptr) Deref(m)->setVec3(SV(k, klen), x, y, z);
}

NS_API void NS_CALL NsMapSetObject(NsMapBuilder* m, const char* k, int32_t klen, NsObjectType t, void* userData)
{
    if (m == nullptr)
        return;
    ObjectType resolved;
    if (!LookupObjectType(t, resolved))
        return;
    Deref(m)->setObject(SV(k, klen), resolved, userData);
}

NS_API NsMapBuilder* NS_CALL NsMapSetMap(NsMapBuilder* m, const char* k, int32_t klen)
{
    return m != nullptr ? Hand(Deref(m)->setMap(SV(k, klen))) : nullptr;
}

NS_API NsListBuilder* NS_CALL NsMapSetList(NsMapBuilder* m, const char* k, int32_t klen)
{
    return m != nullptr ? Hand(Deref(m)->setList(SV(k, klen))) : nullptr;
}

NS_API void NS_CALL NsListReserve(NsListBuilder* l, int32_t count)
{
    if (l != nullptr) Deref(l)->reserve(count);
}

NS_API void NS_CALL NsListResize(NsListBuilder* l, int32_t count)
{
    if (l != nullptr) Deref(l)->resize(count);
}

NS_API int32_t NS_CALL NsListCount(NsListBuilder* l)
{
    return l != nullptr ? Deref(l)->count() : 0;
}

NS_API void NS_CALL NsListPushInt(NsListBuilder* l, int32_t v)
{
    if (l != nullptr) Deref(l)->pushInt(v);
}

NS_API void NS_CALL NsListPushFloat(NsListBuilder* l, float v)
{
    if (l != nullptr) Deref(l)->pushFloat(v);
}

NS_API void NS_CALL NsListPushBool(NsListBuilder* l, int32_t v)
{
    if (l != nullptr) Deref(l)->pushBool(v != 0);
}

NS_API void NS_CALL NsListPushString(NsListBuilder* l, const char* v, int32_t len)
{
    if (l != nullptr) Deref(l)->pushString(SV(v, len));
}

NS_API void NS_CALL NsListPushVec3(NsListBuilder* l, float x, float y, float z)
{
    if (l != nullptr) Deref(l)->pushVec3(x, y, z);
}

NS_API void NS_CALL NsListPushObject(NsListBuilder* l, NsObjectType t, void* userData)
{
    if (l == nullptr)
        return;
    ObjectType resolved;
    if (!LookupObjectType(t, resolved))
        return;
    Deref(l)->pushObject(resolved, userData);
}

NS_API void NS_CALL NsListSetInt(NsListBuilder* l, int32_t index, int32_t v)
{
    if (l != nullptr) Deref(l)->setInt(index, v);
}

NS_API void NS_CALL NsListSetFloat(NsListBuilder* l, int32_t index, float v)
{
    if (l != nullptr) Deref(l)->setFloat(index, v);
}

NS_API void NS_CALL NsListSetString(NsListBuilder* l, int32_t index, const char* v, int32_t len)
{
    if (l != nullptr) Deref(l)->setString(index, SV(v, len));
}

NS_API NsMapBuilder* NS_CALL NsListPushMap(NsListBuilder* l)
{
    return l != nullptr ? Hand(Deref(l)->pushMap()) : nullptr;
}

NS_API NsListBuilder* NS_CALL NsListPushList(NsListBuilder* l)
{
    return l != nullptr ? Hand(Deref(l)->pushList()) : nullptr;
}

//==============================================================================
// 컬렉션 리더
//==============================================================================
NS_API int32_t NS_CALL NsMapHas(NsMapReader* m, const char* k, int32_t klen)
{
    return (m != nullptr && Deref(m)->has(SV(k, klen))) ? 1 : 0;
}

NS_API int32_t NS_CALL NsMapType(NsMapReader* m, const char* k, int32_t klen)
{
    return m != nullptr ? (int32_t)Deref(m)->type(SV(k, klen)) : NS_TYPE_NONE;
}

NS_API int32_t NS_CALL NsMapGetInt(NsMapReader* m, const char* k, int32_t klen, int32_t* out)
{
    if (m == nullptr || out == nullptr)
        return 0;
    return Deref(m)->getInt(SV(k, klen), *out) ? 1 : 0;
}

NS_API int32_t NS_CALL NsMapGetFloat(NsMapReader* m, const char* k, int32_t klen, float* out)
{
    if (m == nullptr || out == nullptr)
        return 0;
    return Deref(m)->getFloat(SV(k, klen), *out) ? 1 : 0;
}

NS_API int32_t NS_CALL NsMapGetBool(NsMapReader* m, const char* k, int32_t klen, int32_t* out)
{
    if (m == nullptr || out == nullptr)
        return 0;
    bool v = false;
    if (!Deref(m)->getBool(SV(k, klen), v))
        return 0;
    *out = v ? 1 : 0;
    return 1;
}

NS_API int32_t NS_CALL NsMapGetString(NsMapReader* m, const char* k, int32_t klen, NsStr* out)
{
    if (m == nullptr || out == nullptr)
        return 0;
    StringView view;
    if (!Deref(m)->getString(SV(k, klen), view))
        return 0;
    *out = ToNsStr(view);
    return 1;
}

NS_API int32_t NS_CALL NsMapGetVec(NsMapReader* m, const char* k, int32_t klen, float* out4)
{
    if (m == nullptr || out4 == nullptr)
        return 0;
    return Deref(m)->getVec(SV(k, klen), out4) ? 1 : 0;
}

NS_API NsMapReader* NS_CALL NsMapGetMap(NsMapReader* m, const char* k, int32_t klen)
{
    if (m == nullptr)
        return nullptr;
    MapReader reader;
    if (!Deref(m)->getMap(SV(k, klen), reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsListReader* NS_CALL NsMapGetList(NsMapReader* m, const char* k, int32_t klen)
{
    if (m == nullptr)
        return nullptr;
    ListReader reader;
    if (!Deref(m)->getList(SV(k, klen), reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsArrayView* NS_CALL NsMapGetArray(NsMapReader* m, const char* k, int32_t klen)
{
    if (m == nullptr)
        return nullptr;
    ArrayView view;
    if (!Deref(m)->getArray(SV(k, klen), view))
        return nullptr;
    return Hand(view);
}

NS_API int32_t NS_CALL NsListRdCount(NsListReader* l)
{
    return l != nullptr ? Deref(l)->count() : 0;
}

NS_API int32_t NS_CALL NsListRdType(NsListReader* l, int32_t index)
{
    return l != nullptr ? (int32_t)Deref(l)->type(index) : NS_TYPE_NONE;
}

NS_API int32_t NS_CALL NsListRdGetInt(NsListReader* l, int32_t index, int32_t* out)
{
    if (l == nullptr || out == nullptr)
        return 0;
    return Deref(l)->getInt(index, *out) ? 1 : 0;
}

NS_API int32_t NS_CALL NsListRdGetFloat(NsListReader* l, int32_t index, float* out)
{
    if (l == nullptr || out == nullptr)
        return 0;
    return Deref(l)->getFloat(index, *out) ? 1 : 0;
}

NS_API int32_t NS_CALL NsListRdGetBool(NsListReader* l, int32_t index, int32_t* out)
{
    if (l == nullptr || out == nullptr)
        return 0;
    bool v = false;
    if (!Deref(l)->getBool(index, v))
        return 0;
    *out = v ? 1 : 0;
    return 1;
}

NS_API int32_t NS_CALL NsListRdGetString(NsListReader* l, int32_t index, NsStr* out)
{
    if (l == nullptr || out == nullptr)
        return 0;
    StringView view;
    if (!Deref(l)->getString(index, view))
        return 0;
    *out = ToNsStr(view);
    return 1;
}

NS_API int32_t NS_CALL NsListRdGetVec(NsListReader* l, int32_t index, float* out4)
{
    if (l == nullptr || out4 == nullptr)
        return 0;
    return Deref(l)->getVec(index, out4) ? 1 : 0;
}

NS_API NsMapReader* NS_CALL NsListRdGetMap(NsListReader* l, int32_t index)
{
    if (l == nullptr)
        return nullptr;
    MapReader reader;
    if (!Deref(l)->getMap(index, reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsListReader* NS_CALL NsListRdGetList(NsListReader* l, int32_t index)
{
    if (l == nullptr)
        return nullptr;
    ListReader reader;
    if (!Deref(l)->getList(index, reader))
        return nullptr;
    return Hand(reader);
}

NS_API NsArrayView* NS_CALL NsListRdGetArray(NsListReader* l, int32_t index)
{
    if (l == nullptr)
        return nullptr;
    ArrayView view;
    if (!Deref(l)->getArray(index, view))
        return nullptr;
    return Hand(view);
}

NS_API int32_t NS_CALL NsArrayElementType(NsArrayView* a)
{
    return a != nullptr ? (int32_t)Deref(a)->elementType() : NS_ELEM_INT;
}

NS_API int32_t NS_CALL NsArrayCount(NsArrayView* a)
{
    return a != nullptr ? Deref(a)->count() : 0;
}

NS_API uint8_t* NS_CALL NsArrayBoolBits(NsArrayView* a)
{
    return a != nullptr ? Deref(a)->boolBits() : nullptr;
}

NS_API int32_t* NS_CALL NsArrayInts(NsArrayView* a)
{
    return a != nullptr ? Deref(a)->ints() : nullptr;
}

NS_API float* NS_CALL NsArrayFloats(NsArrayView* a)
{
    return a != nullptr ? Deref(a)->floats() : nullptr;
}

} // extern "C"
