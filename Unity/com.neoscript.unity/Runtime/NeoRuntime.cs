// NeoRuntime.cs — 런타임 수명, 네이티브 객체 등록, 네이티브→관리 콜백 라우팅.
//
// IL2CPP 제약이 이 파일의 모양을 결정한다:
//  · 네이티브가 부르는 관리 메서드는 static 이어야 하고 [MonoPInvokeCallback] 이 필요하다.
//    그래서 인스턴스 라우팅은 GCHandle 을 user 포인터로 실어 보내는 방식으로 한다.
//  · 관리 예외가 네이티브 스택을 넘어가면 프로세스가 죽는다. 콜백 본문은 전부 try/catch 다.
//  · 네이티브에 넘긴 델리게이트는 static 필드가 붙잡고 있어야 한다. GC 가 썽크를 회수하면
//    네이티브는 죽은 주소를 호출한다.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using AOT;

namespace NeoScript
{
    /// <summary>스크립트가 부를 수 있는 네이티브(C#) 객체.</summary>
    public interface INeoObject
    {
        /// <summary>obj.Method(args) 디스패치. 실패하면 ctx.Fail 로 사유를 남기고 false.</summary>
        bool Invoke(NeoCallContext ctx, NeoName method);
    }

    /// <summary>obj.field 읽기/쓰기까지 받는 객체. 필요할 때만 함께 구현한다.</summary>
    public interface INeoProperties
    {
        bool GetProperty(NeoCallContext ctx, NeoName name);
        bool SetProperty(NeoCallContext ctx, NeoName name);
    }

    /// <summary>import 해석기. 못 찾으면 null 을 반환한다.</summary>
    public delegate byte[] NeoLoader(string path);

    /// <summary>인스턴스 생성 시 전역 심볼에 네이티브 객체를 붙이는 훅.</summary>
    public readonly struct NeoBindContext
    {
        private readonly NeoRuntime _runtime;
        private readonly ulong _instance;

        internal NeoBindContext(NeoRuntime runtime, ulong instance, IntPtr userData)
        {
            _runtime = runtime;
            _instance = instance;
            UserData = userData;
        }

        public NeoRuntime Runtime => _runtime;
        public IntPtr UserData { get; }

        public void BindGlobalObject(string globalName, string typeName, IntPtr userData)
            => _runtime.BindGlobalObjectRaw(_instance, globalName, typeName, userData);

        public void BindGlobalMapObject(string mapName, string key, string typeName, IntPtr userData)
            => _runtime.BindGlobalMapObjectRaw(_instance, mapName, key, typeName, userData);
    }

    public sealed class NeoRuntimeOptions
    {
        /// <summary>import 를 해석한다. Unity 에서는 보통 Resources 나 에셋번들을 읽는다.</summary>
        public NeoLoader Loader;

        /// <summary>import 상대 경로의 기준이 되는 경로.</summary>
        public string LibPath = string.Empty;

        /// <summary>인스턴스 생성 시 전역 바인딩을 다는 훅.</summary>
        public Action<NeoBindContext> OnInstanceBind;
    }

    public sealed unsafe class NeoRuntime : IDisposable
    {
        // 네이티브가 붙잡을 델리게이트. static 이라 GC 대상이 아니다 (파일 상단 주석 참고).
        private static readonly NsLogFn s_print = OnPrint;
        private static readonly NsLogFn s_error = OnError;
        private static readonly NsLoadFn s_load = OnLoad;
        private static readonly NsBindFn s_bind = OnBind;
        private static readonly NsMethodFn s_method = OnMethod;
        private static readonly NsPropertyFn s_property = OnProperty;

        private static Action<string> s_printSink;
        private static Action<string> s_errorSink;
        private static bool s_logHandlerInstalled;
        private static bool s_layoutVerified;

        private IntPtr _handle;
        private GCHandle _self;
        private readonly NeoRuntimeOptions _options;
        private readonly List<int> _ownedSlots = new List<int>();
        private readonly Dictionary<string, uint> _objectTypes = new Dictionary<string, uint>();
        private bool _frozen;

