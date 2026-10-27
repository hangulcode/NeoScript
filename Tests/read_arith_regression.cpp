#include "NeoScript.h"
#include "NeoParser.h"
#include "NeoVMProgram.h"
#include "NeoVMWorker.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>
#include <vector>

// READ_ADD/SUB/MUL/DIV(x op= clt[key]) 융합 명령 회귀. 같은 소스를 융합 켜고/끄고 컴파일해 값·오류·호출 스택을 대조한다.
using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 40) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}
static std::vector<uint8_t> CompileImage(IRuntime* runtime, const std::string& source, bool debug, bool fuse = true)
{
    CompileDesc desc; desc.source = source; desc.sourceName = "read-arith.ns"; desc.includeDebugInfo = debug;
    std::vector<uint8_t> bytes;
    g_neoFuseCollectionReads = fuse;
    const Error error = runtime->CompileToBytecode(desc, bytes);
    g_neoFuseCollectionReads = true;
    Check(error.ok(), "compile: " + error.message + " :: " + source.substr(0, 80));
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
static int CountFused(const std::vector<SVMOperation>& ops, eNOperation op = NOP_NONE)
{
    int count = 0;
    for (const auto& o : ops)
        if (IsReadArithOp(o.op) && (op == NOP_NONE || o.op == op)) ++count;
    return count;
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
// 두 컴파일은 명령 수가 달라 오류 메시지의 IP 숫자만 정규화한다. 오류 종류와 함수·줄 정보는 그대로 비교한다.
static void NormalizeIP(std::string& text)
{
    size_t at = 0;
    while ((at = text.find("IP(", at)) != std::string::npos)
    {
        const size_t end = text.find(')', at);
        if (end == std::string::npos) break;
        text.replace(at + 3, end - at - 3, "*"); at += 5;
    }
}
static void Compare(IRuntime* runtime, const std::string& globals, const std::string& body,
    bool debug, bool requireFusion = true, const std::string& expected = "")
{
    const std::string source = "import math;import system;" + globals + "export fun Probe(){" + body + "}";
    auto fused = CompileImage(runtime, source, debug, true);
    auto plain = CompileImage(runtime, source, debug, false);
    if (fused.empty() || plain.empty()) return;
    Check(CountFused(Operations(plain)) == 0, "reference has no fusion: " + body);
    Check(!requireFusion || CountFused(Operations(fused)) > 0, "fusion emitted: " + body);
    std::string reference = Execute(runtime, plain);
    std::string actual = Execute(runtime, fused);
    if (reference.compare(0, 6, "ERROR:") == 0) { NormalizeIP(reference); NormalizeIP(actual); }
    Check(reference == actual, "same result/error: " + body + " expected=" + reference + " actual=" + actual);
    if (!expected.empty()) Check(actual == expected, "expected result: " + body + " actual=" + actual);
}
static void Matrix(IRuntime* runtime, bool debug)
{
    struct Collection { const char* value; const char* key; };
    const Collection collections[] = {
        {"[3]", "0"}, {"{\"k\":3}", "\"k\""}, {"{7:3}", "7"}, {"{\"k\":3}", "key"},
        {"system.array(3,1)", "0"}, {"system.array(3.5,1)", "0"},
        {"system.array32(3,1)", "0"}, {"system.array32(3.5,1)", "0"}, {"math.Vector2(3,4)", "0"}
    };
    const std::string result = "if(type(r)==\"float\")return type(r)..\":\"..math.tostr(r);return type(r)..\":\"..tostring(r);";
    for (const auto& c : collections) for (const char* operation : {"+", "-", "*", "/"})
        for (const char* scalar : {"17", "17.25", "-0.0", "-9", "9223372036854775807", "1e300"})
        {
            const std::string access = std::string("c[") + c.key + "]";
            const std::string prefix = std::string("var key=\"k\";var c=") + c.value + ";var x=" + scalar + ";";
            Compare(runtime, "", prefix + "var r=x;r" + operation + "=" + access + ";" + result, debug);
            Compare(runtime, "", prefix + "x" + operation + "=" + access + ";var r=x;" + result, debug);
            // 융합되지 않는 형태도 같은 소스로 대조해 파서 변경이 다른 형태를 건드리지 않았는지 본다.
            Compare(runtime, "", prefix + "var r=" + access + operation + "x;" + result, debug, false);
        }
    Compare(runtime, "", "var m={\"k\":3};var n=[4];var r=2;r+=m.k;r*=n[0];return tostring(r);", debug, true, "20");
    Compare(runtime, "", "var c=[2];var r=0;for(var i in 0,20){if(i==7)continue;r+=c[0];}return tostring(r);", debug, true, "38");
    Compare(runtime, "", "var c=[3];var r=0;switch(c[0]){case 3:r=2;r+=c[0];break;default:r=8;}return tostring(r);", debug, true, "5");
    Compare(runtime, "", "var c=[\"b\"];var r=\"a\";r+=c[0];return r;", debug, true, "ab");
    Compare(runtime, "", "var c=[\"b\",\"c\"];var r=c[0];r+=c[1];return r;", debug, true, "bc");
    Compare(runtime, "", "var c=[math.Vector2(3,4)];var d=[math.Vector2(1,1)];var r=c[0];r*=d[0];return \"bad\";", debug);
    Compare(runtime, "var g=9;", "var c=[2];g-=c[0];g*=c[0];return tostring(g);", debug, true, "14");
    Compare(runtime, "", "var x=9;var f=fun(){var c=[2];x-=c[0];return x;};return tostring(f())..\":\"..tostring(f());", debug, true, "7:5");
    Compare(runtime, "fun F(var x){return x;}", "var c=[3];var r=2;r+=c[0];return tostring(F(r));", debug, true, "5");
    Compare(runtime, "var c=[3];fun Change(){c[0]=8;return 2;}", "var r=0;r+=c[0]+Change();return tostring(r);", debug, false, "5");
    Compare(runtime, "var c=[3];fun Change(){c[0]=8;return 2;}", "var r=Change();r+=c[0];return tostring(r);", debug, true, "10");
    Compare(runtime, "", "var c=[2];var x=[0];var r=0;for(var i in 0,12){x=[x];r=1;r+=c[0];}return tostring(r);", debug, true, "3");
    Compare(runtime, "", "var c=[3];var x=2;return tostring((x+=c[0])+x);", debug, true, "10");
    Compare(runtime, "", "var c=[2.0];var z=0.0;var r=(-z);r/=c[0];return math.tostr(1.0/r);", debug);
    Compare(runtime, "", "var c=[0.0/0.0];var r=1;r+=c[0];return tostring(r!=r);", debug, true, "true");
    Compare(runtime, "", "var c=[1.0/0.0];var r=1;r-=c[0];return math.tostr(r);", debug);
    Compare(runtime, "", "var m={\"a\":{\"b\":2}};var l=[[1,2],[3]];var s=0;s+=m.a.b;s+=l[0][1];s+=l[1][0];return tostring(s);", debug, true, "7");
    Compare(runtime, "", "var l=[[1,2],[3]];var s=1;s*=l[0][1];s+=l[1][0];return tostring(s);", debug, true, "5");
    Compare(runtime, "", "var m={};m[7]=4;fun K(){return 7;}var s=1;s+=m[7];s+=m[K()];s*=m[K()];return tostring(s);", debug, true, "36");
    Compare(runtime, "", "var m={\"a\":1};var alias=m;var s=0;s+=alias.a;m.a=5;s+=alias[\"a\"];return tostring(s)..\":\"..tostring(m.a);", debug, true, "6:5");
    Compare(runtime, "", "var v=math.Vector3(1,2,3);var a=system.array(0,2);var f=system.array(0.5,1);a[1]=7;var s=10;s+=v[2];s+=a[1];s*=f[0];return math.tostr(s);", debug, true, "10");
    Compare(runtime, "", "var c=[2,1,5];var s=0;var i=1;s+=c[i];i+=c[i];s+=c[i];return tostring(s)..\":\"..tostring(i);", debug, true, "6:2");
    Compare(runtime, "", "var l=[1];l+=l[0];return tostring(l[0])..\":\"..tostring(l.len());", debug);
    Compare(runtime, "", "var c=[1];var r=1;r+=c[0];r*=c[0];r-=c[0];r/=c[0];return math.tostr(r);", debug, true, "1");
    Compare(runtime, "", "var c=[1];var e=[0];e[0]+=c[0];e[0]=e[0]+c[0];return tostring(e[0]);", debug, false, "2");
    for (const char* body : {
        "var c={};var r=1;r+=c.missing;return \"bad\";",
        "var c=[0];var r=1;r/=c[0];return \"bad\";",
        "var c=[-1];var r=-9223372036854775808;r/=c[0];return \"bad\";",
        "var c=[1];var r=1;r+=c[99];return \"bad\";",
        "var c=[false];var r=1;r+=c[0];return \"bad\";",
        "var c=[math.Vector2(3,4)];var x=math.Vector2(1,2);x+=c[0];return \"bad\";",
        "var c=system.array32(1,1);var r=1;r+=c[\"bad\"];return \"bad\";",
        "var c=[\"a\"];var v=math.Vector2(1,2);v+=c[0];return \"bad\";",
        "var c=[1];var r=\"s\";r/=c[0];return \"bad\";"
    }) Compare(runtime, "", body, debug);
}
// 문장 형태마다 기대한 opcode 가 하나만 나오고, 임시 슬롯을 거치는 명령이 남지 않는지 본다.
static void Shapes(IRuntime* runtime, bool debug)
{
    struct Shape { const char* statement; eNOperation op; };
    const Shape shapes[] = {
        {"r+=n[0];", NOP_READ_ADD}, {"r-=n[i];", NOP_READ_SUB}, {"r*=s.a;", NOP_READ_MUL}, {"r/=s[\"a\"];", NOP_READ_DIV},
        {"r+=n[0][0];", NOP_READ_ADD}
    };
    const std::string prefix = "export fun Probe(){var n=[[3]];var i=0;var s={\"a\":1};var r=0;";
    const std::string suffix = "return tostring(r);}";
    const auto baseline = CompileImage(runtime, prefix + suffix, debug);
    if (baseline.empty()) return;
    for (const auto& shape : shapes)
    {
        const auto image = CompileImage(runtime, prefix + shape.statement + suffix, debug);
        if (image.empty()) continue;
        const auto ops = Operations(image);
        Check(CountFused(ops, shape.op) == 1 && CountFused(ops) == 1, std::string("single fused instruction: ") + shape.statement);
        const size_t extra = ops.size() - Operations(baseline).size();
        const size_t expected = std::strstr(shape.statement, "[0][0]") ? 2u : 1u; // 중첩 경로는 소유자 읽기 하나가 앞선다
        Check(extra == expected, std::string("statement is one instruction: ") + shape.statement + " extra=" + std::to_string(extra));
        for (const auto& op : ops)
            if (IsReadArithOp(op.op)) Check((op.argFlag & ~(NEOS_ARG_N1_LOCAL | NEOS_ARG_N2_LOCAL | NEOS_ARG_N3_LOCAL)) == 0, "only local flags");
    }
}
// 손상된 이미지를 거부하는지 본다.
static void RejectImages(IRuntime* runtime, bool debug)
{
    auto image = CompileImage(runtime, "export fun Probe(){var n=[2];var r=0;for(var i in 0,4){r+=n[0];}return r;}", debug);
    if (image.empty()) return;
    const auto ops = Operations(image);
    size_t head = 0;
    for (size_t i = 2; i < ops.size(); ++i) if (IsReadArithOp(ops[i].op)) head = i;
    Check(head > 0, "image has a fused instruction");
    if (!head) return;
    for (int change = 0; change < 4; ++change)
    {
        auto altered = ops;
        switch (change)
        {
        case 0: altered[head].argFlag |= 0x08; break;                                      // 즉값 플래그
        case 1: altered[head].argFlag |= 0x80; break;                                      // 상위 비트
        case 2: altered[head].argFlag &= ~NEOS_ARG_N3_LOCAL; altered[head].n3 = 0; break;  // 상수를 대입 대상으로
        case 3: altered[head].argFlag |= NEOS_ARG_N3_LOCAL; altered[head].n3 = 30000; break; // 로컬 범위 밖
        }
        Error error;
        auto bad = runtime->LoadProgram(ReplaceCode(image, altered), &error);
        Check(!bad && !error.ok(), "reject malformed image " + std::to_string(change));
        if (bad) runtime->DestroyProgram(bad);
    }
    Error error;
    auto good = runtime->LoadProgram(image, &error);
    Check(bool(good) && error.ok(), "original image loads");
    if (good) runtime->DestroyProgram(good);
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
            "export var co;export var phase=0;export var observed=0;fun Other(){observed=result;phase=1;yield;phase=2;}\n"
            "export fun Prepare(){co=coroutine.create(Other);}\n"
            "export fun Probe(){result+=Host.Value;result+=x+Host.Value;}\n";
        auto image = CompileImage(runtime, source, debug);
        if (image.empty()) continue;
        Check(CountFused(Operations(image)) == 1, "host property compound read fuses");
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
                Check(status == NEOEXEC_SUSPENDED && worker->DebugIsPaused(), "getter pause waits until instruction completes");
                Check(worker->GetVar("result")->_int == 3 && state.count == 1, "first instruction completes before pause");
                state.pause = false; worker->DebugContinue(); status = worker->ResumeTop();
            }
            Check(status == NEOEXEC_COMPLETED && state.count == 2, "getter runs once per access");
            Check(worker->GetVar("result")->_int == 15, "getter result and global snapshot preserved");
            if (mode == 2) Check(worker->GetVar("observed")->_int == 3, "coroutine observes completed arithmetic");
            Check(worker->GetVar("phase")->_int == (mode == 2 ? 1 : 0), "getter context transfer occurs after complete instruction");
        }
        else Check(false, "getter worker");
        NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
    }
}

