// NeoCallContext.cs — 스크립트가 C# 객체의 메서드/프로퍼티를 부를 때 받는 컨텍스트.
//
// 유효 범위는 디스패처 호출 안이다. 필드에 보관하면 죽은 VM 슬롯을 가리키게 된다.

using System;
using UnityEngine;

namespace NeoScript
{
    public readonly unsafe struct NeoCallContext
    {
        private readonly IntPtr _ctx;

        internal NeoCallContext(IntPtr ctx)
        {
            _ctx = ctx;
        }

        /// <summary>이 바인딩의 userData. BindObject 로 준 인스턴스별 값, 없으면 등록 시의 기본값.</summary>
        public IntPtr UserData => NeoNative.NsCtxUserData(_ctx);

        /// <summary>인스턴스 생성 시 준 userData.</summary>
        public IntPtr InstanceUserData => NeoNative.NsCtxInstanceUserData(_ctx);

        public int ArgCount => NeoNative.NsCtxArgCount(_ctx);

        public NeoValueType ArgType(int index) => (NeoValueType)NeoNative.NsCtxArgType(_ctx, index);

        //----------------------------------------------------------------------
        // 인자 읽기 — VM 슬롯 직접 읽기라 복사가 없다
        //----------------------------------------------------------------------
        public int ArgInt(int index) => NeoNative.NsCtxArgInt(_ctx, index);
        public float ArgFloat(int index) => NeoNative.NsCtxArgFloat(_ctx, index);
        public bool ArgBool(int index) => NeoNative.NsCtxArgBool(_ctx, index) != 0;

        public string ArgString(int index)
        {
            NeoNative.NsCtxArgString(_ctx, index, out NsStr text);
            return NeoUtf8.ToString(text);
        }

        public Vector2 ArgVector2(int index)
        {
            float* v = stackalloc float[4];
            NeoNative.NsCtxArgVec(_ctx, index, v);
            return new Vector2(v[0], v[1]);
        }

        public Vector3 ArgVector3(int index)
        {
            float* v = stackalloc float[4];
            NeoNative.NsCtxArgVec(_ctx, index, v);
            return new Vector3(v[0], v[1], v[2]);
        }

        public Vector4 ArgVector4(int index)
        {
            float* v = stackalloc float[4];
            NeoNative.NsCtxArgVec(_ctx, index, v);
            return new Vector4(v[0], v[1], v[2], v[3]);
        }

        public Quaternion ArgQuaternion(int index)
        {
            float* v = stackalloc float[4];
            NeoNative.NsCtxArgVec(_ctx, index, v);
            return new Quaternion(v[0], v[1], v[2], v[3]);
        }

        /// <summary>map 인자. 유효하지 않으면 IsValid==false.</summary>
        public NeoMapReader ArgMap(int index) => new NeoMapReader(NeoNative.NsCtxArgMap(_ctx, index));

        /// <summary>list 인자.</summary>
        public NeoListReader ArgList(int index) => new NeoListReader(NeoNative.NsCtxArgList(_ctx, index));

        /// <summary>원시 배열 인자. 포인터는 이 디스패처 호출 안에서만 유효하다.</summary>
        public NeoArrayView ArgArray(int index) => new NeoArrayView(NeoNative.NsCtxArgArray(_ctx, index));

        /// <summary>네이티브 객체 인자에서 그 객체의 userData 를 꺼낸다. 객체가 아니면 Zero.</summary>
        public IntPtr ArgObjectUserData(int index) => NeoNative.NsCtxArgObjectUserData(_ctx, index);

        /// <summary>
        /// 스크립트 함수/람다 인자를 보관 가능한 핸들로 받는다. 다 쓰면 Dispose 할 것.
        /// 원본 인스턴스가 Destroy/Reset 되면 자동으로 무효가 된다.
        /// </summary>
        public NeoFunction ArgFunction(int index)
        {
            IntPtr fn = NeoNative.NsCtxArgFunction(_ctx, index);
            return fn == IntPtr.Zero ? null : new NeoFunction(fn);
        }

        //----------------------------------------------------------------------
        // 반환 쓰기
        //----------------------------------------------------------------------
        public void Return(int value) => NeoNative.NsCtxRetInt(_ctx, value);
        public void Return(float value) => NeoNative.NsCtxRetFloat(_ctx, value);
        public void Return(bool value) => NeoNative.NsCtxRetBool(_ctx, value ? 1 : 0);
        public void Return(Vector2 value) => NeoNative.NsCtxRetVec2(_ctx, value.x, value.y);
        public void Return(Vector3 value) => NeoNative.NsCtxRetVec3(_ctx, value.x, value.y, value.z);
        public void Return(Vector4 value) => NeoNative.NsCtxRetVec4(_ctx, value.x, value.y, value.z, value.w);
        public void Return(Quaternion value) => NeoNative.NsCtxRetVec4(_ctx, value.x, value.y, value.z, value.w);
        public void ReturnNull() => NeoNative.NsCtxRetNull(_ctx);

        public void Return(string value)
        {
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(value, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    NeoNative.NsCtxRetString(_ctx, buffer + offset, len);
            }
            finally { NeoUtf8.Pop(frame); }
        }

        /// <summary>바인딩된 네이티브 객체를 반환한다. type 은 Runtime.GetObjectType 로 얻는다.</summary>
        public void ReturnObject(uint objectType, IntPtr userData) => NeoNative.NsCtxRetObject(_ctx, objectType, userData);

        /// <summary>반환 슬롯에 map 을 조립한다.</summary>
        public NeoMapBuilder ReturnMap() => new NeoMapBuilder(NeoNative.NsCtxRetMap(_ctx));

        /// <summary>반환 슬롯에 list 를 조립한다.</summary>
        public NeoListBuilder ReturnList() => new NeoListBuilder(NeoNative.NsCtxRetList(_ctx));

        /// <summary>
        /// 실패 사유를 남긴다. 부른 뒤 디스패처는 반드시 false 를 반환해야 한다.
        /// 남긴 메시지는 스크립트 스택트레이스와 함께 조립되어 호출자에게 나간다.
        /// </summary>
        public void Fail(int code, string message)
        {
            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int len = NeoUtf8.Encode(message, out int offset);
                fixed (byte* buffer = NeoUtf8.Buffer)
                    NeoNative.NsCtxFail(_ctx, code, buffer + offset, len);
            }
            finally { NeoUtf8.Pop(frame); }
        }
    }
}
