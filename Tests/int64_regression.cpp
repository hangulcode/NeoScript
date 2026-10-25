#include "NeoScript.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
static_assert(!std::is_convertible<int, NeoScript::ProgramHandle>::value, "program handles reject integers");
static_assert(!std::is_convertible<int, NeoScript::InstanceHandle>::value, "instance handles reject integers");
static_assert(!std::is_convertible<int, NeoScript::DefineSetHandle>::value, "define handles reject integers");
using namespace NeoScript;
static int checks=0, failures=0;
static void Check(bool ok,const char* label) { ++checks; if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",label);} }
static bool Echo(CallContext& c,StringView) { c.retInt(c.argInt(0)); return true; }
static const char* source=R"NS(
import system;
const MAX=9223372036854775807;
const MIN=-9223372036854775808;
export var value=5000000000;
export fun Echo(var x){return Host.Echo(x);}
export fun Data(){return {"n":value,"list":[value],"array":system.array(value,2)};}
export fun Choose(var x){switch(x){case 5000000000:return 1;case 705032704:return 2;case MAX:return 3;default:return 0;}}
export fun Probe(){
 var max=MAX;var min=MIN;
 if(max+1!=MIN || min-1!=MAX || -min!=MIN || max*2!=-2)return false;
 if(MAX+1!=MIN || MIN-1!=MAX || -MIN!=MIN || MAX*2!=-2)return false;
 max++;min--;if(max!=MIN || min!=MAX)return false;
 if((1<<63)!=MIN || (MIN>>63)!=-1 || 0xffffffffffffffff!=-1)return false;
 if(toint("9223372036854775807")!=MAX || toint("-9223372036854775808")!=MIN)return false;
 if(tostring(MAX)!="9223372036854775807" || tostring(MIN)!="-9223372036854775808")return false;
 if(system.jsonDecode("9223372036854775807")!=MAX || system.jsonDecode("-9223372036854775808")!=MIN)return false;
 if(system.jsonEncode(MAX)!="9223372036854775807")return false;
 var a=system.array(value,2);a.append(MAX);a.reserve(10);var b=a.clone();
 if(b[0]!=value || b[2]!=MAX)return false;
 var m={};m[value]=MAX;m[705032704]=MIN;
 var set=system.set([value,705032704]);
 if(m[value]!=MAX || m[705032704]!=MIN || set.len()!=2)return false;
 var total=0;for(var i in MAX-2,MAX)total++;
 for(var i in MIN+2,MIN,-1)total++;
 return total==4 && Choose(value)==1 && Choose(705032704)==2 && Choose(MAX)==3;
}
)NS";
int main(){
 auto rt=CreateRuntime();NativeObjectDesc h;h.name="Host";h.method=Echo;rt->RegisterObject(h);rt->FreezeBindings();
 for(bool debug:{false,true})for(bool image:{false,true}){
  CompileDesc d;d.source=source;d.sourceName="int64.ns";d.includeDebugInfo=debug;
  ProgramHandle p;Error e;
  if(image){std::vector<uint8_t> bytes;e=rt->CompileToBytecode(d,bytes);if(e.ok())p=rt->LoadProgram(bytes,&e);}
  else {auto r=rt->Compile(d);p=r.program;e=r.error;}
  Check(bool(p),e.message.c_str());if(!p)continue;
  auto i=rt->CreateInstance(p);Check(bool(i),"create");
  if(i){
   {auto c=rt->Call(i,"Probe");Check(c.invoke()==RunStatus::Completed&&c.retBool(),"script endpoints and collections");}
   for(int64_t x:{INT64_MIN,INT64_MAX,INT64_C(5000000000),INT64_C(-5000000000)}){
    {auto c=rt->Call(i,"Echo");Check(c.argInt(x).invokeR().asInt()==x,"native callback and copied result");}
    int64_t out=0;Check(rt->SetGlobalInt(i,"value",x)&&rt->GetGlobalInt(i,"value",out)&&out==x,"global roundtrip");
    int32_t narrow=123;Check(!rt->GetGlobalInt(i,"value",narrow)&&narrow==123,"checked narrow global read");
    {auto c=rt->Call(i,"Data");Check(c.invoke()==RunStatus::Completed,"data return");MapReader m;ListReader l;ArrayView a;
     Check(c.retMap(m)&&m.getInt("n",out)&&out==x,"map read");
     Check(m.getList("list",l)&&l.getInt(0,out)&&out==x,"list read");
     Check(m.getArray("array",a)&&a.ints()&&a.ints()[0]==x&&a.ints()[1]==x,"wide array view");}
   }
   rt->DestroyInstance(i);
  }
  rt->DestroyProgram(p);
 }
 for(const char* expr:{"[3][4294967296]","math.Vector2(1,2)[4294967296]","system.array(1,2)[4294967296]","system.array(1,4294967296)","[].resize(4294967296)","[].reserve(4294967296)","system.array(0,0).resize(4294967296)","system.array(0,0).reserve(4294967296)","{}.reserve(4294967296)","1<<64"}){
  std::string text=std::string("import system;import math;export fun Probe(){return ")+expr+";}";CompileDesc d;d.source=text;
  auto r=rt->Compile(d);Check(bool(r.program),(std::string(expr)+": "+r.error.message).c_str());if(!r.program)continue;
  auto i=rt->CreateInstance(r.program);{auto c=rt->Call(i,"Probe");Check(c.invoke()==RunStatus::Failed,"wide index/count rejected");}
  rt->DestroyInstance(i);rt->DestroyProgram(r.program);
 }
 DestroyRuntime(rt);std::printf("int64: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
