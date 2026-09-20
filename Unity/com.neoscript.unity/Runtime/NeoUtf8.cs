// NeoUtf8.cs — 경계를 넘는 문자열 처리.
//
// VM 은 UTF-8, C# 은 UTF-16 이다. 매 호출마다 변환 버퍼를 새로 잡으면 프레임마다 GC 가
// 쌓인다 — 게임에서 이건 그냥 못 쓰는 물건이 된다. 그래서 스레드별 범프 버퍼를 하나 두고
// 프레임 단위로 되감는다.
//
// 사용 규칙이 하나 있다: Encode 는 버퍼를 키울 수 있으므로 **fixed 로 고정하기 전에**
// 필요한 문자열을 전부 인코딩해야 한다. 고정한 뒤 인코딩하면 재할당된 다른 배열에 쓰게 된다.

using System;
using System.Runtime.CompilerServices;
using System.Text;

namespace NeoScript
{
    /// <summary>NeoScript 네이티브 경계에서 발생한 오류.</summary>
    public class NeoException : Exception
    {
        public int Code { get; }

        public NeoException(string message) : base(message) { }
        public NeoException(string message, int code) : base(message) { Code = code; }
    }

    /// <summary>
    /// 네이티브가 넘겨준 UTF-8 이름(메서드명·프로퍼티명). 문자열로 만들지 않고 비교할 수 있다.
    /// 디스패처는 호출마다 불리므로 여기서 string 을 만들면 그게 바로 할당 폭풍이 된다.
    /// 유효 범위는 디스패처 호출 안이다.
    /// </summary>
    public readonly unsafe struct NeoName
    {
        private readonly byte* _data;
        private readonly int _length;

        internal NeoName(byte* data, int length)
        {
            _data = data;
            _length = length;
        }

        public int Length => _length;

        /// <summary>이름이 <paramref name="value"/> 와 같은지. 할당하지 않는다.</summary>
        public bool Is(string value)
        {
            if (value == null || _data == null)
                return false;

            // 메서드 이름은 사실상 전부 ASCII 다. 그 경우 바이트와 char 가 1:1 이라 바로 비교된다.
            if (value.Length != _length)
                return NonAsciiEquals(value);

            for (int i = 0; i < value.Length; ++i)
            {
                char c = value[i];
                if (c > 0x7F)
                    return NonAsciiEquals(value);
                if ((byte)c != _data[i])
                    return false;
            }
            return true;
        }

        private bool NonAsciiEquals(string value)
        {
            int byteCount = Encoding.UTF8.GetByteCount(value);
            if (byteCount != _length)
                return false;

            NeoUtf8.Frame frame = NeoUtf8.Push();
            try
            {
                int length = NeoUtf8.Encode(value, out int offset);
                if (length != _length)
                    return false;
                byte[] buffer = NeoUtf8.Buffer;
                for (int i = 0; i < length; ++i)
                {
                    if (buffer[offset + i] != _data[i])
                        return false;
                }
                return true;
            }
            finally
            {
                NeoUtf8.Pop(frame);
            }
        }

        public override string ToString() => NeoUtf8.ToString((IntPtr)_data, _length);
    }

    /// <summary>스레드별 UTF-8 범프 버퍼. 상세는 파일 상단 주석 참고.</summary>
    internal static class NeoUtf8
    {
        [ThreadStatic] private static byte[] _buffer;
        [ThreadStatic] private static int _used;

        internal struct Frame
        {
            internal int Mark;
        }

        internal static byte[] Buffer => _buffer;

        internal static Frame Push()
        {
            if (_buffer == null)
                _buffer = new byte[1024];
            return new Frame { Mark = _used };
        }

        internal static void Pop(Frame frame)
        {
            _used = frame.Mark;
        }

        /// <summary>
        /// 문자열을 버퍼에 UTF-8 로 써넣고 바이트 길이를 돌려준다. <paramref name="offset"/> 은
        /// 버퍼 안의 시작 위치다. null/빈 문자열은 길이 0.
        /// 주의: 버퍼가 커질 수 있으니 fixed 로 고정하기 전에 전부 인코딩할 것.
        /// </summary>
        internal static int Encode(string value, out int offset)
        {
            if (_buffer == null)
                _buffer = new byte[1024];

            offset = _used;
            if (string.IsNullOrEmpty(value))
                return 0;

            int maxBytes = Encoding.UTF8.GetMaxByteCount(value.Length);
            Grow(_used + maxBytes);
            offset = _used;

            int written = Encoding.UTF8.GetBytes(value, 0, value.Length, _buffer, _used);
            _used += written;
            return written;
        }

        private static void Grow(int required)
        {
            if (_buffer.Length >= required)
                return;
            int size = _buffer.Length;
            while (size < required)
                size *= 2;
            Array.Resize(ref _buffer, size);
        }

        internal static unsafe string ToString(IntPtr data, int length)
        {
            if (data == IntPtr.Zero || length <= 0)
                return string.Empty;
            return Encoding.UTF8.GetString((byte*)data, length);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        internal static string ToString(in NsStr value) => ToString(value.data, value.len);

        internal static unsafe byte[] ToBytes(in NsStr value)
        {
            if (value.data == IntPtr.Zero || value.len <= 0)
                return Array.Empty<byte>();
            byte[] result = new byte[value.len];
            System.Runtime.InteropServices.Marshal.Copy(value.data, result, 0, value.len);
            return result;
        }
    }
}