        /// <summary>
        /// 스크립트 print/error 가 나갈 곳. **프로세스 전역**이다 — 런타임마다 따로 둘 수 없다.
        /// 마지막에 설정한 쪽이 이긴다. Unity 에서는 보통 Debug.Log / Debug.LogError 를 건다.
        /// </summary>
        public static void SetLogSink(Action<string> print, Action<string> error)
        {
            s_printSink = print;
            s_errorSink = error;
            if (!s_logHandlerInstalled)
            {
                NeoNative.NsSetLogHandler(
                    Marshal.GetFunctionPointerForDelegate(s_print),
                    Marshal.GetFunctionPointerForDelegate(s_error),
                    IntPtr.Zero);
                s_logHandlerInstalled = true;
            }
        }

        public NeoRuntime(NeoRuntimeOptions options = null)
        {
            _options = options ?? new NeoRuntimeOptions();

            if (!s_layoutVerified)
            {
                NeoNativeLayout.Verify();
                uint abi = NeoNative.NsGetAbiVersion();
                if (abi != NeoNative.AbiVersion)
                {
                    throw new NeoException(
                        $"native NeoScript reports ABI {abi} but this binding was built for {NeoNative.AbiVersion}. " +
                        "Rebuild the native plugin, or update the package.");
                }
                s_layoutVerified = true;
            }

            _self = GCHandle.Alloc(this, GCHandleType.Normal);

            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int libLen = NeoUtf8.Encode(_options.LibPath, out int libOffset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                {
                    var desc = new NsRuntimeDesc
                    {
                        structSize = (uint)Marshal.SizeOf<NsRuntimeDesc>(),
                        user = GCHandle.ToIntPtr(_self),
                        load = _options.Loader != null ? Marshal.GetFunctionPointerForDelegate(s_load) : IntPtr.Zero,
                        onInstanceBind = _options.OnInstanceBind != null ? Marshal.GetFunctionPointerForDelegate(s_bind) : IntPtr.Zero,
                        method = Marshal.GetFunctionPointerForDelegate(s_method),
                        property = Marshal.GetFunctionPointerForDelegate(s_property),
                        libPath = new NsStr
                        {
                            data = libLen > 0 ? (IntPtr)(buffer + libOffset) : IntPtr.Zero,
                            len = libLen,
                        },
                    };
                    _handle = NeoNative.NsRuntimeCreate(ref desc);
                }
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }

            if (_handle == IntPtr.Zero)
            {
                _self.Free();
                throw new NeoException("NsRuntimeCreate failed.");
            }

            s_live.Add(this);
        }

        // 살아있는 런타임 목록. 에디터가 도메인을 리로드하거나 플레이를 멈출 때 네이티브
        // 런타임이 그대로 남으면 슬롯과 VM 메모리가 새기 때문에 한 번에 정리할 손잡이가 필요하다.
        private static readonly List<NeoRuntime> s_live = new List<NeoRuntime>();

        /// <summary>살아있는 모든 런타임을 파괴한다. 도메인 리로드/플레이 종료 시점용.</summary>
        public static void DisposeAll()
        {
            for (int i = s_live.Count - 1; i >= 0; --i)
                s_live[i].Dispose();
            s_live.Clear();
        }

