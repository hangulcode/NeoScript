// NeoScript v2 공개 API(shim) 런타임 스모크 — 유니파이드 핸들 모델(owning Value 없음).
// 객체 디스패치 / retMap 빌더 / reader / per-instance userData / 프로그램 공유 / zero-copy 를 실행 검증.
#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"
#include "../../NeoSource/NeoVM.h"   // 누수 회귀용 alloc 카운터(SNeoVMAllocStats) — 공개 API 에는 없다

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;

static int g_fail = 0;
static FunctionHandle g_deferredCallback;
static int g_nestedFailureCalls = 0;
static void Check(bool cond, const char* msg)
{
    printf(cond ? "  ok  : %s\n" : "  FAIL: %s\n", msg);
    if (!cond) ++g_fail;
}
static bool Eq(StringView s, const char* c) { return std::string(s.data(), s.size()) == c; }

// 디버거 검증용 리스너: 정지 시 스택 조회 후 Continue.
struct DbgListener : IDebugListener
{
    IDebugger* dbg = nullptr;
    int stops = 0;
    DebugLocation loc;
    bool stack = false;
    void OnStopped(InstanceHandle i, const DebugLocation& l, DebugStopReason) override
    {
        ++stops; loc = l;
        if (dbg) { std::vector<DebugStackFrame> f; dbg->GetStackTrace(i, f); stack = !f.empty(); dbg->Continue(i); }
    }
};

// Host 객체 메서드 디스패처 — 전부 타입드 in-place(zero-copy).
// setObject 검증용 중첩 객체 타입. userData(=인스턴스 포인터)를 id 로 노출.
static bool EntityMethod(CallContext& ctx, StringView method)
{
    std::string m(method.data(), method.size());
    if (m == "id")  { ctx.retInt(static_cast<int32_t>(reinterpret_cast<intptr_t>(ctx.userData()))); return true; }
    if (m == "tag") { ctx.retString("entity"); return true; }
    return false;
}

static bool HostMethod(CallContext& ctx, StringView method)
{
    std::string m(method.data(), method.size());
    if (m == "add")
    {
        ctx.retInt(ctx.argInt(0) + ctx.argInt(1));
        return true;
    }
    if (m == "uid")
    {
        ctx.retInt(static_cast<int32_t>(reinterpret_cast<intptr_t>(ctx.userData())));
        return true;
    }
    if (m == "info")
    {
        MapBuilder mb = ctx.retMap();
        mb.setString("name", "neo");
        mb.setInt("value", 42);
        mb.setVec3("vec", 1.0f, 2.0f, 3.0f);
        ListBuilder lb = mb.setList("items");
        lb.pushInt(7); lb.pushInt(8); lb.pushInt(9);
        return true;
    }
    if (m == "vec2")
    {
        ctx.retVec2(8.0f, 9.0f);
        return true;
    }
    if (m == "boomHost")
    {
        // 네이티브 실패 보고: fail() 로 사유를 남기고 반드시 false 반환.
        ctx.fail(4242, "host said no");
        return false;
    }
    if (m == "nestedFailureOnce")
    {
        if (g_nestedFailureCalls++ == 0)
        {
            CallResult nested = ctx.runtime()->Call(ctx.instance(), "nestedChildFailure").invokeR();
            if (!nested.ok())
            {
                ctx.fail(4243, "nested script failed");
                return false;
            }
        }
        ctx.retBool(true);
        return true;
    }
    if (m == "defer")
    {
        g_deferredCallback = ctx.argFunction(0);
        ctx.retBool(static_cast<bool>(g_deferredCallback));
        return static_cast<bool>(g_deferredCallback);
    }
    if (m == "spawn")
    {
        // 반환 맵에 바인딩된 Entity 객체를 중첩(setObject). 스크립트가 r["entity"].id() 호출 가능.
        MapBuilder mb = ctx.retMap();
        ObjectType et = ctx.runtime()->GetObjectType("Entity");
        mb.setObject("entity", et, reinterpret_cast<void*>(static_cast<intptr_t>(123)));
        mb.setInt("count", 1);
        return true;
    }
    return false;
}

