#pragma once
//==============================================================================
// NeoScript 공개 API (v2 초안) — Runtime / Program / Instance 3-개념 파사드
//------------------------------------------------------------------------------
// 목표: 외부에는 Runtime·Program·Instance 만 노출. Worker/ExecContext/코드포인터/
//       스택풀/refcount 는 전부 내부 구현으로 은닉.
//
// 이 헤더는 "봉인 대상" 리뷰용 초안이다. 실제 구현은 기존 내부(INeoVM/CNeoVMProgram/
// INeoVMWorker) 위에 shim 으로 얹는다(내부 재작성 아님):
//     Runtime  ≈ INeoVM (+ Initialize)         — 이미 존재
//     Program  ≈ CNeoVMProgram                 — 이미 refcount 공유 불변 이미지
//     Instance ≈ INeoVMWorker (CreateWorker 는 이미 u32 id 반환) — 핸들 모델 반쯤 존재
//
// 리뷰 피드백 반영 사항:
//   1) C++11 최저공배수 (std::span/std::string_view/std::byte/지정초기화 미사용 →
//      자체 Span/StringView, uint8_t 바이트코드) — 구형 cocos 클라이언트(C++11)까지 커버
//   2) 네이티브 디스패처가 CallContext& 를 받음 (인스턴스 컨텍스트 유지: 반환 컬렉션 빌드,
//      스크립트 재호출(nested), 에러 보고, zero-copy 인자 읽기)
//   3) Value 는 스칼라+문자열+벡터의 "경계 마샬링 타입"만. 컬렉션은 인스턴스 스코프
//      Builder/Reader 로만 다룸(GC/스레드 안전성 때문에 Value 를 컨테이너로 만들지 않음)
//   4) 디버거는 IDebugger 로 분리(Runtime->GetDebugger()) — NeoEditor 만 링크
//   5) 빌트인 리플렉션·Call 타임아웃/예산·프로그램 직렬화(캐시) 복원
//   6) 호출은 Call(RunStatus, 저비용)/CallR(CallResult)/CallReadMap·List 3갈래 —
//      프레임당 수천 번 도는 void Update 가 반환 마샬링 비용을 안 내게 함
//   7) 네이티브 객체 바인딩(RegisterObject + BindObject): 메서드 디스패처 하나로 GameObject.*/
//      Services.* 처럼 수백 메서드 노출. userData 는 인스턴스별(BindObject). 내부
//      RegisterTableCallBack + NeoGlobalInterface 에 매핑 — Neo3D 통합의 실제 형태.
//   * 네이티브 비동기(beginAsync/CompleteAsync)와 단독 전역 함수 등록은 제외 — VM 실행 모델이
//     지원하지 않거나(전자) 객체 바인딩으로 대체 가능(후자)해서, 쓰이지 않는 API 만 남던 것을 걷어냄
//==============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace NeoScript
{

//------------------------------------------------------------------------------
// C++11 용 문자열 뷰 (std::string_view 대체, C++17 불필요). 소유하지 않는다.
// const char* / std::string 양쪽에서 암시적 생성 → string_view 와 사용감 동일.
//------------------------------------------------------------------------------
class StringView
{
public:
    StringView() noexcept = default;
    StringView(const char* s) noexcept : m_data(s), m_size(s ? std::strlen(s) : 0) {}
    StringView(const char* s, std::size_t n) noexcept : m_data(s), m_size(n) {}
    StringView(const std::string& s) noexcept : m_data(s.c_str()), m_size(s.size()) {}

    const char* data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    std::string str() const { return std::string(m_data ? m_data : "", m_size); }

private:
    const char* m_data = nullptr;
    std::size_t m_size = 0;
};

//------------------------------------------------------------------------------
// C++11 용 경량 뷰 (std::span 대체). 소유하지 않는다.
//------------------------------------------------------------------------------
template <typename T>
class Span
{
public:
    constexpr Span() noexcept = default;
    constexpr Span(const T* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    template <std::size_t N>
    constexpr Span(const T (&arr)[N]) noexcept : m_data(arr), m_size(N) {}
    // vector<A> 에서 생성 — Span<const X> 가 vector<X>(non-const)도 받도록(A* → const X* 변환).
    template <typename A>
    Span(const std::vector<A>& v) noexcept : m_data(v.data()), m_size(v.size()) {}

    constexpr const T* data() const noexcept { return m_data; }
    constexpr std::size_t size() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }
    constexpr const T* begin() const noexcept { return m_data; }
    constexpr const T* end() const noexcept { return m_data + m_size; }
    constexpr const T& operator[](std::size_t i) const noexcept { return m_data[i]; }

private:
    const T* m_data = nullptr;
    std::size_t m_size = 0;
};

//------------------------------------------------------------------------------
// 핸들 — 포인터/refcount 은닉. generation 으로 use-after-free 감지.
//------------------------------------------------------------------------------
struct ProgramHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;
    explicit operator bool() const noexcept { return id != 0; }
};

struct InstanceHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;
    explicit operator bool() const noexcept { return id != 0; }
};

struct FunctionHandle
{
    uint32_t index = UINT32_MAX;
    // 이 핸들이 속한 Program(교차 검증용). 0 = 미지정 → Call 이 검증 생략(하위호환).
    // FindFunction/argFunction 이 채운다. Call 은 non-zero 면 인스턴스의 Program 과 일치 검사.
    uint32_t programId = 0;
    uint32_t programGeneration = 0;
    explicit operator bool() const noexcept { return index != UINT32_MAX; }
};

//------------------------------------------------------------------------------
// 상태 / 에러
//------------------------------------------------------------------------------
enum class RunStatus : uint8_t
{
    Completed,   // 끝까지 실행됨
    Suspended,   // sleep/디버거 등으로 정지 → Resume 필요
    Yielded,     // 코루틴 yield
    Failed,      // 런타임 에러
    Cancelled,   // Cancel 로 실행이 버려짐
};

