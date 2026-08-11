#include "NeoScript.h"
#include "NeoVMImpl.h"

#include <cstdio>
#include <iostream>
#include <sstream>

static bool SetStorageRegression()
{
    using namespace NeoScript;

    CNeoVMImpl vm;
    SetInfo* set = vm.SetAlloc();
    if (set == nullptr)
        return false;

    // All keys collide at every power-of-two capacity.  This exercises chain
    // relocation, rehashing, deletion, and reuse of deleted slots.
    for (int i = 0; i < 64; ++i)
    {
        VarInfo value(i * 128);
        if (!set->Insert(&value))
        {
            vm.FreeSet(set);
            return false;
        }
    }

    for (int i = 0; i < 64; i += 2)
    {
        VarInfo value(i * 128);
        set->Remove(&value);
    }
    for (int i = 64; i < 96; ++i)
    {
        VarInfo value(i * 128);
        if (!set->Insert(&value))
        {
            vm.FreeSet(set);
            return false;
        }
    }

    // A duplicate must not change count, and all retained/new keys must find.
    VarInfo duplicate(65 * 128);
    if (!set->Insert(&duplicate) || set->GetCount() != 64)
    {
        vm.FreeSet(set);
        return false;
    }
    for (int i = 0; i < 96; ++i)
    {
        VarInfo value(i * 128);
        const bool expected = (i >= 64) || (i & 1) != 0;
        if (set->Find(&value) != expected)
        {
            vm.FreeSet(set);
            return false;
        }
    }

    int count = 0;
    long long sum = 0;
    CollectionIterator it = set->FirstNode();
    while (it._pSetNode != nullptr)
    {
        ++count;
        sum += it._pSetNode->data->key._int;
        set->NextNode(it);
    }
    const bool ok = count == 64 && set->GetCount() == 64 && sum == 456704;
    vm.FreeSet(set);
    return ok;
}

static bool StringHashRegression()
{
    // Hash values are used only in-process, but their algorithm must not vary
    // with the C++ standard library selected by a CI platform.
    return NeoScript::GetHashCode(std::string("NeoScript")) == 0x0D431679u
        && NeoScript::GetHashCode(std::string("NeoScript"))
            == NeoScript::GetHashCode(std::string("NeoScript"));
}

static bool StringInternRegression()
{
    using namespace NeoScript;

    CNeoVMImpl vm;
    VarInfo first;
    VarInfo second;
    std::string key = "a canonical string key";
    vm.Var_SetStringA(&first, key);
    vm.Var_SetStringA(&second, key);

    MapInfo* map = vm.TableAlloc();
    SetInfo* set = vm.SetAlloc();
    map->Insert(&first, 17);
    set->Insert(&first);
	StringInfo* canonical = vm.StringFind(key);

    bool ok = first._str != second._str
		&& !first._str->_interned && !second._str->_interned
		&& first._str->_StringLen == (int)key.size()
		&& canonical != nullptr && canonical->_interned
		&& canonical->_StringLen == (int)key.size()
        && map->Find(&second) != nullptr
        && map->Find(key) != nullptr
        && set->Find(&second)
        && set->Find(key);
	// The dynamic key is not interned, but its hash is cached after the first
	// Map/Set lookup so later lookups/removal do not hash its bytes again.
	ok = ok && second._str->_hash != 0;

	map->Remove(&second);
	set->Remove(&second);
	ok = ok && map->Find(&first) == nullptr && !set->Find(&first)
		&& vm.StringFind(key) == nullptr;

    vm.FreeTable(map);
    vm.FreeSet(set);
    vm.Var_Release(&first);
    vm.Var_Release(&second);
    return ok && vm.StringFind(key) == nullptr;
}

