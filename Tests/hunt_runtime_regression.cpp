#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 40) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}

static bool Host(CallContext& context, StringView method)
{
    if (method.str() != "Reenter" || context.argCount() != 1) return false;
    auto callback = context.argFunction(0);
    auto call = context.runtime()->Call(context.instance(), callback);
    if (call.invoke() != RunStatus::Completed)
    {
        context.fail(91, call.error().message);
        return false;
    }
    context.retInt(call.retInt());
    return true;
}

static void Run(IRuntime* runtime, const std::string& label, const std::string& body,
    bool debug, bool image, const char* expectedError = nullptr, bool unlimited = false,
    const std::string& recovery = "return true;")
{
    AllocStats before;
    GetAllocStats(before);
    const std::string source = "import math;import system;import coroutine;" + body +
        "\nexport fun Recovery(){" + recovery + "}";
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "hunt-runtime.ns";
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
        const auto compiled = runtime->Compile(desc);
        program = compiled.program;
        error = compiled.error;
    }
    const std::string where = label + " debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(bool(program), where + " compile: " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), where + " instance");
    if (instance)
    {
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            {
                auto call = runtime->Call(instance, "Probe");
                if (!unlimited) call.timeout(250);
                const auto status = call.invoke();
                Check(expectedError
                    ? status == RunStatus::Failed && call.error().message.find(expectedError) != std::string::npos
                    : status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                    where + " execute: " + call.error().message);
                if (status == RunStatus::Suspended) runtime->Cancel(instance);
            }
            {
                auto call = runtime->Call(instance, "Recovery");
                const auto status = call.invoke();
                Check(status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                    where + " same-instance recovery: " + call.error().message);
            }
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    runtime->CollectCycles(true);
    AllocStats after;
    GetAllocStats(after);
    Check(after.maps == before.maps && after.lists == before.lists && after.arrays == before.arrays &&
        after.sets == before.sets && after.vectors == before.vectors && after.coroutines == before.coroutines &&
        after.closures == before.closures && after.modules == before.modules && after.asyncs == before.asyncs,
        where + " releases tracked objects");
}

static void ComparisonCases(IRuntime* runtime, bool debug, bool image)
{
    const char* invalid[] = {"null", "true", "[1]", "{}", "math.Vector2(1,2)", "system.array(1,1)"};
    for (const char* value : invalid)
        for (const char* op : {"<", "<=", ">", ">="})
            for (bool global : {false, true})
            {
                const std::string declaration = std::string("var value=") + value + ";";
                const std::string prefix = global ? declaration + "export fun Probe(){" : "export fun Probe(){" + declaration;
                const std::string comparison = std::string("value") + op + "1";
                Run(runtime, std::string("if ") + value + op + (global ? " global" : " local"),
                    prefix + "if(" + comparison + ")return false;return false;}", debug, image, "invalid operator");
                Run(runtime, std::string("while ") + value + op,
                    prefix + "while(" + comparison + "){return false;}return false;}", debug, image, "invalid operator");
            }
    for (const char* condition : {"value<1 && true", "value<1 || false", "value>=0 && value<=10",
        "value<0 || value>10", "value<=10 && value>=0", "value>10 || value<0"})
        Run(runtime, std::string("fused condition ") + condition,
            std::string("export fun Probe(){var value=null;if(") + condition + ")return false;return false;}",
            debug, image, "invalid operator");
    Run(runtime, "less diagnostic retains source order",
        "export fun Probe(){var value=\"text\";return value<1;}", debug, image,
        "invalid operator '<' for 'string' and 'int'");
    Run(runtime, "less-equal diagnostic retains source order",
        "export fun Probe(){var value=\"text\";return value<=1;}", debug, image,
        "invalid operator '<=' for 'string' and 'int'");
    Run(runtime, "NaN branch semantics", R"NS(
export fun Probe(){var zero=0.0;var value=zero/zero;
 if(value<0.0)return false;if(value<=0.0)return false;
 if(value>0.0)return false;if(value>=0.0)return false;
 if(value>=0.0 && value<=10.0)return false;
 if(value<0.0 || value>10.0)return false;
 return !(value>=0.0 && value<=10.0) && !(value<0.0 || value>10.0);}
)NS", debug, image);
}