enum class InstanceState : uint8_t
{
    Idle,
    Running,
    Suspended,
    Failed,
};

enum class ValueType : uint8_t
{
    None,
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Map,      // 경계 Value 로는 직접 못 담음 — Reader/Builder 로만 접근
    List,
    Set,
    Function,
};

struct Error
{
    // 0=에러 없음. 컴파일 에러는 컴파일러 코드, 런타임 에러는 1(일반) 또는
    // 네이티브가 CallContext::fail(code,...) 로 준 호스트 정의 코드.
    int32_t code = 0;
    std::string message;      // 소유(값) — string_view 는 임시 수명 문제라 값 보관
    std::string sourceName;
    uint32_t line = 0;
    uint32_t column = 0;

    bool ok() const noexcept { return code == 0; }
};

//------------------------------------------------------------------------------
// 규칙 통일: 경계 타입은 전부 "살아있는 VM 객체 위 핸들"이다(zero-copy, 호출 스코프 내
// 유효). owning 값타입(구 Value)은 없다 — 호스트는 자기 C++ 타입(int/float/StringView)으로
// in-place 로 읽고 쓴다. 컬렉션은 Builder(쓰기)/Reader(읽기) 핸들.
//------------------------------------------------------------------------------
class MapBuilder;
class ListBuilder;
class MapReader;
class ListReader;
class CallContext;
class Invocation;

// RegisterObject 로 등록한 네이티브 객체 타입의 핸들. 반환 맵/리스트에 바인딩된 객체를
// 중첩할 때(setObject/pushObject) 쓴다. 매 호출 문자열 조회를 피하려 GetObjectType 로
// 1회 해석해 재사용(핫패스: 물리 쿼리 히트마다 반복). 유효하지 않으면 falsy.
struct ObjectType
{
    explicit operator bool() const noexcept { return m_impl != nullptr; }
private:
    // 내부 디스패처(RegisteredObject*)를 담는 불투명 포인터. 구성/해석은 Runtime 내부(NeoScriptInternal)만.
    // → 소비자가 임의 포인터를 만들거나 읽지 못하게 은닉(raw void* 그대로라 핫패스 비용은 0).
    friend struct NeoScriptInternal;
    void* m_impl = nullptr;
};

class ListBuilder
{
public:
    void reserve(int count);
    void resize(int count);
    int  count() const;
    void pushInt(int32_t v);
    void pushFloat(float v);
    void pushBool(bool v);
    void pushString(StringView v);
    void pushVec3(float x, float y, float z);
    void setInt(int index, int32_t v);
    void setFloat(int index, float v);
    void setString(int index, StringView v);
    MapBuilder  pushMap();       // 중첩 map 원소
    ListBuilder pushList();      // 중첩 list 원소
    // 바인딩된 네이티브 객체를 원소로 추가. type=RegisterObject 타입(공유 디스패처),
    // userData=이 인스턴스 포인터(스크립트가 그 객체 메서드/프로퍼티 호출 시 전달).
    void        pushObject(ObjectType type, void* userData);

private:
    friend class CallContext; friend class Invocation; friend struct NeoScriptInternal; friend class MapBuilder;
    void* m_impl = nullptr;
};

class MapBuilder
{
public:
    void setInt(StringView key, int32_t v);
    void setFloat(StringView key, float v);
    void setBool(StringView key, bool v);
    void setString(StringView key, StringView v);
    void setVec3(StringView key, float x, float y, float z);
    MapBuilder  setMap(StringView key);   // 중첩 map
    ListBuilder setList(StringView key);  // 중첩 list
    // 바인딩된 네이티브 객체를 값으로. type=RegisterObject 타입(공유 디스패처),
    // userData=이 인스턴스 포인터. (예: 물리 Raycast 반환 맵의 "Object" = 해당 GameObject)
    void        setObject(StringView key, ObjectType type, void* userData);

private:
    friend class CallContext; friend class Invocation; friend struct NeoScriptInternal; friend class ListBuilder;
    void* m_impl = nullptr;
};

class MapReader
{
public:
    bool has(StringView key) const;
    ValueType type(StringView key) const;
    bool getInt(StringView key, int32_t& out) const;
    bool getFloat(StringView key, float& out) const;
    bool getBool(StringView key, bool& out) const;
    bool getString(StringView key, StringView& out) const;  // zero-copy VM 문자열 뷰
    bool getVec(StringView key, float out[4]) const;   // 항상 4개를 채운다(Vec2 면 out[2..3]=0)
    bool getMap(StringView key, MapReader& out) const;
    bool getList(StringView key, ListReader& out) const;

private:
    friend class CallContext; friend class Invocation; friend struct NeoScriptInternal;
    const void* m_impl = nullptr;
};

class ListReader
{
public:
    int count() const;
    ValueType type(int index) const;
    bool getInt(int index, int32_t& out) const;
    bool getFloat(int index, float& out) const;
    bool getBool(int index, bool& out) const;
    bool getString(int index, StringView& out) const;
    bool getVec(int index, float out[4]) const;
    bool getMap(int index, MapReader& out) const;
    bool getList(int index, ListReader& out) const;

private:
    friend class CallContext; friend class Invocation; friend struct NeoScriptInternal;
    const void* m_impl = nullptr;
};

//------------------------------------------------------------------------------
// CallContext — 네이티브 함수/메서드가 받는 컨텍스트. 인자/반환 전부 타입드 zero-copy.
//------------------------------------------------------------------------------
class IRuntime;

