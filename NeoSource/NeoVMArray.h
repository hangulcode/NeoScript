#pragma once

#include <cstddef>

namespace NeoScript
{

// Fixed-size storage for exactly one of bool/int/float.
// The raw pool recycles only this header, so Allocate/Free always own the
// lifetime of the separately allocated payload.
struct ArrayInfo : AllocBase
{
	void* _data = nullptr;
	int _count = 0;
	NeoArrayElementType _elementType = NeoArrayElementType::Bool;

	// Lets VM shutdown release payloads still referenced by host/script values.
	ArrayInfo* _liveNext = nullptr;
	ArrayInfo* _livePrev = nullptr;

	NEOS_FORCEINLINE bool IsValidIndex(int index) const
	{
		return (unsigned)index < (unsigned)_count;
	}

	NEOS_FORCEINLINE size_t DataBytes() const
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:  return ((size_t)_count + 7) >> 3;
		case NeoArrayElementType::Int:   return (size_t)_count * sizeof(int);
		case NeoArrayElementType::Float: return (size_t)_count * sizeof(NS_FLOAT);
		}
		return 0;
	}

	NEOS_FORCEINLINE u8* BoolBits() { return static_cast<u8*>(_data); }
	NEOS_FORCEINLINE const u8* BoolBits() const { return static_cast<const u8*>(_data); }
	NEOS_FORCEINLINE int* IntData() { return static_cast<int*>(_data); }
	NEOS_FORCEINLINE const int* IntData() const { return static_cast<const int*>(_data); }
	NEOS_FORCEINLINE NS_FLOAT* FloatData() { return static_cast<NS_FLOAT*>(_data); }
	NEOS_FORCEINLINE const NS_FLOAT* FloatData() const { return static_cast<const NS_FLOAT*>(_data); }

	NEOS_FORCEINLINE bool GetBool(int index) const
	{
		return (BoolBits()[index >> 3] & (u8)(1u << (index & 7))) != 0;
	}
	NEOS_FORCEINLINE void SetBool(int index, bool value)
	{
		u8& byte = BoolBits()[index >> 3];
		const u8 bit = (u8)(1u << (index & 7));
		if (value)
			byte |= bit;
		else
			byte &= (u8)~bit;
	}

	// CNeoVM::ArrayAlloc only. It must initialize every element before publishing
	// this ArrayInfo through the live list or a script value.
	bool Allocate(NeoArrayElementType elementType, int count)
	{
		_data = nullptr;
		_count = count;
		_elementType = elementType;
		if (count == 0)
			return true;

		switch (elementType)
		{
		case NeoArrayElementType::Bool:
			_data = new u8[((size_t)count + 7) >> 3];
			return true;
		case NeoArrayElementType::Int:
			_data = new int[count];
			return true;
		case NeoArrayElementType::Float:
			_data = new NS_FLOAT[count];
			return true;
		}
		return false;
	}

	void Free()
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:  delete[] BoolBits(); break;
		case NeoArrayElementType::Int:   delete[] IntData(); break;
		case NeoArrayElementType::Float: delete[] FloatData(); break;
		}
		_data = nullptr;
		_count = 0;
	}
};

inline const char* GetArrayElementTypeName(NeoArrayElementType type)
{
	switch (type)
	{
	case NeoArrayElementType::Bool:  return "bool";
	case NeoArrayElementType::Int:   return "int";
	case NeoArrayElementType::Float: return "float";
	}
	return "unknown";
}

} // namespace NeoScript
