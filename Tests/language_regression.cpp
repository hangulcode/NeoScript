#include "NeoScript.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace NeoScript;

static int checks = 0;
static int failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label.c_str()); }
}

class ModuleLoader final : public ILoader
{
public:
    std::map<std::string, std::string> sources;
    bool Load(StringView path, std::vector<uint8_t>& out) override
    {
        const auto it = sources.find(path.str());
        if (it == sources.end()) return false;
        out.assign(it->second.begin(), it->second.end());
        return true;
    }
    StringView LibPath() const override { return "modules/"; }
};

static CompileDesc Describe(const std::string& source, bool debug)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "language.ns";
    desc.includeDebugInfo = debug;
    return desc;
}

static void Run(IRuntime* runtime, const std::string& source, bool debug)
{
    for (bool image : {false, true})
    {
        ProgramHandle program;
        Error error;
        if (image)
        {
            std::vector<uint8_t> bytes;
            error = runtime->CompileToBytecode(Describe(source, debug), bytes);
            if (error.ok()) program = runtime->LoadProgram(bytes, &error);
        }
        else
        {
            CompileResult result = runtime->Compile(Describe(source, debug));
            program = result.program;
            error = result.error;
        }
        bool ok = false;
        if (program)
        {
            InstanceHandle instance = runtime->CreateInstance(program);
            if (instance)
            {
                {
                    Invocation call = runtime->Call(instance, "Test");
                    ok = call.invoke() == RunStatus::Completed
                        && call.retType() == ValueType::Bool && call.retBool();
                }
                runtime->DestroyInstance(instance);
            }
            runtime->DestroyProgram(program);
        }
        Check(ok, "debug=" + std::to_string(debug) + " image=" + std::to_string(image)
            + " " + source + "\n" + error.message);
    }
}

static void Reject(IRuntime* runtime, const std::string& source, const char* reason,
    const char* file, uint32_t line, bool debug)
{
    for (bool image : {false, true})
    {
        Error error;
        bool rejected;
        if (image)
        {
            std::vector<uint8_t> bytes{1, 2, 3};
            error = runtime->CompileToBytecode(Describe(source, debug), bytes);
            rejected = !error.ok() && bytes.empty();
        }
        else
        {
            CompileResult result = runtime->Compile(Describe(source, debug));
            error = result.error;
            rejected = !result.program;
            if (result.program) runtime->DestroyProgram(result.program);
        }
        Check(rejected && error.message.find(reason) != std::string::npos
            && error.message.find(std::string(file) + ": ") == 0
            && error.sourceName == file && error.line == line && error.column > 0,
            "reject " + source + "\n" + error.message + " [" + error.sourceName
                + ":" + std::to_string(error.line) + "]");
    }
}

static void RuntimeReject(IRuntime* runtime, const std::string& source, const char* reason,
    bool debug, bool verify = false)
{
    for (bool image : {false, true})
    {
        ProgramHandle program;
        Error error;
        if (image)
        {
            std::vector<uint8_t> bytes;
            error = runtime->CompileToBytecode(Describe(source, debug), bytes);
            if (error.ok()) program = runtime->LoadProgram(bytes, &error);
        }
        else
        {
            CompileResult compiled = runtime->Compile(Describe(source, debug));
            program = compiled.program;
            error = compiled.error;
        }
        bool ok = false;
        std::string message = error.message;
        if (program)
        {
            InstanceHandle instance = runtime->CreateInstance(program);
            if (instance)
            {
                {
                    Invocation call = runtime->Call(instance, "Test");
                    const RunStatus status = call.invoke();
                    message = call.error().message;
                    ok = status == RunStatus::Failed && message.find(reason) != std::string::npos;
                }
                if (ok && verify)
                {
                    Invocation call = runtime->Call(instance, "Verify");
                    ok = call.invoke() == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool();
                }
                runtime->DestroyInstance(instance);
            }
            runtime->DestroyProgram(program);
        }
        Check(ok, "runtime reject debug=" + std::to_string(debug) + " image=" + std::to_string(image)
            + " " + source + "\n" + message);
    }
}

