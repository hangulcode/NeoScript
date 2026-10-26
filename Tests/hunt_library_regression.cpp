#include "NeoScript.h"
#include <cmath>
#include <codecvt>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <limits>
#include <locale>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok && ++failures <= 30) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}
static ProgramHandle Compile(IRuntime* runtime, const std::string& source, bool debug, bool image) {
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "hunt-library.ns";
    desc.includeDebugInfo = debug;
    Error error;
    ProgramHandle program;
    if (image) {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) program = runtime->LoadProgram(bytes, &error);
    } else {
        auto result = runtime->Compile(desc);
        error = result.error;
        program = result.program;
    }
    Check(bool(program), "compile: " + error.message + " " + source);
    return program;
}
static void Recovery(IRuntime* runtime, InstanceHandle instance) {
    auto call = runtime->Call(instance, "Recovery");
    Check(call.invoke() == RunStatus::Completed && call.retInt() == 23, "worker recovery");
}
static void Script(IRuntime* runtime, const std::string& source, bool debug, bool image,
    const char* expectedError = nullptr) {
    auto program = Compile(runtime, source + "export fun Recovery(){return 23;}", debug, image);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "create script instance");
    if (instance) {
        {
            auto call = runtime->Call(instance, "Probe");
            const auto status = call.invoke();
            Check(expectedError ? status == RunStatus::Failed &&
                call.error().message.find(expectedError) != std::string::npos :
                status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                "script: " + source + " " + call.error().message);
        }
        if (source.find("export fun Verify()") != std::string::npos) {
            auto call = runtime->Call(instance, "Verify");
            Check(call.invoke() == RunStatus::Completed && call.retBool(), "state after rejection: " + source);
        }
        Recovery(runtime, instance);
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

struct NumericFunction
{
    const char* name;
    int arity;
    bool integer;

    NumericFunction(const char* functionName, int argumentCount, bool integerOnly = false)
        : name(functionName), arity(argumentCount), integer(integerOnly) {}
};
static void NumericContracts(IRuntime* runtime, bool debug, bool image) {
    const NumericFunction functions[] = {
        {"abs",1},{"acos",1},{"asin",1},{"atan",1},{"ceil",1},{"floor",1},{"round",1},
        {"sin",1},{"cos",1},{"tan",1},{"log",1},{"log10",1},{"exp",1},{"pow",2},
        {"deg",1},{"rad",1},{"sqrt",1},{"atan2",2},{"hypot",2},{"fmod",2},{"trunc",1},
        {"fract",1},{"sinh",1},{"cosh",1},{"tanh",1},{"Sign",1},{"Min",2},{"Max",2},
        {"Vector2",2},{"Vector3",3},{"Vector4",4},{"Quaternion",4},{"Clamp01",1},
        {"Clamp",3},{"SmoothStep01",1},{"Lerp",3},{"Repeat",2},{"PingPong",2},
        {"WrapAngle",1},{"DeltaAngle",2},{"LerpAngle",3},{"MoveToward",3},{"MoveTowardAngle",3},
        {"InverseLerp",3},{"Remap",5},{"SmoothStep",3},{"Distance2",4},{"srand",1,true},
        {"tostr",1},{"RandRange",2},{"Hash32",1,true},{"ColorRGB",3},{"ColorARGB",4}
    };
    for (const auto& fn : functions) {
        std::string source = "import math;export fun Probe(var pos,var kind){var values=[";
        for (int i = 0; i < fn.arity; ++i) source += i ? ",1" : "1";
        source += "];var bad=[null,true,\"bad\",[],{},math.Vector2(0,0),fun(){},1.5];"
            "if(kind>=0)values[pos]=bad[kind];"
            "if(kind==-2){for(var i in 0,values.len()){values[i]=1.0;}}var result=math.";
        source += fn.name;
        source += "(";
        for (int i = 0; i < fn.arity; ++i)
            source += (i ? "," : "") + std::string("values[") + std::to_string(i) + "]";
        source += ");return type(result)!=\"impossible\";}export fun Recovery(){return 23;}";
        auto program = Compile(runtime, source, debug, image);
        if (!program) continue;
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), std::string("numeric instance ") + fn.name);
        if (instance) {
            for (int position = 0; position < fn.arity; ++position)
                for (int kind = 0; kind < (fn.integer ? 8 : 7); ++kind) {
                    {
                        auto call = runtime->Call(instance, "Probe");
                        const auto status = call.argInt(position).argInt(kind).invoke();
                        Check(status == RunStatus::Failed && call.error().message.find("invalid function call") != std::string::npos,
                            std::string(fn.name) + " invalid argument " + std::to_string(position) + "/" + std::to_string(kind) + " " + call.error().message);
                    }
                    Recovery(runtime, instance);
                }
            for (int kind : {-1, -2}) {
                if (kind == -2 && fn.integer) continue;
                auto call = runtime->Call(instance, "Probe");
                Check(call.argInt(0).argInt(kind).invoke() == RunStatus::Completed && call.retBool(),
                    std::string(fn.name) + " accepts documented numeric type " + call.error().message);
            }
            runtime->DestroyInstance(instance);
        }
        runtime->DestroyProgram(program);
    }
    for (const char* bad : {"null", "true", "\"bad\"", "[]", "{}", "math.Vector2(0,0)", "fun(){}"}) {
        for (const char* name : {"Lerp3", "quat_slerp"}) {
            const std::string good = std::string(name) == "Lerp3" ? "math.Vector3(1,2,3)" : "math.Quaternion(1,0,0,0)";
            Script(runtime, "import math;export fun Probe(){return math." + std::string(name) + "(" + good + "," + good + "," + bad + ");}", debug, image, "invalid function call");
        }
        // NORESULT constructor calls must validate too.
        Script(runtime, "import math;export fun Probe(){math.Vector3(1," + std::string(bad) + ",3);return true;}", debug, image, "invalid function call");
    }
    for (const char* bad : {"null", "true", "\"3\"", "[]", "{}", "1.5"}) {
        const std::string arg = bad;
        Script(runtime, "var a=[1,2,3];export fun Probe(){a.resize(" + arg + ");}"
            "export fun Verify(){return a.len()==3 && a[0]==1 && a[1]==2 && a[2]==3;}", debug, image, "invalid function call");
        Script(runtime, "import system;var a=system.array(7,3);export fun Probe(){a.resize(" + arg + ");}"
            "export fun Verify(){return a.len()==3 && a[0]==7 && a[2]==7;}", debug, image, "invalid function call");
        Script(runtime, "var m={\"x\":7};export fun Probe(){m.reserve(" + arg + ");}"
            "export fun Verify(){return m.len()==1 && m.x==7;}", debug, image, "invalid function call");
        Script(runtime, "export fun Probe(){return \"hello\".sub(" + arg + ",2);}", debug, image, "invalid function call");
        Script(runtime, "export fun Probe(){return \"hello\".sub(1," + arg + ");}", debug, image, "invalid function call");
    }
    Script(runtime, "import math;import system;export fun Probe(){var a=[1,2,3];a.resize(2);"
        "var b=system.array(7,2);b.resize(3);var m={\"x\":1};m.reserve(8);"
        "return a.len()==2 && a[1]==2 && b.len()==3 && b[2]==7 && m.x==1 && \"hello\".sub(1,2)==\"el\""
        " && math.sqrt(4)==2.0 && math.ColorRGB(1.0,0.0,0.0)==-16776961;}", debug, image);
    // JSON intentionally reports its own null/error contract instead of throwing.
    Script(runtime, "import system;export fun Probe(){var a=system.jsonDecode(5);"
        "if(a!=null || system.jsonError()==\"\")return false;var b=system.jsonEncode(1,5);"
        "return b==null && system.jsonError()!=\"\";}", debug, image);
}

