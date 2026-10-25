#include "NeoScript.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>
using namespace NeoScript;
static int checks=0, failures=0;
static void Check(bool ok,const std::string& name){++checks;if(!ok&&++failures<=40)std::fprintf(stderr,"FAIL %s\n",name.c_str());}
static int propertyTrace=0;
static bool Property(CallContext& context,StringView name,bool get){
 if(!get)return false;
 const auto n=name.str();
 if(n=="First"){propertyTrace=propertyTrace*10+1;context.retInt(3);return true;}
 if(n=="Second"){propertyTrace=propertyTrace*10+2;context.retInt(4);return true;}
 if(n=="Trace"){context.retInt(propertyTrace);return true;}
 return false;
}
class Loader final:public ILoader {
public:
 std::map<std::string,std::string> sources;
 bool Load(StringView path,std::vector<uint8_t>& out)override{auto p=sources.find(path.str());if(p==sources.end())return false;out.assign(p->second.begin(),p->second.end());return true;}
 StringView LibPath()const override{return "modules/";}
};
static void Run(IRuntime* runtime,const std::string& name,const std::string& source,bool debug,bool image,bool reject=false,const char* expectedError=nullptr){
 CompileDesc desc;desc.source=source;desc.sourceName="parser-regression.ns";desc.includeDebugInfo=debug;
 ProgramHandle program;Error error;
 if(image){std::vector<uint8_t> bytes;error=runtime->CompileToBytecode(desc,bytes);if(error.ok())program=runtime->LoadProgram(bytes,&error);}
 else{auto c=runtime->Compile(desc);program=c.program;error=c.error;}
 const auto label=name+" debug="+std::to_string(debug)+" image="+std::to_string(image);
 if(reject){Check(!program&&!error.message.empty()&&(!expectedError||error.message.find(expectedError)!=std::string::npos),label+" reject: "+error.message);}
 else{Check(bool(program),label+" compile: "+error.message);if(program){auto instance=runtime->CreateInstance(program);Check(bool(instance),label+" instance");if(instance){for(int repeat=0;repeat<3;++repeat){propertyTrace=0;auto call=runtime->Call(instance,"Probe");auto status=call.timeout(1000).invoke();Check(status==RunStatus::Completed,label+" run: "+call.error().message);if(status==RunStatus::Completed)Check(call.retType()==ValueType::Bool&&call.retBool(),label+" result");}runtime->DestroyInstance(instance);}}}
 if(program)runtime->DestroyProgram(program);
}
int main(){
 Loader loader;loader.sources["modules/a.ns"]="export var collision=1;";loader.sources["modules/b.ns"]="export fun collision(){return 2;}";
 RuntimeDesc runtimeDesc;runtimeDesc.loader=&loader;auto runtime=CreateRuntime(runtimeDesc);NativeObjectDesc host;host.name="Host";host.property=Property;runtime->RegisterObject(host);runtime->FreezeBindings();
 const std::vector<std::pair<std::string,std::string>> positive={
 {"uninitialized locals","fun Read(){var x;return x;}export fun Probe(){var old=17;return Read()==null;}"},
 {"uninitialized loop declaration","export fun Probe(){var good=true;for(var i in 0,4){var x;if(x!=null)good=false;x=7;}return good;}"},
 {"shadow initializer","var x=4;export fun Probe(){var x=x+1;{var x=x+1;if(x!=6)return false;}return x==5;}"},
 {"indexed bitwise assignments","import system;export fun Probe(){var a=[15];var m={\"x\":7};var d=system.array(8,1);a[0]&=6;a[0]|=8;a[0]^=2;m.x<<=2;m.x>>=1;d[0]>>=2;return a[0]==12&&m.x==14&&d[0]==2;}"},
 {"parameter shadows module","import math;fun Read(var math){return math.x;}export fun Probe(){return Read({\"x\":3})==3;}"},
 {"integer remainder and float division","const A=5%2;const B=-5%2;const C=5%-2;const D=-5%-2;const Q=5.5/2.0;const W=(16777216.0+1.0)-16777216.0;export fun Probe(){var matched=false;switch(1){case 5%2:matched=true;default:matched=false;}return A==1&&B==-1&&C==1&&D==-1&&Q==2.75&&W==1.0&&matched;}"},
 {"integer boundary","const MIN=-9223372036854775808;const BITS=0xffffffffffffffff;export fun Probe(){return MIN==-9223372036854775808&&BITS==-1&&0x8000000000000000==-9223372036854775808&&9223372036854775807>0;}"},
 {"unary operators","export fun Probe(){var x=5;return ~5==-6&&~x==-6&&~~x==5&&- -x==5&&-++x==-6&&x==6&&!false;}"},
 {"scientific literal rounding","export fun Probe(){return 2.06735e8==206735000.0 && -2.06735e8==-206735000.0;}"},
 {"host getter operand order","export fun Probe(){var r=Host.First+Host.Second*2;return r==11&&Host.Trace==12;}"},
 {"computed key result","var index=0;var holder=[[1],[2]];fun ChangeIndex(){index=1;return 5;}export fun Probe(){index=0;holder=[[1],[2]];holder[(index=0)][0]+=ChangeIndex();return holder[0][0]==6&&holder[1][0]==2&&index==1;}"},
 {"literal postfix","export fun Probe(){return [2,4][1]==4&&{\"x\":[7]}.x[0]==7&&tostring(25).len()==2;}"},
 {"nested unbraced control","export fun Probe(){var x=0;for(var i in 0,3)if(i>0)x++;else x+=2;while(x<6)if(x==4)x++;else x++;return x==6;}"},
 {"nested unbraced return","export fun Probe(){if(true)if(false)return false;else return true;return false;}"}
 };
 const std::vector<std::pair<std::string,std::string>> negative={
 {"self initializer","export fun Probe(){var x=x+1;return true;}"},
 {"named outer local","export fun Probe(){var outer=3;fun Inner(){return outer;}return Inner()==3;}"},
 {"decimal overflow","export fun Probe(){return 9223372036854775808;}"},
 {"negative overflow","export fun Probe(){return -9223372036854775809;}"},
 {"huge literal","const N=999999999999999999999999999999999999;"},
 {"hex overflow","export fun Probe(){return 0x10000000000000000;}"},
 {"unmatched closing","}"},{"unclosed block","export fun Probe(){{return true;}"},
 {"function variable collision","var Clash=1;fun Clash(){return 2;}"},
 {"export collision","import a;import b;"},
 {"native function value","import math;var f=math.Sin;"},
 {"nonfinite constant","const X=1.0/0.0;"}
 };
 for(bool debug:{false,true})for(bool image:{false,true}){
  for(const auto& c:positive)Run(runtime,c.first,c.second,debug,image);
  for(const auto& c:negative)Run(runtime,c.first,c.second,debug,image,true);
  for(const char* expression:{"5.5%2.0","5.5%2","5%2.0","-5.5%2.0","5.5%0.0","(5/2.0)%2"}){
   Run(runtime,"const float remainder",std::string("const VALUE=")+expression+";",debug,image,true,"invalid operation in const expression near '%'");
   Run(runtime,"case float remainder",std::string("export fun Probe(){switch(1){case ")+expression+":return true;default:return false;}}",debug,image,true,"invalid operation in const expression near '%'");
  }
  Run(runtime,"deep parentheses","export fun Probe(){return "+std::string(300,'(')+"true"+std::string(300,')')+";}",debug,image,true);
  Run(runtime,"deep blocks","export fun Probe(){"+std::string(300,'{')+"return true;"+std::string(301,'}'),debug,image,true);
  std::string chain;for(int i=0;i<300;++i)chain+="if(false)return false;else ";Run(runtime,"deep else if","export fun Probe(){"+chain+"return true;}",debug,image,true);
  std::string condition="false";for(int i=0;i<180;++i)condition+=" || x<0";Run(runtime,"large while condition","export fun Probe(){var x=1;while("+condition+"){return false;}return true;}",debug,image);
 }
 DestroyRuntime(runtime);std::printf("hunt_parser_regression: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
