#include "NeoScript.h"
#include "NeoVM.h"
#include "NeoVMInternal.h"
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>
using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label); }
}
struct State {
    ObjectType object;
    InstanceHandle peer;
    InstanceHandle caller;
    std::unique_ptr<Invocation> kept;
    std::unique_ptr<Invocation> resuming;
    int initCalls = 0;
    int callbacks = 0;
};
static State* activeState;
static bool ObjectMethod(CallContext& ctx, StringView name) {
    if (name.str() == "Take") {
        const FunctionHandle function = ctx.argFunction(0);
        Check(!ctx.runtime()->Call(ctx.instance(),function),"borrower function cannot be called in source program");
        auto call=ctx.runtime()->Call(activeState->caller,function);
        if(call.invoke()!=RunStatus::Completed){ctx.fail(1,call.error().message);return false;}
        ctx.retInt(call.retInt());return true;
    }
    if (name.str() == "Fail") {ctx.fail(827,"shared object failure");return false;}
    if (name.str() != "Ping") return false;
    Check(ctx.instance().id == activeState->peer.id, "shared native object keeps source identity");
    Check(!ctx.runtime()->CanDestroyInstance(ctx.instance()), "shared native callback protects source");
    ctx.runtime()->DestroyInstance(ctx.instance());
    Check(ctx.runtime()->IsAlive(ctx.instance()), "shared native source destruction refused");
    ctx.retInt(static_cast<int>(reinterpret_cast<intptr_t>(ctx.userData())));
    return true;
}
static bool HostMethod(CallContext& ctx, StringView name) {
    auto& state = *static_cast<State*>(ctx.userData());
    const std::string method = name.str();
    if (method == "Init") {
        ++state.initCalls;
        Check(!ctx.runtime()->CanDestroy(), "global initializer protects runtime");
        return true;
    }
    if (method == "Guard") {
        auto rt = ctx.runtime(); auto inst = ctx.instance();
        Check(!rt->CanDestroyInstance(inst) && !rt->CanDestroy(), "executing instance cannot be destroyed");
        Check(!rt->ResetInstance(inst), "callback reset refused");
        rt->DestroyInstance(inst); DestroyRuntime(rt);
        Check(rt->IsAlive(inst), "callback destroy leaves instance and runtime alive");
        Check(rt->Resume(inst) == RunStatus::Failed, "callback resume refused");
        Check(rt->UpdateSliced(inst) == RunStatus::Failed, "callback sliced resume refused");
        Check(rt->GetDebugger()->Run(inst) == RunStatus::Failed, "callback debugger run refused");
        Check(rt->RunGlobalInit(inst) == RunStatus::Failed, "callback global initializer refused");
        Check(!rt->StartSliced(inst, "Child", -1, 0), "callback sliced start refused");
        Check(!rt->Cancel(inst), "callback cancel refused before mutation");
        ctx.retInt(73); return true;
    }
    if (method == "Args") {
        Check(ctx.argCount() == 1 && ctx.argInt(0) == 9, "valid native argument");
        for (size_t i : {size_t(1), size_t(100000), std::numeric_limits<size_t>::max()}) {
            MapReader map; ListReader list; ArrayView array; float vec[4] = {1,1,1,1};
            ctx.argVec(i, vec);
            Check(ctx.argType(i) == ValueType::None && ctx.argInt(i) == 0 && ctx.argFloat(i) == 0 &&
                !ctx.argBool(i) && ctx.argString(i).empty() && !ctx.argIsNativeObject(i) &&
                !ctx.argFunction(i) && !ctx.argAsMap(i,map) && !ctx.argAsList(i,list) &&
                !ctx.argAsArray(i,array) && !ctx.argObjectUserData(i) && vec[0] == 0 && vec[3] == 0,
                "out-of-range native argument defaults");
        }
        ctx.retInt(9); return true;
    }
    if (method == "KeepChild") {
        state.kept.reset(new Invocation(ctx.runtime()->Call(ctx.instance(), "Child")));
        const auto status = state.kept->invoke();
        if (status != RunStatus::Completed) { ctx.fail(1,state.kept->error().message); return false; }
        ctx.retInt(state.kept->retInt()); ++state.callbacks; return true;
    }
    if (method == "Nested") {
        auto call = ctx.runtime()->Call(ctx.instance(), "Child");
        if (call.invoke() != RunStatus::Completed) { ctx.fail(1,call.error().message); return false; }
        ctx.retInt(call.retInt()); ++state.callbacks; return true;
    }
    if (method == "DropOwner") {state.resuming.reset();ctx.retInt(99);return true;}
    if (method == "MakeObj") { ctx.retObject(state.object, reinterpret_cast<void*>(4242)); return true; }
    if (method == "Build") {
        auto map = ctx.retMap();
        map.setList("list").pushObject(state.object,reinterpret_cast<void*>(4242));
        map.setObject("direct",state.object,reinterpret_cast<void*>(4242));
        return true;
    }
    if (method == "Peer") { ctx.retInstanceGlobal(state.peer,ctx.argString(0)); return true; }
    return false;
}
static bool HostProperty(CallContext& ctx, StringView, bool get) {
    auto& state = *static_cast<State*>(ctx.userData());
    Check(ctx.argCount() == (get ? 0u : 1u), "property argument count");
    Check(ctx.argType(get ? 0 : 1) == ValueType::None, "property arguments bounded");
    if (get) ctx.retInstanceGlobal(state.peer,"Score");
    return true;
}
static const char* source = R"NS(
export var Score=77;
export var Text="initial";
export var InitCount=0;
InitCount+=1;Host.Init();
export var Entity=null;
export var Payload=null;
export var Plain={"v":7};
var held=null;var plain=null;
export fun Add(var a,var b){return a+b;}
export fun Sub(var a,var b){return a-b;}
export fun Child(){return 7;}
export fun Guard(){return Host.Guard();}
export fun Args(){return Host.Args(9);}
export fun Echo(var text){return text;}
export fun Getter(){return Host.score;}
export fun KeepChild(){return Host.KeepChild();}
export fun Nested(){return Host.Nested();}
export fun SleepZero(){sleep(0);return 1;}
export fun Spin(var n){var s=0;for(var i in 0,n)s+=1;return s;}
export fun SpinVoid(){var s=0;for(var i in 0,10000)s+=1;Score=s;}
export fun DelayedInt(){var s=0;for(var i in 0,100)s+=1;return 71;}
export fun DelayedString(){var s=0;for(var i in 0,100)s+=1;return Text;}
export fun DelayedMap(){var s=0;for(var i in 0,100)s+=1;return {"v":23};}
export fun ReaderMap(){return {"child":{"v":31},"list":[41]};}
export fun ReaderList(){return [[51],{"v":61}];}
export fun DelayedNested(){var s=0;for(var i in 0,100)s+=1;return Host.Nested()+37;}
export fun DelayedKeptChild(){var s=0;for(var i in 0,100)s+=1;return Host.KeepChild()+37;}
export fun DelayedDropOwner(){var s=0;for(var i in 0,100)s+=1;return Host.DropOwner();}
export fun DelayedFail(){var s=0;for(var i in 0,100)s+=1;var z=0;return 1/z;}
export fun Prepare(){Entity=Host.MakeObj();Payload=Host.Build();return Entity.Ping();}
export fun Grab(){held=Host.Peer("Payload");plain=Host.Peer("Plain");return held.list[0].Ping()+held.direct.Ping();}
export fun Use(){return held.list[0].Ping();}
export fun ReadPlain(){return plain.v;}
export fun Drop(){held=null;plain=null;}
)NS";
static ProgramHandle Compile(IRuntime* rt, const char* text, bool debug, bool image) {
    CompileDesc desc; desc.source=text;desc.sourceName="host-api.ns";desc.includeDebugInfo=debug;
    ProgramHandle program;Error error;
    if(image){std::vector<uint8_t> bytes;error=rt->CompileToBytecode(desc,bytes);if(error.ok())program=rt->LoadProgram(bytes,&error);}
    else{auto result=rt->Compile(desc);program=result.program;error=result.error;}
    Check(bool(program),"compile host API regression");
    if(!program)std::fprintf(stderr,"%s\n",error.message.c_str());
    return program;
}
static void Run(bool debug,bool image) {
    State state;activeState=&state;
    auto rt=CreateRuntime();
    NativeObjectDesc host;host.name="Host";host.method=HostMethod;host.property=HostProperty;host.userData=&state;
    Check(rt->RegisterObject(host),"register Host");
    Check(!rt->RegisterObject(host),"duplicate object rejected");
    NativeObjectDesc object;object.name="Obj";object.method=ObjectMethod;object.declareGlobal=false;
    Check(rt->RegisterObject(object),"register object");
    state.object=rt->GetObjectType("Obj");
    for(int i=0;i<96;++i){const std::string name="Extra"+std::to_string(i);object.name=name;Check(rt->RegisterObject(object),"grow binding registry");}
    rt->FreezeBindings();
    auto program=Compile(rt,source,debug,image);
    if(!program){DestroyRuntime(rt);return;}
    auto inst=rt->CreateInstance(program);state.peer=inst;
    Check(rt->GetDebugger()->Run(inst)==RunStatus::Failed,"idle debugger run safe");
    Check(rt->Call(inst,"Guard").invokeR().asInt()==73,"guarded callback result preserved");
    Check(rt->Call(inst,"Args").invokeR().asInt()==9,"argument defaults preserve normal call");
    Check(rt->Call(inst,"Getter").invokeR().asInt()==77,"cross-instance global property return");
    {
        auto a=rt->Call(inst,"Add");
        Check(bool(a)&&a.retType()==ValueType::None&&a.retInt()==0&&a.retFloat()==0&& !a.retBool()&&a.retString().empty(),"pre-invoke result defaults");
        Check(!rt->CanDestroyInstance(inst)&&!rt->ResetInstance(inst),"armed Invocation protects instance");
        Check(a.argInt(1).argInt(2).invoke()==RunStatus::Completed&&a.retInt()==3,"first invocation result");
        auto b=rt->Call(inst,"Sub");b.argInt(10).argInt(3);
        Check(a.retInt()==0&&a.retType()==ValueType::None,"flushed result defaults");
        a.argString("ignored").timeout(0).budget(1);
        Check(a.invoke()==RunStatus::Failed,"one-shot invocation rejects repeat");
        Check(b.invoke()==RunStatus::Completed&&b.retInt()==7,"stale invocation does not consume or mutate next call");
        Check(!rt->CanDestroyInstance(inst),"completed Invocation still protects instance");
    }
    Check(rt->CanDestroyInstance(inst),"all Invocation leases released");
    {
        auto call=rt->Call(inst,"ReaderMap");MapReader root,child,again;ListReader list;int32_t value=0;
        Check(call.invoke()==RunStatus::Completed&&call.retMap(root)&&root.getMap("child",child)&&root.getList("list",list),"nested map return readers");
        Check(call.retMap(again)&&child.getInt("v",value)&&value==31&&list.getInt(0,value)&&value==41,"repeated retMap preserves existing descendant readers");
    }
    {
        auto call=rt->Call(inst,"ReaderList");ListReader root,child,again;MapReader map;int32_t value=0;
        Check(call.invoke()==RunStatus::Completed&&call.retList(root)&&root.getList(0,child)&&root.getMap(1,map),"nested list return readers");
        Check(call.retList(again)&&child.getInt(0,value)&&value==51&&map.getInt("v",value)&&value==61,"repeated retList preserves existing descendant readers");
    }
    const std::string embedded("a\0b\0c",5);
    Check(rt->SetGlobalString(inst,"Text",embedded),"set NUL string");
    StringView global;Check(rt->GetGlobalString(inst,"Text",global)&&global.str()==embedded,"global string byte length");
    Check(rt->Call(inst,"Echo").argString(embedded).invokeR().asString().str()==embedded,"owned string byte length");
    {auto call=rt->Call(inst,"Echo");call.argString(embedded);Check(call.invoke()==RunStatus::Completed&&call.retString().str()==embedded,"borrowed string byte length");}
    {auto call=rt->Call(inst,"KeepChild");Check(call.invoke()==RunStatus::Completed&&call.retInt()==7,"escaped nested Invocation call");}
    state.kept.reset(); // F7 재현: 바깥 호출 반환 뒤, 다음 호출 전에 자식 Invocation을 파괴한다.
    Check(rt->Call(inst,"SleepZero").invokeR().status==RunStatus::Completed,"escaped child destroyed before later sleep");
    Check(rt->Call(inst,"Spin").argInt(10).timeout(1000).invokeR().asInt()==10,"escaped child does not poison later timeout");
    {
        auto call=rt->Call(inst,"Spin");Check(call.argInt(10000).budget(4).invoke()==RunStatus::Suspended,"instruction budget alone suspends");
        Check(!rt->Call(inst,"Child"),"new call cannot replace suspended result owner");
        Check(rt->RunGlobalInit(inst)==RunStatus::Failed&&call.status()==RunStatus::Suspended,"global init cannot replace suspended execution or limits");
        ResumeDesc limited;limited.instructionBudget=4;
        Check(rt->Resume(inst,limited)==RunStatus::Suspended,"Resume budget applied");
        Check(rt->Resume(inst)==RunStatus::Completed&&call.retInt()==10000,"zero Resume budget completes and retains result");
    }
    for(const char* name:{"DelayedInt","DelayedString","DelayedMap"}){
        auto call=rt->Call(inst,name);Check(call.budget(4).invoke()==RunStatus::Suspended,"delayed result suspends");
        Check(rt->Resume(inst)==RunStatus::Completed,"delayed result resumes");
        if(std::string(name)=="DelayedInt")Check(call.retInt()==71,"resumed int return");
        if(std::string(name)=="DelayedString")Check(call.retString().str()==embedded,"resumed string return");
        if(std::string(name)=="DelayedMap"){MapReader map;int32_t value=0;Check(call.retMap(map)&&map.getInt("v",value)&&value==23,"resumed map return");}
    }
    for(const char* name:{"DelayedNested","DelayedKeptChild"}){
        auto call=rt->Call(inst,name);Check(call.budget(4).invoke()==RunStatus::Suspended,"nested resumed result suspends");
        Check(rt->Resume(inst)==RunStatus::Completed&&call.status()==RunStatus::Completed&&call.retInt()==44,"resumed nested child preserves original result owner");
        state.kept.reset();Check(call.retInt()==44,"escaped resumed child destruction preserves outer result");
    }
    state.resuming.reset(new Invocation(rt->Call(inst,"DelayedDropOwner")));
    Check(state.resuming->budget(4).invoke()==RunStatus::Suspended,"callback-owned suspended invocation");
    Check(rt->Resume(inst)==RunStatus::Completed&&!state.resuming&&rt->GetState(inst)==InstanceState::Idle,"owner destruction during Resume releases completed context");
    {auto call=rt->Call(inst,"DelayedInt");Check(call.budget(4).invoke()==RunStatus::Suspended,"unowned suspended call");}
    Check(rt->Resume(inst)==RunStatus::Completed&&rt->GetState(inst)==InstanceState::Idle,"destroyed suspended Invocation does not retain context");
    {auto call=rt->Call(inst,"DelayedFail");Check(call.budget(4).invoke()==RunStatus::Suspended,"delayed failure suspends");Check(rt->Resume(inst)==RunStatus::Failed&&call.retType()==ValueType::None&&call.retInt()==0,"resumed failure cleans result");}
    {auto call=rt->Call(inst,"Spin");Check(call.argInt(10000).timeout(0).budget(4).invoke()==RunStatus::Suspended,"timeout and budget suspend");Check(rt->Cancel(inst),"cancel suspended invocation");}
    Check(rt->Call(inst,"Nested").invokeR().asInt()==7,"fresh top-level call resets previous limits");
    {auto call=rt->Call(inst,"Nested");Check(call.budget(100000).invoke()==RunStatus::Failed,"user budget preserves synchronous callback restriction");}
    Check(rt->Call(inst,"Nested").invokeR().asInt()==7,"failed limited call does not poison defaults");
    Check(rt->StartSliced(inst,"SpinVoid",-1,4),"StartSliced accepts budget without timeout");
    Check(rt->UpdateSliced(inst)==RunStatus::Suspended,"UpdateSliced retains configured budget");
    Check(rt->Resume(inst)==RunStatus::Completed,"Resume can remove sliced budget");
    Check(rt->Call(inst,"Nested").invokeR().asInt()==7,"StartSliced limits do not leak");
    // 디버거 실행 분할은 네이티브의 스크립트 호출을 허용하며 중단점 없이도 재개된다.
    auto debugProgram=Compile(rt,"export var Result=0;var n=0;for(var i in 0,100)n+=1;Result=Host.Nested();for(var j in 0,100)n+=1;export fun Child(){return 7;}",debug,image);
    InstanceDesc deferred;deferred.runGlobalInit=false;
    auto debugInst=rt->CreateInstance(debugProgram,deferred);auto debugger=rt->GetDebugger();
    debugger->SetSliceBudget(debugInst,4);
    auto debugStatus=rt->RunGlobalInit(debugInst);int debugSlices=0;
    while(debugStatus==RunStatus::Suspended&&debugSlices++<1000)debugStatus=debugger->Run(debugInst);
    int32_t debugResult=0;
    Check(debugStatus==RunStatus::Completed&&debugSlices>0&&rt->GetGlobalInt(debugInst,"Result",debugResult)&&debugResult==7,"debugger scheduling budget permits callback and completes");
    rt->DestroyInstance(debugInst);rt->DestroyProgram(debugProgram);
    // 두 프로그램 표가 모두 1부터 시작해도 런타임 식별자가 충돌하지 않는다.
    auto other=CreateRuntime();other->FreezeBindings();auto otherProgram=Compile(other,"export fun Wrong(){return -99;}",debug,image);
    auto otherInst=other->CreateInstance(otherProgram);auto foreign=rt->FindFunction(program,"Child");
    Check(!other->Call(otherInst,foreign),"foreign runtime function rejected");DestroyRuntime(other);
    // 초기화는 최초 생성 시 지정한 전역 초기화 옵션을 재현한다.
    InstanceDesc noInit;noInit.runGlobalInit=false;const int initBefore=state.initCalls;
    auto noInitInst=rt->CreateInstance(program,noInit);Check(rt->ResetInstance(noInitInst)&&state.initCalls==initBefore,"reset preserves disabled global init");rt->DestroyInstance(noInitInst);
    // 공유 컨테이너가 네이티브 바인딩을 보존하며 원본 초기화와 파괴는 네이티브 호출만 무효화한다.
    auto borrower=rt->CreateInstance(program);state.peer=inst;
    Check(rt->Call(inst,"Prepare").invokeR().asInt()==4242,"pre-freeze object type remains stable");
    Check(rt->Call(borrower,"Grab").invokeR().asInt()==8484,"native objects shared through map and list");
    auto callerProgram=Compile(rt,"export fun Callback(){return 911;} export fun Pass(){var o=Host.Peer(\"Entity\");return o.Take(Callback);} export fun PassClosure(){var x=912;var o=Host.Peer(\"Entity\");return o.Take(fun(){return x;});} export fun Fail(){var o=Host.Peer(\"Entity\");return o.Fail();}",debug,image);
    state.caller=rt->CreateInstance(callerProgram);
    Check(rt->Call(state.caller,"Pass").invokeR().asInt()==911,"shared native function argument belongs to executing program");
    Check(rt->Call(state.caller,"PassClosure").invokeR().asInt()==912,"shared native closure argument belongs to executing instance");
    {auto call=rt->Call(state.caller,"Fail");Check(call.invoke()==RunStatus::Failed&&call.error().code==827,"shared native custom failure belongs to executing instance");}
    rt->DestroyInstance(state.caller);rt->DestroyProgram(callerProgram);
    Check(rt->ResetInstance(inst),"source reset");
    {auto call=rt->Call(borrower,"Use");Check(call.invoke()==RunStatus::Failed&&call.error().message.find("no longer alive")!=std::string::npos,"reset invalidates old shared native binding");}
    Check(rt->Call(borrower,"ReadPlain").invokeR().asInt()==7,"plain shared container survives source reset");
    Check(rt->Call(inst,"Prepare").invokeR().asInt()==4242&&rt->Call(borrower,"Grab").invokeR().asInt()==8484,"new binding after reset");
    rt->DestroyInstance(inst);
    {auto call=rt->Call(borrower,"Use");Check(call.invoke()==RunStatus::Failed&&call.error().message.find("no longer alive")!=std::string::npos,"destroy invalidates native binding without dangling memory");}
    Check(rt->Call(borrower,"ReadPlain").invokeR().asInt()==7,"plain shared container survives source destruction");
    rt->Call(borrower,"Drop").invokeR();rt->DestroyInstance(borrower);
    Check(rt->CanDestroy(),"runtime leases all released");DestroyRuntime(rt);
}
static void PayloadLifetime() {
    auto vm=NeoVMSystem::CreateVM();
    for(int i=0;i<128;++i){
        auto owner=std::make_shared<int>(i);std::weak_ptr<int> weak=owner;
        VarInfo value(VAR_FP_NATIVE);value._fpNative=vm->FunctionPropertyAlloc();value._fpNative->_refCount=1;
        Check(NeoVMSystem::RegisterTableCallBack(&value,nullptr,nullptr,nullptr,nullptr,nullptr,owner),"native payload owner installed");
        owner.reset();Check(!weak.expired(),"native value retains payload owner");
        vm->Var_Release(&value);Check(weak.expired(),"last native value releases payload owner before pool reuse");
    }
    NeoVMSystem::ReleaseVM(vm);
}
int main(){for(bool debug:{false,true})for(bool image:{false,true})Run(debug,image);PayloadLifetime();std::printf("host_api_regression: %d checks, %d failures\n",checks,failures);return failures?1:0;}