#include "NeoScript.h"

#include <cstdio>

using namespace NeoScript;

static const char* const Source = R"NS(
var g = 0;
export fun Reset() { g = 0; }
export fun State() { return g; }
export fun Plain(var c, var inner, var alt)
{
    if (c == true)
    {
        g = 1;
        if (inner == true) { g = 2; return false; }
    }
    else g = 99;
    return true;
}
export fun Braced(var c, var inner, var alt)
{
    if (c)
    {
        g = 1;
        if (inner) { g = 2; return false; }
    }
    else { g = 99; }
    return true;
}
export fun Chain(var c, var inner, var alt)
{
    if (c)
    {
        g = 1;
        if (inner) { g = 2; return false; }
    }
    else if (alt) g = 3;
    else g = 99;
    return true;
}
export fun LaterChain(var c, var inner, var alt)
{
    if (c) g = 10;
    else if (alt)
    {
        g = 1;
        if (inner) { g = 2; return false; }
    }
    else g = 99;
    return true;
}
export fun Logic(var c, var inner, var alt)
{
    if (c && alt)
    {
        g = 1;
        if (inner) { g = 2; return false; }
    }
    else g = 99;
    return true;
}
export fun Loop(var c, var inner, var alt)
{
    if (c)
    {
        g = 1;
        while (inner) { g = 2; return false; }
    }
    else g = 99;
    return true;
}
export fun Implicit(var c, var inner, var alt)
{
    if (c) { g = 1; }
    else if (alt) return true;
    else return true;
}
export fun Direct(var c, var inner, var alt)
{
    if (c) return true;
    else return false;
}
export fun CompleteChain(var c, var inner, var alt)
{
    if (c) return true;
    else if (inner) return false;
    else return alt;
}
export fun NestedComplete(var c, var inner, var alt)
{
    if (c) { if (inner) return true; else return false; }
    else return alt;
}
export fun BareBlock(var c, var inner, var alt)
{
    if (c) { { return true; } }
    else return false;
}
export fun PrefixReturn(var c, var inner, var alt)
{
    if (c) { return true; g = 99; }
    else return false;
}
export fun BreakBeforeReturn(var c, var inner, var alt)
{
    for (var i in 0, 1, 1)
    {
        if (c) { if (inner) break; g = 1; return true; }
        else { g = 99; return false; }
    }
    g = 3;
    return alt;
}
export fun ContinueBeforeReturn(var c, var inner, var alt)
{
    for (var i in 0, 1, 1)
    {
        if (c) { if (inner) continue; g = 1; return true; }
        else { g = 99; return false; }
    }
    g = 3;
    return alt;
}
export fun ImplicitNested(var c, var inner, var alt)
{
    if (c) { g = 1; if (inner) { g = 2; return false; } }
    else return true;
}
export fun EmptyNested(var c, var inner, var alt)
{
    if (c) { g = 1; if (inner) {} }
    else g = 99;
    return true;
}
)NS";

