#include "NeoScript.h"

#include <cstdio>
#include <iostream>
#include <sstream>

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
        "export fun add(var a, var b) { return a + b; }\n"
        "export fun literal() { return 1.25 + 2.75; }\n";
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

    runtime->DestroyInstance(instance);
    runtime->DestroyProgram(compiled.program);
    DestroyRuntime(runtime);

    const bool asmOk = asmOutput.str().find("Fun -") != std::string::npos;
    if (!addOk || !literalOk || !asmOk)
    {
        std::fputs("NeoScript API smoke failed\n", stderr);
        return 1;
    }
    return 0;
}
