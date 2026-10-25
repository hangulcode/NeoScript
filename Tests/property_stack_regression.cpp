#include "NeoScript.h"
#include "NeoVMWorker.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
#include <string>
#include <vector>
using namespace NeoScript;

static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label.c_str()); }
}

struct StackState
{
    int calls = 0, firstBase = -1, lastBase = -1, peakHigh = 0;
    int child = -1;
    bool reused = true;
};
static bool InspectProperty(INeoVMWorker* base, void* data, const VMString*, VarInfo* value, bool get)
{
    auto& state = *static_cast<StackState*>(data);
    auto* worker = static_cast<CNeoVMWorker*>(base);
    if (state.calls++ == 0) state.firstBase = worker->_iSP_Vars;
    state.lastBase = worker->_iSP_Vars;
    state.reused = state.reused && state.lastBase == state.firstBase;
    if (worker->_iSP_Vars_Max2 > state.peakHigh) state.peakHigh = worker->_iSP_Vars_Max2;
    if (get && state.child >= 0)
    {
        // testCall의 반환 슬롯은 콜백 꼬리에 남는다. 외부 결과 슬롯이 소유권을 얻은 뒤 정리되어야 한다.
        VarInfo* result = worker->testCall(state.child, nullptr, 0);
        if (!result || result->GetType() != VAR_LIST) return false;
        worker->Var_Move(value, result);
        if (worker->_iSP_Vars_Max2 > state.peakHigh) state.peakHigh = worker->_iSP_Vars_Max2;
    }
    else if (get) worker->Var_SetInt(value, 1);
    return true;
}

// 재귀 없는 프로퍼티 반복에서 콜백 프레임을 재사용하는지 내부 공개 레지스터로 확인한다.
static void InspectRepeatedFrames(bool debug, bool setter, bool global, bool nested = false)
{
    const std::string body = std::string("for(var i in 0,60000){") +
        (setter ? "Host.Value=1;Result+=1;" : nested ? "Result+=Host.Value[0];" : "Result+=Host.Value;") + "}";
    const std::string source = std::string("import math;export var Result=0;") +
        "export fun Child(){return [1,\"child-\"..42,math.Vector3(1,2,3)];}" +
        (global ? body : "export fun Probe(){" + body + "}");
    const std::string label = std::string(global ? "global" : "function") + (setter ? " setter" : " getter") +
        (nested ? " testCall" : "") + " debug=" + std::to_string(debug);
    auto pool = NeoExecContextPool_Create();
    NeoLoadVMParam load; load.execPool = pool;
    NeoCompilerParam compile(source.c_str(), (int)source.size());
    const NeoGlobalSymbol symbol = { "Host", true };
    const NeoGlobalSymbolTable globals = { &symbol, 1 };
    compile.globalSymbols = &globals;
    std::string error;
    compile.err = &error;
    compile.debug = debug;
    auto vm = NeoVMSystem::CompileAndLoadVM(compile, &load);
    Check(vm != nullptr, label + " compile: " + error);
    if (vm)
    {
        StackState state;
        INeoVMWorker* worker = vm->GetMainWorker();
        if (nested) state.child = worker->FindFunction("Child");
        auto* host = worker->GetVar("Host");
        const bool bound = host && worker->ResetVarType(host, VAR_FP_NATIVE) &&
            NeoVMSystem::RegisterTableCallBack(host, &state, nullptr, InspectProperty);
        Check(bound, label + " bind");
        if (bound)
        {
            int status = worker->ExecuteN(0);
            if (!global && status == NEOEXEC_COMPLETED) status = worker->ExecuteN(worker->FindFunction("Probe"));
            Check(status == NEOEXEC_COMPLETED, label + " completes: " + vm->GetLastErrorMsg());
            Check(state.calls == 60000 && worker->GetVar("Result")->_int == 60000, label + " all accesses complete");
            Check(state.reused && state.peakHigh < 128, label + " bounded reusable callback frame");
        }
        NeoVMSystem::ReleaseVM(vm);
    }
    NeoExecContextPool_Destroy(pool);
}

