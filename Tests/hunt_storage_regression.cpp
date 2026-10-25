#include "NeoScript.h"
#include "NeoVMInternal.h"
#include "NeoNumeric.h"
#include "NeoStringLimits.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& name)
{
    ++checks;
    if (!ok && ++failures <= 30) std::fprintf(stderr, "FAIL %s\n", name.c_str());
}
template <typename F> static void Limit(F action, const char* name)
{
    bool rejected = false;
    try { action(); }
    catch (const std::length_error&) { rejected = true; }
    Check(rejected, name);
}

static void StorageLimits()
{
    CheckRuntimeStringSize(INT_MAX);
    CheckRuntimeStringGrowth(INT_MAX - 1, 1);
    Limit([] { CheckRuntimeStringSize(static_cast<size_t>(INT_MAX) + 1); }, "string byte limit");
    Limit([] { CheckRuntimeStringGrowth(INT_MAX, 1); }, "string growth limit");
    Limit([] { CheckRuntimeStringGrowth(1, std::numeric_limits<size_t>::max()); }, "growth cannot wrap");
    Check(HashCapacityForItems(MAX_HASH_ITEMS) == MAX_HASH_CAPACITY, "largest hash capacity arithmetic");
    Check(HashCapacityForItems(-1) == 2 && HashCapacityForItems(2) == 4, "small hash capacities");
    Limit([] { HashCapacityForItems(MAX_HASH_ITEMS + 1); }, "hash item limit");
    Limit([] { GrowHashCapacity(MAX_HASH_CAPACITY); }, "hash doubling limit");

    CNeoVM vm;
    Limit([&] { vm.TableAlloc(INT_MAX); }, "oversized map construction rejects");
    VarInfo value;
    vm.Var_SetStringA(&value, "an alias of the destination");
    vm.Var_SetStringA(&value, value._str->_str);
    Check(value._str->_str == "an alias of the destination", "string setter supports self alias");
    vm.Var_Release(&value);
    auto map = vm.TableAlloc();
    auto set = vm.SetAlloc();
    VarInfo key(17);
    map->Insert(&key, 91);
    set->Insert(&key);
    for (int count : {MAX_HASH_ITEMS + 1, INT_MAX})
    {
        Limit([&] { map->Reserve(count); }, "map oversized reserve rejects");
        Limit([&] { set->Reserve(count); }, "set oversized reserve rejects");
        Check(map->GetCount() == 1 && map->FindInt(17) && map->FindInt(17)->_int == 91,
            "failed reserve preserves map");
        Check(set->GetCount() == 1 && set->Find(&key), "failed reserve preserves set");
    }
    // Aligned and negative IDs must agree across general lookup and FindInt.
    for (int i = -500; i < 500; ++i)
    {
        VarInfo k(i * 65536);
        map->Insert(&k, i);
        set->Insert(&k);
    }
    map->Reserve(4096);
    set->Reserve(4096);
    for (int i = -500; i < 500; ++i)
    {
        VarInfo k(i * 65536);
        auto general = map->Find(&k);
        Check(general && general == map->FindInt(k._int) && general->_int == i, "integer lookup after rehash");
        Check(set->Find(&k), "aligned set key after rehash");
        if ((i & 1) == 0) { map->Remove(&k); set->Remove(&k); }
    }
    for (int i = -500; i < 500; ++i)
    {
        VarInfo k(i * 65536);
        Check((map->FindInt(k._int) != nullptr) == ((i & 1) != 0), "map deletion preserves chain");
        Check(set->Find(&k) == ((i & 1) != 0), "set deletion preserves chain");
    }
    vm.FreeTable(map);
    vm.FreeSet(set);
}