class CallContext
{
public:
    // 소속 / userData
    void* userData() const;          // 이 호출의 바인딩 userData(BindObject 로 준 인스턴스별 값,
                                     // 없으면 NativeObjectDesc.userData)
    void* instanceUserData() const;  // InstanceDesc.userData (인스턴스 일반 데이터)
    IRuntime* runtime() const;
    InstanceHandle instance() const;
    // (raw INeoVMWorker*/VarInfo* 는 공개 API 에 노출되지 않는다 — 전부 shim 내부.)

    // 인자 읽기 (zero-copy, VM 슬롯 직접)
    std::size_t argCount() const;
    ValueType   argType(std::size_t i) const;
    int32_t     argInt(std::size_t i) const;
    float       argFloat(std::size_t i) const;
    bool        argBool(std::size_t i) const;
    StringView  argString(std::size_t i) const;   // VM 문자열 뷰(호출 중 유효)
    void        argVec(std::size_t i, float out[4]) const;
    bool        argAsMap(std::size_t i, MapReader& out) const;
    bool        argAsList(std::size_t i, ListReader& out) const;
    // [콜백] 스크립트 함수(VAR_FUN) 인자를 FunctionHandle 로 캡처 → 나중에 IRuntime::Call 로 호출.
    // 함수 인자가 아니면 무효 핸들. (구 VarInfo::_fun_index 저장 패턴의 v2 대체.)
    FunctionHandle argFunction(std::size_t i) const;
    // 바인딩된 네이티브 객체 인자에서 그 객체의 userData 를 얻는다(setObject/BindObject 로 준 값).
    // 예: 스크립트가 넘긴 GameObject 인자 → 그 eventID. 객체가 아니면 nullptr.
    void*       argObjectUserData(std::size_t i) const;

    // 반환 쓰기 (VM 반환 슬롯 직접)
    void        retInt(int32_t v);
    void        retFloat(float v);
    void        retBool(bool v);
    void        retString(StringView v);
    void        retVec2(float x, float y);
    void        retVec3(float x, float y, float z);
    void        retVec4(float x, float y, float z, float w);
    // [전이용] 다른 인스턴스의 export 전역변수를 현재 반환값으로(임의 타입). 구 GetExport 의 크로스워커
    // ReturnValue(otherWorker->GetVar(name)) 재현. 미존재면 None. GameObject.GetExport 전용.
    void        retInstanceGlobal(InstanceHandle instance, StringView name);
    MapBuilder  retMap();    // 반환 슬롯에 map 조립
    ListBuilder retList();   // 반환 슬롯에 list 조립
    // 반환 슬롯(프로퍼티 get 이면 그 슬롯)에 바인딩된 네이티브 객체를 넣는다.
    // 예: Scene.Find 가 GameObject 를 반환. userData=그 객체의 eventID.
    void        retObject(ObjectType type, void* userData);
    void        retNull();   // 반환/프로퍼티 슬롯을 None 으로(예: 못 찾은 객체 반환)

    // 이 네이티브 호출의 실패 사유를 남긴다. **호출 후 디스패처는 반드시 false 를 반환해야 한다**
    // (fail 자체가 실행을 끊지는 않는다). 남긴 메시지는 스크립트 IP/라인/스택트레이스와 함께
    // 조립되어 Invocation::error() / TakeLastError() 로 나온다. fail 없이 false 만 반환하면
    // "invalid call" 로 뭉개지므로 실패 경로에서는 되도록 사유를 남길 것.
    // code 는 호스트 정의값으로 Error.code 에 실린다(0 이면 일반 런타임 에러 = 1).
    // 한 호출에서 여러 번 불러도 첫 번째만 유효(가장 안쪽 원인 보존).
    void        fail(int32_t code, StringView message);

private:
    friend class IRuntime;
    friend struct NeoScriptInternal;
    void* m_impl = nullptr;
};

//------------------------------------------------------------------------------
// 네이티브 객체 바인딩 — 호스트 객체를 스크립트 전역으로 노출(GameObject.*, Services.*).
// 메서드 디스패처 하나가 이름으로 분기 → 수백 메서드를 함수 하나로 바인딩.
// (내부 RegisterTableCallBack + NeoGlobalInterface 모델에 매핑)
//
// userData 는 "바인딩별"이다: 이 디스패처가 불릴 때 ctx.userData() 로 받는다.
//  · RegisterObject 의 userData = 기본값(전역 공유 객체, 예: Services)
//  · BindObject(instance, name, userData) = 인스턴스별 덮어쓰기(예: GameObject 의 eventID)
//------------------------------------------------------------------------------
using NativeMethod   = bool (*)(CallContext& ctx, StringView method);
// get: 네이티브가 ctx.retX() 로 값 제공. set: ctx.argX(0) 로 들어온 값 읽음.
using NativeProperty = bool (*)(CallContext& ctx, StringView name, bool isGet);

struct NativeObjectDesc
{
    StringView     name;                 // 스크립트 전역명 ("GameObject","Services")
    NativeMethod   method   = nullptr;   // obj.method(args) 디스패처
    NativeProperty property = nullptr;   // obj.field get/set (선택)
    void*          userData = nullptr;   // 기본 userData(인스턴스별 미지정 시)
    // true: 호스트가 전역 심볼을 선언(GameObject/Services 스타일). false: 스크립트가
    // `export var name` 로 직접 선언하고 호스트는 바인딩만(중복 선언 방지).
    bool           declareGlobal = true;
};

//------------------------------------------------------------------------------
// 컴파일 / 인스턴스 / 호출 디스크립터
//------------------------------------------------------------------------------
// 컴파일 타임 define (구 NeoCompileDefines). text 는 값의 텍스트 표현.
enum class DefineKind : uint8_t { Identifier, Int, Float, String, Bool, Null };
struct CompileDefine
{
    StringView name;
    DefineKind kind = DefineKind::Int;
    StringView text;   // "37" / "1.5" / "hi"(따옴표 없이) / "true" 등. Null 은 text 무시.
};

