#include "NeoScript.h"

#include "NeoVMProgram.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace NeoScript;

static int checks = 0;
static int failures = 0;

static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok && ++failures <= 40) std::fprintf(stderr, "FAIL %s\n", label.c_str());
}

static bool HostMethod(CallContext& context, StringView name)
{
    if (name.str() == "IsNull")
    {
        context.retBool(context.argCount() == 1 && context.argType(0) == ValueType::None);
        return true;
    }
    if (name.str() == "ListCount")
    {
        AllocStats stats;
        GetAllocStats(stats);
        context.retInt(stats.lists);
        return true;
    }
    if (name.str() == "Value")
    {
        context.retInt(9);
        return true;
    }
    if (name.str() == "CaptureIsNull")
    {
        auto call = context.runtime()->Call(context.instance(), context.argFunction(0));
        if (call.invoke() != RunStatus::Completed)
        {
            context.fail(91, call.error().message);
            return false;
        }
        context.retBool(call.retType() == ValueType::None);
        return true;
    }
    return false;
}

static void CheckImageMarkers(const std::vector<uint8_t>& bytes)
{
    // 선언 분석 전용 표식과 임시 원래 슬롯 번호가 저장 이미지에 섞이지 않아야 한다.
    bool clean = bytes.size() >= sizeof(SNeoVMHeader);
    SNeoVMHeader header{};
    if (clean)
    {
        std::memcpy(&header, bytes.data(), sizeof(header));
        clean = header._iCodeSize >= 0 && header._iCodeSize % sizeof(SVMOperation) == 0 &&
            static_cast<std::size_t>(header._iCodeSize) <= bytes.size() - sizeof(header);
    }
    if (clean) for (int offset = 0; offset < header._iCodeSize; offset += sizeof(SVMOperation))
    {
        SVMOperation op{};
        std::memcpy(&op, bytes.data() + sizeof(header) + offset, sizeof(op));
        if (op.op == NOP_VAR_CLEAR && ((op.argFlag & NEOS_ARG_N3_IMMEDIATE) || op.n2 || op.n3))
            clean = false;
    }
    Check(clean, "saved bytecode has no declaration-analysis marker");
}

static ProgramHandle Compile(IRuntime* runtime, const std::string& source,
    bool debug, bool image, Error& error)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "local-initialization.ns";
    desc.includeDebugInfo = debug;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) CheckImageMarkers(bytes);
        return error.ok() ? runtime->LoadProgram(bytes, &error) : ProgramHandle{};
    }
    auto compiled = runtime->Compile(desc);
    error = compiled.error;
    return compiled.program;
}

struct BehaviorCase
{
    const char* name;
    std::string source;
    bool fails = false;
    bool reinitializeGlobals = false;
};

static std::string BranchCase(const char* body)
{
    // 같은 호출 깊이의 이전 함수가 남긴 슬롯과 모든 분기 조합을 함께 검사한다.
    return std::string(R"NS(
import math;
fun Seed(var first,var second) {
    var x=73; var y=74; var z=75;
    if(first)x=[8,9];
    if(second)y=math.Vector2(4,5);
}
fun Exercise(var first,var second) {
)NS") + body + R"NS(
}
export fun Probe() {
    for(var i in 0,12) {
        var first=(i&1)!=0; var second=(i&2)!=0;
        Seed(first,second);
        if(!Exercise(first,second))return false;
    }
    return true;
}
)NS";
}

