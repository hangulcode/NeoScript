#include "NeoScript.h"

#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace NeoScript;

static int checks = 0;
static int failures = 0;
static InstanceHandle peer;

static void Check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label.c_str()); }
}

static void* Handle(int id) { return reinterpret_cast<void*>(static_cast<intptr_t>(id)); }

static bool EntityMethod(CallContext& ctx, StringView name)
{
    if (name.str() != "id") return false;
    ctx.retInt(static_cast<int32_t>(reinterpret_cast<intptr_t>(ctx.userData())));
    return true;
}

static bool HostMethod(CallContext& ctx, StringView name)
{
    const std::string method = name.str();
    if (method == "specialVector")
    {
        const int kind = ctx.argInt(0);
        const float x = kind == 0 ? -0.0f : kind == 1
            ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
        ctx.retVec3(x, 2.0f, 3.0f);
        return true;
    }
    ObjectType entity = ctx.runtime()->GetObjectType("Entity");
    if (method == "get" || method == "other")
    {
        ctx.retObject(method == "get" ? entity : ctx.runtime()->GetObjectType("Other"), Handle(ctx.argInt(0)));
        return true;
    }
    if (method == "bundle")
    {
        MapBuilder map = ctx.retMap();
        map.setObject("node", entity, Handle(1));
        ListBuilder list = map.setList("nodes");
        list.pushObject(entity, Handle(1));
        return true;
    }
    if (method == "peer")
    {
        ctx.retInstanceGlobal(peer, "Entity");
        return true;
    }
    return false;
}

