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
static ProgramHandle Compile(IRuntime* runtime, const std::string& source,
    bool debug, bool image, Error& error) {
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "runtime-edge.ns";
    desc.includeDebugInfo = debug;
    if (image) {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) return runtime->LoadProgram(bytes, &error);
        Check(bytes.empty(), "rejected source produces no image");
        return {};
    }
    auto result = runtime->Compile(desc);
    error = result.error;
    return result.program;
}
static void Run(IRuntime* runtime, const std::string& source, bool debug, bool image,
    const char* failure = nullptr) {
    Error error;
    auto program = Compile(runtime, source + "\nexport fun Recovery(){return 23;}", debug, image, error);
    Check(bool(program), "compile: " + source + " " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "instance: " + source);
    if (instance) {
        {
            auto call = runtime->Call(instance, "Probe");
            const auto status = call.invoke();
            Check(failure ? status == RunStatus::Failed && call.error().message.find(failure) != std::string::npos :
                status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                "execute: " + source + " " + call.error().message);
        }
        {
            auto call = runtime->Call(instance, "Recovery");
            Check(call.invoke() == RunStatus::Completed && call.retInt() == 23, "recovery: " + source);
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}
static void Reject(IRuntime* runtime, const std::string& source, bool debug, bool image) {
    Error error;
    auto program = Compile(runtime, source, debug, image, error);
    Check(!program && !error.ok() && !error.message.empty(), "diagnostic: " + source + " " + error.message);
    if (program) runtime->DestroyProgram(program);
}
int main() {
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true}) {
        Run(runtime, "export fun Probe(){var a=8;var b=2;var c=a/b;a/=b;"
            "var d=a/* separate */ /b;return c==4 && a==4 && d==2 && !false && !!true && !!!false;}", debug, image);
        Run(runtime, "const N=8;export fun Probe(){var n=N/2;var/*name*/a=4;"
            "return n==a && !true==false;}", debug, image);
        for (const char* source : {"var a=8;var b=2;var ab=77;var x=a/*comment*/b;",
            "/*", "var x=1;/*unfinished", "fun F(){/*unfinished", "var x=(;",
            "var x=[0,1][1];", "var x={\"a\":7}.a;", "var x=[1].len();",
            "var x=[;", "var x={;", "var x=[1]x;", "var x=1 2;", "fun F(){return (;}"})
            Reject(runtime, source, debug, image);
        for (const char* value : {"true", "1.0", "null", "\"x\"", "[]", "{}"})
            Run(runtime, std::string("export fun Probe(){var a=3;var b=") + value + ";return a<<b;}", debug, image, "<<");
        Run(runtime, "export fun Probe(){var n=0;var sum=0;for(var i in 2147483646,2147483647,2){"
            "sum=i;n+=1;if(n>2)return false;}if(n!=1 || sum!=2147483646)return false;n=0;"
            "for(var j in -2147483647,-2147483648,-2){sum=j;n+=1;if(n>2)return false;}"
            "return n==1 && sum==-2147483647;}", debug, image);
        Run(runtime, "export fun Probe(){var n=0;for(var i in -2147483648,2147483647,2147483647){"
            "n+=1;if(n>4)return false;}var m=0;for(var j in 2147483647,-2147483648,-2147483648){"
            "m+=1;if(m>3)return false;}return n==3 && m==2;}", debug, image);
        Run(runtime, "export fun Probe(){var n=0;var last=0;for(var i in 2147483646,2147483647,1){"
            "n+=1;last=i;if(n>1)return false;}for(var j in 2147483647,2147483647,1)return false;"
            "for(var k in -2147483648,-2147483647,1){n+=1;if(k!=-2147483648)return false;}"
            "return n==2 && last==2147483646;}", debug, image);
        for (bool reverse : {false, true}) {
            Run(runtime, std::string("import system;export fun Probe(){var p=0.0;var n=-p;var a=") +
                (reverse ? "n;var b=p;" : "p;var b=n;") +
                "var m={};m[a]=1;if(m[b]!=1)return false;m[b]=2;if(m.len()!=1 || m[a]!=2)return false;"
                "var s=system.set([a,b]);if(s.len()!=1)return false;m[b]=null;return m.len()==0;}", debug, image);
        }
        Run(runtime, "import coroutine;import system;export fun Probe(){var c=coroutine.create(fun(){});"
            "coroutine.resume(c);var previous=\"hello\".len();var a=coroutine.close(c);"
            "var b=coroutine.close(c);var request=system.aysnc_create();previous=\"HELLO\".lower();"
            "var w=request.wait();return a==c && b==c && w==false && request.wait()==false;}", debug, image);
        Run(runtime, "import math;export fun Probe(){var p=math.Repeat(100000000.0,3.0);"
            "var q=math.Repeat(1000000000000000.0,3.0);var r=math.Repeat(-1000000000000000.0,3.0);"
            "var a=math.WrapAngle(1000000000000000.0);var b=math.WrapAngle(-1000000000000000.0);"
            "return p==1.0 && q==2.0 && r==1.0 && a>-3.141593 && a<=3.141593 && b>-3.141593 && b<=3.141593;}", debug, image);
        for (const char* value : {"-100.0", "100.0", "-0.0", "0.0"})
            Run(runtime, std::string("import math;export fun Probe(){var r=math.Repeat(") + value +
                ",2.5);return r==0.0 && (1.0 / r)>0.0 && tostring(r)==\"0\";}", debug, image);

        // Reversed bound order must preserve NaN results and short-circuiting.
        for (const char* value : {"n", "-2.0", "0.0", "2.0"}) {
            Run(runtime, std::string("export fun Probe(){var z=0.0;var n=z / z;var x=") + value +
                ";var lo=-1.0;var hi=1.0;var a=x<=hi;var b=x>=lo;var expected=a && b;"
                "var actual=false;if(x<=hi && x>=lo)actual=true;if(actual!=expected)return false;"
                "a=x>hi;b=x<lo;expected=a || b;actual=false;if(x>hi || x<lo)actual=true;"
                "return actual==expected;}", debug, image);
        }
        Run(runtime, "export fun Probe(){var z=0.0;var n=z / z;var bad=\"not numeric\";"
            "if(n<1.0 && n>bad)return false;if(n>0.0 && n<bad)return false;return true;}", debug, image);

        // Literal/local and global operands exercise both fused dispatch variants.
        // Boolean values are the oracle for branches, short-circuiting, and ranges.
        for (bool globals : {false, true}) for (const char* av : {"n", "-1.0", "0.0", "2.0"})
            for (const char* bv : {"n", "-1.0", "0.0", "2.0"}) for (const char* op : {"<", "<=", ">", ">=", "==", "!="}) {
                const std::string init = std::string("var z=0.0;var n=z / z;var a=") + av + ";var b=" + bv + ";";
                const std::string cmp = std::string("a") + op + "b";
                Run(runtime, (globals ? init : "") + "export fun Probe(){" + (globals ? "" : init) +
                    "var expected=(" + cmp + ");var branch=false;if(" + cmp + ")branch=true;"
                    "if(branch!=expected)return false;var count=0;while(" + cmp + "){count+=1;break;}"
                    "return (count==1)==expected && ((" + cmp + ") && true)==expected && ((" + cmp +
                    ") || false)==expected;}", debug, image);
            }
        // Check fused inside/outside jumps against separately evaluated booleans.
        // Both source orders and global/local encodings must retain lazy evaluation.
        for (bool globals : {false, true}) for (bool upperFirst : {false, true})
            for (const char* x : {"n", "-2.0", "-1.0", "0.0", "1.0", "2.0"})
                for (const char* lo : {"n", "-1.0"}) for (const char* hi : {"n", "1.0"})
                    for (bool lowerInclusive : {false, true}) for (bool upperInclusive : {false, true})
                        for (bool outside : {false, true}) {
                            const std::string lowerOp = outside ? (lowerInclusive ? "<=" : "<") : (lowerInclusive ? ">=" : ">");
                            const std::string upperOp = outside ? (upperInclusive ? ">=" : ">") : (upperInclusive ? "<=" : "<");
                            const std::string lower = "x" + lowerOp + "lo", upper = "x" + upperOp + "hi";
                            const std::string first = upperFirst ? upper : lower, second = upperFirst ? lower : upper;
                            const std::string join = outside ? "||" : "&&", expr = first + join + second;
                            const std::string init = std::string("var z=0.0;var n=z / z;var x=") + x +
                                ";var lo=" + lo + ";var hi=" + hi + ";";
                            Run(runtime, (globals ? init : "") + "export fun Probe(){" + (globals ? "" : init) +
                                "var a=(" + first + ");var b=(" + second + ");var expected=a" + join +
                                "b;var actual=false;if(" + expr + ")actual=true;var value=(" + expr +
                                ");return actual==expected && value==expected;}", debug, image);
                        }
        for (const char* init : {"var x=2147483647;var lo=2147483646;var hi=2147483647;",
            "var x=-2147483648;var lo=-2147483648;var hi=-2147483647;",
            "var x=16777217;var lo=16777216.0;var hi=16777218;",
            "var x=16777216.0;var lo=16777217;var hi=16777218.0;",
            "var x=\"b\";var lo=\"a\";var hi=\"c\";", "var x=\"b\";var lo=\"b\";var hi=\"b\";"})
            for (bool upperFirst : {false, true}) for (const char* lowerOp : {"<", "<=", ">", ">="})
                for (const char* upperOp : {"<", "<=", ">", ">="}) for (const char* join : {"&&", "||"}) {
                    const std::string lower = std::string("x") + lowerOp + "lo", upper = std::string("x") + upperOp + "hi";
                    const std::string first = upperFirst ? upper : lower, second = upperFirst ? lower : upper;
                    const std::string expr = first + join + second;
                    Run(runtime, std::string("export fun Probe(){") + init + "var a=(" + first + ");var b=(" + second +
                        ");var expected=a" + join + "b;var actual=false;if(" + expr + ")actual=true;"
                        "var value=(" + expr + ");return actual==expected && value==expected;}", debug, image);
                }
        Run(runtime, "export fun Probe(){var x=2;var bad=\"not numeric\";"
            "if(x>1 || x<bad){}else return false;if(x<3 || x>bad){}else return false;"
            "if(x<1 && x>bad)return false;if(x>3 && x<bad)return false;return true;}", debug, image);
        for (bool list : {false, true}) for (bool closure : {false, true}) {
            const std::string container = list ? "[3,1,2]" : "{\"a\":3,\"b\":1,\"c\":2}";
            const std::string compare = "state.calls+=1;if(state.calls==1){var z=0;return a / z < b;}return a<b;";
            const std::string source = "var state={\"calls\":0,\"after\":0};var m=" + container +
                ";fun Compare(var a,var b){" + compare + "}export fun Probe(){m.sort(" +
                (closure ? "fun(var a,var b){" + compare + "}" : "Compare") +
                ");state.after=1;}export fun Verify(){return state.calls==1 && state.after==0 && m.len()==3;}";
            Error error;
            auto program = Compile(runtime, source, debug, image, error);
            Check(bool(program), "sort compile " + error.message);
            if (!program) continue;
            auto instance = runtime->CreateInstance(program);
            Check(bool(instance), "sort instance");
            if (instance) {
                { auto call = runtime->Call(instance, "Probe");
                  Check(call.invoke() == RunStatus::Failed && call.error().message.find("integer division by zero") != std::string::npos,
                      "sort propagates first error"); }
                { auto call = runtime->Call(instance, "Verify");
                  Check(call.invoke() == RunStatus::Completed && call.retBool(), "sort stops and preserves state"); }
                runtime->DestroyInstance(instance);
            }
            runtime->DestroyProgram(program);
        }
        Run(runtime, "export fun Probe(){var m={\"a\":3,\"b\":1,\"c\":2};m.sort(fun(var a,var b){return 7;});"
            "return m.len()==3;}", debug, image);
        Run(runtime, "export fun Probe(){var m={\"a\":3,\"b\":1,\"c\":2};m.sort(fun(var a,var b){return a<b;});"
            "var v=m.values();if(v[0]!=1 || v[1]!=2 || v[2]!=3)return false;"
            "m.sort(fun(var a,var b){return true;});return m.len()==3;}", debug, image);
        Run(runtime, "export fun Probe(){var m={\"a\":3,\"b\":1};m.sort(fun(var a,var b){m.c=2;return a<b;});}",
            debug, image, "map was modified during sort");
        Run(runtime, "export fun Probe(){var m={\"a\":3,\"b\":1};m.sort(fun(var a,var b){yield;return a<b;});}",
            debug, image, "synchronous native-to-script");
    }
    DestroyRuntime(runtime);
    std::printf("runtime edges: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
