#include "NeoScript.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

using namespace NeoScript;

// 워밍업이 끝난 호출 구간만 집계해 인자 벡터의 반복 할당을 실제로 검출한다.
static thread_local bool countAllocations = false;
static thread_local std::size_t allocationCount = 0;

void* operator new(std::size_t bytes)
{
    void* result = std::malloc(bytes ? bytes : 1);
    if (!result) throw std::bad_alloc();
    if (countAllocations) ++allocationCount;
    return result;
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 40) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}
static bool IsName(StringView value, const char* name)
{
    const std::size_t size = std::strlen(name);
    return value.size() == size && std::memcmp(value.data(), name, size) == 0;
}

struct State
{
    ObjectType object;
    FunctionHandle sum, zero, hop1, hop4, hop8, measure, preparedFunction, closure;
    std::unique_ptr<Invocation> prepared;
    std::string payload = "payload-abcdefghijklmnopqrstuvwxyz-0123456789";
    int token = 37;
    int marker = 0;
    int callbacks = 0;
    bool argumentsIntact = true;
};

static bool ObjectMethod(CallContext& context, StringView name)
{
    if (!IsName(name, "Value")) return false;
    context.retInt(*static_cast<int*>(context.userData()));
    return true;
}

static bool StableArguments(CallContext& context, int depth)
{
    float vector[4];
    context.argVec(3, vector);
    return context.argCount() == 4 && context.argInt(0) == depth && context.argInt(1) == 11 &&
        IsName(context.argString(2), "anchor") && vector[0] == 1 && vector[1] == 2 && vector[2] == 3;
}

static bool HostMethod(CallContext& context, StringView name)
{
    auto& state = *static_cast<State*>(context.userData());
    if (IsName(name, "Measure"))
    {
        // 이 경로는 검사 문자열이나 동적 함수 조회 없이 중첩 인자 버퍼만 사용한다.
        const int depth = context.argInt(0), value = context.argInt(1);
        CallResult result;
        {
            auto child = context.runtime()->Call(context.instance(), state.measure);
            result = child.argInt(depth - 1).argInt(value + 1).invokeR();
        }
        state.argumentsIntact = state.argumentsIntact && context.argCount() == 3 &&
            context.argInt(0) == depth && context.argInt(1) == value && context.argInt(2) == 37 && result.ok();
        ++state.callbacks;
        context.retInt(result.asInt());
        return true;
    }
    if (IsName(name, "Nest"))
    {
        const int depth = context.argInt(0);
        const StringView borrowedText = context.argString(2);
        state.argumentsIntact = state.argumentsIntact && StableArguments(context, depth);
        {
            auto empty = context.runtime()->Call(context.instance(), state.zero);
            state.argumentsIntact = state.argumentsIntact && empty.invoke() == RunStatus::Completed && empty.retInt() == 7;
        }
        int result = 0;
        {
            FunctionHandle target = depth % 3 == 0 ? state.hop1 : (depth % 3 == 1 ? state.hop4 : state.hop8);
            auto child = context.runtime()->Call(context.instance(), target);
            child.argInt(depth - 1);
            if (depth % 3 != 0) child.argInt(11).argString("anchor").argVec3(1, 2, 3);
            if (depth % 3 == 2) child.argInt(21).argFloat(1.5f).argBool(true).argString(state.payload);
            const auto status = child.invoke();
            state.argumentsIntact = state.argumentsIntact && status == RunStatus::Completed &&
                StableArguments(context, depth) && IsName(borrowedText, "anchor");
            if (status != RunStatus::Completed)
            {
                context.fail(91, child.error().message);
                return false;
            }
            result = child.retInt();
        }
        state.argumentsIntact = state.argumentsIntact && StableArguments(context, depth) && IsName(borrowedText, "anchor");
        ++state.callbacks;
        context.retInt(result + depth);
        return true;
    }
    if (IsName(name, "Verify"))
    {
        float vector[4]; context.argVec(4, vector);
        context.retBool(context.argCount() == 6 && context.argInt(0) == state.marker &&
            context.argFloat(1) == 1.25f && context.argBool(2) && context.argString(3).str() == state.payload &&
            vector[0] == 7 && vector[1] == 8 && vector[2] == 9 &&
            context.argObjectUserData(5) == &state.token);
        return true;
    }
    if (IsName(name, "Save"))
    {
        state.closure = context.argFunction(0);
        context.retNull();
        return true;
    }
    if (IsName(name, "Arm"))
    {
        state.marker = context.argInt(0);
        state.prepared.reset(new Invocation(context.runtime()->Call(context.instance(), state.preparedFunction)));
        state.prepared->argInt(state.marker).argString(state.payload).argVec3(7, 8, 9).argObject(state.object, &state.token);
        context.retInt(state.marker);
        return true;
    }
    if (IsName(name, "Prepared"))
    {
        float vector[4]; context.argVec(2, vector);
        context.retBool(context.argCount() == 4 && context.argInt(0) == state.marker &&
            context.argString(1).str() == state.payload && vector[0] == 7 && vector[1] == 8 && vector[2] == 9 &&
            context.argObjectUserData(3) == &state.token);
        return true;
    }
    return false;
}

