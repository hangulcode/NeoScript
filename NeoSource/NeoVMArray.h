#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include "NeoVMVector.h"
#include "NeoNumeric.h"

namespace NeoScript
{

// Typed storage for bool/int/float/Vector2/3/4. 벡터 원소는 VecInfo 를 공유하지 않고 성분 float 를
// 원소당 4개(16 B) 고정 stride 로 저장한다: 읽기는 dest 의 저장소로, 쓰기는 버퍼로 16 B 를 한 번에
// 옮기므로 성분 수에 따른 분기·가변 길이 복사가 없다. 원소가 스크립트 값과 별칭이 되지 않으며 배열은
// refcount 를 갖지 않는다. 배열은 순환에 참여할 수 없다.
// The raw pool recycles only this header, so Allocate/Free always own the
// lifetime of the separately allocated payload.
struct ArrayInfo : AllocBase
{
	CNeoVM* _pVM = nullptr;
	void* _data = nullptr;
	int _count = 0;
	int _capacity = 0;
	NeoArrayElementType _elementType = NeoArrayElementType::Bool;
	u32 _mutationVersion = 0;
	union
	{
		bool _initialBool;
		NS_INT _initialInt;
		NS_FLOAT _initialFloat;
	};

	float _initialVec[4];   // 벡터 원소 타입의 생성 값(성분). resize 로 늘어난 원소를 채운다

	// Lets VM shutdown release payloads still referenced by host/script values.
	ArrayInfo* _liveNext = nullptr;
	ArrayInfo* _livePrev = nullptr;

	NEOS_FORCEINLINE bool IsValidIndex(NS_INT index) const
	{
		return (NS_UINT)index < (NS_UINT)_count;
	}

	enum { kVectorStride = 4 };   // 벡터 원소 하나가 차지하는 float 수(성분 수와 무관)

