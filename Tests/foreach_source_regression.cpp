#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;

static const char* source = R"NS(
import system;
var sourceCalls=0;
var shared=[1,2,3];
fun Make() { sourceCalls+=1; return [1,2,3]; }
fun Shared() { return shared; }
export fun NestedLiterals()
{
    var result="";
    foreach (var a in ["A","B","C"]) {
        foreach (var b in ["x","y"]) {
            foreach (var c in [1,2]) { result=result..a..b..c; }
        }
    }
    return result=="Ax1Ax2Ay1Ay2Bx1Bx2By1By2Cx1Cx2Cy1Cy2";
}
export fun BodyTemporaries()
{
    var sum=0;
    foreach (var value in [1,2,3]) {
        var trash=[90,91,92];
        var map={"a":trash};
        var text="value="..value;
        sum+=value;
    }
    return sum==6;
}
export fun ReturnValue()
{
    sourceCalls=0; var sum=0;
    foreach (var a in Make()) { foreach (var b in Make()) sum+=a*b; }
    return sum==36 && sourceCalls==4;
}
export fun NativeResults()
{
    var sum=0;
    foreach (var a in system.array(2,3)) { var noise=[9,8]; sum+=a; }
    foreach (var b in system.set([1,2,2,3])) { var noise=[9,8]; sum+=b; }
    var map={"a":7,"b":8}; var keys="";
    foreach (var k in map.keys()) { var noise=[9,8]; keys=keys..k; }
    return sum==12 && (keys=="ab" || keys=="ba");
}
export fun MapAndSubscript()
{
    var sum=0;
    foreach (var k,v in {"a":10,"b":20}) {
        foreach (var inner in [1,2]) sum+=v;
    }
    var lists=[[1,2,3],[4]];
    foreach (var item in lists[0]) { var noise=[8,9]; sum+=item; }
    return sum==66;
}
export fun Exits()
{
    var sum=0;
    foreach (var a in [1,2,3]) {
        if (a==2) continue;
        foreach (var b in [4,5]) { sum+=a*b; break; }
    }
    foreach (var a in [10,20]) sum+=a;
    return sum==46;
}
export fun EarlyReturn()
{
    foreach (var a in [1,2,3]) {
        foreach (var b in [4,5]) { var noise=[0]; return a==1 && b==4; }
    }
    return false;
}
export fun Empty()
{
    var sum=0;
    foreach (var a in []) sum+=100;
    foreach (var a in [1,2]) { foreach (var b in []) sum+=100; sum+=a; }
    return sum==3;
}
export fun Mutation()
{
    shared=[1,2,3];
    foreach (var value in Shared()) { shared.append(4); }
    return true;
}
)NS";

int main()
{
    auto runtime=CreateRuntime();
    runtime->FreezeBindings();
    int checks=0, failures=0;
    for (bool debug : {false,true}) for (bool image : {false,true})
    {
        CompileDesc desc; desc.source=source; desc.sourceName="foreach-source.ns"; desc.includeDebugInfo=debug;
        ProgramHandle program; Error error;
        if (image) {
            std::vector<uint8_t> bytes;
            error=runtime->CompileToBytecode(desc,bytes);
            if (error.ok()) program=runtime->LoadProgram(bytes,&error);
        } else {
            auto result=runtime->Compile(desc); program=result.program; error=result.error;
        }
        if (!program) {
            ++failures; std::fprintf(stderr,"compile: %s\n",error.message.c_str()); continue;
        }
        auto instance=runtime->CreateInstance(program);
        if (!instance) { ++failures; runtime->DestroyProgram(program); continue; }
        // 조기 종료한 반복자나 식 슬롯이 다음 호출에 남는 경우도 검사한다.
        for (int repeat=0; repeat<3; ++repeat) {
            for (const char* name : {"NestedLiterals","BodyTemporaries","ReturnValue","NativeResults",
                "MapAndSubscript","Exits","EarlyReturn","Empty","Mutation"}) {
                ++checks;
                auto call=runtime->Call(instance,name);
                const auto status=call.invoke();
                bool ok=status==RunStatus::Completed && call.retType()==ValueType::Bool && call.retBool();
                if (std::string(name)=="Mutation")
                    ok=status==RunStatus::Failed && call.error().message.find("modified")!=std::string::npos;
                if (!ok) {
                    ++failures;
                    std::fprintf(stderr,"FAIL %s debug=%d image=%d repeat=%d: %s\n",name,debug,image,repeat,call.error().message.c_str());
                }
            }
        }
        runtime->DestroyInstance(instance); runtime->DestroyProgram(program);
    }
    DestroyRuntime(runtime);
    std::printf("Foreach source: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
