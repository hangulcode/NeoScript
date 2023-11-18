#include <stdarg.h>
#include <stdio.h>
#include <locale>
#include <codecvt>
#include <stdexcept>

#include "UTFString.h"
#include "NeoTextLoader.h"

template<class I, class E, class S>
struct MyCodecvt : std::codecvt<I, E, S>
{
	~MyCodecvt()
	{ }
};


bool		ToArchiveRdWC(const char* pBuffer, int iBufferSize, CArchiveRdWC& ar)
{
	if (iBufferSize < 3)
		return false;

	u16* pWBuffer;
	int size_toUni;
	if ((*(u16*)pBuffer) == FILE_UNICODE_HEADER_LE) // Little Endian
	{
		size_toUni = iBufferSize / 2 - 1;
		pWBuffer = new u16[size_toUni + 1];
		memcpy(pWBuffer, (u16*)(pBuffer + 2), size_toUni * 2);
		pWBuffer[size_toUni] = 0;
		ar.SetData(pWBuffer, size_toUni);
	}
	else if ((*(u16*)pBuffer) == FILE_UNICODE_HEADER_BE) // Big Endian
	{
		size_toUni = iBufferSize / 2 - 1;
		pWBuffer = new u16[size_toUni + 1];

		u8* pSrc = (u8*)(pBuffer + 2);
		u8* pDest = (u8*)pWBuffer;
		for (int i = 0; i < size_toUni; i++)
		{
			*pDest++ = pSrc[1];
			*pDest++ = pSrc[0];
			pSrc += 2;
		}
		pWBuffer[size_toUni] = 0;
		ar.SetData(pWBuffer, size_toUni);
	}
	else
	{
		std::wstring str;
		if ((*(u16*)pBuffer) == FILE_UTF8_HEADER && *(u8*)(pBuffer + 2) == FILE_UTF8_SUB)
		{
			utf_string::UTF8_UNICODE(pBuffer + 3, iBufferSize - 3, str);
			size_toUni = (int)str.length();
			pWBuffer = new u16[str.length() + 1];
			memcpy(pWBuffer, (u16*)(str.c_str()), size_toUni * 2);
			pWBuffer[size_toUni] = 0;
			ar.SetData(pWBuffer, (int)str.length());
		}
		else
		{
//			std::vector<char> bytes;
//			bytes.resize(iBufferSize);
//			memcpy((void*)&*bytes.begin(), pBuffer, iBufferSize);
#if 0
			std::wstring wstr = std::wstring(ansi_str.begin(), ansi_str.end());
#else
			std::locale utf8Locale(std::locale(), new std::codecvt_utf8<wchar_t>);

			std::wstring_convert<MyCodecvt<wchar_t, char, std::mbstate_t>> converter;
			std::wstring wstr = converter.from_bytes(pBuffer, pBuffer + iBufferSize);


//			std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
//			std::wstring wstr = converter.from_bytes(ansi_str);
#endif
			size_toUni = (int)wstr.length();
			pWBuffer = new u16[size_toUni + 1];
			memcpy(pWBuffer, wstr.c_str(), sizeof(u16) * (size_toUni));

			//int size_toUni = ::MultiByteToWideChar(CP_ACP, 0, pBuffer, iBufferSize, NULL, 0);
			//pWBuffer = new u16[size_toUni + 1];

			//::MultiByteToWideChar(CP_ACP, 0, pBuffer, iBufferSize, (u16*)str.c_str(), size_toUni);
			pWBuffer[size_toUni] = 0;
			ar.SetData(pWBuffer, size_toUni);
		}
	}
	return true;
}




#define white_space(c) ((c) == ' ' || (c) == '\t')
#define valid_digit(c) ((c) >= '0' && (c) <= '9')

bool StringToDoubleLow(double& r, const char *p)
{
	double f2 = 0;
	double scale = 0.1;

	while (true)
	{
		char c = *p++;
		if (c == 0)
			break;

		if (valid_digit(c) == false)
			return false;

		f2 += scale * (double)(c - '0');
		scale /= 10.0;
	}
	r = f2;
	return true;
}

bool StringToDouble(double& r, const char *p) 
{
	while (white_space(*p))
		p += 1;

	r = 0.0;
	int c = 0;

	bool neg = false;
	if (*p == '-')
	{
		neg = true;
		++p;
	}
	else if (*p == '+')
	{
		neg = false;
		++p;
	}

	double scale = 0.1;

	const char *pSave = p;
	while (valid_digit(*p))
	{
		scale *= 10.0;
		++p; 
		++c;
	}

	if (c == 0)
		return false;


	p = pSave;
	double f1 = 0.0;
	while (valid_digit(*p))
	{
		f1 += scale * (double)(*p - '0');
		scale /= 10.0;
		++p;
	}

	if (*p == '.')
	{
		++p;

		double f2 = 0;
		if (false == StringToDoubleLow(f2, p))
			return false;

		r = f1 + f2;
	}
	else
		r = f1;


	if (neg) 
		r = -r; 

	return true;
}

void SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...)
{
	char buff[4096];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsprintf_s(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, 8, lpszString, arg_ptr);
#endif
	va_end(arg_ptr);

	if(ar.m_sErrorString.empty())
		ar.m_sErrorString = buff;
//#ifdef _WIN32	
//	printf(buff);
//#endif
}

void OutAsm(const char*	lpszString, ...)
{
	char buff[4096];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsprintf_s(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, 8, lpszString, arg_ptr);
#endif
	va_end(arg_ptr);


#ifdef _WIN32	
	printf(buff);
#endif
}

void OutBytes(const u8*	pBuffer, int iCount, int iMaxCount)
{
	char buff[4096];
	buff[0] = 0;

#ifdef _WIN32
	for (int i = 0; i < iMaxCount; i++)
	{
		if(i < iCount)
			sprintf_s(buff + i * 3, 4, "%02X ", pBuffer[i]);
		else
			sprintf_s(buff + i * 3, 4, "   ");
	}
#else
	for (int i = 0; i < iMaxCount; i++)
	{
		if (i < iCount)
			sprintf(buff + i * 3, "%02X ", pBuffer[i]);
		else
			sprintf(buff + i * 3, "   ");
	}
#endif

#ifdef _WIN32	
	printf(buff);
#endif
}