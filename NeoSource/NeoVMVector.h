#pragma once

#include "NeoVMMap.h"

namespace NeoScript
{
// 4성분 벡터 전용 공유 저장소. 2·3성분 벡터는 VarInfo 안에 직접 저장한다.
// Quaternion 은 엔진 컨벤션대로 v[0..3] = w,x,y,z 순서.
//
// refcount 로 공유하되 값 의미론을 유지한다 — 대입은 공유(incref)만 하고, 성분을 쓰는
// 순간(v[i] = x) 공유 중이면 복제한다(copy-on-write). 그래서 `var b = a;` 뒤에 b 를
// 고쳐도 a 는 그대로다.
struct VecInfo : AllocBase
{
	float v[4];
};

NEOS_FORCEINLINE float VarInfo::VectorComponent(int index) const
{
	if (GetType() == VAR_VEC)
	{
		// 별도 float 멤버 사이의 포인터 산술 대신 객체 안에서 성분 주소를 계산한다.
		const unsigned char* bytes = reinterpret_cast<const unsigned char*>(this);
		return *reinterpret_cast<const float*>(bytes + 4 + sizeof(float) * index);
	}
	return _vec->v[index];
}
NEOS_FORCEINLINE void VarInfo::CopyVectorComponents(float* out) const
{
	// 호출자가 요청한 유효 성분만 쓴다. 남는 출력 레인은 보존한다.
	if (GetType() == VAR_VEC)
	{
		// 별도 멤버를 float 배열로 색인하지 않고 전체 객체의 바이트 표현에서 복사한다.
		const unsigned char* bytes = reinterpret_cast<const unsigned char*>(this);
		std::memcpy(out, bytes + sizeof(_type) + sizeof(_vecCount) + sizeof(_unused),
			sizeof(float) * _vecCount);
	}
	else
		std::memmove(out, _vec->v, sizeof(float) * 4);
}

} // namespace NeoScript
