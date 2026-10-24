#include "NeoScript.h"
#include "NeoVMProgram.h"
#include "NeoVMInternal.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 50) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}
static std::vector<uint8_t> CompileImage(IRuntime* runtime, const std::string& source, bool debug)
{
    CompileDesc desc; desc.source = source; desc.sourceName = "host-runtime.ns"; desc.includeDebugInfo = debug;
    std::vector<uint8_t> bytes;
    const auto error = runtime->CompileToBytecode(desc, bytes);
    Check(error.ok() && !bytes.empty(), "compile: " + error.message);
    return bytes;
}
static bool SameObjects(const AllocStats& a, const AllocStats& b)
{
    return a.maps == b.maps && a.lists == b.lists && a.arrays == b.arrays && a.sets == b.sets &&
        a.vectors == b.vectors && a.coroutines == b.coroutines && a.closures == b.closures && a.modules == b.modules;
}
static void FrameCase(IRuntime* runtime, bool debug, const std::string& probe, const std::string& label)
{
    const std::string source = R"NS(
import math; import system; import coroutine;
export var entered=0;
fun Recur(var n,var a,var b,var c,var d,var e){
 if(n==0)return 7;
 return Recur(n-1,[n],{"n":n},math.Vector2(n,n),fun(){return n;},system.array(n,2));
}
export fun Recovery(){entered+=1;return 17;}
export fun Probe(){
)NS" + probe + "}";
    auto bytes = CompileImage(runtime, source, debug);
    if (bytes.empty()) return;
    std::string error;
    auto program = NeoVMSystem::CreateProgram(bytes.data(), (int)bytes.size(), &error);
    Check(program != nullptr, label + " load: " + error);
    if (!program) return;
    bool hasOutgoing = false;
    for (const auto& function : program->functions)
        hasOutgoing = hasOutgoing || function._stackSlotCount > function._localAddCount;
    Check(hasOutgoing, label + " outgoing capacity derived independently of stride");
    AllocStats before; GetAllocStats(before);
    auto pool = NeoExecContextPool_Create(100);
    auto vm = NeoVMSystem::CreateVM();
    NeoLoadVMParam load; load.execPool = pool;
    auto worker = vm->LoadVM(&load, program, true, false, 100);
    Check(worker != nullptr, label + " worker");
    if (worker)
    {
        std::vector<VarInfo> args;
        Check(worker->ExecuteTop(0, args) == NEOEXEC_COMPLETED, label + " init");
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            Check(worker->ExecuteTop(worker->FindFunction("Probe"), args) == NEOEXEC_ERROR &&
                vm->_pErrorMsg.find("call stack overflow") != std::string::npos,
                label + " rejects exhausted frame before producer: " + vm->_pErrorMsg);
            Check(worker->GetExecutionState() == NeoExecutionState::Idle, label + " failure releases context");
            Check(worker->ExecuteTop(worker->FindFunction("Recovery"), args) == NEOEXEC_COMPLETED,
                label + " same-worker recovery");
        }
        Check(worker->GetVar("entered") && worker->GetVar("entered")->_int == 3, label + " recovery value");
    }
    NeoVMSystem::ReleaseVM(vm);
    NeoExecContextPool_Destroy(pool);
    program->Release();
    runtime->CollectCycles(true);
    AllocStats after; GetAllocStats(after);
    Check(SameObjects(before, after), label + " outgoing allocated arguments released");
}
static void BudgetCase(IRuntime* runtime, bool debug)
{
    auto bytes = CompileImage(runtime,
        "export var ticks=0;export fun Work(){for(var i in 0,31)ticks+=1;return ticks;}"
        "export fun Fail(){var value=0;return 1/value;}", debug);
    if (bytes.empty()) return;
    std::string error;
    auto program = NeoVMSystem::CreateProgram(bytes.data(), (int)bytes.size(), &error);
    Check(program != nullptr, "budget program: " + error);
    if (!program) return;
    auto pool = NeoExecContextPool_Create(100);
    auto vm = NeoVMSystem::CreateVM(); NeoLoadVMParam load; load.execPool = pool;
    auto worker = vm->LoadVM(&load, program, true, false, 100);
    Check(worker != nullptr, "budget worker");
    if (worker)
    {
        std::vector<VarInfo> args;
        Check(worker->ExecuteTop(0,args) == NEOEXEC_COMPLETED, "budget init");
        worker->SetTimeout(-1, 1);
        worker->SetInstructionBudget(1);
        int status = worker->ExecuteTop(worker->FindFunction("Work"),args), slices=0;
        Check(status == NEOEXEC_SUSPENDED, "budget-only run suspends without clock");
        while (status == NEOEXEC_SUSPENDED && ++slices < 1000) status=worker->ResumeTop(true);
        Check(status == NEOEXEC_COMPLETED && slices > 30 && slices < 1000, "finite one-instruction slices complete");
        Check(worker->GetReturnVar() && worker->GetReturnVar()->GetType() == VAR_INT &&
            worker->GetReturnVar()->_int == 31, "retained completed slice result");
        worker->CancelExecution();
        worker->SetInstructionBudget(0);
        Check(worker->ExecuteTop(worker->FindFunction("Work"),args) == NEOEXEC_COMPLETED &&
            worker->GetVar("ticks")->_int == 62, "zero budget restores unlimited execution");
        worker->SetInstructionBudget(1);
        status=worker->ExecuteTop(worker->FindFunction("Fail"),args); slices=0;
        while(status==NEOEXEC_SUSPENDED && ++slices<100) status=worker->ResumeTop();
        Check(status==NEOEXEC_ERROR && worker->GetExecutionState()==NeoExecutionState::Idle,
            "instruction error settles at exhausted budget");
    }
    NeoVMSystem::ReleaseVM(vm); NeoExecContextPool_Destroy(pool); program->Release();
}
static void Reject(IRuntime* runtime, const std::vector<uint8_t>& image, const std::string& label)
{
    Error error;
    auto program=runtime->LoadProgram(image,&error);
    Check(!program && !error.ok() && !error.message.empty(), label + ": " + error.message);
    if(program)runtime->DestroyProgram(program);
}
static void ImageCases(IRuntime* runtime, bool debug)
{
    auto image=CompileImage(runtime,"export var x=0;export fun Probe(var n){"
        "if(n>=0 && n<=4)x=1;switch(n){case 1:x+=1;break;default:x+=2;}return x;}",debug);
    if(image.empty())return;
    SNeoVMHeader header{}; std::memcpy(&header,image.data(),sizeof(header));
    // 작은 입력이 불가능한 개수를 선언하면 큰 버퍼를 할당하기 전에 거부해야 한다.
    for(int change=0;change<6;++change)
    {
        auto bad=image;auto h=header;
        switch(change){
        case 0:h._iCodeSize=INT_MAX-7;break;
        case 1:h._iStaticVarCount=INT_MAX-h._iGlobalVarCount-1;break;
        case 2:h.m_iDebugCount=INT_MAX;break;
        case 3:h.m_iDebugCount=-1;break;
        case 4:h._iFunctionCount=SHRT_MAX+1;break;
        case 5:h._iExportVarCount=INT_MAX;break;
        }
        std::memcpy(bad.data(),&h,sizeof(h));Reject(runtime,bad,"impossible image count "+std::to_string(change));
    }
    // 코드와 첫 함수 테이블 중간에서 잘린 모든 접두 입력은 거부해야 한다.
    const size_t fixedEnd=sizeof(header)+(size_t)header._iCodeSize;
    for(size_t length=0;length<fixedEnd+19;++length)
        Reject(runtime,std::vector<uint8_t>(image.begin(),image.begin()+length),"truncated required bytes "+std::to_string(length));
    for(uint32_t magic:{0x4E445642u,0x4E44464Eu,0x54495753u,0x45474E52u})
    {
        std::vector<uint8_t> chunk(4);std::memcpy(chunk.data(),&magic,4);
        for(size_t tail=1;tail<=5;++tail)
        {
            auto bad=image;bad.insert(bad.end(),chunk.begin(),chunk.begin()+(std::min)(tail,size_t(4)));
            if(tail==5)bad.push_back(0xff);
            Reject(runtime,bad,"truncated known chunk "+std::to_string(magic)+"/"+std::to_string(tail));
        }
    }
    std::vector<SVMOperation> ops(header._iCodeSize/sizeof(SVMOperation));
    std::memcpy(ops.data(),image.data()+sizeof(header),header._iCodeSize);
    for(size_t i=2;i<ops.size();++i)
    {
        const auto op=ops[i];
        if(op.op!=NOP_MOVI && op.op!=NOP_JMP_RANGE_OUTSIDE && op.op!=NOP_SWITCH && op.op!=NOP_RETURN)continue;
        if(op.op==NOP_MOVI)
        {
            auto changed=ops;changed[i].n1=SHRT_MAX;changed[i].argFlag|=NEOS_ARG_N1_LOCAL;
            auto bad=image;std::memcpy(bad.data()+sizeof(header),changed.data(),header._iCodeSize);
            Reject(runtime,bad,"local operand beyond declared call window "+std::to_string(i));
        }
        auto changed=ops;
        switch(op.op){
        case NOP_MOVI:changed[i].n1=-1;changed[i].argFlag|=NEOS_ARG_N1_LOCAL;break;
        case NOP_SWITCH:changed[i].n1=-1;break;
        case NOP_RETURN:changed[i].op=NOP_MAX;break;
        case NOP_JMP_RANGE_OUTSIDE:changed[i].n1=SHRT_MAX;break;
        default:break;
        }
        auto bad=image;std::memcpy(bad.data()+sizeof(header),changed.data(),header._iCodeSize);
        Reject(runtime,bad,"invalid saved operand "+std::to_string(i));
    }
    // 거부된 입력 뒤에도 정상 이미지는 로드된다. 손상된 이미지는 실행하지 않는다.
    Error error;auto valid=runtime->LoadProgram(image,&error);Check(bool(valid),"valid image after rejects: "+error.message);
    if(valid)runtime->DestroyProgram(valid);
}
int main()
{
    auto runtime=CreateRuntime();runtime->FreezeBindings();
    for(bool debug:{false,true})
    {
        FrameCase(runtime,debug,"return Recur(1000,null,null,null,null,null);","named recursion");
        FrameCase(runtime,debug,"var f=Recur;return f(1000,null,null,null,null,null);","function-value recursion");
        FrameCase(runtime,debug,"var co=coroutine.create(fun(){Recur(1000,null,null,null,null,null);});"
            "coroutine.resume(co);return 0;","coroutine recursion");
        BudgetCase(runtime,debug);
        ImageCases(runtime,debug);
    }
    DestroyRuntime(runtime);
    std::printf("host_runtime_regression: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