static void DebugBoundary(IRuntime* runtime)
{
    auto image = CompileImage(runtime,
        "export var result=3;\nexport fun Probe(){\nvar c=[2];\nresult +=\nc[0];\nreturn result;\n}\n", true);
    if (image.empty()) return;
    std::string error;
    auto program = NeoVMSystem::CreateProgram(image.data(), static_cast<int>(image.size()), &error);
    Check(program != nullptr, "debug load: " + error);
    if (!program) return;
    size_t head = 0;
    for (size_t i = 2; i < program->code.size(); ++i)
        if (IsReadArithOp(program->code[i].op)) head = i;
    Check(head > 0, "multiline fused instruction emitted");
    if (!head) { program->Release(); return; }
    const int line = program->debugData[head]._lineseq;
    auto pool = NeoExecContextPool_Create();
    auto vm = NeoVMSystem::CreateVM(); NeoLoadVMParam load; load.execPool = pool;
    auto worker = vm->LoadVM(&load, program, true, false, 100);
    Check(worker != nullptr, "debug worker");
    if (worker)
    {
        std::vector<VarInfo> args;
        Check(worker->ExecuteTop(0, args) == NEOEXEC_COMPLETED, "debug init");
        worker->DebugSetBreakpoints(std::vector<int>{line});
        Check(worker->ExecuteTop(worker->FindFunction("Probe"), args) == NEOEXEC_SUSPENDED, "breakpoint before fused instruction");
        Check(worker->DebugGetLocation().opIndex == static_cast<int>(head), "stops at the fused instruction");
        worker->DebugStepInto();
        const int status = worker->ResumeTop();
        Check(status == NEOEXEC_SUSPENDED && worker->DebugGetLocation().opIndex > static_cast<int>(head), "step passes the instruction");
        Check(worker->GetVar("result")->_int == 5, "step executes the whole instruction");
        worker->DebugContinue();
        Check(worker->ResumeTop() == NEOEXEC_COMPLETED, "debug run completes");
    }
    NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
}

