#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace NeoScript;

static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& name)
{
    ++checks;
    if (!ok && ++failures <= 30) std::fprintf(stderr, "FAIL %s\n", name.c_str());
}
static bool Property(CallContext& context, StringView name, bool get)
{
    if (!get || name.str() != "Value") return false;
    auto callback = context.runtime()->Call(context.instance(), "Touch");
    if (callback.invoke() != RunStatus::Completed)
    {
        context.fail(91, callback.error().message);
        return false;
    }
    context.retInt(callback.retInt());
    return true;
}
struct Case { std::string name, globals, body; };
static std::vector<Case> Cases()
{
    std::vector<Case> cases = {
        {"global left value", "var hp=100;fun Hit(){hp-=30;return 30;}",
            "var r=hp-Hit();return r==70 && hp==70;"},
        {"argument value", "var x=1;fun G(){x=100;return 0;}fun First(var a,var b){return a;}",
            "return First(x,G())==1 && x==100;"},
        {"all arguments", "var x=1;fun Next(){x++;return x;}fun Pack(var a,var b,var c){return a*100+b*10+c;}",
            "return Pack(x,Next(),Next())==123;"},
        {"function-variable callee resolved before arguments", "fun One(var unused){return 1;}fun Two(var unused){return 2;}var f=One;fun Change(){f=Two;return 0;}",
            "return f(Change())==1;"},
        {"string left value", "var s=\"a\";fun Change(){s=\"b\";return \"c\";}",
            "return (s..Change())==\"ac\";"},
        {"intrinsic argument", "var s=\"a\";fun Change(){s=\"b\";return \"c\";}",
            "return format(\"%s%s\",s,Change())==\"ac\";"},
        {"indexed value", "var a=[3];fun Change(){a[0]=10;return 2;}",
            "return a[0]+Change()==5;"},
        {"map member value", "var a={\"x\":3};fun Change(){a.x=10;return 2;}",
            "return a.x+Change()==5;"},
        {"host getter mutates scalar", "var x=1;export fun Touch(){x=10;return 2;}",
            "return x+Host.Value==3 && x==10;"},
        {"host getter in later argument", "var x=1;export fun Touch(){x=10;return 2;}fun First(var a,var b){return a;}",
            "return First(x,Host.Value)==1 && x==10;"},
        {"local list alias element", "var shared=[1];fun Change(){shared[0]=2;return 0;}",
            "var a=shared;return a[0]+Change()==1 && shared[0]==2;"},
        {"local map alias member", "var shared={\"n\":1};fun Change(){shared.n=2;return 0;}",
            "var a=shared;return a.n+Change()==1 && shared.n==2;"},
        {"local array alias element", "var shared=system.array(1,1);fun Change(){shared[0]=2;return 0;}",
            "var a=shared;return a[0]+Change()==1 && shared[0]==2;"},
        {"global nested element", "var shared=[[1]];fun Change(){shared[0][0]=2;return 0;}",
            "return shared[0][0]+Change()==1 && shared[0][0]==2;"},
        {"hex literal before call", "var g=1;fun Change(){g=2;return 1;}fun First(var a,var b){return a;}",
            "return First(g,0x1e+Change())==1 && g==2;"},
        {"hex literal in comparison", "var g=30;fun Change(){g=31;return 0;}",
            "return (g==0x1e+Change()) && g==31;"},
        {"host getter through alias", "var x=1;export fun Touch(){x=10;return 2;}",
            "var h=Host;return x+h.Value==3 && x==10;"},
        {"host getter through parentheses", "var x=1;export fun Touch(){x=10;return 2;}",
            "return x+(Host).Value==3 && x==10;"},
        {"host getter alias in later argument", "var x=1;export fun Touch(){x=10;return 2;}fun First(var a,var b){return a;}",
            "var h=Host;return First(x,h.Value)==1 && x==10;"},
        {"host getter parentheses in later argument", "var x=1;export fun Touch(){x=10;return 2;}fun First(var a,var b){return a;}",
            "return First(x,(Host).Value)==1 && x==10;"},
        {"vector argument copy", "var v=math.Vector2(3,4);fun Change(){v[0]=10;return 0;}fun First(var a,var b){return a[0];}",
            "return First(v,Change())==3 && v[0]==10;"},
        {"container argument retains identity", "var a=[3];fun Change(){a=[10];return 0;}fun First(var x,var y){return x[0];}",
            "return First(a,Change())==3;"},
        {"container argument shares mutation", "var a=[3];fun Change(){a[0]=10;return 0;}fun First(var x,var y){return x[0];}",
            "return First(a,Change())==10;"},
        {"short circuit", "var calls=0;fun Change(){calls++;return true;}",
            "var a=false && Change();var b=true || Change();return !a && b && calls==0;"},
        {"short circuit ordered values", "var x=1;fun Change(){x=2;return 1;}",
            "return x==Change() && x==2;"},
        {"assignment result snapshots", "var a=1;fun Change(){a=7;return 2;}",
            "var r=(a=3)+Change();return r==5 && a==7;"},
        {"local argument postfix", "fun First(var a,var b){return a;}",
            "var x=1;var r=First(x,x++);return r==1 && x==2;"},
        {"unparenthesized precedence", "var g=2;fun Change(){g=10;return 3;}",
            "var r=g*100+Change();return r==203 && g==10;"},
        {"addition before later call", "var g=2;fun Change(){g=10;return 3;}",
            "var r=g+4+Change();return r==9;"},
        {"higher precedence RHS", "var g=2;fun Change(){g=10;return 3;}",
            "var r=g+4*Change();return r==14;"},
        {"three level owner replacement", "var inner=[math.Vector2(0,0),math.Vector2(0,0)];var holder=[inner];fun Change(){holder[0]=[math.Vector2(9,9),math.Vector2(9,9)];return 5;}",
            "holder[0][1][0]+=Change();return holder[0][1][0]==14 && holder[0][1][1]==9 && inner[1][0]==0;"},
        {"root replacement", "var old=[[1]];var holder=old;fun Change(){holder=[[9]];return 5;}",
            "holder[0][0]+=Change();return holder[0][0]==14 && old[0][0]==1;"},
        {"mixed map list path", "var old={\"list\":[{\"n\":1}]};var holder=[old];fun Change(){holder[0]={\"list\":[{\"n\":9}]};return 5;}",
            "holder[0].list[0].n+=Change();return holder[0].list[0].n==14 && old.list[0].n==1;"},
        {"keys evaluated once", "var log=0;var holder=[[[1]]];fun Key(){log=log*10+1;return 0;}fun Change(){log=log*10+2;holder=[[[9]]];return 5;}",
            "holder[Key()][Key()][Key()]+=Change();return log==1112 && holder[0][0][0]==14;"},
        {"target index variable resolves after RHS", "var i=0;var holder=[[1],[2]];fun Change(){i=1;holder=[[9],[10]];return 5;}",
            "holder[i][0]+=Change();return holder[0][0]==9 && holder[1][0]==15;"},
        {"call receiver evaluated once", "var calls=0;var old=[[1]];var holder=old;fun Get(){calls++;return holder;}fun Change(){holder=[[9]];return 5;}",
            "Get()[0][0]+=Change();return calls==1 && old[0][0]==6 && holder[0][0]==9;"},
        {"vector array deferred parent", "var old=system.array(math.Vector2(1,2),1);var holder=[old];fun Change(){holder[0]=system.array(math.Vector2(9,8),1);return 5;}",
            "holder[0][0][0]+=Change();return holder[0][0][0]==14 && holder[0][0][1]==8 && old[0][0]==1;"},
        {"nested assignment target result", "var a=[[math.Vector2(1,2)]];var b=math.Vector2(3,4);",
            "(a[0][0]=b)[0]=7;return a[0][0][0]==7 && a[0][0][1]==4 && b[0]==3;"},
        {"two variable path replacements", "var old=[[1]];var holder=old;fun Change(){holder=[[9]];return 5;}",
            "var r=(holder[0][0]=Change());return r==5 && holder[0][0]==5 && old[0][0]==1;"}
    };
    // Parentheses must not change when the owner is resolved or when keys run.
    for (int depth = 0; depth < 5; ++depth)
    {
        std::string lhs = std::string(depth, '(') + "holder[0][0]" + std::string(depth, ')');
        cases.push_back({"parenthesized target " + std::to_string(depth),
            "var old=[[1]];var holder=old;fun Change(){holder=[[9]];return 5;}",
            "var r=(" + lhs + "+=Change());return r==14 && holder[0][0]==14 && old[0][0]==1;"});
    }
    return cases;
}

