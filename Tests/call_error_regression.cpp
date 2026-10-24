#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace NeoScript;

static bool Method(CallContext& context, StringView name)
{
    if (name.str() == "Detailed") context.fail(77, "specific native reason");
    if (name.str() == "Good") { context.retInt(7); return true; }
    return false;
}
static bool Property(CallContext& context, StringView name, bool)
{
    if (name.str() == "Detailed") context.fail(78, "specific property reason");
    return false;
}
struct Case
{
    const char* body;
    const char* expected;
    int code;

    Case(const char* source, const char* message, int errorCode = 1)
        : body(source), expected(message), code(errorCode) {}
};
int main()
{
    auto runtime = CreateRuntime();
    NativeObjectDesc host; host.name = "Host"; host.method = Method; host.property = Property;
    runtime->RegisterObject(host);
    runtime->FreezeBindings();
    const Case cases[] = {
        { "return Host.Missing();", "invalid function call 'Missing'" },
        { "return Host.UIScreenRect;", "cannot read property 'UIScreenRect'" },
        { "Host.FillColor = 1;", "cannot write property 'FillColor'" },
        { "var obj=Host; return obj.Missing();", "invalid function call 'Missing'" },
        { "var obj=Host; var key=\"UIScreenRect\"; return obj[key];", "cannot read property 'UIScreenRect'" },
        { "var a=[]; a.missing();", "invalid function call 'missing'" },
        { "return \"abc\".find(5);", "invalid function call 'find'" },
        { "return format(7);", "invalid function call 'format'" },
        { "var f=null; f();", "cannot call a value of type 'null'" },
        { "var obj=null; obj.Missing();", "invalid function call 'Missing'" },
        { "return Host.Detailed();", "specific native reason", 77 },
        { "return Host.Detailed;", "specific property reason", 78 },
        { "Host.Detailed = 1;", "specific property reason", 78 },
    };
    int failures = 0, checks = 0;
    for (bool debug : { false, true }) for (bool image : { false, true })
    {
        for (const Case& test : cases)
        {
            const std::string source = std::string("export fun Probe(){\n") + test.body +
                "\n}\nexport fun Good(){return Host.Good();}";
            CompileDesc desc; desc.source = source; desc.sourceName = "call-errors.ns"; desc.includeDebugInfo = debug;
            ProgramHandle program; Error error;
            if (image) { std::vector<uint8_t> bytes; error = runtime->CompileToBytecode(desc, bytes);
                if (error.ok()) program = runtime->LoadProgram(bytes, &error); }
            else { auto result = runtime->Compile(desc); program = result.program; error = result.error; }
            bool ok = false;
            if (program)
            {
                auto instance = runtime->CreateInstance(program);
                { auto call = runtime->Call(instance, "Probe"); const auto status = call.invoke(); error = call.error();
                  ok = status == RunStatus::Failed && error.code == test.code &&
                      error.message.find(test.expected) == 0 && error.message.find("IP(") != std::string::npos &&
                      error.message.find(debug ? "Line(2)" : "Line(-1)") != std::string::npos; }
                // A failed invocation must not contaminate the next call on the same instance.
                { auto call = runtime->Call(instance, "Good");
                  ok = ok && call.invoke() == RunStatus::Completed && call.retInt() == 7 && call.error().ok(); }
                runtime->DestroyInstance(instance); runtime->DestroyProgram(program);
            }
            ++checks;
            if (!ok) { ++failures; std::printf("FAIL debug=%d image=%d %s: %s\n", debug, image, test.body, error.message.c_str()); }
        }
    }
    DestroyRuntime(runtime);
    std::printf("call errors: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
