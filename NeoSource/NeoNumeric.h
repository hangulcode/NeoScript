#pragma once

#include <climits>
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

namespace NeoScript
{
// Avoid undefined float-to-int conversions while retaining truncation toward zero.
// 범위 안(절대다수)은 비교 두 번으로 끝낸다. NaN 은 두 비교가 모두 거짓이라 아래로 떨어진다.
template <typename Float>
inline int NeoFloatToInt(Float value)
{
	if (value >= static_cast<Float>(INT_MIN) && value < static_cast<Float>(2147483648.0))
		return static_cast<int>(value);
	if (value != value) return 0;
	return value > static_cast<Float>(0) ? INT_MAX : INT_MIN;
}
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
// x86: cvttss2si 는 NaN/범위 밖에서 0x80000000 을 돌려주므로 그 값일 때만 느린 분류를 거친다(비교 1회).
inline int NeoFloatToInt(float value)
{
	const int r = _mm_cvtt_ss2si(_mm_set_ss(value));
	if (r != INT_MIN) return r;
	if (value != value) return 0;
	return value > 0.0f ? INT_MAX : INT_MIN;
}
#endif
}