	NEOS_FORCEINLINE int VectorDimension() const
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Vector2: return 2;
		case NeoArrayElementType::Vector3: return 3;
		case NeoArrayElementType::Vector4: return 4;
		default: return 0;
		}
	}

	NEOS_FORCEINLINE size_t BufferBytes() const
	{
		switch (_elementType)
		{
		case NeoArrayElementType::Bool:  return ((size_t)_capacity + 7) >> 3;
		case NeoArrayElementType::Int:   return (size_t)_capacity * sizeof(NS_INT);
		case NeoArrayElementType::Float: return (size_t)_capacity * sizeof(NS_FLOAT);
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4: return (size_t)_capacity * kVectorStride * sizeof(float);
		case NeoArrayElementType::Int32: return (size_t)_capacity * sizeof(int32_t);
		case NeoArrayElementType::Float32: return (size_t)_capacity * sizeof(float);
		}
		return 0;
	}

	NEOS_FORCEINLINE u8* BoolBits() { return static_cast<u8*>(_data); }
	NEOS_FORCEINLINE const u8* BoolBits() const { return static_cast<const u8*>(_data); }
	NEOS_FORCEINLINE NS_INT* IntData() { return static_cast<NS_INT*>(_data); }
	NEOS_FORCEINLINE const NS_INT* IntData() const { return static_cast<const NS_INT*>(_data); }
	NEOS_FORCEINLINE NS_FLOAT* FloatData() { return static_cast<NS_FLOAT*>(_data); }
	NEOS_FORCEINLINE const NS_FLOAT* FloatData() const { return static_cast<const NS_FLOAT*>(_data); }

	// 32비트 배열도 읽을 때는 일반 int64/double 값으로 확장한다.
	NEOS_FORCEINLINE int32_t* Int32Data() { return static_cast<int32_t*>(_data); }
	NEOS_FORCEINLINE const int32_t* Int32Data() const { return static_cast<const int32_t*>(_data); }
	NEOS_FORCEINLINE float* Float32Data() { return static_cast<float*>(_data); }
	NEOS_FORCEINLINE const float* Float32Data() const { return static_cast<const float*>(_data); }
	static float NarrowFloat32(NS_FLOAT value)
	{
		// 최근접 반올림의 오버플로 경계는 2^128 - 2^103이며, 경계 자체는 무한대가 된다.
		// FLT_MAX와 경계 사이에서는 범위 밖 캐스트 없이 FLT_MAX를 직접 반환한다.
		const float limit = (std::numeric_limits<float>::max)();
		const NS_FLOAT overflow = 3.40282356779733661637539395458142568448e38;
		if (value > limit) return value < overflow ? limit : std::numeric_limits<float>::infinity();
		if (value < -limit) return value > -overflow ? -limit : -std::numeric_limits<float>::infinity();
		return static_cast<float>(value);
	}
	bool Int32OutOfRange(const VarInfo* value) const
	{
		return _elementType == NeoArrayElementType::Int32 && value->GetType() == VAR_INT &&
			(value->_int < INT32_MIN || value->_int > INT32_MAX);
	}

	// 원소 i 의 성분 = VectorAt(i)[0..dim). dim 뒤의 패딩은 의미 없는 값이다(비교·출력은 dim 까지만 본다).
	NEOS_FORCEINLINE float* VectorData() { return static_cast<float*>(_data); }
	NEOS_FORCEINLINE const float* VectorData() const { return static_cast<const float*>(_data); }
	NEOS_FORCEINLINE float* VectorAt(int index) { return VectorData() + (size_t)index * kVectorStride; }
	NEOS_FORCEINLINE const float* VectorAt(int index) const { return VectorData() + (size_t)index * kVectorStride; }
	const float* InitialVector() const { return _initialVec; }
	void SetInitialVector(const float* components) { memcpy(_initialVec, components, sizeof(_initialVec)); }
	// 원소를 out 으로 복사한다. out 이 단독 소유 벡터면 그 저장소를 재사용하므로 루프 안의 반복 읽기는
	// 16 B 이동 하나다. 그 외(첫 읽기, 공유된 dest, 다른 타입)는 ReadVectorRare 가 해제·할당을 맡는다.
	NEOS_FORCEINLINE void ReadVector(int index, VarInfo* out)
	{
		if (out->IsVector() && out->_vec->_refCount <= 1)
		{
			memcpy(out->_vec->v, VectorAt(index), kVectorStride * sizeof(float));
			out->SetVecType(VectorDimension());
			return;
		}
		ReadVectorRare(index, out);
	}
	void ReadVectorRare(int index, VarInfo* out);
	bool SetVectorComponents(int index, const float* components, int dimension)
	{
		if (!components || !IsValidIndex(index) || !VectorDimension() || dimension != VectorDimension())
			return false;
		float copy[kVectorStride] = {};
		memcpy(copy, components, (size_t)dimension * sizeof(float));
		memcpy(VectorAt(index), copy, sizeof(copy));
		return true;
	}

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

	// 같은 타입의 int/float 저장만 인라인한다. 변환·bool·벡터 저장은 드물어 아웃라인으로 보낸다
	// (디스패치 루프의 인라인 크기가 곧 성능 — CltRead 의 배열 읽기와 같은 분리).
	NEOS_FORCEINLINE bool AssignValue(int index, VarInfo* value)
	{
		const VAR_TYPE vt = value->GetType();
		if (_elementType == NeoArrayElementType::Int && vt == VAR_INT) { IntData()[index] = value->_int; return true; }
		if (_elementType == NeoArrayElementType::Float && vt == VAR_FLOAT) { FloatData()[index] = value->_float; return true; }
		return AssignValueRare(index, value);
	}
	NEOS_NOINLINE bool AssignValueRare(int index, VarInfo* value)
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
				IntData()[index] = NeoFloatToInt(value->_float);
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
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4:
			if (!CanAssignValue(value)) return false;
			AssignVector(index, value);
			return true;
		case NeoArrayElementType::Int32:
			if (!CanAssignValue(value)) return false;
			Int32Data()[index] = static_cast<int32_t>(value->_int);
			return true;
		case NeoArrayElementType::Float32:
			if (!value->IsNumber()) return false;
			Float32Data()[index] = NarrowFloat32(value->GetFloatNumber());
			return true;
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
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4:
			return value->IsVector() && value->VectorComponentCount() == VectorDimension();
		case NeoArrayElementType::Int32:
			return value->GetType() == VAR_INT && !Int32OutOfRange(value);
		case NeoArrayElementType::Float32:
			return value->IsNumber();
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
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4:
		{
			// 복제(clone)는 VAR_NONE 으로 만들고 성분을 SetInitialVector 로 따로 넘기므로 벡터가 아니면 0 이다.
			VarInfo v = value;
			memset(_initialVec, 0, sizeof(_initialVec));
			if (v.IsVector())
				memcpy(_initialVec, v._vec->v, sizeof(_initialVec));
			break;
		}
		case NeoArrayElementType::Int32: _initialInt = value._int; break;
		case NeoArrayElementType::Float32: _initialFloat = NarrowFloat32(value._float); break;
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
			NS_INT* replacement = new NS_INT[capacity];
			if (_count > 0)
				memcpy(replacement, IntData(), sizeof(NS_INT) * (size_t)_count);
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
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4:
		{
			float* replacement = new float[(size_t)capacity * kVectorStride];
			if (_count > 0)
				memcpy(replacement, VectorData(), sizeof(float) * kVectorStride * (size_t)_count);
			delete[] VectorData();
			_data = replacement;
			break;
		}
		case NeoArrayElementType::Int32:
		{
			int32_t* replacement = new int32_t[capacity];
			if (_count > 0) memcpy(replacement, Int32Data(), sizeof(int32_t) * (size_t)_count);
			delete[] Int32Data();
			_data = replacement;
			break;
		}
		case NeoArrayElementType::Float32:
		{
			float* replacement = new float[capacity];
			if (_count > 0) memcpy(replacement, Float32Data(), sizeof(float) * (size_t)_count);
			delete[] Float32Data();
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
		memset(_initialVec, 0, sizeof(_initialVec));
		if (count == 0)
			return true;

		switch (elementType)
		{
		case NeoArrayElementType::Bool:
			_data = new u8[((size_t)count + 7) >> 3];
			return true;
		case NeoArrayElementType::Int:
			_data = new NS_INT[count];
			return true;
		case NeoArrayElementType::Float:
			_data = new NS_FLOAT[count];
			return true;
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4:
			_data = new float[(size_t)count * kVectorStride];
			return true;
		case NeoArrayElementType::Int32: _data = new int32_t[count]; return true;
		case NeoArrayElementType::Float32: _data = new float[count]; return true;
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
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4: delete[] VectorData(); break;
		case NeoArrayElementType::Int32: delete[] Int32Data(); break;
		case NeoArrayElementType::Float32: delete[] Float32Data(); break;
		}
		_data = nullptr;
		_count = 0;
		_capacity = 0;
		_mutationVersion = 0;
	}