static void CheckFunctionValues(IRuntime* runtime, bool debug)
{
    // Every case runs both compiled and serialized, with/without debug data.
    // A live local in slot 1 makes an accidental function-ID-as-slot read visible.
    const std::string prefix = "fun One(){return 1;} fun Two(){return 2;}"
        "fun Identity(var value){return value;} export fun Test(){var sentinel=\"wrong slot\";var f=One;";
    const char* bodies[] = {
        "return type(One)==\"function\" && type(f)==\"function\" && f()==1;",
        "return f==One && One==f && One==One && One!=Two && Two!=One && !(f!=One);",
        "if(f!=One)return false;if(One!=f)return false;if(One==Two)return false;return true;",
        "var m={};m[One]=\"x\";return m[f]==\"x\" && m[One]==\"x\" && type(m.keys()[0])==\"function\";",
        "var m={};m[f]=3;m[One]+=4;return m[f]==7 && m[One]==7;",
        "var m={};m[One]=3;m[f]=null;return m.len()==0;",
        "var m={One:7,Two:8};return m[f]==7 && m[One]==7 && m[Two]==8;",
        "var m={One:Identity(9)};return m[f]==9;",
        "return (\"text\"..One)==(\"text\"..f) && (One..\"text\")== (f..\"text\");",
        "return tostring(One)==tostring(f) && toint(One)==toint(f) && tofloat(One)==tofloat(f) && tosize(One)==tosize(f);",
        "return type(fun(){return 3;})==\"function\";",
        "var capture=7;return type(fun(){return capture;})==\"function\";",
        "var capture=7;var a=fun(){return capture;};var b=a;return a==b && a!=fun(){return capture;};",
        "var capture=7;var m={fun(){return capture;}:8};var key=m.keys()[0];return type(key)==\"function\" && key()==7 && m[key]==8;",
        "var l=[One];var m={\"f\":One};l.append(One);return l[0]==One && l[1]==f && m.f==One && Identity(One)==f;",
        "return One()==1 && (One)()==1 && Identity(One)()==1;",
        "return (!One)==(!f) && (One && true)==(f && true) && (true && One)==(true && f) && (One || false)==(f || false);",
        "var n=0;if(One)n+=1;if(f)n-=1;return n==0;",
        "var n=0;while(One){n+=1;break;}while(f){n-=1;break;}return n==0;",
        "switch(One){case \"wrong slot\":return false;default:return true;}",
        "return type(+One)==\"function\" && type((+One))==\"function\";"
    };
    for (const char* body : bodies)
        Run(runtime, prefix + body + "}", debug);
    Run(runtime, "var kind=type(One);var m={One:4};fun One(){return 1;}"
        "export fun Test(){var f=One;return kind==\"function\" && m[f]==4 && One==f;}", debug);
    Run(runtime, "import good as a;import good as b;export fun Test(){var f=a.First;var m={a.First:7};"
        "return type(a.First)==\"function\" && a.First==f && f==b.First && m[b.First]==7 && a.First()==5;}", debug);
    Run(runtime, "fun One(){return 1;}fun Get(){return One;}export fun Test(){return Get()==One;}", debug);
    // Invalid value uses must reject the function type, never consume an unrelated slot.
    for (const char* operand : {"One", "f"})
    {
        const std::string value = operand;
        RuntimeReject(runtime, prefix + "for(var i in " + value + ",2){}}", "fun != number", debug);
        RuntimeReject(runtime, prefix + "for(var i in 0," + value + "){}}", "fun != number", debug);
        RuntimeReject(runtime, prefix + "for(var i in 0,2," + value + "){}}", "fun != number", debug);
        RuntimeReject(runtime, prefix + "foreach(var x in " + value + "){}}", "foreach does not support fun", debug);
        RuntimeReject(runtime, prefix + "return -" + value + ";}", "invalid operator '-' for 'fun'", debug);
        RuntimeReject(runtime, prefix + "return -(" + value + ");}", "invalid operator '-' for 'fun'", debug);
    }
    Reject(runtime, "fun One(){} One=3;", "assignment target", "language.ns", 1, debug);
    Reject(runtime, "fun One(){} One=One;", "assignment target", "language.ns", 1, debug);
    Reject(runtime, "fun One(){} One+=1;", "assignment target", "language.ns", 1, debug);
    Reject(runtime, "fun One(){} ++One;", "invalid target", "language.ns", 1, debug);
    Reject(runtime, "fun One(){} One++;", "do not support", "language.ns", 1, debug);
}

static void CheckSortSliced(IRuntime* runtime, bool debug)
{
    // The nonterminating comparator must never be entered. The CTest timeout
    // also bounds a regression that accidentally disables VM slicing again.
    for (bool image : {false, true})
    for (int timeout : {0, 1000})
    for (bool infinite : {false, true})
    {
        const std::string source = std::string("var a=[3,1,2]; export var calls=0; export var verified=0; fun Compare(var x,var y){calls+=1;") +
            (infinite ? "while(true){}" : "") + "return x<y;}"
            "export fun Test(){var n=0;for(var i in 0,30)n+=i;a.sort(Compare);}"
            "export fun Verify(){if(a[0]!=3 || a[1]!=1 || a[2]!=2)return false;"
            "a.sort(fun(var x,var y){return x<y;});if(a[0]==1 && a[1]==2 && a[2]==3)verified=1;}";
        const std::string label = "sliced sort debug=" + std::to_string(debug) + " image=" + std::to_string(image)
            + " timeout=" + std::to_string(timeout) + " infinite=" + std::to_string(infinite);
        ProgramHandle program;
        if (image)
        {
            std::vector<uint8_t> bytes;
            if (runtime->CompileToBytecode(Describe(source, debug), bytes).ok()) program = runtime->LoadProgram(bytes);
        }
        else program = runtime->Compile(Describe(source, debug)).program;
        Check((bool)program, "compile " + label);
        if (!program) continue;
        InstanceHandle instance = runtime->CreateInstance(program);
        bool rejected = false, untouched = false, reusable = false;
        if (instance)
        {
            RunStatus status = runtime->StartSliced(instance, "Test", timeout, 10)
                ? RunStatus::Suspended : RunStatus::Failed;
            for (int slices = 0; slices < 128 && status == RunStatus::Suspended; ++slices)
                status = runtime->UpdateSliced(instance);
            StringView error;
            rejected = status == RunStatus::Failed && runtime->PeekLastError(error)
                && error.str().find("list.sort is not allowed during time-limited execution") != std::string::npos;
            int32_t calls = -1;
            untouched = runtime->GetGlobalInt(instance, "calls", calls) && calls==0;
            if (rejected)
            {
                // Explicitly clear the worker's retained time-limit setting.
                const bool started = runtime->StartSliced(instance, "Verify", -1, 10);
                int32_t verified = 0;
                reusable = started && !runtime->IsRunning(instance)
                    && runtime->GetGlobalInt(instance, "verified", verified) && verified==1;
            }
            runtime->DestroyInstance(instance);
        }
        Check(rejected, "reject " + label);
        Check(untouched, "never enter comparator " + label);
        Check(reusable, "preserve list and allow subsequent unlimited sort " + label);
        runtime->DestroyProgram(program);
    }
}

static void CheckFormatLimits(IRuntime* runtime, bool debug)
{
    const std::string prefix = "export fun Test(){var s=\"x\";for(var i in 0,20)s=s..s;";
    Run(runtime, prefix + "return format(\"%s\",s).len()==1048576 && s.format().len()==1048576"
        " && format(\"%s%.0s\",s,\"x\").len()==1048576;}", debug);
    Run(runtime, "export fun Test(){var s=\"x\";for(var i in 0,19)s=s..s;"
        "return \"%s%s\".format(s,s).len()==1048576;}", debug);
    const std::string unicode = u8"export fun Test(){var s=\"😀\";for(var i in 0,18)s=s..s;";
    Run(runtime, unicode + "return format(\"%s\",s).len()==262144;}", debug);
    RuntimeReject(runtime, unicode + u8"format(\"%s\",s..\"😀\");}", "exceeds 1048576 bytes", debug);
    const char* overflows[] = {
        "format(\"%s\",s..\"x\");", "format(\"%s%s\",s,\"x\");",
        "format(\"%sX\",s);", "format(\"%s%%\",s);", "(s..\"x\").format();",
        "(s..\"%d\").format(1);", "(s..\"%1s\").format(\"\");", "format(\"%s%.1f\",s,1.0);"
    };
    for (const char* body : overflows)
        RuntimeReject(runtime, prefix + body + "}", "exceeds 1048576 bytes", debug);
    RuntimeReject(runtime, "export fun Test(){format(\"%4097d\",1);}", "width exceeds 4096", debug);
    RuntimeReject(runtime, "export fun Test(){\"%.1025f\".format(1.0);}", "precision exceeds 1024", debug);
}

