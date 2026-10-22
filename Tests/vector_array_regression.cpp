#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;

static bool Host(CallContext& ctx, StringView name)
{
    ArrayView a;
    if (name.str() != "Edit" || !ctx.argAsArray(0, a)) return false;
    const int dim = ctx.argInt(1);
    bool ok = a.vectorDimension() == dim && a.count() == 2 &&
        (int)a.elementType() == (int)ArrayElementType::Vector2 + dim - 2 &&
        a.floats() == nullptr && a.ints() == nullptr && a.boolBits() == nullptr;
    float* data = a.vectorData();
    ok = ok && data != nullptr;
    if (data)
    {
        for (int i = 0; i < 2 * dim; ++i) ok = ok && data[i] == (float)(i % dim + 1);
        data[dim] = 20;
    }
    ctx.retBool(ok);
    return true;
}

static void Run(IRuntime* runtime, const std::string& body, bool debug, bool image,
    const char* expectedError = nullptr)
{
    AllocStats before;
    GetAllocStats(before);
    const std::string source = "import math; import system; " + body;
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "vector-array.ns";
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
        auto compiled = runtime->Compile(desc);
        program = compiled.program; error = compiled.error;
    }
    ++checks;
    if (!program)
    {
        ++failures;
        std::fprintf(stderr, "compile failed: %s\n%s\n", error.message.c_str(), body.c_str());
        return;
    }
    for (int instanceIndex = 0; instanceIndex < 2; ++instanceIndex)
    {
        auto instance = runtime->CreateInstance(program);
        // Repeat to detect state pollution and leaked temporary vector values on errors.
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            ++checks;
            auto call = runtime->Call(instance, "Test");
            const auto status = call.invoke();
            bool ok = expectedError
                ? status == RunStatus::Failed && call.error().message.find(expectedError) != std::string::npos
                : status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool();
            if (!ok)
            {
                ++failures;
                std::fprintf(stderr, "FAIL debug=%d image=%d: %s\n%s\n", debug, image, call.error().message.c_str(), body.c_str());
            }
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    AllocStats after;
    GetAllocStats(after);
    ++checks;
    if (after.vectors != before.vectors || after.arrays != before.arrays || after.bufferBytes != before.bufferBytes)
    {
        ++failures;
        std::fprintf(stderr, "allocation leak after program destruction\n%s\n", body.c_str());
    }
}

int main()
{
    auto runtime = CreateRuntime();
    NativeObjectDesc host; host.name = "Host"; host.method = &Host;
    runtime->RegisterObject(host); runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true})
    {
        for (int dim = 2; dim <= 4; ++dim)
        {
            std::string vector = "math.Vector" + std::to_string(dim) + "(1,2";
            if (dim >= 3) vector += ",3";
            if (dim == 4) vector += ",4";
            vector += ")";
            const std::string setup = "export fun Test(){var v=" + vector + ";var a=system.array(v,2);";
            Run(runtime, setup + "return a.len()==2 && tosize(a)==2 && type(a)==\"array\" && type(a[0])==\"Vector" + std::to_string(dim) + "\" && a[0]==v && a[1]==v;}", debug, image);
            Run(runtime, setup + "var b=a;var p=a[0];p[0]=90;b[1][0]+=3;return a==b && a[0][0]==1 && a[1][0]==4 && v[0]==1 && p[0]==90;}", debug, image);
            Run(runtime, setup + "v[0]=90;a.resize(0);a.append(v);a.resize(30);a[29][0]=8;a.resize(1);a.resize(30);return a[0][0]==90 && a[1][0]==1 && a[29][0]==1;}", debug, image);
            Run(runtime, setup + "var sum=0.0;foreach(var p in a){sum+=p[0];p[0]=99;}return sum==2 && a[0][0]==1 && a[1][0]==1;}", debug, image);
            Run(runtime, setup + "var old=a[0][0]++;var now=++a[1][0];a[0][1]*=4;a[0][1]/=2;a[0][1]-=1;return old==1 && now==2 && a[0][1]==3 && a[1][1]==2;}", debug, image);
            Run(runtime, setup + "var m={\"items\":[a]};m.items[0][1][0]=7;var p=(a[0][0]+=4);return p==5 && a[0][0]==5 && a[1][0]==7;}", debug, image);
            Run(runtime, setup + "a[0]=a[1]+v;a[1]=a[0]*2;return a[0][0]==2 && a[1][0]==4 && a[1][1]==8;}", debug, image);
            Run(runtime, setup + "a=a[0];return a==v;}", debug, image);
            Run(runtime, setup + "var p=a[1];return p[0]==1 && Host.Edit(a," + std::to_string(dim) + ") && a[0][0]==1 && a[1][0]==20 && p[0]==1;}", debug, image);
            Run(runtime, "export fun Test(){var a=system.array(" + vector + ",0);a.append(" + vector + ");return a.len()==1 && a[0]==" + vector + ";}", debug, image);
            const std::string wrong = dim == 2 ? "math.Vector3(1,2,3)" : "math.Vector2(1,2)";
            for (const auto& action : {"a[0]=" + wrong, "a.append(" + wrong + ")", std::string("a[0]=1"), std::string("a[0]=null")})
                Run(runtime, setup + action + ";return true;}", debug, image, "array");
            Run(runtime, setup + "a[0]+=v;return true;}", debug, image, "+=");
            Run(runtime, setup + "a[2][0]=1;return true;}", debug, image, "index");
            Run(runtime, setup + "a[0][" + std::to_string(dim) + "]=1;return true;}", debug, image, "index");
            Run(runtime, setup + "foreach(var p in a){a.append(v);}return true;}", debug, image, "collection was modified");
        }
        Run(runtime, R"(
export fun Test(){
 var a=system.array(math.Quaternion(1,2,3,4),1);a[0][3]=8;
 return type(a[0])=="Vector4" && a[0]==math.Vector4(1,2,3,8);
})", debug, image);
        Run(runtime, R"(
export fun Test(){
 var a=system.array(math.Vector3(1,2,3),2);var i=0;
 var change=fun(){a.resize(64);a[1]=math.Vector3(10,20,30);return 5;};
 a[1][0]+=change();
 var get=fun(){return a;};get()[0][1]=8;
 return a[1]==math.Vector3(15,20,30) && a[0]==math.Vector3(1,8,3);
})", debug, image);
        Run(runtime, R"(
var i=0;var calls=0;
fun Index(){calls+=1;return 0;}
fun Change(){i=1;return 4;}
export fun Test(){i=0;calls=0;var a=system.array(math.Vector2(1,2),2);
 (a[i])[0]+=Change();a[Index()][1]+=Change();
 return calls==1 && a[0]==math.Vector2(1,6) && a[1]==math.Vector2(5,2);
})", debug, image);
        Run(runtime, R"(
export fun Test(){var a=system.array(math.Vector2(1,2),2);
 var change=fun(){a.resize(0);return 4;};a[1][0]+=change();return true;}
)", debug, image, "index");
    }
    DestroyRuntime(runtime);
    std::printf("vector arrays: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
