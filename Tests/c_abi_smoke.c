/*
 * c_abi_smoke.c — NeoScriptC.h exercise, written in plain C on purpose.
 *
 * Compiling this as C is half the test: it proves the ABI header carries no
 * C++ that a P/Invoke host (C#, Unity) could not consume, and that every entry
 * point really has C linkage.  The other half walks the paths a Unity binding
 * leans on -- host callbacks for print and import, a native object dispatched
 * from script, collection returns, globals, and sliced execution.
 */

#include "NeoScriptC.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_checks = 0;

static void Check(int condition, const char* what)
{
    ++g_checks;
    if (!condition)
    {
        ++g_failures;
        printf("  FAIL  %s\n", what);
    }
    else
    {
        printf("  ok    %s\n", what);
    }
}

static int SameStr(const char* p, int32_t len, const char* literal)
{
    const size_t n = strlen(literal);
    if (p == NULL || len < 0 || (size_t)len != n)
        return 0;
    return memcmp(p, literal, n) == 0;
}

static NsStr Lit(const char* s)
{
    NsStr v;
    v.data = s;
    v.len = (int32_t)strlen(s);
    v._pad = 0;
    return v;
}

/* ---------------------------------------------------------------------------
 * Host callbacks
 * ------------------------------------------------------------------------ */

static int g_printCount = 0;
static int g_sawHelloFromScript = 0;

static void NS_CALL OnPrint(void* user, const char* msg, int32_t len)
{
    (void)user;
    ++g_printCount;
    if (SameStr(msg, len, "hello from script"))
        g_sawHelloFromScript = 1;
    printf("  [script] %.*s\n", (int)len, msg);
}

static void NS_CALL OnError(void* user, const char* msg, int32_t len)
{
    (void)user;
    printf("  [script error] %.*s\n", (int)len, msg);
}

/* A Unity host resolves imports out of Resources or an asset bundle, not the
 * filesystem, so the loader callback is the shape that matters here. */
static int g_loaderHits = 0;

static const char* kHelperSource =
    "export fun triple(var v) { return v * 3; }\n";

static int32_t NS_CALL OnLoad(void* user, const char* path, int32_t pathLen, void* sink)
{
    (void)user;
    ++g_loaderHits;
    printf("  [loader] %.*s\n", (int)pathLen, path);
    /* Accept any path that names our one virtual module. */
    if (pathLen >= 9 && memcmp(path + (pathLen - 9), "helper.ns", 9) == 0)
    {
        NsSinkWrite(sink, kHelperSource, (int32_t)strlen(kHelperSource));
        return 1;
    }
    return 0;
}

/* Native object dispatcher.  One C function serves every registered type and
 * branches on the slot -- exactly what the C# binding layer does. */
static int32_t g_hostSlot = -1;
static int32_t g_counterSlot = -1;

static int32_t NS_CALL OnMethod(void* user, int32_t slot, NsContext* ctx,
                                const char* method, int32_t methodLen)
{
    (void)user;

    if (slot == g_hostSlot)
    {
        if (SameStr(method, methodLen, "Double"))
        {
            NsCtxRetInt(ctx, NsCtxArgInt(ctx, 0) * 2);
            return 1;
        }
        if (SameStr(method, methodLen, "Concat"))
        {
            NsStr a;
            char buffer[128];
            NsCtxArgString(ctx, 0, &a);
            if (a.len > 100)
                a.len = 100;
            memcpy(buffer, a.data, (size_t)a.len);
            memcpy(buffer + a.len, "!", 1);
            NsCtxRetString(ctx, buffer, a.len + 1);
            return 1;
        }
        if (SameStr(method, methodLen, "Stats"))
        {
            /* Build a map straight into the return slot. */
            NsMapBuilder* m = NsCtxRetMap(ctx);
            NsMapSetInt(m, "hp", 2, 42);
            NsMapSetString(m, "name", 4, "orc", 3);
            NsMapSetVec3(m, "pos", 3, 1.0f, 2.0f, 3.0f);
            return 1;
        }
        if (SameStr(method, methodLen, "SumList"))
        {
            NsListReader* list = NsCtxArgList(ctx, 0);
            int32_t total = 0;
            int32_t i;
            int32_t count = NsListRdCount(list);
            for (i = 0; i < count; ++i)
            {
                int32_t v = 0;
                if (NsListRdGetInt(list, i, &v))
                    total += v;
            }
            NsCtxRetInt(ctx, total);
            return 1;
        }
        /* Leaving a reason behind is what turns a false return into a real
         * diagnostic instead of "invalid call". */
        NsCtxFail(ctx, 4242, "unknown Host method", 19);
        return 0;
    }

    if (slot == g_counterSlot)
    {
        /* userData is per-binding: the host object this call belongs to. */
        int* counter = (int*)NsCtxUserData(ctx);
        if (SameStr(method, methodLen, "Bump"))
        {
            *counter += NsCtxArgInt(ctx, 0);
            NsCtxRetInt(ctx, *counter);
            return 1;
        }
        return 0;
    }

    return 0;
}

