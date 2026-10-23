#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label.c_str()); }
}
struct State {
    FunctionHandle saved;
    int callbacks = 0;
    int property = 123;
};
static bool Method(CallContext& ctx, StringView method) {
    auto& state = *static_cast<State*>(ctx.userData());
    if (method.str() == "Save") {
        state.saved = ctx.argFunction(0);
        ctx.retNull();
        return true;
    }
    if (method.str() != "Reenter") return false;
    auto callback = ctx.argFunction(0);
    const int depth = ctx.argInt(2);
    const auto text = ctx.argString(1);
    Check(text.str() == "anchor" && ctx.argCount() == 3, "native original arguments");
    int result = 0;
    {
        auto call = ctx.runtime()->Call(ctx.instance(), callback);
        if (call.invoke() != RunStatus::Completed) {
            ctx.fail(90, call.error().message);
            return false;
        }
        result = call.retInt();
        Check(ctx.argInt(2) == depth && ctx.argString(1).str() == "anchor" && text.str() == "anchor",
            "arguments survive while child Invocation is alive");
    }
    Check(ctx.argInt(2) == depth && ctx.argString(1).str() == "anchor" && text.str() == "anchor",
        "arguments survive destruction of child Invocation");
    ++state.callbacks;
    ctx.retInt(result + depth);
    return true;
}
static bool Property(CallContext& ctx, StringView, bool get) {
    auto& state = *static_cast<State*>(ctx.userData());
    const int before = get ? 0 : ctx.argInt(0);
    {
        auto call = ctx.runtime()->Call(ctx.instance(), "Child");
        Check(call.invoke() == RunStatus::Completed && call.retInt() == 7, "property child call");
        if (!get) Check(ctx.argInt(0) == before, "setter value while child is alive");
    }
    if (get) ctx.retInt(state.property);
    else {
        Check(ctx.argInt(0) == before, "setter value after child is destroyed");
        state.property = before;
    }
    return true;
}
static const char* source = R"NS(
import math;
export fun Child(){var x=8;var y=9;return x+y-10;}
export fun Prepare(){
    var n=10;var v=math.Vector3(1,2,3);
    Host.Save(fun(){n+=1;v[0]+=2;return n+toint(v[0]);});
}
export fun Work(var depth){
    if(depth==0)return 17;
    return Host.Reenter(fun(){return Work(depth-1);},"anchor",depth);
}
export fun Churn(){
    var a=[];for(var i in 0,1000)a.append(["changed"..i,math.Vector4(i,2,3,4)]);
    return a.len();
}
export fun Throw(){return Host.Reenter(fun(){var z=0;return 7 / z;},"anchor",3);}
export fun SetterLiteral(){Host.value=123;return Host.value;}
export fun SetterLocal(){var x=123;Host.value=x;return x+Host.value;}
export fun GetterLocal(){var x=11;var y=22;var z=Host.value;return x+y+z;}
)NS";
int main() {
    State state;
    auto runtime = CreateRuntime();
    NativeObjectDesc object;
    object.name = "Host";
    object.method = Method;
    object.property = Property;
    object.userData = &state;
    runtime->RegisterObject(object);
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true}) {
        CompileDesc desc;
        desc.source = source;
        desc.sourceName = "callback-reentry.ns";
        desc.includeDebugInfo = debug;
        ProgramHandle program;
        Error error;
        if (image) {
            std::vector<uint8_t> bytes;
            error = runtime->CompileToBytecode(desc, bytes);
            if (error.ok()) program = runtime->LoadProgram(bytes, &error);
        } else {
            auto result = runtime->Compile(desc);
            program = result.program;
            error = result.error;
        }
        Check(bool(program), "compile " + error.message);
        if (!program) continue;
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), "create instance");
        if (instance) {
            { auto call = runtime->Call(instance, "Prepare");
              Check(call.invoke() == RunStatus::Completed, "capture prepare"); }
            for (int i = 1; i <= 50; ++i) {
                { auto call = runtime->Call(instance, "Churn");
                  Check(call.invoke() == RunStatus::Completed && call.retInt() == 1000, "temporary value churn"); }
                { auto call = runtime->Call(instance, state.saved);
                  Check(call.invoke() == RunStatus::Completed && call.retInt() == 11 + 3 * i, "retained closure state"); }
            }
            for (int depth : {0, 1, 2, 8, 24}) {
                const int before = state.callbacks;
                auto call = runtime->Call(instance, "Work");
                Check(call.argInt(depth).invoke() == RunStatus::Completed && call.retInt() == 17 + depth * (depth + 1) / 2,
                    "recursive callbacks " + call.error().message);
                Check(state.callbacks - before == depth, "callback count");
            }
            { auto call = runtime->Call(instance, "Throw");
              Check(call.invoke() == RunStatus::Failed && call.error().message.find("integer division by zero") != std::string::npos,
                  "nested failure preserves root cause"); }
            { auto call = runtime->Call(instance, "Work");
              Check(call.argInt(2).invoke() == RunStatus::Completed && call.retInt() == 20, "recovery after failure"); }
            const char* names[] = {"SetterLiteral", "SetterLocal", "GetterLocal"};
            const int expected[] = {123, 246, 156};
            for (int n = 0; n < 3; ++n) {
                state.property = 123;
                auto call = runtime->Call(instance, names[n]);
                Check(call.invoke() == RunStatus::Completed && call.retInt() == expected[n], names[n]);
            }
            runtime->DestroyInstance(instance);
            { auto call = runtime->Call(instance, state.saved);
              Check(call.invoke() == RunStatus::Failed, "closure invalid after instance destruction"); }
        }
        state.saved = {};
        runtime->DestroyProgram(program);
    }
    DestroyRuntime(runtime);
    std::printf("callback reentry: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
