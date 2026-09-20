#ifndef NEOSCRIPT_C_H
#define NEOSCRIPT_C_H

//==============================================================================
// NeoScript C ABI — NeoScript.h(C++ 파사드) 를 납작한 C 인터페이스로 편 것.
//
// 존재 이유: C# / Unity 처럼 C++ 이름장식·가상함수·std 타입을 넘길 수 없는 호스트가
// P/Invoke 로 VM 을 쓰기 위함이다. 이 헤더는 순수 C 로 파싱되어야 한다 —
// 클래스·템플릿·기본인자·레퍼런스를 쓰지 않는다.
//
// [수명 규칙] NeoScript.h 의 규칙이 그대로 적용된다. 경계 타입은 전부 "살아있는 VM
// 객체 위의 빌린 핸들"이다. 특히:
//   · 빌더/리더/뷰 핸들(NsMapBuilder 등)은 그것을 발급한 스코프 안에서만 유효하다.
//     네이티브 디스패처가 반환하거나, 읽기 콜백이 끝나거나, NsCallEnd 가 불리면 전부 무효.
//   · NsStr 로 받은 문자열은 VM/래퍼 소유다. 호스트는 즉시 복사할 것.
//   · 한 인스턴스에 동시에 살아있는 NsCall 은 하나뿐이다(네이티브→스크립트 중첩 호출은 예외).
//
// [스레드] VM 인스턴스는 스레드 안전하지 않다. 한 인스턴스는 한 스레드에서만 쓴다.
//==============================================================================

#include <stdint.h>
#include <stddef.h>

//------------------------------------------------------------------------------
// 링크 지정자
//   NEOSCRIPT_C_BUILD_SHARED : 공유 라이브러리(.dll/.so)를 빌드할 때 정의
//   NEOSCRIPT_C_USE_SHARED   : 그 공유 라이브러리를 C/C++ 호스트가 쓸 때 정의
//   (iOS 처럼 정적 링크할 때는 둘 다 정의하지 않는다)
//------------------------------------------------------------------------------
#if defined(_WIN32)
#  define NS_CALL __cdecl
#  if defined(NEOSCRIPT_C_BUILD_SHARED)
#    define NS_API __declspec(dllexport)
#  elif defined(NEOSCRIPT_C_USE_SHARED)
#    define NS_API __declspec(dllimport)
#  else
#    define NS_API
#  endif
#else
#  define NS_CALL
#  if defined(NEOSCRIPT_C_BUILD_SHARED)
#    define NS_API __attribute__((visibility("default")))
#  else
#    define NS_API
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// ABI 버전. 구조체 레이아웃이나 함수 시그니처가 바뀌면 올린다. 바인딩(C#)은 기동 시
// NsGetAbiVersion() 으로 확인해서 어긋나면 바로 실패시킬 것 — 조용한 메모리 깨짐보다 낫다.
//------------------------------------------------------------------------------
#define NS_ABI_VERSION 1

// 한 프로세스에서 등록할 수 있는 네이티브 객체 타입 수. 디스패처가 컨텍스트 없는
// 함수 포인터라 슬롯마다 고정 썽크가 필요해서 상한이 있다.
#define NS_MAX_OBJECT_SLOTS 64

//------------------------------------------------------------------------------
// 불투명 핸들
//------------------------------------------------------------------------------
typedef struct NsRuntime     NsRuntime;      // CreateRuntime 산출물
typedef struct NsCall        NsCall;         // 호스트→스크립트 호출(Invocation)
typedef struct NsContext     NsContext;      // 네이티브 디스패처가 받는 CallContext
typedef struct NsFunction    NsFunction;     // 소유하는 FunctionHandle(명시적 해제 필요)
typedef struct NsMapBuilder  NsMapBuilder;
typedef struct NsListBuilder NsListBuilder;
typedef struct NsMapReader   NsMapReader;
typedef struct NsListReader  NsListReader;
typedef struct NsArrayView   NsArrayView;

// {id, generation} 을 하나의 64비트로 접은 값. 0 이면 무효.
typedef uint64_t NsProgram;
typedef uint64_t NsInstance;
typedef uint64_t NsDefineSet;
// 런타임이 보유한 ObjectType 테이블의 1-기반 인덱스. 0 이면 무효.
typedef uint32_t NsObjectType;

