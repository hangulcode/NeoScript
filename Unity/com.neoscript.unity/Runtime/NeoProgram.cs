// NeoProgram.cs — Program / Function / Instance.
//
// Program 은 불변 공유 코드, Instance 는 그 위의 전역 상태다. 하나의 Program 으로 인스턴스를
// 여럿 찍는 것이 정상 사용법이다(적 100마리 = Program 1 + Instance 100).

using System;

namespace NeoScript
{
    /// <summary>컴파일된 불변 이미지. 인스턴스 여럿이 공유한다.</summary>
    public sealed class NeoProgram : IDisposable
    {
        private readonly NeoRuntime _runtime;
        private ulong _handle;

        internal NeoProgram(NeoRuntime runtime, ulong handle)
        {
            _runtime = runtime;
            _handle = handle;
        }

        internal ulong Handle => _handle;
        public NeoRuntime Runtime => _runtime;

        /// <summary>이 프로그램의 인스턴스를 만든다.</summary>
        /// <param name="runGlobalInit">스크립트 최상위 코드를 지금 실행할지.</param>
        /// <param name="userData">인스턴스 일반 데이터. 네이티브 디스패처가 ctx.InstanceUserData 로 받는다.</param>
        public NeoInstance CreateInstance(bool runGlobalInit = true, IntPtr userData = default)
        {
            if (_handle == 0)
                throw new ObjectDisposedException(nameof(NeoProgram));

            var desc = new NsInstanceDesc
            {
                structSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<NsInstanceDesc>(),
                runGlobalInit = runGlobalInit ? 1 : 0,
                userData = userData,
            };
            ulong instance = NeoNative.NsCreateInstance(_runtime.Handle, _handle, ref desc);
            if (instance == 0)
                throw new NeoException("CreateInstance failed: " + (_runtime.TakeLastError() ?? "unknown reason"));
            return new NeoInstance(_runtime, instance);
        }

        /// <summary>함수 핸들을 미리 받아 둔다. 매 프레임 부르는 함수라면 이걸 캐싱할 것.</summary>
        public NeoFunction FindFunction(string name)
        {
            if (_handle == 0)
                throw new ObjectDisposedException(nameof(NeoProgram));

            IntPtr fn;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                unsafe
                {
                    fixed (byte* buffer = NeoUtf8.Buffer)
                        fn = NeoNative.NsFindFunction(_runtime.Handle, _handle, buffer + offset, len);
                }
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }

            return fn == IntPtr.Zero ? null : new NeoFunction(fn);
        }