static bool StringInternChurnRegression()
{
    using namespace NeoScript;

    CNeoVMImpl vm;
    std::vector<VarInfo> values(256);
	MapInfo* map = vm.TableAlloc();
    bool ok = true;
    for (int i = 0; i < (int)values.size(); ++i)
    {
        const std::string key = "intern-churn-" + std::to_string(i);
        vm.Var_SetStringA(&values[i], key);
		ok = ok && !values[i]._str->_interned && vm.StringFind(key) == nullptr;
		map->Insert(&values[i], i);
		ok = ok && vm.StringFind(key) != nullptr && map->Find(&values[i]) != nullptr;
    }
    for (int i = 0; i < (int)values.size(); ++i)
    {
        const std::string key = "intern-churn-" + std::to_string(i);
        vm.Var_Release(&values[i]);
		ok = ok && vm.StringFind(key) != nullptr;
    }
	vm.FreeTable(map);
	for (int i = 0; i < (int)values.size(); ++i)
	{
		const std::string key = "intern-churn-" + std::to_string(i);
		ok = ok && vm.StringFind(key) == nullptr;
	}
	return ok;
}

static bool HashLoadFactorRegression()
{
    using namespace NeoScript;

    CNeoVMImpl vm;
    MapInfo* map = vm.TableAlloc();
    SetInfo* set = vm.SetAlloc();
    map->Reserve(8);
    set->Reserve(8);
    bool ok = map->_BucketCapa >= 16 && set->_BucketCapa >= 16;

    for (int i = 0; i < 12; ++i)
    {
        VarInfo key(i);
        map->Insert(&key, i);
        ok = ok && set->Insert(&key);
    }
    ok = ok && map->_BucketCapa == 16 && set->_BucketCapa == 16;

    VarInfo extra(12);
    map->Insert(&extra, 12);
    ok = ok && set->Insert(&extra)
        && map->_BucketCapa >= 32 && set->_BucketCapa >= 32
        && map->Find(&extra) != nullptr && set->Find(&extra);

    vm.FreeTable(map);
    vm.FreeSet(set);
    return ok;
}

static bool ContainerDestroyRegression()
{
    using namespace NeoScript;

    // Dropping the external reference leaves this self-cycle for the VM's
    // live-list sweep, which must return without re-entering its destruction.
    {
        CNeoVMImpl vm;
        VarInfo self;
        vm.Var_SetTable(&self, vm.TableAlloc());
        self._tbl->Insert("self", &self);
        vm.Var_Release(&self);
    }

    // A deep acyclic graph must be collected immediately without consuming
    // one native stack frame per container.
    CNeoVMImpl vm;
    VarInfo root;
    vm.Var_SetTable(&root, vm.TableAlloc());
    MapInfo* current = root._tbl;
    for (int i = 0; i < 4096; ++i)
    {
        VarInfo child;
        vm.Var_SetTable(&child, vm.TableAlloc());
        current->Insert("next", &child);
        current = child._tbl;
        vm.Var_Release(&child);
    }
    vm.Var_Release(&root);

    SNeoVMAllocStats stats{};
    vm.GetAllocStats(stats);
    return stats.maps == 0;
}

static void DrainCycles(NeoScript::CNeoVMImpl& vm)
{
    while (vm.CollectCycles() != 0)
    {
    }
}