//------------------------------------------------------------------------------
// 값 타입 — NeoScript.h 의 enum 과 값이 같아야 한다.
// (NeoScriptC.cpp 에 static_assert 로 못을 박아 두었다)
//------------------------------------------------------------------------------
#define NS_RUN_COMPLETED  0
#define NS_RUN_SUSPENDED  1
#define NS_RUN_YIELDED    2
#define NS_RUN_FAILED     3
#define NS_RUN_CANCELLED  4

#define NS_STATE_IDLE      0
#define NS_STATE_RUNNING   1
#define NS_STATE_SUSPENDED 2
#define NS_STATE_FAILED    3

#define NS_TYPE_NONE     0
#define NS_TYPE_BOOL     1
#define NS_TYPE_INT      2
#define NS_TYPE_FLOAT    3
#define NS_TYPE_STRING   4
#define NS_TYPE_VEC2     5
#define NS_TYPE_VEC3     6
#define NS_TYPE_VEC4     7
#define NS_TYPE_MAP      8
#define NS_TYPE_LIST     9
#define NS_TYPE_ARRAY    10
#define NS_TYPE_SET      11
#define NS_TYPE_FUNCTION 12

#define NS_ELEM_BOOL  0
#define NS_ELEM_INT   1
#define NS_ELEM_FLOAT 2

#define NS_DEFINE_IDENTIFIER 0
#define NS_DEFINE_INT        1
#define NS_DEFINE_FLOAT      2
#define NS_DEFINE_STRING     3
#define NS_DEFINE_BOOL       4
#define NS_DEFINE_NULL       5

//------------------------------------------------------------------------------
// 문자열 뷰. 소유하지 않는다. 입력에서는 data==NULL 이 "없음"이다.
// UTF-8 을 전제한다(널 종료는 보장하지 않으니 len 을 쓸 것).
//------------------------------------------------------------------------------
typedef struct NsStr
{
    const char* data;
    int32_t     len;
    int32_t     _pad;
} NsStr;

typedef struct NsError
{
    int32_t  code;          // 0 이면 에러 없음
    uint32_t line;
    uint32_t column;
    uint32_t _pad;
    NsStr    message;       // 런타임 소유. 같은 런타임에서 다음 에러가 날 때까지 유효
    NsStr    sourceName;
} NsError;

// NsCallInvokeR 결과. 문자열은 이 NsCall 이 해제/재사용될 때까지 유효하다.
typedef struct NsCallResult
{
    int32_t status;         // NS_RUN_*
    int32_t type;           // NS_TYPE_*
    int32_t i;              // int / bool
    int32_t _pad;
    float   f[4];           // float 는 f[0], 벡터는 f[0..3]
    NsStr   str;
} NsCallResult;

typedef struct NsAllocStats
{
    int32_t strings;
    int32_t maps;
    int32_t lists;
    int32_t sets;
    int32_t coroutines;
    int32_t modules;
    int32_t asyncs;
    int32_t vectors;
    int32_t arrays;
    int32_t closures;
    int64_t bufferBytes;
    int64_t poolBytes;
    int64_t stringIdleBytes;
} NsAllocStats;

//------------------------------------------------------------------------------
// 콜백. 전부 호스트(C#)가 구현한다.
// [중요] 이 콜백들은 네이티브 스택 위에서 불린다. 관리 코드 예외가 경계를 넘으면 안 된다 —
// 구현 전체를 try/catch 로 감쌀 것. IL2CPP 에서는 static + [MonoPInvokeCallback] 이어야 한다.
// 문자열은 struct-by-value 의 ABI 위험을 피하려고 포인터+길이 두 인자로 넘긴다.
//------------------------------------------------------------------------------

// 스크립트 print/error. 프로세스 전역 훅이다(NsSetLogHandler 참고).
typedef void (NS_CALL* NsLogFn)(void* user, const char* msg, int32_t len);

// import 해석. 찾으면 NsSinkWrite 로 내용을 써넣고 1 을 반환, 못 찾으면 0.
typedef int32_t (NS_CALL* NsLoadFn)(void* user, const char* path, int32_t pathLen, void* sink);

// 인스턴스 로드 시 바인딩 훅. 여기서 NsBindGlobalObject 등을 부른다.
typedef void (NS_CALL* NsBindFn)(void* user, NsRuntime* rt, NsInstance inst, void* instanceUserData);

