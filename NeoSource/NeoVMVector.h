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
	return GetType() == VAR_VEC ? (index == 2 ? _vecZ : _vecXY[index]) : _vec->v[index];
}
NEOS_FORCEINLINE void VarInfo::CopyVectorComponents(float* out) const
{
	// 호출자가 요청한 유효 성분만 쓴다. 남는 출력 레인은 보존한다.
	if (GetType() == VAR_VEC)
	{
		out[0] = _vecXY[0]; out[1] = _vecXY[1];
		if (_vecCount == 3) out[2] = _vecZ;
	}
	else
		std::memmove(out, _vec->v, sizeof(float) * 4);
}

} // namespace NeoScript
