// 순환 참조 파괴 경로 회귀 드라이버.
//
// TestScript/cycle_ref.ns 를 실행하고, 스크립트만으로는 확인할 수 없는 두 가지를 본다.
//
//  1) 회수 가능한 경로가 실제로 회수되는가.
//     파괴 큐/파괴 표식(NEOS_RC_DESTROYING)을 넣으면서, 순환이 아닌 정상 해제까지
//     막아버리는 회귀가 나기 쉽다. CycleReclaimable 을 반복 호출하며 alloc 카운터가
//     늘지 않는지 확인한다.
//
//  2) 런타임을 파괴할 때 죽지 않는가.
//     원 버그가 바로 여기서 났다 — VM 소멸자의 live 리스트 스윕이 순환 객체를 풀다가
//     파괴 중인 자신에게 재진입해 무한재귀 + 이중 풀반납 + 리스트 헤드 오염을 냈다.
//     DestroyRuntime 이 정상 반환하는 것 자체가 판정이다.
//
// 참조 카운트만으로는 순환을 즉시 회수하지 않는다. CycleLeaking 으로 후보를 만든 뒤
// TrimMemory(true)가 외부 사용처 없는 고리를 실제로 회수하는지 확인한다.
#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"
#include "../../NeoSource/NeoVM.h"   // alloc 카운터(SNeoVMAllocStats) — 공개 API 에는 없다

#include <cstdio>
#include <string>

using namespace NeoScript;

static int g_cycFail = 0;
static void CycOk(bool cond, const char* msg)
{
    printf(cond ? "  ok  : %s\n" : "  FAIL: %s\n", msg);
    if (!cond) ++g_cycFail;
}

static int ContainerCount(const SNeoVMAllocStats& s)
{
    return s.maps + s.lists + s.sets;
}

int SAMPLE_cycle_ref(INeoLoader* pLoader, std::string filename)
{
    void* pFileBuffer = nullptr;
    int iFileLen = 0;
    if (!pLoader->Load(filename.c_str(), pFileBuffer, iFileLen))
    {
        printf("file read error: %s\n", filename.c_str());
        return -1;
    }

    g_cycFail = 0;

    RuntimeDesc rd;
    rd.nativeLoader = pLoader;
    IRuntime* rt = CreateRuntime(rd);
    rt->FreezeBindings();

    CompileDesc cd;
    cd.source = StringView((const char*)pFileBuffer, (size_t)iFileLen);
    cd.sourceName = filename.c_str();
    CompileResult cr = rt->Compile(cd);
    pLoader->Unload(filename.c_str(), pFileBuffer, iFileLen);

    if (!cr.program)
    {
        printf("Error - compile failed : %s\n", cr.error.message.c_str());
        DestroyRuntime(rt);
        return -1;
    }

    // 최상위 실행 = CycleRun(). 스크립트 쪽 케이스가 여기서 전부 돈다.
    InstanceHandle inst = rt->CreateInstance(cr.program);
    if (!inst)
    {
        printf("Error - CreateInstance failed\n");
        rt->DestroyProgram(cr.program);
        DestroyRuntime(rt);
        return -1;
    }

    // ---------------------------------------------------------------------
    // 1) 회수 가능한 경로는 반복해도 늘지 않아야 한다.
    //
    // 첫 호출은 정착(문자열 상수 인터닝, 풀 예열)에 쓰고 그 뒤부터 비교한다.
    // 순환을 손으로 끊은 케이스 + 순환 없는 깊은 중첩만 들어 있는 진입점이다.
    // ---------------------------------------------------------------------
    {
        rt->Call(inst, "CycleReclaimable").invoke();

        SNeoVMAllocStats before{}, after{};
        GetNeoVMAllocStats(before);
        for (int i = 0; i < 20; ++i)
            rt->Call(inst, "CycleReclaimable").invoke();
        GetNeoVMAllocStats(after);

        CycOk(ContainerCount(after) <= ContainerCount(before),
              "reclaimable paths do not accumulate containers over 20 calls");
        if (ContainerCount(after) > ContainerCount(before))
            printf("    map %d->%d  lst %d->%d  set %d->%d\n",
                   before.maps, after.maps, before.lists, after.lists, before.sets, after.sets);
    }

    // ---------------------------------------------------------------------
    // 2) 순환 후보를 만든 뒤 TrimMemory가 실제로 회수하는지 본다.
    // ---------------------------------------------------------------------
    {
        SNeoVMAllocStats before{}, queued{}, trimmed{};
        GetNeoVMAllocStats(before);
        const int kRounds = 20;
        for (int i = 0; i < kRounds; ++i)
            rt->Call(inst, "CycleLeaking").invoke();
        GetNeoVMAllocStats(queued);

        const int grown = ContainerCount(queued) - ContainerCount(before);
        printf("  info: cyclic paths queued %d containers over %d rounds (%.1f/round)\n",
               grown, kRounds, kRounds ? (double)grown / kRounds : 0.0);
        CycOk(grown > 0, "cyclic paths are retained until TrimMemory");

        rt->TrimMemory(true);
        GetNeoVMAllocStats(trimmed);
        CycOk(ContainerCount(trimmed) <= ContainerCount(before),
              "TrimMemory collects unreachable cyclic containers");
        if (ContainerCount(trimmed) > ContainerCount(before))
            printf("    map %d->%d  lst %d->%d  set %d->%d\n",
                   before.maps, trimmed.maps, before.lists, trimmed.lists, before.sets, trimmed.sets);
    }

    // ---------------------------------------------------------------------
    // 3) 파괴. 원 버그의 현장 — 여기서 무한재귀/이중해제로 죽었다.
    //    정상 반환하는 것이 판정이다.
    // ---------------------------------------------------------------------
    rt->DestroyInstance(inst);
    rt->DestroyProgram(cr.program);
    DestroyRuntime(rt);
    CycOk(true, "runtime destroyed without crash (cyclic sweep survived)");

    SNeoVMAllocStats atExit{};
    GetNeoVMAllocStats(atExit);
    printf("  info: live after destroy  map=%d lst=%d set=%d str=%d\n",
           atExit.maps, atExit.lists, atExit.sets, atExit.strings);
	CycOk(ContainerCount(atExit) == 0,
	      "runtime destroyed with no live containers");

    if (g_cycFail == 0)
        printf("cycle_ref driver PASS\n");
    else
        printf("cycle_ref driver FAIL (%d)\n", g_cycFail);
    return g_cycFail == 0 ? 0 : -1;
}
