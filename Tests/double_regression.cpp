#include "NeoScript.h"
#include "NeoVMInternal.h"
#include "NeoVMVector.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <iomanip>
#include <locale>
#include <sstream>
#include <type_traits>

using namespace NeoScript;
static_assert(sizeof(NS_FLOAT)==8,"scalar double storage");
static_assert(sizeof(((VecInfo*)nullptr)->v[0])==4,"vector float storage");
static_assert(std::is_same<decltype(ArrayView().floats()),double*>::value,"double array view");
static_assert(sizeof(VarInfo)==sizeof(NS_INT)+alignof(VarInfo),"unchanged value slot");
static int checks=0, failures=0;
static void Check(bool ok,const char* label)
{
    ++checks;
    if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",label);}
}
static bool Host(CallContext& call,StringView method)
{
    if(method.str()=="Echo"){call.retFloat(call.argFloat(0));return true;}
    if(method.str()=="Collections")
    {
        const double value=call.argFloat(0);
        auto map=call.retMap();map.setFloat("value",value);map.setList("items").pushFloat(value);
        return true;
    }
    if(method.str()=="Array")
    {
        ArrayView view;
        if(!call.argAsArray(0,view) || !view.floats() || view.count()!=2)return false;
        view.floats()[1]=view.floats()[0]+0.0000000000009094947017729282379150390625;
        call.retBool(true);return true;
    }
    return false;
}
static const char* source=R"ns(
import math;import system;
export var global_value=0.0;
export fun Echo(var x){return Host.Echo(x);}
export fun Collections(var x){return Host.Collections(x);}
export fun Array(var x){return system.array(x,2);}
export fun Probe(){
    var epsilon=0.0000000000009094947017729282379150390625;
    var x=1.0+epsilon;
    if(x==1.0 || x-1.0!=epsilon || Host.Echo(x)!=x)return false;
    if((16777216.0+1.0)-16777216.0!=1.0)return false;
    var a=system.array(x,2); if(!Host.Array(a) || a[1]-a[0]!=epsilon)return false;
    var c=a.clone();if(c[1]!=a[1])return false;
    var m={};m[1.0]=7;m[x]=9;if(m.len()!=2 || m[x]!=9)return false;
    var set=system.set([1.0,x]);if(set.len()!=2)return false;
    var v=math.Vector3(x,16777217.0,3.0);
    if(v[0]!=1.0 || v[1]!=16777216.0)return false;
    var va=system.array(v,2);va[1][0]=x;
    if(va[1][0]!=1.0 || va[0][1]!=16777216.0)return false;
    var copied=v;copied[0]=2.0;if(v[0]!=1.0)return false;
    if((v*x)[0]!=1.0)return false;
    if(math.Vector2(x,x)[0]!=1.0 || math.Vector4(x,x,x,x)[3]!=1.0)return false;
    if(math.Quaternion(x,x,x,x)[0]!=1.0)return false;
    if(system.jsonDecode(system.jsonEncode(x))!=x || tofloat(math.tostr(x))!=x)return false;
    if(system.jsonDecode("1e300")!=1e300 || 1e300<=1e299)return false;
    if(math.PingPong(1.6e308,1.2e308)!=1.2e308-(1.6e308-1.2e308))return false;
    var angle=math.LerpAngle(0.0,3.0,1e308);if(angle!=angle || angle < -3.15 || angle > 3.15)return false;
    var positive=0.0;var negative=-0.0;
    if(1.0/positive<0.0 || 1.0/negative>0.0)return false;
    return true;
}
)ns";

