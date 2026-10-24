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
static constexpr float pi = 3.14159265358979323846f;
static constexpr float tau = 6.28318530717958647692f;

static void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok && ++failures <= 24) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}

static std::string Label(const char* name, float a, float b = 0, float c = 0) {
    char values[160];
    std::snprintf(values, sizeof(values), "%s(%.9g,%.9g,%.9g)", name, a, b, c);
    return values;
}

// The mathematical period is the language's binary32 tau, not double pi.
// remainder independently reduces to the nearest multiple rather than using
// the implementation's signed fmod correction.
static double Angle(double value) {
    double result = std::remainder(value, static_cast<double>(tau));
    return result <= -static_cast<double>(pi) ? result + tau : result;
}

static float AngleFloat(double value) {
    float result = static_cast<float>(Angle(value));
    return result <= -pi ? pi : result;
}

static bool Near(float actual, double expected) {
    return std::isfinite(actual) && std::fabs(static_cast<double>(actual) - expected) <=
        std::max(2e-6, std::fabs(expected) * 2e-6);
}

static float Scalar(IRuntime* runtime, InstanceHandle instance, const char* name,
    std::initializer_list<float> args) {
    auto call = runtime->Call(instance, name);
    for (float value : args) call.argFloat(value);
    const auto status = call.invoke();
    Check(status == RunStatus::Completed && call.retType() == ValueType::Float,
        std::string("invoke ") + name + ": " + call.error().message);
    return status == RunStatus::Completed ? call.retFloat() : std::numeric_limits<float>::quiet_NaN();
}

static void CheckAngle(float actual, float expected, const std::string& label) {
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
    const float maximum = std::numeric_limits<float>::max();
    const float starts[] = {-maximum, -3e38f, -1e20f, -1e8f, -1000, -tau,
        -pi, std::nextafter(-pi, 0.0f), -1, 0, 1, std::nextafter(pi, 0.0f), pi,
        std::nextafter(pi, maximum), tau, 1000, 1e8f, 1e20f, 3e38f, maximum};
    const float targets[] = {-3e38f, -1e8f, -pi, -1, -std::numeric_limits<float>::denorm_min(),
        0, std::numeric_limits<float>::denorm_min(), 1, pi, 1e8f, 3e38f};
    for (float from : starts) for (float to : targets) {
        const double start = Angle(from), finish = Angle(to);
        const double delta = Angle(finish - start);
        CheckAngle(Scalar(runtime, instance, "Delta", {from, to}), AngleFloat(delta), Label("Delta", from, to));
        for (float t : {-0.5f, 0.0f, 0.25f, 0.5f, 1.0f, 1.5f}) {
            const float expected = t == 0 ? AngleFloat(from) : t == 1 ? AngleFloat(to) : AngleFloat(start + delta * t);
            const float actual = Scalar(runtime, instance, "Lerp", {from, to, t});
            const std::string label = Label("Lerp", from, to, t);
            CheckAngle(actual, expected, label);
            if (t == 0 || t == 1) Check(actual == expected, label + " exact endpoint");
        }
        for (float step : {0.0f, 0.125f, 4.0f, maximum}) {
            const float expected = std::fabs(delta) <= step ? AngleFloat(to) :
                AngleFloat(start + std::copysign(static_cast<double>(step), delta));
            const float actual = Scalar(runtime, instance, "Move", {from, to, step});
            CheckAngle(actual, expected, Label("Move", from, to, step));
            if (step >= pi) Check(actual == AngleFloat(to), "Move reaches exact wrapped target");
        }
    }
    // Reducing the interpolation product before addition preserves the start
    // even when the finite interpolation parameter is enormous.
    for (float t : {-maximum, -1e20f, 1e20f, maximum})
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

int main() {
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    const std::string source = "import math;"
        "export fun Normalize(var v){return math.Normalize3(v,math.Vector3(7,8,9));}"
        "export fun Delta(var a,var b){return math.DeltaAngle(a,b);}"
        "export fun Lerp(var a,var b,var t){return math.LerpAngle(a,b,t);}"
        "export fun Move(var a,var b,var step){return math.MoveTowardAngle(a,b,step);}"
        "export fun Ping(var x,var length){return math.PingPong(x,length);}";
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
            runtime->DestroyInstance(instance);
        }
        runtime->DestroyProgram(program);
    }
    DestroyRuntime(runtime);
    std::printf("math_boundary_regression: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
