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
    desc.sourceName = "integer-shift.ns";
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
                    call.retBool(), "valid shift " + source + " " + call.error().message);
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
    const char* reason = "shift count must be between 0 and 63";
    for (bool debug : {false, true}) for (bool image : {false, true}) {
        for (const std::string op : {"<<", ">>"}) {
            // An independent int64 arithmetic oracle covers every legal count,
            // sign extension, discarded high bits, and both integer boundaries.
            for (int64_t a : {INT64_MIN, INT64_MIN+1, int64_t(-3), int64_t(-1), int64_t(0), int64_t(1), int64_t(3), INT64_MAX}) {
                for (int count = 0; count < 64; ++count) {
                    uint64_t bits = static_cast<uint64_t>(a);
                    if (op == "<<") bits <<= count;
                    else { bits >>= count; if (a < 0 && count != 0) bits |= (~uint64_t(0)) << (64-count); }
                    const int64_t expected = bits <= uint64_t(INT64_MAX) ? int64_t(bits) : -1 - int64_t(~bits);
                    const std::string lhs = std::to_string(a), rhs = std::to_string(count);
                    const std::string expression = "(" + lhs + ") " + op + " " + rhs;
                    Run(runtime, "const C=" + expression + ";export fun Probe(){var a=" + lhs + ";var b=" + rhs +
                        ";var r=a " + op + " b;a " + op + "= b;return r==" + std::to_string(expected) +
                        " && a==r && C==r && (" + expression + ")==r;}export fun Verify(){return true;}",
                        nullptr, debug, image);
                }
            }
            for (const std::string count : {"-2147483648", "-33", "-1", "64", "65", "2147483647"}) {
                for (const std::string target : {"x", "list[0]", "map.k", "array[0]", "nested[0][0]", "mapped.k[0]"}) {
                    const std::string prefix = "import system;var x=7;var b=" + count +
                        ";var list=[x];var map={\"k\":x};var array=system.array(x,1);"
                        "var nested=[[x]];var mapped={\"k\":[x]};var sentinel=19;";
                    const std::string verify = "export fun Verify(){return x==7 && list[0]==7 && map.k==7 && array[0]==7"
                        " && nested[0][0]==7 && mapped.k[0]==7 && sentinel==19;}";
                    // Collection elements support assignment of a shifted value;
                    // direct <<= / >>= on a collection selector is unsupported.
                    const std::string write = target == "x" ? target + " " + op + "= b" :
                        target + "=" + target + " " + op + " b";
                    Run(runtime, prefix + "export fun Probe(){" + write + ";sentinel=0;}" +
                        verify, reason, debug, image);
                    Run(runtime, prefix + "export fun Probe(){sentinel=" + target + " " + op + " b;}" +
                        verify, reason, debug, image);
                }
                const std::string expression = "7 " + op + " (" + count + ")";
                Run(runtime, "export fun Probe(){return " + expression + ";}export fun Verify(){return true;}",
                    reason, debug, image);
                Run(runtime, "export fun Probe(){return true || (" + expression +
                    ")==0;}export fun Verify(){return true;}", nullptr, debug, image);
                for (const std::string source : {"const C=" + expression + ";",
                    "export fun Probe(){switch(1){case (" + expression + "):return 1;default:return 0;}}"}) {
                    Error error;
                    auto program = Compile(runtime, source, debug, image, error);
                    Check(!program && !error.ok() && error.message.find(reason) != std::string::npos,
                        "compile rejection " + source + " " + error.message);
                    if (program) runtime->DestroyProgram(program);
                }
            }
        }
    }
    DestroyRuntime(runtime);
    std::printf("integer shifts: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
