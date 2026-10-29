#include "NeoScript.h"
#include "NeoVMInternal.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label.c_str()); }
}
static bool Host(CallContext& call, StringView name)
{
    ArrayView a;
    if (!call.argAsArray(0, a)) return false;
    bool ok = a.count() == 3 && !a.ints() && !a.floats() && !a.boolBits() && a.vectorDimension() == 0;
    if (name.str() == "Ints")
    {
        ok = ok && a.elementType() == ArrayElementType::Int32 && a.ints32() && !a.floats32();
        if (ok) { ok = a.ints32()[0] == -7 && a.ints32()[2] == -7; a.ints32()[1] = INT32_MAX; }
    }
    else if (name.str() == "Floats")
    {
        ok = ok && a.elementType() == ArrayElementType::Float32 && a.floats32() && !a.ints32();
        if (ok) { ok = a.floats32()[0] == 1.25f && a.floats32()[2] == 1.25f; a.floats32()[1] = 2.5f; }
    }
    else return false;
    call.retBool(ok);
    return true;
}
static void Run(IRuntime* rt, bool debug, bool image, const std::string& body,
    const char* failure = nullptr, const std::string& preserved = "true")
{
    rt->CollectCycles(true);
    AllocStats before; GetAllocStats(before);
    CompileDesc desc;
    const std::string source = "import system;import math;export var saved;export fun Probe(){" + body +
        "}export fun Recovery(){return " + preserved + ";}";
    desc.source = source;
    desc.sourceName = "array32.ns"; desc.includeDebugInfo = debug;
    ProgramHandle program; Error error;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = rt->CompileToBytecode(desc, bytes);
        if (error.ok()) program = rt->LoadProgram(bytes, &error);
    }
    else { auto result = rt->Compile(desc); program = result.program; error = result.error; }
    Check(bool(program), "compile " + body + " " + error.message);
    if (!program) return;
    auto instance = rt->CreateInstance(program);
    Check(bool(instance), "instance");
    if (instance)
    {
        {
            auto call = rt->Call(instance, "Probe");
            const auto status = call.invoke();
            Check(failure ? status == RunStatus::Failed && call.error().message.find(failure) != std::string::npos :
                status == RunStatus::Completed && call.retBool(), "execute " + body + " " + call.error().message);
        }
        {
            auto call = rt->Call(instance, "Recovery");
            Check(call.invoke() == RunStatus::Completed && call.retBool(), "failure preserves data: " + preserved);
        }
        rt->DestroyInstance(instance);
    }
    rt->DestroyProgram(program); rt->CollectCycles(true);
    AllocStats after; GetAllocStats(after);
    Check(after.arrays == before.arrays && after.bufferBytes == before.bufferBytes, "array storage released");
}
static void Float32Boundaries()
{
    const double maximum = (std::numeric_limits<float>::max)();
    const double midpoint = std::ldexp(1.0, 128) - std::ldexp(1.0, 103);
    const double below = std::nextafter(midpoint, 0.0);
    const double above = std::nextafter(midpoint, std::numeric_limits<double>::infinity());
    for (double sign : {1.0, -1.0})
    {
        for (double value : {maximum, std::nextafter(maximum, midpoint), 3.4028235e38, below})
            Check(ArrayInfo::NarrowFloat32(sign * value) == sign * maximum, "float32 upper finite rounding interval");
        for (double value : {midpoint, above, 1e300, std::numeric_limits<double>::infinity()})
        {
            const float result = ArrayInfo::NarrowFloat32(sign * value);
            Check(std::isinf(result) && std::signbit(result) == (sign < 0), "float32 midpoint overflows with sign");
        }
    }
    for (double value : {0.0, -0.0})
        Check(ArrayInfo::NarrowFloat32(value) == 0 && std::signbit(ArrayInfo::NarrowFloat32(value)) == std::signbit(value), "float32 signed zero preserved");
    Check(std::isnan(ArrayInfo::NarrowFloat32(std::numeric_limits<double>::quiet_NaN())), "float32 NaN preserved");
}
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
// 컴파일러가 상수 변환을 접거나 모드 변경 밖으로 이동하지 못하게 입력을 런타임에 전달한다.
NEOS_NOINLINE static float NarrowInCurrentMode(double value)
{
    return NeoNarrowFloat32(value);
}
struct ScopedMxcsr
{
    unsigned saved;
    ScopedMxcsr() : saved(_mm_getcsr()) {}
    ~ScopedMxcsr() { _mm_setcsr(saved); }
};
#endif
static void Float32Environment()
{
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    const unsigned original = _mm_getcsr();
    {
        ScopedMxcsr restore;
        _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
        Float32Boundaries();
        volatile double nearLimit = 3.4028235e38;
        volatile double huge = 1e300;
        const float maximum = (std::numeric_limits<float>::max)();
        Check(NarrowInCurrentMode(nearLimit) == maximum && std::isinf(NarrowInCurrentMode(huge)),
            "nearest rounding follows documented overflow boundary");
        // 다른 모드는 지원 계약 밖이다. 하드웨어 변환이 모드를 강제로 덮어쓰지 않음을 기록한다.
        for (unsigned mode : {unsigned(_MM_ROUND_UP), unsigned(_MM_ROUND_DOWN), unsigned(_MM_ROUND_TOWARD_ZERO)}) {
            _MM_SET_ROUNDING_MODE(mode);
            const float nearResult = NarrowInCurrentMode(nearLimit);
            const float hugeResult = NarrowInCurrentMode(huge);
            Check(mode == _MM_ROUND_UP ? std::isinf(nearResult) : nearResult == maximum,
                "hardware narrowing follows host rounding near FLT_MAX");
            Check(mode == _MM_ROUND_UP ? std::isinf(hugeResult) : hugeResult == maximum,
                "hardware narrowing follows host rounding on overflow");
            Check(_MM_GET_ROUNDING_MODE() == mode, "narrowing does not change host rounding mode");
        }
    }
    Check(_mm_getcsr() == original, "rounding test restores complete host MXCSR");
#else
    Float32Boundaries();
#endif
}
static void VectorNarrowingCases(IRuntime* rt, bool debug, bool image)
{
    // 생성 명령·성분 쓰기·산술이 배열과 같은 축소 경계를 쓰는지 확인한다.
    for (const char* name : {"Vector2", "Vector3", "Vector4", "Quaternion"})
    {
        const int dim = std::string(name) == "Vector2" ? 2 : std::string(name) == "Vector3" ? 3 : 4;
        const std::string ctor = std::string("math.") + name;
        std::string arguments = "x", ones = "1.0";
        for (int i = 1; i < dim; ++i) { arguments += ",x"; ones += ",1.0"; }
        const std::string body = std::string(R"NS(
var same=fun(var a,var b){if(a!=a)return b!=b;if(a==0.0 && b==0.0)return 1.0/a==1.0/b;return a==b;};
var z=0.0;
var values=[0.0,-0.0,1.0,-1.0,16777217.0,3.4028234663852886e38,3.4028235e38,
 3.4028235677973362e38,3.4028235677973366e38,3.402823567797337e38,
 -3.4028235e38,-3.4028235677973366e38,1e300,-1e300,1.0/z,-1.0/z,z/z,
 1.401298464324817e-45,1e-300,-1e-300];
foreach(var x in values){
 var expected=system.array32(x,1)[0];
)NS") + "var direct=" + ctor + "(" + arguments + ");var unit=" + ctor + "(" + ones + ");" + R"NS(
 var multiplied=unit*x;var divided=unit/x;var inv=system.array32(1.0/expected,1)[0];
 var list=[unit];var map={"v":unit};var array=system.array(unit,1);
)NS" + "for(var i in 0," + std::to_string(dim) + "){" + R"NS(
 if(!same(direct[i],expected))return false;
 if(!same(multiplied[i],expected) || !same(divided[i],inv))return false;
 unit[i]=x;list[0][i]=x;map.v[i]=x;array[0][i]=x;
 if(!same(unit[i],expected) || !same(list[0][i],expected) || !same(map.v[i],expected) || !same(array[0][i],expected))return false;
 unit[i]=1.0;unit[i]*=x;if(!same(unit[i],expected))return false;
 unit[i]=1.0;unit[i]/=x;if(!same(unit[i],inv))return false;
 unit[i]=0.0;unit[i]+=x;if(!same(unit[i],system.array32(0.0+expected,1)[0]))return false;
 unit[i]=0.0;unit[i]-=x;if(!same(unit[i],system.array32(0.0-expected,1)[0]))return false;
 if(!same(direct[i],expected))return false;
 }}return true;
)NS";
        Run(rt, debug, image, body);
    }
}
int main()
{
    Float32Environment();
    auto rt = CreateRuntime(); NativeObjectDesc host; host.name = "Host"; host.method = Host;
    Check(rt->RegisterObject(host), "register host"); rt->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true})
    {
        VectorNarrowingCases(rt, debug, image);
        const char* cases[] = {
            "var a=system.array32(-7,3);return Host.Ints(a) && a[0]==-7 && a[1]==2147483647 && a[2]==-7;",
            "var a=system.array32(1.25,3);return Host.Floats(a) && a[0]==1.25 && a[1]==2.5 && a[2]==1.25;",
            "var a=system.array32(-2147483648,2);a[1]=2147483647;return a[0]==-2147483648 && a[1]+1==2147483648;",
            "var a=system.array32(16777217.0,2);return a[0]==16777216.0 && a[0]+1.0==16777217.0;",
            "var a=system.array32(0.0,1);a[0]=16777217;return a[0]==16777216.0;",
            "var a=system.array32(0,1);a[0]=5;a[0]+=3;a[0]*=2;a[0]-=1;a[0]/=3;a[0]%=4;return a[0]++==1 && ++a[0]==3 && a[0]--==3 && --a[0]==1;",
            "var a=system.array32(0.0,1);a[0]=16777216.0;a[0]+=1.0;return a[0]==16777216.0;",
            "var a=system.array32(1.25,1);a[0]*=2;a[0]/=2;a[0]-=0.25;return a[0]++==1.0 && --a[0]==1.0;",
            "var a=system.array32(2,3);var b=a;a.reserve(50);a.fill(7);var c=a.clone();c[0]=9;c.resize(5);return b[0]==7 && c[0]==9 && c[3]==2 && c.len()==5 && a.len()==3;",
            "var a=system.array32(0.1,3);var initial=a[0];a.fill(0.25);var b=a.clone();b.clear();b.resize(2);return b[0]==initial && b[1]==initial && a[0]==0.25;",
            "var a=system.array32(1,0);a.append(2);a.append(3);a.resize(5);var sum=0;foreach(var v in a){a.reserve(100);sum+=v;}return sum==8 && a.find(3)==1 && a.contains(3.0) && !a.contains(3.5);",
            "var a=system.array32(0.5,0);a.append(1);a.resize(3);var sum=0.0;foreach(var v in a)sum+=v;return sum==2.0 && a.find(0.5)==1 && a.contains(1);",
            "var a=system.array32(0.1,2);return a.contains(a[0]) && !a.contains(0.1);",
            "var a=system.array32(-0.0,1);var c=a.clone();c.resize(2);return 1.0/c[0]<0 && 1.0/c[1]<0;",
            "var a=system.array32(1e300,2);a[1]=-1e300;return a[0]==1.0/0.0 && a[1]==-1.0/0.0;",
            "var a=system.array32(0.0/0.0,1);return a[0]!=a[0] && !a.contains(a[0]);",
            "var a=system.array32(0.0,2);a[0]=1.401298464324817e-45;a[1]=1e-300;return a[0]>0.0 && a[1]==0.0;",
            "var a=system.array32(3,1);var m={\"a\":a};m.a[0]+=4;var l=[a];l[0][0]++;return a[0]==8;",
            "var a=system.array(0,1);a[0]=2147483648;var b=system.array(0.0,1);b[0]=16777217.0;return a[0]==2147483648 && b[0]==16777217.0;",
            "var a=system.array32(0,0);a.reserve(0);a.fill(2);a.resize(-1);return type(a)==\"array\" && tosize(a)==0;"
        };
        for (const char* body : cases) Run(rt, debug, image, body);
        Run(rt, debug, image, R"ns(
            var maximum=3.4028234663852886e38;var infinity=1.0/0.0;
            var values=[maximum,tofloat("3.4028235e38"),3.4028235677973362e38,3.4028235677973366e38,3.402823567797337e38,1e300];
            var expected=[maximum,maximum,maximum,infinity,infinity,infinity];
            foreach(var sign in [1.0,-1.0])for(var i in 0,values.len()){
                var value=sign*values[i];var want=sign*expected[i];
                var a=system.array32(value,1);if(a[0]!=want)return false;
                a[0]=a[0];if(a[0]!=want)return false;
                a.resize(3);var copy=a.clone();if(copy[2]!=want)return false;
                a[0]=0.0;a[0]+=value;if(a[0]!=want)return false;
                a.fill(value);a.append(value);if(a[1]!=want || a[3]!=want)return false;
            }
            var a=system.array32(maximum,1);
            a[0]=tofloat(math.tostr(a[0]));if(a[0]!=maximum)return false;
            a[0]=tofloat("3.4028235e38");if(a[0]!=maximum)return false;
            a[0]=tofloat("-3.4028235e38");return a[0]==-maximum;
        )ns");
        for (const char* value : {"true", "null", "\"x\"", "[]", "{}", "math.Vector2(0,0)", "math.Vector3(0,0,0)", "math.Vector4(0,0,0,0)"})
            Run(rt, debug, image, std::string("system.array32(") + value + ",0);return true;", "array32 initial");
        for (const char* value : {"2147483648", "-2147483649", "9223372036854775807"})
        {
            Run(rt, debug, image, std::string("system.array32(") + value + ",0);return true;", "out of range");
            for (const char* action : {"saved[0]=", "saved.append(", "saved.fill("})
                Run(rt, debug, image, std::string("saved=system.array32(7,2);") + action + value +
                    (action[5] == '[' ? ";" : ");") + "return true;", "out of range", "saved.len()==2 && saved[0]==7 && saved[1]==7");
        }
        for (const char* operation : {"saved[0]+=1", "saved[0]++", "++saved[0]", "saved[0]*=2"})
            Run(rt, debug, image, std::string("saved=system.array32(2147483647,1);") + operation + ";return true;", "out of range", "saved[0]==2147483647");
        Run(rt, debug, image, "saved=system.array32(-2147483648,1);saved[0]--;return true;", "out of range", "saved[0]==-2147483648");
        for (const char* value : {"1.0", "true", "null", "\"x\""})
            Run(rt, debug, image, std::string("saved=system.array32(7,1);saved[0]=") + value + ";return true;", "cannot assign", "saved[0]==7");
        Run(rt, debug, image, "var a=system.array32(1,1);foreach(var v in a)a.append(2);return true;", "modified during foreach");
        for (const char* count : {"-1", "2147483648", "1.0", "true"})
            Run(rt, debug, image, std::string("system.array32(0,") + count + ");return true;", "call");
    }
    DestroyRuntime(rt);
    std::printf("array32_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
