#pragma once

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
	char c[4];
};
#pragma pack()



#define FILE_NEOS	(('N' << 24) | ('E' << 16) | ('O' << 8) | ('S'))
// 0110: switch/case 테이블 chunk 추가 (이전 캐시 이미지와 호환 안 됨)
// 0111: _L(전 오퍼랜드 로컬) opcode 를 각 원본 뒤에 삽입 → 이후 op 번호가 전부 밀림.
//       이미지 자체는 원본 op 만 담고 _L 치환은 로드 후(PatchLocalOps) 이뤄지지만,
//       0110 이미지를 그대로 읽으면 op 번호가 어긋나므로 버전을 올린다.
#define NEO_VER		(('0' << 24) | ('1' << 16) | ('1' << 8) | ('1'))

#if defined(_MSC_VER) && !defined(_DEBUG)
#define NEOS_FORCEINLINE __forceinline
#elif defined(__GNUC__) && __GNUC__ >= 4 && defined(NDEBUG)
#define NEOS_FORCEINLINE __attribute__((always_inline))
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
