// NeoNative.cs — NeoScriptC.h 의 P/Invoke 선언. 이 파일은 헤더의 거울이다.
//
// 규칙:
//  · 여기에는 마샬링 편의를 넣지 않는다. 구조체는 전부 blittable, 문자열은 포인터+길이.
//    편의는 상위 래퍼(NeoRuntime/NeoInstance/...)가 제공한다. 경계가 섞이면 레이아웃
//    불일치를 디버깅할 수 없게 된다.
//  · 구조체 필드 순서와 크기는 NeoScriptC.h 와 1:1 이어야 한다. 어긋나면 조용히 메모리가
//    깨진다 — NeoNativeLayout.Verify() 가 기동 시 크기를 대조한다.
//  · 델리게이트를 네이티브에 넘길 때는 반드시 static 필드로 붙잡아 둔다. 안 그러면 GC 가
//    썽크를 회수해서 네이티브가 죽은 주소를 부른다.

using System;
using System.Runtime.InteropServices;

namespace NeoScript
{
    /// <summary>실행 결과. NeoScriptC.h 의 NS_RUN_* 과 값이 같다.</summary>
    public enum NeoRunStatus
    {
        Completed = 0,
        Suspended = 1,
        Yielded   = 2,
        Failed    = 3,
        Cancelled = 4,
    }

    public enum NeoInstanceState
    {
        Idle      = 0,
        Running   = 1,
        Suspended = 2,
        Failed    = 3,
    }

    public enum NeoValueType
    {
        None     = 0,
        Bool     = 1,
        Int      = 2,
        Float    = 3,
        String   = 4,
        Vec2     = 5,
        Vec3     = 6,
        Vec4     = 7,
        Map      = 8,
        List     = 9,
        Array    = 10,
        Set      = 11,
        Function = 12,
    }

    public enum NeoArrayElementType
    {
        Bool  = 0,
        Int   = 1,
        Float = 2,
    }