// 네이티브 객체 메서드 디스패처. slot = NsRegisterObject 가 돌려준 타입 슬롯.
// 실패 시 NsCtxFail 로 사유를 남기고 0 을 반환할 것.
typedef int32_t (NS_CALL* NsMethodFn)(void* user, int32_t slot, NsContext* ctx,
                                      const char* method, int32_t methodLen);

// 네이티브 객체 프로퍼티 get/set 디스패처. isGet!=0 이면 ctx 에 값을 쓰고, 0 이면 arg0 을 읽는다.
typedef int32_t (NS_CALL* NsPropertyFn)(void* user, int32_t slot, NsContext* ctx,
                                        const char* name, int32_t nameLen, int32_t isGet);

// 컬렉션 반환 읽기 콜백. 핸들은 이 콜백 안에서만 유효하다.
typedef void (NS_CALL* NsReadMapFn)(void* user, NsMapReader* reader);
typedef void (NS_CALL* NsReadListFn)(void* user, NsListReader* reader);
typedef void (NS_CALL* NsReadArrayFn)(void* user, NsArrayView* view);

//------------------------------------------------------------------------------
// 디스크립터
//------------------------------------------------------------------------------
typedef struct NsRuntimeDesc
{
    uint32_t     structSize;    // sizeof(NsRuntimeDesc) — 버전 가드
    uint32_t     _pad;
    void*        user;          // 아래 콜백 전부에 그대로 전달
    NsLoadFn     load;          // NULL 이면 import 미지원
    NsBindFn     onInstanceBind;
    NsMethodFn   method;        // 등록된 모든 객체가 공유하는 디스패처(slot 으로 분기)
    NsPropertyFn property;
    NsStr        libPath;       // 런타임이 복사해 보관
} NsRuntimeDesc;

typedef struct NsObjectDesc
{
    uint32_t structSize;
    int32_t  hasMethod;         // 0 이 아니면 RuntimeDesc.method 를 이 타입에 연결
    int32_t  hasProperty;
    int32_t  declareGlobal;     // 호스트가 전역 심볼을 선언할지
    NsStr    name;
    void*    userData;          // 기본 바인딩 userData
} NsObjectDesc;

typedef struct NsDefine
{
    NsStr   name;
    NsStr   text;
    int32_t kind;               // NS_DEFINE_*
    int32_t _pad;
} NsDefine;

typedef struct NsCompileDesc
{
    uint32_t        structSize;
    int32_t         includeDebugInfo;
    int32_t         emitAsm;
    int32_t         defineCount;
    NsStr           source;
    NsStr           sourceName;
    NsStr           debugSourcePath;
    const NsDefine* defines;    // defineSet 이 있으면 무시
    NsDefineSet     defineSet;
} NsCompileDesc;

typedef struct NsInstanceDesc
{
    uint32_t structSize;
    int32_t  runGlobalInit;
    void*    userData;
} NsInstanceDesc;

//==============================================================================
// 프로세스 전역
//==============================================================================
NS_API uint32_t NS_CALL NsGetAbiVersion(void);

// 모든 런타임의 print/error 를 가로챈다. 마지막 호출자가 이긴다. NULL 로 해제.
NS_API void NS_CALL NsSetLogHandler(NsLogFn print, NsLogFn error, void* user);

NS_API void NS_CALL NsGetAllocStats(NsAllocStats* out);

// NsLoadFn 구현이 읽은 바이트를 돌려줄 때 쓴다. sink 는 콜백이 받은 그 포인터.
NS_API void NS_CALL NsSinkWrite(void* sink, const void* data, int32_t len);

//==============================================================================
// Runtime
//==============================================================================
NS_API NsRuntime* NS_CALL NsRuntimeCreate(const NsRuntimeDesc* desc);
NS_API void       NS_CALL NsRuntimeDestroy(NsRuntime* rt);
NS_API void       NS_CALL NsRuntimeSetLibPath(NsRuntime* rt, const char* path, int32_t len);