        public void Dispose()
        {
            if (_handle == 0 || _runtime.IsDisposed)
            {
                _handle = 0;
                return;
            }
            NeoNative.NsDestroyProgram(_runtime.Handle, _handle);
            _handle = 0;
        }
    }

    /// <summary>
    /// 이름 조회를 건너뛰는 함수 핸들. 캡처 람다를 스크립트에서 받아 온 경우 핸들 자체가
    /// 캡처 보관함을 소유하므로, 인덱스만 따로 저장하면 안 되고 이 객체를 들고 있어야 한다.
    /// </summary>
    public sealed class NeoFunction : IDisposable
    {
        private IntPtr _handle;

        internal NeoFunction(IntPtr handle)
        {
            _handle = handle;
        }

        internal IntPtr Handle => _handle;
        public bool IsValid => _handle != IntPtr.Zero && NeoNative.NsFunctionValid(_handle) != 0;

        public void Dispose()
        {
            if (_handle == IntPtr.Zero)
                return;
            NeoNative.NsFunctionRelease(_handle);
            _handle = IntPtr.Zero;
        }
    }

    /// <summary>프로그램 하나의 실행 상태 + 전역 변수.</summary>
    public sealed unsafe class NeoInstance : IDisposable
    {
        private readonly NeoRuntime _runtime;
        private ulong _handle;

        internal NeoInstance(NeoRuntime runtime, ulong handle)
        {
            _runtime = runtime;
            _handle = handle;
        }

        internal ulong Handle => _handle;
        public NeoRuntime Runtime => _runtime;
        public bool IsAlive => _handle != 0 && !_runtime.IsDisposed && NeoNative.NsIsAlive(_runtime.Handle, _handle) != 0;
        public NeoInstanceState State => (NeoInstanceState)NeoNative.NsGetState(_runtime.Handle, _handle);
        public bool IsRunning => NeoNative.NsIsRunning(_runtime.Handle, _handle) != 0;

        private void ThrowIfDead()
        {
            if (_handle == 0 || _runtime.IsDisposed)
                throw new ObjectDisposedException(nameof(NeoInstance));
        }

        //----------------------------------------------------------------------
        // 호출
        //----------------------------------------------------------------------

        /// <summary>이름으로 호출을 시작한다. 반환된 NeoCall 은 Invoke* 로 끝내야 한다.</summary>
        public NeoCall Call(string functionName)
        {
            ThrowIfDead();
            IntPtr call;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(functionName, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    call = NeoNative.NsCallBeginName(_runtime.Handle, _handle, buffer + offset, len);
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }
            return new NeoCall(call);
        }

        /// <summary>캐싱한 함수 핸들로 호출을 시작한다. 이름 조회가 없다.</summary>
        public NeoCall Call(NeoFunction function)
        {
            ThrowIfDead();
            if (function == null)
                throw new ArgumentNullException(nameof(function));
            return new NeoCall(NeoNative.NsCallBegin(_runtime.Handle, _handle, function.Handle));
        }

        //----------------------------------------------------------------------
        // 중단 / 재개 / 시간분할
        //----------------------------------------------------------------------

        /// <summary>CreateInstance(runGlobalInit:false) 로 만든 인스턴스의 최상위 코드를 실행한다.</summary>
        public NeoRunStatus RunGlobalInit()
        {
            ThrowIfDead();
            return (NeoRunStatus)NeoNative.NsRunGlobalInit(_runtime.Handle, _handle);
        }

        /// <summary>정지된 실행을 재개한다.</summary>
        public NeoRunStatus Resume(uint instructionBudget = 0, float deltaTime = 0f)
        {
            ThrowIfDead();
            return (NeoRunStatus)NeoNative.NsResume(_runtime.Handle, _handle, instructionBudget, deltaTime);
        }

        /// <summary>진행 중이거나 정지된 실행을 버리고 Idle 로 되돌린다. 전역 변수는 보존된다.</summary>
        public bool Cancel()
        {
            ThrowIfDead();
            return NeoNative.NsCancel(_runtime.Handle, _handle) != 0;
        }

        /// <summary>전역 변수를 초기화하고 바인딩을 재구성한다. 핸들은 그대로 유효하다.</summary>
        public bool Reset()
        {
            ThrowIfDead();
            return NeoNative.NsResetInstance(_runtime.Handle, _handle) != 0;
        }

        /// <summary>
        /// 오래 걸리는 함수를 슬라이스로 쪼개 실행한다. 시작 후 <see cref="UpdateSliced"/> 를
        /// Suspended 가 아닐 때까지 매 프레임 돌린다.
        /// </summary>
        public bool StartSliced(string functionName, int timeoutMs = -1, uint budget = 0)
        {
            ThrowIfDead();
            int ok;
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(functionName, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    ok = NeoNative.NsStartSliced(_runtime.Handle, _handle, buffer + offset, len, timeoutMs, budget);
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }
            return ok != 0;
        }

        /// <summary>한 슬라이스 진행. Completed = 끝, Suspended = 계속 돌릴 것.</summary>
        public NeoRunStatus UpdateSliced()
        {
            ThrowIfDead();
            return (NeoRunStatus)NeoNative.NsUpdateSliced(_runtime.Handle, _handle);
        }

        //----------------------------------------------------------------------
        // 전역 변수
        //
        // [주의] 호스트가 이름으로 볼 수 있는 전역은 스크립트가 `export var` 로 내보낸 것뿐이다.
        // 그냥 `var` 로 선언한 전역은 여기서 찾지 못한다.
        //----------------------------------------------------------------------

        public bool TryGetInt(string name, out int value)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    return NeoNative.NsGetGlobalInt(_runtime.Handle, _handle, buffer + offset, len, out value) != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGetFloat(string name, out float value)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    return NeoNative.NsGetGlobalFloat(_runtime.Handle, _handle, buffer + offset, len, out value) != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGetString(string name, out string value)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                NsStr text;
                int ok;
                fixed (byte* buffer = NeoUtf8.Buffer)
                    ok = NeoNative.NsGetGlobalString(_runtime.Handle, _handle, buffer + offset, len, out text);
                value = ok != 0 ? NeoUtf8.ToString(text) : null;
                return ok != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool SetInt(string name, int value)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    return NeoNative.NsSetGlobalInt(_runtime.Handle, _handle, buffer + offset, len, value) != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool SetFloat(string name, float value)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    return NeoNative.NsSetGlobalFloat(_runtime.Handle, _handle, buffer + offset, len, value) != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool SetString(string name, string value)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                // 고정하기 전에 둘 다 인코딩한다 — Encode 는 버퍼를 재할당할 수 있다.
                int nameLen = NeoUtf8.Encode(name, out int nameOffset);
                int valueLen = NeoUtf8.Encode(value, out int valueOffset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                {
                    return NeoNative.NsSetGlobalString(_runtime.Handle, _handle,
                        buffer + nameOffset, nameLen, buffer + valueOffset, valueLen) != 0;
                }
            }
            finally { NeoUtf8.Pop(frame); }
        }

        /// <summary>이 인스턴스에 한해 등록된 네이티브 객체의 userData 를 덮어쓴다.</summary>
        public bool BindObject(string name, IntPtr userData)
        {
            ThrowIfDead();
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(name, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    return NeoNative.NsBindObject(_runtime.Handle, _handle, buffer + offset, len, userData) != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public void Dispose()
        {
            if (_handle == 0)
                return;
            if (!_runtime.IsDisposed)
                NeoNative.NsDestroyInstance(_runtime.Handle, _handle);
            _handle = 0;
        }
    }
}