    public enum NeoDefineKind
    {
        Identifier = 0,
        Int        = 1,
        Float      = 2,
        String     = 3,
        Bool       = 4,
        Null       = 5,
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsStr
    {
        public IntPtr data;
        public int    len;
        public int    pad;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsError
    {
        public int   code;
        public uint  line;
        public uint  column;
        public uint  pad;
        public NsStr message;
        public NsStr sourceName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsCallResult
    {
        public int    status;
        public int    type;
        public int    i;
        public int    pad;
        public float  f0;
        public float  f1;
        public float  f2;
        public float  f3;
        public NsStr  str;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsAllocStats
    {
        public int  strings;
        public int  maps;
        public int  lists;
        public int  sets;
        public int  coroutines;
        public int  modules;
        public int  asyncs;
        public int  vectors;
        public int  arrays;
        public int  closures;
        public long bufferBytes;
        public long poolBytes;
        public long stringIdleBytes;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsRuntimeDesc
    {
        public uint   structSize;
        public uint   pad;
        public IntPtr user;
        public IntPtr load;
        public IntPtr onInstanceBind;
        public IntPtr method;
        public IntPtr property;
        public NsStr  libPath;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsObjectDesc
    {
        public uint   structSize;
        public int    hasMethod;
        public int    hasProperty;
        public int    declareGlobal;
        public NsStr  name;
        public IntPtr userData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsDefine
    {
        public NsStr name;
        public NsStr text;
        public int   kind;
        public int   pad;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsCompileDesc
    {
        public uint   structSize;
        public int    includeDebugInfo;
        public int    emitAsm;
        public int    defineCount;
        public NsStr  source;
        public NsStr  sourceName;
        public NsStr  debugSourcePath;
        public IntPtr defines;
        public ulong  defineSet;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NsInstanceDesc
    {
        public uint   structSize;
        public int    runGlobalInit;
        public IntPtr userData;
    }

    // --- 네이티브가 관리 코드를 부르는 콜백 ---
    // IL2CPP 에서는 이 시그니처에 붙는 관리 메서드가 static 이고 [MonoPInvokeCallback] 이어야 한다.
    // 어떤 경우에도 예외를 경계 밖으로 흘리면 안 된다.

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void NsLogFn(IntPtr user, IntPtr msg, int len);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int NsLoadFn(IntPtr user, IntPtr path, int pathLen, IntPtr sink);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void NsBindFn(IntPtr user, IntPtr rt, ulong inst, IntPtr instanceUserData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int NsMethodFn(IntPtr user, int slot, IntPtr ctx, IntPtr method, int methodLen);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int NsPropertyFn(IntPtr user, int slot, IntPtr ctx, IntPtr name, int nameLen, int isGet);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void NsReadMapFn(IntPtr user, IntPtr reader);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void NsReadListFn(IntPtr user, IntPtr reader);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void NsReadArrayFn(IntPtr user, IntPtr view);

    /// <summary>NeoScriptC.h 의 평면 C 진입점. 상위 래퍼를 통해 쓰는 것을 권장한다.</summary>
    public static unsafe class NeoNative
    {
        // iOS 는 동적 로딩이 막혀 있어 정적 링크 + "__Internal" 로 해소한다.
        // 그 외에는 NeoScript.dll / libNeoScript.so 를 찾는다.
#if UNITY_IOS && !UNITY_EDITOR
        public const string Lib = "__Internal";
#else
        public const string Lib = "NeoScript";
#endif

        public const int AbiVersion = 1;
        public const int MaxObjectSlots = 64;

        private const CallingConvention Cdecl = CallingConvention.Cdecl;

        // --- 프로세스 전역 ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern uint NsGetAbiVersion();
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsSetLogHandler(IntPtr print, IntPtr error, IntPtr user);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsGetAllocStats(out NsAllocStats stats);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsSinkWrite(IntPtr sink, void* data, int len);

        // --- Runtime ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsRuntimeCreate(ref NsRuntimeDesc desc);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsRuntimeDestroy(IntPtr rt);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsRuntimeSetLibPath(IntPtr rt, byte* path, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsRegisterObject(IntPtr rt, ref NsObjectDesc desc, out int outSlot);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsFreezeBindings(IntPtr rt);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern uint NsGetObjectType(IntPtr rt, byte* name, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsTakeLastError(IntPtr rt, out NsStr outStr);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsPeekLastError(IntPtr rt, out NsStr outStr);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern long NsTrimMemory(IntPtr rt, int force);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCollectCycles(IntPtr rt, int force);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsSetEmptyPageHoldSeconds(IntPtr rt, float sec);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern float NsGetEmptyPageHoldSeconds(IntPtr rt);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsSetTrimPagesPerCall(IntPtr rt, int pages);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsGetTrimPagesPerCall(IntPtr rt);

        // --- Program ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern ulong NsCompile(IntPtr rt, ref NsCompileDesc desc, out NsError err);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCompileToBytecode(IntPtr rt, ref NsCompileDesc desc, out NsStr outBytes, out NsError err);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern ulong NsLoadProgram(IntPtr rt, void* bytes, int len, out NsError err);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsDestroyProgram(IntPtr rt, ulong program);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern ulong NsCreateDefineSet(IntPtr rt, NsDefine* defines, int count);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsDestroyDefineSet(IntPtr rt, ulong set);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsFindFunction(IntPtr rt, ulong program, byte* name, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsFunctionRelease(IntPtr fn);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsFunctionValid(IntPtr fn);

        // --- Instance ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern ulong NsCreateInstance(IntPtr rt, ulong program, ref NsInstanceDesc desc);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsDestroyInstance(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsIsAlive(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsResetInstance(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsGetState(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsBindObject(IntPtr rt, ulong inst, byte* name, int len, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsBindGlobalObject(IntPtr rt, ulong inst, byte* name, int len, uint type, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsBindGlobalMapObject(IntPtr rt, ulong inst, byte* mapName, int mapLen, byte* key, int keyLen, uint type, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsRunGlobalInit(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsResume(IntPtr rt, ulong inst, uint budget, float deltaTime);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCancel(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsStartSliced(IntPtr rt, ulong inst, byte* name, int len, int timeoutMs, uint budget);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsUpdateSliced(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsIsRunning(IntPtr rt, ulong inst);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsGetGlobalInt(IntPtr rt, ulong inst, byte* name, int len, out int value);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsGetGlobalFloat(IntPtr rt, ulong inst, byte* name, int len, out float value);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsGetGlobalString(IntPtr rt, ulong inst, byte* name, int len, out NsStr value);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsSetGlobalInt(IntPtr rt, ulong inst, byte* name, int len, int value);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsSetGlobalFloat(IntPtr rt, ulong inst, byte* name, int len, float value);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsSetGlobalString(IntPtr rt, ulong inst, byte* name, int len, byte* v, int vlen);

        // --- Call ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCallBegin(IntPtr rt, ulong inst, IntPtr fn);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCallBeginName(IntPtr rt, ulong inst, byte* name, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallEnd(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallArgInt(IntPtr call, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallArgFloat(IntPtr call, float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallArgBool(IntPtr call, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallArgString(IntPtr call, byte* v, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallArgVec3(IntPtr call, float x, float y, float z);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallArgObject(IntPtr call, uint type, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallTimeout(IntPtr call, int ms);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallBudget(IntPtr call, uint ops);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallInvoke(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallInvokeR(IntPtr call, out NsCallResult result);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallInvokeReadMap(IntPtr call, IntPtr fn, IntPtr user);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallInvokeReadList(IntPtr call, IntPtr fn, IntPtr user);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallInvokeReadArray(IntPtr call, IntPtr fn, IntPtr user);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallStatus(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallGetError(IntPtr call, out NsError err);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallRetType(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallRetInt(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern float NsCallRetFloat(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCallRetBool(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallRetString(IntPtr call, out NsStr outStr);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCallRetVec(IntPtr call, float* out4);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCallRetMap(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCallRetList(IntPtr call);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCallRetArray(IntPtr call);

        // --- CallContext ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxUserData(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxInstanceUserData(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxRuntime(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern ulong NsCtxInstance(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCtxArgCount(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCtxArgType(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCtxArgInt(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern float NsCtxArgFloat(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsCtxArgBool(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxArgString(IntPtr ctx, int i, out NsStr outStr);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxArgVec(IntPtr ctx, int i, float* out4);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxArgMap(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxArgList(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxArgArray(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxArgObjectUserData(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxArgFunction(IntPtr ctx, int i);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetInt(IntPtr ctx, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetFloat(IntPtr ctx, float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetBool(IntPtr ctx, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetString(IntPtr ctx, byte* v, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetVec2(IntPtr ctx, float x, float y);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetVec3(IntPtr ctx, float x, float y, float z);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetVec4(IntPtr ctx, float x, float y, float z, float w);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetNull(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetObject(IntPtr ctx, uint type, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxRetInstanceGlobal(IntPtr ctx, ulong inst, byte* name, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxRetMap(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsCtxRetList(IntPtr ctx);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsCtxFail(IntPtr ctx, int code, byte* msg, int len);

        // --- 빌더 ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsMapSetInt(IntPtr m, byte* k, int klen, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsMapSetFloat(IntPtr m, byte* k, int klen, float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsMapSetBool(IntPtr m, byte* k, int klen, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsMapSetString(IntPtr m, byte* k, int klen, byte* v, int vlen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsMapSetVec3(IntPtr m, byte* k, int klen, float x, float y, float z);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsMapSetObject(IntPtr m, byte* k, int klen, uint t, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsMapSetMap(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsMapSetList(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListReserve(IntPtr l, int count);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListResize(IntPtr l, int count);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListCount(IntPtr l);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListPushInt(IntPtr l, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListPushFloat(IntPtr l, float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListPushBool(IntPtr l, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListPushString(IntPtr l, byte* v, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListPushVec3(IntPtr l, float x, float y, float z);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListPushObject(IntPtr l, uint t, IntPtr userData);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListSetInt(IntPtr l, int index, int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListSetFloat(IntPtr l, int index, float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern void NsListSetString(IntPtr l, int index, byte* v, int len);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsListPushMap(IntPtr l);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsListPushList(IntPtr l);

        // --- 리더 ---
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapHas(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapType(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapGetInt(IntPtr m, byte* k, int klen, out int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapGetFloat(IntPtr m, byte* k, int klen, out float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapGetBool(IntPtr m, byte* k, int klen, out int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapGetString(IntPtr m, byte* k, int klen, out NsStr v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsMapGetVec(IntPtr m, byte* k, int klen, float* out4);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsMapGetMap(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsMapGetList(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsMapGetArray(IntPtr m, byte* k, int klen);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdCount(IntPtr l);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdType(IntPtr l, int index);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdGetInt(IntPtr l, int index, out int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdGetFloat(IntPtr l, int index, out float v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdGetBool(IntPtr l, int index, out int v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdGetString(IntPtr l, int index, out NsStr v);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsListRdGetVec(IntPtr l, int index, float* out4);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsListRdGetMap(IntPtr l, int index);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsListRdGetList(IntPtr l, int index);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern IntPtr NsListRdGetArray(IntPtr l, int index);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsArrayElementType(IntPtr a);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int NsArrayCount(IntPtr a);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern byte* NsArrayBoolBits(IntPtr a);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern int* NsArrayInts(IntPtr a);
        [DllImport(Lib, CallingConvention = Cdecl)] public static extern float* NsArrayFloats(IntPtr a);
    }

    /// <summary>
    /// 구조체 레이아웃이 네이티브와 어긋나면 조용히 메모리가 깨진다. 기동 시 한 번 대조해서
    /// 어긋났으면 바로 터뜨린다 — 나중에 알아내는 것보다 훨씬 싸다.
    /// </summary>
    public static class NeoNativeLayout
    {
        public static void Verify()
        {
            Expect<NsStr>(16);
            Expect<NsError>(48);
            Expect<NsCallResult>(48);
            Expect<NsAllocStats>(64);
            Expect<NsRuntimeDesc>(64);
            Expect<NsObjectDesc>(40);
            Expect<NsDefine>(40);
            Expect<NsCompileDesc>(80);
            Expect<NsInstanceDesc>(16);
        }

        private static void Expect<T>(int bytes) where T : struct
        {
            int actual = Marshal.SizeOf<T>();
            if (actual != bytes)
            {
                throw new NeoException(
                    $"{typeof(T).Name} is {actual} bytes on the managed side but the C ABI expects {bytes}. " +
                    "NeoNative.cs and NeoScriptC.h have drifted apart.");
            }
        }
    }
}