// 성공하면 1 을 반환하고 *outSlot 에 타입 슬롯을 채운다. 슬롯이 동나거나 Freeze 이후면 0.
NS_API int32_t      NS_CALL NsRegisterObject(NsRuntime* rt, const NsObjectDesc* desc, int32_t* outSlot);
NS_API void         NS_CALL NsFreezeBindings(NsRuntime* rt);
NS_API NsObjectType NS_CALL NsGetObjectType(NsRuntime* rt, const char* name, int32_t len);

// 마지막 런타임 에러. Take 는 소비하고 Peek 은 남긴다. 있으면 1.
NS_API int32_t NS_CALL NsTakeLastError(NsRuntime* rt, NsStr* out);
NS_API int32_t NS_CALL NsPeekLastError(NsRuntime* rt, NsStr* out);

NS_API int64_t NS_CALL NsTrimMemory(NsRuntime* rt, int32_t force);
NS_API int32_t NS_CALL NsCollectCycles(NsRuntime* rt, int32_t force);
NS_API void    NS_CALL NsSetEmptyPageHoldSeconds(NsRuntime* rt, float sec);
NS_API float   NS_CALL NsGetEmptyPageHoldSeconds(NsRuntime* rt);
NS_API void    NS_CALL NsSetTrimPagesPerCall(NsRuntime* rt, int32_t pages);
NS_API int32_t NS_CALL NsGetTrimPagesPerCall(NsRuntime* rt);

//==============================================================================
// Program
//==============================================================================
// 실패하면 0 을 반환하고 outErr(있으면)을 채운다.
NS_API NsProgram NS_CALL NsCompile(NsRuntime* rt, const NsCompileDesc* desc, NsError* outErr);

// 바이트코드만 만든다. 성공 시 1, *outBytes 는 런타임 소유 버퍼를 가리킨다 —
// 같은 런타임에서 다음 NsCompileToBytecode 를 부를 때까지 유효하니 바로 복사할 것.
NS_API int32_t   NS_CALL NsCompileToBytecode(NsRuntime* rt, const NsCompileDesc* desc,
                                             NsStr* outBytes, NsError* outErr);
NS_API NsProgram NS_CALL NsLoadProgram(NsRuntime* rt, const void* bytes, int32_t len, NsError* outErr);
NS_API void      NS_CALL NsDestroyProgram(NsRuntime* rt, NsProgram program);

NS_API NsDefineSet NS_CALL NsCreateDefineSet(NsRuntime* rt, const NsDefine* defines, int32_t count);
NS_API void        NS_CALL NsDestroyDefineSet(NsRuntime* rt, NsDefineSet set);

// 못 찾으면 NULL. 받은 핸들은 반드시 NsFunctionRelease 로 해제한다.
NS_API NsFunction* NS_CALL NsFindFunction(NsRuntime* rt, NsProgram program, const char* name, int32_t len);
NS_API void        NS_CALL NsFunctionRelease(NsFunction* fn);
NS_API int32_t     NS_CALL NsFunctionValid(NsFunction* fn);

//==============================================================================
// Instance
//==============================================================================
NS_API NsInstance NS_CALL NsCreateInstance(NsRuntime* rt, NsProgram program, const NsInstanceDesc* desc);
NS_API void       NS_CALL NsDestroyInstance(NsRuntime* rt, NsInstance inst);
NS_API int32_t    NS_CALL NsIsAlive(NsRuntime* rt, NsInstance inst);
NS_API int32_t    NS_CALL NsResetInstance(NsRuntime* rt, NsInstance inst);
NS_API int32_t    NS_CALL NsGetState(NsRuntime* rt, NsInstance inst);

NS_API int32_t NS_CALL NsBindObject(NsRuntime* rt, NsInstance inst,
                                    const char* name, int32_t len, void* userData);
NS_API void    NS_CALL NsBindGlobalObject(NsRuntime* rt, NsInstance inst,
                                          const char* globalName, int32_t len,
                                          NsObjectType type, void* userData);
NS_API void    NS_CALL NsBindGlobalMapObject(NsRuntime* rt, NsInstance inst,
                                             const char* mapName, int32_t mapLen,
                                             const char* key, int32_t keyLen,
                                             NsObjectType type, void* userData);

NS_API int32_t NS_CALL NsRunGlobalInit(NsRuntime* rt, NsInstance inst);
NS_API int32_t NS_CALL NsResume(NsRuntime* rt, NsInstance inst, uint32_t instructionBudget, float deltaTime);
NS_API int32_t NS_CALL NsCancel(NsRuntime* rt, NsInstance inst);