struct HostState
{
    int gets = 0, sets = 0, lastValue = -1;
    bool setterIntact = true;
};
static bool Method(CallContext& context, StringView name)
{
    if (name.str() != "ThrowMethod") return false;
    auto discarded = context.retList();
    discarded.pushString("discarded-method-result");
    throw std::runtime_error("method callback exception");
}
static void ThrowPrint(const char*)
{
    throw std::runtime_error("print callback exception");
}
static bool Property(CallContext& context, StringView name, bool get)
{
    auto& state = *static_cast<HostState*>(context.userData());
    const std::string member = name.str();
    if (member == "Value")
    {
        if (get) { ++state.gets; context.retInt(1); }
        else { ++state.sets; state.lastValue = context.argInt(0); }
        return true;
    }
    const bool throwing = member == "Throw";
    const bool failure = member == "Fail" || member == "FailSet" || throwing;
    if (!get && member != "Nested" && member != "FailSet") return false;
    const std::string before = !get && member == "Nested" ? context.argString(0).str() : std::string();
    const char* child = member == "NestedClosure" ? "InvokeClosureThrow" : member == "ScriptFail" ? "ChildError" : member == "Vector" ? "ChildVector" : member == "Text" ? "ChildText" :
        member == "List" ? "ChildList" : "ChildScalar";
    {
        auto call = context.runtime()->Call(context.instance(), child);
        if (call.invoke() != RunStatus::Completed)
        {
            context.fail(91, call.error().message);
            return false;
        }
        if (!get)
        {
            if (member == "Nested") state.setterIntact = state.setterIntact && context.argString(0).str() == before;
        }
        else if (member == "Vector")
        {
            float value[4]; call.retVec(value);
            context.retVec3(value[0], value[1], value[2]);
        }
        else if (member == "Text") context.retString(call.retString());
        else if (member == "List")
        {
            ListReader values;
            int32_t number = 0;
            StringView text;
            float vector[4];
            if (!call.retList(values) || !values.getInt(0, number) || !values.getString(1, text) || !values.getVec(2, vector))
            {
                context.fail(92, "invalid child list");
                return false;
            }
            auto result = context.retList();
            result.pushInt(number);
            result.pushString(text);
            result.pushVec3(vector[0], vector[1], vector[2]);
        }
        else if (failure && get)
        {
            auto discarded = context.retList();
            discarded.pushString("discarded-property-result");
            discarded.pushVec3(1, 2, 3);
        }
        else if (get) context.retInt(call.retInt());
    }
    if (!get && member == "Nested") state.setterIntact = state.setterIntact && context.argString(0).str() == before;
    if (throwing) throw std::runtime_error("property callback exception");
    if (failure) { context.fail(93, "property failure"); return false; }
    return true;
}