static bool CycleCollectionRegression()
{
    using namespace NeoScript;

    CNeoVMImpl vm;
    SNeoVMAllocStats stats{};

    // Scalar-only map/list/set values cannot participate in a reference cycle.
    // Releasing one of two references must not create a cycle ticket.
    VarInfo scalarList(VAR_LIST);
    scalarList._lst = vm.ListAlloc();
    ++scalarList._lst->_refCount;
    VarInfo scalarListHold;
    Move_DestNoRelease(&scalarListHold, &scalarList);
    VarInfo scalarValue(7);
    scalarList._lst->InsertLast(&scalarValue);
    vm.Var_Release(&scalarList);
    const bool scalarListSkipsCycleQueue = vm.CollectCycles() == 0;
    vm.Var_Release(&scalarListHold);

    VarInfo scalarMap;
    vm.Var_SetTable(&scalarMap, vm.TableAlloc());
    VarInfo scalarMapHold;
    Move_DestNoRelease(&scalarMapHold, &scalarMap);
    VarInfo scalarMapKey(1);
    scalarMap._tbl->Insert(&scalarMapKey, 7);
    vm.Var_Release(&scalarMap);
    const bool scalarMapSkipsCycleQueue = vm.CollectCycles() == 0;
    vm.Var_Release(&scalarMapHold);

    VarInfo scalarSet(VAR_SET);
    scalarSet._set = vm.SetAlloc();
    ++scalarSet._set->_refCount;
    VarInfo scalarSetHold;
    Move_DestNoRelease(&scalarSetHold, &scalarSet);
    scalarSet._set->Insert(&scalarValue);
    vm.Var_Release(&scalarSet);
    const bool scalarSetSkipsCycleQueue = vm.CollectCycles() == 0;
    vm.Var_Release(&scalarSetHold);

    // self / list / set cycle is collected by the VM's internal safe-point
    // scheduler, without the host calling TrimMemory.
    VarInfo self;
    vm.Var_SetTable(&self, vm.TableAlloc());
    self._tbl->Insert("self", &self);
    vm.Var_Release(&self);

    VarInfo list(VAR_LIST);
    list._lst = vm.ListAlloc();
    ++list._lst->_refCount;
    list._lst->InsertLast(&list);
    vm.Var_Release(&list);

    VarInfo set(VAR_SET);
    set._set = vm.SetAlloc();
    ++set._set->_refCount;
    set._set->Insert(&set);
    vm.Var_Release(&set);

    vm.GetAllocStats(stats);
    const bool queuedCyclesRemain = stats.maps == 1 && stats.lists == 1 && stats.sets == 1;
    // TrimMemory is pool-page policy only; it must not consume cycle tickets.
    vm.CollectEmptyPages(true);
    vm.GetAllocStats(stats);
    const bool trimDoesNotCollectCycles = stats.maps == 1 && stats.lists == 1 && stats.sets == 1;
    // A direct-use VM has no workers, so its next safe point immediately
    // reaches the worker-round boundary and runs the cycle collector.
    vm.OnVMSafePoint();
    vm.GetAllocStats(stats);
    const bool scheduledCollectionOk = stats.maps == 0 && stats.lists == 0 && stats.sets == 0;

    // An external holder keeps the same self-cycle alive.  Releasing that
    // holder queues it again, and the next VM cycle pass may collect it.
    VarInfo root;
    VarInfo hold;
    vm.Var_SetTable(&root, vm.TableAlloc());
    root._tbl->Insert("self", &root);
    Move_DestNoRelease(&hold, &root);
    vm.Var_Release(&root);
    DrainCycles(vm);
    vm.GetAllocStats(stats);
    const bool externalReferencePreserved = stats.maps == 1;
    vm.Var_Release(&hold);
    DrainCycles(vm);
    vm.GetAllocStats(stats);
    const bool releasedAfterExternalGone = stats.maps == 0;

    // A queued candidate can die before the VM's next cycle pass when its cycle is broken.
    VarInfo stale;
    VarInfo staleHold;
    vm.Var_SetTable(&stale, vm.TableAlloc());
    VarInfo staleKey(1);
    stale._tbl->Insert(&staleKey, &stale);
    MapInfo* staleMap = stale._tbl;
    Move_DestNoRelease(&staleHold, &stale);
    vm.Var_Release(&stale);
    staleMap->Remove(&staleKey);
    vm.Var_Release(&staleHold);
    DrainCycles(vm);
    vm.GetAllocStats(stats);
    const bool staleTicketSafe = stats.maps == 0;

    // Metadata links are refcounted container edges too, not just map values.
    VarInfo metaA;
    VarInfo metaB;
    vm.Var_SetTable(&metaA, vm.TableAlloc());
    vm.Var_SetTable(&metaB, vm.TableAlloc());
    metaA._tbl->_meta = metaB._tbl;
    metaA._tbl->MarkContainerChild();
    ++metaB._tbl->_refCount;
    metaB._tbl->_meta = metaA._tbl;
    metaB._tbl->MarkContainerChild();
    ++metaA._tbl->_refCount;
    vm.Var_Release(&metaA);
    vm.Var_Release(&metaB);
    DrainCycles(vm);
    vm.GetAllocStats(stats);
    const bool metaCycleCollected = stats.maps == 0;

    // A -> B, B -> A and B -> B.  B is first examined while A has an
    // external reference, then A becomes the only candidate.  Collection
    // must still destroy the complete white set without leaving B->A stale.
    VarInfo nestedA;
    VarInfo nestedB;
    vm.Var_SetTable(&nestedA, vm.TableAlloc());
    vm.Var_SetTable(&nestedB, vm.TableAlloc());
    nestedA._tbl->Insert("b", &nestedB);
    nestedB._tbl->Insert("back", &nestedA);
    nestedB._tbl->Insert("self", &nestedB);
    vm.Var_Release(&nestedB);
    DrainCycles(vm);
    vm.Var_Release(&nestedA);
    DrainCycles(vm);
    vm.GetAllocStats(stats);
    const bool nestedWhiteSetCollected = stats.maps == 0;

    // Each automatic cycle pass processes max(16, ceil(queue_size * 2%)).
    constexpr int kCandidateCount = 4000;
    for (int i = 0; i < kCandidateCount; ++i)
    {
        VarInfo value;
        vm.Var_SetTable(&value, vm.TableAlloc());
        value._tbl->Insert("self", &value);
        vm.Var_Release(&value);
    }
    vm.GetAllocStats(stats);
    const int before = stats.maps;
    const int processed = vm.CollectCycles();
    vm.GetAllocStats(stats);
    const bool percentageBudgetOk = before == kCandidateCount && processed == 80 && stats.maps <= before - 80;
    DrainCycles(vm);
    vm.GetAllocStats(stats);

    return scalarListSkipsCycleQueue && scalarMapSkipsCycleQueue && scalarSetSkipsCycleQueue
        && queuedCyclesRemain && trimDoesNotCollectCycles && scheduledCollectionOk && externalReferencePreserved
        && releasedAfterExternalGone && staleTicketSafe && metaCycleCollected && nestedWhiteSetCollected && percentageBudgetOk
        && stats.maps == 0;
}