// 예외 콜백 안에서 프레임을 복사해야 실행 컨텍스트 정리 전의 위치를 검증할 수 있다.
struct FaultListener : INeoVMDebugListener
{
    bool stopped = false;
    bool callerLocal = false;
    NeoDebugLocation location;
    std::vector<NeoDebugStackFrame> frames;
    void OnNeoDebugStopped(INeoVMWorker* worker, const NeoDebugLocation& at, NeoDebugStopReason reason) override
    {
        if (reason != NEO_DEBUG_STOP_EXCEPTION || stopped) return;
        stopped = true;
        location = at;
        worker->DebugGetStackTrace(frames);
        for (const auto& frame : frames)
        {
            if (frame.functionName.find("Fault") == std::string::npos) continue;
            std::vector<NeoDebugVariable> variables;
            worker->DebugGetFrameVariables(frame.frameId, variables);
            for (const auto& variable : variables)
                if (variable.name == "x" && variable.value == "10") callerLocal = true;
        }
    }
};
static bool FailingGetter(INeoVMWorker* base, void* data, const VMString*, VarInfo* out, bool)
{
    const int mode = *static_cast<int*>(data);
    auto* worker = static_cast<CNeoVMWorker*>(base);
    if (mode == 4)
        return worker->testCall(base->FindFunction("Child"), nullptr, 0) != nullptr;
    if (mode == 5 || mode == 6)
    {
        // mode 6: 값을 넣은 뒤 던진다. 융합 명령이 지역에 들고 있던 값이 예외 뒤에도 해제돼야 한다.
        if (mode == 6) base->Var_SetString(out, "leaked-through-exception");
        throw std::runtime_error("getter exception");
    }
    worker->SetError("fused getter failure");
    return false;
}
static void FaultBoundaries(IRuntime* runtime)
{
    for (int mode = 0; mode < 7; ++mode)
    {
        const std::string rhs = mode >= 3 ? "Host.Value" : mode == 0 ? "values[5]" : "values[0]";
        const std::string values = mode == 2 ? "[\"bad\"]" : "[0]";
        const std::string source =
            "export var Host;export var after=0;\n"
            "export fun Child(){\nvar values=[0];var x=10;\nx /=\nvalues[0];\nreturn x;\n}\n"
            "export fun Fault(){\nvar values=" + values + ";var x=10;\nx /=\n" + rhs + ";\n" +
            "after=1;\nreturn x;\n}\n"
            "export fun Middle(){\nvar r=Fault();\nreturn r+1;\n}\n"
            "export fun Probe(){\nvar r=Middle();\nreturn r+1;\n}\n";
        auto image = CompileImage(runtime, source, true);
        if (image.empty()) continue;
        std::string error;
        auto program = NeoVMSystem::CreateProgram(image.data(), static_cast<int>(image.size()), &error);
        Check(program != nullptr, "fault image: " + error);
        if (!program) continue;
        AllocStats before; GetAllocStats(before);
        auto pool = NeoExecContextPool_Create();
        auto vm = NeoVMSystem::CreateVM(); NeoLoadVMParam load; load.execPool = pool;
        auto worker = vm->LoadVM(&load, program, true, false, 100);
        Check(worker != nullptr, "fault worker");
        if (worker)
        {
            std::vector<VarInfo> args;
            Check(worker->ExecuteTop(0, args) == NEOEXEC_COMPLETED, "fault init");
            if (mode >= 3)
            {
                VarInfo* host = worker->GetVar("Host");
                Check(worker->ResetVarType(host, VAR_FP_NATIVE) &&
                    NeoVMSystem::RegisterTableCallBack(host, &mode, nullptr, FailingGetter), "fault getter bind");
            }
            const int function = worker->FindFunction(mode == 4 ? "Child" : "Fault");
            const int begin = program->functions[function]._codePtr / sizeof(SVMOperation);
            int end = static_cast<int>(program->code.size());
            for (const auto& fun : program->functions)
            {
                const int start = fun._codePtr / sizeof(SVMOperation);
                if (start > begin && start < end) end = start;
            }
            int head = -1;
            for (int i = begin; i < end; ++i) if (IsReadArithOp(program->code[i].op)) head = i;
            Check(head >= 0, "fault uses a fused instruction");
            FaultListener listener; worker->DebugSetListener(&listener);
            const int status = worker->ExecuteTop(worker->FindFunction("Probe"), args);
            const std::string detail = vm->GetLastErrorMsg();
            Check(status == NEOEXEC_ERROR, "fault fails mode=" + std::to_string(mode));
            Check(worker->GetVar("after")->_int == 0, "fault does not continue");
            Check(listener.stopped && listener.location.opIndex == head,
                "fault identifies the fused instruction mode=" + std::to_string(mode) + " detail=" + detail);
            if (head >= 0)
            {
                Check(listener.location.line == program->debugData[head]._lineseq &&
                    listener.location.file == program->debugData[head]._fileseq, "fault identifies the source line");
                Check(detail.find("IP(" + std::to_string(head) + ")") != std::string::npos, "text error identifies the IP");
            }
            Check(!listener.frames.empty() && listener.frames[0].opIndex == head, "debug top frame identifies the instruction");
            // 호스트 getter 가 testCall 로 연 중첩 실행은 getter 를 부른 스크립트 프레임(Fault)을 콜스택에 남기지 않는다(기존 동작).
            if (mode != 4) Check(listener.callerLocal, "caller variables use the original script stack base");
            Check((mode == 4 || detail.find("Fault") != std::string::npos) && detail.find("Middle") != std::string::npos &&
                detail.find("Probe") != std::string::npos,
                "outer script call stack retained mode=" + std::to_string(mode) + " detail=" + detail);
            worker->DebugSetListener(nullptr);
        }
        NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
        // 예외로 끝난 실행이 스크립트 객체(문자열·리스트)를 남기면 안 된다.
        AllocStats after; GetAllocStats(after);
        Check(after.strings == before.strings && after.lists == before.lists,
            "no script object leaks after fault mode=" + std::to_string(mode) +
            " strings " + std::to_string(before.strings) + "->" + std::to_string(after.strings));
    }
}

