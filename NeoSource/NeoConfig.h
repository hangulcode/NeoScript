#pragma once

typedef unsigned char	u8;
typedef char			s8;
typedef unsigned short	u16;
typedef short			s16;
typedef unsigned int	u32;
typedef int				s32;

#pragma pack(1)
struct debug_info
{
	union
	{
		//u32			_data;
		u16			_data;
		struct
		{
		//	u16		_fileseq;
			u16		_lineseq;
		};
	};

	debug_info()
	{
		//_fileseq = 0;
		_lineseq = 0;
	}
	debug_info(u16 file, u16 line)
	{
		//_fileseq = file;
		_lineseq = line;
	}
};
#pragma pack()


#define FILE_NEOS	(('N' << 24) | ('E' << 16) | ('O' << 8) | ('S'))
#define NEO_VER		(('0' << 24) | ('1' << 16) | ('0' << 8) | ('0'))

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
