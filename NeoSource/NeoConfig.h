#pragma once

namespace NeoScript
{


typedef unsigned char	u8;
typedef char			s8;
typedef unsigned short	u16;
typedef short			s16;
typedef unsigned int	u32;
typedef int				s32;

// 기본은 float (게임 엔진 정렬 + 벡터 값타입 인라인). double 이 필요하면 NS_DOUBLE_PRECISION 정의.
#ifdef NS_DOUBLE_PRECISION
	typedef double		NS_FLOAT;
#else
	typedef float		NS_FLOAT;
#endif


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
#define NEO_VER		(('0' << 24) | ('1' << 16) | ('1' << 8) | ('0'))

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