int main()
{
    auto runtime = CreateRuntime(); runtime->FreezeBindings();
    for (bool debug : {false, true})
    {
        Matrix(runtime, debug);
        Shapes(runtime, debug);
        GetterBoundary(runtime, debug);
        RejectImages(runtime, debug);
        // 키를 변수로 둬 분리 컴파일의 상수 hoist 프롤로그가 명령 수 비교에 끼어들지 않게 한다.
        const char* source = "export fun Probe(){var c=[2];var k=0;var r=0;for(var i in 0,7)r+=c[k];return r;}";
        auto fused = CompileImage(runtime, source, debug, true);
        auto plain = CompileImage(runtime, source, debug, false);
        if (fused.empty() || plain.empty()) continue;
        // 융합 명령 하나가 예산 한 단위다: 분리 컴파일은 반복마다 명령이 하나 더 많다.
        const int fusedSlices = RunSliced(fused, true);
        const int plainSlices = RunSliced(plain, true);
        Check(fusedSlices > 0 && fusedSlices + 7 == plainSlices, "one budget unit per fused instruction fused=" +
            std::to_string(fusedSlices) + " plain=" + std::to_string(plainSlices));
        RunSliced(fused, false);
    }
    DebugBoundary(runtime);
    FaultBoundaries(runtime);
    const auto deep = CompileImage(runtime,
        "fun Recurse(var n){if(n>0){var r=Recurse(n-1);return r+1;}var c=[0];var x=1;x/=c[0];return x;}"
        "export fun Probe(){var r=Recurse(80);return r+1;}", true);
    if (!deep.empty())
    {
        const std::string trace = Execute(runtime, deep);
        size_t at = 0, frames = 0;
        while ((at = trace.find("\n  #", at)) != std::string::npos) { ++frames; at += 4; }
        Check(frames == 64 && trace.find("18 frames omitted") != std::string::npos &&
            trace.find("Probe") != std::string::npos, "deep trace retains both ends within 64 frames");
    }
    DestroyRuntime(runtime);
    std::printf("read_arith_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