static const char* source = R"NS(
import math;
import coroutine;
export var InitResult=0;
for(var i in 0,60000){InitResult+=Host.Value;Host.Value=i;}
export fun GetLoop(){var total=0;for(var i in 0,60000)total+=Host.Value;return total;}
export fun SetLoop(){for(var i in 0,60000)Host.Value=i;return true;}
export fun ChildScalar(){var allocated=["temporary-"..42,math.Vector3(1,2,3)];return allocated.len()+5;}
export fun ChildVector(){return math.Vector3(3,4,5);}
export fun ChildText(){return "nested-"..42;}
export fun ChildList(){return [3,"nested-"..42,math.Vector3(3,4,5)];}
export fun ChildError(){var allocated=["error-"..42,math.Vector3(1,2,3)];var zero=0;return allocated.len()/zero;}
fun Pack(var a,var b,var c){return a*100+b*10+c;}
fun Keep(var a,var b,var c){return a=="anchor42"&&b==7&&c[0]==3;}
export fun PendingArguments(){
    var n=1;
    return Pack(n+1,Host.Child,n+2)==273 && Keep("anchor"..42,Host.Child,Host.Vector);
}
export fun AllocatedResults(){
    var v=Host.Vector;var text=Host.Text;var list=Host.List;
    var again=Host.Vector;
    return v==math.Vector3(3,4,5)&&text=="nested-42"&&list[0]==3&&list[1]=="nested-42"&&list[2]==again;
}
export fun NestedLoop(){var total=0;for(var i in 0,60000)total+=Host.Child;return total;}
export fun NestedSetter(){for(var i in 0,100)Host.Nested="anchor-"..i;return true;}
export fun FailGet(){return Host.Fail;}
export fun FailSet(){Host.FailSet=["anchor",math.Vector3(1,2,3)];}
export fun ThrowGet(){return Host.Throw;}
export fun ScriptFail(){return Host.ScriptFail;}
export var Captured;
export fun PrepareClosure(var variant){
    var n=7;
    Captured=fun(var shouldThrow){
        n+=1;
        if(shouldThrow){
            if(variant==0){var ignored=Host.Throw;}
            else if(variant==1)Host.ThrowMethod();
            else print("native-print-exception");
        }
        return n;
    };
}
export fun InvokeClosure(var shouldThrow){return Captured(shouldThrow);}
export fun InvokeClosureThrow(){return Captured(true);}
export fun NestedClosureFailure(){return Host.NestedClosure;}
export var Comparator;
var ThrowComparator=false;
export fun PrepareComparator(){
    var n=7;
    var payload=[math.Vector3(1,2,3),"capture-"..42];
    Comparator=fun(var a,var b){
        n+=1;var retained=payload;
        if(ThrowComparator)Host.ThrowMethod();
        return n;
    };
}
fun NamedComparator(var a,var b){return Comparator(a,b);}
export fun FailListSort(){ThrowComparator=true;var values=[2,1];values.sort(Comparator);}
export fun FailMapSort(){ThrowComparator=true;var values={"a":2,"b":1};values.sort(Comparator);}
export fun FailNamedListSort(){ThrowComparator=true;var values=[2,1];values.sort(NamedComparator);}
export fun FailNamedMapSort(){ThrowComparator=true;var values={"a":2,"b":1};values.sort(NamedComparator);}
export fun RetryComparator(){ThrowComparator=false;return Comparator(1,2);}
export fun ClearComparator(){Comparator=null;}
export var CoroutineResult=0;
fun CoroutineBody(){for(var i in 0,60000)CoroutineResult+=Host.Value;}
export fun CoroutineLoop(){
    CoroutineResult=0;var co=coroutine.create(CoroutineBody);coroutine.resume(co);
    return CoroutineResult;
}
)NS";

static bool SameObjects(const AllocStats& a, const AllocStats& b)
{
    return a.strings == b.strings && a.maps == b.maps && a.lists == b.lists && a.sets == b.sets &&
        a.coroutines == b.coroutines && a.asyncs == b.asyncs && a.vectors == b.vectors &&
        a.arrays == b.arrays && a.closures == b.closures && a.bufferBytes == b.bufferBytes;
}