// 미리 빌드한 define 집합. 같은 defines 로 여러 번 컴파일할 때 매 Compile 의 맵 재구성
// (문자열 생성 + 해시맵 할당)을 피한다. CreateDefineSet 으로 한 번 만들어 여러 CompileDesc 에 넘긴다.
struct DefineSetHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;
    explicit operator bool() const noexcept { return id != 0; }
};

struct CompileDesc
{
    StringView source;
    StringView sourceName;
    bool optimize = true;
    bool includeDebugInfo = false;
    bool emitAsm = false;   // true 면 컴파일 시 디스어셈블(ASM)을 stdout 으로 덤프(진단용)
    // define 은 둘 중 하나로. 재사용이면 defineSet(빌드 1회), 매번 다르면 defines(인라인).
    Span<const CompileDefine> defines;   // 인라인(적은 수/매번 다를 때)
    DefineSetHandle           defineSet; // 미리 만든 집합(우선). 있으면 defines 무시.
    // 디버그 다중 소스 매핑(선택, includeDebugInfo=true 일 때): 메인 소스 경로 +
    // import 로 끌려온 소스 파일 경로 목록(out). 파일 인덱스는 이 목록 순서.
    StringView                debugSourcePath;
    std::vector<std::string>* debugSourceFiles = nullptr;
};

struct CompileResult
{
    ProgramHandle program;
    Error error;
};

struct InstanceDesc
{
    void* userData = nullptr;
    bool runGlobalInit = true;   // 생성 시 스크립트 최상위(전역 초기화) 실행
};

struct ResumeDesc
{
    uint32_t instructionBudget = 0;
    float deltaTime = 0.0f;
};

//------------------------------------------------------------------------------
// Invocation — 호스트→스크립트 호출 핸들. 인자를 VM 슬롯에 직접 쓰고(zero-copy), 실행 후
// 반환을 in-place 로 읽는다. 반환/리더는 이 핸들이 살아있는 동안만 유효(컨텍스트 보유).
//   int x = 0;
//   {
//       Invocation call = rt->Call(inst, "compute");   // 인스턴스의 실행 컨텍스트를 빌림
//       if (call.argInt(10).argInt(20).invoke() == RunStatus::Completed)
//           x = call.retInt();
//   }                                                   // 여기서 컨텍스트 반납(EndHostCall)
//
// [수명 규칙] Invocation 은 자기 인스턴스보다 **먼저** 소멸해야 한다. 인스턴스의 실행
// 컨텍스트를 소멸 시점까지 빌리고 있으므로, DestroyInstance 이후까지 살아있으면 파괴된
// 인스턴스를 건드린다(UB). → 반드시 좁은 스코프 `{ }` 안에서 쓰거나 임시객체로 즉시 소진할 것.
// 이동 전용(컨텍스트 보유). 소멸 시 EndHostCall.
//------------------------------------------------------------------------------
// invokeR() 결과 — 스칼라/벡터/문자열 반환을 값으로 소유한다. Invocation 수명이나 이후 다른 Call 과
// 무관하게 안전(여러 결과를 동시에 들고 있어도 됨). 컬렉션(map/list)은 값 복사가 비싸/부적합하므로
// 여기 담지 않는다 → invokeReadMap/invokeReadList 로 "살아있는 스코프 안에서" 읽을 것.
struct CallResult
{
    RunStatus status = RunStatus::Failed;
    bool       ok()       const noexcept { return status == RunStatus::Completed; }
    ValueType  type()     const noexcept { return m_type; }
    int32_t    asInt()    const noexcept { return m_i; }
    float      asFloat()  const noexcept { return m_f[0]; }
    bool       asBool()   const noexcept { return m_i != 0; }
    StringView asString() const noexcept { return StringView(m_s.c_str(), m_s.size()); } // CallResult 살아있는 동안 유효
    void       asVec(float out[4]) const noexcept { out[0]=m_f[0]; out[1]=m_f[1]; out[2]=m_f[2]; out[3]=m_f[3]; }
private:
    friend struct NeoScriptInternal;
    ValueType   m_type = ValueType::None;
    int32_t     m_i = 0;
    float       m_f[4] = { 0, 0, 0, 0 };
    std::string m_s;
};

class Invocation
{
public:
    Invocation() noexcept = default;
    Invocation(Invocation&& o) noexcept : m_impl(o.m_impl), m_seq(o.m_seq) { o.m_impl = nullptr; o.m_seq = 0; }
    Invocation& operator=(Invocation&& o) noexcept;
    Invocation(const Invocation&) = delete;
    Invocation& operator=(const Invocation&) = delete;
    ~Invocation();

    // 인자 push (호출 순서대로)
    Invocation& argInt(int32_t v);
    Invocation& argFloat(float v);
    Invocation& argBool(bool v);
    Invocation& argString(StringView v);
    Invocation& argVec3(float x, float y, float z);
    // 바인딩된 네이티브 객체(GameObject 등)를 인자로 전달. type=RegisterObject 타입, userData=인스턴스(eventID 등).
    // 스크립트 콜백에 GameObject 를 넘길 때 사용(구 neo_globalinterface2 로 arg 바인딩하던 것의 v2 대체).
    Invocation& argObject(ObjectType type, void* userData);
    Invocation& timeout(int32_t ms);
    Invocation& budget(uint32_t ops);

    RunStatus invoke();
    RunStatus status() const;
    // 직전 invoke/invokeR/invokeRead* 의 실패 사유. 성공이면 ok()==true 인 빈 Error.
    // message = VM 이 조립한 상세("사유 : IP(n), Line(l)" + 스택트레이스), code 는 Error 참고.
    // 이 Invocation 이 소유한 스냅샷이라 TakeLastError() 처럼 소비되지 않고, 다른 인스턴스의
    // 에러와 섞이지도 않는다. (line/column 은 채우지 않는다 — 위치는 message 안에 있다.)
    const Error& error() const;