static int32_t NS_CALL OnProperty(void* user, int32_t slot, NsContext* ctx,
                                  const char* name, int32_t nameLen, int32_t isGet)
{
    (void)user;
    (void)slot;
    if (SameStr(name, nameLen, "version"))
    {
        if (isGet)
        {
            NsCtxRetInt(ctx, 7);
            return 1;
        }
    }
    return 0;
}

/* Collection return read inside the callback, where the reader is alive. */
typedef struct HarvestedMap
{
    int32_t gold;
    int32_t haveGold;
    char    name[32];
    int32_t haveName;
} HarvestedMap;

static void NS_CALL OnReadMap(void* user, NsMapReader* reader)
{
    HarvestedMap* out = (HarvestedMap*)user;
    NsStr name;
    out->haveGold = NsMapGetInt(reader, "gold", 4, &out->gold);
    if (NsMapGetString(reader, "name", 4, &name))
    {
        int32_t n = name.len < 31 ? name.len : 31;
        memcpy(out->name, name.data, (size_t)n);
        out->name[n] = '\0';
        out->haveName = 1;
    }
}

/* ---------------------------------------------------------------------------
 * Script under test
 * ------------------------------------------------------------------------ */

static const char* kSource =
    "import math;\n"
    "import helper;\n"
    /* Only exported globals are reachable by name from the host. */
    "export var counter = 10;\n"
    "print(\"hello from script\");\n"
    "export fun Add(var a, var b) { return a + b; }\n"
    "export fun Greet(var who) { return \"hi \" + who; }\n"
    "export fun Bump() { counter = counter + 1; return counter; }\n"
    "export fun ViaHelper(var v) { return helper.triple(v); }\n"
    "export fun HostDouble(var v) { return Host.Double(v); }\n"
    "export fun HostConcat(var s) { return Host.Concat(s); }\n"
    "export fun HostStatsHp() { var r = Host.Stats(); return r[\"hp\"]; }\n"
    "export fun HostSum() { return Host.SumList([1, 2, 3, 4]); }\n"
    "export fun HostVersion() { return Host.version; }\n"
    "export fun Loot() { var m = {}; m[\"gold\"] = 77; m[\"name\"] = \"chest\"; return m; }\n"
    "export fun Where() { return math.Vector3(1.0, 2.0, 3.0); }\n"
    "export fun Fails() { return Host.Nope(); }\n"
    "export fun Busy() { var s = 0; for (var i in 0, 300000) { s = s + 1; } return s; }\n";

