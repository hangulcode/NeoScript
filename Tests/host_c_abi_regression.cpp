#include "NeoScriptC.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>

#if UINTPTR_MAX == UINT64_MAX
static_assert(sizeof(NsStr)==16 && sizeof(NsError)==48 && sizeof(NsJsonResult)==48, "64-bit string layouts");
static_assert(sizeof(NsCallResult)==64 && sizeof(NsRuntimeDesc)==64 && sizeof(NsObjectDesc)==40, "64-bit call layouts");
static_assert(sizeof(NsDefine)==40 && sizeof(NsCompileDesc)==80 && sizeof(NsInstanceDesc)==16, "64-bit descriptors");
#else
static_assert(sizeof(NsStr)==12 && sizeof(NsError)==40 && sizeof(NsJsonResult)==36, "32-bit string layouts");
static_assert(sizeof(NsCallResult)==64 && sizeof(NsRuntimeDesc)==40 && sizeof(NsObjectDesc)==32, "32-bit call layouts");
static_assert(sizeof(NsDefine)==32 && sizeof(NsCompileDesc)==64 && sizeof(NsInstanceDesc)==12, "32-bit descriptors");
#endif
static_assert(sizeof(NsAllocStats)==64, "allocation statistics layout");

static int checks=0, failures=0;
static void Check(bool ok,const char* label) { ++checks; if(!ok){++failures; std::printf("FAIL: %s\n",label);} }
static NsStr Str(const char* p) { return {p,static_cast<int32_t>(std::strlen(p)),0}; }
static bool Name(const char* p,int32_t n,const char* q) { return n==static_cast<int32_t>(std::strlen(q)) && std::memcmp(p,q,n)==0; }
struct Host
{
    NsRuntime* rt=nullptr;
    NsInstance instance=0;
    NsObjectType type=0;
    NsCall* retained=nullptr;
    NsMapReader* fromCallback=nullptr;
};
static int32_t NS_CALL Method(void* user,int32_t,NsContext* ctx,const char* name,int32_t len)
{
    Host& host=*static_cast<Host*>(user);
    if(Name(name,len,"Value")){NsCtxRetInt(ctx,77);return 1;}
    if(Name(name,len,"Object")){NsCtxRetObject(ctx,host.type,nullptr);return 1;}
    if(Name(name,len,"Objects"))
    {
        NsMapBuilder* map=NsCtxRetMap(ctx);
        NsMapSetObject(map,"obj",3,host.type,nullptr);
        NsListBuilder* list=NsMapSetList(map,"list",4);
        NsListPushObject(list,host.type,nullptr);
        NsMapSetInt(NsListPushMap(list),"n",1,9);
        return 1;
    }
    if(Name(name,len,"Dispose"))
    {
        Check(NsRuntimeTryDestroy(host.rt)==0,"active runtime destruction refused");
        Check(NsTryDestroyInstance(host.rt,host.instance)==0,"active instance destruction refused");
        NsRuntimeDestroy(host.rt);
        NsDestroyInstance(host.rt,host.instance);
        Check(NsIsAlive(host.rt,host.instance)!=0,"void destroy also retains active owner");
        NsCtxRetInt(ctx,13);
        return 1;
    }
    if(Name(name,len,"ReadOther"))
    {
        host.fromCallback=NsMapGetMap(NsCallRetMap(host.retained),"child",5);
        NsCtxRetInt(ctx,5);
        return 1;
    }
    return 0;
}
static NsRuntime* Runtime(Host& host)
{
    NsRuntimeDesc desc={};desc.structSize=sizeof(desc);desc.user=&host;desc.method=&Method;
    host.rt=NsRuntimeCreate(&desc);
    Check(host.rt!=nullptr,"runtime create");
    NsObjectDesc object={};object.structSize=sizeof(object);object.name=Str("Host");object.hasMethod=1;object.declareGlobal=1;
    int32_t slot=-1;
    Check(NsRegisterObject(host.rt,&object,&slot)!=0,"register Host");
    NsFreezeBindings(host.rt);
    host.type=NsGetObjectType(host.rt,"Host",4);
    Check(host.type!=0,"object type exists");
    return host.rt;
}
static const char* source=
    "import math;import system;export var object;export var objects={};"
    "export fun Map(var n){return {\"who\":n,\"child\":{\"n\":n},\"list\":[{\"n\":n}],\"array\":system.array(n,2)};}"
    "export fun List(var n){return [{\"n\":n},[{\"n\":n}],system.array(n,2)];}"
    "export fun Array(var n){return system.array(n,2);}"
    "export fun Array32(var n){return system.array32(n,2);}"
    "export fun Number(){return 42;}export fun Dispose(){return Host.Dispose()+1;}"
    "export fun ReadOther(){return Host.ReadOther();}"
    "export fun IsObject(){return object!=null;}export fun Value(){return object.Value();}"
    "export fun MapObject(){return objects.item!=null;}"
    "export fun ArgObject(var v){return v!=null;}"
    "export fun RetObject(){return Host.Object()!=null;}"
    "export fun BuiltObjects(){var m=Host.Objects();return m.obj!=null && m.list[0]!=null && m.list[1].n==9;}"
    "export fun Quaternion(){return math.Quaternion(1,2,3,4);}"
    "export fun EchoFloat(var x){return x;}";
