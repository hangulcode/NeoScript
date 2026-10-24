#include "NeoScript.h"
#include "NeoVMProgram.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;

static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label.c_str()); }
}

static bool Host(CallContext& ctx, StringView name)
{
    if (name.str() != "ArrayCount") return false;
    AllocStats stats;
    GetAllocStats(stats);
    ctx.retInt(stats.arrays);
    return true;
}

static ProgramHandle Compile(IRuntime* runtime, const std::string& source,
    bool debug, bool image, Error& error)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "foreach-source.ns";
    desc.includeDebugInfo = debug;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        return error.ok() ? runtime->LoadProgram(bytes, &error) : ProgramHandle{};
    }
    auto result = runtime->Compile(desc);
    error = result.error;
    return result.program;
}

static void Run(IRuntime* runtime, const std::string& label, const std::string& body,
    bool debug, bool image, const char* expectedError = nullptr)
{
    AllocStats before;
    GetAllocStats(before);
    Error error;
    auto program = Compile(runtime, "import system;import math;" + body +
        "\nexport fun Recovery(){return 23;}", debug, image, error);
    const std::string context = label + " debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(bool(program), context + " compile: " + error.message);
    if (!program) return;
    for (int copy = 0; copy < 2; ++copy)
    {
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), context + " instance");
        if (!instance) continue;
        // Reusing the same instance covers stale iterator/local slots after
        // normal completion, break, return and a failed mutation check.
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            {
                auto call = runtime->Call(instance, "Probe");
                const auto status = call.timeout(1000).invoke();
                Check(expectedError
                    ? status == RunStatus::Failed && call.error().message.find(expectedError) != std::string::npos
                    : status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                    context + " execute: " + call.error().message);
            }
            {
                auto call = runtime->Call(instance, "Recovery");
                Check(call.invoke() == RunStatus::Completed && call.retInt() == 23, context + " recovery");
            }
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    runtime->CollectCycles(true);
    AllocStats after;
    GetAllocStats(after);
    Check(after.maps == before.maps && after.lists == before.lists && after.sets == before.sets &&
        after.arrays == before.arrays && after.vectors == before.vectors &&
        after.closures == before.closures && after.bufferBytes == before.bufferBytes,
        context + " releases retained sources");
}

static const char* legacySource = R"NS(
import system;
var sourceCalls=0;
var shared=[1,2,3];
fun Make() { sourceCalls+=1; return [1,2,3]; }
fun Shared() { return shared; }
export fun NestedLiterals()
{
    var result="";
    foreach (var a in ["A","B","C"]) {
        foreach (var b in ["x","y"]) {
            foreach (var c in [1,2]) { result=result..a..b..c; }
        }
    }
    return result=="Ax1Ax2Ay1Ay2Bx1Bx2By1By2Cx1Cx2Cy1Cy2";
}
export fun BodyTemporaries()
{
    var sum=0;
    foreach (var value in [1,2,3]) {
        var trash=[90,91,92];
        var map={"a":trash};
        var text="value="..value;
        sum+=value;
    }
    return sum==6;
}
export fun ReturnValue()
{
    sourceCalls=0; var sum=0;
    foreach (var a in Make()) { foreach (var b in Make()) sum+=a*b; }
    return sum==36 && sourceCalls==4;
}
export fun NativeResults()
{
    var sum=0;
    foreach (var a in system.array(2,3)) { var noise=[9,8]; sum+=a; }
    foreach (var b in system.set([1,2,2,3])) { var noise=[9,8]; sum+=b; }
    var map={"a":7,"b":8}; var keys="";
    foreach (var k in map.keys()) { var noise=[9,8]; keys=keys..k; }
    return sum==12 && (keys=="ab" || keys=="ba");
}
export fun MapAndSubscript()
{
    var sum=0;
    foreach (var k,v in {"a":10,"b":20}) {
        foreach (var inner in [1,2]) sum+=v;
    }
    var lists=[[1,2,3],[4]];
    foreach (var item in lists[0]) { var noise=[8,9]; sum+=item; }
    return sum==66;
}
export fun Exits()
{
    var sum=0;
    foreach (var a in [1,2,3]) {
        if (a==2) continue;
        foreach (var b in [4,5]) { sum+=a*b; break; }
    }
    foreach (var a in [10,20]) sum+=a;
    return sum==46;
}
export fun EarlyReturn()
{
    foreach (var a in [1,2,3]) {
        foreach (var b in [4,5]) { var noise=[0]; return a==1 && b==4; }
    }
    return false;
}
export fun Empty()
{
    var sum=0;
    foreach (var a in []) sum+=100;
    foreach (var a in [1,2]) { foreach (var b in []) sum+=100; sum+=a; }
    return sum==3;
}
export fun Mutation()
{
    shared=[1,2,3];
    foreach (var value in Shared()) { shared.append(4); }
    return true;
}
)NS";


