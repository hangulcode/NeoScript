#include <stdarg.h>
#include <stdio.h>
#include <locale>
#include <codecvt>
#include <stdexcept>
#include <iostream>

#include "UTFString.h"
#include "NeoTextLoader.h"

namespace NeoScript
{

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

#if false

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
#else
//#include <cmath>
//#include <cctype>

// 헥사 숫자 판단 및 값을 얻기 위한 헬퍼 함수들
bool isHexDigit(char c) {
	return (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

int hexValue(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;  // isHexDigit()를 사용하면 여기에 도달하지 않음.
}

// 문자열을 double로 변환하는 함수
// 문자열에 약간의 오류라도 있으면 false를 리턴하고,
// 올바른 경우에만 변환한 값을 r에 대입합니다.
bool StringToDouble(double& r, const char* p) {
	// 선행 공백(공백, 탭) 건너뛰기
/*	while (*p == ' ' || *p == '\t')
		p++;
*/
	// 부호 처리
	bool negative = false;
	if (*p == '-' || *p == '+') {
		negative = (*p == '-');
		p++;
	}

	// 16진수 형식 처리: "0x" 또는 "0X"로 시작하는 경우
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		p += 2;  // "0x" 또는 "0X" 건너뛰기
		double value = 0.0;
		int digits = 0;
		// 헥사 정수부 파싱
		while (isHexDigit(*p)) {
			value = value * 16 + hexValue(*p);
			p++;
			digits++;
		}
		if (digits == 0)
			return false;  // 헥사 숫자가 하나도 없음

		// 선택적 헥사 소수부 (예, "0x1a.FF")
		if (*p == '.') {
			p++;
			double frac = 0.0;
			double scale = 1.0 / 16;
			int fracDigits = 0;
			while (isHexDigit(*p)) {
				frac += hexValue(*p) * scale;
				scale /= 16;
				p++;
				fracDigits++;
			}
			if (fracDigits == 0)
				return false;  // 소수점 뒤에 헥사 숫자가 없으면 오류
			value += frac;
		}

		// 선택적 헥사 부동소수점 지수부 (C99 스타일, 'p' 또는 'P' 사용)
		if (*p == 'p' || *p == 'P') {
			p++;
			bool expNegative = false;
			if (*p == '-' || *p == '+') {
				expNegative = (*p == '-');
				p++;
			}
			if (!std::isdigit(*p))
				return false;  // 지수부는 최소 한 자리 숫자 필요
			int exp = 0;
			while (std::isdigit(*p)) {
				exp = exp * 10 + (*p - '0');
				p++;
			}
			if (expNegative)
				exp = -exp;
			// ldexp(value, exp) == value * 2^(exp)
			value = std::ldexp(value, exp);
		}

		// 문자열의 끝이어야 함
		if (*p != '\0')
			return false;

		r = negative ? -value : value;
		return true;
	}
	else {
		// 10진수 형식 처리

		// 정수부 파싱
		double intPart = 0.0;
		bool hasDigits = false;
		while (std::isdigit(*p)) {
			hasDigits = true;
			intPart = intPart * 10 + (*p - '0');
			p++;
		}

		// 소수부 파싱 (있다면)
		double fracPart = 0.0;
		if (*p == '.') {
			p++;
			bool fracDigits = false;
			double factor = 0.1;
			while (std::isdigit(*p)) {
				fracDigits = true;
				fracPart += (*p - '0') * factor;
				factor /= 10;
				p++;
			}
			// 정수부, 소수부 모두 숫자가 없으면 오류 처리
			if (!hasDigits && !fracDigits)
				return false;
		}
		else if (!hasDigits) {
			// 정수부가 하나도 없고 소수점이 없는 경우
			return false;
		}

		double value = intPart + fracPart;

		// 선택적 지수부 파싱 (e 또는 E 사용)
		if (*p == 'e' || *p == 'E') {
			p++;
			bool expNegative = false;
			if (*p == '-' || *p == '+') {
				expNegative = (*p == '-');
				p++;
			}
			if (!std::isdigit(*p))
				return false;  // 지수부는 최소 한 자리 숫자 필요
			int exp = 0;
			while (std::isdigit(*p)) {
				exp = exp * 10 + (*p - '0');
				p++;
			}
			if (expNegative)
				exp = -exp;
			value *= std::pow(10, exp);
		}

		// 남은 문자가 없어야 함
		if (*p != '\0')
			return false;

		r = negative ? -value : value;
		return true;
	}
}

#endif

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
	std::cout << buff;
#endif
}
void OutString(const char* lpszString)
{
#ifdef _WIN32	
	std::cout << lpszString;
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
	std::cout << buff;
#endif
}

};
