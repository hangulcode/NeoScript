#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label.c_str()); }
}
static CompileDesc Describe(const std::string& source, bool debug) {
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "integer-division.ns";
    desc.includeDebugInfo = debug;
    return desc;
}
static ProgramHandle Compile(IRuntime* runtime, const std::string& source,
    bool debug, bool image, Error& error) {
    if (image) {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(Describe(source, debug), bytes);
        if (error.ok()) return runtime->LoadProgram(bytes, &error);
        Check(bytes.empty(), "failed compilation clears bytecode");
        return {};
    }
    auto compiled = runtime->Compile(Describe(source, debug));
    error = compiled.error;
    return compiled.program;
}
static void Run(IRuntime* runtime, const std::string& source, const char* reason,
    bool debug, bool image) {
    Error error;
    auto program = Compile(runtime, source, debug, image, error);
    Check(bool(program), "compile " + source + " " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "create instance");
    if (instance) {
        {
            auto call = runtime->Call(instance, "Probe");
            auto status = call.invoke();
            if (reason) {
                const auto& message = call.error().message;
                Check(status == RunStatus::Failed && message.find(reason) == 0 &&
                    message.find("IP(") != std::string::npos &&
                    (debug ? message.find("Line(-1)") == std::string::npos :
                             message.find("Line(-1)") != std::string::npos),
                    source + " " + message);
            } else {
                Check(status == RunStatus::Completed && call.retType() == ValueType::Bool &&
                    call.retBool(), "valid arithmetic " + source + " " + call.error().message);
            }
        }
        // Recovery and unchanged persistent destinations after a failed call.
        {
            auto call = runtime->Call(instance, "Verify");
            Check(call.invoke() == RunStatus::Completed && call.retType() == ValueType::Bool &&
                call.retBool(), "recovery/state " + source + " " + call.error().message);
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}
int main() {
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true}) {
        for (const std::string op : {"/", "%"}) {
            for (bool overflow : {false, true}) {
                const std::string a = overflow ? "-9223372036854775808" : "7";
                const std::string b = overflow ? "-1" : "0";
                const char* reason = overflow ? "integer division overflow" : "integer division by zero";
                for (const std::string target : {"x", "list[0]", "map.k", "array[0]", "nested[0][0]", "mapped.k[0]"}) {
                    const std::string prefix = "import system;var x=" + a + ";var b=" + b +
                        ";var list=[x];var map={\"k\":x};var array=system.array(x,1);"
                        "var nested=[[x]];var mapped={\"k\":[x]};var sentinel=19;\n";
                    const std::string verify = "\nexport fun Verify(){return x==" + a +
                        " && list[0]==x && map.k==x && array[0]==x && nested[0][0]==x && mapped.k[0]==x && sentinel==19;}";
                    Run(runtime, prefix + "export fun Probe(){" + target + " " + op + "= b;sentinel=0;}" +
                        verify, reason, debug, image);
                    Run(runtime, prefix + "export fun Probe(){sentinel=" + target + " " + op + " b;}" +
                        verify, reason, debug, image);
                }
                // Invalid literals must not be folded using undefined C++ arithmetic.
                const std::string expression = a + " " + op + " " + b;
                Run(runtime, "var sentinel=19;export fun Probe(){sentinel=" + expression +
                    ";}export fun Verify(){return sentinel==19;}", reason, debug, image);
                Run(runtime, "export fun Probe(){return false && (" + expression +
                    ")==0 || true;}export fun Verify(){return true;}", nullptr, debug, image);
                for (const std::string source : {
                    "const X = " + expression + ";",
                    "export fun Probe(){switch(1){case (" + expression + "):return 1;default:return 0;}}"}) {
                    Error error;
                    auto program = Compile(runtime, source, debug, image, error);
                    Check(!program && !error.ok() && error.message.find(overflow ?
                        "integer division overflow in const expression" :
                        "division by zero in const expression") != std::string::npos,
                        "compile rejection " + source + " " + error.message);
                    if (program) runtime->DestroyProgram(program);
                }
            }
            // Boundary-adjacent operands, signs, and both literal and runtime evaluation.
            struct Pair { const char* a; const char* b; const char* div; const char* mod; };
            for (const Pair& p : {Pair{"-9223372036854775808","1","-9223372036854775808","0"},
                Pair{"-9223372036854775808","2","-4611686018427387904","0"},
                Pair{"-9223372036854775807","-1","9223372036854775807","0"},
                Pair{"9223372036854775807","-1","-9223372036854775807","0"},
                Pair{"-7","3","-2","-1"}, Pair{"7","-3","-2","1"}, Pair{"0","-1","0","0"}}) {
                const std::string expected = op == "/" ? p.div : p.mod;
                const std::string expression = std::string(p.a) + " " + op + " " + p.b;
                Run(runtime, "const C=" + expression + ";export fun Probe(){var a=" + p.a +
                    ";var b=" + p.b + ";var r=a " + op + " b;a " + op + "= b;return r==" +
                    expected + " && a==r && C==r && (" + expression +
                    ")==r;}export fun Verify(){return true;}", nullptr, debug, image);
            }
        }
        // Floating point still has infinity/NaN; vector division uses the same policy.
        Run(runtime, "import math;export fun Probe(){var zero=0.0;var inf=1.0 / zero;"
            "var nan=zero / zero;var mixed=1 / zero;var f=1.0;f /= 0;"
            "var v=math.Vector2(1,0);var w=v / zero;v[0] /= zero;"
            "return inf>1.0 && mixed==inf && f==inf && nan!=nan && w[0]==inf && w[1]!=w[1] && v[0]==inf;}"
            "export fun Verify(){return true;}", nullptr, debug, image);
    }
    DestroyRuntime(runtime);
    std::printf("integer division: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