private:
	friend class CNeoVM;
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
		case NeoArrayElementType::Vector2:
		case NeoArrayElementType::Vector3:
		case NeoArrayElementType::Vector4:
			for (int i = begin; i < end; ++i)
				memcpy(VectorAt(i), _initialVec, sizeof(_initialVec));
			break;
		case NeoArrayElementType::Int32:
			for (int i = begin; i < end; ++i) Int32Data()[i] = static_cast<int32_t>(_initialInt);
			break;
		case NeoArrayElementType::Float32:
			for (int i = begin; i < end; ++i) Float32Data()[i] = static_cast<float>(_initialFloat);
			break;
		}
	}
	// 값 벡터의 성분을 원소 자리에 16 B 로 복사한다. 공유가 생기지 않으므로 호출측 벡터는 단독 소유를 유지한다.
	NEOS_FORCEINLINE void AssignVector(int index, const VarInfo* value)
	{
		memcpy(VectorAt(index), value->_vec->v, kVectorStride * sizeof(float));
	}
};

inline const char* GetArrayElementTypeName(NeoArrayElementType type)
{
	switch (type)
	{
	case NeoArrayElementType::Bool:  return "bool";
	case NeoArrayElementType::Int:   return "int";
	case NeoArrayElementType::Float: return "float";
	case NeoArrayElementType::Vector2: return "Vector2";
	case NeoArrayElementType::Vector3: return "Vector3";
	case NeoArrayElementType::Vector4: return "Vector4";
	case NeoArrayElementType::Int32: return "int32";
	case NeoArrayElementType::Float32: return "float32";
	}
	return "unknown";
}

} // namespace NeoScript