    //--- 안전 반환(권장) — Invocation 수명/다음 Call 과 무관 ---
    // 스칼라(int/float/bool/string/vec) 반환을 값으로 스냅샷해 돌려준다. 실행 후 컨텍스트는 즉시 반납되므로
    // 여러 CallResult 를 동시에 보관하거나, 이후 같은 인스턴스를 다시 Call 해도 안전하다.
    //   CallResult r = rt->Call(inst,"GetScore").argInt(id).invokeR();  if (r.ok()) int s = r.asInt();
    CallResult invokeR();
    // 컬렉션(map/list) 반환을 "컨텍스트 살아있는 스코프(콜백) 안에서만" 읽는다. 리더를 fn 밖으로
    // 들고 나가면 안 된다(그 시점엔 이미 반납). 필요한 값은 fn 안에서 호스트 변수로 복사할 것.
    //   rt->Call(inst,"GetInv").invokeReadMap([&](MapReader m){ m.getInt("gold", gold); });
    template<class Fn> RunStatus invokeReadMap (Fn&& fn) { return invokeReadMapImpl (&fn, &ReadTramp<MapReader,  Fn>); }
    template<class Fn> RunStatus invokeReadList(Fn&& fn) { return invokeReadListImpl(&fn, &ReadTramp<ListReader, Fn>); }

    //--- 저수준 반환 읽기 (invoke 후, 핸들 소멸/다음 Call 전까지만 유효) ---
    // ⚠️ 이 뷰들은 인스턴스의 공유 반환 컨텍스트를 라이브로 읽는다. 같은 인스턴스에 다시 Call 하면
    //    무효화된다. 값을 나중에 쓰거나 여러 개를 동시에 보관하려면 위 invokeR/invokeReadMap 을 쓸 것.
    ValueType  retType() const;
    int32_t    retInt() const;
    float      retFloat() const;
    bool       retBool() const;
    StringView retString() const;   // zero-copy VM 문자열 뷰
    void       retVec(float out[4]) const;   // 항상 4개를 채운다(Vec2 면 out[2..3]=0, 벡터가 아니면 전부 0)
    bool       retMap(MapReader& out) const;
    bool       retList(ListReader& out) const;

private:
    friend class IRuntime;
    friend struct NeoScriptInternal;
    // 콜백 트램폴린(zero-alloc): void* 로 넘긴 람다 주소를 원래 타입으로 되돌려 호출.
    template<class R, class Fn> static void ReadTramp(void* p, R r) { (*static_cast<typename std::remove_reference<Fn>::type*>(p))(r); }
    RunStatus invokeReadMapImpl (void* ctx, void(*cb)(void*, MapReader));
    RunStatus invokeReadListImpl(void* ctx, void(*cb)(void*, ListReader));
    void* m_impl = nullptr;   // InstanceRec* (인스턴스별 호출 스크래치)
    uint32_t m_seq = 0;       // 이 Invocation 의 호출 시퀀스(소유권 토큰). Call 이 부여, 소멸자가 소유 확인에 사용.
};

//------------------------------------------------------------------------------
// 디버거 — 분리된 선택 인터페이스. Runtime::GetDebugger() 로 취득(없으면 nullptr).
//------------------------------------------------------------------------------
enum class DebugStopReason : uint8_t
{
    Breakpoint,
    Step,
    Pause,
    Exception,
};

struct DebugLocation
{
    int32_t opIndex = -1;
    int32_t file = -1;
    int32_t line = -1;
    int32_t callDepth = 0;
};

// 프로그램에 저장된 디버그 명령 1개. code 는 원시 바이트코드, assembly 는 사람이 읽는 VM 명령이다.
struct DebugInstruction
{
    int32_t opIndex = -1;
    int32_t file = -1;
    int32_t line = -1;
    std::string code;
    std::string assembly;
};

struct DebugBreakpoint
{
    int32_t file = -1;
    int32_t line = -1;
};

struct DebugStackFrame
{
    int32_t frameId = 0;
    int32_t functionId = -1;
    std::string functionName;
    int32_t file = -1;
    int32_t line = -1;
    int32_t opIndex = -1;
};

struct DebugVariable
{
    std::string name;
    std::string type;
    std::string value;
    std::vector<DebugVariable> children;
};

struct IDebugListener
{
    virtual ~IDebugListener() = default;
    virtual void OnStopped(InstanceHandle instance, const DebugLocation& location, DebugStopReason reason) = 0;
};

class IDebugger
{
public:
    virtual ~IDebugger() = default;

    virtual void SetListener(IDebugListener* listener) = 0;
    virtual void SetBreakpoints(InstanceHandle instance, Span<const DebugBreakpoint> breakpoints) = 0;

    virtual void Continue(InstanceHandle instance) = 0;
    virtual void StepInto(InstanceHandle instance) = 0;
    virtual void StepOver(InstanceHandle instance) = 0;
    virtual void StepOut(InstanceHandle instance) = 0;
    virtual void Pause(InstanceHandle instance) = 0;
    virtual bool IsPaused(InstanceHandle instance) = 0;
    // Continue/Step 로 재개 모드를 설정한 뒤(콜백 밖에서) 실행을 다음 정지/종료까지 진행(= 구 worker->Run()).
    virtual RunStatus Run(InstanceHandle instance) = 0;

    virtual DebugLocation GetLocation(InstanceHandle instance) = 0;
    virtual void GetStackTrace(InstanceHandle instance, std::vector<DebugStackFrame>& frames) = 0;
    virtual void GetFrameVariables(InstanceHandle instance, int frameId, std::vector<DebugVariable>& vars) = 0;

