// 순환 참조 파괴 경로 회귀 드라이버.
//
// TestScript/cycle_ref.ns 를 실행하고, 스크립트만으로는 확인할 수 없는 두 가지를 본다.
//
//  1) 회수 가능한 경로가 실제로 회수되는가.
//     파괴 큐/파괴 표식(NEOS_RC_DESTROYING)을 넣으면서, 순환이 아닌 정상 해제까지
//     막아버리는 회귀가 나기 쉽다. CycleReclaimable 을 반복 호출하며 alloc 카운터가
//     늘지 않는지 확인한다.
//
//  2) 호스트가 선택한 시점에 순환을 회수할 수 있는가.
//     참조 카운팅은 순환을 회수하지 못한다. CycleLeaking 으로 고리를 만든 뒤
//     IRuntime::CollectCycles(false)로 증분 회수를 요청한다. TrimMemory는 호출하지
//     않는다 — pool 페이지 정책과 순환 회수가 분리돼 있음을 함께 검증한다.
//
//  3) 런타임을 파괴할 때 죽지 않는가.
//     원 버그가 바로 여기서 났다 — VM 소멸자의 live 리스트 스윕이 순환 객체를 풀다가
//     파괴 중인 자신에게 재진입해 무한재귀 + 이중 풀반납 + 리스트 헤드 오염을 냈다.
//     DestroyRuntime 이 정상 반환하는 것 자체가 판정이다.
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

    // 전체 순환 참조 케이스를 같은 인스턴스에서 검증한다.
    InstanceHandle inst = rt->CreateInstance(cr.program);
    if (!inst)
    {
        printf("Error - CreateInstance failed\n");
        rt->DestroyProgram(cr.program);
        DestroyRuntime(rt);
        return -1;
    }

    // ---------------------------------------------------------------------
    // 1) 순환은 host의 명시적 CollectCycles 호출로 회수되어야 한다.
    //
    // 이 드라이버는 TrimMemory 를 한 번도 부르지 않는다. 그래서 여기서 컨테이너가
    // 기준선으로 돌아온다면, pool 회수가 아니라 공개 CollectCycles API가 회수한 것이다.
    //
    // 후보 큐는 객체 자신의 intrusive 링크라, 객체가 먼저 죽으면 즉시 unlink된다.
    // 따라서 대량 churn이 있더라도 죽은 후보가 수집 예산을 소비하지 않는다.
    // 검사는 반드시 실제 호출 경로(invoke)로 해야 하고, 내부 함수를 직접 부르면
    // 배선 자체가 미검증으로 남는다.
    // ---------------------------------------------------------------------
    {
        SNeoVMAllocStats before{}, peak{}, after{};
        GetNeoVMAllocStats(before);

        const int kLeakRounds = 20;
        for (int i = 0; i < kLeakRounds; ++i)
            rt->Call(inst, "CycleLeaking").invoke();
        GetNeoVMAllocStats(peak);
        printf("  info: cyclic rounds=%d, containers %d -> %d\n",
               kLeakRounds, ContainerCount(before), ContainerCount(peak));

        // false는 한 번에 후보 일부만 처리한다. 호스트 프레임 루프가 이 호출을
        // 원하는 빈도로 넣는 것과 같은 형태로, 큐가 빌 때까지 반복한다.
        int collectPasses = 0;
        int collected = 0;
        while (collectPasses < 256)
        {
            const int processed = rt->CollectCycles(false);
            if (processed == 0)
                break;
            collected += processed;
            ++collectPasses;
        }
        CycOk(collected > 0, "CollectCycles(false) processed cycle candidates");
        GetNeoVMAllocStats(after);

        CycOk(ContainerCount(after) <= ContainerCount(before),
              "host CollectCycles reclaims unreachable cycles without TrimMemory");
        if (ContainerCount(after) > ContainerCount(before))
            printf("    map %d->%d  lst %d->%d  set %d->%d\n",
                   before.maps, after.maps, before.lists, after.lists, before.sets, after.sets);
    }

    // ---------------------------------------------------------------------
    // 2) 회수 가능한 경로는 반복해도 늘지 않아야 한다.
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
    // 3) native root 가 붙든 순환은 회수되면 안 된다 — 조기 해제 검출.
    //
    // 위 1)/2) 는 수집기가 **덜** 회수할 때만 실패한다. 수집기를 통째로 비활성화해도
    // 통과하고, 반대로 너무 공격적이어서 살아있는 객체를 해제해도 통과한다. 이 검사가
    // 그 반대 방향이다: 모듈 전역과 서스펜드된 코루틴이 붙든 순환을 강제 수집 뒤에도
    // 읽어보고, 컨테이너 수가 줄지 않았는지도 같이 본다.
    // ---------------------------------------------------------------------
    {
        SNeoVMAllocStats before{}, held{}, after{}, dropped{};
        GetNeoVMAllocStats(before);
        rt->Call(inst, "CycleNativeRootHold").invoke();
        GetNeoVMAllocStats(held);

        for (int i = 0; i < 8; ++i)
            rt->CollectCycles(true);
        GetNeoVMAllocStats(after);

        CycOk(ContainerCount(after) == ContainerCount(held),
              "native-root-held cycles survive forced collection (no premature free)");
        if (ContainerCount(after) != ContainerCount(held))
            printf("    held %d -> after %d\n", ContainerCount(held), ContainerCount(after));

        // 값까지 읽는다 — 해제된 객체를 읽으면 여기서 죽거나 틀린 값이 나온다.
        CallResult touch = rt->Call(inst, "CycleNativeRootTouch").invokeR();
        CycOk(touch.ok() && touch.asInt() == 1,
              "global- and coroutine-held cycles are still intact after collection");

        // 놓으면 회수돼야 한다. 이 방향이 빠지면 위 검사는 "아무것도 회수 안 함" 으로도 통과한다.
        CallResult drop = rt->Call(inst, "CycleNativeRootDrop").invokeR();
        CycOk(drop.ok() && drop.asInt() == 2, "held coroutine ran to completion after collection");
        rt->CollectCycles(true);
        GetNeoVMAllocStats(dropped);
        CycOk(ContainerCount(dropped) < ContainerCount(after),
              "cycles become collectable once the native root releases them");
    }

    // ---------------------------------------------------------------------
    // 4) 리프를 매단 순환 — 그래프에서 제외된 리프도 부모와 함께 회수되는가.
    // ---------------------------------------------------------------------
    {
        SNeoVMAllocStats before{}, peak{}, after{};
        GetNeoVMAllocStats(before);
        rt->Call(inst, "CycleLeafParents").argInt(200).invoke();
        GetNeoVMAllocStats(peak);
        CycOk(peak.lists - before.lists >= 200, "leaf lists accumulated inside cycles");

        rt->CollectCycles(true);
        GetNeoVMAllocStats(after);
        CycOk(after.lists <= before.lists,
              "pruned leaf lists are reclaimed with their cyclic parents");
        if (after.lists > before.lists)
            printf("    lst %d -> %d (peak %d)\n", before.lists, after.lists, peak.lists);
    }

    // ---------------------------------------------------------------------
    // 5) 전체 순환 참조 스크립트 검증 뒤 파괴한다.
    //    정상 반환하는 것이 판정이다.
    // ---------------------------------------------------------------------
    rt->Call(inst, "CycleRun").invoke();
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