static const char* source = R"NS(
import system;
import math;
import coroutine;
var saved;
var savedKeys = {};
fun First() { return 1; }
fun Second() { return 1; }
fun Make(var n) { return fun() { return n; }; }
fun Same(var a, var alias, var other)
{
    var eq = a == alias;
    var ne = a != alias;
    if (!eq || ne || a != a || !(a == a)) return false;
    if (alias != a || other == a || a == other || !(a != other)) return false;
    if (a == null || null == a || !(a != null)) return false;
    var keys = {};
    keys[a] = 7;
    if (keys[alias] != 7) return false;
    keys[alias] = 9;
    if (keys.len() != 1 || keys[a] != 9) return false;
    keys[other] = 3;
    keys.reserve(256);
    if (keys.len() != 2 || keys[a] != 9 || keys[other] != 3) return false;
    var copies = [a, alias, a];
    foreach (var key in copies) { if (keys[key] != 9) return false; }
    keys[alias] = null;
    if (keys[a] != null || keys[other] != 3 || keys.len() != 1) return false;
    var unique = system.set([a, alias, other, a, other]);
    return unique.len() == 2;
}
export fun List() { var a=[1,2]; return Same(a,a,[1,2]); }
export fun Map() { var a={"x":1}; return Same(a,a,{"x":1}); }
export fun Set() { var a=system.set([1]); return Same(a,a,system.set([1])); }
export fun ArrayInt() { var a=system.array(1,3); return Same(a,a,system.array(1,3)); }
export fun ArrayFloat() { var a=system.array(1.0,3); return Same(a,a,system.array(1.0,3)); }
export fun ArrayBool() { var a=system.array(false,3); return Same(a,a,system.array(false,3)); }
export fun Function() { return Same(First,First,Second); }
export fun Module() { var a=system.load("var n=1;","a"); return Same(a,a,system.load("var n=1;","b")); }
export fun Closure() { var a=Make(1); return Same(a,a,Make(1)); }
export fun Coroutine() { var a=coroutine.create(First); return Same(a,a,coroutine.create(First)); }
export fun Async() { var a=system.aysnc_create(); return Same(a,a,system.aysnc_create()); }
export fun MutatedKeys()
{
    var list=[1]; var map={}; var array=system.array(1,1);
    var keys={}; keys[list]=1; keys[map]=2; keys[array]=3;
    list.append(2); map.x=5; array.resize(100); array[0]=4;
    var alias=list;
    return keys[alias]==1 && keys[map]==2 && keys[array]==3;
}
export fun Cycles()
{
    var a=[]; a.append(a); var b=[]; b.append(b);
    var x={}; x.self=x; var y={}; y.self=y;
    return Same(a,a,b) && Same(x,x,y);
}
export fun Scalars()
{
    var i=1; var f=1.0; var b=true; var n=null; var s="ab";
    var v=math.Vector3(1.0,2.0,3.0); var w=math.Vector3(1.0,2.0,3.0);
    var keys={}; keys[i]=1; keys[f]=2;
    return i==f && f==i && b==true && n==null && s==("a".."b") && v==w
        && keys.len()==2 && keys[i]==1 && keys[f]==2;
}
export fun MixedTypes() { var a=[]; var b={}; var c=system.array(0,0); var f=First; return a!=b && b!=a && a!=c && b!=c && a!=f; }
export fun VectorKeys()
{
    return Same(math.Vector2(1,2), math.Vector2(1,2), math.Vector2(2,1))
        && Same(math.Vector3(1,2,3), math.Vector3(1,2,3), math.Vector3(1,2,4))
        && Same(math.Vector4(1,2,3,4), math.Vector4(1,2,3,4), math.Vector4(1,2,4,3))
        && Same(math.Quaternion(1,0,0,0), math.Vector4(1,0,0,0), math.Vector4(0,1,0,0));
}
export fun VectorDimensions()
{
    var a=math.Vector2(1,2); var b=math.Vector3(1,2,0); var c=math.Vector4(1,2,0,0);
    var k={}; k[a]=2; k[b]=3; k[c]=4;
    return a!=b && b!=a && a!=c && c!=a && b!=c && c!=b && a!=null && null!=a
        && k.len()==3 && k[math.Vector2(1,2)]==2 && k[math.Vector3(1,2,0)]==3
        && k[math.Vector4(1,2,0,0)]==4 && system.set([a,b,c]).len()==3;
}
export fun VectorKeyCopies()
{
    var v=math.Vector3(1,2,3); var alias=v; var k={}; k[v]=7;
    var s=system.set([v]);
    v[0]=9;
    if (alias[0]!=1 || k[v]!=null || k[math.Vector3(1,2,3)]!=7) return false;
    var keys=k.keys(); var extracted=keys[0]; extracted[1]=8;
    if (k[math.Vector3(1,2,3)]!=7 || k[extracted]!=null || keys[0]!=math.Vector3(1,2,3)) return false;
    foreach (var item in s) { item[2]=10; }
    foreach (var item in s) { if (item!=math.Vector3(1,2,3)) return false; }
    alias[2]=6;
    k[math.Vector3(1,2,3)]=null;
    return k.len()==0;
}
export fun VectorKeyGrowth()
{
    var k={}; var values=[];
    for (var i in 0,300) {
        k[math.Vector3(i,i+1,i+2)]=i;
        values.append(math.Vector3(i,i+1,i+2));
        values.append(math.Vector3(i,i+1,i+2));
    }
    k.reserve(2048);
    if (k.len()!=300 || system.set(values).len()!=300) return false;
    for (var i in 0,300) {
        var key=math.Vector3(i,i+1,i+2);
        if (k[key]!=i) return false;
        k[key]=null;
    }
    return k.len()==0;
}
export fun VectorSpecialValues()
{
    var z=math.Vector3(0.0,2,3); var nz=Host.specialVector(0);
    var inf=Host.specialVector(1); var nan=Host.specialVector(2);
    var k={}; k[z]=7; k[inf]=9;
    if (z!=nz || nz!=z || k[nz]!=7 || k[Host.specialVector(1)]!=9) return false;
    k[nz]=8;
    if (k.len()!=2 || k[z]!=8 || system.set([z,nz,inf,Host.specialVector(1)]).len()!=2) return false;
    k[nz]=null;
    return k[z]==null && k.len()==1 && nan!=nan;
}
export fun Native() { return Same(Host.get(1),Host.get(1),Host.get(2)); }
export fun NativeType() { return Same(Host.get(1),Host.get(1),Host.other(1)); }
export fun NullHandle() { return Same(Host.get(0),Host.get(0),Host.other(0)); }
export fun Global() { return Same(Entity,Host.get(1),Host.get(2)); }
export fun Bundle() { var b=Host.bundle(); return Same(b.node,b.nodes[0],Host.get(2)) && b.node==Host.get(1); }
export fun CrossInstance() { return Same(Host.peer(),Host.get(1),Host.get(2)); }
export fun Argument(var a) { return Same(a,Host.get(1),Host.get(2)); }
export fun Capture() { saved=Entity; savedKeys[saved]=7; return true; }
export fun Rebound()
{
    return saved==Host.get(1) && saved.id()==1 && Entity==Host.get(2) && Entity.id()==2
        && saved!=Entity && savedKeys[Host.get(1)]==7 && savedKeys[Entity]==null;
}
)NS";

