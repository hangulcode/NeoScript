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
int main() {
    NeoVMSystem::Initialize();
    const char* source = R"(
import math;
export var items=[math.Vector3(1,2,3)];
export var map={};map.v=math.Vector3(4,5,6);
export var savedList;export var savedMap;
export fun Write(){items[0][0]+=1;map.v[1]+=2;}
export fun Increment(){var old=items[0][1]++;var next=++map.v[0];return old+next;}
export fun Share(){savedList=items[0];savedMap=map.v;}
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
        auto a = item->_vec; auto b = member->_vec;
        Check(a->_refCount == 1 && b->_refCount == 1, "initial vectors uniquely owned");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "unique writes complete");
        Check(item->_vec == a && member->_vec == b, "each unique write avoids COW");
        Check(worker->ExecuteN(worker->FindFunction("Increment")) == NEOEXEC_COMPLETED, "increments complete");
        Check(item->_vec == a && member->_vec == b, "prefix and postfix increments avoid COW");
        Check(a->v[0] == 2 && a->v[1] == 3 && b->v[0] == 5 && b->v[1] == 7, "components updated");
        Check(worker->ExecuteN(worker->FindFunction("Share")) == NEOEXEC_COMPLETED, "ordinary reads complete");
        Check(a->_refCount == 2 && b->_refCount == 2, "ordinary reads share value storage");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "shared writes complete");
        Check(item->_vec != a && member->_vec != b, "shared writes detach");
        Check(a->v[0] == 2 && a->v[1] == 3 && b->v[0] == 5 && b->v[1] == 7, "saved values unchanged");
        a = item->_vec; b = member->_vec;
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "repeated writes complete");
        Check(item->_vec == a && member->_vec == b, "detached vectors remain unique on later writes");
        Check(worker->ExecuteN(worker->FindFunction("Bad")) == NEOEXEC_ERROR, "invalid component fails");
        Check(item->_vec == a && a->_refCount == 1, "failed write preserves storage and ownership");
        Check(worker->ExecuteN(worker->FindFunction("Write")) == NEOEXEC_COMPLETED, "execution recovers after failed borrow");
        NeoVMSystem::ReleaseVM(vm);
        NeoExecContextPool_Destroy(pool);
    }
    NeoVMSystem::Shutdown();
    std::printf("Vector references: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
