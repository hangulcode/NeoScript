// 2·3성분의 인라인 값 복사와 4성분의 공유/COW가 같은 스크립트 결과를 내는지 확인한다.
#include "NeoVMWorker.h"
#include <cstdio>
#include <cstring>
using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
// Native getters can transfer control before the component update; setters can
// do so after it. Exercise both boundaries without relying on private VM state.
struct PropertyState {
    float x = 1;
    int gets = 0, sets = 0;
    bool pause = false, switchGet = false, switchSet = false;
};
static bool PositionProperty(INeoVMWorker* worker, void* data, const VMString*, VarInfo* value, bool get) {
    auto& state = *static_cast<PropertyState*>(data);
    if (get) {
        ++state.gets;
        worker->Var_SetVec3(value, state.x, 2, 3);
        if (state.pause) { state.pause = false; worker->DebugPause(); }
    } else {
        ++state.sets;
        state.x = value->VectorComponent(0);
    }
    bool& transfer = get ? state.switchGet : state.switchSet;
    if (transfer) {
        transfer = false;
        auto coroutine = worker->GetVar("co");
        const auto* functions = CNeoVM::GetSystemModule("coroutine");
        if (!functions || !coroutine || coroutine->GetType() != VAR_COROUTINE) return false;
        for (const auto& function : *functions) {
            if (function.fname != "resume") continue;
            worker->Var_SetCoroutine(worker->GetStackVar(1), coroutine->_cor);
            const bool result = CNeoVM::CallDefaultNativeByIndex(function.nativeIndex,
                static_cast<CNeoVMWorker*>(worker), 1);
            worker->Var_Release(worker->GetStackVar(1));
            return result;
        }
        return false;
    }
    return true;
}
static void NativeWriteBoundaries() {
    const char* prefix =
        "import coroutine;\n"
        "export var Host; export var phase=0; export var result;\n"
        "export fun Never(){return 0;}\n"
        "export var co;\n"
        "fun Other(){phase=1;var a=[[1]];a[0][0]+=2;yield;phase=2;}\n"
        "export fun Prepare(){co=coroutine.create(Other);}\n";
    // Local component/value operands also exercise the CLT_MOV_L load patch.
    for (const char* expression : {"Host.Position[i]+=n", "Host.Position[i]=n+1",
        "Host.Position[i]*=n+1", "Host.Position[i]-=-n", "Host.Position[i]/=0.25"}) {
        const std::string source = std::string(prefix) +
            "export fun Write(){var i=0;var n=3;result=(" + expression + ");phase+=10;}\n";
        for (bool debug : {false, true}) {
            for (int mode = 0; mode < 3; ++mode) {
                if (mode == 0 && !debug) continue;
                auto pool = NeoExecContextPool_Create();
                NeoLoadVMParam load; load.execPool = pool;
                NeoCompilerParam compile(source.c_str(), (int)source.size());
                std::string error; compile.err = &error; compile.debug = debug;
                auto vm = NeoVMSystem::CompileAndLoadRunVM(compile, &load);
                Check(vm != nullptr, error.c_str());
                if (!vm) { NeoExecContextPool_Destroy(pool); continue; }
                INeoVMWorker* worker = vm->GetMainWorker();
                PropertyState state;
                auto host = worker->GetVar("Host");
                Check(worker->ResetVarType(host, VAR_FP_NATIVE) &&
                    NeoVMSystem::RegisterTableCallBack(host, &state, nullptr, PositionProperty), "bind native property");
                Check(worker->ExecuteN(worker->FindFunction("Prepare")) == NEOEXEC_COMPLETED, "prepare callback coroutine");
                state.pause = mode == 0;
                state.switchGet = mode == 1;
                state.switchSet = mode == 2;
                // Keep debug dispatch active with a breakpoint in an uncalled function.
                if (mode == 0) worker->DebugSetBreakpoints(std::vector<int>{3});
                int status = worker->ExecuteN(worker->FindFunction("Write"));
                if (mode == 0) {
                    Check(status == NEOEXEC_SUSPENDED && worker->DebugIsPaused(), "getter pause suspends before mutation");
                    Check(state.gets == 1 && state.sets == 0 && state.x == 1, "paused getter has not committed");
                    worker->DebugContinue();
                    status = worker->ResumeTop();
                }
                Check(status == NEOEXEC_COMPLETED, "native component write resumes and completes");
                Check(state.gets == 1 && state.sets == 1 && state.x == 4, "native getter and setter run exactly once");
                auto result = worker->GetVar("result");
                Check(result->GetType() == VAR_FLOAT && result->_float == 4, "resumed assignment returns modified component");
                Check(worker->GetVar("phase")->_int == (mode == 0 ? 10 : 11), "callback coroutine returns to correct frame");
                NeoVMSystem::ReleaseVM(vm);
                NeoExecContextPool_Destroy(pool);
            }
        }
    }
}
int main() {
    NeoVMSystem::Initialize();
    const char* sourceTemplate = R"(
import math; import system;
export var items=[math.Vector3(1,2,3)];
export var map={};map.v=math.Vector3(4,5,6);
export var array=system.array(math.Vector3(1,2,3),2);
array[0]=math.Vector3(7,8,9);
export var savedList;export var savedMap;export var savedArray;
export fun Write(){items[0][0]+=1;map.v[1]+=2;array[0][0]+=1;}
export fun Increment(){var old=items[0][1]++;var next=++map.v[0];array[0][1]++;++array[0][2];return old+next;}
export fun Share(){savedList=items[0];savedMap=map.v;savedArray=array[0];}
export fun BadArray(){array[0][9]+=1;}
export fun Bad(){items[0][9]+=1;}
)";
    for (int dim : {2, 3, 4}) {
    std::string source = sourceTemplate;
    const auto replace = [&source](const std::string& from, const std::string& to) {
        size_t at = 0;
        while ((at = source.find(from, at)) != std::string::npos) {
            source.replace(at, from.size(), to); at += to.size();
        }
    };
    if (dim == 2) {
        replace("Vector3(1,2,3)", "Vector2(1,2)");
        replace("Vector3(4,5,6)", "Vector2(4,5)");
        replace("Vector3(7,8,9)", "Vector2(7,8)");
        replace("++array[0][2]", "++array[0][1]");
    } else if (dim == 4) {
        replace("Vector3(1,2,3)", "Vector4(1,2,3,4)");
        replace("Vector3(4,5,6)", "Vector4(4,5,6,7)");
        replace("Vector3(7,8,9)", "Vector4(7,8,9,10)");
    }
    for (bool debug : {false, true}) {
        auto pool = NeoExecContextPool_Create();
        NeoLoadVMParam load; load.execPool = pool;
        NeoCompilerParam compile(source.c_str(), (int)source.size());
        std::string error; compile.err = &error; compile.debug = debug;
        auto vm = NeoVMSystem::CompileAndLoadRunVM(compile, &load);
        Check(vm != nullptr, error.c_str());
        if (!vm) { NeoExecContextPool_Destroy(pool); continue; }
        INeoVMWorker* worker = vm->GetMainWorker();
        auto item = worker->GetVar("items")->_lst->GetValue(0);
        auto member = worker->GetVar("map")->_tbl->Find("v");
        auto array = worker->GetVar("array")->_array;
        const VarInfo* element = array->VectorValues();
        Check(array->VectorDimension() == dim && element->VectorComponent(0) == 7 && element->VectorComponent(1) == 8 && (dim < 3 || element->VectorComponent(2) == 9), "array stores vector values");
        Check(array->VectorValues()[1].VectorComponent(0) == 1 && array->VectorValues()[1].VectorComponent(1) == 2 &&
            array->_initialVector.VectorComponent(0) == 1 && (dim < 3 || array->_initialVector.VectorComponent(2) == 3), "array initializer fills unmodified elements by value");
        auto a = dim == 4 ? item->_vec : nullptr;
        auto b = dim == 4 ? member->_vec : nullptr;
        Check(dim == 4 ? a->_refCount == 1 && b->_refCount == 1 :
            item->GetType() == VAR_VEC && !item->IsAllocType() && !member->IsAllocType(), "storage matches vector dimension");
        SNeoVMAllocStats beforeWrites;
        vm->GetAllocStats(beforeWrites);
        Check(dim == 4 || beforeWrites.vectors == 0, "inline vectors need no pool objects");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "unique writes complete");
        Check(dim != 4 || (item->_vec == a && member->_vec == b), "each unique write avoids COW");
        Check(element->VectorComponent(0) == 8, "array component write lands in the buffer");
        Check(worker->ExecuteN(worker->FindFunction("Increment")) == NEOEXEC_COMPLETED, "increments complete");
        Check(dim != 4 || (item->_vec == a && member->_vec == b), "prefix and postfix increments avoid COW");
        Check(dim == 2 ? element->VectorComponent(1) == 10 : element->VectorComponent(1) == 9 && element->VectorComponent(2) == 10, "array increments write through");
        Check(item->VectorComponent(0) == 2 && item->VectorComponent(1) == 3 && member->VectorComponent(0) == 5 && member->VectorComponent(1) == 7, "components updated");
        Check(worker->ExecuteN(worker->FindFunction("Share")) == NEOEXEC_COMPLETED, "ordinary reads complete");
        auto savedList = worker->GetVar("savedList");
        auto savedMap = worker->GetVar("savedMap");
        Check(dim != 4 || (a->_refCount == 2 && b->_refCount == 2), "four-component reads share value storage");
        auto savedArray = worker->GetVar("savedArray");
        Check(savedArray->IsVector() && savedArray->VectorComponent(0) == 8 &&
            (dim == 4 ? savedArray->_vec == element->_vec && savedArray->_vec->_refCount == 2 : !savedArray->IsAllocType()), "array read copies into value storage");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "shared writes complete");
        Check(dim != 4 || (item->_vec != a && member->_vec != b), "shared four-component writes detach");
        Check(element->VectorComponent(0) == 9 && savedArray->VectorComponent(0) == 8, "array write leaves earlier copies unchanged");
        Check(savedList->VectorComponent(0) == 2 && savedList->VectorComponent(1) == 3 && savedMap->VectorComponent(0) == 5 && savedMap->VectorComponent(1) == 7, "saved values unchanged");
        if (dim == 4) { a = item->_vec; b = member->_vec; }
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "repeated writes complete");
        Check(dim != 4 || (item->_vec == a && member->_vec == b), "detached vectors remain unique on later writes");
        Check(element->VectorComponent(0) == 10, "array repeated writes accumulate");
        Check(worker->ExecuteN(worker->FindFunction("BadArray")) == NEOEXEC_ERROR, "array invalid component fails");
        Check(element->VectorComponent(0) == 10, "array failed component write leaves the element unchanged");
        Check(worker->ExecuteN(worker->FindFunction("Bad")) == NEOEXEC_ERROR, "invalid component fails");
        Check(dim != 4 || (item->_vec == a && a->_refCount == 1), "failed write preserves storage and ownership");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "execution recovers after failed borrow");
        SNeoVMAllocStats afterWrites;
        vm->GetAllocStats(afterWrites);
        Check(dim == 4 || afterWrites.vectors == beforeWrites.vectors, "inline writes and copies allocate no vector storage");
        NeoVMSystem::ReleaseVM(vm);
        NeoExecContextPool_Destroy(pool);
    }
    }
    NativeWriteBoundaries();
    NeoVMSystem::Shutdown();
    std::printf("Vector references: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
