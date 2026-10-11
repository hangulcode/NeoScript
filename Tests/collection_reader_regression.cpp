#include "NeoScript.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace NeoScript;
static bool Host(CallContext& ctx, StringView method)
{
    const std::string name=method.str();
    if(name=="Text") {ctx.retString(StringView("a\0b",3));return true;}
    if(name=="Nulls") {auto list=ctx.retList();list.pushInt(1);list.pushNull();list.pushInt(3);return true;}
    if(name=="CheckText") {ctx.retBool(ctx.argString(0).size()==3);return true;}
    if(name=="CheckMap")
    {
        MapReader map; if(!ctx.argAsMap(0,map))return false;
        std::vector<StringView> keys; ValueType invalid=ValueType::None;
        bool ok=map.stringKeys(keys,&invalid) && keys.size()==4;
        StringView text; ok=ok && map.getString("text",text) && text.size()==3;
        MapReader alias; ok=ok && map.getMap("self",alias) && alias.sameStorage(map);
        ListReader list, second; ok=ok && map.getList("list",list) && map.getList("alias",second) && list.sameStorage(second);
        ok=ok && list.type(1)==ValueType::None && list.getString(2,text) && text.size()==3;
        ctx.retBool(ok);return true;
    }
    if(name=="CheckInvalidKeys")
    {
        MapReader map;if(!ctx.argAsMap(0,map))return false;
        std::vector<StringView> keys{StringView("old")};ValueType invalid=ValueType::None;
        ctx.retBool(!map.stringKeys(keys,&invalid) && keys.empty() && invalid==ValueType::Int);return true;
    }
    if(name=="CheckObject")
    {
        MapReader map; ListReader list;
        bool ok=ctx.argIsNativeObject(0) && ctx.argObjectUserData(0)==nullptr &&
            ctx.argIsNativeObject(3) && ctx.argObjectUserData(3)!=nullptr &&
            !ctx.argIsNativeObject(1) && !ctx.argIsNativeObject(99) &&
            ctx.argAsMap(1,map) && ctx.argAsList(2,list);
        ok=ok && map.type("object")==ValueType::None && map.isNativeObject("object") &&
            !map.isNull("object") && !map.isNativeObject("number") && !map.isNull("number") &&
            !map.isNativeObject("missing") && !map.isNull("missing") &&
            list.isNativeObject(0) && !list.isNull(0) && list.isNull(1) && !list.isNativeObject(1) &&
            !list.isNull(2) && !list.isNativeObject(2) &&
            list.type(3)==ValueType::None && !list.isNull(3) && !list.isNativeObject(3) &&
            !list.isNull(99) && !list.isNativeObject(99);
        ctx.retBool(ok);return true;
    }
    return false;
}
int main()
{
    auto runtime=CreateRuntime();NativeObjectDesc desc;desc.name="Host";desc.method=&Host;
    runtime->RegisterObject(desc);
    int payload=1; NativeObjectDesc other=desc;other.name="WithData";other.userData=&payload;
    runtime->RegisterObject(other);runtime->FreezeBindings();
    const std::string source=R"(
import coroutine;
export fun Test(){
    var text=Host.Text();if(!Host.CheckText(text))return false;
    var list=[1,null,text];var map={"text":text,"list":list,"alias":list};map.self=map;
    if(!Host.CheckMap(map))return false;
    var mixed={"a":1};mixed[7]=2;if(!Host.CheckInvalidKeys(mixed))return false;
    if(!Host.CheckObject(Host,{"object":Host,"number":1},
        [Host,null,1,coroutine.create(fun(){})],WithData))return false;
    var nulls=Host.Nulls();return nulls.len()==3 && nulls[0]==1 && nulls[1]==null && nulls[2]==3;
})";
    int failures=0;
    for(bool debug:{false,true})for(bool image:{false,true})
    {
        CompileDesc compile;compile.source=source;compile.sourceName="collection-readers.ns";compile.includeDebugInfo=debug;
        ProgramHandle program;Error error;
        if(image){std::vector<uint8_t> bytes;error=runtime->CompileToBytecode(compile,bytes);if(error.ok())program=runtime->LoadProgram(bytes,&error);}
        else{auto result=runtime->Compile(compile);program=result.program;error=result.error;}
        bool ok=false;
        if(program)
        {
            auto instance=runtime->CreateInstance(program);
            {auto call=runtime->Call(instance,"Test");ok=call.invoke()==RunStatus::Completed && call.retBool();if(!ok)error=call.error();}
            runtime->DestroyInstance(instance);runtime->DestroyProgram(program);
        }
        if(!ok){++failures;std::fprintf(stderr,"FAIL debug=%d image=%d: %s\n",debug,image,error.message.c_str());}
    }
    DestroyRuntime(runtime);std::printf("collection readers: 4 checks, %d failures\n",failures);return failures?1:0;
}
