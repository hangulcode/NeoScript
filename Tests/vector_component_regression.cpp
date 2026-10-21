#include "NeoScript.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0;
static int failures = 0;

// Position: the getter always returns (8, 8, 8); the setter records what the script wrote so a
// component write through the property (Host.Position[0] += 2) can be observed via Last.
// ReadOnly rejects writes, which the VM reports as "cannot write property".
static float g_lastWritten[3] = {0.0f, 0.0f, 0.0f};
static int g_positionReads = 0;   // Reads: how many times the Position getter ran
static bool HostProperty(CallContext& ctx, StringView name, bool get)
{
    if (name.str() == "Position")
    {
        if (get) { ++g_positionReads; ctx.retVec3(8.0f, 8.0f, 8.0f); return true; }
        float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        ctx.argVec(0, v);
        g_lastWritten[0] = v[0]; g_lastWritten[1] = v[1]; g_lastWritten[2] = v[2];
        return true;
    }
    if (name.str() == "Reads" && get)
    {
        ctx.retInt(g_positionReads);
        return true;
    }
    if (name.str() == "Last" && get)
    {
        ctx.retVec3(g_lastWritten[0], g_lastWritten[1], g_lastWritten[2]);
        return true;
    }
    if (name.str() == "ReadOnly")
    {
        if (get) { ctx.retVec3(1.0f, 1.0f, 1.0f); return true; }
        return false;
    }
    return false;
}

static void Run(IRuntime* runtime, const std::string& source, bool debug, bool image,
    const char* expectedError = nullptr)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "vector-components.ns";
    desc.includeDebugInfo = debug;
    ProgramHandle program;
    Error error;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) program = runtime->LoadProgram(bytes, &error);
    }
    else
    {
        const auto result = runtime->Compile(desc);
        program = result.program;
        error = result.error;
    }
    if (!program)
    {
        ++checks;
        ++failures;
        std::fprintf(stderr, "Compile failed: %s\n%s\n", source.c_str(), error.message.c_str());
        return;
    }
    // Both instances use the same program. A write in the first must not mutate
    // constants or the initial vector state in the second.
    for (int i = 0; i < 2; ++i)
    {
        ++checks;
        auto instance = runtime->CreateInstance(program);
        bool ok = false;
        std::string message;
        if (instance)
        {
            {
                auto call = runtime->Call(instance, "Test");
                const auto status = call.invoke();
                message = call.error().message;
                ok = expectedError
                    ? status == RunStatus::Failed && message.find(expectedError) != std::string::npos
                    : status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool();
            }
            if (ok && expectedError)
            {
                auto call = runtime->Call(instance, "Verify");
                ok = call.invoke() == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool();
            }
            runtime->DestroyInstance(instance);
        }
        if (!ok)
        {
            ++failures;
            std::fprintf(stderr, "FAIL debug=%d image=%d instance=%d\n%s\n%s\n",
                debug, image, i, source.c_str(), message.c_str());
        }
    }
    runtime->DestroyProgram(program);
}

