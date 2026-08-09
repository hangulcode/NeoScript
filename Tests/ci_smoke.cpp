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
        sum += it._pSetNode->key._int;
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

int main()
{
    using namespace NeoScript;

    IRuntime* runtime = CreateRuntime();
    if (runtime == nullptr)
    {
        std::fputs("CreateRuntime failed\n", stderr);
        return 1;
    }

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
	const bool stringHashOk = StringHashRegression();
	const bool stringInternOk = StringInternRegression();
	const bool stringInternChurnOk = StringInternChurnRegression();
	const bool hashLoadFactorOk = HashLoadFactorRegression();

    runtime->DestroyInstance(instance);
    runtime->DestroyProgram(compiled.program);
    DestroyRuntime(runtime);

    const bool asmOk = asmOutput.str().find("Fun -") != std::string::npos;
    if (!addOk || !literalOk || !mapOk || !mapDeleteReinsertOk || !mapSortNormalOk || !mapSortMutationOk || !setOk || !stringHashOk || !stringInternOk || !stringInternChurnOk || !hashLoadFactorOk || !asmOk)
    {
        std::fputs("NeoScript API smoke failed\n", stderr);
        return 1;
    }
    return 0;
}
