// Verify the optimization itself: unique stored vectors must keep their VecInfo,
// while ordinary value copies must still detach on the first write.
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
        state.x = value->_vec->v[0];
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
    const char* source = R"(
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
    for (bool debug : {false, true}) {
        auto pool = NeoExecContextPool_Create();
        NeoLoadVMParam load; load.execPool = pool;
        NeoCompilerParam compile(source, (int)std::strlen(source));
        std::string error; compile.err = &error; compile.debug = debug;
        auto vm = NeoVMSystem::CompileAndLoadRunVM(compile, &load);
        Check(vm != nullptr, error.c_str());
        if (!vm) { NeoExecContextPool_Destroy(pool); continue; }
        INeoVMWorker* worker = vm->GetMainWorker();
        auto item = worker->GetVar("items")->_lst->GetValue(0);
        auto member = worker->GetVar("map")->_tbl->Find("v");
        auto array = worker->GetVar("array")->_array;
        auto element = &array->VectorValues()[0];
        auto c = element->_vec;
        Check(element->GetType() == VAR_VEC && c->_refCount == 1, "array stores ordinary uniquely owned VAR_VEC");
        Check(array->VectorValues()[1]._vec == array->_initialVector._vec && array->_initialVector._vec->_refCount == 2,
            "array initializer and unmodified elements share COW storage");
        auto a = item->_vec; auto b = member->_vec;
        Check(a->_refCount == 1 && b->_refCount == 1, "initial vectors uniquely owned");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "unique writes complete");
        Check(item->_vec == a && member->_vec == b, "each unique write avoids COW");
        Check(element->_vec == c && c->v[0] == 8, "unique array component write avoids COW");
        Check(worker->ExecuteN(worker->FindFunction("Increment")) == NEOEXEC_COMPLETED, "increments complete");
        Check(item->_vec == a && member->_vec == b, "prefix and postfix increments avoid COW");
        Check(element->_vec == c && c->v[1] == 9 && c->v[2] == 10, "array increments avoid COW");
        Check(a->v[0] == 2 && a->v[1] == 3 && b->v[0] == 5 && b->v[1] == 7, "components updated");
        Check(worker->ExecuteN(worker->FindFunction("Share")) == NEOEXEC_COMPLETED, "ordinary reads complete");
        Check(a->_refCount == 2 && b->_refCount == 2, "ordinary reads share value storage");
        Check(c->_refCount == 2 && worker->GetVar("savedArray")->_vec == c, "array read shares existing vector without allocation");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "shared writes complete");
        Check(item->_vec != a && member->_vec != b, "shared writes detach");
        Check(element->_vec != c && c->v[0] == 8, "array COW protects ordinary copies");
        c = element->_vec;
        Check(a->v[0] == 2 && a->v[1] == 3 && b->v[0] == 5 && b->v[1] == 7, "saved values unchanged");
        a = item->_vec; b = member->_vec;
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "repeated writes complete");
        Check(item->_vec == a && member->_vec == b, "detached vectors remain unique on later writes");
        Check(element->_vec == c && c->_refCount == 1, "array detached value stays unique on later writes");
        Check(worker->ExecuteN(worker->FindFunction("BadArray")) == NEOEXEC_ERROR, "array invalid component fails");
        Check(element->_vec == c && c->_refCount == 1, "array failed borrow preserves ownership");
        Check(worker->ExecuteN(worker->FindFunction("Bad")) == NEOEXEC_ERROR, "invalid component fails");
        Check(item->_vec == a && a->_refCount == 1, "failed write preserves storage and ownership");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "execution recovers after failed borrow");
        NeoVMSystem::ReleaseVM(vm);
        NeoExecContextPool_Destroy(pool);
    }
    NativeWriteBoundaries();
    NeoVMSystem::Shutdown();
    std::printf("Vector references: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
