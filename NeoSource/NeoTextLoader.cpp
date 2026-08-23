#include <stdarg.h>
#include <stdio.h>
#include <iostream>

#include "UTFString.h"
#include "NeoTextLoader.h"

namespace NeoScript
{

// Source code and string literals must be UTF-8.  A few legacy scripts have
// CP949 text in comments only; comments carry no script meaning, so discard
// their non-ASCII bytes before the second UTF-8 decode attempt.  Invalid bytes
// anywhere else remain a compile error.
static bool SanitizeLegacyCommentBytes(const char* text, int length, std::string& sanitized)
{
	if (text == nullptr || length <= 0)
		return false;

	sanitized.assign(text, static_cast<size_t>(length));
	enum class State { Code, SingleQuote, DoubleQuote, LineComment, BlockComment };
	State state = State::Code;
	bool changed = false;

	for (int i = 0; i < length; ++i)
	{
		const char c = sanitized[i];
		switch (state)
		{
		case State::Code:
			if (c == '/' && i + 1 < length && sanitized[i + 1] == '/')
			{
				state = State::LineComment;
				++i;
			}
			else if (c == '/' && i + 1 < length && sanitized[i + 1] == '*')
			{
				state = State::BlockComment;
				++i;
			}
			else if (c == '\'')
				state = State::SingleQuote;
			else if (c == '"')
				state = State::DoubleQuote;
			break;

		case State::SingleQuote:
		case State::DoubleQuote:
			if (c == '\\' && i + 1 < length)
				++i;
			else if ((state == State::SingleQuote && c == '\'') || (state == State::DoubleQuote && c == '"'))
				state = State::Code;
			break;

		case State::LineComment:
			if (c == '\r' || c == '\n')
				state = State::Code;
			else if (static_cast<u8>(c) >= 0x80)
			{
				sanitized[i] = ' ';
				changed = true;
			}
			break;

		case State::BlockComment:
			if (c == '*' && i + 1 < length && sanitized[i + 1] == '/')
			{
				state = State::Code;
				++i;
			}
			else if (static_cast<u8>(c) >= 0x80)
			{
				sanitized[i] = ' ';
				changed = true;
			}
			break;
		}
	}
	return changed;
}

bool		ToArchiveRdWC(const char* pBuffer, int iBufferSize, CArchiveRdWC& ar)
{
	if (iBufferSize < 0 || (pBuffer == nullptr && iBufferSize != 0))
		return false;

	const u8* bytes = reinterpret_cast<const u8*>(pBuffer);
	std::u16string source;

	// Detect BOMs as bytes.  Casting an arbitrary char buffer to u16* is
	// unaligned/aliasing-unsafe on several Linux targets.
	if (iBufferSize >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) // UTF-16 LE
	{
		const int payloadSize = iBufferSize - 2;
		if ((payloadSize & 1) != 0)
			return false;

		source.resize(static_cast<size_t>(payloadSize / 2));
		for (size_t i = 0; i < source.size(); ++i)
			source[i] = static_cast<char16_t>(bytes[2 + i * 2] | (static_cast<u16>(bytes[3 + i * 2]) << 8));
	}
	else if (iBufferSize >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) // UTF-16 BE
	{
		const int payloadSize = iBufferSize - 2;
		if ((payloadSize & 1) != 0)
			return false;

		source.resize(static_cast<size_t>(payloadSize / 2));
		for (size_t i = 0; i < source.size(); ++i)
			source[i] = static_cast<char16_t>((static_cast<u16>(bytes[2 + i * 2]) << 8) | bytes[3 + i * 2]);
	}
	else
	{
		// Neo Script source is UTF-8 whether or not an editor wrote a BOM.
		const int bomSize = (iBufferSize >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) ? 3 : 0;
		const char* utf8 = pBuffer == nullptr ? nullptr : pBuffer + bomSize;
		if (utf_string::UTF8_UNICODE(utf8, iBufferSize - bomSize, source) < 0)
		{
			std::string sanitized;
			if (!SanitizeLegacyCommentBytes(utf8, iBufferSize - bomSize, sanitized) ||
				utf_string::UTF8_UNICODE(sanitized.data(), static_cast<int>(sanitized.size()), source) < 0)
				return false;
		}
	}

	if (!utf_string::IsValidUTF16(source))
		return false;

	ar.SetData(std::move(source));
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

// 정수/실수를 명시적으로 구분해 파싱. 정수는 double 을 거치지 않고 int32 로 정확히 만든다.
// (0xffffffff → uint32 4294967295 → int32 재해석 -1. 예전 (int)(double) 은 범위초과 시 INT_MIN 이었음.)
bool StringToNumber(ParsedNumber& out, const char* p)
{
	const char* orig = p;
	bool negative = false;
	if (*p == '-' || *p == '+') { negative = (*p == '-'); p++; }

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
	{
		const char* q = p + 2;
		uint64_t uval = 0;
		int digits = 0;
		while (isHexDigit(*q)) { uval = uval * 16 + (uint64_t)hexValue(*q); q++; digits++; }
		if (digits == 0)
			return false;

		if (*q == '.' || *q == 'p' || *q == 'P') // hex float (0x1a.FF, 0x1p4)
		{
			double d;
			if (false == StringToDouble(d, orig))
				return false;
			out.type = ParsedNumber::Type::Float;
			out.floatValue = d;
			out.intValue = (int32_t)(long long)d;
			return true;
		}
		if (*q != '\0')
			return false;

		uint32_t u32 = (uint32_t)uval;              // 32비트로 절단 후 int32 비트패턴
		double   mag = (double)uval;
		out.type = ParsedNumber::Type::Int;
		out.intValue   = negative ? -(int32_t)u32 : (int32_t)u32;
		out.floatValue = negative ? -mag : mag;
		return true;
	}

	// 10진: '.' 또는 'e'/'E' 가 있으면 실수
	const char* q = p;
	while (valid_digit(*q)) q++;
	if (*q == '.' || *q == 'e' || *q == 'E')
	{
		double d;
		if (false == StringToDouble(d, orig))
			return false;
		out.type = ParsedNumber::Type::Float;
		out.floatValue = d;
		out.intValue = (int32_t)(long long)d;
		return true;
	}

	// 10진 정수: int64 누적 후 int32
	long long ival = 0;
	int digits = 0;
	const char* r = p;
	while (valid_digit(*r)) { ival = ival * 10 + (long long)(*r - '0'); r++; digits++; }
	if (digits == 0 || *r != '\0')
		return false;

	out.type = ParsedNumber::Type::Int;
	out.intValue   = negative ? -(int32_t)ival : (int32_t)ival;
	out.floatValue = negative ? -(double)ival : (double)ival;
	return true;
}

void SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...)
{
	char buff[4096];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsnprintf(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, sizeof(buff), lpszString, arg_ptr);
#endif
	va_end(arg_ptr);

	if(ar.m_sErrorString.empty())
		ar.m_sErrorString = buff;
//#ifdef _WIN32	
//	printf(buff);
//#endif
}

// 포맷은 여기 한 곳에서만 한다. 디스어셈블 결과는 콘솔로 찍기도 하고(컴파일 리스팅)
// 에디터로 넘기기도 하므로(디버거 어셈블리 뷰), 만드는 코드를 한 벌만 두고
// 목적지는 호출자가 정한다.
std::string FormatAsm(const char*	lpszString, ...)
{
	char buff[4096];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsnprintf(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, sizeof(buff), lpszString, arg_ptr);
#endif
	va_end(arg_ptr);
	return buff;
}

void OutAsm(const char*	lpszString, ...)
{
	char buff[4096];
	va_list arg_ptr;
	va_start(arg_ptr, lpszString);
#ifdef _WIN32
	vsnprintf(buff, _countof(buff), lpszString, arg_ptr);
#else
	vsnprintf(buff, sizeof(buff), lpszString, arg_ptr);
#endif
	va_end(arg_ptr);

	OutString(buff);
}
void OutString(const char* lpszString)
{
	std::cout << lpszString;
}

// iMaxCount 칸을 "%02X " 3문자씩 채운다. iCount 를 넘는 칸은 공백이라 폭이 항상 같다.
std::string FormatBytes(const u8*	pBuffer, int iCount, int iMaxCount)
{
	std::string r;
	if (iMaxCount <= 0)
		return r;
	r.reserve((size_t)iMaxCount * 3);
	char cell[8];
	for (int i = 0; i < iMaxCount; i++)
	{
		if (i < iCount)
		{
			snprintf(cell, sizeof(cell), "%02X ", pBuffer[i]);
			r.append(cell, 3);
		}
		else
		{
			r.append("   ", 3);
		}
	}
	return r;
}

void OutBytes(const u8*	pBuffer, int iCount, int iMaxCount)
{
	OutString(FormatBytes(pBuffer, iCount, iMaxCount).c_str());
}

};