NS_API int32_t NS_CALL NsStartSliced(NsRuntime* rt, NsInstance inst,
                                     const char* functionName, int32_t len,
                                     int32_t timeoutMs, uint32_t budget);
NS_API int32_t NS_CALL NsUpdateSliced(NsRuntime* rt, NsInstance inst);
NS_API int32_t NS_CALL NsIsRunning(NsRuntime* rt, NsInstance inst);

NS_API int32_t NS_CALL NsGetGlobalInt(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, int32_t* out);
NS_API int32_t NS_CALL NsGetGlobalFloat(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, float* out);
NS_API int32_t NS_CALL NsGetGlobalString(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, NsStr* out);
NS_API int32_t NS_CALL NsSetGlobalInt(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, int32_t v);
NS_API int32_t NS_CALL NsSetGlobalFloat(NsRuntime* rt, NsInstance inst, const char* name, int32_t len, float v);
NS_API int32_t NS_CALL NsSetGlobalString(NsRuntime* rt, NsInstance inst, const char* name, int32_t len,
                                         const char* v, int32_t vlen);

//==============================================================================
// 호스트 → 스크립트 호출
//
//   NsCall* c = NsCallBeginName(rt, inst, "Update", 6);
//   NsCallArgFloat(c, dt);
//   NsCallResult r; NsCallInvokeR(c, &r);
//   NsCallEnd(c);                       // ← 반드시. 인스턴스의 실행 컨텍스트를 반납한다.
//
// NsCallEnd 는 인스턴스가 파괴되기 전에 불려야 한다.
//==============================================================================
NS_API NsCall* NS_CALL NsCallBegin(NsRuntime* rt, NsInstance inst, NsFunction* fn);
NS_API NsCall* NS_CALL NsCallBeginName(NsRuntime* rt, NsInstance inst, const char* name, int32_t len);
NS_API void    NS_CALL NsCallEnd(NsCall* call);

NS_API void NS_CALL NsCallArgInt(NsCall* call, int32_t v);
NS_API void NS_CALL NsCallArgFloat(NsCall* call, float v);
NS_API void NS_CALL NsCallArgBool(NsCall* call, int32_t v);
NS_API void NS_CALL NsCallArgString(NsCall* call, const char* v, int32_t len);
NS_API void NS_CALL NsCallArgVec3(NsCall* call, float x, float y, float z);
NS_API void NS_CALL NsCallArgObject(NsCall* call, NsObjectType type, void* userData);
NS_API void NS_CALL NsCallTimeout(NsCall* call, int32_t ms);
NS_API void NS_CALL NsCallBudget(NsCall* call, uint32_t ops);

NS_API int32_t NS_CALL NsCallInvoke(NsCall* call);
NS_API int32_t NS_CALL NsCallInvokeR(NsCall* call, NsCallResult* out);
NS_API int32_t NS_CALL NsCallInvokeReadMap(NsCall* call, NsReadMapFn fn, void* user);
NS_API int32_t NS_CALL NsCallInvokeReadList(NsCall* call, NsReadListFn fn, void* user);
NS_API int32_t NS_CALL NsCallInvokeReadArray(NsCall* call, NsReadArrayFn fn, void* user);
NS_API int32_t NS_CALL NsCallStatus(NsCall* call);
NS_API void    NS_CALL NsCallGetError(NsCall* call, NsError* out);

// 저수준 반환 읽기 — 같은 인스턴스에 다시 Call 하면 무효가 된다. 웬만하면 InvokeR 을 쓸 것.
NS_API int32_t NS_CALL NsCallRetType(NsCall* call);
NS_API int32_t NS_CALL NsCallRetInt(NsCall* call);
NS_API float   NS_CALL NsCallRetFloat(NsCall* call);
NS_API int32_t NS_CALL NsCallRetBool(NsCall* call);
NS_API void    NS_CALL NsCallRetString(NsCall* call, NsStr* out);
NS_API void    NS_CALL NsCallRetVec(NsCall* call, float* out4);
NS_API NsMapReader*  NS_CALL NsCallRetMap(NsCall* call);
NS_API NsListReader* NS_CALL NsCallRetList(NsCall* call);
NS_API NsArrayView*  NS_CALL NsCallRetArray(NsCall* call);