static std::vector<BehaviorCase> BehaviorCases()
{
    std::vector<BehaviorCase> cases;
    const std::pair<const char*, const char*> branches[] = {
        {"straight assignment", "var x;x=9;return x==9;"},
        {"both branches", "var x;if(first)x=1;else x=2;if(first)return x==1;return x==2;"},
        {"nested complete branches", "var x;if(first){if(second)x=1;else x=2;}else{if(second)x=3;else x=4;}if(first){if(second)return x==1;return x==2;}if(second)return x==3;return x==4;"},
        {"missing else", "var x;if(first)x=9;if(first)return x==9;return x==null;"},
        {"condition reads null", "var x;if(x==null)x=9;else x=1;return x==9;"},
        {"assignment RHS reads null", "var x;x=(x==null);return x;"},
        {"self assignment reads null", "var x;x=x;return x==null;"},
        {"copy before assignment", "var x;var copy=x;x=9;return copy==null&&x==9;"},
        {"short circuit reads null", "var x;var good=first||(x==null);x=9;return good&&x==9;"},
        {"short circuit conditional write", "var x;var ignored=first&&((x=9)==9);if(first)return x==9;return x==null;"},
        {"zero iteration range", "var x;for(var i in 0,0)x=9;return x==null;"},
        {"zero iteration while", "var x;var run=false;while(run)x=9;return x==null;"},
        {"continue and break before write", "var x;for(var i in 0,3){if(i<2)continue;if(first)break;x=9;}if(first)return x==null;return x==9;"},
        {"loop declaration reinitializes", "for(var i in 0,5){var x;if(x!=null)return false;if((i&1)==0){x=8;continue;}x=[7];}return true;"},
        {"while declaration reinitializes", "var i=0;while(i<5){var x;if(x!=null)return false;i++;if(i<4){x=8;continue;}x=[7];break;}return true;"},
        {"closure captures before assignment", "var x;var read=fun(){return x;};x=9;return read()==null&&x==9;"},
        {"closure after complete assignment", "var x;if(first)x=1;else x=2;var read=fun(){return x;};x=9;if(first)return read()==1;return read()==2;"},
        {"native observes null", "var x;var good=Host.IsNull(x);x=9;return good&&x==9;"},
        {"native reentry observes capture", "var x;var good=Host.CaptureIsNull(fun(){return x;});x=9;return good&&x==9;"},
        {"native RHS assigns", "var x;x=Host.Value();return x==9;"},
        {"multiple declarations", "var x;var y;if(first){x=1;y=2;}else{x=3;y=4;}if(first)return x+y==3;return x+y==7;"},
        {"candidate followed by null assignment", "var x;var y;x=y;y=9;return x==null&&y==9;"},
        {"null temporary retargets another local", "var x;var y=null;x=9;return y==null&&x==9;"},
        {"scalar and shared container branches", "var x;if(first)x=[2];else x=3;if(first){var alias=x;alias[0]=5;return x[0]==5;}return x==3;"},
        {"vector COW after complete assignment", "var original=math.Vector2(2,3);var x;if(first)x=original;else x=math.Vector2(4,5);x[0]=9;if(first)return original[0]==2&&x[1]==3;return original[0]==2&&x[1]==5;"},
        {"range surrounds candidates", "var sum=0;for(var i in 0,4){var x;if(i<2)x=1;else x=2;sum+=x;}return sum==6;"},
        {"while surrounds candidates", "var i=0;var sum=0;while(i<4){var x;if(i<2)x=1;else x=2;sum+=x;i++;}return sum==6;"},
        {"switch surrounds candidate", "var key=0;if(first)key++;if(second)key+=2;var sum=0;switch(key){case 0:{var x;if(second)x=10;else x=11;sum=x;}case 1:{var x;if(second)x=20;else x=21;sum=x;}case 2:{var x;if(second)x=30;else x=31;sum=x;}default:{var x;if(second)x=40;else x=41;sum=x;}}if(key==0)return sum==11;if(key==1)return sum==21;if(key==2)return sum==30;return sum==40;"},
        {"nested switch jump targets", "var key=0;if(first)key++;if(second)key+=2;var result=0;switch(key){case 0,1:{var x;if(first)x=1;else x=2;switch(x){case 1:result=11;default:result=12;}}default:{var x;if(first)x=3;else x=4;switch(x){case 3:result=23;default:result=24;}}}if(key==0)return result==12;if(key==1)return result==11;if(key==2)return result==24;return result==23;"},
        {"switch writes candidate", "var x;switch(first){case false:x=1;default:x=2;}if(first)return x==2;return x==1;"},
        {"switch missing default", "var x;switch(first){case true:x=9;}if(first)return x==9;return x==null;"},
        {"switch and loop exits", "var total=0;for(var i in 0,5){var x;if(i<2)x=1;else x=2;switch(i){case 0:continue;case 1:total+=x;default:{var y;if(first)y=3;else y=4;total+=x+y;}}if(i==3)break;}if(first)return total==11;return total==13;"}
    };
    for (const auto& branch : branches) cases.push_back({branch.first, BranchCase(branch.second)});
    cases.push_back({"callback sees released stale container", R"NS(
fun Seed(){var x=[7];}
fun Control(){var x=null;var count=Host.ListCount();x=3;return count;}
fun Exercise(){var x;var count=Host.ListCount();x=3;return count;}
export fun Probe(){Seed();var expected=Control();Seed();var actual=Exercise();return actual==expected;}
)NS"});
    cases.push_back({"global declaration resets before assignment", R"NS(
export var value;
var observed=value==null;
value=9;
export fun Probe(){return observed&&value==9;}
)NS", false, true});
    for (const char* operation : {"x+=1;", "x++;", "x=x+1;", "x<<=1;"})
    {
        cases.push_back({operation, std::string("fun Seed(){var x=73;}fun Exercise(){var x;") +
            operation + "return true;}export fun Probe(){Seed();return Exercise();}", true});
    }
#ifdef CSTYLE_FOR
    cases.push_back({"classic for surrounds candidates", R"NS(
export fun Probe(){var sum=0;for(var i=0;i<4;i++){var x;if(i<2)x=1;else x=2;sum+=x;}return sum==6;}
)NS"});
#endif
    return cases;
}

static void RunBehavior(IRuntime* runtime, const BehaviorCase& test, bool debug, bool image)
{
    AllocStats before;
    GetAllocStats(before);
    Error error;
    auto program = Compile(runtime, test.source + "\nexport fun Recovery(){return 23;}", debug, image, error);
    const std::string label = std::string(test.name) + " debug=" + std::to_string(debug) +
        " image=" + std::to_string(image);
    Check(bool(program), label + " compile: " + error.message);
    if (!program) return;
    for (int instanceNo = 0; instanceNo < 2; ++instanceNo)
    {
        auto instance = runtime->CreateInstance(program);
        Check(bool(instance), label + " instance");
        if (!instance) continue;
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            if (test.reinitializeGlobals)
            {
                Check(runtime->SetGlobalInt(instance, "value", 73), label + " seed global");
                Check(runtime->RunGlobalInit(instance) == RunStatus::Completed, label + " rerun initializer");
            }
            {
                auto call = runtime->Call(instance, "Probe");
                // 재진입 검사에는 호출 시간 제한을 걸지 않고 CTest 전체 제한을 사용한다.
                const auto status = call.invoke();
                Check(test.fails ? status == RunStatus::Failed && !call.error().message.empty()
                    : status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                    label + " invoke: " + call.error().message);
            }
            {
                auto call = runtime->Call(instance, "Recovery");
                Check(call.invoke() == RunStatus::Completed && call.retInt() == 23, label + " recovery");
            }
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    runtime->CollectCycles(true);
    AllocStats after;
    GetAllocStats(after);
    Check(after.maps == before.maps && after.lists == before.lists && after.arrays == before.arrays &&
        after.vectors == before.vectors && after.closures == before.closures &&
        after.bufferBytes == before.bufferBytes, label + " releases local references");
}

struct ShapeCase
{
    const char* name;
    const char* source;
    int optimizedClears;
    int debugClears;
};

static void RunShapes(IRuntime* runtime, bool debug, bool image)
{
    // 실행 결과만 같아도 초기화 명령이 남을 수 있으므로 실제 CLR 개수를 검사한다.
    const ShapeCase cases[] = {
        {"straight overwrite", "export fun Probe(){var x;x=9;return x;}", 0, 1},
        {"float overwrite", "export fun Probe(){var x;x=1.25;return x;}", 0, 1},
        {"local copy overwrite", "export fun Probe(var value){var x;x=value;return x+1;}", 0, 1},
        {"copy fused into return stays conservative", "export fun Probe(var value){var x;x=value;return x;}", 1, 1},
        {"explicit null overwrite", "export fun Probe(){var x;x=null;return x;}", 1, 2},
        {"both branch overwrites", "export fun Probe(var c){var x;if(c)x=1;else x=2;return x;}", 0, 1},
        {"nested branch overwrites", "export fun Probe(var c,var d){var x;if(c){if(d)x=1;else x=2;}else{if(d)x=3;else x=4;}return x;}", 0, 1},
        {"separated candidates", "export fun Probe(){var x;x=1;var y;y=2;return x+y;}", 0, 2},
        {"other local write is a barrier", "export fun Probe(var c){var x;var y;if(c){x=1;y=2;}else{x=3;y=4;}return x+y;}", 2, 2},
        {"read second candidate before write", "export fun Probe(){var x;var y;x=y;y=9;return x;}", 2, 2},
        {"null temporary is not a declaration", "export fun Probe(){var x;var y=null;x=9;return y;}", 2, 2},
        {"allocation is a barrier", "export fun Probe(){var x;var other=[1];x=9;return x;}", 1, 1},
        {"incomplete branch", "export fun Probe(var c){var x;if(c)x=1;return x;}", 1, 1},
        {"condition reads candidate", "export fun Probe(){var x;if(x==null)x=1;else x=2;return x;}", 2, 2},
        {"RHS reads candidate", "export fun Probe(){var x;x=(x==null);return x;}", 2, 2},
        {"self assignment reads candidate", "export fun Probe(){var x;x=x;return x;}", 1, 1},
        {"compound reads candidate", "export fun Probe(){var x;x+=1;return x;}", 1, 1},
        {"increment reads candidate", "export fun Probe(){var x;x++;return x;}", 1, 1},
        {"range may not execute", "export fun Probe(var n){var x;for(var i in 0,n)x=1;return x;}", 1, 1},
        {"while may not execute", "export fun Probe(var c){var x;while(c){x=1;break;}return x;}", 1, 1},
        {"range descriptor reads upper", "export fun Probe(var n){var x;if(n>=0&&n<=x)x=1;else x=2;return x;}", 1, 1},
        {"range descriptor reads lower", "export fun Probe(var n){var x;if(n>=x&&n<=9)x=1;else x=2;return x;}", 1, 1},
        {"local range condition permits overwrite", "export fun Probe(var n){var x;if(n>=0&&n<=9)x=1;else x=2;return x;}", 0, 1},
        {"global condition is a barrier", "var condition=true;export fun Probe(){var x;if(condition)x=1;else x=2;return x;}", 1, 1},
        {"global copy is a barrier", "var value=3;export fun Probe(){var x;x=value;return x+1;}", 1, 1},
        {"call before overwrite", "export fun Probe(){var x;Host.Value();x=9;return x;}", 1, 1},
        {"call supplies overwrite", "export fun Probe(){var x;x=Host.Value();return x;}", 1, 1},
        {"closure reads candidate", "export fun Probe(){var x;var read=fun(){return x;};x=9;return read();}", 1, 1},
        {"global remains initialized", "export var x;x=9;export fun Probe(){return x;}", 1, 1}
    };
    for (const auto& test : cases)
    {
        Error error;
        auto program = Compile(runtime, test.source, debug, image, error);
        const std::string label = std::string(test.name) + " shape debug=" + std::to_string(debug) +
            " image=" + std::to_string(image);
        Check(bool(program), label + " compile: " + error.message);
        if (!program) continue;
        std::vector<DebugInstruction> instructions;
        runtime->GetDebugInstructions(program, instructions);
        int count = 0;
        for (const auto& instruction : instructions)
            if (instruction.assembly.compare(0, 5, "CLR  ") == 0) ++count;
        const int expected = debug ? test.debugClears : test.optimizedClears;
        Check(!instructions.empty() && count == expected,
            label + " CLR=" + std::to_string(count) + " expected=" + std::to_string(expected));
        runtime->DestroyProgram(program);
    }
}

int main()
{
    auto runtime = CreateRuntime();
    Check(runtime != nullptr, "runtime create");
    if (!runtime) return 1;
    NativeObjectDesc host;
    host.name = "Host";
    host.method = HostMethod;
    Check(runtime->RegisterObject(host), "register Host");
    runtime->FreezeBindings();
    const auto cases = BehaviorCases();
    for (bool debug : {false, true}) for (bool image : {false, true})
    {
        for (const auto& test : cases) RunBehavior(runtime, test, debug, image);
        RunShapes(runtime, debug, image);
    }
    DestroyRuntime(runtime);
    std::printf("local_initialization_regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
