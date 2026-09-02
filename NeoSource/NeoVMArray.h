#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace NeoScript
{

// Dense storage for exactly one of bool/int/float.
// The raw pool recycles only this header, so Allocate/Free always own the
// lifetime of the separately allocated payload.
struct ArrayInfo : AllocBase
{
	void* _data = nullptr;
	int _count = 0;
	int _capacity = 0;
	NeoArrayElementType _elementType = NeoArrayElementType::Bool;
	u32 _mutationVersion = 0;
	union
	{
		bool _initialBool;
		int _initialInt;
		NS_FLOAT _initialFloat;
	};

	// Lets VM shutdown release payloads still referenced by host/script values.
	ArrayInfo* _liveNext = nullptr;
	ArrayInfo* _livePrev = nullptr;

	NEOS_FORCEINLINE bool IsValidIndex(int index) const
	{
		return (unsigned)index < (unsigned)_count;
	}

	NEOS_FORCEINLINE size_t BufferBytes() const
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:  return ((size_t)_capacity + 7) >> 3;
		case NeoArrayElementType::Int:   return (size_t)_capacity * sizeof(int);
		case NeoArrayElementType::Float: return (size_t)_capacity * sizeof(NS_FLOAT);
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

	NEOS_FORCEINLINE bool AssignValue(int index, VarInfo* value)
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:
			if (value->GetType() != VAR_BOOL)
				return false;
			SetBool(index, value->_bl);
			return true;
		case NeoArrayElementType::Int:
			if (value->GetType() == VAR_INT)
			{
				IntData()[index] = value->_int;
				return true;
			}
			if (value->GetType() == VAR_FLOAT)
			{
				IntData()[index] = (int)value->_float;
				return true;
			}
			return false;
		case NeoArrayElementType::Float:
			if (value->GetType() == VAR_INT)
			{
				FloatData()[index] = (NS_FLOAT)value->_int;
				return true;
			}
			if (value->GetType() == VAR_FLOAT)
			{
				FloatData()[index] = value->_float;
				return true;
			}
			return false;
		}
		return false;
	}

	NEOS_FORCEINLINE bool CanAssignValue(VarInfo* value) const
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:
			return value->GetType() == VAR_BOOL;
		case NeoArrayElementType::Int:
		case NeoArrayElementType::Float:
			return value->GetType() == VAR_INT || value->GetType() == VAR_FLOAT;
		}
		return false;
	}

	void SetInitialValue(const VarInfo& value)
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:  _initialBool = value._bl; break;
		case NeoArrayElementType::Int:   _initialInt = value._int; break;
		case NeoArrayElementType::Float: _initialFloat = value._float; break;
		}
	}

	void Reserve(int capacity)
	{
		if (capacity <= _capacity)
			return;

		switch (_elementType)
		{
		case NeoArrayElementType::Bool:
		{
			const size_t replacementBytes = ((size_t)capacity + 7) >> 3;
			u8* replacement = new u8[replacementBytes];
			memset(replacement, 0, replacementBytes);
			const size_t oldBytes = BufferBytes();
			if (oldBytes > 0)
				memcpy(replacement, BoolBits(), oldBytes);
			delete[] BoolBits();
			_data = replacement;
			break;
		}
		case NeoArrayElementType::Int:
		{
			int* replacement = new int[capacity];
			if (_count > 0)
				memcpy(replacement, IntData(), sizeof(int) * (size_t)_count);
			delete[] IntData();
			_data = replacement;
			break;
		}
		case NeoArrayElementType::Float:
		{
			NS_FLOAT* replacement = new NS_FLOAT[capacity];
			if (_count > 0)
				memcpy(replacement, FloatData(), sizeof(NS_FLOAT) * (size_t)_count);
			delete[] FloatData();
			_data = replacement;
			break;
		}
		}
		_capacity = capacity;
	}

	bool Resize(int count)
	{
		if (count < 0)
			count = 0;
		if (count == _count)
			return false;

		const int oldCount = _count;
		if (count > _capacity)
		{
			const int64_t grownCapacity = _capacity == 0 ? 4 : (int64_t)_capacity + _capacity / 2;
			const int capacity = grownCapacity > count && grownCapacity <= INT32_MAX ? (int)grownCapacity : count;
			Reserve(capacity);
		}
		if (count > oldCount)
			SetInitialRange(oldCount, count);
		_count = count;
		++_mutationVersion;
		return true;
	}

	// CNeoVM::ArrayAlloc only. It must initialize every element before publishing
	// this ArrayInfo through the live list or a script value.
	bool Allocate(NeoArrayElementType elementType, int count)
	{
		_data = nullptr;
		_count = count;
		_capacity = count;
		_elementType = elementType;
		_mutationVersion = 0;
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
		_capacity = 0;
		_mutationVersion = 0;
	}

private:
	void SetInitialRange(int begin, int end)
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:
			while (begin < end && (begin & 7) != 0)
				SetBool(begin++, _initialBool);
			if (begin < end)
			{
				const int fullByteEnd = end & ~7;
				if (begin < fullByteEnd)
					memset(BoolBits() + (begin >> 3), _initialBool ? 0xFF : 0x00, (size_t)(fullByteEnd - begin) >> 3);
				begin = fullByteEnd;
			}
			while (begin < end)
				SetBool(begin++, _initialBool);
			break;
		case NeoArrayElementType::Int:
			for (int i = begin; i < end; ++i)
				IntData()[i] = _initialInt;
			break;
		case NeoArrayElementType::Float:
			for (int i = begin; i < end; ++i)
				FloatData()[i] = _initialFloat;
			break;
		}
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