static void CallCases(IRuntime* runtime, bool debug, bool image)
{
    for (const char* value : {"[1]", "{}", "\"text\"", "system.set([1])", "system.array(1,1)",
        "system.aysnc_create()", "null", "1", "math.Vector2(1,2)"})
        Run(runtime, std::string("noncallable ") + value,
            std::string("export fun Probe(){var value=") + value + ";value();return false;}",
            debug, image, "cannot call a value of type");
    for (const char* arguments : {"1", "1,2,3"})
    {
        Run(runtime, std::string("named function value arity ") + arguments,
            std::string("fun Target(var a,var b){return b;}export fun Probe(){var value=Target;value(") +
                arguments + ");return false;}", debug, image, "argument count mismatch");
        Run(runtime, std::string("captured function value arity ") + arguments,
            std::string("export fun Probe(){var n=4;var value=fun(var a,var b){return b+n;};value(") +
                arguments + ");return false;}", debug, image, "argument count mismatch");
        Run(runtime, std::string("map function value arity ") + arguments,
            std::string("fun Target(var a,var b){return b;}export fun Probe(){var value={\"f\":Target};value.f(") +
                arguments + ");return false;}", debug, image, "argument count mismatch");
    }
    Run(runtime, "valid dynamic functions", R"NS(
fun Target(var a,var b){return a+b;}
export fun Probe(){var n=3;var f=Target;var g=fun(var a,var b){return a+b+n;};
 var map={"f":f,"g":g};return f(1,2)==3 && g(1,2)==6 && map.f(2,3)==5 && map.g(2,3)==8;}
)NS", debug, image);
    Run(runtime, "native reentry depth", R"NS(
fun Recur(var depth){if(depth==0)return 7;return Host.Reenter(fun(){return Recur(depth-1);});}
export fun Probe(){return Recur(1000)==7;}
)NS", debug, image, "call stack overflow", true);
    Run(runtime, "bounded native reentry", R"NS(
fun Recur(var depth){if(depth==0)return 7;return Host.Reenter(fun(){return Recur(depth-1);});}
export fun Probe(){return Recur(8)==7;}
)NS", debug, image, nullptr, true);
}

static void CoroutineCases(IRuntime* runtime, bool debug, bool image)
{
    for (const char* source : {"[3,2,1]", "{\"a\":3,\"b\":2,\"c\":1}"})
        Run(runtime, std::string("coroutine top-level sort ") + source,
            std::string("export fun Probe(){var values=") + source + ";var state=[false];"
            "var co=coroutine.create(fun(){values.sort(fun(var a,var b){return a<b;});state[0]=true;});"
            "coroutine.resume(co);var other=coroutine.create(fun(){state[0]=state[0] && true;});"
            "coroutine.resume(other);return state[0] && coroutine.status(co)==\"dead\" && coroutine.status(other)==\"dead\";}",
            debug, image, nullptr, true);
    for (bool map : {false, true})
        Run(runtime, map ? "recursive map sorting" : "recursive list sorting",
            "fun Recur(var depth){if(depth==0)return 0;var values=" +
            std::string(map ? "{\"a\":2,\"b\":1}" : "[2,1]") +
            ";values.sort(fun(var a,var b){Recur(depth-1);return a<b;});return depth;}"
            "export fun Probe(){return Recur(1000)==1000;}", debug, image, "call stack overflow", true);
    Run(runtime, "reject closing main", "export fun Probe(){var value=[1];coroutine.close();return value[0]==1;}",
        debug, image, "main");
    Run(runtime, "first resume error releases stack", R"NS(
var saved=null;
fun Body(){var values={"key":[1,2,3]};var bad=null+1;}
export fun Probe(){saved=coroutine.create(Body);coroutine.resume(saved);return false;}
)NS", debug, image, "invalid operator", false, "return coroutine.status(saved)==\"dead\";");
    Run(runtime, "nested coroutine error releases both stacks", R"NS(
var parent=null;var child=null;
fun Child(){var values={"key":[4,5,6]};var bad=null+1;}
fun Parent(){var values={"key":[1,2,3]};child=coroutine.create(Child);coroutine.resume(child);}
export fun Probe(){parent=coroutine.create(Parent);coroutine.resume(parent);return false;}
)NS", debug, image, "invalid operator", false,
        "return coroutine.status(parent)==\"dead\" && coroutine.status(child)==\"dead\";");
    Run(runtime, "self resume failure cleanup", R"NS(
var saved=null;
fun Body(){var values={"key":[1]};coroutine.resume(saved);}
export fun Probe(){saved=coroutine.create(Body);coroutine.resume(saved);return false;}
)NS", debug, image, "invalid function call", false, "return coroutine.status(saved)==\"dead\";");
    Run(runtime, "retained capturing coroutine survives named-call failure", R"NS(
var saved=null;var kept=null;var fail=true;
fun Fail(){if(fail){var bad=null+1;}}
export fun Probe(){fail=true;var cap={"n":1};
 kept=fun(){cap.n+=1;Fail();return cap.n;};
 saved=coroutine.create(kept);coroutine.resume(saved);return false;}
)NS", debug, image, "invalid operator", false,
        "var dead=coroutine.status(saved)==\"dead\";saved=null;fail=false;return dead && kept()==3;");
    for (bool namedHelper : {false, true})
        Run(runtime, namedHelper ? "global-init closure yield through named helper" : "global-init closure direct yield",
            std::string("fun PauseOnce(){yield;}fun DirectYield(){var values=[];var callback=fun(){values.append(1);") +
            (namedHelper ? "PauseOnce();" : "yield;") +
            "};callback();}DirectYield();export fun Probe(){return true;}", debug, image);
    Run(runtime, "ordinary coroutine yield and close", R"NS(
export fun Probe(){var state=[0];var co=coroutine.create(fun(){state[0]=1;yield;state[0]=2;});
 coroutine.resume(co);var first=state[0]==1 && coroutine.status(co)=="suspended";
 coroutine.resume(co);var second=state[0]==2 && coroutine.status(co)=="dead";
 coroutine.close(co);return first && second;}
)NS", debug, image);
    for (const char* statement : {"yield;", "sleep(1);"})
        Run(runtime, std::string("pcall suspension ") + statement,
            std::string("export fun Probe(){var module=system.load(\"") + statement +
            "\",\"chunk\");system.pcall(module);return false;}", debug, image,
            "synchronous native-to-script");
}