int main()
{
    using namespace NeoScript;

    IRuntime* runtime = CreateRuntime();
    if (runtime == nullptr)
    {
        std::fputs("CreateRuntime failed\n", stderr);
        return 1;
    }
    runtime->SetCycleCollectIntervalSeconds(0.125f);
    const bool cycleIntervalApiOk = runtime->GetCycleCollectIntervalSeconds() == 0.125f;
    runtime->SetCycleCollectIntervalSeconds(0.02f);

    const char* source =
		"var sortMutationMap = null;\n"
		"fun sortMutating(var a, var b) { sortMutationMap[9] = 9; return a < b; }\n"
        "export fun add(var a, var b) { return a + b; }\n"
        "export fun literal() { return 1.25 + 2.75; }\n"
        "export fun mapRehash(var n) {\n"
        "  var m = {};\n"
        "  for (var i in 0, n, 1) { var key = \"key\" .. i; m[key] = i; }\n"
        "  var sum = 0;\n"
        "  for (var i in 0, n, 1) { var key = \"key\" .. i; sum += m[key]; }\n"
        "  for (var i in 0, n, 2) { var key = \"key\" .. i; m[key] = null; }\n"
        "  var remain = 0;\n"
        "  foreach (var key, value in m) { remain += value; }\n"
        "  return sum * 4096 + remain;\n"
        "}\n"
        "export fun mapDeleteReinsert() {\n"
        "  var m = {};\n"
        "  for (var i in 0, 8, 1) { m[i * 8] = i; }\n"
        "  for (var i in 0, 8, 2) { m[i * 8] = null; }\n"
        "  for (var i in 8, 12, 1) { m[i * 8] = i; }\n"
        "  var sum = 0;\n"
        "  foreach (var key, value in m) { sum += value; }\n"
        "  return sum;\n"
        "}\n"
        "export fun mapSortNormal() {\n"
        "  var m = {3, 1, 2};\n"
        "  m.sort(fun(var a, var b) { return a < b; });\n"
        "  return m[0] * 100 + m[1] * 10 + m[2];\n"
        "}\n"
        "export fun mapSortMutation() {\n"
        "  sortMutationMap = {3, 1, 2};\n"
        "  sortMutationMap.sort(sortMutating);\n"
        "  return 0;\n"
        "}\n"
        "}\n";
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "ci_smoke.ns";
    desc.emitAsm = true;
    std::ostringstream asmOutput;
    std::streambuf* const originalCout = std::cout.rdbuf(asmOutput.rdbuf());
    CompileResult compiled = runtime->Compile(desc);
    std::cout.rdbuf(originalCout);
    if (!compiled.program)
    {
        std::fprintf(stderr, "Compile failed: %s\n", compiled.error.message.c_str());
        DestroyRuntime(runtime);
        return 1;
    }

    InstanceHandle instance = runtime->CreateInstance(compiled.program);
    if (!instance)
    {
        std::fputs("CreateInstance failed\n", stderr);
        runtime->DestroyProgram(compiled.program);
        DestroyRuntime(runtime);
        return 1;
    }

    bool addOk = false;
    {
        Invocation add = runtime->Call(instance, "add");
        addOk = add.argInt(20).argInt(22).invoke() == RunStatus::Completed && add.retInt() == 42;
    }

    bool literalOk = false;
    {
        Invocation literal = runtime->Call(instance, "literal");
        literalOk = literal.invoke() == RunStatus::Completed && literal.retFloat() == 4.0f;
    }

    bool mapOk = false;
    {
        Invocation mapRehash = runtime->Call(instance, "mapRehash");
        mapOk = mapRehash.argInt(64).invoke() == RunStatus::Completed && mapRehash.retInt() == 8258560;
    }

	bool mapDeleteReinsertOk = false;
	{
		Invocation mapDeleteReinsert = runtime->Call(instance, "mapDeleteReinsert");
		mapDeleteReinsertOk = mapDeleteReinsert.invoke() == RunStatus::Completed && mapDeleteReinsert.retInt() == 54;
	}

	bool mapSortNormalOk = false;
	{
		Invocation mapSortNormal = runtime->Call(instance, "mapSortNormal");
		mapSortNormalOk = mapSortNormal.invoke() == RunStatus::Completed && mapSortNormal.retInt() == 123;
	}

	bool mapSortMutationOk = false;
	{
		Invocation mapSortMutation = runtime->Call(instance, "mapSortMutation");
		mapSortMutationOk = mapSortMutation.invoke() == RunStatus::Failed;
	}


	const bool setOk = SetStorageRegression();
	const bool containerDestroyOk = ContainerDestroyRegression();
	const bool cycleTrimOk = CycleCollectionRegression();
	const bool stringHashOk = StringHashRegression();
	const bool stringInternOk = StringInternRegression();
	const bool stringInternChurnOk = StringInternChurnRegression();
	const bool hashLoadFactorOk = HashLoadFactorRegression();

    runtime->DestroyInstance(instance);
    runtime->DestroyProgram(compiled.program);
    DestroyRuntime(runtime);

    const bool asmOk = asmOutput.str().find("Fun -") != std::string::npos;
    if (!addOk || !literalOk || !mapOk || !mapDeleteReinsertOk || !mapSortNormalOk || !mapSortMutationOk || !setOk || !containerDestroyOk || !cycleTrimOk || !cycleIntervalApiOk || !stringHashOk || !stringInternOk || !stringInternChurnOk || !hashLoadFactorOk || !asmOk)
    {
        std::fputs("NeoScript API smoke failed\n", stderr);
        return 1;
    }
    return 0;
}