static NsProgram Compile(NsRuntime* rt,bool debug,bool image)
{
    NsCompileDesc desc={};desc.structSize=sizeof(desc);desc.source=Str(source);desc.sourceName=Str("host-c-abi.ns");desc.includeDebugInfo=debug;
    NsError error={};NsProgram program=0;
    if(image){NsStr bytes={};Check(NsCompileToBytecode(rt,&desc,&bytes,&error)!=0,"compile image");program=NsLoadProgram(rt,bytes.data,bytes.len,&error);}
    else program=NsCompile(rt,&desc,&error);
    if(!program)std::printf("compile: %.*s\n",error.message.len,error.message.data);
    Check(program!=0,"compile program");return program;
}
static NsCall* Begin(NsRuntime* rt,NsInstance inst,const char* name)
{
    NsCall* call=NsCallBeginName(rt,inst,name,static_cast<int32_t>(std::strlen(name)));
    Check(call!=nullptr,"begin valid call");return call;
}
static int InvokeInt(NsRuntime* rt,NsInstance inst,const char* name)
{
    NsCall* call=Begin(rt,inst,name);NsCallResult result={};
    Check(NsCallInvokeR(call,&result)==NS_RUN_COMPLETED,"invoke value");NsCallEnd(call);return result.i;
}
static bool MapInt(NsMapReader* map,const char* key,int expected)
{
    int64_t value=-999;return NsMapGetInt(map,key,static_cast<int32_t>(std::strlen(key)),&value)!=0 && value==expected;
}
static bool ArrayInt(NsArrayView* a,int expected)
{
    int64_t* data=NsArrayInts(a);return NsArrayCount(a)==2 && data!=nullptr && data[0]==expected;
}
static void Readers(Host& host,NsProgram program,bool closeFirst)
{
    NsInstance a=NsCreateInstance(host.rt,program,nullptr),b=NsCreateInstance(host.rt,program,nullptr);
    Check(a && b,"reader instances");
    NsCall* ca=Begin(host.rt,a,"Map");NsCallArgInt(ca,1);
    NsCall* cb=Begin(host.rt,b,"List");NsCallArgInt(cb,2);
    Check(NsCallInvoke(ca)==NS_RUN_COMPLETED,"invoke A after B begins");
    NsMapReader* ma=NsCallRetMap(ca);
    NsMapReader* child=NsMapGetMap(ma,"child",5);
    NsMapReader* listChild=NsListRdGetMap(NsMapGetList(ma,"list",4),0);
    NsArrayView* array=NsMapGetArray(ma,"array",5);
    const NsStr* keys=nullptr;int32_t count=0;
    Check(NsMapStringKeys(child,&keys,&count,nullptr)!=0 && count==1,"keys owned by reader call");
    Check(NsCallInvoke(cb)==NS_RUN_COMPLETED,"invoke B");
    NsListReader* lb=NsCallRetList(cb);
    NsMapReader* bchild=NsListRdGetMap(lb,0);
    NsMapReader* nested=NsListRdGetMap(NsListRdGetList(lb,1),0);
    NsArrayView* barray=NsListRdGetArray(lb,2);
    NsCallEnd(closeFirst?ca:cb);
    NsInstance reusable=closeFirst?a:b;
    for(int i=0;i<12;++i)
    {
        NsCall* current=Begin(host.rt,reusable,"Map");NsCallArgInt(current,99);
        Check(NsCallInvoke(current)==NS_RUN_COMPLETED,"replacement call");
        NsMapReader* replacement=NsCallRetMap(current);
        (void)NsMapGetMap(replacement,"child",5);
        if(closeFirst)
        {
            Check(MapInt(bchild,"n",2) && MapInt(nested,"n",2),"non-LIFO surviving list descendants");
            Check(ArrayInt(barray,2),"non-LIFO surviving array");
        }
        else
        {
            Check(MapInt(ma,"who",1) && MapInt(child,"n",1) && MapInt(listChild,"n",1),"LIFO interleaving retains A descendants");
            Check(ArrayInt(array,1),"A nested array retained");
            Check(count==1 && Name(keys[0].data,keys[0].len,"n"),"A key descriptors retained");
        }
        NsCallEnd(current);
    }
    if(!closeFirst)
    {
        host.retained=ca;
        Check(InvokeInt(host.rt,b,"ReadOther")==5,"read live call from native callback");
        Check(MapInt(host.fromCallback,"n",1),"call reader created during callback survives callback");
        host.retained=nullptr;
    }
    NsCallEnd(closeFirst?cb:ca);
    NsCall* ac=Begin(host.rt,a,"Array");NsCallArgInt(ac,7);Check(NsCallInvoke(ac)==NS_RUN_COMPLETED,"direct array return");
    NsArrayView* av=NsCallRetArray(ac);
    Check(InvokeInt(host.rt,b,"Number")==42 && ArrayInt(av,7),"direct array survives other call");
    NsCallEnd(ac);NsDestroyInstance(host.rt,a);NsDestroyInstance(host.rt,b);
}
static void Run(bool debug,bool image)
{
    Host dead;Runtime(dead);const NsObjectType stale=dead.type;NsRuntimeDestroy(dead.rt);
    Host host;Runtime(host);const NsObjectType valid=host.type;
    Check(valid!=stale,"object type tokens are not recycled");
    Check(NsGetObjectType(host.rt,"Host",4)==valid,"same runtime type cache");
    NsProgram program=Compile(host.rt,debug,image);if(!program){NsRuntimeDestroy(host.rt);return;}
    host.instance=NsCreateInstance(host.rt,program,nullptr);Check(host.instance!=0,"instance create");
    Check(NsCallBeginName(host.rt,host.instance,"Missing",7)==nullptr,"missing call returns null");
    NsFunction* fn=NsFindFunction(host.rt,program,"Number",6);
    NsInstance deadInstance=NsCreateInstance(host.rt,program,nullptr);NsDestroyInstance(host.rt,deadInstance);
    Check(NsCallBegin(host.rt,deadInstance,fn)==nullptr,"dead instance call returns null");NsFunctionRelease(fn);
    Host foreign;Runtime(foreign);
    for(NsObjectType invalid : {stale,foreign.type})
    {
        NsBindGlobalObject(host.rt,host.instance,"object",6,invalid,nullptr);
        Check(InvokeInt(host.rt,host.instance,"IsObject")==0,"invalid type cannot bind global");
        NsBindGlobalMapObject(host.rt,host.instance,"objects",7,"item",4,invalid,nullptr);
        Check(InvokeInt(host.rt,host.instance,"MapObject")==0,"invalid type cannot bind map");
        host.type=invalid;Check(InvokeInt(host.rt,host.instance,"RetObject")==0,"invalid returned type rejected");
        Check(InvokeInt(host.rt,host.instance,"BuiltObjects")==0,"invalid builder type rejected");
    }
    host.type=valid;NsRuntimeDestroy(foreign.rt);
    NsBindGlobalObject(host.rt,host.instance,"object",6,valid,nullptr);
    Check(InvokeInt(host.rt,host.instance,"Value")==77,"fresh type uses fresh runtime binding");
    Check(InvokeInt(host.rt,host.instance,"BuiltObjects")==1,"valid nested builder objects");
    NsCall* call=Begin(host.rt,host.instance,"Dispose");
    Check(NsRuntimeTryDestroy(host.rt)==0 && NsTryDestroyInstance(host.rt,host.instance)==0,"prepared call blocks destruction");
    NsCallResult result={};Check(NsCallInvokeR(call,&result)==NS_RUN_COMPLETED && result.i==14,"execution continues after refused disposal");
    Check(NsRuntimeTryDestroy(host.rt)==0 && NsTryDestroyInstance(host.rt,host.instance)==0,"consumed live call blocks destruction");
    NsCallEnd(call);
    Check(InvokeInt(host.rt,host.instance,"Value")==77,"refused disposal preserves binding cache");
    call=Begin(host.rt,host.instance,"Quaternion");Check(NsCallInvokeR(call,&result)==NS_RUN_COMPLETED,"raw quaternion call");
    Check(result.f[0]==1 && result.f[1]==2 && result.f[2]==3 && result.f[3]==4,"C ABI retains quaternion script order");NsCallEnd(call);
    const double precise=1.0000000000009094947017729282379150390625;
    call=Begin(host.rt,host.instance,"EchoFloat");NsCallArgFloat(call,precise);
    Check(NsCallInvoke(call)==NS_RUN_COMPLETED && NsCallRetFloat(call)==precise,"C ABI double argument and result");NsCallEnd(call);
    call=Begin(host.rt,host.instance,"EchoFloat");NsCallArgFloat(call,precise);
    Check(NsCallInvokeR(call,&result)==NS_RUN_COMPLETED && result.d==precise,"C ABI double snapshot");NsCallEnd(call);
    call=Begin(host.rt,host.instance,"Array32");NsCallArgInt(call,-7);
    Check(NsCallInvoke(call)==NS_RUN_COMPLETED,"C ABI int32 array call");
    NsArrayView* a32=NsCallRetArray(call);int32_t* ints32=NsArrayInts32(a32);
    Check(NsArrayElementType(a32)==NS_ELEM_INT32 && NsArrayCount(a32)==2 && ints32 && ints32[0]==-7 && ints32[1]==-7 && !NsArrayInts(a32) && !NsArrayFloats32(a32),"C ABI int32 view");
    if(ints32){ints32[1]=INT32_MAX;Check(ints32[0]==-7 && ints32[1]==INT32_MAX,"int32 pointer stride");}NsCallEnd(call);
    call=Begin(host.rt,host.instance,"Array32");NsCallArgFloat(call,16777217.0);
    Check(NsCallInvoke(call)==NS_RUN_COMPLETED,"C ABI float32 array call");
    a32=NsCallRetArray(call);float* floats32=NsArrayFloats32(a32);
    Check(NsArrayElementType(a32)==NS_ELEM_FLOAT32 && floats32 && floats32[0]==16777216.0f && floats32[1]==16777216.0f && !NsArrayFloats(a32) && !NsArrayInts32(a32),"C ABI float32 view");
    if(floats32){floats32[1]=1.25f;Check(floats32[0]==16777216.0f && floats32[1]==1.25f,"float32 pointer stride");}NsCallEnd(call);
    Check(!NsArrayInts32(nullptr) && !NsArrayFloats32(nullptr),"empty narrow views");
    Readers(host,program,false);Readers(host,program,true);
    Check(NsTryDestroyInstance(host.rt,host.instance)!=0,"instance disposal allowed after calls end");
    NsDestroyProgram(host.rt,program);Check(NsRuntimeTryDestroy(host.rt)!=0,"runtime disposal allowed after calls end");
}
struct HttpState { std::atomic<bool> entered{false}, cancelled{false}; bool block=false; };
static void NS_CALL HttpSend(void* user,const NsHttpRequest* request,void* output)
{
    auto& state=*static_cast<HttpState*>(user);
    state.entered=true;
    if(state.block)
    {
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!NsHttpIsCancelled(request) && std::chrono::steady_clock::now()<until)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        state.cancelled=NsHttpIsCancelled(request)!=0;
        NsHttpRespond(output,0,"cancelled",9);return;
    }
    NsStr name={},value={};NsStr method=NsHttpMethod(request),url=NsHttpUrl(request),body=NsHttpBody(request);
    const bool ok=Name(method.data,method.len,"POST") && Name(url.data,url.len,"https://test") &&
        Name(body.data,body.len,"payload") && NsHttpTimeout(request)==-1 && NsHttpHeaderCount(request)==1 &&
        NsHttpHeader(request,0,&name,&value) && Name(name.data,name.len,"X-Test") && Name(value.data,value.len,"v");
    std::string response="copied response";NsHttpRespond(output,ok?1:0,response.data(),(int)response.size());
}
static void HttpTransport(bool block)
{
    HttpState state;state.block=block;NsRuntimeDesc desc={};desc.structSize=sizeof(desc);
    auto rt=NsRuntimeCreateWithHttpTransport(&desc,HttpSend,&state);Check(rt!=nullptr,"C HTTP runtime");if(!rt)return;
    std::string text="import system;var ok=false;fun Done(var success,var text){ok=success && text==\"copied response\";}"
        "var a=system.aysnc_create();a.add_header(\"X-Test\",\"v\");a.post(-1,\"https://test\",\"payload\",Done);";
    if(!block)text+="a.wait();export fun Probe(){if(ok)return 1;return 0;}";
    NsCompileDesc cd={};cd.structSize=sizeof(cd);cd.source=Str(text.c_str());NsError error={};auto p=NsCompile(rt,&cd,&error);
    Check(p!=0,"C HTTP compile");if(!p)std::printf("%.*s\n",error.message.len,error.message.data);
    if(p)
    {
        NsInstanceDesc id={};id.structSize=sizeof(id);auto instance=NsCreateInstance(rt,p,&id);
        Check(NsRunGlobalInit(rt,instance)==NS_RUN_COMPLETED,"C HTTP init");
        if(!block)Check(InvokeInt(rt,instance,"Probe")==1,"C HTTP response copied and callback invoked");
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!state.entered && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Check(state.entered,"C HTTP callback started");
    }
    NsRuntimeDestroy(rt);
    if(block)Check(state.cancelled,"C HTTP shutdown cancellation");
}

int main()
{
    HttpTransport(false);HttpTransport(true);
    for(bool debug:{false,true})for(bool image:{false,true})Run(debug,image);
    std::printf("host_c_abi_regression: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