    // 프로그램 정적 정보(브레이크포인트 검증용 실행가능 라인)
    virtual void GetExecutableLines(ProgramHandle program, std::vector<int>& lines) = 0;
    // 파일별 실행가능 위치(file+line). DAP 처럼 다중 소스에서 라인을 파일로 구분할 때.
    virtual void GetExecutableLocations(ProgramHandle program, std::vector<DebugLocation>& out) = 0;
};

//------------------------------------------------------------------------------
// 빌트인 리플렉션 (자동완성/코드어시스트)
//------------------------------------------------------------------------------
struct BuiltinInfo
{
    std::string module;                 // "math","system",... 또는 "string","list","map"
    std::string name;
    int32_t argCount = -1;
    std::string ret;
    std::vector<std::string> params;
};

//------------------------------------------------------------------------------
// Runtime 생성 / 인터페이스
//------------------------------------------------------------------------------
struct ILoader
{
    virtual ~ILoader() = default;
    virtual bool Load(StringView path, std::vector<uint8_t>& out) = 0;
    virtual StringView LibPath() const = 0;
};

struct RuntimeDesc
{
    ILoader* loader = nullptr;                    // import 해석(선택, Runtime별). nativeLoader 가 있으면 그쪽 우선.
    // [프로세스-전역] print/error 는 내부 VM 의 단일 전역 훅이다 — Runtime별이 아니다.
    // 최초 생성된 Runtime 의 printFn/errorFn 이 프로세스 전체에 적용된다(이후 Runtime 값은 무시).
    // SetLogHandler() 도 같은 전역 훅을 덮어쓴다(마지막 호출자 우선). 로그를 갈아끼우려면 그쪽을 쓸 것.
    void (*printFn)(StringView) = nullptr;  // 스크립트 print (프로세스-전역, 최초 Runtime 값)
    void (*errorFn)(StringView) = nullptr;  // 스크립트 error (프로세스-전역, 최초 Runtime 값)
    // 이미 내부 로더(INeoLoader)를 가진 호스트용 직접 전달(interop). 있으면 loader 대신 사용.
    void* nativeLoader = nullptr;
    // 인스턴스 로드 시 호출되는 바인딩 훅. 호스트(Neo3D)가 전역 심볼(GameObject/Services)에
    // 네이티브 객체를 등록하는 곳. runtime=이 인스턴스를 만든 Runtime(다중 Runtime 대응),
    // instance=이 인스턴스 핸들, userData=InstanceDesc.userData.
    // 훅 안에서는 runtime->BindGlobalObject/BindGlobalMapObject 로 인스턴스 전역에 바인딩한다.
    void (*onInstanceBind)(IRuntime* runtime, InstanceHandle instance, void* instanceUserData) = nullptr;
    // 실행 컨텍스트 풀은 내부에서 스레드별로 관리(은닉).
};

class IRuntime
{
public:
    virtual ~IRuntime() = default;

    //--- 초기 설정 (FreezeBindings 이후 등록 불가) ---
    // 네이티브 바인딩은 RegisterObject(객체 + 메서드/프로퍼티 디스패처) 하나로 통일한다.
    // 단독 전역 함수 등록 API 는 없다 — 전역 함수가 필요하면 Services.Foo() 처럼 객체 메서드로 낼 것.
    // 네이티브 객체를 전역 심볼로 선언 + 디스패처 등록(컴파일이 전역명을 알아야 하므로 Freeze/Compile 전).
    virtual bool RegisterObject(const NativeObjectDesc& desc) = 0;
    virtual void FreezeBindings() = 0;
    // 등록된 객체 타입 핸들 조회(setObject/pushObject 용). 없으면 falsy. FreezeBindings 후에도 가능.
    virtual ObjectType GetObjectType(StringView name) = 0;

    //--- Program (불변 공유 코드) ---
    // 컴파일 = 두 단계로 분리: (1) 소스→바이트코드(빌드 산출물), (2) 바이트코드→Program(실행 가능).
    //   • Compile           : 소스를 곧바로 Program 으로(내부에서 두 단계 수행). "지금 실행" 하는 흔한 경우.
    //   • CompileToBytecode  : 소스를 바이트코드로만. 그 바이트코드를 저장/전송해 뒀다가 LoadProgram 으로 로드.
    //     → "컴파일 결과를 저장하고 싶을 때만" 쓰는 경로(오프라인 캐시/서버 DB 저장). 별도 보관 상태 없음.
    virtual CompileResult Compile(const CompileDesc& desc) = 0;
    // 소스를 바이트코드로 컴파일해 outBytecode 에 채운다(Program 은 만들지 않음). 반환 Error.ok()==성공.
    // 저장한 바이트코드는 나중에 LoadProgram(bytecode) 으로 Program 화한다.
    virtual Error CompileToBytecode(const CompileDesc& desc, std::vector<uint8_t>& outBytecode) = 0;
    virtual ProgramHandle LoadProgram(Span<const uint8_t> bytecode, Error* error = nullptr) = 0;
    virtual void DestroyProgram(ProgramHandle program) = 0;   // 인스턴스 생존 시 refcount 로 지연 해제
    virtual void GetDebugInstructions(ProgramHandle program, std::vector<DebugInstruction>& out) const = 0;
    // 재사용 define 집합: 한 번 빌드해 CompileDesc.defineSet 으로 여러 Compile 에 재사용(재구성 0).
    virtual DefineSetHandle CreateDefineSet(Span<const CompileDefine> defines) = 0;
    virtual void DestroyDefineSet(DefineSetHandle set) = 0;