static void IntegerKeyPatterns()
{
    struct Pattern { const char* name; std::vector<int> keys; };
    std::vector<Pattern> patterns = {
        {"sequential", {}}, {"large positive sequential", {}}, {"negative sequential", {}},
        {"minimum integer sequential", {}}, {"maximum integer sequential", {}},
        {"word boundary sequential", {}}, {"aligned IDs", {}}, {"packed grid", {}},
        {"negative packed grid", {}}
    };
    for (int i = 0; i < 4096; ++i)
    {
        patterns[0].keys.push_back(i);
        patterns[1].keys.push_back(0x40000000 + i);
        patterns[2].keys.push_back(-4096 + i);
        patterns[3].keys.push_back(INT_MIN + i);
        patterns[4].keys.push_back(INT_MAX - 4095 + i);
        patterns[5].keys.push_back(64000 + i);
        patterns[6].keys.push_back((i - 2048) * 65536);
    }
    for (int x = 0; x < 128; ++x) for (int y = 0; y < 128; ++y)
    {
        patterns[7].keys.push_back(x * 65536 + y);
        patterns[8].keys.push_back((x - 64) * 65536 + y);
    }
    CNeoVM vm;
    for (const auto& pattern : patterns)
    {
        auto map = vm.TableAlloc();
        auto set = vm.SetAlloc();
        const int count = static_cast<int>(pattern.keys.size());
        for (int i = 0; i < count; ++i)
        {
            VarInfo key(pattern.keys[i]);
            if (i & 1) map->Insert(&key, i + 1); else map->Insert(key._int, i + 1);
            set->Insert(&key);
        }
        Check(map->GetCount() == count && set->GetCount() == count, pattern.name);
        // 실제 맵과 집합 노드에서 버킷 분포를 확인한다.
        // 정렬된 ID나 압축 좌표가 긴 충돌 사슬로 몰리는 회귀를 잡는다.
        std::vector<int> mapBuckets(map->_BucketCapa), setBuckets(set->_BucketCapa);
        for (int i = 0; i < map->_BucketCapa; ++i)
            if (IsMapNodeUsed(map->_Bucket[i])) ++mapBuckets[map->_Bucket[i].hash & map->_HashBase];
        for (int i = 0; i < set->_BucketCapa; ++i)
            if (IsSetNodeUsed(set->_Bucket[i])) ++setBuckets[set->_Bucket[i].hash & set->_HashBase];
        Check(*std::max_element(mapBuckets.begin(), mapBuckets.end()) <= 16,
            std::string(pattern.name) + " map bucket distribution");
        Check(*std::max_element(setBuckets.begin(), setBuckets.end()) <= 16,
            std::string(pattern.name) + " set bucket distribution");
        map->Reserve(count * 2);
        set->Reserve(count * 2);
        for (int i = 0; i < count; ++i)
        {
            VarInfo key(pattern.keys[i]);
            auto value = map->Find(&key);
            Check(value && value == map->FindInt(key._int) && value->_int == i + 1,
                std::string(pattern.name) + " read after rehash");
            Check(set->Find(&key), std::string(pattern.name) + " set after rehash");
            if (i % 3 == 0) { map->Remove(&key); set->Remove(&key); }
        }
        for (int i = 0; i < count; ++i)
        {
            VarInfo key(pattern.keys[i]);
            auto value = map->FindInt(key._int);
            Check((value != nullptr) == (i % 3 != 0) && (!value || value->_int == i + 1),
                std::string(pattern.name) + " map after deletion");
            Check(set->Find(&key) == (i % 3 != 0), std::string(pattern.name) + " set after deletion");
            if (i % 3 == 0) { map->Insert(&key, i + 1); set->Insert(&key); }
        }
        Check(map->GetCount() == count && set->GetCount() == count,
            std::string(pattern.name) + " reinsertion");
        vm.FreeTable(map);
        vm.FreeSet(set);
    }
}

static void Scripts(IRuntime* runtime, bool debug, bool image)
{
    CompileDesc desc;
    desc.sourceName = "storage-limits.ns";
    desc.includeDebugInfo = debug;
    desc.source = R"NS(
import system;
var saved = {"key":17};
export fun TooBig() { saved.reserve(805306369); }
export fun Healthy() { return saved.key == 17 && saved.len() == 1; }
export fun Probe(var input) {
    var arr = system.array(0, 2);
    arr[0] = input;
    arr[1] = input;
    return toint(input) == arr[0] && arr[0] == arr[1];
}
export fun Converted(var input) { return toint(input); }
export fun Text() {
    var a="abc"; a = a .. "def";
    return a == "abcdef" && "abcabc".replaceAll("a", "xy") == "xybcxybc";
}
)NS";
    ProgramHandle program;
    Error error;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) program = runtime->LoadProgram(bytes, &error);
    }
    else { auto c = runtime->Compile(desc); program = c.program; error = c.error; }
    Check(bool(program), "storage compile: " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "storage instance");
    if (instance)
    {
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            auto bad = runtime->Call(instance, "TooBig");
            Check(bad.timeout(1000).invoke() != RunStatus::Completed &&
                bad.error().message.find("capacity exceeds limit") != std::string::npos, "oversized reserve runtime error");
            for (const char* name : {"Healthy", "Text"})
            {
                auto call = runtime->Call(instance, name);
                Check(call.invoke() == RunStatus::Completed && call.retBool(), std::string(name) + " after error");
            }
        }
        const float inf = std::numeric_limits<float>::infinity();
        const float inputs[] = {0, -0.0f, 1.75f, -1.75f, 2147483520.0f, 2147483648.0f,
            -2147483648.0f, -2147483904.0f, inf, -inf, std::numeric_limits<float>::quiet_NaN()};
        const int64_t expected[] = {0, 0, 1, -1, 2147483520LL, 2147483648LL, -2147483648LL, -2147483904LL, INT64_MAX, INT64_MIN, 0};
        for (size_t n = 0; n < sizeof(inputs) / sizeof(inputs[0]); ++n)
        {
            auto call = runtime->Call(instance, "Converted");
            call.argFloat(inputs[n]);
            Check(call.invoke() == RunStatus::Completed && call.retInt() == expected[n], "float to int boundary");
            auto array = runtime->Call(instance, "Probe");
            array.argFloat(inputs[n]);
            Check(array.invoke() == RunStatus::Completed && array.retBool(), "array coercion matches toint");
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

int main()
{
    StorageLimits();
    IntegerKeyPatterns();
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true}) Scripts(runtime, debug, image);
    DestroyRuntime(runtime);
    std::printf("hunt_storage_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