int NeoScriptV2Smoke()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== NeoScript v2 shim smoke (unified handle) ===\n");
    g_fail = 0;

    RuntimeDesc rdesc;
    rdesc.printFn = [](StringView s) { printf("[script] %.*s\n", static_cast<int>(s.size()), s.data()); };
    IRuntime* rt = CreateRuntime(rdesc);

    NativeObjectDesc od;
    od.name = "Host";
    od.method = &HostMethod;
    rt->RegisterObject(od);
    NativeObjectDesc ed;               // 중첩 전용 타입(declareGlobal=false → 전역 심볼 없음)
    ed.name = "Entity";
    ed.method = &EntityMethod;
    rt->RegisterObject(ed);
    rt->FreezeBindings();

    // UTF-8 source must produce the same UTF-16 parser input on Windows and
    // Linux. Keep the bytes escaped so this test does not depend on the C++
    // source-file code page.
    const char* unicodeText = "\xED\x95\x9C\xEA\xB8\x80 \xF0\x9F\x98\x80"; // 한글 😀
    const char* unicodeSource =
        "export fun unicodeText() { return \""
        "\xED\x95\x9C\xEA\xB8\x80 \xF0\x9F\x98\x80"
        "\"; }\n"
        "export fun unicodeFirst() { foreach (var c in \"\xF0\x9F\x98\x80\") { return c; } return \"\"; }\n"
        "export fun unicodeConcat() { foreach (var c in \"\xF0\x9F\x98\x80\") { if (c == \"\xF0\x9F\x98\x80\" && c >= \"\xF0\x9F\x98\x80\") return c + c; } return \"\"; }\n"
        "export fun unicodeMapKey() { var m = {}; foreach (var c in \"\xF0\x9F\x98\x80\") { m[c] = 77; } return m[\"\xF0\x9F\x98\x80\"]; }\n"
        "export fun unicodeSize() { return tosize(\"\xED\x95\x9C\xEA\xB8\x80 \xF0\x9F\x98\x80\"); }\n";
    CompileDesc unicodeDesc;
    unicodeDesc.source = unicodeSource;
    unicodeDesc.sourceName = "utf8_unicode_smoke.ns";
    CompileResult unicodeProgram = rt->Compile(unicodeDesc);
    Check(static_cast<bool>(unicodeProgram.program), "UTF-8 Korean/emoji source compiles");
    if (unicodeProgram.program)
    {
        InstanceHandle unicodeInstance = rt->CreateInstance(unicodeProgram.program);
        Invocation unicodeCall = rt->Call(unicodeInstance, "unicodeText");
        Check(unicodeCall.invoke() == RunStatus::Completed && Eq(unicodeCall.retString(), unicodeText),
            "UTF-8 Korean/emoji source round-trips through parser");
        const char* emoji = "\xF0\x9F\x98\x80";
        Invocation unicodeFirst = rt->Call(unicodeInstance, "unicodeFirst");
        Check(unicodeFirst.invoke() == RunStatus::Completed && Eq(unicodeFirst.retString(), emoji),
            "UTF-8 four-byte foreach character returns safely");
        Invocation unicodeConcat = rt->Call(unicodeInstance, "unicodeConcat");
        Check(unicodeConcat.invoke() == RunStatus::Completed && Eq(unicodeConcat.retString(), "\xF0\x9F\x98\x80\xF0\x9F\x98\x80"),
            "UTF-8 foreach character compares and concatenates safely");
        Invocation unicodeMapKey = rt->Call(unicodeInstance, "unicodeMapKey");
        Check(unicodeMapKey.invoke() == RunStatus::Completed && unicodeMapKey.retInt() == 77,
            "UTF-8 foreach character is a normal string map key");
        Invocation unicodeSize = rt->Call(unicodeInstance, "unicodeSize");
        Check(unicodeSize.invoke() == RunStatus::Completed && unicodeSize.retInt() == 4,
            "tosize(string) returns UTF-8 character count like string.len");
        rt->DestroyInstance(unicodeInstance);
        rt->DestroyProgram(unicodeProgram.program);
    }

    // Legacy CP949 comments have no script meaning and may be ignored, but
    // the same invalid bytes in a string literal must still be rejected.
    const char* legacyCommentSource =
        "// \xB0\xA1\n"
        "export fun legacyComment() { return 7; }\n";
    CompileDesc legacyCommentDesc;
    legacyCommentDesc.source = legacyCommentSource;
    legacyCommentDesc.sourceName = "legacy_comment_smoke.ns";
    CompileResult legacyCommentProgram = rt->Compile(legacyCommentDesc);
    Check(static_cast<bool>(legacyCommentProgram.program), "legacy non-UTF-8 comment is ignored");
    if (legacyCommentProgram.program)
    {
        InstanceHandle legacyCommentInstance = rt->CreateInstance(legacyCommentProgram.program);
        Invocation legacyCommentCall = rt->Call(legacyCommentInstance, "legacyComment");
        Check(legacyCommentCall.invoke() == RunStatus::Completed && legacyCommentCall.retInt() == 7,
            "legacy non-UTF-8 comment does not alter script execution");
        rt->DestroyInstance(legacyCommentInstance);
        rt->DestroyProgram(legacyCommentProgram.program);
    }

    const char* invalidStringSource = "export fun invalidText() { return \"\xB0\xA1\"; }\n";
    CompileDesc invalidStringDesc;
    invalidStringDesc.source = invalidStringSource;
    invalidStringDesc.sourceName = "invalid_encoding_smoke.ns";
    CompileResult invalidStringProgram = rt->Compile(invalidStringDesc);
    Check(!invalidStringProgram.program, "non-UTF-8 string literal is rejected");
    if (invalidStringProgram.program)
        rt->DestroyProgram(invalidStringProgram.program);

    const char* src =
        "export fun compute(var a, var b)\n"        // 1
        "{\n"                                        // 2
        "    return Host.add(a, b);\n"               // 3
        "}\n"                                        // 4
        "export fun uid() { return Host.uid(); }\n"  // 5
        "export fun info() { return Host.info(); }\n"  // 6
        "export fun spawn() { var r = Host.spawn(); return r[\"entity\"].id(); }\n"  // 7
        "export fun spawnTag() { var r = Host.spawn(); return r[\"entity\"].tag(); }\n"  // 8
        "export fun boom() { var z = 0; return 10 / z; }\n"                              // 9 (런타임 에러)
        "export fun callBoom() { return Host.boomHost(); }\n"                            // 10 (네이티브 fail)
        "export fun vec2() { return Host.vec2(); }\n"                                    // 11
        "export fun badNativeRead() { return Host[3]; }\n"                               // 12
        "export fun badNativeWrite() { Host[3] = 1; }\n"                                  // 13
        "fun closureExplode() { return Host.boomHost(); }\n"
        "fun namedClosureChild(var value) { return value + 1; }\n"
        "export fun nestedChildFailure() { var z = 0; return 1 / z; }\n"
        "export fun queueCounter(var start) { var value = start; Host.defer(fun() { value = value + 1; return value; }); }\n"
        "export fun queueNamedCallCounter(var start) { var value = start; Host.defer(fun() { value = value + 1; namedClosureChild(value); return value; }); }\n"
        "export fun queueStaticReturnCounter(var start) { var value = start; Host.defer(fun() { value = value + 1; if (value == 11) return \"FIRST\"; return \"NEXT\"; }); }\n"
        "export fun queueNestedFailureClosure() { var tag = \"KEEP-ME\"; var n = 0; Host.defer(fun() { n = n + 1; Host.nestedFailureOnce(); return tag .. \"/\" .. n; }); }\n"
        "export fun queueStringCounter() { var text = \"a\"; Host.defer(fun() { text = text + \"x\"; return text.len(); }); }\n"
        "export fun queueErrorCounter(var start) { var value = start; Host.defer(fun() { value = value + 1; if (value == 11) closureExplode(); return value; }); }\n"
        "export fun queueSleepCounter(var start) { var value = start; Host.defer(fun() { value = value + 1; if (value == 11) { sleep(1); } return value; }); }\n"
        "export fun nestedCapture() { var v = \"V-outer\"; var mid = fun() { var pad1 = 111; var pad2 = 222; return fun() { return v; }; }; var inner = mid(); return inner(); }\n"
        "export fun selfCaptureSource() { var f = \"some-long-heap-string-value\"; f = fun() { return f; }; return f(); }\n"
        "export fun sortCapture() { var direction = 1; var m = { 5, 3, 7 }; m.sort(fun(var a, var b) { return a * direction > b * direction; }); return m[0] * 100 + m[1] * 10 + m[2]; }\n"
        "fun PlainSortCmp(var a, var b) { return a < b; }\n"
        "export fun namedSortInsideClosure() { var tag = \"KEEP-ME\"; var n = 0; var f = fun() { n = n + 1; var m = { 3, 1, 2 }; m.sort(PlainSortCmp); return tag .. \"/\" .. n; }; var first = f(); var second = f(); return first .. \"|\" .. second; }\n";
    // ※ 새 함수는 반드시 뒤에 덧붙일 것 — 아래 디버거 테스트가 line 3(compute 본문)에 BP 를 건다.

    CompileDesc cd;
    cd.source = src;
    cd.sourceName = "smoke.ns";
    cd.includeDebugInfo = true;   // 디버거(브레이크포인트/실행라인)용
    CompileResult cr = rt->Compile(cd);
    Check(static_cast<bool>(cr.program), "compile");
    if (!cr.program) { printf("  compile error: %s\n", cr.error.message.c_str()); DestroyRuntime(rt); return 1; }

    InstanceHandle a = rt->CreateInstance(cr.program);
    InstanceHandle b = rt->CreateInstance(cr.program);
    Check(static_cast<bool>(a) && static_cast<bool>(b), "create 2 instances (shared program)");
    rt->BindObject(a, "Host", reinterpret_cast<void*>(static_cast<intptr_t>(7)));
    rt->BindObject(b, "Host", reinterpret_cast<void*>(static_cast<intptr_t>(99)));

    // 스칼라 인자/반환 + 객체 메서드 디스패치 (인자 VM 슬롯 직접 write, 반환 in-place read)
    {
        Invocation call = rt->Call(a, "compute");
        RunStatus st = call.argInt(10).argInt(20).invoke();
        Check(st == RunStatus::Completed, "compute status Completed");
        Check(call.retInt() == 30, "Host.add(10,20) == 30");
    }

    {
        g_deferredCallback = FunctionHandle();
        Check(rt->Call(a, "queueCounter").argInt(10).invoke() == RunStatus::Completed,
            "closure callback: script creates and host retains lambda");
        Check(static_cast<bool>(g_deferredCallback), "closure callback: FunctionHandle is valid");
        CallResult one = rt->Call(a, g_deferredCallback).invokeR();
        CallResult two = rt->Call(a, g_deferredCallback).invokeR();
        Check(one.ok() && one.asInt() == 11, "closure callback: first delayed call sees captured 10");
        Check(two.ok() && two.asInt() == 12, "closure callback: second delayed call preserves updated capture");
        g_deferredCallback = FunctionHandle();
    }

    // 캡처 람다 안의 일반 함수 CALL은 RET에서 부모 closure를 복원해야 하며,
    // 바깥 RET_CLOSURE가 그 갱신값을 보관함에 동기화한다.
    {
        g_deferredCallback = FunctionHandle();
        Check(rt->Call(a, "queueNamedCallCounter").argInt(10).invoke() == RunStatus::Completed,
            "closure named call: script creates retained lambda");
        CallResult one = rt->Call(a, g_deferredCallback).invokeR();
        CallResult two = rt->Call(a, g_deferredCallback).invokeR();
        Check(one.ok() && one.asInt() == 11, "closure named call: first callback restores outer closure");
        Check(two.ok() && two.asInt() == 12, "closure named call: second callback preserves updated capture");
        g_deferredCallback = FunctionHandle();
    }

    // 캡처 람다의 static 반환도 RET_CLOSURE 경로에서 보관함을 동기화한다.
    {
        g_deferredCallback = FunctionHandle();
        Check(rt->Call(a, "queueStaticReturnCounter").argInt(10).invoke() == RunStatus::Completed,
            "closure static return: script creates retained lambda");
        CallResult one = rt->Call(a, g_deferredCallback).invokeR();
        CallResult two = rt->Call(a, g_deferredCallback).invokeR();
        Check(one.ok() && Eq(one.asString(), "FIRST"), "closure static return: RET_CLOSURE returns first static value");
        Check(two.ok() && Eq(two.asString(), "NEXT"), "closure static return: RET_CLOSURE keeps captured state");
        g_deferredCallback = FunctionHandle();
    }

    // 람다 -> native -> 중첩 스크립트 오류 뒤에는 부모 프레임이 무효다. 첫 호출의
    // write-back을 버리고, 다음 호출이 원래 보관함에서 다시 시작해야 한다.
    {
        g_nestedFailureCalls = 0;
        g_deferredCallback = FunctionHandle();
        Check(rt->Call(a, "queueNestedFailureClosure").invoke() == RunStatus::Completed,
            "nested error: script creates retained lambda");
        CallResult failed = rt->Call(a, g_deferredCallback).invokeR();
        CallResult after = rt->Call(a, g_deferredCallback).invokeR();
        Check(!failed.ok(), "nested error: inner script failure aborts outer lambda");
        Check(after.ok() && std::string(after.asString().data(), after.asString().size()) == "KEEP-ME/1",
            "nested error: invalid outer frame does not overwrite closure captures");
        g_deferredCallback = FunctionHandle();
    }

    // 반환 슬롯이 아닌 alloc 캡처는 RET에서 closure 보관함으로 소유권을 넘긴다.
    // 두 번째 호출도 같은 문자열 상태를 이어야 한다.
    {
        g_deferredCallback = FunctionHandle();
        Check(rt->Call(a, "queueStringCounter").invoke() == RunStatus::Completed,
            "closure transfer: script creates retained string lambda");
        CallResult one = rt->Call(a, g_deferredCallback).invokeR();
        CallResult two = rt->Call(a, g_deferredCallback).invokeR();
        Check(one.ok() && one.asInt() == 2, "closure transfer: first string capture update");
        Check(two.ok() && two.asInt() == 3, "closure transfer: alloc capture ownership persists");
        g_deferredCallback = FunctionHandle();
    }

    {
        CallResult nested = rt->Call(a, "nestedCapture").invokeR();
        Check(nested.ok() && std::string(nested.asString().data(), nested.asString().size()) == "V-outer",
            "closure nested capture uses each direct parent frame slot");
        CallResult self = rt->Call(a, "selfCaptureSource").invokeR();
        Check(self.ok() && std::string(self.asString().data(), self.asString().size()) == "some-long-heap-string-value",
            "closure creation copies alloc capture before replacing destination");
        CallResult sorted = rt->Call(a, "sortCapture").invokeR();
        Check(sorted.ok() && sorted.asInt() == 753,
            "map.sort invokes captured closure with its stored values");
        CallResult namedSort = rt->Call(a, "namedSortInsideClosure").invokeR();
        Check(namedSort.ok() && std::string(namedSort.asString().data(), namedSort.asString().size()) == "KEEP-ME/1|KEEP-ME/2",
            "map.sort named function does not overwrite active closure captures");
    }

    {
        const char* coroutineSrc =
            "import coroutine;\n"
            "var captured = 0;\n"
            "export fun closureCoroutine() { var v = 37; var co = coroutine.create(fun() { captured = v; yield; captured = captured + v; }); coroutine.resume(co); coroutine.resume(co); return captured; }\n";
        CompileDesc coroutineDesc;
        coroutineDesc.source = coroutineSrc;
        coroutineDesc.sourceName = "closure_coroutine.ns";
        CompileResult coroutineProgram = rt->Compile(coroutineDesc);
        Check(static_cast<bool>(coroutineProgram.program), "coroutine closure: compile");
        if (coroutineProgram.program)
        {
            InstanceHandle coroutineInstance = rt->CreateInstance(coroutineProgram.program);
            CallResult coroutine = rt->Call(coroutineInstance, "closureCoroutine").invokeR();
            Check(coroutine.ok() && coroutine.asInt() == 74,
                "coroutine.create preserves captured closure values across yield/resume");
            rt->DestroyInstance(coroutineInstance);
            rt->DestroyProgram(coroutineProgram.program);
        }
    }

    // 오류 unwind도 정상 RET처럼 부모 람다 프레임의 변경분을 closure로 반영해야 한다.
    // closureExplode는 람다 안에서 일반 스크립트 함수를 한 단계 더 호출해, 상위 스택의
    // active closure 동기화까지 검증한다.
    {
        Check(rt->Call(a, "queueErrorCounter").argInt(10).invoke() == RunStatus::Completed,
            "closure error: script creates retained lambda");
        CallResult failed = rt->Call(a, g_deferredCallback).invokeR();
        CallResult after = rt->Call(a, g_deferredCallback).invokeR();
        Check(!failed.ok(), "closure error: first delayed call fails after increment");
        Check(after.ok() && after.asInt() == 12, "closure error: unwind preserved captured increment");
        g_deferredCallback = FunctionHandle();
    }

    // timeout 모드의 sleep은 컨텍스트를 retain한다. Resume 후에도 active closure가
    // 살아 있어야 RET가 값을 되쓰고 두 번째 지연 호출이 12를 반환한다.
    {
        Check(rt->Call(a, "queueSleepCounter").argInt(10).invoke() == RunStatus::Completed,
            "closure sleep: script creates retained lambda");
        Invocation suspended = rt->Call(a, g_deferredCallback);
        suspended.timeout(10);
        Check(suspended.invoke() == RunStatus::Suspended, "closure sleep: delayed call suspends");
        ::Sleep(5);
        Check(rt->Resume(a) == RunStatus::Completed, "closure sleep: resume completes closure");
        Invocation afterResume = rt->Call(a, g_deferredCallback);
        afterResume.timeout(10);
        Check(afterResume.invoke() == RunStatus::Completed && afterResume.retInt() == 12,
            "closure sleep: resume preserved capture for next delayed call");
        g_deferredCallback = FunctionHandle();
    }

    // Cancel도 RET를 밟지 않으므로 같은 정리가 필요하다. 취소 직전의 value=11을
    // closure에 동기화해야 다음 호출이 12로 이어진다.
    {
        Check(rt->Call(a, "queueSleepCounter").argInt(10).invoke() == RunStatus::Completed,
            "closure cancel: script creates retained lambda");
        Invocation suspended = rt->Call(a, g_deferredCallback);
        suspended.timeout(10);
        Check(suspended.invoke() == RunStatus::Suspended, "closure cancel: delayed call suspends");
        Check(rt->Cancel(a), "closure cancel: cancels suspended closure");
        Invocation afterCancel = rt->Call(a, g_deferredCallback);
        afterCancel.timeout(10);
        Check(afterCancel.invoke() == RunStatus::Completed && afterCancel.retInt() == 12,
            "closure cancel: capture survives cancel cleanup");
        g_deferredCallback = FunctionHandle();
    }

    // per-instance userData (BindObject)
    Check(rt->Call(a, "uid").invoke() == RunStatus::Completed, "uid a status");
    {
        Invocation ua = rt->Call(a, "uid"); ua.invoke();
        Invocation ub = rt->Call(b, "uid"); ub.invoke();
        Check(ua.retInt() == 7, "instance a Host.uid() == 7");
        Check(ub.retInt() == 99, "instance b Host.uid() == 99 (per-instance userData)");
    }

    // retMap 빌더 + reader (구조화 반환, zero-copy 뷰)
    {
        Invocation call = rt->Call(a, "info");
        RunStatus st = call.invoke();
        Check(st == RunStatus::Completed, "info status Completed");
        MapReader info;
        Check(call.retMap(info), "info returns a map");
        if (st == RunStatus::Completed)
        {
            StringView nm;
            Check(info.getString("name", nm) && Eq(nm, "neo"), "info.name == \"neo\"");
            int32_t vv = 0;
            Check(info.getInt("value", vv) && vv == 42, "info.value == 42");
            float vec[4] = {-1,-1,-1,-1};
            Check(info.getVec("vec", vec) && vec[0]==1.0f && vec[1]==2.0f && vec[2]==3.0f && vec[3]==0.0f, "info.vec == (1,2,3,0)");
            ListReader items;
            Check(info.getList("items", items) && items.count() == 3, "info.items has 3 elements");
            if (items.count() == 3)
            {
                int32_t i0 = 0, i2 = 0;
                items.getInt(0, i0); items.getInt(2, i2);
                Check(i0 == 7 && i2 == 9, "info.items[0]==7, [2]==9");
            }
        }
    }

    // 내부 Vec2 setter는 x/y만 쓰되, 공개 retVec(float[4]) 계약은 나머지 두 레인을 0으로 보정한다.
    {
        Invocation call = rt->Call(a, "vec2");
        Check(call.invoke() == RunStatus::Completed, "vec2 status Completed");
        float vec[4] = {-1,-1,-1,-1};
        call.retVec(vec);
        Check(vec[0] == 8.0f && vec[1] == 9.0f && vec[2] == 0.0f && vec[3] == 0.0f,
            "vec2 public read == (8,9,0,0)");
    }

    // 빌더 키 누수 회귀: setList/setMap/setObject 는 키 StringInfo 를 임시로 만들어 Insert 한다.
    // Insert 는 키를 공유(incref)할 뿐이라 로컬 참조를 놓지 않으면 맵이 해제돼도 키가 영원히 남는다.
    // (Resource.LoadJson 처럼 키가 수만 개인 경로에서 그대로 수만 개 누수로 드러났다)
    {
        SNeoVMAllocStats before{}, after{};
        rt->Call(a, "info").invoke();
        rt->Call(a, "spawn").invoke();
        GetNeoVMAllocStats(before);
        for (int i = 0; i < 200; ++i)
        {
            rt->Call(a, "info").invoke();
            rt->Call(a, "spawn").invoke();
        }
        GetNeoVMAllocStats(after);
        printf("  [pool] %lld bytes reserved (object pools + exec context pool)\n", after.poolBytes);
        Check(after.strings <= before.strings && after.vectors <= before.vectors && after.maps <= before.maps
              && after.lists <= before.lists, "builders do not leak across 200 calls");
        if (after.strings > before.strings || after.vectors > before.vectors || after.maps > before.maps || after.lists > before.lists)
            printf("  str %d->%d  vec %d->%d  map %d->%d  lst %d->%d\n",
                   before.strings, after.strings, before.vectors, after.vectors,
                   before.maps, after.maps, before.lists, after.lists);
    }

    // 안전 반환(invokeR): 스칼라를 값으로 스냅샷 → 두 결과를 동시에 들고 있어도 서로 안 깨진다.
    // (구 retInt 라면 두 번째 Call 이 첫 반환 컨텍스트를 flush 해 첫 값이 무효화되던 GetScore/GetHp 시나리오)
    {
        CallResult first  = rt->Call(a, "compute").argInt(40).argInt(60).invokeR(); // 100
        CallResult second = rt->Call(a, "compute").argInt(3).argInt(4).invokeR();    // 7
        Check(first.ok() && second.ok(), "invokeR both Completed");
        Check(first.asInt() == 100, "invokeR first keeps 100 after a second Call (overlap-safe)");
        Check(second.asInt() == 7,  "invokeR second == 7");
    }

    // 안전 반환(invokeReadMap): 컬렉션은 콜백 스코프 안에서만 읽는다
    {
        int32_t value = 0; bool nameOk = false;
        RunStatus st = rt->Call(a, "info").invokeReadMap([&](MapReader m){
            StringView nm; nameOk = m.getString("name", nm) && Eq(nm, "neo");
            m.getInt("value", value);
        });
        Check(st == RunStatus::Completed, "invokeReadMap status Completed");
        Check(nameOk && value == 42, "invokeReadMap read info.name/value inside scope");
    }

    // 실패 호출은 이전 성공값을 누출하지 않는다: 성공(100) 직후 실패 호출 → CallResult 기본 무효값(0)
    {
        CallResult good = rt->Call(a, "compute").argInt(40).argInt(60).invokeR(); // 100 (성공)
        CallResult bad  = rt->Call(a, "boom").invokeR();                          // 런타임 에러
        Check(good.ok() && good.asInt() == 100, "invokeR success-before-failure keeps 100");
        Check(!bad.ok(), "invokeR failed call is not ok()");
        Check(bad.asInt() == 0, "invokeR failed call returns default 0 (no stale return leaked)");
    }
    // invokeReadMap: 실패면 콜백이 아예 호출되지 않는다(이전 map 누출 방지)
    {
        bool called = false;
        RunStatus st = rt->Call(a, "boom").invokeReadMap([&](MapReader){ called = true; });
        Check(st != RunStatus::Completed && !called, "invokeReadMap on failure: callback not invoked");
    }

    // 네이티브 객체 프로퍼티 이름은 문자열만 허용한다. 숫자 키가 VarInfo 유니온의 _str로
    // 해석되면 크래시하므로, 읽기와 쓰기 모두 런타임 오류로 끝나야 한다.
    {
        Invocation read = rt->Call(a, "badNativeRead");
        Check(read.invoke() == RunStatus::Failed
              && read.error().message.find("native object property name must be a string") != std::string::npos,
              "native property numeric read reports an error");
        Invocation write = rt->Call(a, "badNativeWrite");
        Check(write.invoke() == RunStatus::Failed
              && write.error().message.find("native object property name must be a string") != std::string::npos,
              "native property numeric write reports an error");
    }

    // 저수준 소유권(토큰): 이전 Invocation 의 소멸자가 "나중 호출"의 pending 컨텍스트를 닫으면 안 된다.
    // prev 를 heap 에 둬서 next 가 살아있는 동안 먼저 파괴 → 토큰 없으면 next 의 pending 이 조기 종료돼 깨짐.
    {
        Invocation* prev = new Invocation(rt->Call(a, "compute"));
        prev->argInt(1).argInt(2).invoke();          // prev pending (=3)
        Invocation next = rt->Call(a, "compute");
        next.argInt(40).argInt(60).invoke();         // next pending (=100); prev 의 pending 은 이 Call 에서 이미 flush
        delete prev;                                 // prev 소멸 — next 의 pending 을 건드리면 안 됨
        Check(next.retInt() == 100, "low-level: prev dtor does not close next's pending (ownership token)");
    }

    // setObject: 반환 맵에 중첩된 바인딩 객체를 스크립트가 메서드 호출(호출 종료 후에도 유효)
    {
        Invocation call = rt->Call(a, "spawn"); call.invoke();
        Check(call.status() == RunStatus::Completed, "spawn status Completed");
        Check(call.retInt() == 123, "nested Entity.id() == 123 (setObject userData)");
    }
    {
        Invocation call = rt->Call(a, "spawnTag"); call.invoke();
        StringView tag;
        Check(call.status() == RunStatus::Completed && call.retString().size() > 0, "spawnTag status");
        Check(Eq(call.retString(), "entity"), "nested Entity.tag() == \"entity\"");
    }

    // CallContext::fail() → Invocation::error(): 네이티브가 남긴 코드/메시지가 그대로 전달된다
    {
        Invocation call = rt->Call(a, "callBoom");
        RunStatus st = call.invoke();
        const Error& e = call.error();
        printf("  [error] code=%d msg=%s\n", e.code, e.message.c_str());
        Check(st == RunStatus::Failed, "native fail(): status Failed");
        Check(!e.ok() && e.code == 4242, "native fail(): error().code == host code");
        Check(e.message.find("host said no") != std::string::npos, "native fail(): host message kept (not \"invalid call\")");
        // 위치는 이 호출(callBoom, line 10)이어야 한다 — 직전 boom(line 9) 에러 위치가 새면 안 됨.
        Check(e.message.find("Line(10)") != std::string::npos, "native fail(): location is this call (line 10)");
        Check(e.message.find("callBoom") != std::string::npos, "native fail(): stack trace names callBoom");
    }
    // VM 런타임 에러(0 나누기): code 1 + 상세 메시지
    {
        Invocation call = rt->Call(a, "boom");
        Check(call.invoke() == RunStatus::Failed, "runtime error: status Failed");
        Check(call.error().code == 1 && !call.error().message.empty(), "runtime error: code 1 + message");
    }
    // 성공 호출은 직전 에러를 물고 오지 않는다(VM 의 sticky 에러가 호출마다 초기화되는지)
    {
        Invocation call = rt->Call(a, "compute");
        call.argInt(1).argInt(2).invoke();
        Check(call.error().ok(), "success after failure: error() is empty (no sticky error)");
        Check(call.retInt() == 3, "success after failure: still returns 3");
    }

    // ResetInstance: 핸들을 유지한 채 전역 초기화 재실행 + BindObject userData 보존
    {
        const char* rsrc =
            "var counter = 5;\n"
            "export fun bump() { counter = counter + 1; return counter; }\n"
            "export fun get() { return counter; }\n"
            "export fun holdLambda() { var n = 10; Host.defer(fun() { return n; }); }\n"
            "export fun uid2() { return Host.uid(); }\n";
        CompileDesc rcd; rcd.source = rsrc; rcd.sourceName = "reset.ns";
        CompileResult rcr = rt->Compile(rcd);
        Check(static_cast<bool>(rcr.program), "reset: compile");
        if (rcr.program)
        {
            InstanceHandle r = rt->CreateInstance(rcr.program);
            rt->BindObject(r, "Host", reinterpret_cast<void*>(static_cast<intptr_t>(55)));
            FunctionHandle getFn = rt->FindFunction(rcr.program, "get");
            g_deferredCallback = FunctionHandle();
            Check(rt->Call(r, "holdLambda").invoke() == RunStatus::Completed && static_cast<bool>(g_deferredCallback),
                "reset: retained closure FunctionHandle valid before reset");
            FunctionHandle capturedBeforeReset = g_deferredCallback;
            rt->Call(r, "bump").invoke();
            Check(rt->Call(r, "get").invokeR().asInt() == 6, "reset: global mutated to 6");
            Check(rt->ResetInstance(r), "reset: ResetInstance succeeded");
            Check(rt->IsAlive(r), "reset: InstanceHandle still valid");
            Check(!static_cast<bool>(capturedBeforeReset), "reset: captured closure FunctionHandle invalidated safely");
            g_deferredCallback = FunctionHandle();
            CallResult after = rt->Call(r, "get").invokeR();
            Check(after.ok() && after.asInt() == 5, "reset: global back to initial 5");
            CallResult viaHandle = rt->Call(r, getFn).invokeR();
            Check(viaHandle.ok() && viaHandle.asInt() == 5, "reset: FunctionHandle still valid");
            CallResult uid = rt->Call(r, "uid2").invokeR();
            Check(uid.ok() && uid.asInt() == 55, "reset: BindObject userData preserved");
            rt->DestroyInstance(r);
            rt->DestroyProgram(rcr.program);
        }
    }

    // Cancel: 시분할로 돌던 실행을 버리고 인스턴스를 재사용 가능한 Idle 로 되돌린다
    {
        const char* csrc =
            "export fun spin() { var i = 0; while (i >= 0) { i = i + 1; } }\n"
            "export fun ping() { return 77; }\n";
        CompileDesc ccd; ccd.source = csrc; ccd.sourceName = "cancel.ns";
        CompileResult ccr = rt->Compile(ccd);
        Check(static_cast<bool>(ccr.program), "cancel: compile");
        if (ccr.program)
        {
            InstanceHandle c = rt->CreateInstance(ccr.program);
            Check(rt->StartSliced(c, "spin", /*timeoutMs=*/5), "cancel: StartSliced");
            RunStatus st = rt->UpdateSliced(c);
            Check(st == RunStatus::Suspended && rt->IsRunning(c), "cancel: slice suspended, still running");
            Check(rt->Cancel(c), "cancel: Cancel succeeded");
            Check(!rt->IsRunning(c), "cancel: not running after Cancel");
            Check(rt->GetState(c) == InstanceState::Idle, "cancel: state back to Idle");
            CallResult p = rt->Call(c, "ping").invokeR();
            Check(p.ok() && p.asInt() == 77, "cancel: instance still callable after Cancel");
            rt->DestroyInstance(c);
            rt->DestroyProgram(ccr.program);
        }
    }

    // 디버거 (IDebugger shim) — 브레이크포인트/스택
    {
        IDebugger* dbg = rt->GetDebugger();
        Check(dbg != nullptr, "GetDebugger not null");
        if (dbg)
        {
            std::vector<int> lines;
            dbg->GetExecutableLines(cr.program, lines);
            Check(!lines.empty(), "GetExecutableLines non-empty");

            DbgListener lis; lis.dbg = dbg;
            dbg->SetListener(&lis);
            DebugBreakpoint bp; bp.file = 0; bp.line = 3; // compute 본문
            DebugBreakpoint arr[1] = { bp };
            dbg->SetBreakpoints(a, arr);

            rt->Call(a, "compute").argInt(1).argInt(2).invoke();
            printf("  [debug] OnStopped=%d lastLine=%d stack=%d\n", lis.stops, lis.loc.line, lis.stack ? 1 : 0);
            Check(lis.stops > 0, "breakpoint hit (OnStopped fired)");
            Check(lis.stops == 0 || lis.stack, "stack non-empty at stop");

            dbg->SetBreakpoints(a, Span<const DebugBreakpoint>());
            dbg->SetListener(nullptr);
        }
    }

    // 디버거 — 전역(top-level) 코드 브레이크포인트: CreateInstance(false) → BP → RunGlobalInit
    {
        const char* gsrc =
            "var g = 0;\n"      // 1
            "g = 1 + 2;\n"      // 2  (top-level 실행문)
            "g = g + 10;\n";    // 3
        CompileDesc gcd; gcd.source = gsrc; gcd.sourceName = "glob.ns"; gcd.includeDebugInfo = true;
        CompileResult gcr = rt->Compile(gcd);
        Check(static_cast<bool>(gcr.program), "top-level compile");
        if (gcr.program)
        {
            InstanceDesc idesc; idesc.runGlobalInit = false;   // 생성 시 init 미실행
            InstanceHandle g = rt->CreateInstance(gcr.program, idesc);
            IDebugger* dbg = rt->GetDebugger();
            DbgListener glis; glis.dbg = dbg;
            dbg->SetListener(&glis);
            DebugBreakpoint bp; bp.file = 0; bp.line = 2;
            DebugBreakpoint arr[1] = { bp };
            dbg->SetBreakpoints(g, arr);
            RunStatus st = rt->RunGlobalInit(g);               // 전역 코드 실행 → line 2 정지
            printf("  [glob-debug] status=%d OnStopped=%d line=%d\n", (int)st, glis.stops, glis.loc.line);
            Check(glis.stops > 0, "top-level breakpoint hit (RunGlobalInit)");
            Check(glis.stops == 0 || glis.loc.line == 2, "stopped at top-level line 2");
            dbg->SetListener(nullptr);
            rt->DestroyInstance(g);
            rt->DestroyProgram(gcr.program);
        }
    }

    // 캡처 람다의 수명. 호스트가 핸들을 오래 들고 있을 때 alloc 캡처(문자열/맵)가
    // 함께 살아 있어야 하고, Runtime 이 죽으면 핸들은 무효가 되되 해제는 새어나가면 안 된다.
    {
        SNeoVMAllocStats base{}, held{}, afterRuntime{}, afterHandle{};
        GetNeoVMAllocStats(base);
        FunctionHandle kept;
        {
            RuntimeDesc rd2;
            IRuntime* rt2 = CreateRuntime(rd2);
            NativeObjectDesc od2; od2.name = "Host"; od2.method = &HostMethod;
            rt2->RegisterObject(od2);
            rt2->FreezeBindings();
            const char* src2 =
                "export fun hold() { var s = \"LIFE\"; var m = { \"n\" : 1 };"
                " Host.defer(fun() { m[\"n\"] = m[\"n\"] + 1; return s .. m[\"n\"]; }); }" "\n";
            CompileDesc cd2; cd2.source = src2; cd2.sourceName = "life.ns";
            CompileResult cr2 = rt2->Compile(cd2);
            Check(static_cast<bool>(cr2.program), "lifetime: compile");
            InstanceHandle i2 = rt2->CreateInstance(cr2.program);
            rt2->BindObject(i2, "Host", nullptr);
            g_deferredCallback = FunctionHandle();
            rt2->Call(i2, "hold").invoke();
            kept = g_deferredCallback;              // 호스트가 오래 들고 있는 사본
            g_deferredCallback = FunctionHandle();
            GetNeoVMAllocStats(held);
            CallResult r1 = rt2->Call(i2, kept).invokeR();
            Check(r1.ok() && std::string(r1.asString().data(), r1.asString().size()) == "LIFE2",
                "lifetime: host handle keeps the alloc captures alive");
            Check(held.strings > base.strings || held.maps > base.maps,
                "lifetime: captured string/map are still allocated while the handle lives");
            rt2->DestroyProgram(cr2.program);
            DestroyRuntime(rt2);                    // 핸들을 든 채로 Runtime 파괴
        }
        GetNeoVMAllocStats(afterRuntime);
        Check(!static_cast<bool>(kept), "lifetime: handle goes invalid when the runtime dies");
        Check(afterRuntime.maps <= base.maps && afterRuntime.strings <= base.strings,
            "lifetime: captures are released with the runtime, not leaked");
        kept = FunctionHandle();                    // 죽은 런타임의 핸들 소멸 — 크래시 없어야 한다
        GetNeoVMAllocStats(afterHandle);
        Check(afterHandle.maps <= base.maps, "lifetime: destroying a stale handle is safe");
    }

    // 순환에 속한 closure 라도 호스트 핸들이 살아 있으면 수집기가 가져가면 안 된다.
    {
        RuntimeDesc rd3;
        IRuntime* rt3 = CreateRuntime(rd3);
        NativeObjectDesc od3; od3.name = "Host"; od3.method = &HostMethod;
        rt3->RegisterObject(od3);
        rt3->FreezeBindings();
        const char* src3 =
            "export fun holdCyclic() { var m = { \"n\" : 7 };"
            " m[\"self\"] = fun() { return m[\"n\"]; };"
            " Host.defer(m[\"self\"]); }" "\n";
        CompileDesc cd3; cd3.source = src3; cd3.sourceName = "cyc.ns";
        CompileResult cr3 = rt3->Compile(cd3);
        InstanceHandle i3 = rt3->CreateInstance(cr3.program);
        rt3->BindObject(i3, "Host", nullptr);
        g_deferredCallback = FunctionHandle();
        rt3->Call(i3, "holdCyclic").invoke();
        FunctionHandle cyclic = g_deferredCallback;
        g_deferredCallback = FunctionHandle();
        for (int i = 0; i < 64 && rt3->CollectCycles(true) != 0; ++i) {}
        CallResult r3 = rt3->Call(i3, cyclic).invokeR();
        Check(r3.ok() && r3.asInt() == 7,
            "lifetime: a host handle protects its closure from the cycle collector");
        cyclic = FunctionHandle();
        rt3->DestroyInstance(i3);
        rt3->DestroyProgram(cr3.program);
        DestroyRuntime(rt3);
    }

    rt->DestroyInstance(a);
    rt->DestroyInstance(b);
    rt->DestroyProgram(cr.program);
    DestroyRuntime(rt);

    printf("=== smoke %s (%d failure%s) ===\n", g_fail == 0 ? "PASSED" : "FAILED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