// 공개 API 경로에서도 이미지 재로드, 중첩 호출, 실패 복구와 할당된 반환값 수명을 함께 검사한다.
static void RunFacade(bool debug, bool image)
{
    AllocStats before; GetAllocStats(before);
    HostState state;
    auto runtime = CreateRuntime();
    NativeObjectDesc host;
    host.name = "Host"; host.property = Property; host.method = Method; host.userData = &state;
    Check(runtime->RegisterObject(host), "register property host");
    runtime->FreezeBindings();
    CompileDesc desc;
    desc.source = source; desc.sourceName = "property-stack.ns"; desc.includeDebugInfo = debug;
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
        auto result = runtime->Compile(desc); program = result.program; error = result.error;
    }
    const std::string mode = " debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(bool(program), "compile" + mode + ": " + error.message);
    if (program)
    {
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), "global initialization completes" + mode);
        if (instance)
        {
            int32_t initialized = 0;
            Check(runtime->GetGlobalInt(instance, "InitResult", initialized) && initialized == 60000 &&
                state.gets == 60000 && state.sets == 60000 && state.lastValue == 59999, "global accesses" + mode);
            for (const char* name : {"GetLoop", "SetLoop", "PendingArguments", "AllocatedResults", "NestedLoop", "NestedSetter", "CoroutineLoop"})
            {
                const int gets = state.gets, sets = state.sets;
                auto call = runtime->Call(instance, name);
                const auto status = call.invoke();
                const std::string function = name;
                Check(status == RunStatus::Completed, function + mode + ": " + call.error().message);
                if (status != RunStatus::Completed) continue;
                if (function == "GetLoop" || function == "CoroutineLoop")
                    Check(call.retInt() == 60000 && state.gets - gets == 60000, function + " result" + mode);
                else if (function == "NestedLoop") Check(call.retInt() == 420000, function + " result" + mode);
                else Check(call.retBool(), function + " result" + mode);
                if (function == "SetLoop") Check(state.sets - sets == 60000 && state.lastValue == 59999, "setter result" + mode);
                if (function == "NestedSetter") Check(state.setterIntact, "setter argument survives child invocation" + mode);
            }
            for (const char* name : {"FailGet", "FailSet", "ThrowGet", "ScriptFail"})
            {
                AllocStats beforeFailure; GetAllocStats(beforeFailure);
                {
                    auto call = runtime->Call(instance, name);
                    const std::string reason = std::string(name) == "ThrowGet" ? "exception" :
                        std::string(name) == "ScriptFail" ? "integer division by zero" : "property failure";
                    Check(call.invoke() == RunStatus::Failed && call.error().message.find(reason) != std::string::npos,
                        std::string(name) + " preserves failure" + mode);
                }
                {
                    auto recovery = runtime->Call(instance, "AllocatedResults");
                    Check(recovery.invoke() == RunStatus::Completed && recovery.retBool(), "property failure recovery" + mode);
                }
                AllocStats afterFailure; GetAllocStats(afterFailure);
                Check(SameObjects(beforeFailure, afterFailure), std::string(name) + " releases temporary objects before instance destruction" + mode);
            }
            for (int variant = 0; variant < 3; ++variant)
            {
                const std::string callback = variant == 0 ? "property" : variant == 1 ? "method" : "print";
                {
                    auto prepare = runtime->Call(instance, "PrepareClosure");
                    Check(prepare.argInt(variant).invoke() == RunStatus::Completed, "prepare captured " + callback + " failure" + mode);
                }
                {
                    auto throwing = runtime->Call(instance, "InvokeClosure");
                    if (variant == 2) SetLogHandler(ThrowPrint, nullptr);
                    const auto status = throwing.argBool(true).invoke();
                    if (variant == 2) SetLogHandler(nullptr, nullptr);
                    Check(status == RunStatus::Failed && throwing.error().message.find("exception") != std::string::npos,
                        "captured " + callback + " exception" + mode);
                }
                {
                    auto retry = runtime->Call(instance, "InvokeClosure");
                    const auto status = retry.argBool(false).invoke();
                    const int value = status == RunStatus::Completed ? retry.retInt() : -1;
                    Check(status == RunStatus::Completed && value == 9,
                        "captured local survives " + callback + " exception" + mode + " actual=" + std::to_string(value) + ": " + retry.error().message);
                }
            }
            {
                auto prepare = runtime->Call(instance, "PrepareClosure");
                Check(prepare.argInt(1).invoke() == RunStatus::Completed, "prepare nested captured failure" + mode);
            }
            {
                auto throwing = runtime->Call(instance, "NestedClosureFailure");
                Check(throwing.invoke() == RunStatus::Failed && throwing.error().message.find("exception") != std::string::npos,
                    "property named child closure exception" + mode);
            }
            {
                auto retry = runtime->Call(instance, "InvokeClosure");
                const auto status = retry.argBool(false).invoke();
                const int value = status == RunStatus::Completed ? retry.retInt() : -1;
                Check(status == RunStatus::Completed && value == 9,
                    "captured local survives property named child exception" + mode + " actual=" + std::to_string(value) + ": " + retry.error().message);
            }
            // sort의 두 testCall 경로도 실패한 비교자의 캡처와 참조를 잃지 않아야 한다.
            for (const char* name : {"FailListSort", "FailMapSort", "FailNamedListSort", "FailNamedMapSort"})
            {
                AllocStats beforeSort; GetAllocStats(beforeSort);
                const std::string label = std::string(name) + mode;
                {
                    auto prepare = runtime->Call(instance, "PrepareComparator");
                    Check(prepare.invoke() == RunStatus::Completed, "prepare " + label);
                }
                {
                    auto throwing = runtime->Call(instance, name);
                    Check(throwing.invoke() == RunStatus::Failed && throwing.error().message.find("exception") != std::string::npos,
                        "comparator failure " + label);
                }
                {
                    auto retry = runtime->Call(instance, "RetryComparator");
                    const auto status = retry.invoke();
                    const int value = status == RunStatus::Completed ? retry.retInt() : -1;
                    Check(status == RunStatus::Completed && value == 9,
                        "comparator capture after failure " + label + " actual=" + std::to_string(value) + ": " + retry.error().message);
                }
                {
                    auto clear = runtime->Call(instance, "ClearComparator");
                    Check(clear.invoke() == RunStatus::Completed, "clear comparator " + label);
                }
                AllocStats afterSort; GetAllocStats(afterSort);
                Check(SameObjects(beforeSort, afterSort), "comparator objects released " + label);
            }
            runtime->DestroyInstance(instance);
        }
        runtime->DestroyProgram(program);
    }
    DestroyRuntime(runtime);
    AllocStats after; GetAllocStats(after);
    Check(SameObjects(before, after), "property temporary objects released" + mode);
}

