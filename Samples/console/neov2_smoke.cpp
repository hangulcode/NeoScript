// NeoScript v2 공개 API(shim) 런타임 스모크 — 유니파이드 핸들 모델(owning Value 없음).
// 객체 디스패치 / retMap 빌더 / reader / per-instance userData / 프로그램 공유 / zero-copy 를 실행 검증.
#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;

static int g_fail = 0;
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
    if (m == "boomHost")
    {
        // 네이티브 실패 보고: fail() 로 사유를 남기고 반드시 false 반환.
        ctx.fail(4242, "host said no");
        return false;
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
        "export fun callBoom() { return Host.boomHost(); }\n";                           // 10 (네이티브 fail)
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
            float vec[4] = {0,0,0,0};
            Check(info.getVec("vec", vec) && vec[0]==1.0f && vec[1]==2.0f && vec[2]==3.0f, "info.vec == (1,2,3)");
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
            "export fun uid2() { return Host.uid(); }\n";
        CompileDesc rcd; rcd.source = rsrc; rcd.sourceName = "reset.ns";
        CompileResult rcr = rt->Compile(rcd);
        Check(static_cast<bool>(rcr.program), "reset: compile");
        if (rcr.program)
        {
            InstanceHandle r = rt->CreateInstance(rcr.program);
            rt->BindObject(r, "Host", reinterpret_cast<void*>(static_cast<intptr_t>(55)));
            FunctionHandle getFn = rt->FindFunction(rcr.program, "get");
            rt->Call(r, "bump").invoke();
            Check(rt->Call(r, "get").invokeR().asInt() == 6, "reset: global mutated to 6");
            Check(rt->ResetInstance(r), "reset: ResetInstance succeeded");
            Check(rt->IsAlive(r), "reset: InstanceHandle still valid");
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

    rt->DestroyInstance(a);
    rt->DestroyInstance(b);
    rt->DestroyProgram(cr.program);
    DestroyRuntime(rt);

    printf("=== smoke %s (%d failure%s) ===\n", g_fail == 0 ? "PASSED" : "FAILED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
