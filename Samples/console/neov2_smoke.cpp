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
        "export fun spawnTag() { var r = Host.spawn(); return r[\"entity\"].tag(); }\n"; // 8

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