static void FloatImmediates(IRuntime* runtime, bool debug, bool image)
{
    struct Case { double value; bool immediate; };
    const double largest = (std::numeric_limits<float>::max)();
    const double smallest = std::numeric_limits<float>::denorm_min();
    const Case cases[] = {
        {0.0,true}, {-0.0,true}, {0.5,true}, {-1.0,true}, {16777216.0,true},
        {largest,true}, {-largest,true}, {smallest,true}, {-smallest,true},
        {0.1,false}, {16777217.0,false}, {std::nextafter(1.0,2.0),false},
        {std::nextafter(largest,0.0),false}, {std::nextafter(largest,INFINITY),false},
        {smallest/2,false}, {1e300,false}, {-1e300,false},
        {std::numeric_limits<double>::denorm_min(),false}
    };
    for (const Case& test : cases)
    {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::scientific << std::setprecision(17) << test.value;
        const std::string literal = text.str();
        // 전역 초기화, 소유 값 덮어쓰기, 인자 전달을 각각 직접 실행과 이미지 로드로 검사한다.
        const std::string source = "export var stored=" + literal + ";"
            "export fun Literal(){var local=[1];local=" + literal + ";stored=\"old\";stored=" + literal +
            ";return Host.Echo(local);} export fun Argument(){return Host.Echo(" + literal + ");}";
        CompileDesc desc;desc.source=source;desc.sourceName="movf.ns";desc.includeDebugInfo=debug;
        ProgramHandle program;Error error;
        if(image){std::vector<uint8_t> bytes;error=runtime->CompileToBytecode(desc,bytes);if(error.ok())program=runtime->LoadProgram(bytes,&error);}
        else{auto result=runtime->Compile(desc);program=result.program;error=result.error;}
        Check(bool(program),error.message.c_str());if(!program)continue;
        std::vector<DebugInstruction> instructions;runtime->GetDebugInstructions(program,instructions);
        int immediateCount=0;
        for(const auto& instruction:instructions)
            if(instruction.assembly.find("MOVF ")==0)
            {
                ++immediateCount;
                const size_t equals=instruction.assembly.find(" = ");
                Check(equals!=std::string::npos,"MOVF assembly has a numeric operand");
                if(equals!=std::string::npos)
                {
                    std::istringstream value(instruction.assembly.substr(equals+3));value.imbue(std::locale::classic());
                    double decoded=0;value>>decoded;
                    Check(!value.fail() && decoded==test.value && std::signbit(decoded)==std::signbit(test.value),"MOVF disassembly preserves value and zero sign");
                }
            }
        Check(test.immediate ? immediateCount>=3 : immediateCount==0,"only exact float32 literals use MOVF");
        auto instance=runtime->CreateInstance(program);Check(bool(instance),"MOVF instance");
        if(instance)
        {
            double value=0;
            Check(runtime->GetGlobalFloat(instance,"stored",value) && value==test.value && std::signbit(value)==std::signbit(test.value),"MOVF global initializer");
            for(const char* function:{"Literal","Argument"})
            {
                auto call=runtime->Call(instance,function);
                Check(call.invoke()==RunStatus::Completed && call.retType()==ValueType::Float && call.retFloat()==test.value &&
                    std::signbit(call.retFloat())==std::signbit(test.value),"literal assignment and argument preserve double");
            }
            Check(runtime->GetGlobalFloat(instance,"stored",value) && value==test.value && std::signbit(value)==std::signbit(test.value),"MOVF global overwrite");
            runtime->DestroyInstance(instance);
        }
        runtime->DestroyProgram(program);
    }
}

int main()
{
    auto runtime=CreateRuntime();NativeObjectDesc desc;desc.name="Host";desc.method=Host;
    Check(runtime->RegisterObject(desc),"register");runtime->FreezeBindings();
    for(bool debug:{false,true})for(bool image:{false,true})
    {
        FloatImmediates(runtime,debug,image);
        CompileDesc compile;compile.source=source;compile.sourceName="double.ns";compile.includeDebugInfo=debug;
        ProgramHandle program;Error error;
        if(image){std::vector<uint8_t> bytes;error=runtime->CompileToBytecode(compile,bytes);if(error.ok())program=runtime->LoadProgram(bytes,&error);}
        else{auto result=runtime->Compile(compile);program=result.program;error=result.error;}
        Check(bool(program),error.message.c_str());if(!program)continue;
        auto instance=runtime->CreateInstance(program);
        {auto call=runtime->Call(instance,"Probe");Check(call.invoke()==RunStatus::Completed&&call.retBool(),"scalar precision and float vectors");}
        for(double value:{1.0000000000009094947017729282379150390625,16777217.0,1e300,-1e300,
            (std::numeric_limits<double>::max)(),std::numeric_limits<double>::denorm_min(),0.0,-0.0})
        {
            {auto call=runtime->Call(instance,"Echo");Check(call.argFloat(value).invoke()==RunStatus::Completed && call.retFloat()==value && std::signbit(call.retFloat())==std::signbit(value),"double host roundtrip");}
            {auto call=runtime->Call(instance,"Echo");auto result=call.argFloat(value).invokeR();Check(result.ok()&&result.asFloat()==value&&std::signbit(result.asFloat())==std::signbit(value),"owned double result");}
            double global=0;Check(runtime->SetGlobalFloat(instance,"global_value",value)&&runtime->GetGlobalFloat(instance,"global_value",global)&&global==value,"global double roundtrip");
            {auto call=runtime->Call(instance,"Collections");Check(call.argFloat(value).invoke()==RunStatus::Completed,"host collections");MapReader map;ListReader list;double a=0,b=0;
             Check(call.retMap(map)&&map.getFloat("value",a)&&map.getList("items",list)&&list.getFloat(0,b)&&a==value&&b==value,"map and list double values");}
            {auto call=runtime->Call(instance,"Array");Check(call.argFloat(value).invoke()==RunStatus::Completed,"array result");ArrayView array;
             Check(call.retArray(array)&&array.floats()&&array.count()==2&&array.floats()[0]==value&&array.floats()[1]==value,"double array stride");}
        }
        runtime->DestroyInstance(instance);runtime->DestroyProgram(program);
    }
    DestroyRuntime(runtime);
    std::printf("double_regression: %d checks, %d failures; scalar=%zu vector_component=%zu slot=%zu\n",checks,failures,sizeof(NS_FLOAT),sizeof(((VecInfo*)nullptr)->v[0]),sizeof(VarInfo));
    return failures?1:0;
}