struct FailedCallState
{
    int child = -1;
    int calls = 0;
    bool closure = false;
    bool swallowed = false;
};

static bool SwallowFailure(INeoVMWorker* base, void* data, const VMString*, VarInfo* value, bool get)
{
    if (!get) return false;
    auto& state = *static_cast<FailedCallState*>(data);
    auto* worker = static_cast<CNeoVMWorker*>(base);
    ++state.calls;
    VarInfo* result = state.closure ? worker->testCall(base->GetVar("Callback"), nullptr, 0)
                                   : worker->testCall(state.child, nullptr, 0);
    state.swallowed = result == nullptr;
    // 호스트가 실패를 삼키고 성공을 반환해도 바깥 스크립트를 재개하면 안 된다.
    worker->Var_SetInt(value, 1);
    return true;
}

static void FailedTestCall(bool debug, bool closure)
{
    const std::string source =
        "export var After=0; export var Callback;"
        "export fun Child(){var z=0; return 1/z;}"
        "export fun Setup(){var z=0; Callback=fun(){return 1/z;};}"
        "fun Inner(){for(var i in 0,100){var value=Host.Value;} After=1;}"
        "export fun Outer(){Inner(); After=2;}"
        "export fun Recover(){return 7;}";
    auto pool = NeoExecContextPool_Create();
    NeoLoadVMParam load; load.execPool = pool;
    NeoCompilerParam compile(source.c_str(), (int)source.size());
    const NeoGlobalSymbol symbol = { "Host", true };
    const NeoGlobalSymbolTable globals = { &symbol, 1 };
    compile.globalSymbols = &globals;
    compile.debug = debug;
    std::string error; compile.err = &error;
    auto vm = NeoVMSystem::CompileAndLoadVM(compile, &load);
    Check(vm != nullptr, "testCall failure compile: " + error);
    if (vm)
    {
        INeoVMWorker* worker = vm->GetMainWorker();
        FailedCallState state; state.child = worker->FindFunction("Child"); state.closure = closure;
        auto* host = worker->GetVar("Host");
        const bool bound = host && worker->ResetVarType(host, VAR_FP_NATIVE) &&
            NeoVMSystem::RegisterTableCallBack(host, &state, nullptr, SwallowFailure);
        Check(bound, "testCall failure bind");
        if (bound)
        {
            Check(worker->ExecuteN(0) == NEOEXEC_COMPLETED, "testCall failure init");
            Check(worker->ExecuteN(worker->FindFunction("Setup")) == NEOEXEC_COMPLETED, "testCall closure setup");
            Check(worker->ExecuteN(worker->FindFunction("Outer")) == NEOEXEC_ERROR, "swallowed testCall still fails outer execution");
            Check(state.swallowed && state.calls == 1, "failed testCall never repeats host call");
            Check(worker->GetVar("After")->_int == 0, "failed testCall skips caller continuation");
            Check(std::string(vm->GetLastErrorMsg()).find("division by zero") != std::string::npos, "original error retained");
            Check(worker->ExecuteN(worker->FindFunction("Recover")) == NEOEXEC_COMPLETED, "worker reusable after testCall failure");
        }
        NeoVMSystem::ReleaseVM(vm);
    }
    NeoExecContextPool_Destroy(pool);
}

