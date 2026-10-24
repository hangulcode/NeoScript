#pragma once

#include <climits>
#include <cstddef>
#include <stdexcept>

namespace NeoScript
{
inline void CheckRuntimeStringSize(size_t size)
{
	if (size > static_cast<size_t>(INT_MAX))
		throw std::length_error("string byte length exceeds limit");
}

inline void CheckRuntimeStringGrowth(size_t current, size_t additional)
{
	CheckRuntimeStringSize(current);
	if (additional > static_cast<size_t>(INT_MAX) - current)
		throw std::length_error("string byte length exceeds limit");
}
}