static const char* source = R"NS(
import math;
export fun Zero(){return 7;}
export fun Sum(var a,var b,var c,var d,var e,var f,var g,var h){return a+b+c+d+e+f+g+h;}
export fun Mixed(var n,var f,var b,var text,var v,var object){return Host.Verify(n,f,b,text,v,object);}
export fun Text(var text){return text;}
export fun Vector(var v){v[0]+=1;return v;}
export fun Prepare(var base){var captured=base;Host.Save(fun(var n,var text){return captured+n+text.len();});}
export fun Arm(var marker){return Host.Arm(marker);}
export fun Prepared(var n,var text,var v,var object){return Host.Prepared(n,text,v,object);}
export fun Hop1(var depth){if(depth==0)return 0;return Host.Nest(depth,11,"anchor",math.Vector3(1,2,3));}
export fun Hop4(var depth,var n,var text,var v){
 if(n!=11||text!="anchor"||v[0]!=1.0||v[1]!=2.0||v[2]!=3.0)return -1000;
 if(depth==0)return 0;return Host.Nest(depth,n,text,v);
}
export fun Hop8(var depth,var n,var text,var v,var other,var f,var b,var longText){
 if(n!=11||text!="anchor"||v[0]!=1.0||v[1]!=2.0||v[2]!=3.0||other!=21||f!=1.5||!b||longText.len()<32)return -2000;
 if(depth==0)return 0;return Host.Nest(depth,n,text,v);
}
export fun Measure(var depth,var value){if(depth==0)return value;return Host.Measure(depth,value,37);}
export fun Failure(var text,var v){var zero=0;return toint(v[0])/zero;}
export fun Delayed(var n,var text,var v,var object){
 var count=0;for(var i in 0,100)count++;
 return n+text.len()+toint(v[0])+object.Value()+count;
}
)NS";

static ProgramHandle Compile(IRuntime* runtime, bool debug, bool image)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "host-argument-buffer.ns";
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
    Check(bool(program), "compile debug=" + std::to_string(debug) + " image=" + std::to_string(image) + ": " + error.message);
    return program;
}

static CallResult Sum(IRuntime* runtime, InstanceHandle instance, FunctionHandle function, int first)
{
    return runtime->Call(instance, function).argInt(first).argInt(2).argInt(3).argInt(4)
        .argInt(5).argInt(6).argInt(7).argInt(8).invokeR();
}

static void AllocationChecks(IRuntime* runtime, InstanceHandle instance, State& state)
{
    for (int i = 0; i < 32; ++i)
    {
        Sum(runtime, instance, state.sum, i);
        runtime->Call(instance, state.measure).argInt(6).argInt(i).invokeR();
    }
    bool values = true;
    allocationCount = 0;
    countAllocations = true;
    for (int i = 0; i < 256; ++i)
    {
        const auto result = Sum(runtime, instance, state.sum, i);
        values = values && result.ok() && result.asInt() == i + 35;
    }
    countAllocations = false;
    const std::size_t topAllocations = allocationCount;
    Check(values, "warmed FunctionHandle integer arguments retain values");
    Check(topAllocations == 0, "warmed top calls allocate no argument storage: " + std::to_string(topAllocations));

    values = true;
    state.argumentsIntact = true;
    const int callbacksBefore = state.callbacks;
    allocationCount = 0;
    countAllocations = true;
    for (int i = 0; i < 128; ++i)
    {
        const auto result = runtime->Call(instance, state.measure).argInt(6).argInt(i).invokeR();
        values = values && result.ok() && result.asInt() == i + 6;
    }
    countAllocations = false;
    const std::size_t nestedAllocations = allocationCount;
    Check(values && state.argumentsIntact && state.callbacks - callbacksBefore == 128 * 6,
        "warmed nested integer arguments retain values");
    Check(nestedAllocations == 0, "warmed nested calls allocate no argument storage: " + std::to_string(nestedAllocations));
}

