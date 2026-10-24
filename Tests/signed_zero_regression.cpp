#include "NeoScript.h"
#include "NeoVMProgram.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace NeoScript;

static int checks = 0, failures = 0;

static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 32) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}

static void FloatValue(float actual, float expected, const std::string& label)
{
    // Equality alone deliberately cannot distinguish the two zero signs.
    Check(actual == expected && std::signbit(actual) == std::signbit(expected), label);
}

enum class Result { List, Map, Vector, Array, VectorArray, Bool };

static void ReadResult(Invocation& call, Result result, const std::vector<float>& expected,
    const std::string& label)
{
    switch (result)
    {
    case Result::List:
    {
        ListReader list;
        const bool opened = call.retList(list);
        const bool valid = opened && list.count() == static_cast<int>(expected.size());
        Check(valid, label + " list result");
        if (!valid) return;
        for (size_t i = 0; i < expected.size(); ++i)
        {
            float actual = 1;
            Check(list.type(static_cast<int>(i)) == ValueType::Float && list.getFloat(static_cast<int>(i), actual),
                label + " float element " + std::to_string(i));
            FloatValue(actual, expected[i], label + " sign " + std::to_string(i));
        }
        break;
    }
    case Result::Map:
    {
        MapReader map;
        const bool opened = call.retMap(map);
        Check(opened, label + " map result");
        if (!opened) return;
        const char* keys[] = {"first", "second"};
        for (size_t i = 0; i < expected.size(); ++i)
        {
            float actual = 1;
            Check(map.type(keys[i]) == ValueType::Float && map.getFloat(keys[i], actual), label + " map float");
            FloatValue(actual, expected[i], label + " sign " + keys[i]);
        }
        break;
    }
    case Result::Vector:
    {
        Check(call.retType() == ValueType::Vec2, label + " vector result");
        float actual[4] = {};
        call.retVec(actual);
        for (size_t i = 0; i < expected.size(); ++i)
            FloatValue(actual[i], expected[i], label + " component " + std::to_string(i));
        break;
    }
    case Result::Array:
    {
        ArrayView array;
        const bool opened = call.retArray(array);
        const bool valid = opened && array.elementType() == ArrayElementType::Float &&
            array.count() == static_cast<int>(expected.size()) && array.floats();
        Check(valid, label + " array result");
        if (!valid) return;
        for (size_t i = 0; i < expected.size(); ++i)
            FloatValue(array.floats()[i], expected[i], label + " element " + std::to_string(i));
        break;
    }
    case Result::VectorArray:
    {
        ArrayView array;
        const bool opened = call.retArray(array);
        const bool valid = opened && array.elementType() == ArrayElementType::Vector2 &&
            array.count() * 2 == static_cast<int>(expected.size());
        Check(valid, label + " vector array result");
        if (!valid) return;
        for (int i = 0; i < array.count(); ++i)
        {
            float actual[4] = {};
            Check(array.getVec(i, actual), label + " vector array read");
            for (int j = 0; j < 2; ++j)
                FloatValue(actual[j], expected[static_cast<size_t>(i * 2 + j)],
                    label + " component " + std::to_string(i * 2 + j));
        }
        break;
    }
    case Result::Bool:
        Check(call.retType() == ValueType::Bool && call.retBool(), label + " equality and key compatibility");
        break;
    }
}

