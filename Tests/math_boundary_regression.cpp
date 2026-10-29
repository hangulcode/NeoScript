#include "NeoScript.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

using namespace NeoScript;

static int checks = 0, failures = 0;
static constexpr double pi = 3.14159265358979323846;
static constexpr double tau = 6.28318530717958647692;

static void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok && ++failures <= 24) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}

static std::string Label(const char* name, double a, double b = 0, double c = 0) {
    char values[160];
    std::snprintf(values, sizeof(values), "%s(%.9g,%.9g,%.9g)", name, a, b, c);
    return values;
}

// 수학적 주기는 스칼라 double의 tau이며 독립적인 remainder로 검증한다.
// remainder independently reduces to the nearest multiple rather than using
// the implementation's signed fmod correction.
static double Angle(double value) {
    double result = std::remainder(value, static_cast<double>(tau));
    return result <= -static_cast<double>(pi) ? result + tau : result;
}

static double AngleFloat(double value) {
    double result = Angle(value);
    return result <= -pi ? pi : result;
}

static bool Near(float actual, double expected) {
    return std::isfinite(actual) && std::fabs(static_cast<double>(actual) - expected) <=
        std::max(2e-6, std::fabs(expected) * 2e-6);
}

static double Scalar(IRuntime* runtime, InstanceHandle instance, const char* name,
    std::initializer_list<double> args) {
    auto call = runtime->Call(instance, name);
    for (double value : args) call.argFloat(value);
    const auto status = call.invoke();
    Check(status == RunStatus::Completed && call.retType() == ValueType::Float,
        std::string("invoke ") + name + ": " + call.error().message);
    return status == RunStatus::Completed ? call.retFloat() : std::numeric_limits<float>::quiet_NaN();
}

static void CheckAngle(double actual, double expected, const std::string& label) {
    Check(std::isfinite(actual) && actual > -pi && actual <= pi, label + " range");
    // Equivalent values near the branch cut are compared by angular distance.
    Check(std::isfinite(actual) && std::fabs(Angle(static_cast<double>(actual) - expected)) <= 3e-6,
        label + " direction");
}

static void NormalizeCases(IRuntime* runtime, InstanceHandle instance) {
    const float tiny = std::numeric_limits<float>::denorm_min();
    const float maximum = std::numeric_limits<float>::max();
    const float magnitudes[] = {0, tiny, std::numeric_limits<float>::min(), 1e-20f, 1e-5f,
        std::nextafter(1e-4f, 0.0f), 1e-4f, std::nextafter(1e-4f, 1.0f), 0.001f,
        1, 3, 1e10f, 1e18f, 1e19f, 2e19f, 1e20f, 1e30f, maximum};
    std::vector<std::array<float, 3>> inputs;
    for (float magnitude : magnitudes) {
        for (float sign : {-1.0f, 1.0f}) for (int axis = 0; axis < 3; ++axis) {
            std::array<float, 3> value = {{0, 0, 0}};
            value[axis] = sign * magnitude;
            inputs.push_back(value);
        }
        inputs.push_back({{magnitude, -magnitude, magnitude}});
    }
    inputs.push_back({{maximum, 1, tiny}});
    inputs.push_back({{-maximum, maximum, tiny}});
    for (const auto& value : inputs) {
        auto call = runtime->Call(instance, "Normalize");
        call.argVec3(value[0], value[1], value[2]);
        const auto status = call.invoke();
        const std::string label = Label("Normalize", value[0], value[1], value[2]);
        Check(status == RunStatus::Completed && call.retType() == ValueType::Vec3, label + " invoke");
        float actual[4] = {};
        call.retVec(actual);
        // This threshold intentionally follows the documented float lengthSq
        // calculation, including its rounding at the fallback boundary.
        const float lengthSq = value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
        const bool fallback = lengthSq < 1e-8f;
        const double length = std::hypot(std::hypot(static_cast<double>(value[0]), value[1]), value[2]);
        for (int axis = 0; axis < 3; ++axis)
            Check(Near(actual[axis], fallback ? 7.0 + axis : value[axis] / length), label + " component");
        if (!fallback)
            Check(Near(static_cast<float>(std::hypot(std::hypot(static_cast<double>(actual[0]), actual[1]), actual[2])), 1.0),
                label + " unit length");
    }
}

