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

// NOP_MOVF 는 SVMOperation 의 n23(정확히 4 byte)에 float 비트를 직접 담는다.
// NS_FLOAT 를 double 로 바꾸면 이 값도 컴파일 타임에 false 가 되어, 컴파일러는
// 기존 static 상수 + NOP_MOV 경로만 사용한다.
static constexpr bool NEOS_CAN_EMBED_FLOAT_IMMEDIATE = (sizeof(NS_FLOAT) == 4 && sizeof(int) == 4);

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
#pragma pack()



#define FILE_NEOS	(('N' << 24) | ('E' << 16) | ('O' << 8) | ('S'))
// 0110: switch/case 테이블 chunk 추가 (이전 캐시 이미지와 호환 안 됨)
// 0111: _L(전 오퍼랜드 로컬) opcode 를 각 원본 뒤에 삽입 → 이후 op 번호가 전부 밀림.
//       이미지 자체는 원본 op 만 담고 _L 치환은 로드 후(PatchLocalOps) 이뤄지지만,
//       0110 이미지를 그대로 읽으면 op 번호가 어긋나므로 버전을 올린다.
// 0112: 벡터 타입 통합 — VAR_VEC2/3/4/QUAT -> VAR_VEC(+성분수), VEC*_MAKE 4개 -> VEC_MAKE.
//       VAR_TYPE / opcode 번호가 모두 밀리므로 이전 이미지와 호환되지 않는다.
// 0113: NOP_MOVI_L 뒤에 NOP_MOVF/_L 삽입. 이후 opcode 번호가 바뀌므로 캐시를 재생성한다.
// 0114: 인라인 문자 타입을 제거하고 foreach 문자열 원소도 VAR_STRING 으로 통합.
//       VAR_TYPE 값이 바뀌므로 이전 캐시 이미지는 읽을 수 없다.
// 0115: 함수 테이블에 익명 함수의 지역 VarInfo 캡처 슬롯을 추가.
// 0116: 다단계 익명 함수 캡처가 중간 부모 프레임의 숨은 슬롯을 경유하도록 컴파일된다.
//       0115 이미지에는 그 중간 슬롯이 없으므로 캐시를 다시 만들어야 한다.
// 0118: RET_CLOSURE의 _L 변형을 제거한다. 0117 캐시는 서로 다른 opcode 번호를 쓰므로
//       다시 만들어야 한다.
// 0119: RET_CLOSURE opcode를 제거하고 closure 반환 여부를 SCallStack에 기록한다.
//       이후 opcode 번호가 바뀌므로 캐시를 다시 만들어야 한다.
// 0120: 범위 조건 jump opcode와 'RNGE' 범위 descriptor chunk를 추가한다.
//       이전 캐시는 새 opcode/chunk를 모르므로 다시 만들어야 한다.
// 0121: 고정 길이 원시 배열(VAR_ARRAY)을 추가한다. 이전 캐시는 enum 번호가 달라 다시 만든다.
#define NEO_VER		(('0' << 24) | ('1' << 16) | ('2' << 8) | ('1'))

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