static void CheckExportVisibility(IRuntime* runtime, bool debug)
{
    const std::string source = "import settings as cfg; public const LOCAL=7; export var visible=cfg.COUNT;";
    CompileResult compiled = runtime->Compile(Describe(source, debug));
    Check((bool)compiled.program, "compile exported const host visibility");
    if (!compiled.program) return;
    InstanceHandle instance = runtime->CreateInstance(compiled.program);
    int32_t value = 0;
    Check((bool)instance && runtime->GetGlobalInt(instance, "visible", value) && value==22,
        "export var remains visible to the host");
    Check(!runtime->GetGlobalInt(instance, "LOCAL", value)
        && !runtime->GetGlobalInt(instance, "COUNT", value)
        && !runtime->GetGlobalInt(instance, "cfg.COUNT", value), "const creates no host global");
    if (instance) runtime->DestroyInstance(instance);
    runtime->DestroyProgram(compiled.program);
}

static void CheckModuleState(IRuntime* runtime, bool debug)
{
    const std::string source = "import private_a as a; import private_a as again;"
        "var visibleA=99; export var own=7;"
        "export fun Change(){a.Set(41);return again.Get();}"
        "export fun Read(){return a.Get();} export fun Own(){return visibleA;}";
    std::vector<uint8_t> bytes;
    const Error compiled = runtime->CompileToBytecode(Describe(source, debug), bytes);
    Error error;
    ProgramHandle program = compiled.ok() ? runtime->LoadProgram(bytes, &error) : ProgramHandle{};
    Check((bool)program, "module state image: " + compiled.message + error.message);
    if (!program) return;
    InstanceHandle first = runtime->CreateInstance(program);
    InstanceHandle second = runtime->CreateInstance(program);
    Check((bool)first && (bool)second, "two module state instances");
    if (first && second)
    {
        int32_t value = 0;
        Check(runtime->GetGlobalInt(first, "visibleA", value) && value==12,
            "host export uses imported slot despite same private root name");
        Check(runtime->GetGlobalInt(first, "own", value) && value==7,
            "root host export retains its name and slot");
        Check(!runtime->GetGlobalInt(first, "value", value)
            && !runtime->GetGlobalInt(first, "private_a.value", value), "private globals are not host exports");
        {
            Invocation call = runtime->Call(first, "Change");
            Check(call.invoke()==RunStatus::Completed && call.retInt()==41, "two aliases share state");
        }
        {
            Invocation call = runtime->Call(second, "Read");
            Check(call.invoke()==RunStatus::Completed && call.retInt()==10, "instances have separate module state");
        }
        {
            Invocation call = runtime->Call(first, "Own");
            Check(call.invoke()==RunStatus::Completed && call.retInt()==99, "root private name remains independent");
        }
    }
    if (first) runtime->DestroyInstance(first);
    if (second) runtime->DestroyInstance(second);
    runtime->DestroyProgram(program);
}

