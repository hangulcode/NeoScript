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
    NeoVMSystem::Shutdown();
    std::printf("Vector references: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