int main()
{
    auto runtime = CreateRuntime();
    NativeObjectDesc host;
    host.name = "Host";
    host.property = HostProperty;
    runtime->RegisterObject(host);
    runtime->FreezeBindings();
    const char* constructors[] = {
        "math.Vector2(8,8)", "math.Vector3(8,8,8)",
        "math.Vector4(8,8,8,8)", "math.Quaternion(8,8,8,8)"
    };
    const int counts[] = {2, 3, 4, 4};
    const char* operators[] = {"+=", "-=", "*=", "/="};
    const char* operands[] = {"2", "0.5"};
    const char* expected[2][4] = {{"10.0", "6.0", "16.0", "4.0"}, {"8.5", "7.5", "4.0", "16.0"}};
    for (bool debug : {false, true}) for (bool image : {false, true})
    {
        for (int type = 0; type < 4; ++type)
        for (int component = 0; component < counts[type]; ++component)
        for (int op = 0; op < 4; ++op)
        for (int rhs = 0; rhs < 2; ++rhs)
        {
            const std::string index = std::to_string(component);
            const std::string other = std::to_string((component + 1) % counts[type]);
            const std::string source = std::string("import math; export fun Test(){var v=")
                + constructors[type] + ";var copy=v;v[" + index + "]" + operators[op] + operands[rhs]
                + ";return v[" + index + "]==" + expected[rhs][op] + " && copy[" + index + "]==8.0"
                + " && v[" + other + "]==8.0 && tosize(v)==" + std::to_string(counts[type]) + ";}";
            Run(runtime, source, debug, image);
        }
        const char* success[] = {
            "import math; var v=math.Vector3(8,8,8); var copy=v; export fun Test(){v[0]+=2;return v[0]==10 && copy[0]==8;}",
            "import math; fun Modify(var v){v[0]+=3;return v;} export fun Test(){var a=math.Vector3(8,8,8);var b=Modify(a);return a[0]==8 && b[0]==11;}",
            "import math; var indices=0; var values=0; fun Index(){indices++;return 1;} fun Amount(){values++;return 2;}"
                "export fun Test(){var v=math.Vector3(8,8,8);v[Index()]+=Amount();return v[1]==10 && indices==1 && values==1;}",
            "import math; export fun Test(){var v=math.Vector3(2,4,8);v[0]+=v[1];return v[0]==6 && v[1]==4;}",
            "export fun Test(){var v=Host.Position;var copy=v;v[0]+=2;var original=Host.Position;return v[0]==10 && copy[0]==8 && original[0]==8;}",
            "import system; export fun Test(){var a=system.array(8.0,2);a[0]+=2;a[0]-=1;a[0]*=3;a[0]/=3;return a[0]==9 && a[1]==8;}",
            "export fun Test(){var a=[8];var m={\"x\":8};a[0]+=2;m.x-=2;return a[0]==10 && m.x==6;}",
            "import math; export fun Test(){var v=math.Vector3(8,8,8);var copy=v;v[0]++;++v[1];v[2]--;--v[2];"
                "return v[0]==9 && v[1]==9 && v[2]==6 && copy[0]==8 && copy[1]==8 && copy[2]==8;}",
            "import math; export fun Test(){var v=math.Vector3(8,8,8);var a=v[0]++;var b=++v[1];var c=v[2]--;"
                "return a==8 && b==9 && c==8 && v[0]==9 && v[1]==9 && v[2]==7;}",
            "export fun Test(){Host.Position[0]+=2;var l=Host.Last;return l[0]==10 && l[1]==8 && l[2]==8;}",
            "export fun Test(){var before=Host.Reads;Host.Position[0]+=2;var mid=Host.Reads;Host.Position[1]=1;"
                "return mid==before+1 && Host.Reads==before+2 && Host.Last[1]==1 && Host.Last[0]==8;}",
            "export fun Test(){Host.Position[1]=5;var a=Host.Last;Host.Position[2]--;var b=Host.Last;return a[1]==5 && a[2]==8 && b[1]==8 && b[2]==7;}",
            "import math; export fun Test(){var l=[math.Vector3(8,8,8)];l[0][0]*=2;var m={};m.v=math.Vector2(8,8);m.v[1]-=3;return l[0][0]==16 && m.v[1]==5;}"
        };
        for (const char* source : success) Run(runtime, source, debug, image);

        const std::string prefix = "import math;var v=math.Vector3(8,8,8);var copy=v;export fun Test(){";
        const std::string suffix = "}export fun Verify(){return v[0]==8 && v[1]==8 && v[2]==8 && copy==v;}";
        for (const char* op : operators)
        {
            for (const char* index : {"-1", "3", "2147483647", "0.0", "true", "\"x\""})
                Run(runtime, prefix + "v[" + index + "]" + op + "2;" + suffix,
                    debug, image, "vector index out of range");
            for (const char* rhs : {"null", "true", "\"x\"", "[]", "{}", "math.Vector3(1,2,3)"})
                Run(runtime, prefix + "v[0]" + op + rhs + ";" + suffix, debug, image, "invalid operator");
        }
        Run(runtime, prefix + "v[0]%=2;" + suffix, debug, image, "invalid operator '%='");
        Run(runtime, "export fun Test(){Host.ReadOnly[0]+=1;}export fun Verify(){return Host.ReadOnly[0]==1;}",
            debug, image, "cannot write property 'ReadOnly'");
    }
    DestroyRuntime(runtime);
    std::printf("Vector component regression %s: %d/%d\n", failures ? "FAIL" : "PASS", checks-failures, checks);
    return failures ? 1 : 0;
}