int main()
{
    ModuleLoader loader;
    loader.sources["modules/good.ns"] =
        "var moduleValue=5; var items=[7,8]; public fun First(){return Last();}"
        "fun Last(){return moduleValue;} public fun Items(){return items;}";
    loader.sources["modules/outer.ns"] = "import inner;\npublic fun Outer(){return inner.First();}";
    loader.sources["modules/inner.ns"] = "public fun First(){return Last();}\nfun Last(){return 9;}";
    loader.sources["modules/bad.ns"] = "fun Broken(){\n return missing;\n}";
    loader.sources["modules/badouter.ns"] = "import bad;";
    loader.sources["modules/prototype.ns"] = "// old syntax\nfun Removed();";
    loader.sources["modules/duplicate.ns"] = "fun F(){}\nfun F(){}";
    loader.sources["modules/order.ns"] = "fun F(){return later;}\nvar later=1;";
    loader.sources["modules/badencoding.ns"] = std::string(1, (char)0xff);
    loader.sources["modules/settings.ns"] =
        "public const COUNT=22; public const NEG=-7; public const RATE=1.25; public const WHOLE=2.0;"
        "public const TEXT=\"red\"; public const EMPTY=\"\"; public const YES=true; public const NO=false;"
        "public const NOTHING=null; public const MASK=1<<3; const PRIVATE=100;"
        "var mutableValue=42; public fun Fetch(){return COUNT;} public fun GetMutable(){return mutableValue;}";
    loader.sources["modules/other.ns"] = "public const COUNT=33; const PRIVATE=200;";
    loader.sources["modules/facade.ns"] =
        "import settings as src; public const COUNT=src.COUNT+1; public const TEXT=src.TEXT;"
        "const HIDDEN=src.COUNT; public fun Fetch(){return src.Fetch();}";
    loader.sources["modules/privateuse.ns"] = "import settings as src;\nconst BAD=src.PRIVATE;";
    loader.sources["modules/constorder.ns"] = "fun F(){return LATE;}\npublic const LATE=1;";
    loader.sources["modules/memberfun.ns"] =
        "var holder={}; holder.fun=3; public fun Read(){return holder.fun;}";
    loader.sources["modules/private_a.ns"] =
        "var value=10; export var visibleA=12; public const LIMIT=20;"
        "public fun Get(){return value;} public fun Set(var x){value=x;}"
        "public fun Closure(var n){return fun(){return fun(){return value+n;};};}";
    loader.sources["modules/private_b.ns"] =
        "import private_a as a; var value=30; const visibleA=99;"
        "public fun Get(){return value+a.Get();} public fun Host(){return type(HostAPI);}";
    loader.sources["modules/private_leak.ns"] = "import private_a; fun Get(){return value;}";
    loader.sources["modules/root_leak.ns"] = "fun Get(){return rootPrivate;}";
    loader.sources["modules/export_duplicate.ns"] = "export var visibleA=44;";
    loader.sources["modules/shared.ns"] =
        "public var count=1; var hidden=2; public var items=[1,2]; fun Private(){return 1;}"
        "export fun HostOnly(){return 2;} export public fun Both(){return 3;}"
        "public fun Get(){return count;} public fun Items(){return items;} public fun Bump(){count=count+100;}";
    loader.sources["modules/shadow.ns"] = "var count=5; public fun Own(){return count;}";
    RuntimeDesc runtimeDesc;
    runtimeDesc.loader = &loader;
    IRuntime* runtime = CreateRuntime(runtimeDesc);
    NativeObjectDesc host;
    host.name = "HostAPI";
    runtime->RegisterObject(host);
    runtime->FreezeBindings();

    const char* bodies[] = {
        "var a=[1,3]; a.insert(1,2); a.insert(0,0); a.insert(4,4); return a.len()==5 && a[0]==0 && a[2]==2 && a[4]==4;",
        "var a=[]; a.insert(0,null); a.append(3,0); return a.len()==2 && a[0]==3 && a[1]==null;",
        "var a=[1,2,3]; var shared=a; var r=a.remove(1); return r==2 && shared.len()==2 && a[1]==3;",
        "var a=[1,null,3]; var r=a.remove(1); return r==null && a.len()==2 && a[1]==3;",
        "var a=[1,2,3]; var first=a.remove(0); var last=a.remove(1); var only=a.remove(0); return first==1 && last==3 && only==2 && a.len()==0;",
        "var a=[{\"x\":3},[4],\"tail\"]; var r=a.remove(0); a.resize(0); return r.x==3;",
        "var a=[]; a.append(a); var r=a.remove(0); r.append(7); return a.len()==1 && a[0]==7;",
        "var a=[3,1,2]; a.sort(fun(var x,var y){return x<y;}); return a[0]==1 && a[1]==2 && a[2]==3;",
        "var a=[3,1,2,1]; var alias=a; a.sort(fun(var x,var y){return x>y;}); return alias[0]==3 && a[1]==2 && a[2]==1 && a[3]==1;",
        "var a=[{\"k\":1,\"id\":0},{\"k\":0,\"id\":1},{\"k\":1,\"id\":2}]; a.sort(fun(var x,var y){return x.k<y.k;}); return a[0].id==1 && a[1].id==0 && a[2].id==2;",
        "var descending=true; var a=[1,3,2]; a.sort(fun(var x,var y){if(descending)return x>y;return x<y;}); return a[0]==3 && a[2]==1;",
        "var a=[\"b\",\"a\",\"c\"]; a.sort(fun(var x,var y){return x<y;}); return a[0]==\"a\" && a[2]==\"c\";",
        "var a=[]; var cmp=fun(var x,var y){return x<y;}; a.sort(cmp); a.append(5); a.sort(cmp); return a.len()==1 && a[0]==5;",
        "var a=[3,2,1]; a.sort(fun(var x,var y){return true;}); return a.len()==3;",
        "var a=[]; for(var i in 257,0,-1)a.append(i); a.sort(fun(var x,var y){return x<y;}); for(var i in 0,257){if(a[i]!=i+1)return false;} return true;",
        u8"return \"글자\".len()==2 && '글자'.len()==2 && (\"가\"..\"나\").len()==2;",
        "return \" a,b \".trim().split(\",\")[1].upper()==\"B\";",
        "var a=[7]; return (a)[0]==7 && (fun(){return 3;})()==3;",
        "var n=7; return (fun(){return n;})()==7;",
        "var a=[1]; (a).append(2); \"unused\".len(); return a.len()==2;",
        u8"return \"\\uAE00\\uc790\"==\"글자\" && \"\\uD83D\\uDE00\".len()==1 && '\\u0041'==\"A\";",
        "return \"\\\\u0041\"==\"\\u005Cu0041\" && \"\\u0022\"==\"\\\"\" && \"\\u0027\"==\"'\";",
        "return format(\"%04d / %.2f / %s / %%\",-3,1.25,\"ok\")==\"-003 / 1.25 / ok / %\";",
        "return \"%d %.0f\".format(-2147483648,2147483647)==\"-2147483648 2147483647\";",
        "return format(\"%f\",2)==\"2.000000\" && format(\"%08.2f\",-1.5)==\"-0001.50\";",
        "return format(\"[%5d][%-5s][%-05d]\",3,\"ab\",7)==\"[    3][ab   ][7    ]\";",
        u8"return format(\"[%4.2s]\",\"한😀글\")==\"[  한😀]\" && \"%.0s\".format(\"abc\")==\"\";",
        "return format(\"%%\")==\"%\" && \"plain\".format()==\"plain\" && format(\"\")==\"\";",
        "return format(\"%4096d\",1).len()==4096 && \"%-4096s\".format(\"x\").len()==4096;",
        "return format(\"%.1024f\",1.0).len()==1026 && \"%.2147483647s\".format(\"abc\")==\"abc\";",
        // Keyword-shaped members must not become declarations in the pre-pass.
        "var t={}; t.fun=3; var x=t.fun; return x==3;",
        "var t={\"fun\":3}; t.fun+=2; return t.fun==5;",
        "var t={\"fun\":{\"fun\":3}}; return t.fun.fun==3;",
        "var n=3; var t={}; t.fun=fun(){return n;}; return t.fun()==3;",
        "var a=[11,22]; var m={\"x\":a[1]}; return type(m.x)==\"int\" && m.x==22;",
        "var a=[11,22]; var m={a[1]:77}; return m[22]==77 && m[a]==null;",
        "var a={\"n\":22}; var m={\"x\":a.n}; return type(m.x)==\"int\" && m.x==22;",
        "var a=[11,22]; var m={a[1]}; return m[0]==22;",
        "var a=[[11,22]]; var m={\"x\":{\"y\":a[0][1]}}; return m.x.y==22;",
        "var a=[11,22]; var m={\"x\":a[1],\"y\":a[0]}; return m.x==22 && m.y==11;",
        "var a=[null]; var m={\"x\":a[0]}; return m.len()==0;",
        "var a=[11,22]; var b=[a[1]]; return b[0]==22;",
        "var s=\"abc\"; return s.sub(1,99)==\"bc\" && s.sub(3,1)==\"\" && s.sub(99,1)==\"\";",
        "var s=\"abc\"; return s.sub(-5,2)==\"ab\" && s.sub(1,-5)==\"\" && s.sub(1,0)==\"\";",
        "var s=\"\"; return s.sub(0,0)==\"\" && s.sub(0,10)==\"\" && s.sub(-1,3)==\"\";",
        "var s=\"abc\"; return s.sub(2147483647,2147483647)==\"\" && s.sub(-2147483648,2147483647)==s;",
        u8"var s=\"A한😀글Z\"; return s.len()==5 && s.sub(1,3)==\"한😀글\" && s.sub(4,99)==\"Z\";",
        u8"var s=\"A한😀글Z\"; return s.find(\"😀\")==2 && s.find(\"missing\")==-1 && s.find(\"\")==0;",
        "var s=\"aba\"; return s.replace(\"a\",\"X\")==\"Xba\" && s==\"aba\";",
        "var s=\"abc\"; return s.replace(\"z\",\"X\")==s && s.replace(\"\",\"X\")==\"Xabc\";",
        "var s=\"\"; return s.replace(\"a\",\"X\")==\"\" && s.replaceAll(\"a\",\"X\")==\"\";",
        "var s=\"ababa\"; return s.replaceAll(\"a\",\"X\")==\"XbXbX\" && s==\"ababa\";",
        "var s=\"ababa\"; return s.replaceAll(\"aba\",\"X\")==\"Xba\";",
        "var s=\"aaa\"; return s.replaceAll(\"a\",\"aa\")==\"aaaaaa\" && s.replaceAll(\"a\",\"\")==\"\";",
        "var s=\"abc\"; return s.replaceAll(\"z\",\"X\")==s && s.replaceAll(\"\",\"X\")==s;",
        u8"var s=\"한😀한😀\"; return s.replaceAll(\"한😀\",\"글\")==\"글글\";"
    };
    const char* programs[] = {
        "var value=3; fun Get(){return value;} fun Set(){value=9;return 4;}"
        "export fun Test(){var sum=Get()+Set();return sum==7 && Get()==9;}",
        "var list=[1];fun Get(){return list;}fun Reset(){list=[9];return 0;}"
        "export fun Test(){var old=Get();Get()[Reset()]=7;return old[0]==7 && Get()[0]==9;}",
        "var count=0;fun Touch(){count++;return count;}fun Minus(){return -2;}"
        "fun Capture(var n){return fun(){return n;};}export fun Test(){var f=Capture(5);"
        "return Touch()+Touch()==3 && count==2 && Minus()==-2 && f()==5;}",
        "var value=1; import private_a as a; import private_b as b; import private_a as again;"
        "export fun Test(){a.Set(17);var f=a.Closure(5);return value==1 && again.Get()==17 && b.Get()==47 && f()()==22 && a.LIMIT==20 && b.Host()==type(HostAPI);}",
        "import private_a as a; var value=2; export fun Test(){var value=4;{var value=8;if(value!=8)return false;}return value==4 && a.Get()==10;}",
        "const TEXT=\"abc\"; export fun Test(){return TEXT.len()==3 && (TEXT).upper()==\"ABC\";}",
        "const TEXT=\"\\u0041\"; export fun Test(){switch(\"A\"){case TEXT:return true;default:return false;}}",
        "import system; export fun Test(){var s=system.set([1,1,2]); return s.len()==2 && tosize(s)==2 && system.set([]).len()==0;}",
        "import system; fun Previous(){return 7;} export fun Test(){var s=system.set([1,2]); Previous(); return s.len()==2;}",
        "var a=[3,2,1]; fun Compare(var x,var y){return x<y;} export fun Test(){a.sort(Compare);return a[0]==1 && a[2]==3;}",
        "var a=[3,2,1]; var hold=a; fun Compare(var x,var y){a=null;return x<y;} export fun Test(){hold.sort(Compare);return a==null && hold[0]==1 && hold[2]==3;}",
        "fun Make(){return [{\"x\":7}];} export fun Test(){return Make().remove(0).x==7;}",
        "var t={}; t.fun=3; var x=t.fun; export fun Test(){return x==3;}",
        "import math; export fun Test(){var l=[math.Vector3(1,2,3)];l[0][1]=7;l[0][2]+=10;l[0][0]++;--l[0][1];return l[0][0]==2 && l[0][1]==6 && l[0][2]==13;}",
        "import math; export fun Test(){var m={};m.k=math.Vector3(1,2,3);m.k[0]=9;m.k[1]-=2;return m.k[0]==9 && m.k[1]==0 && m.k[2]==3;}",
        "import math; export fun Test(){var a=[[math.Vector2(1,2)]];a[0][0][1]=5;a[0][0][0]*=4;return a[0][0][0]==4 && a[0][0][1]==5;}",
        "import math; export fun Test(){var v=math.Vector3(1,2,3);var l=[v];l[0][0]=9;return v[0]==1 && l[0][0]==9;}",
        "import math; export fun Test(){var l=[math.Vector3(1,2,3)];var t=l[0];t[0]=9;return l[0][0]==1 && t[0]==9;}",
        "import math; export fun Test(){var l=[math.Vector3(1,2,3)];var r=l[0][1]++;var s=++l[0][2];return r==2 && s==4 && l[0][1]==3 && l[0][2]==4;}",
        "export fun Test(){var q=[[1,2,3]];q[0][1]=7;q[0][2]+=1;var m={};m.k=[5];m.k[0]--;return q[0][1]==7 && q[0][2]==4 && m.k[0]==4;}",
        "import math; var g=[math.Vector3(0,0,0)]; export fun Test(){g[0][2]=4;g[0][2]*=2;return g[0][2]==8;}",
        "import math; fun Make(){return [math.Vector3(1,2,3)];} export fun Test(){var l=Make();l[0][0]=5;return l[0][0]==5;}",
        "import math; var i=0; var a=[math.Vector3(1,2,3),math.Vector3(100,200,300)]; fun F(){i=1;return 10;}"
            "export fun Test(){a[i][0]+=F();return a[0][0]==1 && a[1][0]==110 && i==1;}",
        "import math; var k=[math.Vector3(1,2,3)]; var orig=k; var other=[math.Vector3(7,7,7)]; fun H(){k=other;return 10;}"
            "export fun Test(){k[0][0]+=H();return orig[0][0]==1 && other[0][0]==17;}",
        "import math; export fun Test(){var b=[math.Vector3(1,2,3)];b[0][0]=++b[0][1];return b[0][0]==3 && b[0][1]==3 && b[0][2]==3;}",
        "import math; var fs=0; var gs=0; var l=[math.Vector3(0,0,0),math.Vector3(0,0,0)]; fun F(){fs++;return 1;} fun G(){gs=fs*10+1;return 5;}"
            "export fun Test(){l[F()][2]=G();return l[1][2]==5 && l[0][2]==0 && fs==1 && gs==11;}",
        "import math; export fun Test(){var l=[math.Vector3(1,2,3)];var m={};m.v=l[0];m.v[0]=l[0][1]++;return m.v[0]==2 && l[0][1]==3 && l[0][0]==1;}",
        "import math; var i=0; var a=[math.Vector3(1,2,3),math.Vector3(100,200,300)]; fun F(){i=1;return 10;}"
            "export fun Test(){(a[i][0])+=F();return a[0][0]==1 && a[1][0]==110;}",
        "import math; export fun Test(){var b=[math.Vector3(1,2,3)];((b[0][0]))=++b[0][1];return b[0][0]==3 && b[0][1]==3 && b[0][2]==3;}",
        "import math; export fun Test(){var l=[math.Vector3(1,2,3)];var x=(l[0][1]+=5);var y=(l[0][2]=9);var z=toint(l[0][0]*=4);"
            "return x==7 && y==9 && z==4 && l[0][0]==4 && l[0][1]==7 && l[0][2]==9;}",
        "import math; fun Take(var v){return v;} export fun Test(){var l=[math.Vector3(1,2,3)];var r=Take(l[0][1]);(l[0][1])++;return r==2 && l[0][1]==3;}",
        "var t={}; t.fun=Later; export fun Test(){return t.fun()==3;} fun Later(){return 3;}",
        "export fun Test(){return Make().fun[0]==3;} fun Make(){return {\"fun\":[3]};}",
        "import memberfun; export fun Test(){return memberfun.Read()==3;}",
        "export fun Test(){return Later(6)==7;} fun Later(var n){return n+1;}",
        "fun Even(var n){if(n==0)return true;return Odd(n-1);}"
        "fun Odd(var n){if(n==0)return false;return Even(n-1);} export fun Test(){return Even(10);}",
        "var first=Later(); fun Later(){return 7;} export fun Test(){return first==7;}",
        "var f=Later; var a=[Later]; var m={\"f\":Later};"
        "fun Later(){return 7;} export fun Test(){return f()==7 && a[0]()==7 && m.f()==7;}",
        "var value=7; export fun Test(){return Later()==value;} fun Later(){return value;}",
        "export fun Test(){return Inner()==7;fun Inner(){return 7;}}",
        "export fun Test(){var n=7;var f=fun(){return n+Later();};return f()==9;} fun Later(){return 2;}",
        "var text=\"fun Fake(); { }\"; /* fun Fake(); */ // fun Fake();\n"
        "export fun Test(){return Later()==7;} fun Later(){return 7;}",
        "import good; export fun Test(){return good.First()==5 && good.Items()[1]==8;}",
        "import outer; export fun Test(){return outer.Outer()==9;}",
        "import good as a; import good as b; export fun Test(){return a.First()==b.First();}",
        "var keys=[3]; fun Value(){keys[0]=4;return 77;} var m={keys[0]:Value()};"
        "export fun Test(){return m[3]==77 && m[4]==null && keys[0]==4;}",
        "var calls=0; fun Index(){calls+=1;return 0;} var a=[22]; var m={a[Index()]:a[Index()]};"
        "export fun Test(){return calls==2 && m[22]==22;}",
        "import settings as cfg; var n=cfg.COUNT; export fun Test(){return n==22 && cfg.Fetch()==22;}",
        "import settings as cfg; const SCALE=(cfg.COUNT+2)*2; export fun Test(){return SCALE==48;}",
        "import settings as cfg; export fun Test(){switch(22){case cfg.COUNT:return true;default:return false;}}",
        "import settings as cfg; export fun Test(){switch(23){case cfg.COUNT+1:return true;default:return false;}}",
        "import settings as cfg; export fun Test(){switch(\"red\"){case cfg.TEXT:return true;default:return false;}}",
        "import settings as cfg; export fun Test(){switch(true){case cfg.YES:return true;default:return false;}}",
        "import settings as cfg; const RATE=cfg.RATE*2; const TEXT=cfg.TEXT; const YES=cfg.YES; const NIL=cfg.NOTHING;"
        "export fun Test(){return RATE==2.5 && TEXT==\"red\" && YES==true && NIL==null;}",
        "import settings as cfg; export fun Test(){return cfg.NEG==-7 && -cfg.COUNT==-22 && -cfg.NEG==7;}",
        "import settings as cfg; export fun Test(){return cfg.RATE==1.25 && type(cfg.WHOLE)==\"float\";}",
        "import settings as cfg; export fun Test(){return cfg.YES && !cfg.NO && cfg.NOTHING==null;}",
        "import settings as cfg; export fun Test(){return cfg.TEXT.len()==3 && cfg.TEXT.upper()==\"RED\" && cfg.EMPTY.len()==0;}",
        "import settings as cfg; export fun Test(){var a=[cfg.COUNT,cfg.TEXT,cfg.NOTHING];"
        "var m={cfg.TEXT:cfg.COUNT}; return a[0]==22 && a[1]==\"red\" && a[2]==null && m.red==22;}",
        "import settings as cfg; export fun Test(){var n=0; for(var i in 0,cfg.COUNT) n+=1; return n==22;}",
        "import settings as cfg; const MASK=cfg.MASK|1; export fun Test(){return MASK==9 && (cfg.MASK|2)==10;}",
        "import settings as cfg; const COUNT=99; const Fetch=77; const TEXT=123;"
        "export fun Test(){return cfg.COUNT==22 && cfg.Fetch()==22 && cfg.TEXT==\"red\" && COUNT==99;}",
        "import settings as cfg; const COUNT=99; const COPY=cfg.COUNT;"
        "export fun Test(){switch(COPY){case cfg.COUNT:return true;default:return false;}}",
        "import settings as a; import settings as b; import other as c;"
        "export fun Test(){return a.COUNT==b.COUNT && c.COUNT==33;}",
        "import facade as cfg; export fun Test(){return cfg.COUNT==23 && cfg.TEXT==\"red\" && cfg.Fetch()==22;}",
        "import settings as cfg; export fun Test(){return cfg.GetMutable()==42;}",
        "import settings; export fun Test(){return settings.COUNT==22;}",
        "import settings as cfg; export fun Test(){var get=fun(){return cfg.COUNT;};return get()==22;}",
        // export var: 별칭으로 읽고 쓰며 자기 모듈의 전역과 같은 슬롯이다.
        "import shared as s; export fun Test(){return s.count==1 && s.items.len()==2 && s.items[1]==2;}",
        "import shared as s; export fun Test(){s.count=5; s.count+=2; s.count++; ++s.count; return s.count==9 && s.Get()==9;}",
        "import shared as s; export fun Test(){s.Bump(); return s.count==101 && -s.count==-101 && s.count*2==202;}",
        "import shared as s; export fun Test(){s.items.append(3); s.items[0]=9;"
        "return s.Items()[0]==9 && s.Items().len()==3 && s.items==s.Items();}",
        "import shared as s; export fun Test(){s.items=[4]; return s.Items()[0]==4 && s.Items().len()==1;}",
        "import shared as a; import shared as b; export fun Test(){a.count=7; return b.count==7 && b.Get()==7;}",
        "import shared as s; export fun Test(){var f=fun(){return s.count+1;}; s.count=3; return f()==4;}",
        "import shared as s; import shadow as sh; export fun Test(){sh.Own(); return s.count==1 && sh.Own()==5;}",
        "import private_a as a; export fun Test(){return a.LIMIT==20 && a.Get()==10;}",
        "import shared as s; export fun Test(){return s.Both()==3;}",
        "public export var both=5; public var p=1; export public var both2=6; export fun Test(){return both==5 && p==1 && both2==6;}",
        "import shared as s; export fun Test(){var m={}; m[s.count]=\"one\"; return m[1]==\"one\";}"
    };
    for (bool debug : {false, true})
    {
        CheckModuleState(runtime, debug);
        Reject(runtime, "import private_a; export fun Test(){return value;}", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "import private_a; export fun Test(){value=8;}", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "import private_leak;", "unknown identifier", "modules/private_leak.ns", 1, debug);
        Reject(runtime, "var rootPrivate=3; import root_leak;", "unknown identifier", "modules/root_leak.ns", 1, debug);
        Reject(runtime, "import private_a; import export_duplicate;", "duplicate exported variable", "modules/export_duplicate.ns", 1, debug);
        Reject(runtime, "import private_a; export fun Test(){return visibleA;}", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "import shared as s; export fun Test(){return s.hidden;}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import shared as s; export fun Test(){s.hidden=1;}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import shared as s; import shadow as sh; export fun Test(){return sh.count;}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import private_b as b; export fun Test(){return b.visibleA;}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import shared as s; export fun Test(){return count;}", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "import shared as s; export fun Test(){return s.Private();}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import shared as s; export fun Test(){return s.HostOnly();}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import private_a as a; export fun Test(){return a.visibleA;}", "no public member", "language.ns", 1, debug);
        Reject(runtime, "export const X=1;", "use 'public const'", "language.ns", 1, debug);
        Reject(runtime, "export public const X=1;", "use 'public const'", "language.ns", 1, debug);
        Reject(runtime, "public public var x=1;", "duplicate 'public'", "language.ns", 1, debug);
        Reject(runtime, "export export var x=1;", "duplicate 'export'", "language.ns", 1, debug);
        Reject(runtime, "public x=1;", "expected 'var', 'const' or 'fun' after 'public'", "language.ns", 1, debug);
        Reject(runtime, "export fun Test(){public var x=1;}", "'public' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "{ public var x=1; } var x=2; export fun Test(){return x;}", "'public' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "{ public fun G(){return 1;} }", "'public' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "{ const N=1; }", "'const' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "export fun Test(){const N=1; return N;}", "'const' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "{ export var x=1; }", "'export' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "export fun Test(){export var x=1;}", "'export' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "{ export fun G(){return 1;} }", "'export' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "{ import math; }", "'import' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "export fun Test(){import math; return 1;}", "'import' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "if (true) { import settings as cfg; }", "'import' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "var HostAPI=1;", "already defined", "language.ns", 1, debug);
        CheckFunctionValues(runtime, debug);
        Run(runtime, R"(export fun Test(){var s="A\uAC00\uAC01\uD83D\uDE00";
            return s.codepoint(0)==65 && s.codepoint(1)==44032 && s.codepoint(2)==44033 &&
                s.codepoint(3)==128512 && s.codepoint(4)==-1 && s.codepoint(-1)==-1 &&
                "".codepoint(0)==-1 && s.codepoint(2147483647)==-1 &&
                (s.codepoint(2)-44032)%28==1;})", debug);
        RuntimeReject(runtime, R"(export fun Test(){"a".codepoint(0.0);})", "", debug);
        RuntimeReject(runtime, R"(export fun Test(){"a".codepoint();})", "", debug);

        CheckSortSliced(runtime, debug);
        CheckFormatLimits(runtime, debug);
        CheckExportVisibility(runtime, debug);
        for (const char* body : bodies)
            Run(runtime, std::string("export fun Test(){") + body + "}", debug);
        for (const char* source : programs) Run(runtime, source, debug);
        const char* badEscapes[] = {"\\u12", "\\u12G4", "\\uD800", "\\uDC00", "\\uD800\\u0041", "\\uD800\\uD800", "\\u0000"};
        for (const char* escaped : badEscapes)
            Reject(runtime, std::string("var s=\"") + escaped + "\";", "invalid Unicode escape", "language.ns", 1, debug);
        const char* invalidCalls[] = {
            "var a=[]; a.insert(-1,3);", "var a=[1]; a.insert(2,3);",
            "var a=[]; a.remove(0);", "var a=[1]; a.remove(-1);", "var a=[1]; a.remove(1);",
            "var a=[1]; a.insert(true,3);", "var a=[1]; a.remove(0.0);",
            "var a=[]; a.insert(0);", "var a=[]; a.remove();", "var a=[]; a.append();",
            "var a=[]; a.sort(7);", "var a=[]; a.sort();",
            "var a=[1,2]; a.sort(fun(var x,var y){return 1;});",
            "var a=[1,2]; a.sort(fun(var x){return true;});",
            "var a=[1,2]; foreach(var v in a)a.remove(0);",
            "var a=[1,2]; foreach(var v in a)a.insert(0,3);",
            "var a=[1,2]; foreach(var v in a)a.sort(fun(var x,var y){return x<y;});",
            "format();", "format(1);", "format(\"%d\");", "format(\"x\",1);", "format(\"%n\",1);",
            "format(\"%\");", "format(\"%d\",1.5);", "format(\"%f\",\"1\");", "format(\"%s\",1);",
            "format(\"%*d\",3,1);", "format(\"%.d\",1);", "format(\"%.2d\",1);", "format(\"%03s\",\"a\");",
            "format(\"%2147483648d\",1);", "format(\"%.2147483648f\",1.0);", "\"%d\".format();",
            "var s=system.set([1]); s.missing();", "var s=system.set([1]); s.len(1);"
        };
        for (const char* body : invalidCalls)
            RuntimeReject(runtime, std::string("import system; export fun Test(){") + body + "}", "", debug);
        RuntimeReject(runtime, "var a=[3,1,2]; fun C(var x,var y){a.append(4);return x<y;} export fun Test(){a.sort(C);}"
            "export fun Verify(){return a.len()==4 && a[0]==3 && a[1]==1 && a[3]==4;}", "modified during sort", debug, true);
        RuntimeReject(runtime, "var a=[3,1,2]; fun C(var x,var y){a.resize(0);return x<y;} export fun Test(){a.sort(C);}"
            "export fun Verify(){return a.len()==0;}", "modified during sort", debug, true);
        RuntimeReject(runtime, "var a=[3,1,2]; fun C(var x,var y){var bad=[1];return bad[9];} export fun Test(){a.sort(C);}"
            "export fun Verify(){return a[0]==3 && a[1]==1 && a[2]==2;}", "", debug, true);
        for (const char* pause : {"yield;", "sleep(1);", "coroutine.resume(coroutine.create(fun(){}));", "coroutine.close();"})
            RuntimeReject(runtime, std::string("import coroutine; var a=[3,1,2]; fun C(var x,var y){") + pause +
                "return x<y;} export fun Test(){a.sort(C);}"
                "export fun Verify(){a.sort(fun(var x,var y){return x<y;});return a[0]==1 && a[1]==2 && a[2]==3;}",
                "not allowed", debug, true);
        Reject(runtime, "fun Old();", "forward declarations", "language.ns", 1, debug);
        Reject(runtime, "fun = 3;", "invalid function name", "language.ns", 1, debug);
        Reject(runtime, "fun Old(var n);\nfun Old(var n){return n;}", "forward declarations", "language.ns", 1, debug);
        Reject(runtime, "fun F(){}\nfun F(){}", "duplicate function", "language.ns", 2, debug);
        Reject(runtime, "fun First(){return Last();}\nfun Last(var n){return n;}", "argument count mismatch", "language.ns", 1, debug);
        Reject(runtime, "var a=later;\nvar later=7;", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "fun F(){return later;}\nvar later=7;", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "fun F(){return local;var local=7;}", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "fun F(){return LATE;}\nconst LATE=7;", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "\nimport bad;", "unknown identifier", "modules/bad.ns", 2, debug);
        Reject(runtime, "import badouter;", "unknown identifier", "modules/bad.ns", 2, debug);
        Reject(runtime, "import prototype;", "forward declarations", "modules/prototype.ns", 2, debug);
        Reject(runtime, "import duplicate;", "duplicate function", "modules/duplicate.ns", 2, debug);
        Reject(runtime, "import order;", "unknown identifier", "modules/order.ns", 1, debug);
        Reject(runtime, "import badencoding;", "Invalid script source encoding", "modules/badencoding.ns", 1, debug);
        Reject(runtime, "import good; var value=good.items[0];", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; var v=cfg.PRIVATE;", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; const V=cfg.PRIVATE;", "no public const", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; var v=cfg.mutableValue;", "no public member", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; const V=cfg.mutableValue;", "no public const", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; const V=cfg.Fetch;", "no public const", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; var v=COUNT;", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "var v=cfg.COUNT; import settings as cfg;", "unknown identifier", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; const cfg=1;", "already defined", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.COUNT=1;", "assignment target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.NOTHING=1;", "assignment target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.TEXT=\"blue\";", "assignment target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.COUNT+=1;", "assignment target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; ++cfg.COUNT;", "invalid target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; ++cfg.NOTHING;", "invalid target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.COUNT++;", "do not support", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.NOTHING++;", "do not support", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.COUNT=fun(){return 1;};", "assignment target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; cfg.NOTHING=fun(){return 1;};", "assignment target", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; switch(2){case cfg.WHOLE: break;}", "float is not allowed", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; switch(22){case cfg.COUNT: break;case 22: break;}", "duplicate case", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; const V=cfg.UNKNOWN;", "no public const", "language.ns", 1, debug);
        Reject(runtime, "import settings as cfg; var v=cfg.;", "member name", "language.ns", 1, debug);
        Reject(runtime, "fun F(){public const N=1;}", "'public' is only allowed at file scope", "language.ns", 1, debug);
        Reject(runtime, "import privateuse;", "no public const", "modules/privateuse.ns", 2, debug);
        Reject(runtime, "import constorder;", "unknown identifier", "modules/constorder.ns", 1, debug);
        Reject(runtime, "import facade as cfg; const N=cfg.HIDDEN;", "no public const", "language.ns", 1, debug);
        Reject(runtime, "public const VALUE=1; public const VALUE=2;", "already defined", "language.ns", 1, debug);
    }
    DestroyRuntime(runtime);
    std::printf("Language regression %s: %d/%d\n", failures ? "FAIL" : "PASS", checks-failures, checks);
    return failures ? 1 : 0;
}
