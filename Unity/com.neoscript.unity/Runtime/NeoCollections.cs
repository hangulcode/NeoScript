// NeoCollections.cs — map / list / 원시 배열의 읽기·쓰기 핸들.
//
// 전부 VM 저장소 위의 빌린 뷰다. 값을 복사해 오지 않으므로 빠르지만, 발급한 스코프
// (네이티브 디스패처 호출, 또는 Invoke*Read 콜백) 밖으로 들고 나가면 안 된다.
// struct 로 둔 것은 이 수명 규칙을 코드 모양으로 드러내기 위함이다.

using System;
using UnityEngine;

namespace NeoScript
{
    /// <summary>map 을 조립한다.</summary>
    public readonly unsafe struct NeoMapBuilder
    {
        private readonly IntPtr _handle;
        internal NeoMapBuilder(IntPtr handle) { _handle = handle; }
        public bool IsValid => _handle != IntPtr.Zero;

        public void Set(string key, int value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsMapSetInt(_handle, b + o, n, value); }
            finally { NeoUtf8.Pop(frame); }
        }

        public void Set(string key, float value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsMapSetFloat(_handle, b + o, n, value); }
            finally { NeoUtf8.Pop(frame); }
        }

        public void Set(string key, bool value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsMapSetBool(_handle, b + o, n, value ? 1 : 0); }
            finally { NeoUtf8.Pop(frame); }
        }

        public void Set(string key, string value)
        {
            var frame = NeoUtf8.Push();
            try
            {
                // 고정 전에 둘 다 인코딩한다 — Encode 는 버퍼를 재할당할 수 있다.
                int keyLen = NeoUtf8.Encode(key, out int keyOffset);
                int valueLen = NeoUtf8.Encode(value, out int valueOffset);
                fixed (byte* b = NeoUtf8.Buffer)
                    NeoNative.NsMapSetString(_handle, b + keyOffset, keyLen, b + valueOffset, valueLen);
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public void Set(string key, Vector3 value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsMapSetVec3(_handle, b + o, n, value.x, value.y, value.z); }
            finally { NeoUtf8.Pop(frame); }
        }

        /// <summary>바인딩된 네이티브 객체를 값으로 넣는다.</summary>
        public void SetObject(string key, uint objectType, IntPtr userData)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsMapSetObject(_handle, b + o, n, objectType, userData); }
            finally { NeoUtf8.Pop(frame); }
        }

        public NeoMapBuilder SetMap(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return new NeoMapBuilder(NeoNative.NsMapSetMap(_handle, b + o, n)); }
            finally { NeoUtf8.Pop(frame); }
        }

        public NeoListBuilder SetList(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return new NeoListBuilder(NeoNative.NsMapSetList(_handle, b + o, n)); }
            finally { NeoUtf8.Pop(frame); }
        }
    }

    /// <summary>list 를 조립한다.</summary>
    public readonly unsafe struct NeoListBuilder
    {
        private readonly IntPtr _handle;
        internal NeoListBuilder(IntPtr handle) { _handle = handle; }
        public bool IsValid => _handle != IntPtr.Zero;

        public int Count => NeoNative.NsListCount(_handle);
        public void Reserve(int count) => NeoNative.NsListReserve(_handle, count);
        public void Resize(int count) => NeoNative.NsListResize(_handle, count);

        public void Push(int value) => NeoNative.NsListPushInt(_handle, value);
        public void Push(float value) => NeoNative.NsListPushFloat(_handle, value);
        public void Push(bool value) => NeoNative.NsListPushBool(_handle, value ? 1 : 0);
        public void Push(Vector3 value) => NeoNative.NsListPushVec3(_handle, value.x, value.y, value.z);
        public void PushObject(uint objectType, IntPtr userData) => NeoNative.NsListPushObject(_handle, objectType, userData);

        public void Push(string value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(value, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsListPushString(_handle, b + o, n); }
            finally { NeoUtf8.Pop(frame); }
        }

        public void Set(int index, int value) => NeoNative.NsListSetInt(_handle, index, value);
        public void Set(int index, float value) => NeoNative.NsListSetFloat(_handle, index, value);

        public void Set(int index, string value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(value, out int o); fixed (byte* b = NeoUtf8.Buffer) NeoNative.NsListSetString(_handle, index, b + o, n); }
            finally { NeoUtf8.Pop(frame); }
        }

        public NeoMapBuilder PushMap() => new NeoMapBuilder(NeoNative.NsListPushMap(_handle));
        public NeoListBuilder PushList() => new NeoListBuilder(NeoNative.NsListPushList(_handle));
    }

    /// <summary>map 을 읽는다. 발급 스코프 밖에서 쓰면 안 된다.</summary>
    public readonly unsafe struct NeoMapReader
    {
        private readonly IntPtr _handle;
        internal NeoMapReader(IntPtr handle) { _handle = handle; }
        public bool IsValid => _handle != IntPtr.Zero;

        public bool Has(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return NeoNative.NsMapHas(_handle, b + o, n) != 0; }
            finally { NeoUtf8.Pop(frame); }
        }

        public NeoValueType TypeOf(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return (NeoValueType)NeoNative.NsMapType(_handle, b + o, n); }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGet(string key, out int value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return NeoNative.NsMapGetInt(_handle, b + o, n, out value) != 0; }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGet(string key, out float value)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return NeoNative.NsMapGetFloat(_handle, b + o, n, out value) != 0; }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGet(string key, out bool value)
        {
            var frame = NeoUtf8.Push();
            try
            {
                int n = NeoUtf8.Encode(key, out int o);
                int raw;
                fixed (byte* b = NeoUtf8.Buffer)
                {
                    if (NeoNative.NsMapGetBool(_handle, b + o, n, out raw) == 0) { value = false; return false; }
                }
                value = raw != 0;
                return true;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGet(string key, out string value)
        {
            var frame = NeoUtf8.Push();
            try
            {
                int n = NeoUtf8.Encode(key, out int o);
                NsStr text;
                int ok;
                fixed (byte* b = NeoUtf8.Buffer)
                    ok = NeoNative.NsMapGetString(_handle, b + o, n, out text);
                value = ok != 0 ? NeoUtf8.ToString(text) : null;
                return ok != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public bool TryGet(string key, out Vector3 value)
        {
            var frame = NeoUtf8.Push();
            try
            {
                int n = NeoUtf8.Encode(key, out int o);
                float* v = stackalloc float[4];
                int ok;
                fixed (byte* b = NeoUtf8.Buffer)
                    ok = NeoNative.NsMapGetVec(_handle, b + o, n, v);
                value = ok != 0 ? new Vector3(v[0], v[1], v[2]) : default;
                return ok != 0;
            }
            finally { NeoUtf8.Pop(frame); }
        }

        public int GetInt(string key, int fallback = 0) => TryGet(key, out int v) ? v : fallback;
        public float GetFloat(string key, float fallback = 0f) => TryGet(key, out float v) ? v : fallback;
        public bool GetBool(string key, bool fallback = false) => TryGet(key, out bool v) ? v : fallback;
        public string GetString(string key, string fallback = null) => TryGet(key, out string v) ? v : fallback;

        public NeoMapReader GetMap(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return new NeoMapReader(NeoNative.NsMapGetMap(_handle, b + o, n)); }
            finally { NeoUtf8.Pop(frame); }
        }

        public NeoListReader GetList(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return new NeoListReader(NeoNative.NsMapGetList(_handle, b + o, n)); }
            finally { NeoUtf8.Pop(frame); }
        }

        public NeoArrayView GetArray(string key)
        {
            var frame = NeoUtf8.Push();
            try { int n = NeoUtf8.Encode(key, out int o); fixed (byte* b = NeoUtf8.Buffer) return new NeoArrayView(NeoNative.NsMapGetArray(_handle, b + o, n)); }
            finally { NeoUtf8.Pop(frame); }
        }
    }

    /// <summary>list 를 읽는다. 발급 스코프 밖에서 쓰면 안 된다.</summary>
    public readonly unsafe struct NeoListReader
    {
        private readonly IntPtr _handle;
        internal NeoListReader(IntPtr handle) { _handle = handle; }
        public bool IsValid => _handle != IntPtr.Zero;

        public int Count => NeoNative.NsListRdCount(_handle);
        public NeoValueType TypeOf(int index) => (NeoValueType)NeoNative.NsListRdType(_handle, index);

        public bool TryGet(int index, out int value) => NeoNative.NsListRdGetInt(_handle, index, out value) != 0;
        public bool TryGet(int index, out float value) => NeoNative.NsListRdGetFloat(_handle, index, out value) != 0;

        public bool TryGet(int index, out bool value)
        {
            if (NeoNative.NsListRdGetBool(_handle, index, out int raw) == 0) { value = false; return false; }
            value = raw != 0;
            return true;
        }

        public bool TryGet(int index, out string value)
        {
            if (NeoNative.NsListRdGetString(_handle, index, out NsStr text) == 0) { value = null; return false; }
            value = NeoUtf8.ToString(text);
            return true;
        }

        public bool TryGet(int index, out Vector3 value)
        {
            float* v = stackalloc float[4];
            if (NeoNative.NsListRdGetVec(_handle, index, v) == 0) { value = default; return false; }
            value = new Vector3(v[0], v[1], v[2]);
            return true;
        }

        public int GetInt(int index, int fallback = 0) => TryGet(index, out int v) ? v : fallback;
        public float GetFloat(int index, float fallback = 0f) => TryGet(index, out float v) ? v : fallback;
        public string GetString(int index, string fallback = null) => TryGet(index, out string v) ? v : fallback;

        public NeoMapReader GetMap(int index) => new NeoMapReader(NeoNative.NsListRdGetMap(_handle, index));
        public NeoListReader GetList(int index) => new NeoListReader(NeoNative.NsListRdGetList(_handle, index));
        public NeoArrayView GetArray(int index) => new NeoArrayView(NeoNative.NsListRdGetArray(_handle, index));
    }

    /// <summary>
    /// system.array 저장소를 복사 없이 들여다본다. 포인터는 발급 스코프 안에서만 유효하고,
    /// 스크립트가 배열 크기를 바꾸면 재할당될 수 있으니 재진입 뒤에 재사용하면 안 된다.
    /// </summary>
    public readonly unsafe struct NeoArrayView
    {
        private readonly IntPtr _handle;
        internal NeoArrayView(IntPtr handle) { _handle = handle; }
        public bool IsValid => _handle != IntPtr.Zero;

        public NeoArrayElementType ElementType => (NeoArrayElementType)NeoNative.NsArrayElementType(_handle);
        public int Count => NeoNative.NsArrayCount(_handle);

        /// <summary>int 원소. ElementType 이 Int 가 아니면 비어 있다.</summary>
        public Span<int> Ints
        {
            get
            {
                int* p = NeoNative.NsArrayInts(_handle);
                return p == null ? Span<int>.Empty : new Span<int>(p, Count);
            }
        }

        /// <summary>float 원소. ElementType 이 Float 가 아니면 비어 있다.</summary>
        public Span<float> Floats
        {
            get
            {
                float* p = NeoNative.NsArrayFloats(_handle);
                return p == null ? Span<float>.Empty : new Span<float>(p, Count);
            }
        }

        /// <summary>bool 원소는 비트로 채워져 있다. 하위 비트가 먼저다.</summary>
        public bool GetBool(int index)
        {
            byte* bits = NeoNative.NsArrayBoolBits(_handle);
            if (bits == null || index < 0 || index >= Count)
                return false;
            return ((bits[index >> 3] >> (index & 7)) & 1) != 0;
        }
    }
}