static void AngleCases(IRuntime* runtime, InstanceHandle instance) {
    const double maximum = static_cast<double>(std::numeric_limits<float>::max());
    const double starts[] = {-maximum, -3e38, -1e20, -1e8, -1000, -tau,
        -pi, std::nextafter(-pi, 0.0), -1, 0, 1, std::nextafter(pi, 0.0), pi,
        std::nextafter(pi, maximum), tau, 1000, 1e8, 1e20, 3e38, maximum};
    const double targets[] = {-3e38, -1e8, -pi, -1, -std::numeric_limits<double>::denorm_min(),
        0, std::numeric_limits<double>::denorm_min(), 1, pi, 1e8, 3e38};
    for (double from : starts) for (double to : targets) {
        const double start = Angle(from), finish = Angle(to);
        const double delta = Angle(finish - start);
        CheckAngle(Scalar(runtime, instance, "Delta", {from, to}), AngleFloat(delta), Label("Delta", from, to));
        for (double t : {-0.5, 0.0, 0.25, 0.5, 1.0, 1.5}) {
            const double expected = t == 0 ? AngleFloat(from) : t == 1 ? AngleFloat(to) : AngleFloat(start + delta * t);
            const double actual = Scalar(runtime, instance, "Lerp", {from, to, t});
            const std::string label = Label("Lerp", from, to, t);
            CheckAngle(actual, expected, label);
            if (t == 0 || t == 1) Check(actual == expected, label + " exact endpoint");
        }
        for (double step : {0.0, 0.125, 4.0, maximum}) {
            const double expected = std::fabs(delta) <= step ? AngleFloat(to) :
                AngleFloat(start + std::copysign(static_cast<double>(step), delta));
            const double actual = Scalar(runtime, instance, "Move", {from, to, step});
            CheckAngle(actual, expected, Label("Move", from, to, step));
            if (step >= pi) Check(actual == AngleFloat(to), "Move reaches exact wrapped target");
        }
    }
    // Reducing the interpolation product before addition preserves the start
    // even when the finite interpolation parameter is enormous.
    for (double t : {-maximum, -1e20, 1e20, maximum})
        CheckAngle(Scalar(runtime, instance, "Lerp", {1, 3, t}),
            AngleFloat(1.0 + Angle(2.0 * t)), Label("Lerp huge parameter", 1, 3, t));
}

static void PingPongCases(IRuntime* runtime, InstanceHandle instance) {
    const float tiny = std::numeric_limits<float>::denorm_min();
    const float maximum = std::numeric_limits<float>::max();
    const float lengths[] = {tiny, std::numeric_limits<float>::min(), 1e-20f, 0.001f,
        1, 3, 1000, 1e8f, 1e20f, 1e38f, 3e38f, maximum};
    for (float length : lengths) {
        const double period = 2.0 * length;
        std::vector<float> positions = {0, tiny, 1, maximum, length * 0.25f, length * 0.5f,
            length, std::nextafter(length, 0.0f)};
        if (length < maximum) positions.push_back(std::nextafter(length, maximum));
        if (period <= maximum) {
            const float end = static_cast<float>(period);
            positions.push_back(std::nextafter(end, 0.0f));
            positions.push_back(end);
            if (end < maximum) positions.push_back(std::nextafter(end, maximum));
        }
        for (float positive : positions) for (float sign : {-1.0f, 1.0f}) {
            const float x = positive * sign;
            // Distance to the closest multiple of a full period is the
            // triangular wave; this avoids copying the production branch.
            const float expected = static_cast<float>(std::fabs(std::remainder(static_cast<double>(x), period)));
            const float actual = Scalar(runtime, instance, "Ping", {x, length});
            const std::string label = Label("Ping", x, length);
            Check(std::isfinite(actual) && actual >= 0 && actual <= length, label + " range");
            const double tolerance = std::max(static_cast<double>(tiny), std::fabs(static_cast<double>(expected)) * 2e-6);
            Check(std::isfinite(actual) && std::fabs(static_cast<double>(actual) - expected) <= tolerance, label + " value");
            if (std::fabs(x) <= length) Check(actual == std::fabs(x), label + " first segment");
            if (actual == 0) Check(!std::signbit(actual), label + " positive zero");
        }
    }
    // Nonpositive lengths remain outside the positive-length wave contract;
    // retain the previous result instead of introducing a new runtime error.
    Check(Scalar(runtime, instance, "Ping", {1, 0}) == 0, "Ping zero length compatibility");
    Check(Scalar(runtime, instance, "Ping", {1, -3}) == -6, "Ping negative length compatibility");
}

