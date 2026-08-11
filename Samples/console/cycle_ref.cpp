// 순환 참조 파괴 경로 회귀 드라이버.
//
// TestScript/cycle_ref.ns 를 실행하고, 스크립트만으로는 확인할 수 없는 두 가지를 본다.
//
//  1) 회수 가능한 경로가 실제로 회수되는가.
//     파괴 큐/파괴 표식(NEOS_RC_DESTROYING)을 넣으면서, 순환이 아닌 정상 해제까지
//     막아버리는 회귀가 나기 쉽다. CycleReclaimable 을 반복 호출하며 alloc 카운터가
//     늘지 않는지 확인한다.
//
//  2) 순환이 호스트 개입 없이 회수되는가.
//     참조 카운팅은 순환을 회수하지 못한다. CycleLeaking 으로 고리를 만든 뒤
//     CycleTick(빈 함수)으로 안전 지점만 공급해, VM 이 스스로 회수하는지 본다.
//     이 드라이버는 TrimMemory 를 한 번도 부르지 않는다 — 그게 판정의 전제다.
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
    // 1) 순환은 호스트가 TrimMemory 를 부르지 않아도 회수되어야 한다.
    //
    // 이 드라이버는 TrimMemory 를 한 번도 부르지 않는다. 그래서 여기서 컨테이너가
    // 기준선으로 돌아온다면, VM 이 자기 안전 지점에서 수집했다는 뜻이다.
    //
    // [이 검사가 왜 필요한가]
    // 안전 지점은 실행 경로마다 따로 배선된다. 예전에 EndHostCall 의 배선이 빠져
    // **공개 v2 API(rt->Call(...).invoke())로는 수집이 한 번도 일어나지 않은** 적이
    // 있는데, 그때도 아래 1)/3) 과 스크립트 케이스는 전부 통과했다. 즉 이 검사가
    // 없으면 "수집기가 통째로 비활성" 인 상태를 CI 가 구분하지 못한다.
    //
    // [순서가 중요하다 — 반드시 첫 번째여야 한다]
    // 후보 큐에는 무효화된 티켓이 그대로 남는다(CancelCycleCandidate 는 ticket->object
    // 만 null 로 만들고 deque 에서 빼지 않는다). 아래 2) 의 대량 컨테이너 churn 을
    // 먼저 돌리면 죽은 티켓이 수십만 개 쌓이고, 이후 만든 진짜 순환은 그 뒤에 줄서서
    // 예산 안에 도달하지 못한다. 그래서 이 검사는 큐가 깨끗한 맨 처음에 둔다.
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

        // 수집은 증분이다 — worker 라운드가 한 바퀴 돌고 후보 예산만큼씩 처리한다.
        // CycleTick 은 순환을 만들지 않는 빈 함수라, 안전 지점만 공급한다.
        //
        // 호출이 실제로 성공했는지 반드시 확인한다. 이름이 틀리거나 export 가 빠지면
        // invoke 는 조용히 아무 일도 하지 않고, 그러면 "안전 지점을 공급했다"는 전제가
        // 무너진 채로 아래 판정만 남는다(실제로 그렇게 잘못 만든 적이 있다).
        int tickOk = 0;
        const int kTicks = 256;
        for (int i = 0; i < kTicks; ++i)
        {
            if (rt->Call(inst, "CycleTick").invoke() == RunStatus::Completed)
                ++tickOk;
        }
        CycOk(tickOk == kTicks, "CycleTick invocations all completed (safe points really were supplied)");
        GetNeoVMAllocStats(after);

        CycOk(ContainerCount(after) <= ContainerCount(before),
              "VM collects unreachable cycles at its own safe points (host never calls TrimMemory)");
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
    // 3) 전체 순환 참조 스크립트 검증 뒤 파괴한다.
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