//==============================================================================
// CallContext — 네이티브 디스패처 안에서만 유효
//==============================================================================
NS_API void*      NS_CALL NsCtxUserData(NsContext* ctx);
NS_API void*      NS_CALL NsCtxInstanceUserData(NsContext* ctx);
NS_API NsRuntime* NS_CALL NsCtxRuntime(NsContext* ctx);
NS_API NsInstance NS_CALL NsCtxInstance(NsContext* ctx);

NS_API int32_t NS_CALL NsCtxArgCount(NsContext* ctx);
NS_API int32_t NS_CALL NsCtxArgType(NsContext* ctx, int32_t i);
NS_API int32_t NS_CALL NsCtxArgInt(NsContext* ctx, int32_t i);
NS_API float   NS_CALL NsCtxArgFloat(NsContext* ctx, int32_t i);
NS_API int32_t NS_CALL NsCtxArgBool(NsContext* ctx, int32_t i);
NS_API void    NS_CALL NsCtxArgString(NsContext* ctx, int32_t i, NsStr* out);
NS_API void    NS_CALL NsCtxArgVec(NsContext* ctx, int32_t i, float* out4);
NS_API NsMapReader*  NS_CALL NsCtxArgMap(NsContext* ctx, int32_t i);
NS_API NsListReader* NS_CALL NsCtxArgList(NsContext* ctx, int32_t i);
NS_API NsArrayView*  NS_CALL NsCtxArgArray(NsContext* ctx, int32_t i);
NS_API void*         NS_CALL NsCtxArgObjectUserData(NsContext* ctx, int32_t i);
// 스크립트 함수/람다 인자를 보관 가능한 핸들로. NsFunctionRelease 로 해제할 것.
NS_API NsFunction*   NS_CALL NsCtxArgFunction(NsContext* ctx, int32_t i);

NS_API void NS_CALL NsCtxRetInt(NsContext* ctx, int32_t v);
NS_API void NS_CALL NsCtxRetFloat(NsContext* ctx, float v);
NS_API void NS_CALL NsCtxRetBool(NsContext* ctx, int32_t v);
NS_API void NS_CALL NsCtxRetString(NsContext* ctx, const char* v, int32_t len);
NS_API void NS_CALL NsCtxRetVec2(NsContext* ctx, float x, float y);
NS_API void NS_CALL NsCtxRetVec3(NsContext* ctx, float x, float y, float z);
NS_API void NS_CALL NsCtxRetVec4(NsContext* ctx, float x, float y, float z, float w);
NS_API void NS_CALL NsCtxRetNull(NsContext* ctx);
NS_API void NS_CALL NsCtxRetObject(NsContext* ctx, NsObjectType type, void* userData);
NS_API void NS_CALL NsCtxRetInstanceGlobal(NsContext* ctx, NsInstance inst, const char* name, int32_t len);
NS_API NsMapBuilder*  NS_CALL NsCtxRetMap(NsContext* ctx);
NS_API NsListBuilder* NS_CALL NsCtxRetList(NsContext* ctx);

// 실패 사유를 남긴다. 부른 뒤 디스패처는 반드시 0 을 반환해야 한다.
NS_API void NS_CALL NsCtxFail(NsContext* ctx, int32_t code, const char* msg, int32_t len);

//==============================================================================
// 컬렉션 빌더 / 리더 — 발급 스코프 안에서만 유효
//==============================================================================
NS_API void NS_CALL NsMapSetInt(NsMapBuilder* m, const char* k, int32_t klen, int32_t v);
NS_API void NS_CALL NsMapSetFloat(NsMapBuilder* m, const char* k, int32_t klen, float v);
NS_API void NS_CALL NsMapSetBool(NsMapBuilder* m, const char* k, int32_t klen, int32_t v);
NS_API void NS_CALL NsMapSetString(NsMapBuilder* m, const char* k, int32_t klen, const char* v, int32_t vlen);
NS_API void NS_CALL NsMapSetVec3(NsMapBuilder* m, const char* k, int32_t klen, float x, float y, float z);
NS_API void NS_CALL NsMapSetObject(NsMapBuilder* m, const char* k, int32_t klen, NsObjectType t, void* userData);
NS_API NsMapBuilder*  NS_CALL NsMapSetMap(NsMapBuilder* m, const char* k, int32_t klen);
NS_API NsListBuilder* NS_CALL NsMapSetList(NsMapBuilder* m, const char* k, int32_t klen);