static void Run(IRuntime* runtime, const std::string& label, const std::string& source,
    Result result, const std::vector<float>& expected, bool debug, bool image,
    bool hostArguments = false, float first = 0, float second = 0)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "signed-zero.ns";
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
        program = compiled.program;
        error = compiled.error;
    }
    const std::string context = label + " debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(bool(program), context + " compile: " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), context + " instance");
    if (instance)
    {
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            auto call = runtime->Call(instance, "Probe");
            if (hostArguments) call.argFloat(first).argFloat(second);
            const auto status = call.timeout(1000).invoke();
            Check(status == RunStatus::Completed, context + " invoke: " + call.error().message);
            if (status == RunStatus::Completed) ReadResult(call, result, expected, context);
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

static void Cases(IRuntime* runtime, bool debug, bool image, bool negativeFirst)
{
    const std::string a = negativeFirst ? "-0.0" : "0.0";
    const std::string b = negativeFirst ? "0.0" : "-0.0";
    const float first = std::copysign(0.0f, negativeFirst ? -1.0f : 1.0f);
    const float second = std::copysign(0.0f, negativeFirst ? 1.0f : -1.0f);
    const std::vector<float> pair = {first, second};
    const std::string order = negativeFirst ? " negative first" : " positive first";
    const std::string imports = "import math;import system;";
    auto run = [&](const char* name, const std::string& source, Result result,
        const std::vector<float>& expected, bool arguments = false)
    {
        Run(runtime, name + order, imports + source, result, expected, debug, image, arguments, first, second);
    };

    // Compile both encounter orders independently: values must not depend on
    // whether an earlier literal in the same program had the opposite sign.
    run("list literal", "export fun Probe(){return [" + a + "," + b + "];}", Result::List, pair);
    run("map literal", "export fun Probe(){return {\"first\":" + a + ",\"second\":" + b + "};}", Result::Map, pair);
    run("global/local copies", "var a=" + a + ";var b=" + b + ";"
        "export fun Probe(){var x=a;var y;y=b;return [x,y];}", Result::List, pair);
    run("runtime list assignment", "export fun Probe(var a,var b){var x=a;var y=b;"
        "var values=[1.0,1.0];values[0]=x;values[1]=y;var alias=values;return alias;}", Result::List, pair, true);
    run("runtime map assignment", "export fun Probe(var a,var b){var values={};"
        "values.first=a;values.second=b;return values;}", Result::Map, pair, true);
    run("constants", "const A=" + a + ";const B=" + b + ";export fun Probe(){return [A,B];}", Result::List, pair);
    run("folded multiply", "const A=" + a + "*1.0;const B=" + b + "*1.0;"
        "export fun Probe(){return [A,B];}", Result::List, pair);
    run("folded divide", "const A=" + a + "/1.0;const B=" + b + "/1.0;"
        "export fun Probe(){return [A,B];}", Result::List, pair);
    run("literal expression multiply", "export fun Probe(){return [" + a + "*1.0," + b + "*1.0];}",
        Result::List, pair);
    run("literal expression divide", "export fun Probe(){return [" + a + "/1.0," + b + "/1.0];}",
        Result::List, pair);
    run("runtime arithmetic", "export fun Probe(var a,var b){return [a*1.0,b/1.0];}", Result::List, pair, true);
    const float infinity = std::numeric_limits<float>::infinity();
    run("reciprocal observation", "export fun Probe(){var values=[" + a + "," + b + "];"
        "return [1.0/values[0],1.0/values[1]];}", Result::List,
        {std::copysign(infinity, first), std::copysign(infinity, second)});

    run("vector literal", "export fun Probe(){return math.Vector2(" + a + "," + b + ");}", Result::Vector, pair);
    run("vector copy on write", "export fun Probe(){var value=math.Vector2(" + a + "," + b + ");"
        "var copy=value;copy[0]=copy[1];return [value[0],value[1],copy[0],copy[1]];}",
        Result::List, {first, second, second, second});
    run("float array assignment and growth", "export fun Probe(){var values=system.array(" + a + ",2);"
        "values[1]=" + b + ";values.resize(3);return values;}", Result::Array, {first, second, first});
    run("vector array copy and growth", "export fun Probe(){var values=system.array(math.Vector2(" + a + "," + b + "),2);"
        "var copy=values[0];copy[0]=copy[1];values[1]=copy;values.resize(3);return values;}",
        Result::VectorArray, {first, second, second, second, first, second});

    run("literal equality and map keys", "export fun Probe(){var values=[" + a + "," + b + "];"
        "var keys={" + a + ":1," + b + ":2};return values[0]==values[1] && !(values[0]!=values[1])"
        " && tosize(keys)==1 && keys[values[0]]==2 && keys[values[1]]==2;}", Result::Bool, {});
    // Host inputs guarantee genuinely opposite signs independently of literal
    // compilation. Numeric and vector key canonicalization must remain intact.
    run("runtime equality and key canonicalization", R"NS(
export fun Probe(var a,var b){
 var keys={};keys[a]=1;keys[b]=2;
 var scalar=tosize(keys)==1 && keys[a]==2 && keys[b]==2;
 keys[0]=3;scalar=scalar && tosize(keys)==2 && keys[a]==2;
 var vectors={};var va=math.Vector2(a,1.0);var vb=math.Vector2(b,1.0);
 vectors[va]=1;vectors[vb]=2;
 var values=system.set([a,b]);
 return a==b && !(a!=b) && va==vb && scalar && tosize(values)==1
  && tosize(vectors)==1 && vectors[va]==2 && vectors[vb]==2;
})NS", Result::Bool, {}, true);
}

static void RejectPreviousImage(IRuntime* runtime)
{
    CompileDesc desc;
    desc.source = "export fun Probe(){return [0.0,-0.0];}";
    desc.sourceName = "signed-zero-image.ns";
    std::vector<uint8_t> bytes;
    Error error = runtime->CompileToBytecode(desc, bytes);
    Check(error.ok() && bytes.size() >= sizeof(SNeoVMHeader), "compile image for previous-version rejection");
    if (!error.ok() || bytes.size() < sizeof(SNeoVMHeader)) return;
    SNeoVMHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    header._dwNeoVersion = ('0' << 24) | ('1' << 16) | ('3' << 8) | '1';
    std::memcpy(bytes.data(), &header, sizeof(header));
    auto old = runtime->LoadProgram(bytes, &error);
    Check(!old && error.message.find("version mismatch") != std::string::npos,
        "reject 0131 images whose constants may have lost the zero sign");
    if (old) runtime->DestroyProgram(old);
}

int main()
{
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true})
        for (bool negativeFirst : {false, true}) Cases(runtime, debug, image, negativeFirst);
    RejectPreviousImage(runtime);
    DestroyRuntime(runtime);
    std::printf("signed_zero_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
