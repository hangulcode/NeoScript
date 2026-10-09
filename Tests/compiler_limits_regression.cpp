#include "NeoScript.h"
#include "NeoParser.h"
#include "NeoVMProgram.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace NeoScript;

static int failures = 0;
static int checks = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label.c_str()); }
}

static CompileDesc Describe(const std::string& source, bool debug = false)
{
    CompileDesc d;
    d.source = source;
    d.sourceName = "compiler_limits.ns";
    d.includeDebugInfo = debug;
    return d;
}

static std::string Pool(int count)
{
    std::string s = "export var g = 3; var a = [];\n";
    for (int i = 0; i < count; ++i)
        s += "a.append(\"s" + std::to_string(i) + "\");" + (i % 2 ? std::string("\n") : std::string());
    return s;
}

static bool InvokeBool(IRuntime* rt, InstanceHandle instance)
{
    Invocation c = rt->Call(instance, "Test");
    return c.invoke() == RunStatus::Completed && c.retType() == ValueType::Bool && c.retBool();
}

static void Run(IRuntime* rt, const std::string& source, const std::string& label,
    bool debug = false, int expectedLoadK = -1, size_t minimumImage = 0, bool globals = false)
{
    std::vector<uint8_t> image;
    Error error = rt->CompileToBytecode(Describe(source, debug), image);
    Check(error.code == 0 && image.size() >= minimumImage, label + " compile: " + error.message);
    if (error.code || image.empty()) return;
    SNeoVMHeader h{};
    std::memcpy(&h, image.data(), sizeof(h));
    int loadks = 0;
    for (int offset = 0; offset < h._iCodeSize; offset += sizeof(SVMOperation))
    {
        SVMOperation op{};
        std::memcpy(&op, image.data() + sizeof(h) + offset, sizeof(op));
        if (op.op == NOP_LOADK) ++loadks;
    }
    if (expectedLoadK >= 0)
        Check(expectedLoadK ? loadks > 0 : loadks == 0, label + " opcode selection");
    ProgramHandle p = rt->LoadProgram(image, &error);
    Check((bool)p, label + " reload: " + error.message);
    if (!p) return;
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        InstanceHandle inst = rt->CreateInstance(p);
        Check((bool)inst && InvokeBool(rt, inst), label + " execute");
        if (globals && inst)
        {
            int32_t value = 0;
            Check(rt->GetGlobalInt(inst, "g", value) && value == 7, label + " host read global");
            Check(rt->SetGlobalInt(inst, "g", 91) && rt->GetGlobalInt(inst, "g", value) && value == 91,
                label + " host write global");
        }
        if (inst) rt->DestroyInstance(inst);
    }
    // Exercise the disassembler on large pools, without formatting tens of
    // thousands of separate functions in the function-count boundary tests.
    if (debug && h._iFunctionCount < 512)
    {
        std::vector<DebugInstruction> listing;
        rt->GetDebugInstructions(p, listing);
        Check(!listing.empty(), label + " disassembly");
    }
    rt->DestroyProgram(p);
    std::printf("PASS: %s (%zu bytes, %d LOADK)\n", label.c_str(), image.size(), loadks);
}

static void Reject(IRuntime* rt, const std::string& source, const char* reason, bool debug = false)
{
    std::vector<uint8_t> image{1, 2, 3};
    Error e = rt->CompileToBytecode(Describe(source, debug), image);
    Check(e.code != 0 && e.message.find(reason) != std::string::npos && image.empty(),
        std::string("reject ") + reason + ": " + e.message);
}

static std::string Functions(int n, bool unique)
{
    std::string s;
    for (int i = 0; i < n; ++i)
        s += "fun f" + std::to_string(i) + "() { return " + std::to_string(unique ? i : 7) + "; }\n";
    return s;
}

