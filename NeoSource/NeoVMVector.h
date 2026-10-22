#pragma once

#include "NeoVMMap.h"

namespace NeoScript
{
// 벡터 성분 저장소. 유효 성분 수는 VarInfo::_vecCount 가 결정한다.
// Quaternion 은 엔진 컨벤션대로 v[0..3] = w,x,y,z 순서.
//
// refcount 로 공유하되 값 의미론을 유지한다 — 대입은 공유(incref)만 하고, 성분을 쓰는
// 순간(v[i] = x) 공유 중이면 복제한다(copy-on-write). 그래서 `var b = a;` 뒤에 b 를
// 고쳐도 a 는 그대로다.
struct VecInfo : AllocBase
{
	float v[4];
};

} // namespace NeoScript