static void CompatibilityCases(IRuntime* runtime, InstanceHandle instance) {
    const float infinity = std::numeric_limits<float>::infinity();
    Check(std::isnan(Scalar(runtime, instance, "Delta", {infinity, 1})), "Delta nonfinite compatibility");
    Check(std::isnan(Scalar(runtime, instance, "Lerp", {1, infinity, 0})), "Lerp nonfinite target at zero");
    Check(std::isnan(Scalar(runtime, instance, "Lerp", {infinity, 1, 1})), "Lerp nonfinite start at one");
    Check(std::isnan(Scalar(runtime, instance, "Lerp", {1, 1, infinity})), "Lerp nonfinite parameter");
    Check(std::isnan(Scalar(runtime, instance, "Ping", {infinity, 3})), "Ping nonfinite position compatibility");
    Check(std::isnan(Scalar(runtime, instance, "Ping", {1, infinity})), "Ping nonfinite length compatibility");
    Check(Scalar(runtime, instance, "Move", {1, 2, -0.25f}) == 0.75f, "Move negative step compatibility");
    auto call = runtime->Call(instance, "Normalize");
    Check(call.argVec3(infinity, 0, 0).invoke() == RunStatus::Completed && call.retType() == ValueType::Vec3,
        "Normalize nonfinite input remains a value");
    float result[4] = {};
    call.retVec(result);
    Check(std::isnan(result[0]) && result[1] == 0 && result[2] == 0, "Normalize nonfinite compatibility");
}

static void ScalarMathCases(IRuntime* runtime, InstanceHandle instance) {
    const double infinity = std::numeric_limits<double>::infinity();
    const double values[] = {0.0, -0.0, 1.0, -1.0, 0.1, pi, -pi,
        std::numeric_limits<double>::denorm_min(), 1e20, -1e20, 1e300,
        infinity, -infinity, std::numeric_limits<double>::quiet_NaN()};
    for (const char* name : {"Sin", "Cos", "SinValue", "CosValue"}) {
        const bool sine = name[0] == 'S';
        for (double value : values) {
            const double expected = sine ? std::sin(value) : std::cos(value);
            const double actual = Scalar(runtime, instance, name, {value});
            Check(std::isnan(expected) ? std::isnan(actual) :
                actual == expected && std::signbit(actual) == std::signbit(expected),
                Label(name, value) + " scalar result and zero sign");
        }
        for (int64_t value : {int64_t(0), int64_t(-7), int64_t(9007199254740993LL)}) {
            auto call = runtime->Call(instance, name);
            const double expected = sine ? std::sin(static_cast<double>(value)) : std::cos(static_cast<double>(value));
            Check(call.argInt(value).invoke() == RunStatus::Completed &&
                call.retType() == ValueType::Float && call.retFloat() == expected,
                std::string(name) + " int64 argument");
        }
    }
    // 결과를 버리는 호출도 인자를 검사하며 다음 호출의 스택과 반환값을 오염시키지 않는다.
    auto call = runtime->Call(instance, "DiscardMath");
    Check(call.argFloat(0.0).invoke() == RunStatus::Completed && call.retBool(),
        "discarded scalar math result preserves later calls");
}

int main() {
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    const std::string source = "import math;"
        "export fun Normalize(var v){return math.Normalize3(v,math.Vector3(7,8,9));}"
        "export fun Delta(var a,var b){return math.DeltaAngle(a,b);}"
        "export fun Lerp(var a,var b,var t){return math.LerpAngle(a,b,t);}"
        "export fun Move(var a,var b,var step){return math.MoveTowardAngle(a,b,step);}"
        "export fun Ping(var x,var length){return math.PingPong(x,length);}"
        "export fun Sin(var x){var result=[1];result=math.sin(x);return result;}"
        "export fun Cos(var x){var result=[1];result=math.cos(x);return result;}"
        "export fun SinValue(var x){var f=Sin;return f(x);}"
        "export fun CosValue(var x){var f=Cos;return f(x);}"
        "export fun DiscardMath(var x){math.sin(x);math.cos(x);return math.sin(x)==0.0&&math.cos(x)==1.0;}";
    for (bool debug : {false, true}) for (bool image : {false, true}) {
        CompileDesc desc;
        desc.source = source;
        desc.sourceName = "math-boundary.ns";
        desc.includeDebugInfo = debug;
        ProgramHandle program;
        Error error;
        if (image) {
            std::vector<uint8_t> bytes;
            error = runtime->CompileToBytecode(desc, bytes);
            if (error.ok()) program = runtime->LoadProgram(bytes, &error);
        } else {
            auto compiled = runtime->Compile(desc);
            program = compiled.program;
            error = compiled.error;
        }
        Check(bool(program), "compile math boundary: " + error.message);
        if (!program) continue;
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), "create math boundary instance");
        if (instance) {
            NormalizeCases(runtime, instance);
            AngleCases(runtime, instance);
            PingPongCases(runtime, instance);
            CompatibilityCases(runtime, instance);
            ScalarMathCases(runtime, instance);
            runtime->DestroyInstance(instance);
        }
        runtime->DestroyProgram(program);
    }
    DestroyRuntime(runtime);
    std::printf("math_boundary_regression: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