    //--- Instance (프로그램별 실행 상태 + 전역변수) ---
    virtual InstanceHandle CreateInstance(ProgramHandle program, const InstanceDesc& desc = {}) = 0;
    virtual void DestroyInstance(InstanceHandle instance) = 0;
    virtual bool IsAlive(InstanceHandle instance) const = 0;   // 핸들이 아직 유효한 인스턴스인지(파괴 후 false)
    // 인스턴스를 **핸들을 유지한 채** 갓 생성된 상태로 되돌린다(전역변수 초기화 + 바인딩 재구성 +
    // CreateInstance 때의 runGlobalInit 재실행). 호스트가 들고 있던 InstanceHandle/FunctionHandle 이
    // 그대로 유효하다는 점이 DestroyInstance→CreateInstance 와의 차이다.
    // BindObject 로 준 인스턴스별 userData 는 보존된다. 살아있는 Invocation 이 있으면 실패(false).
    // 실패 시 인스턴스는 파괴되고 핸들이 무효화된다(IsAlive 로 확인할 것).
    virtual bool ResetInstance(InstanceHandle instance) = 0;
    virtual InstanceState GetState(InstanceHandle instance) const = 0;
    // RegisterObject 로 선언한 객체의 userData 를 이 인스턴스에 한해 덮어씀(GameObject 의 eventID 등).
    // 디스패처는 ctx.userData() 로 이 값을 받는다. 미지정이면 NativeObjectDesc.userData 사용.
    virtual bool BindObject(InstanceHandle instance, StringView name, void* userData) = 0;
    // 인스턴스의 전역 변수(globalName)를 네이티브 객체로 바인딩(onInstanceBind 훅에서 사용).
    // type=GetObjectType 로 얻은 디스패처, userData=인스턴스별 self(예: eventID). 워커는 shim 내부에만.
    virtual void BindGlobalObject(InstanceHandle instance, StringView globalName, ObjectType type, void* userData) = 0;
    // 인스턴스의 전역 VAR_MAP(mapName)에 key 로 네이티브 객체를 삽입(서브서비스 바인딩).
    virtual void BindGlobalMapObject(InstanceHandle instance, StringView mapName, StringView key, ObjectType type, void* userData) = 0;
    // CreateInstance(runGlobalInit=false) 로 만든 인스턴스의 최상위(전역 초기화) 코드를 실행.
    // 디버거로 전역 코드에 BP 를 걸려면: CreateInstance(false) → SetBreakpoints → RunGlobalInit.
    // 반환 Completed=완료, Suspended=디버거/sleep 로 정지(Continue/Resume 필요).
    virtual RunStatus RunGlobalInit(InstanceHandle instance) = 0;

    //--- 함수 조회/호출 ---
    virtual FunctionHandle FindFunction(ProgramHandle program, StringView name) const = 0;

    // 호출: Invocation 핸들을 만들어 인자를 VM 슬롯에 직접 쓰고(zero-copy) invoke → 반환을
    // in-place 로 읽는다. owning Value 없음 — 규칙 통일(살아있는 VM객체 위 핸들).
    virtual Invocation Call(InstanceHandle instance, FunctionHandle function) = 0;
    virtual Invocation Call(InstanceHandle instance, StringView functionName) = 0;

    //--- 중단/재개 ---
    virtual RunStatus Resume(InstanceHandle instance, const ResumeDesc& desc = {}) = 0;
    // 정지(sleep/디버거)됐거나 StartSliced 로 진행 중인 실행을 버리고 인스턴스를 Idle 로 되돌린다.
    // 전역변수와 바인딩은 보존된다(초기화까지 하려면 ResetInstance).
    // **실행 중인 인스턴스를 선점하지는 않는다** — 네이티브 콜백 안에서 자기 인스턴스를 Cancel 하면
    // false 를 반환한다(그 경우엔 ctx.fail() 로 실패시킬 것). 슬라이스 사이/정지 상태에서 호출할 것.
    virtual bool Cancel(InstanceHandle instance) = 0;

    //--- 협조적/시간분할 실행 (구 BindWorkerFunction/UpdateWorker, Setup_TL/Call_TL).
    // 장시간/무한 함수를 슬라이스로 나눠 실행: StartSliced 로 시작 → IsRunning 동안 UpdateSliced 반복.
    // timeoutMs/budget = 슬라이스당 한도(-1/0 = 무제한). ---
    virtual bool StartSliced(InstanceHandle instance, StringView functionName, int32_t timeoutMs = -1, uint32_t budget = 0) = 0;
    virtual RunStatus UpdateSliced(InstanceHandle instance) = 0;   // 한 슬라이스. Completed=끝, Suspended=계속.
    virtual bool IsRunning(InstanceHandle instance) const = 0;

    //--- 전역변수 (타입드 in-place) ---
    virtual bool GetGlobalInt(InstanceHandle instance, StringView name, int32_t& out) const = 0;
    virtual bool GetGlobalFloat(InstanceHandle instance, StringView name, float& out) const = 0;
    virtual bool GetGlobalString(InstanceHandle instance, StringView name, StringView& out) const = 0;
    virtual bool SetGlobalInt(InstanceHandle instance, StringView name, int32_t v) = 0;
    virtual bool SetGlobalFloat(InstanceHandle instance, StringView name, float v) = 0;
    virtual bool SetGlobalString(InstanceHandle instance, StringView name, StringView v) = 0;

    // [비동기 네이티브는 지원하지 않는다] 네이티브 호출은 항상 동기 완료다. 스크립트가 오래 걸리는
    // 작업을 기다려야 하면 (1) 스크립트 레벨 async 라이브러리(system.async + async.get/wait)를 쓰거나
    // (2) 호스트가 결과를 폴링해 스크립트 콜백을 Call 하는 형태로 설계할 것.

    //--- 마지막 런타임 에러 (init/call 중 발생). 있으면 true+텍스트, 소비(clear)한다. ---
    virtual bool TakeLastError(StringView& out) = 0;
    // 비파괴 읽기(clear 안 함). 여러 곳에서 같은 에러를 봐야 할 때(예: DAP 이벤트+출력).
    virtual bool PeekLastError(StringView& out) const = 0;

