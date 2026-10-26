#include "NeoScript.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& name) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name.c_str()); } }
static bool Special(CallContext& c, StringView name) {
    if (name.str() == "BadUtf8") c.retString(StringView("\xc0\xaf", 2));
    else if (name.str() == "Nul") c.retString(StringView("a\0b", 3));
    else if (name.str() == "Nan") c.retFloat(std::numeric_limits<float>::quiet_NaN());
    else if (name.str() == "Inf") c.retFloat(std::numeric_limits<float>::infinity());
    else if (name.str() == "Encode") { JsonResult result; std::string text; if (c.argToJson(0, text, result, true)) c.retString(text); else c.retNull(); }
    else if (name.str() == "Decode") { JsonResult result; c.retJson(c.argString(0), result); }
    else return false;
    return true;
}
static void Script(IRuntime* runtime, const std::string& body, const std::string& name) {
    for (bool debug : {false, true}) {
        std::string text = "import system; import math; export fun Test(){" + body + "}";
        CompileDesc desc; desc.source = text; desc.sourceName = "json.ns"; desc.includeDebugInfo = debug;
        auto compiled = runtime->Compile(desc);
        if (!compiled.program) { Check(false, name + ": " + compiled.error.message); continue; }
        auto instance = runtime->CreateInstance(compiled.program);
        { auto call = runtime->Call(instance, "Test"); auto state = call.invoke();
          Check(state == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(), name + ": " + call.error().message); }
        runtime->DestroyInstance(instance); runtime->DestroyProgram(compiled.program);
    }
}
int main() {
    auto runtime = CreateRuntime(); NativeObjectDesc special; special.name = "Special"; special.method = Special;
    runtime->RegisterObject(special); runtime->FreezeBindings();
    Script(runtime, R"(var d={"z":100.0,"a":[null,1,true,{"z":0.1,"a":"hello"}]}; var s=system.jsonEncode(d,true); return s=="{\"a\":[null,1,true,{\"a\":\"hello\",\"z\":0.1}],\"z\":100.0}" && Special.Encode(d)==s && type(system.jsonDecode(s).z)=="float" && type(system.jsonDecode(s).a[1])=="int";)", "recursive sorting and host parity");
    Script(runtime, R"(var d=system.jsonDecode("{\"x\":1,\"x\":2,\"gone\":1,\"gone\":null,\"list\":[null]}"); return d.x==2 && d.gone==null && d.list.len()==1 && system.jsonEncode(d,true)=="{\"list\":[null],\"x\":2}";)", "duplicate keys and null");
    Script(runtime, R"(var s="a\n\t\"\\🚀한글"; return system.jsonDecode(system.jsonEncode(s))==s && Special.Decode(system.jsonEncode(s))==s;)", "Unicode and escaping");
    Script(runtime, R"(var a={"x":[1]}; var d=system.jsonDecode(system.jsonEncode([a,a])); d[0].x[0]=2; return d[1].x[0]==1;)", "shared reference becomes independent");
    Script(runtime, R"(var a=[]; a.append(a); var s=system.jsonEncode({"a":a}); return s==null && system.jsonError().find("$.a[0]")>=0;)", "cyclic list");
    Script(runtime, R"(var a={}; a.x=a; return system.jsonEncode(a)==null && system.jsonError().find("$.x")>=0;)", "cyclic map");
    Script(runtime, R"(var d={}; d[5]=7; return system.jsonEncode(d)==null && system.jsonError().find("non-string key")>=0;)", "non-string keys");
    for (const auto& expr : {"math.Vector2(1,2)", "system.array(0,3)", "Special", "fun(){return 1;}", "Special.Nan()", "Special.Inf()", "Special.BadUtf8()", "Special.Nul()"})
        Script(runtime, std::string("return system.jsonEncode({\"a\":[") + expr + "]})==null && system.jsonError().find(\"$.a[0]\")>=0;", std::string("reject ") + expr);
    Script(runtime, R"(return system.jsonDecode("null")==null && system.jsonError()=="" && system.jsonEncode(null)=="null";)", "successful null root");
    Script(runtime, R"(system.jsonDecode("{"); var error=system.jsonError(); var a=system.jsonDecode("[1,1.0,1e0,-2147483648,2147483647]"); return error!="" && system.jsonError()=="" && type(a[0])=="int" && type(a[1])=="float" && type(a[2])=="float" && a[3]==-2147483648 && a[4]==2147483647;)", "numeric tokens and clearing error");
    for (const char* token : {"9223372036854775808", "-9223372036854775809", "18446744073709551616", "9999999999999999999999999999999999999999999999", "1e9999", "1.8e308"})
        Script(runtime, std::string("return system.jsonDecode(\"{\\\"a\\\":[") + token + "]}\")==null && system.jsonError().find(\"$.a[0]\")>=0;", token);
    Script(runtime, R"(var a=0; var i=0; while(i<64) { a=[a]; i++; } var s=system.jsonEncode(a); if(s==null || system.jsonDecode(s)==null) return false; return system.jsonEncode([a])==null && system.jsonDecode("["..s.."]")==null;)", "depth 64 and 65");
    Script(runtime, R"(return system.jsonDecode("{\"a\":\"\\u0000\"}")==null && system.jsonError().find("$.a")>=0;)", "NUL rejection");
    Script(runtime, R"(return system.jsonEncode(1,7)==null && system.jsonError().find("sortKeys")>=0;)", "invalid option");
    Script(runtime, R"(var a={"x":1.0}; a=system.jsonEncode(a); a=system.jsonDecode(a); return a.x==1.0;)", "return alias");

    CompileDesc desc; desc.source = "import system; export fun Encode(var x){return system.jsonEncode(x);} export fun Decode(var text){return system.jsonDecode(text);} export fun Error(){return system.jsonError();}";
    auto program = runtime->Compile(desc); Check(bool(program.program), "float fixture compiles");
    if (program.program) {
        auto instance = runtime->CreateInstance(program.program), second = runtime->CreateInstance(program.program);
        { auto call = runtime->Call(instance, "Decode"); call.argString("{"); call.invoke(); }
        { auto call = runtime->Call(second, "Error"); call.invoke(); Check(call.retString().str().empty(), "isolated error state"); }
        { auto call = runtime->Call(instance, "Decode"); call.argString(StringView("\xef\xbb\xbf{\"x\":1}", 10)); Check(call.invoke() == RunStatus::Completed && call.retType() == ValueType::Map, "BOM accepted"); }
        uint64_t state = 12345;
        for (int i = 0; i < 2008; ++i) {
            state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
            const uint64_t edges[] = {UINT64_C(0x7fefffffffffffff),UINT64_C(0xffefffffffffffff),UINT64_C(0x0010000000000000),UINT64_C(0x8010000000000000),1,UINT64_C(0x8000000000000001),0,UINT64_C(0x8000000000000000)};
            if (i >= 2000) state = edges[i - 2000];
            double input; std::memcpy(&input, &state, 8); if (!std::isfinite(input)) continue;
            std::string json;
            { auto call = runtime->Call(instance, "Encode"); call.argFloat(input); if (call.invoke() == RunStatus::Completed) json = call.retString().str(); }
            { auto call = runtime->Call(instance, "Decode"); call.argString(json); auto status = call.invoke(); double output = call.retFloat();
              Check(status == RunStatus::Completed && call.retType() == ValueType::Float && std::memcmp(&input,&output,8)==0, "float64 bit roundtrip " + json); }
        }
        runtime->DestroyInstance(second); runtime->DestroyInstance(instance); runtime->DestroyProgram(program.program);
    }
    DestroyRuntime(runtime); std::printf("json: %d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
}
