#pragma once

#include <cstring>
#include <cmath>

namespace NeoScript
{


typedef unsigned char	u8;
typedef char			s8;
typedef unsigned short	u16;
typedef short			s16;
typedef unsigned int	u32;
typedef int				s32;

// 반드시 float 로 설정
#define NS_SINGLE_PRECISION
typedef float		NS_FLOAT;

#pragma pack(1)
struct debug_info
{
	union
	{
		u32			_data;
		struct
		{
			u16		_fileseq;
			u16		_lineseq;
		};
	};

	debug_info()
	{
		_fileseq = 0;
		_lineseq = 0;
	}
	debug_info(u16 file, u16 line)
	{
		_fileseq = file;
		_lineseq = line;
	}
};
struct SUtf8One
{
	// UTF-8 최대 시퀀스가 4바이트라 4로 고정한다. 절대 늘리지 말 것 —
	// 이 구조체는 VarInfo 익명 유니온의 멤버라(NeoVM.h), 5바이트로 넓히면
	// Win32 에서 유니온이 4->8 로 커지며 VarInfo 가 8->12 바이트가 되고
	// NeoVM.h 의 static_assert(sizeof(VarInfo)==8) 가 깨져 32비트 빌드가 전부 실패한다.
	// NUL 종료가 필요하면 쓰는 쪽에서 지역 버퍼로 처리한다.
	char c[4];
};
#pragma pack()



#define FILE_NEOS	(('N' << 24) | ('E' << 16) | ('O' << 8) | ('S'))
// 0110: switch/case 테이블 chunk 추가 (이전 캐시 이미지와 호환 안 됨)
// 0111: _L(전 오퍼랜드 로컬) opcode 를 각 원본 뒤에 삽입 → 이후 op 번호가 전부 밀림.
//       이미지 자체는 원본 op 만 담고 _L 치환은 로드 후(PatchLocalOps) 이뤄지지만,
//       0110 이미지를 그대로 읽으면 op 번호가 어긋나므로 버전을 올린다.
// 0112: 벡터 타입 통합 — VAR_VEC2/3/4/QUAT -> VAR_VEC(+성분수), VEC*_MAKE 4개 -> VEC_MAKE.
//       VAR_TYPE / opcode 번호가 모두 밀리므로 이전 이미지와 호환되지 않는다.
#define NEO_VER		(('0' << 24) | ('1' << 16) | ('1' << 8) | ('2'))

#if defined(_MSC_VER) && !defined(_DEBUG)
#define NEOS_FORCEINLINE __forceinline
#elif defined(__GNUC__) && __GNUC__ >= 4 && defined(NDEBUG)
// always_inline 만으로는 inline 이 아니다. 헤더에 정의가 들어가면 TU 마다 실체가 생겨
// 중복 정의로 링크가 깨지므로 inline 을 반드시 함께 붙인다.
#define NEOS_FORCEINLINE inline __attribute__((always_inline))
#else
#define NEOS_FORCEINLINE inline
#endif

// 콜드 핸들러 out-of-line 강제용. RunInternal 이 L1I(32KB) 를 넘지 않게 덩치 큰/드문 op 를 분리.
#if defined(_MSC_VER)
#define NEOS_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define NEOS_NOINLINE __attribute__((noinline))
#else
#define NEOS_NOINLINE
#endif

};

#include <vector>
#include <list>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <atomic>