    //--- 리플렉션 / 디버거 ---
    virtual void GetBuiltins(std::vector<BuiltinInfo>& out) const = 0;
    virtual IDebugger* GetDebugger() = 0;   // 디버그 빌드가 아니면 nullptr

    //--- 메모리 ---
    // 이미 객체가 풀에 반납된 뒤 비어 있는 페이지를 OS로 돌려준다.
    // 호스트가 씬 전환·로딩 화면·메모리 압박처럼 실제 예약 메모리를 줄이고 싶은
    // 시점에 호출한다. 반환값은 OS로 돌려준 페이지 바이트이며, 아직 사용 중인 객체나
    // 풀의 일부만 비어 있는 페이지는 대상이 아니다.
    //
    //  force=false : 보유 시간(기본 5초)이 지난 빈 페이지를 한 번에
    //                SetTrimPagesPerCall() 장까지만 반환한다.
    //  force=true  : 보유 시간과 페이지 예산을 무시하고 현재 비어 있는 페이지를
    //                모두 반환한다. 맵 전환 같은 명시적 강제 정리 시점용이다.
    virtual int64_t TrimMemory(bool force = false) = 0;
    // 순환 참조 후보를 회수한다. 호출 시점과 빈도는 호스트가 결정한다.
    // force=false : max(16, 후보 전체의 2%)개만 증분 처리한다.
    // force=true  : 후보 큐가 빌 때까지 모두 처리한다(로딩 화면 등 비프레임 시점용).
    // 반환값은 처리한 후보 수다.
    virtual int CollectCycles(bool force = false) = 0;
    // 빈 페이지 보유 시간(초). 0 이면 스윕이 빈 걸 본 즉시 회수 대상이 된다.
    virtual void SetEmptyPageHoldSeconds(float sec) = 0;
    virtual float GetEmptyPageHoldSeconds() const = 0;
    // TrimMemory(false) 한 번이 해제할 페이지 수 상한(기본 4). 회수 속도와 프레임당
    // 비용의 트레이드오프 손잡이다 — 크게 하면 빨리 돌려주고, 작게 하면 더 잘게 나눈다.
    // 0 이하면 상한 없음(예전처럼 한 번에 전부).
    virtual void SetTrimPagesPerCall(int pages) = 0;
    virtual int GetTrimPagesPerCall() const = 0;
};

IRuntime* CreateRuntime(const RuntimeDesc& desc = {});
void      DestroyRuntime(IRuntime* runtime);

// 전역 로그 싱크 — 모든 스크립트 print/error(런타임 무관, 컴파일 진단 포함)를 가로챈다.
// RuntimeDesc.printFn/errorFn 과 **같은** 프로세스-전역 훅을 설정한다(마지막 호출자 우선) — 둘을
// 함께 쓰면 나중에 설정한 쪽이 이긴다. 로그 인프라 정합을 위해 원시 C 문자열 콜백(널 종료 보장)을
// 받는다. 호스트 전역 로깅(에디터 로그 패널 등)에 사용. nullptr 로 해제.
void SetLogHandler(void (*print)(const char* msg), void (*error)(const char* msg));

// VM 전역 메모리 진단 — 살아있는 스크립트 객체 수(디버그 오버레이/누수 추적용, 모든 런타임 합산).
struct AllocStats
{
    int32_t strings = 0;
    int32_t maps = 0;
    int32_t lists = 0;
    int32_t sets = 0;
    int32_t coroutines = 0;
    int32_t modules = 0;
    int32_t asyncs = 0;
    int32_t vectors = 0;   // Vector2/3/4/Quaternion 성분 저장소(VecInfo)
    // VM 메모리풀이 확보해 들고 있는 총 바이트(사용중 + 여유). 풀은 반납해도 페이지를 OS 에
    // 돌려주지 않으므로 이 값이 곧 실제 점유량이다. 모든 런타임의 오브젝트 풀 + 스레드별
    // 실행 컨텍스트 풀(var 스택 포함) 합계.
    // 주의: 컬렉션이 따로 잡는 힙(list/map 의 버킷 배열, 문자열 본문)은 풀 밖이라 포함되지 않는다.
    int64_t poolBytes = 0;
    // 반납되어 놀고 있는 문자열 노드가 아직 들고 있는 문자 버퍼 합계(poolBytes 밖의 힙).
    int64_t stringIdleBytes = 0;
};
void GetAllocStats(AllocStats& out);


} // namespace NeoScript

//==============================================================================
// 사용 예시 (C++17 — 지정초기화 미사용)
//==============================================================================
#if 0
// 반환 컬렉션 빌드 (기존 _tbl->Insert 대체) — 전부 타입드 in-place, owning Value 없음.
static bool NativeRaycast(NeoScript::CallContext& ctx)
{
    NeoScript::MapBuilder m = ctx.retMap();
    m.setBool("Hit", true);
    m.setFloat("Distance", 12.5f);
    m.setVec3("Point", 1, 2, 3);
    return true;
}

void Sample(NeoScript::IRuntime* rt, NeoScript::InstanceHandle a, float deltaTime)
{
    // 호스트→스크립트: 인자를 VM 슬롯에 직접 쓰고 실행 → 반환 in-place 읽기(zero-copy)
    NeoScript::Invocation call = rt->Call(a, "Update");
    if (call.argFloat(deltaTime).timeout(50).invoke() == NeoScript::RunStatus::Completed)
    {
        int score = call.retInt();       // 스칼라 즉시
        (void)score;
        NeoScript::MapReader info;
        if (call.retMap(info))           // 구조화 반환은 리더(핸들 살아있는 동안 유효)
        {
            int hp = 0; info.getInt("hp", hp);
        }
    }
}
#endif
