#include "NeoScript.h"
#include "NeoScriptC.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace NeoScript;
static int checks=0, failures=0;
static void Check(bool ok,const char* text) { ++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",text);} }
static const float values[4]={1,2,3,4};
static ValueType Type(int n){return n==2?ValueType::Vec2:n==3?ValueType::Vec3:ValueType::Vec4;}
// 전달하는 영역의 앞뒤와 미사용 성분을 함께 검사한다.
struct Buffer { float before=91, v[4]={71,72,73,74}, after=92; };
static bool Copied(const Buffer& b,int n) { if(b.before!=91||b.after!=92)return false;for(int j=0;j<4;++j)if(b.v[j]!=(j<n?values[j]:float(71+j)))return false;return true; }
static bool Host(CallContext& c,StringView name)
{
    if(name.str()=="Read") {
        const int dim=int(c.argInt(1));
        for(int n=1;n<=5;++n){Buffer b;bool ok=c.argVec(0,b.v,n);Check(ok==(n>=2&&n<=dim),"ctx read dimension");Check(Copied(b,ok?n:0),"ctx read exact count/no failure mutation");}
        Buffer b;Check(!c.argVec(99,b.v,2)&&Copied(b,0),"missing argument untouched");Check(!c.argVec(0,nullptr,2),"null argument output");
        Check(c.argQuat(0,b.v)==(dim==4),"quaternion needs four components");
        c.retBool(true);return true;
    }
    if(name.str()=="Echo") {
        const int dim=int(c.argInt(1));float v[4];if(!c.argVec(0,v,dim))return false;
        c.retInt(19);Check(!c.retVec(v,1)&&!c.retVec(nullptr,dim),"ret invalid dimensions");
        Check(c.retVec(v,dim),"ret vector");return true;
    }
    if(name.str()=="Build") {
        auto map=c.retMap();
        map.setVec2("fixed2",1,2);map.setVec3("fixed3",1,2,3);map.setVec4("fixed4",1,2,3,4);
        for(int n=2;n<=4;++n){const std::string key="v"+std::to_string(n);Check(map.setVec(key,values,n),"map write");}
        Check(map.setQuat("q",values),"map quaternion");Check(!map.setVec("absent",values,5)&&!map.setVec("v2",nullptr,2),"map invalid write unchanged");
        auto list=map.setList("items");list.pushVec2(1,2);list.pushVec3(1,2,3);list.pushVec4(1,2,3,4);
        Check(list.pushQuat(values),"list quaternion");Check(!list.pushVec(values,1)&&!list.pushVec(nullptr,4)&&list.count()==4,"invalid push appends nothing");
        Check(list.setVec(0,values,2)&&list.setQuat(3,values),"list direct replace");Check(!list.setVec(-1,values,2)&&!list.setVec(4,values,2),"list write bounds");
        return true;
    }
    if(name.str()=="Array") {
        ArrayView a;Check(c.argAsArray(0,a),"array view");const int dim=int(c.argInt(1));
        for(int n=1;n<=5;++n){Buffer b;bool ok=a.getVec(0,b.v,n);Check(ok==(n>=2&&n<=dim),"array prefix dimension");Check(Copied(b,ok?n:0),"array read canaries");}
        Buffer b;Check(!a.getVec(-1,b.v,2)&&!a.getVec(2,b.v,2)&&Copied(b,0),"array invalid bounds untouched");
        float small[2]={9,8};float medium[3]={9,8,7};
        Check(!a.setVec(0,small,dim==2?3:2),"array exact write dimension");
        Check(a.setVec(1,dim==2?small:dim==3?medium:values,dim),"array sized write");
        float old[4]={9,9,9,9};Check(a.getVec(0,old),"array legacy read");for(int i=0;i<4;++i)Check(old[i]==(i<dim?values[i]:0),"legacy array padding");
        c.retBool(true);return true;
    }
    return false;
}
static const char* source=R"NS(import math;import system;
export fun Read(var x,var n){return Host.Read(x,n);}
export fun Echo(var x,var n){return Host.Echo(x,n);}
export fun Plain(var x){return x;}
export fun Built(){return Host.Build();}
export fun Array(var n){if(n==2)return Host.Array(system.array(math.Vector2(1,2),2),n);if(n==3)return Host.Array(system.array(math.Vector3(1,2,3),2),n);return Host.Array(system.array(math.Vector4(1,2,3,4),2),n);}
export fun RawArray(var n){if(n==2)return system.array(math.Vector2(1,2),2);if(n==3)return system.array(math.Vector3(1,2,3),2);return system.array(math.Vector4(1,2,3,4),2);}
)NS";
static void Readers(MapReader& m) {
    Check(m.type("absent")==ValueType::None,"invalid map insertion absent");
    for(int dim=2;dim<=4;++dim){const std::string key="v"+std::to_string(dim);
        for(int n=1;n<=5;++n){Buffer b;bool ok=m.getVec(key,b.v,n);Check(ok==(n>=2&&n<=dim),"map read dimension");Check(Copied(b,ok?n:0),"map read canaries");}
        float old[4];Check(m.getVec(key,old),"map legacy");for(int i=0;i<4;++i)Check(old[i]==(i<dim?values[i]:0),"map legacy padding");
    }
    Buffer b;Check(!m.getVec("absent",b.v,2)&&Copied(b,0),"missing map untouched");Check(m.getQuat("q",b.v)&&Copied(b,4),"map quat wxyz");
    ListReader list;Check(m.getList("items",list)&&list.count()==4,"list reader");
    for(int i=0;i<4;++i){const int dim=i==3?4:i+2;for(int n=1;n<=5;++n){Buffer out;bool ok=list.getVec(i,out.v,n);Check(ok==(n>=2&&n<=dim),"list read dimension");Check(Copied(out,ok?n:0),"list read canaries");}}
    Buffer out;Check(!list.getVec(-1,out.v,2)&&!list.getVec(4,out.v,2)&&Copied(out,0),"list read bounds");
}
static void CppTest(bool debug,bool image) {
    AllocStats before;GetAllocStats(before);
    auto* rt=CreateRuntime();NativeObjectDesc host;host.name="Host";host.method=Host;rt->RegisterObject(host);rt->FreezeBindings();
    CompileDesc d;d.source=source;d.sourceName="vector-api.ns";d.includeDebugInfo=debug;ProgramHandle p;Error e;
    if(image){std::vector<uint8_t>b;e=rt->CompileToBytecode(d,b);if(e.ok())p=rt->LoadProgram(b,&e);}else{auto r=rt->Compile(d);p=r.program;e=r.error;}
    Check(bool(p),"C++ compile");if(!p){std::fprintf(stderr,"%s\n",e.message.c_str());DestroyRuntime(rt);return;}
    auto inst=rt->CreateInstance(p);
    for(int dim=0;dim<=4;++dim){
        if(dim==1)continue;
        {auto call=rt->Call(inst,"Read");if(dim)Check(call.argVec(values,dim),"invocation vector argument");else call.argInt(7);call.argInt(dim);Check(call.invoke()==RunStatus::Completed&&call.retBool(),"ctx input probe");}
        if(!dim)continue;
        {auto call=rt->Call(inst,"Echo");Check(!call.argVec(values,1)&&!call.argVec(nullptr,dim),"invalid argument appends nothing");
            if(dim==2)call.argVec2(1,2);else if(dim==3)call.argVec3(1,2,3);else Check(call.argQuat(values),"invocation quat argument");call.argInt(dim);
            Check(call.invoke()==RunStatus::Completed&&call.retType()==Type(dim),"dimension returned");
            for(int n=1;n<=5;++n){Buffer b;bool ok=call.retVec(b.v,n);Check(ok==(n>=2&&n<=dim),"invocation read dimension");Check(Copied(b,ok?n:0),"invocation read canaries");}
            Buffer q;Check(call.retQuat(q.v)==(dim==4),"invocation quaternion");
            float legacy[4];call.retVec(legacy);for(int j=0;j<4;++j)Check(legacy[j]==(j<dim?values[j]:0),"legacy invocation padding");
        }
        {auto call=rt->Call(inst,"Plain");Check(call.argVec(values,dim),"snapshot arg");auto r=call.invokeR();for(int n=1;n<=5;++n){Buffer b;bool ok=r.asVec(b.v,n);Check(ok==(n>=2&&n<=dim)&&Copied(b,ok?n:0),"snapshot dimension/canaries");}}
        {auto call=rt->Call(inst,"Array");call.argInt(dim);Check(call.invoke()==RunStatus::Completed&&call.retBool(),"array probe");}
    }
    {auto call=rt->Call(inst,"Built");Check(call.invoke()==RunStatus::Completed,"collection builders");MapReader map;Check(call.retMap(map),"return map");Readers(map);}
    // 호출이 끝난 뒤에도 결과 사본이 벡터 값을 유지하는지 검사한다.
    CallResult snapshot;{auto call=rt->Call(inst,"Plain");call.argVec4(1,2,3,4);snapshot=call.invokeR();}{auto call=rt->Call(inst,"Plain");call.argInt(0);call.invoke();}Buffer b;Check(snapshot.asQuat(b.v)&&Copied(b,4),"owned vector snapshot");
    rt->DestroyInstance(inst);rt->DestroyProgram(p);DestroyRuntime(rt);
    AllocStats after;GetAllocStats(after);Check(after.vectors==before.vectors&&after.arrays==before.arrays&&after.bufferBytes==before.bufferBytes,"no vector/array allocation leak");
}
static bool Name(const char* p,int n,const char* s){return n==int(std::strlen(s))&&!std::memcmp(p,s,n);}
static int32_t NS_CALL CMethod(void*,int32_t,NsContext* ctx,const char* name,int32_t len){
    if(Name(name,len,"Echo")){const int dim=int(NsCtxArgInt(ctx,1));Buffer b;Check(NsCtxArgVecN(ctx,0,b.v,dim)&&Copied(b,dim),"C ctx sized read");Check(!NsCtxArgVecN(ctx,-1,b.v,dim),"C ctx invalid index");return NsCtxRetVec(ctx,b.v,dim);}
    if(Name(name,len,"Build")){auto* m=NsCtxRetMap(ctx);Check(NsMapSetVec(m,"v2",2,values,2),"C map write");Check(!NsMapSetVec(m,"absent",6,values,1),"C map invalid write");auto* l=NsMapSetList(m,"items",5);for(int n=2;n<=4;++n)Check(NsListPushVec(l,values,n),"C list push");Check(NsListSetVec(l,0,values,2)&&!NsListSetVec(l,9,values,2),"C list write bounds");return 1;}
    return 0;
}
static void CTest(){
    Check(NsGetAbiVersion()==8,"ABI 8");NsRuntimeDesc rd={};rd.structSize=sizeof(rd);rd.method=CMethod;auto* rt=NsRuntimeCreate(&rd);
    NsObjectDesc ob={};ob.structSize=sizeof(ob);ob.name={"Host",4,0};ob.hasMethod=1;ob.declareGlobal=1;int32_t slot;Check(NsRegisterObject(rt,&ob,&slot),"C register");NsFreezeBindings(rt);
    NsCompileDesc cd={};cd.structSize=sizeof(cd);cd.source={source,int(std::strlen(source)),0};cd.sourceName={"vector-api.ns",13,0};NsError e={};auto p=NsCompile(rt,&cd,&e);Check(p!=0,"C compile");if(!p){std::fprintf(stderr,"%.*s\n",e.message.len,e.message.data);NsRuntimeDestroy(rt);return;}
    auto inst=NsCreateInstance(rt,p,nullptr);
    for(int dim=2;dim<=4;++dim){auto* call=NsCallBeginName(rt,inst,"Echo",4);Check(!NsCallArgVec(call,values,5)&&NsCallArgVec(call,values,dim),"C arg dimension");NsCallArgInt(call,dim);Check(NsCallInvoke(call)==NS_RUN_COMPLETED,"C invoke");for(int n=1;n<=5;++n){Buffer b;bool ok=NsCallRetVecN(call,b.v,n)!=0;Check(ok==(n>=2&&n<=dim)&&Copied(b,ok?n:0),"C return size and canaries");}NsCallEnd(call);
        call=NsCallBeginName(rt,inst,"Echo",4);NsCallArgVec(call,values,dim);NsCallArgInt(call,dim);NsCallResult r;std::memset(&r,0x7f,sizeof(r));Check(NsCallInvokeR(call,&r)==NS_RUN_COMPLETED,"C snapshot");for(int j=0;j<4;++j)Check(r.f[j]==(j<dim?values[j]:0),"C result padding");NsCallEnd(call);
        call=NsCallBeginName(rt,inst,"RawArray",8);NsCallArgInt(call,dim);Check(NsCallInvoke(call)==NS_RUN_COMPLETED,"C array invoke");auto* a=NsCallRetArray(call);for(int n=1;n<=5;++n){Buffer b;bool ok=NsArrayGetVecN(a,0,b.v,n)!=0;Check(ok==(n>=2&&n<=dim)&&Copied(b,ok?n:0),"C array read");}NsCallEnd(call);
    }
    auto* call=NsCallBeginName(rt,inst,"Built",5);Check(NsCallInvoke(call)==NS_RUN_COMPLETED,"C builder invoke");auto* m=NsCallRetMap(call);Buffer b;Check(NsMapGetVecN(m,"v2",2,b.v,2)&&Copied(b,2),"C map reader");Buffer bad;Check(!NsMapGetVecN(m,"v2",2,bad.v,3)&&Copied(bad,0),"C map short vector");auto* l=NsMapGetList(m,"items",5);for(int i=0;i<3;++i){Buffer out;Check(NsListRdGetVecN(l,i,out.v,i+2)&&Copied(out,i+2),"C list reader");}NsCallEnd(call);
    NsDestroyInstance(rt,inst);NsDestroyProgram(rt,p);NsRuntimeDestroy(rt);
}
int main(){for(bool debug:{false,true})for(bool image:{false,true})CppTest(debug,image);CTest();std::printf("vector API: %d checks, %d failures\n",checks,failures);return failures?1:0;}
