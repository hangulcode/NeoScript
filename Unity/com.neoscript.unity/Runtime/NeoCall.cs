// NeoCall.cs — 호스트 → 스크립트 호출.
//
// C++ API 에는 두 가지 반환 읽기가 있다. 저수준(invoke + retX)은 인스턴스의 공유 컨텍스트를
// 라이브로 읽어서 다음 호출에 무효가 되고, 안전한 쪽(invokeR / invokeRead*)은 값을 떼어 준다.
// C# 에는 **안전한 쪽만** 낸다. 저수준 경로는 이 언어에서 방어할 방법이 없는 함정이고,
// 얻는 건 스칼라 복사 한 번뿐이다.
//
// 그래서 모든 Invoke* 종결자는 호출을 자동으로 닫는다. NeoCall 을 만들어 놓고 종결자를
// 부르지 않으면 인스턴스의 실행 컨텍스트가 묶인 채로 남으므로, 만들면 반드시 끝낼 것.

using System;
using AOT;
using UnityEngine;

namespace NeoScript
{
    /// <summary>스칼라/벡터 반환을 값으로 떼어 온 결과. 이후 다른 호출과 무관하게 안전하다.</summary>
    public readonly struct NeoResult
    {
        public readonly NeoRunStatus Status;
        public readonly NeoValueType Type;
        private readonly int _int;
        private readonly float _f0, _f1, _f2, _f3;
        private readonly string _string;
        private readonly string _error;

        internal NeoResult(in NsCallResult raw, string error)
        {
            Status = (NeoRunStatus)raw.status;
            Type = (NeoValueType)raw.type;
            _int = raw.i;
            _f0 = raw.f0;
            _f1 = raw.f1;
            _f2 = raw.f2;
            _f3 = raw.f3;
            _string = Type == NeoValueType.String ? NeoUtf8.ToString(raw.str) : null;
            _error = error;
        }

        public bool Ok => Status == NeoRunStatus.Completed;

        /// <summary>실패했을 때의 사유. 성공이면 null.</summary>
        public string Error => _error;

        public int AsInt => _int;
        public bool AsBool => _int != 0;
        public float AsFloat => _f0;
        public string AsString => _string ?? string.Empty;
        public Vector2 AsVector2 => new Vector2(_f0, _f1);
        public Vector3 AsVector3 => new Vector3(_f0, _f1, _f2);
        public Vector4 AsVector4 => new Vector4(_f0, _f1, _f2, _f3);
        public Quaternion AsQuaternion => new Quaternion(_f0, _f1, _f2, _f3);

        internal NeoException ToException(string what)
            => new NeoException($"{what} failed ({Status}): {_error ?? "no reason reported"}");
    }

