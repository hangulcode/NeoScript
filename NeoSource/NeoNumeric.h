#pragma once

#include <climits>
#include <limits>
#include "NeoConfig.h"
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <emmintrin.h>
#endif

namespace NeoScript
{
// 배열·벡터·호스트 변환은 같은 float32 경계를 사용한다. 범위 밖 값은 캐스팅 전에 처리한다.
// float32 범위 밖(|v| > FLT_MAX)만 다루는 드문 경로. 호출 지점이 벡터 산술·성분 쓰기 같은 디스패치 루프의
// 핫 경로라 본문을 밖으로 빼고, 인라인에는 비교 하나만 남긴다.
NEOS_NOINLINE inline float NeoNarrowFloat32Rare(NS_FLOAT value)
{
	const float limit = (std::numeric_limits<float>::max)();
	// 최근접 반올림의 오버플로 경계는 2^128 - 2^103이며, 경계 자체는 무한대가 된다.
	const NS_FLOAT overflow = 3.40282356779733661637539395458142568448e38;
	if (value > limit) return value < overflow ? limit : std::numeric_limits<float>::infinity();
	if (value < -limit) return value > -overflow ? -limit : -std::numeric_limits<float>::infinity();
	return static_cast<float>(value);
}
NEOS_FORCEINLINE float NeoNarrowFloat32(NS_FLOAT value)
{
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
	// cvtsd2ss 는 IEEE 최근접 반올림이라 FLT_MAX 와 2^128-2^103 사이는 FLT_MAX 로, 그 이상은 ∞ 로 만들고
	// NaN 과 부호 있는 0 을 보존한다 — 위 Rare 와 결과가 같고 ISA 가 정의한 동작이므로 비교 없이 한 명령이다.
	return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(static_cast<double>(value))));
#else
	// 범위 안과 NaN 은 캐스트 결과가 그대로이므로(NaN 은 비교가 거짓) 비교 한 번으로 끝낸다.
	const NS_FLOAT limit = (std::numeric_limits<float>::max)();
	if (value > limit || value < -limit) return NeoNarrowFloat32Rare(value);
	return static_cast<float>(value);
#endif
}

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