static int ThrowFunctionValue(INeoVMWorker*, FunctionPtr*, short)
{
    throw std::runtime_error("native function value exception");
}
static void FunctionValueException(bool debug, bool nested)
{
    const std::string source =
        "export var Cb;export var Probe;export var Out;"
        "export fun SetCb(var f){Cb=f;}"
        "export fun Prepare(){var n=7;Probe=fun(var fail){n+=1;if(fail)Cb(123,456);return n;};}"
        "export fun Invoke(var fail){Out=Probe(fail);}"
        "export fun Child(){Invoke(true);}"
        "export fun Outer(){Out=Host.Value;}";
    auto pool = NeoExecContextPool_Create();
    NeoLoadVMParam load; load.execPool = pool;
    NeoCompilerParam compile(source.c_str(), (int)source.size());
    const NeoGlobalSymbol symbol = {"Host", true}; const NeoGlobalSymbolTable globals = {&symbol,1};
    compile.globalSymbols=&globals; compile.debug=debug;
    std::string error; compile.err=&error;
    auto vm=NeoVMSystem::CompileAndLoadVM(compile,&load);
    Check(vm!=nullptr, "function value exception compile: "+error);
    if(vm)
    {
        INeoVMWorker* w=vm->GetMainWorker();
        VarInfo wide;long narrow=0;
        w->Var_SetInt(&wide,INT64_MAX);w->_read(&wide,narrow);
        Check(narrow==(std::numeric_limits<long>::max)(),"long read upper bound");
        w->Var_SetInt(&wide,INT64_MIN);w->_read(&wide,narrow);
        Check(narrow==(std::numeric_limits<long>::min)(),"long read lower bound");
        FailedCallState state;state.child=w->FindFunction("Child");
        auto host=w->GetVar("Host");w->ResetVarType(host,VAR_FP_NATIVE);
        NeoVMSystem::RegisterTableCallBack(host,&state,nullptr,SwallowFailure);
        Check(w->ExecuteN(0)==NEOEXEC_COMPLETED,"function value init");
        NeoFunction fn;fn._pWorker=nullptr;fn._fun_index=-1;
        fn._fun._argCount=2;fn._fun._fn=ThrowFunctionValue;fn._fun._func=&state;
        Check(w->ExecuteN(w->FindFunction("SetCb"),fn)==NEOEXEC_COMPLETED,"function value binding");
        Check(w->ExecuteN(w->FindFunction("Prepare"))==NEOEXEC_COMPLETED,"function value captures");
        Check(w->ExecuteN(w->FindFunction("Invoke"),false)==NEOEXEC_COMPLETED && w->GetVar("Out")->_int==8,"capture before exception");
        const int status=nested?w->ExecuteN(w->FindFunction("Outer")):w->ExecuteN(w->FindFunction("Invoke"),true);
        Check(status==NEOEXEC_ERROR,"function value exception reported");
        Check(w->ExecuteN(w->FindFunction("Invoke"),false)==NEOEXEC_COMPLETED && w->GetVar("Out")->_int==10,"capture preserved after exception");
        NeoVMSystem::ReleaseVM(vm);
    }
    NeoExecContextPool_Destroy(pool);
}

int main()
{
    NeoVMSystem::Initialize();
    for (bool debug : {false, true}) for (bool nested : {false, true}) FunctionValueException(debug, nested);
    for (bool debug : {false, true}) for (bool closure : {false, true}) FailedTestCall(debug, closure);
    for (bool debug : {false, true})
        for (bool setter : {false, true})
            for (bool global : {false, true}) InspectRepeatedFrames(debug, setter, global);
    for (bool debug : {false, true}) InspectRepeatedFrames(debug, false, false, true);
    for (bool debug : {false, true}) for (bool image : {false, true}) RunFacade(debug, image);
    std::printf("property_stack_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
