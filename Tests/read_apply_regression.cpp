#include "NeoScript.h"
#include "NeoVMProgram.h"
#include "NeoVMWorker.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 30) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}
static std::vector<uint8_t> CompileImage(IRuntime* runtime, const std::string& source, bool debug)
{
    CompileDesc desc; desc.source = source; desc.sourceName = "read-apply.ns"; desc.includeDebugInfo = debug;
    std::vector<uint8_t> bytes;
    const Error error = runtime->CompileToBytecode(desc, bytes);
    Check(error.ok(), "compile: " + error.message);
    return bytes;
}
static std::vector<SVMOperation> Operations(const std::vector<uint8_t>& image)
{
    SNeoVMHeader header{};
    std::memcpy(&header, image.data(), sizeof(header));
    std::vector<SVMOperation> ops(header._iCodeSize / sizeof(SVMOperation));
    std::memcpy(ops.data(), image.data() + sizeof(header), header._iCodeSize);
    return ops;
}
static std::vector<uint8_t> ReplaceCode(const std::vector<uint8_t>& image, const std::vector<SVMOperation>& ops)
{
    auto bytes = image;
    std::memcpy(bytes.data() + sizeof(SNeoVMHeader), ops.data(), ops.size() * sizeof(SVMOperation));
    return bytes;
}
static std::string Execute(IRuntime* runtime, const std::vector<uint8_t>& image)
{
    Error error;
    auto program = runtime->LoadProgram(image, &error);
    Check(bool(program), "load: " + error.message);
    if (!program) return "LOAD ERROR";
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "create instance");
    std::string result;
    if (instance)
    {
        {
            auto call = runtime->Call(instance, "Probe");
            const auto status = call.invoke();
            if (status == RunStatus::Failed) result = "ERROR:" + call.error().message;
            else
            {
                Check(status == RunStatus::Completed, "completed");
                Check(call.retType() == ValueType::String, "string result");
                result = call.retString().str();
            }
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    return result;
}
static void Compare(IRuntime* runtime, const std::string& globals, const std::string& body,
    bool debug, bool requirePair = true, const std::string& expected = "")
{
    auto image = CompileImage(runtime, "import math;import system;" + globals +
        "export fun Probe(){" + body + "}", debug);
    if (image.empty()) return;
    auto ops = Operations(image);
    int pairs = 0;
    for (auto& op : ops) if (op.op == NOP_READ_APPLY) { ++pairs; op.op = NOP_CLT_READ; }
    Check(!requirePair || pairs > 0, "pair emitted: " + body);
    // 스냅샷 위치와 모든 다른 명령을 고정하고 READ_APPLY만 일반 READ로 바꿔 대조한다.
    const auto ordinary = ReplaceCode(image, ops);
    const std::string reference = Execute(runtime, ordinary);
    const std::string actual = Execute(runtime, image);
    Check(reference == actual, "same result/error: " + body + " expected=" + reference + " actual=" + actual);
    if (!expected.empty()) Check(actual == expected, "expected result: " + body + " actual=" + actual);
}
static void Matrix(IRuntime* runtime, bool debug)
{
    struct Collection { const char* value; const char* key; };
    const Collection collections[] = {
        {"[3]", "0"}, {"{\"k\":3}", "\"k\""}, {"{7:3}", "7"},
        {"system.array(3,1)", "0"}, {"system.array(3.5,1)", "0"},
        {"system.array32(3,1)", "0"}, {"system.array32(3.5,1)", "0"}
    };
    const std::string result = "if(type(r)==\"float\")return type(r)..\":\"..math.tostr(r);return type(r)..\":\"..tostring(r);";
    for (const auto& c : collections) for (const char* operation : {"+", "-", "*", "/"})
        for (const char* scalar : {"17", "17.25", "-0.0", "-9", "9223372036854775807", "1e300"})
        {
            const std::string access = std::string("c[") + c.key + "]";
            const std::string prefix = std::string("var c=") + c.value + ";var x=" + scalar + ";";
            Compare(runtime, "", prefix + "var r=" + access + operation + "x;" + result, debug);
            Compare(runtime, "", prefix + "var r=x" + operation + access + ";" + result, debug);
            Compare(runtime, "", prefix + "var r=x;r" + operation + "=" + access + ";" + result, debug);
            Compare(runtime, "", prefix + "x=x" + operation + access + ";var r=x;" + result, debug);
        }
    Compare(runtime, "", "var m={\"k\":3};var n=[4];return tostring(m.k*n[0]+2);", debug, true, "14");
    Compare(runtime, "", "var c=[2];var r=0;for(var i in 0,20){if(i==7)continue;r+=c[0];}return tostring(r);", debug, true, "38");
    Compare(runtime, "", "var c=[3];var r=0;switch(c[0]){case 3:r=2+c[0];break;default:r=8;}return tostring(r);", debug, true, "5");
    Compare(runtime, "", "var c=[\"b\"];var r=\"a\";r+=c[0];return r;", debug, true, "ab");
    Compare(runtime, "", "var x=math.Vector2(1,2);var c=[math.Vector2(3,4)];x=x+c[0];return tostring(x[0])..\":\"..tostring(x[1]);", debug, true, "4:6");
    Compare(runtime, "", "var x=[1];var c=[[2]];var r=x+c[0];return tostring(r[0])..\":\"..tostring(r[1]);", debug, true, "1:2");
    Compare(runtime, "var g=9;", "var c=[2];g=g-c[0];g*=c[0];return tostring(g);", debug, true, "14");
    Compare(runtime, "", "var x=9;var f=fun(){var c=[2];x-=c[0];return x;};return tostring(f())..\":\"..tostring(f());", debug, true, "7:5");
    Compare(runtime, "fun F(var x){return x;}", "var c=[3];return tostring(F(c[0]+2));", debug, true, "5");
    Compare(runtime, "var c=[3];fun Change(){c[0]=8;return 2;}", "return tostring(c[0]+Change());", debug, false, "5");
    Compare(runtime, "", "var c=[3];var x=1;return tostring(c[0]+(x=2));", debug, false, "5");
    Compare(runtime, "", "var c=[2];var x=[0];var r=0;for(var i in 0,12){x=[x];r=1+c[0];}return tostring(r);", debug, false);
    Compare(runtime, "", "var c=[3];c=c[0]+2;return tostring(c);", debug, true, "5");
    Compare(runtime, "", "var c=[3];c=2+c[0];return tostring(c);", debug, true, "5");
    Compare(runtime, "", "var c=[3];var x=2;return tostring((x+=c[0])+x);", debug, true, "10");
    Compare(runtime, "", "var c=[2.0];var z=0.0;var r=(-z)/c[0];return math.tostr(1.0/r);", debug);
    Compare(runtime, "", "var c=[0.0/0.0];var r=1+c[0];return tostring(r!=r);", debug, true, "true");
    Compare(runtime, "", "var c=[1.0/0.0];var r=c[0]-1;return math.tostr(r);", debug);
    for (const char* body : {
        "var c={};var r=1;r+=c.missing;return \"bad\";",
        "var c=[0];var r=1;r/=c[0];return \"bad\";",
        "var c=[-1];var r=-9223372036854775808/c[0];return \"bad\";",
        "var c=[1];var r=1+c[99];return \"bad\";",
        "var c=[false];var r=1+c[0];return \"bad\";",
        "var c=[math.Vector2(3,4)];var x=math.Vector2(1,2);x+=c[0];return \"bad\";",
        "var c=system.array32(1,1);var r=1+c[\"bad\"];return \"bad\";"
    }) Compare(runtime, "", body, debug);
}
static void RejectPairs(IRuntime* runtime, bool debug)
{
    auto image = CompileImage(runtime, "export fun Probe(){var c=[2];var r=0;for(var i in 0,4){r+=c[0];}return r;}", debug);
    if (image.empty()) return;
    const auto ops = Operations(image);
    size_t pair = 0, jump = 0;
    for (size_t i = 2; i < ops.size(); ++i)
    {
        if (ops[i].op == NOP_READ_APPLY) pair = i;
        if (ops[i].op == NOP_JMP) jump = i;
    }
    Check(pair > 0 && jump > 0, "image has pair and jump");
    if (!pair || !jump) return;
    for (int change = 0; change < 6; ++change)
    {
        auto altered = ops;
        switch (change)
        {
        case 0: altered[pair + 1].op = NOP_NONE; break;
        case 1: altered[pair].argFlag &= ~NEOS_ARG_N3_LOCAL; break;
        case 2: altered[pair + 1].n2 = altered[pair + 1].n1; break;
        case 3: altered[jump].n1 = static_cast<short>(pair - jump); break;
        case 4: altered[pair + 1].argFlag |= NEOS_OP_CALL_NORESULT; break;
        case 5: altered[pair + 1].n1 = altered[pair].n3; break;
        }
        Error error;
        auto bad = runtime->LoadProgram(ReplaceCode(image, altered), &error);
        Check(!bad && !error.ok(), "reject malformed pair " + std::to_string(change));
        if (bad) runtime->DestroyProgram(bad);
    }
}
static int RunSliced(const std::vector<uint8_t>& image, bool budget)
{
    std::string error;
    auto program = NeoVMSystem::CreateProgram(image.data(), static_cast<int>(image.size()), &error);
    Check(program != nullptr, "sliced load: " + error);
    if (!program) return -1;
    auto pool = NeoExecContextPool_Create(100);
    auto vm = NeoVMSystem::CreateVM();
    NeoLoadVMParam load; load.execPool = pool;
    auto worker = vm->LoadVM(&load, program, true, false, 100);
    Check(worker != nullptr, "sliced worker");
    int slices = 0;
    if (worker)
    {
        std::vector<VarInfo> args;
        Check(worker->ExecuteTop(0, args) == NEOEXEC_COMPLETED, "sliced init");
        if (budget) { worker->SetTimeout(-1, 1); worker->SetInstructionBudget(1); }
        int status = worker->ExecuteTop(worker->FindFunction("Probe"), args);
        while (status == NEOEXEC_SUSPENDED && ++slices < 1000) status = worker->ResumeTop(true);
        Check(status == NEOEXEC_COMPLETED, "sliced completion");
        if (budget) Check(worker->GetReturnVar() && worker->GetReturnVar()->GetType() == VAR_INT &&
            worker->GetReturnVar()->_int == 14, "sliced value");
    }
    NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
    return slices;
}
// getter는 호출자 전역을 바꾸거나 디버거 정지를 요청해도 한 번만 실행되어야 한다.
struct GetterState { int count = 0; bool pause = false, transfer = false; };
static bool Getter(INeoVMWorker* worker, void* user, const VMString*, VarInfo* out, bool get)
{
    if (!get) return false;
    auto& state = *static_cast<GetterState*>(user);
    ++state.count;
    const int child = worker->FindFunction("Touch");
    VarInfo* returned = static_cast<CNeoVMWorker*>(worker)->testCall(child, nullptr, 0);
    if (!returned) return false;
    worker->Var_Move(out, returned);
    if (state.pause) worker->DebugPause();
    if (state.transfer)
    {
        state.transfer = false;
        VarInfo* co = worker->GetVar("co");
        const auto* functions = CNeoVM::GetSystemModule("coroutine");
        for (const auto& function : *functions)
        {
            if (function.fname != "resume") continue;
            worker->Var_SetCoroutine(worker->GetStackVar(1), co->_cor);
            const bool result = CNeoVM::CallDefaultNativeByIndex(function.nativeIndex,
                static_cast<CNeoVMWorker*>(worker), 1);
            worker->Var_Release(worker->GetStackVar(1));
            return result;
        }
        return false;
    }
    return true;
}
static void GetterBoundary(IRuntime* runtime, bool debug)
{
    for (int mode = 0; mode < 3; ++mode)
    {
        const bool pause = mode == 1;
        if (pause && !debug) continue;
        const std::string source =
            "import coroutine;export var Host; export var result=1; export var x=1;\n"
            "export fun Never(){return 0;}\n"
            "export fun Touch(){x=10;return 2;}\n"
            "export var co;export var phase=0;fun Other(){phase=1;yield;phase=2;}\n"
            "export fun Prepare(){co=coroutine.create(Other);}\n"
            "export fun Probe(){result+=Host.Value;result+=x+Host.Value;}\n";
        auto image = CompileImage(runtime, source, debug);
        if (image.empty()) continue;
        std::string error;
        auto program = NeoVMSystem::CreateProgram(image.data(), static_cast<int>(image.size()), &error);
        Check(program != nullptr, "getter load: " + error);
        if (!program) continue;
        auto pool = NeoExecContextPool_Create();
        auto vm = NeoVMSystem::CreateVM(); NeoLoadVMParam load; load.execPool = pool;
        auto worker = vm->LoadVM(&load, program, true, false, 100);
        if (worker)
        {
            std::vector<VarInfo> args;
            Check(worker->ExecuteTop(0, args) == NEOEXEC_COMPLETED, "getter init");
            GetterState state; state.pause = pause; state.transfer = mode == 2;
            Check(worker->ExecuteTop(worker->FindFunction("Prepare"), args) == NEOEXEC_COMPLETED, "getter coroutine setup");
            VarInfo* host = worker->GetVar("Host");
            Check(worker->ResetVarType(host, VAR_FP_NATIVE) &&
                NeoVMSystem::RegisterTableCallBack(host, &state, nullptr, Getter), "getter bind");
            if (pause) worker->DebugSetBreakpoints(std::vector<int>{2});
            int status = worker->ExecuteTop(worker->FindFunction("Probe"), args);
            if (pause)
            {
                Check(status == NEOEXEC_SUSPENDED && worker->DebugIsPaused(), "getter pause waits until pair completes");
                Check(worker->GetVar("result")->_int == 3 && state.count == 1, "first pair completes before pause");
                state.pause = false; worker->DebugContinue(); status = worker->ResumeTop();
            }
            Check(status == NEOEXEC_COMPLETED && state.count == 2, "getter resumes once per access");
            Check(worker->GetVar("result")->_int == 15, "getter result and global snapshot preserved");
            Check(worker->GetVar("phase")->_int == (mode == 2 ? 1 : 0), "getter context transfer returns to pending arithmetic");
        }
        else Check(false, "getter worker");
        NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
    }
}

static void DebugPairBoundary(IRuntime* runtime)
{
    auto image = CompileImage(runtime,
        "export var result=0;\nexport fun Probe(){\nvar c=[2];var z=3;\nresult=c[0]+\nz;\nreturn result;\n}\n", true);
    if (image.empty()) return;
    std::string error;
    auto program = NeoVMSystem::CreateProgram(image.data(), static_cast<int>(image.size()), &error);
    Check(program != nullptr, "debug pair load: " + error);
    if (!program) return;
    size_t pair = 0;
    for (size_t i = 2; i < program->code.size(); ++i)
        if (program->code[i].op == NOP_READ_APPLY) pair = i;
    Check(pair > 0, "multiline pair emitted");
    if (!pair) { program->Release(); return; }
    const int firstLine = program->debugData[pair]._lineseq;
    const int tailLine = program->debugData[pair + 1]._lineseq;
    Check(firstLine != tailLine, "pair crosses source lines");
    auto pool = NeoExecContextPool_Create();
    auto vm = NeoVMSystem::CreateVM(); NeoLoadVMParam load; load.execPool = pool;
    auto worker = vm->LoadVM(&load, program, true, false, 100);
    Check(worker != nullptr, "debug pair worker");
    if (worker)
    {
        std::vector<VarInfo> args;
        Check(worker->ExecuteTop(0, args) == NEOEXEC_COMPLETED, "debug pair init");
        std::vector<int> lines; worker->DebugGetExecutableLines(lines);
        bool tailAdvertised = false;
        for (int line : lines) if (line == tailLine) tailAdvertised = true;
        Check(!tailAdvertised, "tail-only line is not a breakpoint location");
        worker->DebugSetBreakpoints(std::vector<int>{tailLine});
        Check(worker->ExecuteTop(worker->FindFunction("Probe"), args) == NEOEXEC_COMPLETED,
            "tail breakpoint does not split pair");
        worker->DebugSetBreakpoints(std::vector<int>{firstLine});
        Check(worker->ExecuteTop(worker->FindFunction("Probe"), args) == NEOEXEC_SUSPENDED,
            "breakpoint before pair still works");
        Check(worker->DebugGetLocation().opIndex == static_cast<int>(pair), "stops at pair head");
        worker->DebugStepInto();
        const int status = worker->ResumeTop();
        Check(status == NEOEXEC_SUSPENDED && worker->DebugGetLocation().opIndex > static_cast<int>(pair + 1),
            "step skips pair tail");
        Check(worker->GetVar("result")->_int == 5, "step executes whole pair");
        worker->DebugContinue();
        Check(worker->ResumeTop() == NEOEXEC_COMPLETED, "debug pair completes");
    }
    NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
}

int main()
{
    auto runtime = CreateRuntime(); runtime->FreezeBindings();
    for (bool debug : {false, true})
    {
        Matrix(runtime, debug);
        GetterBoundary(runtime, debug);
        RejectPairs(runtime, debug);
        auto image = CompileImage(runtime, "export fun Probe(){var c=[2];var r=0;for(var i in 0,7)r+=c[0];return r;}", debug);
        if (image.empty()) continue;
        auto ops = Operations(image);
        for (auto& op : ops) if (op.op == NOP_READ_APPLY) op.op = NOP_CLT_READ;
        const int paired = RunSliced(image, true);
        Check(paired > 0 && paired + 7 == RunSliced(ReplaceCode(image, ops), true), "one budget unit per completed pair");
        RunSliced(image, false);
    }
    DebugPairBoundary(runtime);
    DestroyRuntime(runtime);
    std::printf("read_apply_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