static void CollectionErrorCases(IRuntime* runtime, bool debug, bool image)
{
    for (const char* value : {"\"\"", "\"ab\""})
        Run(runtime, std::string("string two-variable foreach ") + value,
            std::string("export fun Probe(){foreach(var key,value in ") + value +
            "){return false;}return false;}", debug, image, "foreach on string does not support two variables");
    for (const char* op : {"+=", "-=", "*=", "/=", "%="})
        Run(runtime, std::string("list compound bounds diagnostic ") + op,
            std::string("export fun Probe(){var values=[1];values[3]") + op +
            "1;return false;}", debug, image, "list index out of range on assign");
    Run(runtime, "vector array component write bounds diagnostic",
        "export fun Probe(){var values=system.array(math.Vector2(1,2),1);values[3][0]+=1;return false;}",
        debug, image, "array index out of range on assign");
}

static void NumericCases(IRuntime* runtime, bool debug, bool image)
{
    Run(runtime, "saturating integer conversion", R"NS(
export fun Probe(){var zero=0.0;var inf=1.0/zero;var nan=zero/zero;
 return toint(inf)==2147483647 && toint(-inf)==-2147483648 && toint(nan)==0
  && toint(3000000000.0)==2147483647 && toint(-3000000000.0)==-2147483648
  && toint(2147483520.0)==2147483520 && toint(-2147483648.0)==-2147483648
  && toint(1.9)==1 && toint(-1.9)==-1 && toint(-0.0)==0;}
)NS", debug, image);
    Run(runtime, "range numeric conversion", R"NS(
export fun Probe(){var sum=0;for(var i in 0.9,3.9,1.9)sum+=i;
 var negative=0;for(var i in -3.9,0.9,1.9)negative+=i;
 var high=0;for(var i in 2147483646,3000000000.0)high+=1;
 var low=0;for(var i in -3000000000.0,-2147483647)low+=1;
 var empty=0;for(var i in 3000000000.0,3000000000.0)empty+=1;
 var zero=0.0;var hugeStep=0;for(var i in 0,3,1.0/zero)hugeStep+=1;
 return sum==3 && negative==-6 && high==1 && low==1 && empty==0 && hugeStep==1;}
)NS", debug, image);
    Run(runtime, "NaN range step becomes zero",
        "export fun Probe(){var zero=0.0;var step=zero/zero;for(var i in 0,2,step){}return false;}",
        debug, image, "'for' step is zero");
    for (const char* value : {"\"2\"", "null", "true", "[2]"})
        for (bool noResult : {false, true})
            Run(runtime, std::string("vector numeric argument ") + value + (noResult ? " discarded" : " assigned"),
                "export fun Probe(){" + std::string(noResult ? "" : "var result=") + "math.Vector3(1," + value +
                ",3);return false;}", debug, image, "invalid function call");
}

int main()
{
    auto runtime = CreateRuntime();
    NativeObjectDesc host;
    host.name = "Host";
    host.method = Host;
    runtime->RegisterObject(host);
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true})
    {
        ComparisonCases(runtime, debug, image);
        CallCases(runtime, debug, image);
        CoroutineCases(runtime, debug, image);
        NumericCases(runtime, debug, image);
        CollectionErrorCases(runtime, debug, image);
    }
    DestroyRuntime(runtime);
    std::printf("hunt_runtime_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
