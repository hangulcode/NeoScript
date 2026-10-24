#pragma once

#include <climits>

namespace NeoScript
{
// Avoid undefined float-to-int conversions while retaining truncation toward zero.
template <typename Float>
inline int NeoFloatToInt(Float value)
{
	if (value != value) return 0;
	if (value >= static_cast<Float>(INT_MAX)) return INT_MAX;
	if (value <= static_cast<Float>(INT_MIN)) return INT_MIN;
	return static_cast<int>(value);
}
}