        /// <summary>
        /// 네이티브가 들고 있는 로그 콜백을 떼어 낸다.
        /// 도메인이 리로드되면 관리 델리게이트의 썽크 주소가 사라지는데, DLL 이 계속 로드된
        /// 상태라면(플레이 중 재컴파일) 네이티브는 그 죽은 주소를 그대로 들고 있게 된다.
        /// 리로드 직전에 반드시 떼어 낼 것.
        /// </summary>
        public static void ClearLogSink()
        {
            s_printSink = null;
            s_errorSink = null;
            s_logHandlerInstalled = false;
            NeoNative.NsSetLogHandler(IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        }

        internal IntPtr Handle => _handle;

        public bool IsDisposed => _handle == IntPtr.Zero;

        private void ThrowIfDisposed()
        {
            if (_handle == IntPtr.Zero)
                throw new ObjectDisposedException(nameof(NeoRuntime));
        }

        public void Dispose()
        {
            if (_handle == IntPtr.Zero)
                return;
            // 네이티브는 런타임이 죽을 때 디스패치 슬롯을 놓아 준다. 관리 쪽 대상 테이블도
            // 같이 비워야 죽은 객체를 붙잡지 않는다 (유니티 도메인 리로드마다 반복된다).
            ReleaseSlots();
            NeoNative.NsRuntimeDestroy(_handle);
            _handle = IntPtr.Zero;
            _objectTypes.Clear();
            s_live.Remove(this);
            if (_self.IsAllocated)
                _self.Free();
        }

        //----------------------------------------------------------------------
        // 네이티브 객체 등록
        //----------------------------------------------------------------------

        /// <summary>
        /// C# 객체를 스크립트 전역 심볼로 노출한다. <see cref="Freeze"/> 전에만 가능하다.
        /// declareGlobal=false 면 스크립트가 `export var name` 으로 직접 선언하고 여기서는
        /// 디스패처만 등록한다.
        /// </summary>
        public void RegisterObject(string name, INeoObject target, bool declareGlobal = true, IntPtr userData = default)
        {
            ThrowIfDisposed();
            if (_frozen)
                throw new NeoException("RegisterObject must run before Freeze().");
            if (string.IsNullOrEmpty(name))
                throw new ArgumentException("name is required", nameof(name));
            if (target == null)
                throw new ArgumentNullException(nameof(target));

            int slot;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int nameLen = NeoUtf8.Encode(name, out int nameOffset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                {
                    var desc = new NsObjectDesc
                    {
                        structSize = (uint)Marshal.SizeOf<NsObjectDesc>(),
                        hasMethod = 1,
                        hasProperty = target is INeoProperties ? 1 : 0,
                        declareGlobal = declareGlobal ? 1 : 0,
                        name = new NsStr { data = (IntPtr)(buffer + nameOffset), len = nameLen },
                        userData = userData,
                    };
                    if (NeoNative.NsRegisterObject(_handle, ref desc, out slot) == 0)
                    {
                        throw new NeoException(
                            $"RegisterObject(\"{name}\") failed. Either the name is already taken, bindings are " +
                            $"frozen, or all {NeoNative.MaxObjectSlots} dispatch slots are in use.");
                    }
                }
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }

            // 슬롯 번호는 프로세스 전역이라 런타임별 리스트에 그대로 인덱싱할 수 없다.
            while (s_slotTargets.Count <= slot)
                s_slotTargets.Add(null);
            s_slotTargets[slot] = target;
            _ownedSlots.Add(slot);
        }

        /// <summary>등록을 마감한다. 이 뒤에는 컴파일만 가능하다.</summary>
        public void Freeze()
        {
            ThrowIfDisposed();
            if (_frozen)
                return;
            NeoNative.NsFreezeBindings(_handle);
            _frozen = true;
        }

        /// <summary>등록된 객체 타입 핸들. 맵/리스트/인자에 네이티브 객체를 실을 때 쓴다.</summary>
        public uint GetObjectType(string typeName)
        {
            ThrowIfDisposed();
            if (_objectTypes.TryGetValue(typeName, out uint cached))
                return cached;

            uint type;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(typeName, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    type = NeoNative.NsGetObjectType(_handle, buffer + offset, len);
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }

            if (type != 0)
                _objectTypes[typeName] = type;
            return type;
        }

        internal void BindGlobalObjectRaw(ulong instance, string globalName, string typeName, IntPtr userData)
        {
            uint type = GetObjectType(typeName);
            if (type == 0)
                throw new NeoException($"unknown native object type \"{typeName}\".");

            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(globalName, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    NeoNative.NsBindGlobalObject(_handle, instance, buffer + offset, len, type, userData);
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }
        }

        internal void BindGlobalMapObjectRaw(ulong instance, string mapName, string key, string typeName, IntPtr userData)
        {
            uint type = GetObjectType(typeName);
            if (type == 0)
                throw new NeoException($"unknown native object type \"{typeName}\".");

            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int mapLen = NeoUtf8.Encode(mapName, out int mapOffset);
                int keyLen = NeoUtf8.Encode(key, out int keyOffset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                {
                    NeoNative.NsBindGlobalMapObject(_handle, instance,
                        buffer + mapOffset, mapLen, buffer + keyOffset, keyLen, type, userData);
                }
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }
        }

        //----------------------------------------------------------------------
        // 컴파일 / 로드
        //----------------------------------------------------------------------

        /// <summary>소스를 컴파일한다. 실패하면 NeoException.</summary>
        public NeoProgram Compile(string source, string sourceName = "script.ns", bool includeDebugInfo = false)
        {
            ThrowIfDisposed();

            ulong program;
            NsError error;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int sourceLen = NeoUtf8.Encode(source, out int sourceOffset);
                int nameLen = NeoUtf8.Encode(sourceName, out int nameOffset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                {
                    var desc = new NsCompileDesc
                    {
                        structSize = (uint)Marshal.SizeOf<NsCompileDesc>(),
                        includeDebugInfo = includeDebugInfo ? 1 : 0,
                        source = new NsStr { data = (IntPtr)(buffer + sourceOffset), len = sourceLen },
                        sourceName = new NsStr { data = (IntPtr)(buffer + nameOffset), len = nameLen },
                    };
                    program = NeoNative.NsCompile(_handle, ref desc, out error);
                }
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }

            if (program == 0)
                throw MakeCompileException(sourceName, error);
            return new NeoProgram(this, program);
        }

        /// <summary>
        /// 바이트코드만 만든다. 빌드 타임에 구워서 플레이어에는 이미지만 싣고 싶을 때 쓴다
        /// (iOS 나 로딩 시간을 생각하면 이쪽이 정석이다).
        /// </summary>
        public byte[] CompileToBytecode(string source, string sourceName = "script.ns", bool includeDebugInfo = false)
        {
            ThrowIfDisposed();

            int ok;
            NsStr bytes;
            NsError error;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int sourceLen = NeoUtf8.Encode(source, out int sourceOffset);
                int nameLen = NeoUtf8.Encode(sourceName, out int nameOffset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                {
                    var desc = new NsCompileDesc
                    {
                        structSize = (uint)Marshal.SizeOf<NsCompileDesc>(),
                        includeDebugInfo = includeDebugInfo ? 1 : 0,
                        source = new NsStr { data = (IntPtr)(buffer + sourceOffset), len = sourceLen },
                        sourceName = new NsStr { data = (IntPtr)(buffer + nameOffset), len = nameLen },
                    };
                    ok = NeoNative.NsCompileToBytecode(_handle, ref desc, out bytes, out error);
                }
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }

            if (ok == 0)
                throw MakeCompileException(sourceName, error);
            return NeoUtf8.ToBytes(bytes);
        }

        public NeoProgram LoadProgram(byte[] bytecode)
        {
            ThrowIfDisposed();
            if (bytecode == null || bytecode.Length == 0)
                throw new ArgumentException("bytecode is empty", nameof(bytecode));

            ulong program;
            NsError error;
            fixed (byte* p = bytecode)
                program = NeoNative.NsLoadProgram(_handle, p, bytecode.Length, out error);

            if (program == 0)
                throw MakeCompileException("<bytecode>", error);
            return new NeoProgram(this, program);
        }

        private static NeoException MakeCompileException(string sourceName, in NsError error)
        {
            string message = NeoUtf8.ToString(error.message);
            string source = NeoUtf8.ToString(error.sourceName);
            if (string.IsNullOrEmpty(source))
                source = sourceName;
            string where = error.line > 0 ? $"{source}({error.line},{error.column})" : source;
            return new NeoException($"{where}: {message}", error.code);
        }

        //----------------------------------------------------------------------
        // 진단 / 메모리
        //----------------------------------------------------------------------

        public string TakeLastError()
        {
            ThrowIfDisposed();
            return NeoNative.NsTakeLastError(_handle, out NsStr text) != 0 ? NeoUtf8.ToString(text) : null;
        }

        /// <summary>빈 페이지를 OS 로 반납한다. 씬 전환이나 로딩 화면에서 부른다.</summary>
        public long TrimMemory(bool force = false) { ThrowIfDisposed(); return NeoNative.NsTrimMemory(_handle, force ? 1 : 0); }

        /// <summary>순환 참조 후보를 회수한다. 호출 시점은 호스트가 정한다.</summary>
        public int CollectCycles(bool force = false) { ThrowIfDisposed(); return NeoNative.NsCollectCycles(_handle, force ? 1 : 0); }

        public static NsAllocStats GetAllocStats()
        {
            NeoNative.NsGetAllocStats(out NsAllocStats stats);
            return stats;
        }

        //----------------------------------------------------------------------
        // 네이티브 → 관리 트램폴린
        //
        // 슬롯 번호는 프로세스 전역이라 대상 테이블도 전역이다. 런타임이 죽으면 네이티브가
        // 슬롯을 놓아 주므로, 여기서도 같이 비워서 죽은 객체를 붙잡지 않게 한다.
        //----------------------------------------------------------------------
        private static readonly List<INeoObject> s_slotTargets = new List<INeoObject>();

        internal void ReleaseSlots()
        {
            foreach (int slot in _ownedSlots)
            {
                if (slot >= 0 && slot < s_slotTargets.Count)
                    s_slotTargets[slot] = null;
            }
            _ownedSlots.Clear();
        }

        [MonoPInvokeCallback(typeof(NsLogFn))]
        private static void OnPrint(IntPtr user, IntPtr msg, int len)
        {
            try { s_printSink?.Invoke(NeoUtf8.ToString(msg, len)); }
            catch { /* 관리 예외가 네이티브 스택을 넘어가면 안 된다 */ }
        }

        [MonoPInvokeCallback(typeof(NsLogFn))]
        private static void OnError(IntPtr user, IntPtr msg, int len)
        {
            try { s_errorSink?.Invoke(NeoUtf8.ToString(msg, len)); }
            catch { }
        }

        [MonoPInvokeCallback(typeof(NsLoadFn))]
        private static int OnLoad(IntPtr user, IntPtr path, int pathLen, IntPtr sink)
        {
            try
            {
                var runtime = GCHandle.FromIntPtr(user).Target as NeoRuntime;
                NeoLoader loader = runtime?._options.Loader;
                if (loader == null)
                    return 0;

                byte[] content = loader(NeoUtf8.ToString(path, pathLen));
                if (content == null)
                    return 0;
                if (content.Length > 0)
                {
                    fixed (byte* p = content)
                        NeoNative.NsSinkWrite(sink, p, content.Length);
                }
                return 1;
            }
            catch
            {
                return 0;
            }
        }

        [MonoPInvokeCallback(typeof(NsBindFn))]
        private static void OnBind(IntPtr user, IntPtr rt, ulong instance, IntPtr instanceUserData)
        {
            try
            {
                var runtime = GCHandle.FromIntPtr(user).Target as NeoRuntime;
                runtime?._options.OnInstanceBind?.Invoke(new NeoBindContext(runtime, instance, instanceUserData));
            }
            catch { }
        }

        [MonoPInvokeCallback(typeof(NsMethodFn))]
        private static int OnMethod(IntPtr user, int slot, IntPtr ctx, IntPtr method, int methodLen)
        {
            var context = new NeoCallContext(ctx);
            try
            {
                INeoObject target = slot >= 0 && slot < s_slotTargets.Count ? s_slotTargets[slot] : null;
                if (target == null)
                {
                    context.Fail(1, "native object is no longer bound");
                    return 0;
                }
                return target.Invoke(context, new NeoName((byte*)method, methodLen)) ? 1 : 0;
            }
            catch (Exception e)
            {
                // 여기서 예외를 흘리면 프로세스가 죽는다. 스크립트 쪽 런타임 에러로 바꿔 넘긴다.
                try { context.Fail(1, e.Message); } catch { }
                return 0;
            }
        }

        [MonoPInvokeCallback(typeof(NsPropertyFn))]
        private static int OnProperty(IntPtr user, int slot, IntPtr ctx, IntPtr name, int nameLen, int isGet)
        {
            var context = new NeoCallContext(ctx);
            try
            {
                INeoObject target = slot >= 0 && slot < s_slotTargets.Count ? s_slotTargets[slot] : null;
                if (!(target is INeoProperties properties))
                {
                    context.Fail(1, "native object has no properties");
                    return 0;
                }
                var propertyName = new NeoName((byte*)name, nameLen);
                return (isGet != 0
                    ? properties.GetProperty(context, propertyName)
                    : properties.SetProperty(context, propertyName)) ? 1 : 0;
            }
            catch (Exception e)
            {
                try { context.Fail(1, e.Message); } catch { }
                return 0;
            }
        }
    }
}