    /// <summary>
    /// 인자를 쌓고 실행한다. Invoke* 종결자가 호출을 닫는다 — ref struct 라 필드에 보관할 수 없다.
    /// </summary>
    public readonly ref struct NeoCall
    {
        private readonly IntPtr _call;

        internal NeoCall(IntPtr call)
        {
            _call = call;
        }

        public bool IsValid => _call != IntPtr.Zero;

        //----------------------------------------------------------------------
        // 인자
        //----------------------------------------------------------------------
        public NeoCall Arg(int value) { NeoNative.NsCallArgInt(_call, value); return this; }
        public NeoCall Arg(float value) { NeoNative.NsCallArgFloat(_call, value); return this; }
        public NeoCall Arg(bool value) { NeoNative.NsCallArgBool(_call, value ? 1 : 0); return this; }
        public NeoCall Arg(Vector3 value) { NeoNative.NsCallArgVec3(_call, value.x, value.y, value.z); return this; }

        public unsafe NeoCall Arg(string value)
        {
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(value, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    NeoNative.NsCallArgString(_call, buffer + offset, len);
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }
            return this;
        }

        /// <summary>바인딩된 네이티브 객체를 인자로 넘긴다. type 은 Runtime.GetObjectType 로 얻는다.</summary>
        public NeoCall ArgObject(uint objectType, IntPtr userData)
        {
            NeoNative.NsCallArgObject(_call, objectType, userData);
            return this;
        }

        /// <summary>이 호출의 벽시계 상한(ms). 스크립트가 무한루프에 빠져도 프레임을 지킨다.</summary>
        public NeoCall Timeout(int milliseconds) { NeoNative.NsCallTimeout(_call, milliseconds); return this; }

        /// <summary>이 호출의 명령어 예산.</summary>
        public NeoCall Budget(uint instructions) { NeoNative.NsCallBudget(_call, instructions); return this; }

        //----------------------------------------------------------------------
        // 종결자 — 전부 호출을 닫는다
        //----------------------------------------------------------------------

        /// <summary>실행하고 결과를 값으로 떼어 온다. 실패해도 예외를 던지지 않는다.</summary>
        public NeoResult Invoke()
        {
            if (_call == IntPtr.Zero)
                throw new NeoException("call could not be started (unknown function, or the instance is busy).");

            NeoNative.NsCallInvokeR(_call, out NsCallResult raw);
            string error = null;
            if (raw.status != (int)NeoRunStatus.Completed)
            {
                NeoNative.NsCallGetError(_call, out NsError err);
                error = NeoUtf8.ToString(err.message);
            }
            var result = new NeoResult(raw, error);
            NeoNative.NsCallEnd(_call);
            return result;
        }

        /// <summary>실행하고 반환을 버린다. 실패하면 NeoException.</summary>
        public void InvokeVoid()
        {
            NeoResult r = Invoke();
            if (!r.Ok)
                throw r.ToException("call");
        }

        public int InvokeInt()
        {
            NeoResult r = Invoke();
            if (!r.Ok) throw r.ToException("call");
            return r.AsInt;
        }

        public float InvokeFloat()
        {
            NeoResult r = Invoke();
            if (!r.Ok) throw r.ToException("call");
            return r.AsFloat;
        }

        public bool InvokeBool()
        {
            NeoResult r = Invoke();
            if (!r.Ok) throw r.ToException("call");
            return r.AsBool;
        }

        public string InvokeString()
        {
            NeoResult r = Invoke();
            if (!r.Ok) throw r.ToException("call");
            return r.AsString;
        }

        public Vector3 InvokeVector3()
        {
            NeoResult r = Invoke();
            if (!r.Ok) throw r.ToException("call");
            return r.AsVector3;
        }

        /// <summary>
        /// map 반환을 콜백 안에서 읽는다. 리더는 콜백 밖에서 유효하지 않으니 필요한 값은
        /// 안에서 복사해 갈 것.
        /// </summary>
        public NeoRunStatus InvokeReadMap(Action<NeoMapReader> read)
        {
            if (_call == IntPtr.Zero)
                throw new NeoException("call could not be started.");
            if (read == null)
                throw new ArgumentNullException(nameof(read));

            Action<NeoMapReader> previous = t_mapReader;
            t_mapReader = read;
            try
            {
                int status = NeoNative.NsCallInvokeReadMap(_call, s_readMapPtr, IntPtr.Zero);
                return (NeoRunStatus)status;
            }
            finally
            {
                t_mapReader = previous;
                NeoNative.NsCallEnd(_call);
            }
        }

        /// <summary>list 반환을 콜백 안에서 읽는다.</summary>
        public NeoRunStatus InvokeReadList(Action<NeoListReader> read)
        {
            if (_call == IntPtr.Zero)
                throw new NeoException("call could not be started.");
            if (read == null)
                throw new ArgumentNullException(nameof(read));

            Action<NeoListReader> previous = t_listReader;
            t_listReader = read;
            try
            {
                int status = NeoNative.NsCallInvokeReadList(_call, s_readListPtr, IntPtr.Zero);
                return (NeoRunStatus)status;
            }
            finally
            {
                t_listReader = previous;
                NeoNative.NsCallEnd(_call);
            }
        }

        /// <summary>원시 배열 반환을 콜백 안에서 읽는다. 포인터는 콜백 밖에서 무효다.</summary>
        public NeoRunStatus InvokeReadArray(Action<NeoArrayView> read)
        {
            if (_call == IntPtr.Zero)
                throw new NeoException("call could not be started.");
            if (read == null)
                throw new ArgumentNullException(nameof(read));

            Action<NeoArrayView> previous = t_arrayView;
            t_arrayView = read;
            try
            {
                int status = NeoNative.NsCallInvokeReadArray(_call, s_readArrayPtr, IntPtr.Zero);
                return (NeoRunStatus)status;
            }
            finally
            {
                t_arrayView = previous;
                NeoNative.NsCallEnd(_call);
            }
        }

        /// <summary>인자만 쌓고 실행하지 않은 채 버린다. 예외 경로 정리용.</summary>
        public void Abandon()
        {
            if (_call != IntPtr.Zero)
                NeoNative.NsCallEnd(_call);
        }

        //----------------------------------------------------------------------
        // 읽기 콜백 트램폴린
        //
        // 델리게이트를 GCHandle 로 넘기면 호출마다 할당이 생긴다. 콜백이 같은 스레드에서
        // 동기적으로 되돌아오므로 [ThreadStatic] 으로 넘기고, 중첩에 대비해 이전 값을
        // 저장·복원한다.
        //----------------------------------------------------------------------
        [ThreadStatic] private static Action<NeoMapReader> t_mapReader;
        [ThreadStatic] private static Action<NeoListReader> t_listReader;
        [ThreadStatic] private static Action<NeoArrayView> t_arrayView;

        private static readonly NsReadMapFn s_readMap = OnReadMap;
        private static readonly NsReadListFn s_readList = OnReadList;
        private static readonly NsReadArrayFn s_readArray = OnReadArray;

        private static readonly IntPtr s_readMapPtr =
            System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(s_readMap);
        private static readonly IntPtr s_readListPtr =
            System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(s_readList);
        private static readonly IntPtr s_readArrayPtr =
            System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(s_readArray);

        [MonoPInvokeCallback(typeof(NsReadMapFn))]
        private static void OnReadMap(IntPtr user, IntPtr reader)
        {
            try { t_mapReader?.Invoke(new NeoMapReader(reader)); }
            catch (Exception e) { Debug.LogException(e); }
        }

        [MonoPInvokeCallback(typeof(NsReadListFn))]
        private static void OnReadList(IntPtr user, IntPtr reader)
        {
            try { t_listReader?.Invoke(new NeoListReader(reader)); }
            catch (Exception e) { Debug.LogException(e); }
        }

        [MonoPInvokeCallback(typeof(NsReadArrayFn))]
        private static void OnReadArray(IntPtr user, IntPtr view)
        {
            try { t_arrayView?.Invoke(new NeoArrayView(view)); }
            catch (Exception e) { Debug.LogException(e); }
        }
    }
}