static void ExistingSources(IRuntime* runtime, bool debug, bool image)
{
    Error error;
    auto program = Compile(runtime, legacySource, debug, image, error);
    const std::string context = "original sources debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(bool(program), context + " compile: " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), context + " instance");
    if (instance)
    {
        // Preserve the original suite's cross-call ordering, including a
        // mutation failure followed by another call in the same instance.
        for (int repeat = 0; repeat < 3; ++repeat)
            for (const char* name : {"NestedLiterals", "BodyTemporaries", "ReturnValue", "NativeResults",
                "MapAndSubscript", "Exits", "EarlyReturn", "Empty", "Mutation"})
            {
                auto call = runtime->Call(instance, name);
                const auto status = call.timeout(1000).invoke();
                const bool mutation = std::strcmp(name, "Mutation") == 0;
                Check(mutation
                    ? status == RunStatus::Failed && call.error().message.find("modified") != std::string::npos
                    : status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                    context + " " + name + " repeat=" + std::to_string(repeat) + ": " + call.error().message);
            }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    runtime->CollectCycles(true);
}

struct SourceCase
{
    const char* name;
    std::string initial;
    std::string replacement;
    bool map;
    bool string;
    int sum;
};

static void Rebinding(IRuntime* runtime, bool debug, bool image)
{
    const SourceCase cases[] = {
        {"list", "[1,2,3,4]", "[99]", false, false, 10},
        {"array", "system.array(3,4)", "system.array(99,1)", false, false, 12},
        {"map", "{\"a\":1,\"b\":2,\"c\":3,\"d\":4}", "{\"x\":99}", true, false, 10},
        {"set", "system.set([1,2,3,4])", "system.set([99])", false, false, 10},
        {"string", u8"\"A\uD55C\U0001F600Z\"", "\"Q\"", false, true, 4}
    };
    for (const auto& test : cases)
        for (const auto& replacement : {test.replacement, std::string("null"), std::string("[]"),
            std::string("{}"), std::string("\"replacement\""), std::string("math.Vector2(9,8)")})
            for (int scope = 0; scope < 3; ++scope)
            {
                std::string loop = "var count=0;var sum=0;var trace=\"\";foreach(";
                loop += test.map ? "var key,value in source){" : "var value in source){";
                loop += "source=" + replacement + ";count+=1;if(count>8)return false;";
                // Churn expression temporaries after dropping the user's last
                // reference. The retained loop source must remain alive.
                loop += "var noise=[{\"v\":math.Vector3(count,2,3)},system.array(7,16)];";
                loop += test.string ? "sum+=1;trace=trace..value;" : "sum+=value;";
                loop += "}return count==4 && sum==" + std::to_string(test.sum);
                if (test.string) loop += " && trace==" + test.initial;
                loop += ";";
                std::string source;
                if (scope == 0) source = "export fun Probe(){var source=" + test.initial + ";" + loop + "}";
                else if (scope == 1) source = "var source=null;export fun Probe(){source=" + test.initial + ";" + loop + "}";
                else source = "fun Each(var source){" + loop + "}export fun Probe(){return Each(" + test.initial + ");}";
                Run(runtime, std::string(test.name) + " rebind " + replacement + " scope=" + std::to_string(scope),
                    source, debug, image);
            }
}

static void Lifecycle(IRuntime* runtime, bool debug, bool image)
{
    Run(runtime, "nested break continue return", R"(
fun Early(){var source=[1,2,3];foreach(var v in source){source=null;return v;}return 0;}
export fun Probe(){
 var total=0;
 for(var round in 0,4){
  var source=[1,2,3];
  foreach(var v in source){source=null;var inner=[10,20,30];
   foreach(var w in inner){inner=null;total+=w;break;}
   if(v==2)continue;total+=v;
  }
 }
 return total==136 && Early()==1 && Early()==1;
})", debug, image);
    Run(runtime, "source evaluated once and temporaries retained", R"(
var calls=0;var holder=null;
fun Get(){calls+=1;return holder.source;}
export fun Probe(){calls=0;holder={"source":[1,2,3]};var sum=0;
 foreach(var v in Get()){holder.source=null;sum+=v;}
 holder.source=[4,5,6];foreach(var v in holder.source){holder.source=null;sum+=v;}
 foreach(var v in [7,8,9]){var noise=[1,2,3];sum+=v;}
 return calls==1 && sum==45;
})", debug, image);
    Run(runtime, "captured source and caller rebind", R"(
var source=null;
fun Replace(){source="replaced";}
export fun Probe(){source=[1,2,3];var sum=0;foreach(var v in source){Replace();sum+=v;}
 var local=[4,5,6];var f=fun(){var n=0;foreach(var v in local){local=null;n+=v;}return n;};
 return sum==6 && f()==15 && local.len()==3;
})", debug, image);
    // A top-level hidden snapshot is a global slot. Release it at normal/break
    // exit, rather than keeping the old array alive until instance destruction.
    for (bool early : {false, true})
        Run(runtime, early ? "global snapshot break cleanup" : "global snapshot normal cleanup",
            "var before=Host.ArrayCount();var source=system.array(7,4);var n=0;"
            "foreach(var v in source){source=null;n+=1;" + std::string(early ? "break;" : "") +
            "}var released=Host.ArrayCount()==before;export fun Probe(){return released && n==" +
            std::string(early ? "1" : "4") + ";}", debug, image);
    Run(runtime, "empty sources", R"(
export fun Probe(){var count=0;var source=[];foreach(var v in source)count+=1;
 source=system.array(0,0);foreach(var v in source)count+=1;
 source={};foreach(var k,v in source)count+=1;
 source=system.set([]);foreach(var v in source)count+=1;
 source="";foreach(var v in source)count+=1;return count==0;}
)", debug, image);
}

static void Mutations(IRuntime* runtime, bool debug, bool image)
{
    for (const auto& item : std::vector<std::pair<std::string, std::string>>{
        {"[1,2,3]", "original.append(4)"},
        {"system.array(1,3)", "original.resize(4)"},
        {"{\"a\":1,\"b\":2}", "original[\"new\"]=4"},
        {"{\"a\":1,\"b\":2}", "original.reserve(1024)"}})
        Run(runtime, "original structural mutation " + item.second,
            "export fun Probe(){var source=" + item.first + ";var original=source;"
            "foreach(var v in source){source=null;" + item.second + ";}return true;}",
            debug, image, "collection was modified");
    for (const auto& item : std::vector<std::pair<std::string, int>>{
        {"[1,2,3]", 12}, {"system.array(1,3)", 11}})
        Run(runtime, "original element updates " + item.first,
            "export fun Probe(){var source=" + item.first + ";var original=source;var sum=0;"
            "foreach(var v in source){source=null;original[2]=9;sum+=v;}return sum==" +
            std::to_string(item.second) + ";}", debug, image);
    Run(runtime, "original map value updates", R"(
export fun Probe(){var source={"a":1,"b":2,"c":3};var original=source;
 var count=0;var valid=true;foreach(var key,value in source){source=null;
 if(count>0 && value!=9)valid=false;original.a=9;original.b=9;original.c=9;count+=1;}
 return valid && count==3;}
)", debug, image);
}

static void RejectPreviousImage(IRuntime* runtime)
{
    CompileDesc desc;
    desc.source = "export fun Probe(){var source=[1];foreach(var v in source)source=null;return true;}";
    desc.sourceName = "foreach-image.ns";
    std::vector<uint8_t> bytes;
    Error error = runtime->CompileToBytecode(desc, bytes);
    Check(error.ok() && bytes.size() >= sizeof(SNeoVMHeader), "compile image for previous-version rejection");
    if (!error.ok() || bytes.size() < sizeof(SNeoVMHeader)) return;
    SNeoVMHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    header._dwNeoVersion = ('0' << 24) | ('1' << 16) | ('3' << 8) | '0';
    std::memcpy(bytes.data(), &header, sizeof(header));
    auto old = runtime->LoadProgram(bytes, &error);
    Check(!old && error.message.find("version mismatch") != std::string::npos,
        "reject 0130 foreach images compiled without retained source snapshots");
    if (old) runtime->DestroyProgram(old);
}

int main()
{
    auto runtime = CreateRuntime();
    NativeObjectDesc host;
    host.name = "Host";
    host.method = &Host;
    runtime->RegisterObject(host);
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true})
    {
        ExistingSources(runtime, debug, image);
        Rebinding(runtime, debug, image);
        Lifecycle(runtime, debug, image);
        Mutations(runtime, debug, image);
    }
    RejectPreviousImage(runtime);
    DestroyRuntime(runtime);
    std::printf("foreach sources: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