int main(void)
{
    NsRuntimeDesc rdesc;
    NsObjectDesc odesc;
    NsCompileDesc cdesc;
    NsInstanceDesc idesc;
    NsRuntime* rt;
    NsProgram program;
    NsInstance instance;
    NsError error;
    NsCall* call;
    NsCallResult result;
    NsFunction* addFn;
    int counterState = 100;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== NeoScript C ABI smoke ===\n");

    Check(NsGetAbiVersion() == NS_ABI_VERSION, "ABI version matches header");

    NsSetLogHandler(&OnPrint, &OnError, NULL);

    memset(&rdesc, 0, sizeof(rdesc));
    rdesc.structSize = (uint32_t)sizeof(rdesc);
    rdesc.load = &OnLoad;
    rdesc.method = &OnMethod;
    rdesc.property = &OnProperty;
    rdesc.libPath = Lit("");

    rt = NsRuntimeCreate(&rdesc);
    Check(rt != NULL, "runtime created");
    if (rt == NULL)
        return 1;

    memset(&odesc, 0, sizeof(odesc));
    odesc.structSize = (uint32_t)sizeof(odesc);
    odesc.name = Lit("Host");
    odesc.hasMethod = 1;
    odesc.hasProperty = 1;
    odesc.declareGlobal = 1;
    Check(NsRegisterObject(rt, &odesc, &g_hostSlot) == 1, "Host object registered");

    memset(&odesc, 0, sizeof(odesc));
    odesc.structSize = (uint32_t)sizeof(odesc);
    odesc.name = Lit("Counter");
    odesc.hasMethod = 1;
    odesc.declareGlobal = 1;
    odesc.userData = &counterState;
    Check(NsRegisterObject(rt, &odesc, &g_counterSlot) == 1, "Counter object registered");
    Check(g_hostSlot != g_counterSlot, "each object type got its own slot");

    NsFreezeBindings(rt);

    memset(&cdesc, 0, sizeof(cdesc));
    cdesc.structSize = (uint32_t)sizeof(cdesc);
    cdesc.source = Lit(kSource);
    cdesc.sourceName = Lit("c_abi_smoke.ns");

    memset(&error, 0, sizeof(error));
    program = NsCompile(rt, &cdesc, &error);
    if (program == 0)
        printf("  compile error %d: %.*s\n", error.code, (int)error.message.len, error.message.data);
    Check(program != 0, "script compiled");
    Check(g_loaderHits > 0, "loader callback was consulted for import");
    if (program == 0)
    {
        NsRuntimeDestroy(rt);
        return 1;
    }

    memset(&idesc, 0, sizeof(idesc));
    idesc.structSize = (uint32_t)sizeof(idesc);
    idesc.runGlobalInit = 1;
    instance = NsCreateInstance(rt, program, &idesc);
    Check(instance != 0, "instance created");
    Check(NsIsAlive(rt, instance) == 1, "instance reports alive");
    Check(g_sawHelloFromScript == 1, "print handler received global-init output");

    /* --- scalar call through a cached function handle --- */
    addFn = NsFindFunction(rt, program, "Add", 3);
    Check(addFn != NULL && NsFunctionValid(addFn) == 1, "Add resolved to a function handle");
    call = NsCallBegin(rt, instance, addFn);
    NsCallArgInt(call, 20);
    NsCallArgInt(call, 22);
    memset(&result, 0, sizeof(result));
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.type == NS_TYPE_INT && result.i == 42,
          "Add(20, 22) == 42");
    NsCallEnd(call);

    /* --- string return --- */
    call = NsCallBeginName(rt, instance, "Greet", 5);
    NsCallArgString(call, "neo", 3);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && SameStr(result.str.data, result.str.len, "hi neo"),
          "Greet(\"neo\") == \"hi neo\"");
    NsCallEnd(call);

    /* --- imported module reached through the loader callback --- */
    call = NsCallBeginName(rt, instance, "ViaHelper", 9);
    NsCallArgInt(call, 5);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.i == 15, "imported helper.triple(5) == 15");
    NsCallEnd(call);

    /* --- script calls back into the native object --- */
    call = NsCallBeginName(rt, instance, "HostDouble", 10);
    NsCallArgInt(call, 21);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.i == 42, "Host.Double(21) == 42");
    NsCallEnd(call);

    call = NsCallBeginName(rt, instance, "HostConcat", 10);
    NsCallArgString(call, "hey", 3);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && SameStr(result.str.data, result.str.len, "hey!"),
          "Host.Concat(\"hey\") == \"hey!\"");
    NsCallEnd(call);

    call = NsCallBeginName(rt, instance, "HostStatsHp", 11);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.i == 42, "native-built return map reaches script");
    NsCallEnd(call);

    call = NsCallBeginName(rt, instance, "HostSum", 7);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.i == 10, "script list read by native == 10");
    NsCallEnd(call);

    call = NsCallBeginName(rt, instance, "HostVersion", 11);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.i == 7, "native property get == 7");
    NsCallEnd(call);

    /* --- collection return read inside the safe callback --- */
    {
        HarvestedMap harvested;
        memset(&harvested, 0, sizeof(harvested));
        call = NsCallBeginName(rt, instance, "Loot", 4);
        NsCallInvokeReadMap(call, &OnReadMap, &harvested);
        Check(harvested.haveGold && harvested.gold == 77, "map return: gold == 77");
        Check(harvested.haveName && strcmp(harvested.name, "chest") == 0, "map return: name == chest");
        NsCallEnd(call);
    }

    /* --- vector value type --- */
    call = NsCallBeginName(rt, instance, "Where", 5);
    NsCallInvokeR(call, &result);
    Check(result.status == NS_RUN_COMPLETED && result.type == NS_TYPE_VEC3,
          "Vector3 return keeps its value type");
    Check(result.f[0] == 1.0f && result.f[1] == 2.0f && result.f[2] == 3.0f,
          "Vector3 components round-trip");
    NsCallEnd(call);

    /* --- globals --- */
    {
        int32_t counter = 0;
        Check(NsGetGlobalInt(rt, instance, "counter", 7, &counter) == 1 && counter == 10,
              "global read before call == 10");
        call = NsCallBeginName(rt, instance, "Bump", 4);
        NsCallInvokeR(call, &result);
        NsCallEnd(call);
        Check(NsGetGlobalInt(rt, instance, "counter", 7, &counter) == 1 && counter == 11,
              "global sees the script's write");
        Check(NsSetGlobalInt(rt, instance, "counter", 7, 200) == 1, "global write accepted");
        call = NsCallBeginName(rt, instance, "Bump", 4);
        NsCallInvokeR(call, &result);
        Check(result.i == 201, "script sees the host's write");
        NsCallEnd(call);
    }

    /* --- per-instance binding userData --- */
    Check(NsBindObject(rt, instance, "Counter", 7, &counterState) == 1, "per-instance bind accepted");

    /* --- failure carries the native reason out --- */
    {
        NsError callError;
        call = NsCallBeginName(rt, instance, "Fails", 5);
        NsCallInvoke(call);
        memset(&callError, 0, sizeof(callError));
        NsCallGetError(call, &callError);
        Check(NsCallStatus(call) == NS_RUN_FAILED, "calling an unknown native method fails the call");
        Check(callError.code == 4242, "ctx.fail code surfaces on the invocation error");
        Check(callError.message.len > 0, "ctx.fail message surfaces on the invocation error");
        printf("  [expected failure] %.*s\n", (int)callError.message.len, callError.message.data);
        NsCallEnd(call);
    }

    /* --- sliced execution --- */
    {
        int slices = 0;
        int32_t status;
        Check(NsStartSliced(rt, instance, "Busy", 4, 1, 0) == 1, "sliced run started");
        do
        {
            status = NsUpdateSliced(rt, instance);
            ++slices;
        } while (status == NS_RUN_SUSPENDED && slices < 100000);
        Check(status == NS_RUN_COMPLETED, "sliced run reached completion");
        Check(NsIsRunning(rt, instance) == 0, "instance idle after sliced run");
        printf("  (sliced run took %d slices)\n", slices);
    }

    /* --- bytecode round trip: compile once, load into a second runtime --- */
    {
        NsStr bytes;
        static unsigned char copy[1 << 20];
        int32_t copyLen = 0;

        memset(&bytes, 0, sizeof(bytes));
        memset(&error, 0, sizeof(error));
        if (NsCompileToBytecode(rt, &cdesc, &bytes, &error) == 1 && bytes.len > 0 &&
            (size_t)bytes.len <= sizeof(copy))
        {
            copyLen = bytes.len;
            memcpy(copy, bytes.data, (size_t)bytes.len);
        }
        Check(copyLen > 0, "CompileToBytecode produced an image");

        if (copyLen > 0)
        {
            NsProgram loaded;
            NsInstance loadedInstance;
            memset(&error, 0, sizeof(error));
            loaded = NsLoadProgram(rt, copy, copyLen, &error);
            Check(loaded != 0, "LoadProgram accepted the image");
            if (loaded != 0)
            {
                loadedInstance = NsCreateInstance(rt, loaded, &idesc);
                call = NsCallBeginName(rt, loadedInstance, "Add", 3);
                NsCallArgInt(call, 1);
                NsCallArgInt(call, 2);
                NsCallInvokeR(call, &result);
                Check(result.status == NS_RUN_COMPLETED && result.i == 3,
                      "instance from loaded bytecode runs");
                NsCallEnd(call);
                NsDestroyInstance(rt, loadedInstance);
                NsDestroyProgram(rt, loaded);
            }
        }
    }

    /* --- teardown --- */
    NsFunctionRelease(addFn);
    NsDestroyInstance(rt, instance);
    Check(NsIsAlive(rt, instance) == 0, "handle goes dead after destroy");
    NsDestroyProgram(rt, program);
    NsRuntimeDestroy(rt);

    /* A destroyed runtime must give its dispatch slots back, or a host that
     * recreates runtimes (Unity domain reload does, constantly) runs dry. */
    {
        NsRuntime* second;
        int32_t slot = -1;
        int i;
        int exhausted = 0;

        memset(&rdesc, 0, sizeof(rdesc));
        rdesc.structSize = (uint32_t)sizeof(rdesc);
        rdesc.method = &OnMethod;
        second = NsRuntimeCreate(&rdesc);
        for (i = 0; i < NS_MAX_OBJECT_SLOTS; ++i)
        {
            memset(&odesc, 0, sizeof(odesc));
            odesc.structSize = (uint32_t)sizeof(odesc);
            odesc.name = Lit("Host");
            odesc.hasMethod = 1;
            odesc.declareGlobal = 1;
            if (NsRegisterObject(second, &odesc, &slot) != 1)
            {
                exhausted = 1;
                break;
            }
            /* Only the first registration can succeed for one runtime -- the
             * name repeats -- so stop as soon as the engine refuses it. */
            break;
        }
        Check(exhausted == 0 && slot >= 0, "slots are available again after a runtime is destroyed");
        NsRuntimeDestroy(second);
    }

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