static bool SameObjects(const AllocStats& a, const AllocStats& b)
{
    return a.strings == b.strings && a.maps == b.maps && a.lists == b.lists && a.arrays == b.arrays &&
        a.vectors == b.vectors && a.closures == b.closures && a.bufferBytes == b.bufferBytes;
}

static void Run(bool debug, bool image)
{
    AllocStats before; GetAllocStats(before);
    State state;
    auto runtime = CreateRuntime();
    NativeObjectDesc host; host.name = "Host"; host.method = HostMethod; host.userData = &state;
    Check(runtime->RegisterObject(host), "register Host");
    NativeObjectDesc object; object.name = "Payload"; object.method = ObjectMethod; object.declareGlobal = false;
    Check(runtime->RegisterObject(object), "register argument object");
    state.object = runtime->GetObjectType("Payload");
    runtime->FreezeBindings();
    auto program = Compile(runtime, debug, image);
    if (!program) { DestroyRuntime(runtime); return; }
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "create instance");
    if (!instance) { runtime->DestroyProgram(program); DestroyRuntime(runtime); return; }
    state.sum = runtime->FindFunction(program, "Sum");
    state.zero = runtime->FindFunction(program, "Zero");
    state.hop1 = runtime->FindFunction(program, "Hop1");
    state.hop4 = runtime->FindFunction(program, "Hop4");
    state.hop8 = runtime->FindFunction(program, "Hop8");
    state.measure = runtime->FindFunction(program, "Measure");
    state.preparedFunction = runtime->FindFunction(program, "Prepared");
    const auto mixed = runtime->FindFunction(program, "Mixed");
    const auto text = runtime->FindFunction(program, "Text");
    const auto vector = runtime->FindFunction(program, "Vector");
    const auto failure = runtime->FindFunction(program, "Failure");
    const auto delayed = runtime->FindFunction(program, "Delayed");

    for (int repeat = 0; repeat < 24; ++repeat)
    {
        state.marker = repeat;
        const auto result = runtime->Call(instance, mixed).argInt(repeat).argFloat(1.25f).argBool(true)
            .argString(state.payload).argVec3(7, 8, 9).argObject(state.object, &state.token).invokeR();
        Check(result.ok() && result.asBool(), "repeated mixed argument types");
        const std::string embedded("a\0b\0c", 5);
        const auto stringResult = runtime->Call(instance, text).argString(embedded).invokeR();
        Check(stringResult.ok() && stringResult.asString().str() == embedded, "string bytes survive reused argument buffer");
        const auto vectorResult = runtime->Call(instance, vector).argVec3(7, 8, 9).invokeR();
        float value[4]; vectorResult.asVec(value);
        Check(vectorResult.ok() && value[0] == 8 && value[1] == 8 && value[2] == 9,
            "vector argument and owned result");
        const auto sum = Sum(runtime, instance, state.sum, repeat);
        Check(sum.ok() && sum.asInt() == repeat + 35 && stringResult.asString().str() == embedded,
            "smaller and larger arities do not corrupt owned results");
    }
    Check(runtime->Call(instance, "Prepare").argInt(100).invokeR().ok() && bool(state.closure), "capture function handle");
    for (int repeat = 0; repeat < 24; ++repeat)
    {
        const auto result = runtime->Call(instance, state.closure).argInt(repeat).argString(state.payload).invokeR();
        Check(result.ok() && result.asInt() == 100 + repeat + static_cast<int>(state.payload.size()),
            "closure arguments survive detached buffer reuse");
    }
    for (int depth : {1, 2, 3, 8, 24, 3, 16})
    {
        state.argumentsIntact = true;
        const int previousCallbacks = state.callbacks;
        const auto result = runtime->Call(instance, state.hop1).argInt(depth).invokeR();
        Check(result.ok() && result.asInt() == depth * (depth + 1) / 2 && state.argumentsIntact &&
            state.callbacks - previousCallbacks == depth, "nested alternating arities and borrowed parent arguments");
    }
    for (int repeat = 0; repeat < 8; ++repeat)
    {
        {
            auto parent = runtime->Call(instance, "Arm");
            Check(parent.argInt(70 + repeat).invoke() == RunStatus::Completed && parent.retInt() == 70 + repeat,
                "parent returns while prepared child owns nonempty arguments");
            Check(state.prepared && bool(*state.prepared), "escaped prepared child remains armed");
        }
        Check(!runtime->Call(instance, state.zero), "prepared child prevents another call overwriting its arguments");
        Check(!runtime->ResetInstance(instance), "prepared child rejects reset without changing arguments");
        Check(state.prepared->invoke() == RunStatus::Completed && state.prepared->retBool(),
            "escaped prepared child executes original argument values");
        state.prepared.reset();
        Check(Sum(runtime, instance, state.sum, repeat).asInt() == repeat + 35, "recovery after escaped child");
    }
    {
        Check(runtime->Call(instance, "Arm").argInt(91).invokeR().ok(), "prepare child for abandonment");
        state.prepared.reset();
        Check(runtime->Call(instance, state.zero).invokeR().asInt() == 7, "abandoned prepared child releases its arguments");
    }
    for (int repeat = 0; repeat < 12; ++repeat)
    {
        {
            auto abandoned = runtime->Call(instance, mixed);
            abandoned.argInt(1).argFloat(1.25f).argBool(true).argString(state.payload)
                .argVec3(7, 8, 9).argObject(state.object, &state.token);
        }
        {
            auto call = runtime->Call(instance, failure);
            Check(call.argString(state.payload).argVec3(7, 8, 9).invoke() == RunStatus::Failed &&
                call.error().message.find("integer division by zero") != std::string::npos,
                "failed call releases reference arguments");
        }
        {
            auto call = runtime->Call(instance, state.sum);
            Check(call.argString(state.payload).argVec3(7, 8, 9).invoke() == RunStatus::Failed,
                "argument count failure releases detached values");
        }
        Check(Sum(runtime, instance, state.sum, repeat).asInt() == repeat + 35, "fresh call after abandonment and errors");
    }
    {
        auto call = runtime->Call(instance, delayed);
        const auto status = call.argInt(5).argString(state.payload).argVec3(7, 8, 9)
            .argObject(state.object, &state.token).budget(2).invoke();
        Check(status == RunStatus::Suspended, "reference arguments suspend after transfer into frame");
        Check(runtime->Resume(instance) == RunStatus::Completed && call.retInt() ==
            5 + static_cast<int>(state.payload.size()) + 7 + state.token + 100,
            "resumed call retains all argument types");
    }
    {
        auto call = runtime->Call(instance, delayed);
        Check(call.argInt(5).argString(state.payload).argVec3(7, 8, 9)
            .argObject(state.object, &state.token).budget(2).invoke() == RunStatus::Suspended,
            "reference arguments suspended for cancel");
        Check(runtime->Cancel(instance), "cancel releases suspended arguments");
    }
    Check(runtime->ResetInstance(instance), "reset after all invocation owners release");
    Check(!runtime->Call(instance, state.closure), "reset invalidates old closure while preserving buffer reuse");
    state.closure = {};
    Check(Sum(runtime, instance, state.sum, 9).asInt() == 44, "named function handles survive reset");
    AllocationChecks(runtime, instance, state);
    Check(runtime->CanDestroyInstance(instance), "all invocation leases released");
    runtime->DestroyInstance(instance);
    runtime->DestroyProgram(program);
    runtime->CollectCycles(true);
    DestroyRuntime(runtime);
    AllocStats after; GetAllocStats(after);
    Check(SameObjects(before, after), "argument strings vectors containers and closures released");
}

int main()
{
    for (bool debug : {false, true}) for (bool image : {false, true}) Run(debug, image);
    std::printf("host_argument_buffer_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
