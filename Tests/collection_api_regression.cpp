#include "NeoScript.h"
#include "NeoVMInternal.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace NeoScript;
static int checks = 0, failures = 0;
static void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label.c_str()); }
}
static ProgramHandle Compile(IRuntime* runtime, const std::string& source,
    bool debug, bool image, Error& error) {
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "collection-api.ns";
    desc.includeDebugInfo = debug;
    if (image) {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) return runtime->LoadProgram(bytes, &error);
        Check(bytes.empty(), "rejected source produces no image");
        return {};
    }
    auto result = runtime->Compile(desc);
    error = result.error;
    return result.program;
}
static void Run(IRuntime* runtime, const std::string& source, bool debug, bool image,
    const char* failure = nullptr) {
    runtime->CollectCycles(true);
    AllocStats before;
    GetAllocStats(before);
    Error error;
    auto program = Compile(runtime, source + "\nexport fun Recovery(){return 23;}", debug, image, error);
    Check(bool(program), "compile: " + source + " " + error.message);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "instance: " + source);
    if (instance) {
        {
            auto call = runtime->Call(instance, "Probe");
            const auto status = call.invoke();
            Check(failure ? status == RunStatus::Failed && call.error().message.find(failure) != std::string::npos :
                status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(),
                "execute: " + source + " " + call.error().message);
        }
        {
            auto call = runtime->Call(instance, "Recovery");
            Check(call.invoke() == RunStatus::Completed && call.retInt() == 23, "recovery: " + source);
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
    runtime->CollectCycles(true);
    AllocStats after;
    GetAllocStats(after);
    Check(before.maps == after.maps && before.lists == after.lists && before.sets == after.sets &&
        before.arrays == after.arrays && before.vectors == after.vectors && before.closures == after.closures &&
        before.bufferBytes == after.bufferBytes, "releases values: " + source);
}
static void CheckCapacity() {
    CNeoVM vm;
    VarInfo map, set(VAR_SET), list(VAR_LIST), array(VAR_ARRAY);
    vm.Var_SetTable(&map, vm.TableAlloc());
    set._set = vm.SetAlloc(); ++set._set->_refCount;
    list._lst = vm.ListAlloc(); ++list._lst->_refCount;
    VarInfo initial(3);
    array._array = vm.ArrayAlloc(NeoArrayElementType::Int, 2, initial); ++array._array->_refCount;
    map._tbl->Reserve(100); set._set->Reserve(100); list._lst->Reserve(100);
    vm.ArrayReserve(array._array, 100);
    SNeoVMAllocStats stats;
    vm.GetAllocStats(stats);
    Check(stats.bufferBytes == 100 * sizeof(int64_t), "reserve updates array allocation accounting");
    map._tbl->Insert(&initial, &initial); set._set->Insert(&initial); list._lst->InsertLast(&initial);
    const int mapCapacity = map._tbl->_BucketCapa, setCapacity = set._set->_BucketCapa;
    map._tbl->Clear(); set._set->Clear(); list._lst->Resize(0); vm.ArrayResize(array._array, 0);
    Check(map._tbl->_BucketCapa == mapCapacity && set._set->_BucketCapa == setCapacity &&
        list._lst->_BucketCapa == 100 && array._array->_capacity == 100, "clear retains allocated capacity");
    vm.Var_Release(&map); vm.Var_Release(&set); vm.Var_Release(&list); vm.Var_Release(&array);
    vm.CollectCycles(true);
    vm.GetAllocStats(stats);
    Check(stats.maps == 0 && stats.sets == 0 && stats.lists == 0 && stats.arrays == 0 && stats.bufferBytes == 0,
        "releases reserved collection storage");
}
int main() {
    CheckCapacity();
    auto runtime = CreateRuntime();
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true}) {
        for (const char* body : {
            R"ns(var a=[1,2,1];return a.find(1)==0 && a.find(9)==-1 && a.contains(2) && !a.contains("2");)ns",
            R"ns(var a=[16777217];return !a.contains(16777216) && !a.contains(16777216.0) && a.contains(16777217.0) && a.contains(16777217);)ns",
            R"ns(var a=[null,true,"abc"];return a.contains(null) && a.find(true)==1 && !a.contains(1) && a.find("ab".."c")==2;)ns",
            R"ns(var n=0.0 / 0.0;var a=[n,-0.0];return !a.contains(n) && a.find(0.0)==1;)ns",
            R"ns(var a=[];var b=[];var l=[a];return l.contains(a) && !l.contains(b) && ![].contains(null);)ns",
            R"ns(var f=fun(){return 1;};var l=[f];return l.contains(f) && !l.contains(fun(){return 1;});)ns",
            R"ns(var v=math.Vector2(1,2);var a=[v];return a.contains(math.Vector2(1,2)) && !a.contains(math.Vector3(1,2,0));)ns",
            R"ns(var a=[1,2];var alias=a;a.reserve(100);a.reserve(0);a.clear();a.append(3);return alias==a && alias.len()==1 && alias[0]==3;)ns",
            R"ns(var a=[];a.append(a);a.clear();a.append(4);return a.len()==1 && a[0]==4;)ns",
            R"ns(var a=[1,[2],math.Vector2(3,4)];var b=a.clone();b[0]=9;b[1][0]=8;b[2][0]=7;return a!=b && a[0]==1 && a[1][0]==8 && a[2][0]==3;)ns",
            R"ns(var a=[];a.append(a);var b=a.clone();return b!=a && b[0]==a;)ns",
            R"ns(var a=[1];a=a.clone();return a.len()==1 && a[0]==1;)ns",
            R"ns(var a={"x":1,"child":[2],"v":math.Vector2(3,4)};var b=a.clone();b.x=9;b.child[0]=8;b.v[0]=7;return a!=b && a.x==1 && a.child[0]==8 && a.v[0]==3;)ns",
            R"ns(var a={};a.self=a;var b=a.clone();a.clear();return a.len()==0 && b.self==a;)ns",
            R"ns(var a={"x":1};var b=a;a.reserve(100);a.clear();a.y=2;return b.len()==1 && b.y==2 && b.x==null;)ns",
            R"ns(var a={};a[a]=a;a.clear();a[1]=2;return a.len()==1 && a[1]==2;)ns",
            R"ns(var a=system.set([]);if(!a.add(1) || a.add(1))return false;return a.contains(1) && !a.contains(1.0) && !a.remove(2) && a.remove(1) && !a.remove(1) && a.len()==0;)ns",
            R"ns(var a=system.set([]);a.add(null);a.add(true);a.add("ab".."c");return a.contains(null) && a.contains(true) && !a.contains(1) && a.remove("abc") && a.len()==2;)ns",
            R"ns(var a=system.set([]);var v=math.Vector2(1,2);a.add(v);v[0]=3;return a.contains(math.Vector2(1,2)) && !a.contains(v) && a.remove(math.Vector2(1,2));)ns",
            R"ns(var child=[];var a=system.set([child]);var b=a.clone();b.add(2);child.append(1);return a!=b && a.len()==1 && b.contains(child) && !b.contains([1]);)ns",
            R"ns(var a=system.set([]);a.add(a);var b=a.clone();a.clear();return a.len()==0 && b.contains(a) && !b.contains(b);)ns",
            R"ns(var a=system.set([]);a.add(a);return a.remove(a) && a.len()==0;)ns",
            R"ns(var a=system.set([1,2]);var b=a;a.clear();a.add(3);return b.len()==1 && b.contains(3);)ns",
            R"ns(var a=system.set([1]);foreach(var v in a){if(a.add(v) || a.remove(9))return false;}return true;)ns",
            R"ns(var a=system.array(3,2);a.fill(7.9);a.reserve(100);a.resize(3);return a[0]==7 && a[1]==7 && a[2]==3 && a.find(7.9)==-1 && a.find(7.0)==0;)ns",
            R"ns(var a=system.array(1.0,2);a.fill(3);return a.find(3)==0 && a.contains(3.0) && !a.contains(true);)ns",
            R"ns(var a=system.array(false,9);a.fill(true);var b=a.clone();a.clear();a.resize(12);return b.len()==9 && b.find(true)==0 && !b.contains(false) && a.find(true)==-1;)ns",
            R"ns(var a=system.array(2,2);a[0]=9;var b=a.clone();b[0]=8;b.resize(3);return a[0]==9 && b[0]==8 && b[2]==2 && a!=b;)ns",
            R"ns(var a=system.array(-0.0,2);a[0]=1.0;var b=a.clone();b.resize(3);return b[0]==1.0 && 1.0 / b[2]<0.0;)ns",
            R"ns(var a=system.array(0.0,2);a.fill(0.0 / 0.0);return a.find(a[0])==-1;)ns",
            R"ns(var a=system.array(16777217,1);return !a.contains(16777216) && !a.contains(16777216.0) && a.contains(16777217.0);)ns",
            R"ns(var a=system.array(1,2);var alias=a;a.clear();a.append(9);return alias.len()==1 && alias[0]==9;)ns",
            R"ns(var a=system.array(1,2);var sum=0;foreach(var v in a){a.reserve(100);a.fill(2);sum+=v;}return sum==3;)ns",
            R"ns(var a=[1,2];var sum=0;foreach(var v in a){a.reserve(100);sum+=v;}return sum==3;)ns",
            R"ns(var a=[];var b={};var c=system.set([]);var d=system.array(0,0);a.clear();b.clear();c.clear();d.clear();d.fill(1);return a.clone()!=a && b.clone()!=b && c.clone()!=c && d.clone()!=d && d.find(0)==-1;)ns"
        }) Run(runtime, std::string("import system;import math;export fun Probe(){") + body + "}", debug, image);
        for (const char* body : {
            R"ns(var a=[1,2,null];var alias=a;var n=a.pop();return n==null && a.pop()==2 && alias.len()==1 && a[0]==1;)ns",
            R"ns(var a=[1,2,3,4];var alias=a;return a.remove_swap(1)==2 && alias.len()==3 && a[0]==1 && a[1]==4 && a[2]==3;)ns",
            R"ns(var a=[1];return a.remove_swap(0)==1 && a.len()==0;)ns",
            R"ns(var a=[1,2];return a.remove_swap(1)==2 && a.len()==1 && a[0]==1;)ns",
            R"ns(var a=[];a.append(a);var b=a.pop();return b==a && a.len()==0;)ns",
            R"ns(var a=[];a.append(a);a.append(3);var b=a.remove_swap(0);return b==a && a.len()==1 && a[0]==3;)ns",
            R"ns(var a=[];a.append(3);a.append(a);return a.remove_swap(0)==3 && a[0]==a;)ns",
            R"ns(var x=[1];var a=[x,x];var b=a.remove_swap(0);b[0]=9;return a.len()==1 && a[0][0]==9 && b==x;)ns",
            R"ns(var v=math.Vector2(1,2);var a=[v,v];var b=a.remove_swap(0);b[0]=9;return a[0][0]==1 && v[0]==1;)ns",
            R"ns(var a=[1];a=a.pop();var b=[[2],3];b=b.remove_swap(0);return a==1 && b[0]==2;)ns",
            R"ns(var m={};m[1]=false;return m.contains(1) && !m.contains(1.0) && !m.remove(1.0) && m.remove(1) && !m.remove(1);)ns",
            R"ns(var m={};m[null]=3;m[true]=4;return m.contains(null) && m.contains(true) && m.remove(null) && !m.contains(null) && m.len()==1;)ns",
            R"ns(var m={"a":1};var alias=m;return m.remove("a") && alias.len()==0 && !m.contains("a");)ns",
            R"ns(var m={};m[m]=m;return m.contains(m) && m.remove(m) && m.len()==0;)ns",
            R"ns(var key=[];var m={};m[key]=1;return m.contains(key) && !m.contains([]) && m.remove(key);)ns",
            R"ns(var m={};m[math.Vector2(1,2)]=3;return m.contains(math.Vector2(1,2)) && m.remove(math.Vector2(1,2)) && m.len()==0;)ns",
            R"ns(var m={"ab":1};return m.contains("a".."b") && m.remove("a".."b");)ns",
            R"ns(var m={"a":1};foreach(var k in m){if(m.remove("missing"))return false;}return m.len()==1;)ns",
            R"ns(var s=system.set([]);var a=s.values();return type(a)=="list" && a.len()==0;)ns",
            R"ns(var s=system.set([1,2,2,null]);var a=s.values();a.clear();return s.len()==3 && s.values().contains(null);)ns",
            R"ns(var child=[1];var s=system.set([child]);var a=s.values();a[0][0]=7;return child[0]==7 && s.contains(child);)ns",
            R"ns(var s=system.set([math.Vector2(1,2)]);var a=s.values();a[0][0]=7;return s.contains(math.Vector2(1,2)) && !s.contains(a[0]);)ns",
            R"ns(var s=system.set([]);s.add(s);var a=s.values();s.clear();return a[0]==s && a[0].len()==0;)ns",
            R"ns(var s=system.set([1,2]);var count=0;foreach(var v in s){count+=s.values().len();}return count==4;)ns",
            R"ns(var s=system.set([1]);s=s.values();return s.len()==1 && s[0]==1;)ns"
        }) Run(runtime, std::string("import system;import math;export fun Probe(){")+body+"}",debug,image);
        for (int size = 1; size <= 12; ++size) for (int index = 0; index < size; ++index)
            Run(runtime, std::string("export fun Probe(){var a=[];for(var i in 0,")+std::to_string(size)+
                "){a.append(i);}var k="+std::to_string(index)+";var n=a.len();var r=a.remove_swap(k);"
                "if(r!=k || a.len()!=n-1)return false;for(var j in 0,a.len()){"
                "var expected=j;if(j==k)expected=n-1;if(a[j]!=expected)return false;}return true;}",debug,image);
        Run(runtime, "export fun Probe(){return [].pop();}", debug,image,"cannot pop an empty list");
        for (const char* index : {"-1","2","2147483647","-2147483648"})
            Run(runtime,std::string("export fun Probe(){var a=[1,2];return a.remove_swap(")+index+");}",debug,image,"index out of range");
        for (const char* body : {
            "var a=[1,2];foreach(var v in a){a.pop();}",
            "var a=[1,2];foreach(var v in a){a.remove_swap(0);}",
            "var m={1:2};foreach(var k in m){m.remove(k);}"
        }) Run(runtime,std::string("export fun Probe(){")+body+"return true;}",debug,image,"collection was modified during foreach");
        for (const char* expr : {"[1].pop(0)", "[1].remove_swap()", "[1].remove_swap(0.0)", "[1].remove_swap(true)",
            "[1].remove_swap(0,1)", "{}.contains()", "{}.contains(1,2)", "{}.remove()", "{}.remove(1,2)",
            "system.set([]).values(1)"})
            Run(runtime,std::string("import system;export fun Probe(){return ")+expr+";}",debug,image,"");
        Run(runtime, "import system;export fun Probe(){var m={};var s=system.set([]);"
            "for(var round in 0,8){for(var i in 0,257){m[i]=i;s.add(i);}var mc=m.clone();var sc=s.clone();"
            "m.clear();s.clear();if(mc.len()!=257 || sc.len()!=257)return false;"
            "for(var j in 0,257){if(mc[j]!=j || !sc.contains(j))return false;}}return m.len()==0 && s.len()==0;}",debug,image);
        for (const char* size : {"0","1","7","8","9","17"})
            Run(runtime, std::string("import system;export fun Probe(){var a=system.array(true,")+size+
                ");a.reserve(100);a.fill(false);var b=a.clone();b.resize(101);"
                "for(var i in 0,a.len()){if(a[i] || b[i])return false;}"
                "return b[100] && !a.contains(true);}",debug,image);
        for (const char* vector : {"math.Vector2(1,2)","math.Vector3(1,2,3)","math.Vector4(1,2,3,4)"}) {
            Run(runtime, std::string("import math;import system;export fun Probe(){var initial=") + vector +
                ";var a=system.array(initial,2);var v=initial;v[0]=5;a.fill(v);var b=a.clone();"
                "b[0][0]=9;a.reserve(100);a.resize(3);return a[0][0]==5 && a[1][0]==5 && a[2][0]==1 && "
                "b[0][0]==9 && b[1][0]==5 && a.find(v)==0 && !a.contains(5) && initial[0]==1;}", debug, image);
        }
        for (const char* init : {"[1,2]", "{\"x\":1}", "system.set([1,2])", "system.array(1,2)"})
            Run(runtime, std::string("import system;export fun Probe(){var a=")+init+
                ";foreach(var v in a){a.clear();}return true;}",debug,image,"collection was modified during foreach");
        for (const char* body : {
            "var a=[];a.reserve(-1);", "var a=system.array(0,0);a.reserve(-1);"
        }) Run(runtime, std::string("import system;export fun Probe(){")+body+"return true;}", debug,image,"reserve capacity");
        for (const char* expr : {"[].find()", "[].contains(1,2)", "[].reserve(1.0)", "[].clear(1)", "[].clone(1)",
            "{}.clear(1)", "{}.clone(1)", "system.set([]).add()", "system.set([]).remove()", "system.set([]).contains()",
            "system.array(0,0).fill(true)", "system.array(false,2).fill(1)",
            "system.array(math.Vector2(1,2),2).fill(math.Vector3(1,2,3))"})
            Run(runtime, std::string("import system;import math;export fun Probe(){return ")+expr+";}",debug,image,"");
    }
    std::printf("collection API: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