static std::tm LocalTime(int32_t timestamp) {
    const std::time_t time = static_cast<uint32_t>(timestamp);
    std::tm result{};
#ifdef _WIN32
    Check(localtime_s(&result, &time) == 0, "native local time");
#else
    Check(localtime_r(&time, &result) != nullptr, "native local time");
#endif
    return result;
}
static std::string NativeDate(const wchar_t* pattern, const std::tm& tm) {
    wchar_t buffer[512]{};
    const size_t size = std::wcsftime(buffer, 512, pattern, &tm);
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
#endif
    return convert.to_bytes(buffer, buffer + size);
}
static void DateContracts(IRuntime* runtime, bool debug, bool image) {
    auto program = Compile(runtime, "import system;export fun Date(var pattern,var time){return system.date(pattern,time);}"
        "export fun Recovery(){return 23;}", debug, image);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "date instance");
    if (instance) {
        auto expect = [&](const std::string& pattern, int32_t timestamp, const std::string& expected) {
            auto call = runtime->Call(instance, "Date");
            const auto result = call.argString(pattern).argInt(timestamp).invokeR();
            Check(result.ok() && result.type() == ValueType::String && result.asString().str() == expected,
                "date pattern " + pattern + " " + call.error().message);
        };
        for (int32_t timestamp : {0, 1700000000, -1, std::numeric_limits<int32_t>::min()}) {
            const auto tm = LocalTime(timestamp);
            expect("%Y-%m-%d %H:%M:%S", timestamp, NativeDate(L"%Y-%m-%d %H:%M:%S", tm));
            expect("%Y\xe5\xb9\xb4 %m\xe6\x9c\x88 \xf0\x9f\x8c\x9e", timestamp,
                NativeDate(L"%Y", tm) + "\xe5\xb9\xb4 " + NativeDate(L"%m", tm) + "\xe6\x9c\x88 \xf0\x9f\x8c\x9e");
            expect("%Z", timestamp, NativeDate(L"%Z", tm));
            expect("%A / %B", timestamp, NativeDate(L"%A / %B", tm));
            expect("", timestamp, "");
            expect("literal %%", timestamp, "literal %");
        }
        const auto tm = LocalTime(0);
        for (char code : std::string("aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%")) {
            const wchar_t native[] = {L'%', static_cast<wchar_t>(code), 0};
            expect(std::string("%") + code, 0, NativeDate(native, tm));
        }
#ifdef _WIN32
        expect("%#Y/%#m/%#d", 0, NativeDate(L"%#Y/%#m/%#d", tm));
#else
        expect("%EY/%Od", 0, NativeDate(L"%EY/%Od", tm));
#endif
        expect(std::string(79, 'a'), 0, std::string(79, 'a'));
        std::string unicode;
        for (int i = 0; i < 26; ++i) unicode += "\xed\x95\x9c";
        expect(unicode, 0, unicode);
        const std::vector<std::string> invalid = {"%", "abc%", "%Q", "%E", "%O", "%#", "%##Y", "%0Y",
            std::string(80, 'a'), unicode + "\xed\x95\x9c", std::string(76, 'a') + "%Y", std::string("abc\0def",7)};
        for (const auto& pattern : invalid) {
            {
                auto call = runtime->Call(instance, "Date");
                Check(call.argString(pattern).argInt(0).invoke() == RunStatus::Failed &&
                    call.error().message.find("system.date") != std::string::npos, "invalid/long date pattern " + pattern);
            }
            Recovery(runtime, instance);
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

static void FractContracts(IRuntime* runtime, bool debug, bool image) {
    auto program = Compile(runtime, "import math;export fun Fract(var x){return math.fract(x);}", debug, image);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "fract instance");
    if (instance) {
        const float max = std::numeric_limits<float>::max(), tiny = std::numeric_limits<float>::denorm_min();
        const float values[] = {-max,-1e20f,-10.5f,-2,-1.5f,-1,std::nextafter(-1.0f,0.0f),-0.5f,
            -1e-7f,-1e-8f,-tiny,-0.0f,0.0f,tiny,1e-8f,0.5f,std::nextafter(1.0f,0.0f),1,1.5f,2,10.5f,1e20f,max};
        for (float value : values) {
            auto call = runtime->Call(instance, "Fract");
            const auto result = call.argFloat(value).invokeR();
            double integral = 0;
            double fraction = std::modf(static_cast<double>(value), &integral);
            if (fraction < 0) fraction += 1;
            const double expected = fraction >= 1 ? std::nextafter(1.0,0.0) :
                std::fmin(fraction, std::nextafter(1.0,0.0));
            Check(result.ok() && result.type() == ValueType::Float && result.asFloat() >= 0 && result.asFloat() < 1,
                "fract finite range");
            Check(result.asFloat() == expected, "fract independent modf value");
        }
        for (float value : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
            auto call = runtime->Call(instance, "Fract");
            const auto result = call.argFloat(value).invokeR();
            Check(result.ok() && std::isnan(result.asFloat()), "fract nonfinite compatibility");
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

static void ColorContracts(IRuntime* runtime, bool debug, bool image) {
    auto program = Compile(runtime, "import math;"
        "export fun RGB(var r,var g,var b){return math.ColorRGB(r,g,b);}"
        "export fun ARGB(var a,var r,var g,var b){return math.ColorARGB(a,r,g,b);}", debug, image);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "color instance");
    if (instance) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        struct ChannelCase { const char* name; float value; uint32_t byte; };
        const ChannelCase channels[] = {
            {"NaN",nan,0},{"negative NaN",-nan,0},
            {"positive infinity",infinity,255},{"negative infinity",-infinity,0},
            {"negative",-1,0},{"negative zero",-0.0f,0},{"zero",0,0},
            {"subnormal",std::numeric_limits<float>::denorm_min(),0},
            {"quarter",0.25f,63},{"half",0.5f,127},{"three quarters",0.75f,191},
            {"below one",std::nextafter(1.0f,0.0f),254},{"one",1,255},{"above one",2,255}
        };
        auto verify = [&](bool alpha, const float* values, const uint32_t* bytes, const std::string& label) {
            auto call = runtime->Call(instance, alpha ? "ARGB" : "RGB");
            if (alpha) call.argFloat(values[0]);
            const auto status = call.argFloat(values[1]).argFloat(values[2]).argFloat(values[3]).invoke();
            // 기대값은 unsigned 비트 조합으로 만들어 구현의 signed 산술과 독립적으로 검증한다.
            const uint32_t expected = ((alpha ? bytes[0] : 255u) << 24) |
                (bytes[3] << 16) | (bytes[2] << 8) | bytes[1];
            Check(status == RunStatus::Completed && call.retType() == ValueType::Int &&
                static_cast<uint32_t>(call.retInt()) == expected,
                std::string(alpha ? "ARGB " : "RGB ") + label + " debug=" + std::to_string(debug) +
                " image=" + std::to_string(image) + " " + call.error().message);
        };
        for (bool alpha : {false,true}) for (const auto& channel : channels) {
            for (int index = alpha ? 0 : 1; index < 4; ++index) {
                float values[] = {1.0f,0.25f,0.5f,0.75f};
                uint32_t bytes[] = {255,63,127,191};
                values[index] = channel.value;
                bytes[index] = channel.byte;
                verify(alpha, values, bytes, std::string(channel.name) + " channel=" + std::to_string(index));
            }
            const float values[] = {channel.value,channel.value,channel.value,channel.value};
            const uint32_t bytes[] = {channel.byte,channel.byte,channel.byte,channel.byte};
            verify(alpha, values, bytes, std::string(channel.name) + " all channels");
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

static void CoroutineContracts(IRuntime* runtime, bool debug, bool image) {
    Script(runtime, "import coroutine;var after=0;export fun Probe(){var keep=\"local\";coroutine.close();"
        "after=1;return keep==\"local\";}export fun Verify(){return after==0;}", debug, image, "main context");
    Script(runtime, "import coroutine;export fun Probe(){var state={\"n\":0};"
        "var c=coroutine.create(fun(){state.n=1;coroutine.close();state.n=2;});coroutine.resume(c);"
        "var a=coroutine.close(c);var b=coroutine.close(c);return state.n==1 && a==c && b==c && coroutine.status(c)==\"dead\";}", debug, image);
    for (const char* pause : {"yield;", "sleep(1);", "coroutine.resume(coroutine.create(fun(){}));", "coroutine.close();"}) {
        const std::string prefix = "import coroutine;";
        for (bool inCoroutine : {false,true}) {
            std::string source = "import system;import coroutine;var after=0;export fun Probe(){var m=system.load(\"" +
                prefix + pause + "\",\"sync\");";
            if (inCoroutine) source += "var c=coroutine.create(fun(){system.pcall(m);after=1;});coroutine.resume(c);";
            else source += "system.pcall(m);after=1;";
            source += "return true;}export fun Verify(){return after==0;}";
            Script(runtime, source, debug, image, "not allowed");
        }
    }
    Script(runtime, "import system;export fun Probe(){var m=system.load(\"var x=7; x+=3;\",\"sync\");"
        "return system.pcall(m)==null;}", debug, image);
    Script(runtime, "import system;export fun Probe(){var m=system.load(\"var z=0; var x=1 / z;\",\"sync\");"
        "system.pcall(m);return true;}", debug, image, "integer division by zero");
}
int main() {
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    for (bool debug : {false,true}) for (bool image : {false,true}) {
        NumericContracts(runtime, debug, image);
        DateContracts(runtime, debug, image);
        FractContracts(runtime, debug, image);
        ColorContracts(runtime, debug, image);
        CoroutineContracts(runtime, debug, image);
    }
    DestroyRuntime(runtime);
    std::printf("hunt_library_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
