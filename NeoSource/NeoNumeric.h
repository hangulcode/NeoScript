#pragma once

#include <climits>
#include "NeoConfig.h"
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <emmintrin.h>
#endif

namespace NeoScript
{
// 정수 산술은 하위 64비트를 보존하며 부호 있는 C++ 오버플로를 일으키지 않는다.
inline NS_INT NeoIntFromBits(NS_UINT value) { NS_INT result; std::memcpy(&result, &value, sizeof(result)); return result; }
inline NS_INT NeoIntAdd(NS_INT a, NS_INT b) { return NeoIntFromBits(NS_UINT(a) + NS_UINT(b)); }
inline NS_INT NeoIntSub(NS_INT a, NS_INT b) { return NeoIntFromBits(NS_UINT(a) - NS_UINT(b)); }
inline NS_INT NeoIntMul(NS_INT a, NS_INT b) { return NeoIntFromBits(NS_UINT(a) * NS_UINT(b)); }
inline NS_INT NeoIntNeg(NS_INT a) { return NeoIntFromBits(NS_UINT(0) - NS_UINT(a)); }

inline int NeoNarrowInt(NS_INT value)
{
	return value < INT_MIN ? INT_MIN : value > INT_MAX ? INT_MAX : static_cast<int>(value);
}

// 범위 안은 0 방향으로 버리고, NaN과 범위 초과는 정수로 캐스팅하기 전에 처리한다.
template <typename Float>
inline NS_INT NeoFloatToInt(Float value)
{
	if (value >= static_cast<Float>(NS_INT_MIN) && value < static_cast<Float>(9223372036854775808.0))
		return static_cast<NS_INT>(value);
	if (value != value) return 0;
	return value > static_cast<Float>(0) ? NS_INT_MAX : NS_INT_MIN;
}
#if defined(_M_X64) || defined(__x86_64__)
// x64 변환의 최소 정수 결과만 따로 분류하여 NaN과 범위 초과를 처리한다.
inline NS_INT NeoFloatToInt(float value)
{
	const NS_INT r = _mm_cvttss_si64(_mm_set_ss(value));
	if (r != NS_INT_MIN) return r;
	if (value != value) return 0;
	return value > 0.0f ? NS_INT_MAX : NS_INT_MIN;
}
inline NS_INT NeoFloatToInt(double value)
{
	const NS_INT r = _mm_cvttsd_si64(_mm_set_sd(value));
	if (r != NS_INT_MIN) return r;
	if (value != value) return 0;
	return value > 0.0 ? NS_INT_MAX : NS_INT_MIN;
}
#endif
}