NS_API void    NS_CALL NsListReserve(NsListBuilder* l, int32_t count);
NS_API void    NS_CALL NsListResize(NsListBuilder* l, int32_t count);
NS_API int32_t NS_CALL NsListCount(NsListBuilder* l);
NS_API void    NS_CALL NsListPushInt(NsListBuilder* l, int32_t v);
NS_API void    NS_CALL NsListPushFloat(NsListBuilder* l, float v);
NS_API void    NS_CALL NsListPushBool(NsListBuilder* l, int32_t v);
NS_API void    NS_CALL NsListPushString(NsListBuilder* l, const char* v, int32_t len);
NS_API void    NS_CALL NsListPushVec3(NsListBuilder* l, float x, float y, float z);
NS_API void    NS_CALL NsListPushObject(NsListBuilder* l, NsObjectType t, void* userData);
NS_API void    NS_CALL NsListSetInt(NsListBuilder* l, int32_t index, int32_t v);
NS_API void    NS_CALL NsListSetFloat(NsListBuilder* l, int32_t index, float v);
NS_API void    NS_CALL NsListSetString(NsListBuilder* l, int32_t index, const char* v, int32_t len);
NS_API NsMapBuilder*  NS_CALL NsListPushMap(NsListBuilder* l);
NS_API NsListBuilder* NS_CALL NsListPushList(NsListBuilder* l);

NS_API int32_t NS_CALL NsMapHas(NsMapReader* m, const char* k, int32_t klen);
NS_API int32_t NS_CALL NsMapType(NsMapReader* m, const char* k, int32_t klen);
NS_API int32_t NS_CALL NsMapGetInt(NsMapReader* m, const char* k, int32_t klen, int32_t* out);
NS_API int32_t NS_CALL NsMapGetFloat(NsMapReader* m, const char* k, int32_t klen, float* out);
NS_API int32_t NS_CALL NsMapGetBool(NsMapReader* m, const char* k, int32_t klen, int32_t* out);
NS_API int32_t NS_CALL NsMapGetString(NsMapReader* m, const char* k, int32_t klen, NsStr* out);
NS_API int32_t NS_CALL NsMapGetVec(NsMapReader* m, const char* k, int32_t klen, float* out4);
NS_API NsMapReader*  NS_CALL NsMapGetMap(NsMapReader* m, const char* k, int32_t klen);
NS_API NsListReader* NS_CALL NsMapGetList(NsMapReader* m, const char* k, int32_t klen);
NS_API NsArrayView*  NS_CALL NsMapGetArray(NsMapReader* m, const char* k, int32_t klen);

NS_API int32_t NS_CALL NsListRdCount(NsListReader* l);
NS_API int32_t NS_CALL NsListRdType(NsListReader* l, int32_t index);
NS_API int32_t NS_CALL NsListRdGetInt(NsListReader* l, int32_t index, int32_t* out);
NS_API int32_t NS_CALL NsListRdGetFloat(NsListReader* l, int32_t index, float* out);
NS_API int32_t NS_CALL NsListRdGetBool(NsListReader* l, int32_t index, int32_t* out);
NS_API int32_t NS_CALL NsListRdGetString(NsListReader* l, int32_t index, NsStr* out);
NS_API int32_t NS_CALL NsListRdGetVec(NsListReader* l, int32_t index, float* out4);
NS_API NsMapReader*  NS_CALL NsListRdGetMap(NsListReader* l, int32_t index);
NS_API NsListReader* NS_CALL NsListRdGetList(NsListReader* l, int32_t index);
NS_API NsArrayView*  NS_CALL NsListRdGetArray(NsListReader* l, int32_t index);

NS_API int32_t  NS_CALL NsArrayElementType(NsArrayView* a);
NS_API int32_t  NS_CALL NsArrayCount(NsArrayView* a);
NS_API uint8_t* NS_CALL NsArrayBoolBits(NsArrayView* a);
NS_API int32_t* NS_CALL NsArrayInts(NsArrayView* a);
NS_API float*   NS_CALL NsArrayFloats(NsArrayView* a);

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif   /* NEOSCRIPT_C_H */
