#include "NeoScript.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace NeoScript;

class ModuleLoader final : public ILoader
{
public:
    bool Load(StringView path, std::vector<uint8_t>& out) override
    {
        if (std::string(path.data(), path.size()) != "postfix_module.ns")
            return false;
        const char* source =
            "fun L() { return [4, 5]; }"
            "fun S() { return \"hello\"; }"
            "fun Factory() { return L; }";
        out.assign(source, source + std::strlen(source));
        return true;
    }
    StringView LibPath() const override { return ""; }
};

static const char* const Prefix = R"NS(
import math;
import system;
import postfix_module;
var calls = 0;
var shared = { "values": [1, 2, 3], "value": 7 };
fun L() { calls += 1; return [1, 2, 3]; }
fun S() { return " abc,def "; }
fun Value() { return 7; }
fun GiveL() { return L; }
fun GiveValue() { return Value; }
fun Functions() { return [L, Value]; }
fun Box() { return { "items": [1, 2, 3], "text": "hello", "load": L, "factory": GiveL }; }
fun Shared() { return shared; }
fun Echo(var value) { return value; }
fun Index() { calls += 1; return 1; }
)NS";

int main()
{
    ModuleLoader loader;
    RuntimeDesc runtimeDesc;
    runtimeDesc.loader = &loader;
    IRuntime* runtime = CreateRuntime(runtimeDesc);
    runtime->FreezeBindings();
    // Each case compiles separately: one broken suffix must not hide the others.
    const char* const cases[] = {
        "return L().len() == 3;",
        "return S().len() == 9;",
        "return Echo(S().len()) == 9;",
        "return S().trim().upper().split(\",\")[1].lower() == \"def\";",
        "return S().trim().split(\",\")[0].len() == 3;",
        "var s = S(); return s.trim().upper().len() == 7;",
        "var f = L; return f().len() == 3;",
        "var f = L; return f()[1] == 2;",
        "var f = S; return f().trim().split(\",\")[1].len() == 3;",
        "return GiveL()().len() == 3;",
        "var factory = GiveL; return factory()()[2] == 3;",
        "return Functions()[0]().len() == 3;",
        "return Functions()[1]() == 7;",
        "return Box().items[1] == 2;",
        "return Box().items.len() == 3;",
        "return Box()[\"text\"].upper().len() == 5;",
        "return Box().load().len() == 3;",
        "return Box()[\"load\"]()[1] == 2;",
        "return Box().factory()().len() == 3;",
        "return Box().keys().len() == 4;",
        "return Echo(L()).len() == 3 && calls == 1;",
        "return L()[Index()] == 2 && calls == 2;",
        "var n = Echo(L().len()) + Echo(S().trim().len()); return n == 10 && calls == 1;",
        "L().len(); return calls == 1;",
        "return (false && L().len() == 3) == false && calls == 0;",
        "return system.array(4, 3).len() == 3;",
        "return system.array(4, 3)[2] == 4;",
        "return math.Vector3(1.0, 2.0, 3.0)[0] == 1.0;",
        "return postfix_module.L().len() == 2;",
        "return postfix_module.S().upper().len() == 5;",
        "return postfix_module.Factory()()[1] == 5;",
        "return -Value() == -7;",
        "var f = Value; return -f() == -7;",
        "return -GiveValue()() == -7;",
        "return -L()[1] == -2 && calls == 1;",
        "return -S().trim().len() == -7;",
        "var a = L(); return -a.len() == -3;",
        "return +L().len() == 3;",
        "Shared().values[1] += 5; return shared.values[1] == 7;",
        "Shared().value = 9; return shared.value == 9;",
        "Shared().values.append(4); return shared.values.len() == 4;",
        "Shared().values[0]++; return shared.values[0] == 2;"
    };
    const char* const invalid[] = {
        "return L().;",
        "return L().(1);",
        "return L()[];",
        "return L()[1;",
        "return L().len(,);"
    };
    int total = 0;
    int failed = 0;
    for (bool debug : { false, true })
    {
        for (const char* body : cases)
        {
            ++total;
            const std::string source = std::string(Prefix) + "export fun Test() { " + body + " }";
            CompileDesc desc;
            desc.source = source;
            desc.sourceName = "postfix_regression.ns";
            desc.includeDebugInfo = debug;
            CompileResult compiled = runtime->Compile(desc);
            bool ok = false;
            if (compiled.program)
            {
                InstanceHandle instance = runtime->CreateInstance(compiled.program);
                if (instance)
                {
                    {
                        Invocation call = runtime->Call(instance, "Test");
                        ok = call.invoke() == RunStatus::Completed
                            && call.retType() == ValueType::Bool && call.retBool();
                    }
                    runtime->DestroyInstance(instance);
                }
                runtime->DestroyProgram(compiled.program);
            }
            if (!ok)
            {
                ++failed;
                std::fprintf(stderr, "Postfix(debug=%d): %s\n%s\n", debug, body, compiled.error.message.c_str());
            }
        }
        for (const char* body : invalid)
        {
            ++total;
            const std::string source = std::string(Prefix) + "export fun Test() { " + body + " }";
            CompileDesc desc;
            desc.source = source;
            desc.sourceName = "invalid_postfix.ns";
            desc.includeDebugInfo = debug;
            CompileResult compiled = runtime->Compile(desc);
            if (compiled.program || compiled.error.message.empty())
            {
                ++failed;
                std::fprintf(stderr, "Invalid postfix was not rejected(debug=%d): %s\n", debug, body);
            }
            if (compiled.program) runtime->DestroyProgram(compiled.program);
        }
    }
    DestroyRuntime(runtime);
    std::printf("Postfix regression %s: %d/%d\n", failed == 0 ? "PASS" : "FAIL", total - failed, total);
    return failed == 0 ? 0 : 1;
}