static void Run(IRuntime* runtime, bool debug, bool image)
{
    CompileDesc desc;
    desc.source = source;
    desc.sourceName = "reference_equality.ns";
    desc.includeDebugInfo = debug;
    ProgramHandle program;
    Error error;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) program = runtime->LoadProgram(bytes, &error);
    }
    else
    {
        CompileResult result = runtime->Compile(desc);
        program = result.program;
        error = result.error;
    }
    const std::string mode = " debug=" + std::to_string(debug) + " image=" + std::to_string(image);
    Check(static_cast<bool>(program), "compile" + mode + " " + error.message);
    if (!program) return;
    InstanceHandle instance = runtime->CreateInstance(program);
    peer = runtime->CreateInstance(program);
    Check(static_cast<bool>(instance) && static_cast<bool>(peer), "instances" + mode);
    if (instance && peer)
    {
        for (const char* name : {"List", "Map", "Set", "ArrayInt", "ArrayFloat", "ArrayBool", "Function",
            "Module", "Closure", "Coroutine", "Async", "MutatedKeys", "Cycles", "Scalars", "MixedTypes",
            "VectorKeys", "VectorDimensions", "VectorKeyCopies", "VectorKeyGrowth", "VectorSpecialValues",
            "Native", "NativeType", "NullHandle", "Global", "Bundle", "CrossInstance", "Capture"})
        {
            Invocation call = runtime->Call(instance, name);
            const RunStatus status = call.invoke();
            Check(status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(), name + mode);
        }
        {
            Invocation call = runtime->Call(instance, "Argument");
            call.argObject(runtime->GetObjectType("Entity"), Handle(1));
            Check(call.invoke() == RunStatus::Completed && call.retBool(), "argObject" + mode);
        }
        runtime->BindObject(instance, "Entity", Handle(2));
        {
            Invocation call = runtime->Call(instance, "Rebound");
            Check(call.invoke() == RunStatus::Completed && call.retBool(), "rebind preserves keys and aliases" + mode);
        }
    }
    if (peer) runtime->DestroyInstance(peer);
    if (instance) runtime->DestroyInstance(instance);
    runtime->DestroyProgram(program);
}

int main()
{
    IRuntime* runtime = CreateRuntime(RuntimeDesc{});
    NativeObjectDesc host;
    host.name = "Host"; host.method = HostMethod;
    runtime->RegisterObject(host);
    NativeObjectDesc entity;
    entity.name = "Entity"; entity.method = EntityMethod; entity.userData = Handle(1);
    runtime->RegisterObject(entity);
    entity.name = "Other"; entity.declareGlobal = false;
    runtime->RegisterObject(entity);
    runtime->FreezeBindings();
    for (bool debug : {false, true}) for (bool image : {false, true}) Run(runtime, debug, image);
    DestroyRuntime(runtime);
    std::printf("Reference equality %s: %d/%d\n", failures ? "FAIL" : "PASS", checks - failures, checks);
    return failures ? 1 : 0;
}