static void Run(IRuntime* runtime, const Case& c, bool debug, bool image)
{
    CompileDesc desc;
    desc.sourceName = "evaluation-order.ns";
    desc.includeDebugInfo = debug;
    const std::string source = "import math;import system;" + c.globals + "export fun Probe(){" + c.body + "}";
    desc.source = source;
    Error error;
    ProgramHandle program;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) program = runtime->LoadProgram(bytes, &error);
    }
    else { auto result = runtime->Compile(desc); program = result.program; error = result.error; }
    const auto label = c.name + " debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(bool(program), label + " compile: " + error.message);
    if (!program) return;
    for (int instanceNo = 0; instanceNo < 2; ++instanceNo)
    {
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), label + " instance");
        if (!instance) continue;
        auto call = runtime->Call(instance, "Probe");
        auto status = call.invoke();
        Check(status == RunStatus::Completed, label + " invoke: " + call.error().message);
        if (status == RunStatus::Completed) Check(call.retType() == ValueType::Bool && call.retBool(), label + " result");
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

int main()
{
    auto runtime = CreateRuntime();
    NativeObjectDesc host;
    host.name = "Host";
    host.property = Property;
    runtime->RegisterObject(host);
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true})
        for (const auto& c : Cases()) Run(runtime, c, debug, image);
    DestroyRuntime(runtime);
    std::printf("hunt_evaluation_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