int main()
{
    IRuntime* runtime = CreateRuntime(RuntimeDesc{});
    runtime->FreezeBindings();
    const char* const functions[] = {
        "Plain", "Braced", "Chain", "LaterChain", "Logic", "Loop", "Implicit", "Direct",
        "CompleteChain", "NestedComplete", "BareBlock", "PrefixReturn", "BreakBeforeReturn",
        "ContinueBeforeReturn", "ImplicitNested", "EmptyNested"
    };
    constexpr std::size_t functionCount = sizeof(functions) / sizeof(functions[0]);
    int total = 0;
    int failed = 0;
    for (bool debug : { false, true })
    {
        CompileDesc desc;
        desc.source = Source;
        desc.sourceName = "control_flow_regression.ns";
        desc.includeDebugInfo = debug;
        CompileResult compiled = runtime->Compile(desc);
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
        for (int bits = 0; bits < 8; ++bits)
        {
            const bool c = (bits & 1) != 0;
            const bool inner = (bits & 2) != 0;
            const bool alt = (bits & 4) != 0;
            const int nested = inner ? 2 : 1;
            const int expectedState[] = {
                c ? nested : 99, c ? nested : 99,
                c ? nested : (alt ? 3 : 99), c ? 10 : (alt ? nested : 99),
                (c && alt) ? nested : 99, c ? nested : 99, c ? 1 : 0, 0,
                0, 0, 0, 0, c ? (inner ? 3 : 1) : 99,
                c ? (inner ? 3 : 1) : 99, c ? nested : 0, c ? 1 : 99
            };
            const bool expectedReturn[] = {
                !(c && inner), !(c && inner), !(c && inner), !(!c && alt && inner),
                !(c && alt && inner), !(c && inner), true, c,
                c || (!inner && alt), c ? inner : alt, c, c,
                c && (!inner || alt), c && (!inner || alt), !c, true
            };
            static_assert(sizeof(expectedState) / sizeof(expectedState[0]) == functionCount, "State cases");
            static_assert(sizeof(expectedReturn) / sizeof(expectedReturn[0]) == functionCount, "Return cases");
            for (std::size_t i = 0; i < functionCount; ++i)
            {
                ++total;
                bool ok = runtime->Call(instance, "Reset").invoke() == RunStatus::Completed;
                {
                    Invocation call = runtime->Call(instance, functions[i]);
                    ok = call.argBool(c).argBool(inner).argBool(alt).invoke() == RunStatus::Completed && ok;
                    if ((i == 6 && c) || (i == 14 && c && !inner))
                        ok = call.retType() == ValueType::None && ok;
                    else
                        ok = call.retType() == ValueType::Bool && call.retBool() == expectedReturn[i] && ok;
                }
                Invocation state = runtime->Call(instance, "State");
                ok = state.invoke() == RunStatus::Completed && state.retInt() == expectedState[i] && ok;
                if (!ok)
                {
                    ++failed;
                    std::fprintf(stderr, "%s(c=%d, inner=%d, alt=%d, debug=%d): g=%d, expected %d\n",
                        functions[i], c, inner, alt, debug, state.retInt(), expectedState[i]);
                }
            }
        }
        runtime->DestroyInstance(instance);
        runtime->DestroyProgram(compiled.program);
    }

    // Counts include two VM startup operations and one global-init RET.
    // These checks fail if dead JMPs or redundant default RETs are restored.
    struct ShapeCase { const char* source; std::size_t instructions; };
    const ShapeCase shapes[] = {
        { "export fun F(var c) { if(c) return 1; else return 2; }", 6 },
        { "export fun F(var c, var alt) { if(c) return 1; else if(alt) return 2; else return 3; }", 8 },
        { "export fun F(var c, var inner) { if(c) { if(inner) return 1; else return 2; } else return 3; }", 8 },
        { "export fun F(var c) { if(c) { { return 1; } } else return 2; }", 6 },
        { "export fun F(var c) { if(c) return; else return; }", 6 }
    };
    for (bool debug : { false, true })
    {
        for (const ShapeCase& shape : shapes)
        {
            ++total;
            CompileDesc desc;
            desc.source = shape.source;
            desc.sourceName = "return_optimization.ns";
            desc.includeDebugInfo = debug;
            CompileResult compiled = runtime->Compile(desc);
            std::vector<DebugInstruction> instructions;
            runtime->GetDebugInstructions(compiled.program, instructions);
            if (!compiled.program || instructions.size() != shape.instructions)
            {
                ++failed;
                std::fprintf(stderr, "Return optimization(debug=%d): %zu instructions, expected %zu: %s\n",
                    debug, instructions.size(), shape.instructions, compiled.error.message.c_str());
                std::fprintf(stderr, "%s\n", shape.source);
                for (const DebugInstruction& instruction : instructions)
                    std::fprintf(stderr, "%s\n", instruction.assembly.c_str());
            }
            if (compiled.program) runtime->DestroyProgram(compiled.program);
        }
    }

    // The former conditional-chain keyword must never compile as syntax.
    const char* const invalid[] = {
        "fun F() { if (true) {} elif (false) {} }",
        "fun F() { if (true) return 1; elif (false) return 2; else return 3; }"
    };
    for (const char* source : invalid)
    {
        ++total;
        CompileDesc desc;
        desc.source = source;
        desc.sourceName = "removed_keyword.ns";
        CompileResult compiled = runtime->Compile(desc);
        if (compiled.program || compiled.error.message.empty())
        {
            ++failed;
            std::fputs("Legacy conditional syntax was not rejected\n", stderr);
        }
        if (compiled.program) runtime->DestroyProgram(compiled.program);
    }
    DestroyRuntime(runtime);
    std::printf("Control flow regression %s: %d/%d\n", failed == 0 ? "PASS" : "FAIL", total - failed, total);
    return failed == 0 ? 0 : 1;
}