int main()
{
    IRuntime* rt = CreateRuntime(RuntimeDesc{});
    rt->FreezeBindings();
    for (bool debug : {false, true})
    {
        // Null assignment preserves list positions, but removes map keys. Run
        // both with direct constants and after a pool that requires LOADK.
        const char* const nullAssignments[] = {
            "var l=[1,2,3]; l[1]=null; export fun Test(){"
                "return l.len()==3 && l[0]==1 && l[1]==null && l[2]==3;}",
            "export fun Test(){var l=[1,2,3]; l[0]=null; l[2]=null;"
                "return l.len()==3 && l[0]==null && l[1]==2 && l[2]==null;}",
            "fun Clear(var x,var i){x[i]=null;} export fun Test(){"
                "var l=[1,2,3]; Clear(l,1); var m={\"keep\":1,\"drop\":2}; Clear(m,\"drop\");"
                "return l.len()==3 && l[0]==1 && l[1]==null && l[2]==3"
                " && m.len()==1 && m.keep==1 && m.drop==null;}",
            "export fun Test(){var l=[null,2,null];"
                "return l.len()==3 && l[0]==null && l[1]==2 && l[2]==null;}",
            "var box={\"items\":[1,2,3]}; fun Box(){return box;} export fun Test(){"
                "Box().items[1]=null; return box.items.len()==3 && box.items[0]==1"
                " && box.items[1]==null && box.items[2]==3;}",
            "var calls=0; fun Nothing(){calls+=1; return null;} export fun Test(){"
                "var l=[1,2,3]; var n=null; l[0]=n; l[2]=Nothing();"
                "return calls==1 && l.len()==3 && l[0]==null && l[1]==2 && l[2]==null;}",
            "export fun Test(){var l=[\"left\",{\"owned\":[1,2]},\"right\"]; l[1]=null;"
                "return l.len()==3 && l[0]==\"left\" && l[1]==null && l[2]==\"right\";}",
            "export fun Test(){var m={\"keep\":1,\"drop\":2}; m.drop=null; m[\"missing\"]=null;"
                "return m.len()==1 && m.keep==1 && m.drop==null;}",
            "export fun Test(){var m={}; m[1]=7; m[2]=8; m[1]=null; m[9]=null;"
                "return m.len()==1 && m[1]==null && m[2]==8;}"
        };
        for (bool largePool : {false, true})
        {
            const std::string prefix = largePool ? Pool(15000) : std::string();
            for (size_t i = 0; i < sizeof(nullAssignments) / sizeof(nullAssignments[0]); ++i)
                Run(rt, prefix + nullAssignments[i], "null assignment " + std::to_string(i)
                    + (largePool ? " large pool" : " small pool") + (debug ? " debug" : ""), debug);
        }
        for (int n : {14990, 14997, 20000, 33000, 66000})
        {
            const std::string s = Pool(n) +
                "fun Seven() { return 7; } export fun Test() { g = Seven(); return g == 7 && a.len() == "
                + std::to_string(n) + " && a[" + std::to_string(n - 1) + "] == \"s" + std::to_string(n - 1) + "\"; }";
            Run(rt, s, "pool " + std::to_string(n) + (debug ? " debug" : " release"),
                debug, n < 14997 ? 0 : 1, n == 66000 ? 1024 * 1024 + 1 : 0, true);
        }
        Run(rt, Pool(15000) + R"NS(
import math;
fun Seven() { return 7; }
fun Echo(var value) { return value; }
export fun Test() {
    var m = { "late": "value", "fn": Seven };
    var key = "late";
    m[key] = "new";
    var nums = [100001, 100002];
    var lo = 100000; var hi = 100010;
    var text = "upper";
    var flag = false;
    g = Seven();
    var f = m.fn;
    if (false) { return false; }
    return m.late == "new" && f() == 7 && Echo("late arg") == "late arg"
        && text.upper() == "UPPER" && ("left" .. "right") == "leftright"
        && nums[0] > lo && nums[1] < hi && -100003 == (0 - 100003)
        && math.Vector3(1.0, 2.0, 3.0)[2] == 3.0 && !flag && true;
}
)NS", debug ? "extended operands debug" : "extended operands", debug, 1, 0, true);
        Run(rt, "export fun Test() { var s=\"" + std::string(32767, 'x') + "\"; return s.len() == 32767; }",
            "maximum string", debug);
        Reject(rt, "var s=\"" + std::string(32768, 'x') + "\";", "string constant bytes", debug);
    }

    Run(rt, Functions(20000, true) + "export fun Test() { var f=f19999; return f19999()==19999 && f()==19999; }",
        "unique-return functions", true, 1);
    Run(rt, Functions(32766, false) + "export fun Test() { var f=f32765; return f32765()==7 && f()==7; }",
        "maximum function id", false, 0);
    Reject(rt, Functions(32768, false), "function id");

    std::string globals;
    for (int i = 0; i < 32767; ++i) globals += "var g" + std::to_string(i) + ";\n";
    for (bool debug : {false, true})
        Run(rt, globals + R"NS(
fun Value(var x) { return x; }
fun Nested(var x) { var y=Value(x); return y+1; }
fun NoValue() { return; }
export fun Test() {
    g0=Nested(2);
    g32766=Nested(8);
    var local=Nested(4);
    Nested(100);
    if (g0!=3 || g32766!=9 || local!=5) return false;
    g0=[1,2];
    g32766=[3,4];
    g0=NoValue();
    g32766=NoValue();
    return g0==null && g32766==null;
}
)NS", debug ? "global return slot boundaries debug" : "global return slot boundaries", debug);
    Reject(rt, globals + "var overflow;", "global variable count");
    std::string combined;
    for (int i = 0; i < 32765; ++i) combined += "var g" + std::to_string(i) + ";\n";
    Run(rt, combined + Pool(33000) +
        "fun Seven(){return 7;} export fun Test(){g=Seven(); g0=11; g32764=13;"
        "return a.len()==33000 && a[32999]==\"s32999\" && g0+g32764==24 && g==7;}",
        "large pool and maximum globals together", true, 1, 1024*1024, true);

    std::string locals;
    for (int i = 0; i < 9999; ++i) locals += "var v" + std::to_string(i) + ";";
    Run(rt, "export fun Test(){" + locals + "v9998=9; while(v9998<0){v9998+=1;} return v9998==9;}", "maximum locals, hoist fallback");
    Reject(rt, "fun f(){" + locals + "var overflow;}", "local variable slot");

    std::string params, args;
    for (int i = 0; i < COMPILE_MAX_ARGUMENTS; ++i)
    {
        params += (i ? ",var a" : "var a") + std::to_string(i);
        args += i ? ",1" : "1";
    }
    Run(rt, "fun f(" + params + "){return a2765;} export fun Test(){return f(" + args + ")==1;}", "maximum arguments");
    Reject(rt, "fun f(" + params + ",var extra){}", "function arguments");
    Reject(rt, "print(" + args + ",1);", "call arguments");

    std::string body;
    for (int i = 0; i < 32767; ++i) body += "g+=1;";
    Run(rt, "var g=0; export fun Test(){if(g<0){" + body + "} return g==0;}", "maximum forward jump");
    Run(rt, "var g=0; export fun Test(){while(g<0){" + body + "} return g==0;}", "maximum backward jump");
    body += "g+=1;";
    Reject(rt, "var g=0; fun f(var b){if(b){" + body + "} return g;}", "jump distance");
    Reject(rt, "var g=0; fun f(){while(g<1){" + body + "}}", "jump distance");
    Run(rt, std::string(65534, '\n') + "export fun Test(){return true;}", "last debug line", true);
    Reject(rt, std::string(65535, '\n') + "var a=1;", "debug line", true);
    Run(rt, std::string(65535, '\n') + "export fun Test(){return true;}", "wide source lines without debug");

    std::string tempList = "fun f(){var a=[";
    for (int i = 0; i < 5000; ++i) tempList += i ? ",[]" : "[]";
    Reject(rt, tempList + "];}", "temporary variable slot");

    std::string switches;
    for (int i = 0; i < 65536; ++i) switches += "switch(0){}";
    Run(rt, "export fun Test(){" + switches + "return true;}", "maximum switch tables", false, 0);
    Reject(rt, "fun f(){" + switches + "switch(0){}}", "switch");

    std::string codeOnly = "var g=0;";
    for (int i = 0; i < 140000; ++i) codeOnly += "g+=1;";
    Run(rt, codeOnly + "export fun Test(){return g==140000;}", "code archive above 1 MiB", false, 0, 1024*1024+1);
    std::string constantsOnly = "var a=[];";
    for (int i = 0; i < 1000; ++i)
        constantsOnly += "a.append(\"" + std::string(1100, 'x') + std::to_string(i) + "\");";
    Run(rt, constantsOnly + "export fun Test(){return a.len()==1000;}", "constant data above 1 MiB", false, 0, 1024*1024+1);
    std::string debugOnly = "var g=0;";
    for (int i = 0; i < 90000; ++i) debugOnly += "g+=1;";
    Run(rt, debugOnly + "export fun Test(){return g==90000;}", "debug data crosses 1 MiB", true, 0, 1024*1024+1);

    // A tiny explicit archive exercises the same failure path as the default
    // INT_MAX ceiling without allocating gigabytes in the test runner.
    CNArchive limited(64);
    std::string error;
    const char* small = "var a=1;";
    NeoCompilerParam param(small, (int)std::strlen(small)); param.err = &error;
    Check(!NeoVMSystem::Compile(limited, param) && limited.GetBufferOffset() == 0 &&
        error.find("script image/code bytes") != std::string::npos, "archive write failure reaches compiler");
    CNArchive archive(64);
    char bytes[64]{};
    archive.Write(bytes, sizeof(bytes));
    Check(archive.GetBufferOffset() == 64, "archive exact capacity");
    bool rejected = false;
    try { archive.Write(bytes, 1); } catch (const CompileLimitError&) { rejected = true; }
    Check(rejected && archive.GetBufferOffset() == 64, "archive capacity plus one");
    rejected = false;
    try { archive.Write(bytes, (int64_t)INT_MAX + 1); } catch (const CompileLimitError&) { rejected = true; }
    Check(rejected, "archive size checked before narrowing");

    DestroyRuntime(rt);
    std::printf("compiler limits: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
