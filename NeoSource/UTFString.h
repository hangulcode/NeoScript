#pragma once

#include "NeoArchive.h"

namespace NeoScript
{

// Text used by the parser is UTF-16.  Do not use wchar_t here: its width is
// platform dependent (UTF-16 on Windows, generally UTF-32 on Linux).
struct utf_string
{
private:
	static bool IsHighSurrogate(u16 value)
	{
		return value >= 0xD800 && value <= 0xDBFF;
	}

	static bool IsLowSurrogate(u16 value)
	{
		return value >= 0xDC00 && value <= 0xDFFF;
	}

	static int AppendUTF8CodePoint(u32 codePoint, std::string& str)
	{
		if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
			return -1;

		if (codePoint <= 0x7F)
		{
			str.push_back(static_cast<char>(codePoint));
			return 1;
		}
		if (codePoint <= 0x7FF)
		{
			str.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
			str.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			return 2;
		}
		if (codePoint <= 0xFFFF)
		{
			str.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
			str.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
			str.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			return 3;
		}

		str.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
		str.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
		str.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
		str.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		return 4;
	}

	static bool DecodeUTF8CodePoint(const u8* bytes, int length, int& offset, u32& codePoint)
	{
		if (offset >= length)
			return false;

		const u8 first = bytes[offset];
		int sequenceLength = 0;
		u32 value = 0;
		u32 minimumValue = 0;
		if (first <= 0x7F)
		{
			codePoint = first;
			++offset;
			return true;
		}
		if (first >= 0xC2 && first <= 0xDF)
		{
			sequenceLength = 2;
			value = first & 0x1F;
			minimumValue = 0x80;
		}
		else if (first >= 0xE0 && first <= 0xEF)
		{
			sequenceLength = 3;
			value = first & 0x0F;
			minimumValue = 0x800;
		}
		else if (first >= 0xF0 && first <= 0xF4)
		{
			sequenceLength = 4;
			value = first & 0x07;
			minimumValue = 0x10000;
		}
		else
		{
			return false;
		}

		if (length - offset < sequenceLength)
			return false;
		for (int i = 1; i < sequenceLength; ++i)
		{
			const u8 next = bytes[offset + i];
			if ((next & 0xC0) != 0x80)
				return false;
			value = (value << 6) | (next & 0x3F);
		}

		if (value < minimumValue || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
			return false;

		codePoint = value;
		offset += sequenceLength;
		return true;
	}

public:
	static bool IsValidUTF16(const std::u16string& str)
	{
		for (size_t i = 0; i < str.size(); ++i)
		{
			const u16 value = static_cast<u16>(str[i]);
			if (IsHighSurrogate(value))
			{
				if (i + 1 >= str.size() || !IsLowSurrogate(static_cast<u16>(str[++i])))
					return false;
			}
			else if (IsLowSurrogate(value))
			{
				return false;
			}
		}
		return true;
	}

/////////////////////////////////////////////////////////////////////////
// UTF-16 -> UTF-8
/////////////////////////////////////////////////////////////////////////
	static int UNICODE_UTF8(const char16_t* pUniStr, int iLength, std::string& str)
	{
		if (pUniStr == nullptr)
			return iLength == 0 ? 0 : -1;
		if (iLength < 0)
			iLength = static_cast<int>(std::char_traits<char16_t>::length(pUniStr));

		str.clear();
		for (int i = 0; i < iLength; ++i)
		{
			const u16 first = static_cast<u16>(pUniStr[i]);
			u32 codePoint = first;
			if (IsHighSurrogate(first))
			{
				if (i + 1 >= iLength)
				{
					str.clear();
					return -1;
				}
				const u16 second = static_cast<u16>(pUniStr[++i]);
				if (!IsLowSurrogate(second))
				{
					str.clear();
					return -1;
				}
				codePoint = 0x10000 + ((static_cast<u32>(first) - 0xD800) << 10) + (static_cast<u32>(second) - 0xDC00);
			}
			else if (IsLowSurrogate(first))
			{
				str.clear();
				return -1;
			}

			if (AppendUTF8CodePoint(codePoint, str) < 0)
			{
				str.clear();
				return -1;
			}
		}
		return static_cast<int>(str.size());
	}

	static int UNICODE_UTF8(const char16_t* pUniStr, int iLength, u8* pUtf8Str)
	{
		std::string utf8;
		const int result = UNICODE_UTF8(pUniStr, iLength, utf8);
		if (result < 0)
			return -1;
		if (pUtf8Str != nullptr)
		{
			memcpy(pUtf8Str, utf8.data(), utf8.size());
			pUtf8Str[utf8.size()] = 0;
		}
		return result;
	}

	static int UNICODE_UTF8_ONE(u16 value, std::string& str)
	{
		if (IsHighSurrogate(value) || IsLowSurrogate(value))
			return -1;
		return AppendUTF8CodePoint(value, str);
	}

	static int UNICODE_UTF8_PAIR(u16 high, u16 low, std::string& str)
	{
		if (!IsHighSurrogate(high) || !IsLowSurrogate(low))
			return -1;
		const u32 codePoint = 0x10000 + ((static_cast<u32>(high) - 0xD800) << 10) + (static_cast<u32>(low) - 0xDC00);
		return AppendUTF8CodePoint(codePoint, str);
	}

/////////////////////////////////////////////////////////////////////////
// UTF-8 -> UTF-16
/////////////////////////////////////////////////////////////////////////
	static int UTF8_UNICODE(const char* pUtf8Str, int iLength, std::u16string& str)
	{
		if (pUtf8Str == nullptr)
			return iLength == 0 ? 0 : -1;
		if (iLength < 0)
			iLength = static_cast<int>(strlen(pUtf8Str));

		str.clear();
		const u8* bytes = reinterpret_cast<const u8*>(pUtf8Str);
		for (int offset = 0; offset < iLength;)
		{
			u32 codePoint = 0;
			if (!DecodeUTF8CodePoint(bytes, iLength, offset, codePoint))
			{
				str.clear();
				return -1;
			}

			if (codePoint <= 0xFFFF)
			{
				str.push_back(static_cast<char16_t>(codePoint));
			}
			else
			{
				codePoint -= 0x10000;
				str.push_back(static_cast<char16_t>(0xD800 + (codePoint >> 10)));
				str.push_back(static_cast<char16_t>(0xDC00 + (codePoint & 0x3FF)));
			}
		}
		return static_cast<int>(str.size());
	}

	static int UTF8_UNICODE(const char* pUtf8Str, int iLength, char16_t* pUniStr)
	{
		std::u16string utf16;
		const int result = UTF8_UNICODE(pUtf8Str, iLength, utf16);
		if (result < 0)
			return -1;
		if (pUniStr != nullptr)
		{
			memcpy(pUniStr, utf16.data(), utf16.size() * sizeof(char16_t));
			pUniStr[utf16.size()] = 0;
		}
		return result;
	}

/////////////////////////////////////////////////////////////////////////
// UTF-8 character traversal. Invalid input is consumed one byte at a time
// so runtime string operations cannot run past the end of a malformed value.
/////////////////////////////////////////////////////////////////////////
	static int UTF8_LENGTH(const std::string& utf8)
	{
		const u8* bytes = reinterpret_cast<const u8*>(utf8.data());
		const int length = static_cast<int>(utf8.size());
		int count = 0;
		for (int offset = 0; offset < length; ++count)
		{
			const int start = offset;
			u32 codePoint = 0;
			if (!DecodeUTF8CodePoint(bytes, length, offset, codePoint))
				offset = start + 1;
		}
		return count;
	}

// index : byte index
// offset : character index
	static int UTF8_INDEX2OFFSET(const std::string& utf8, int iFindIndex)
	{
		if (iFindIndex < 0)
			return iFindIndex;

		const u8* bytes = reinterpret_cast<const u8*>(utf8.data());
		const int length = static_cast<int>(utf8.size());
		int count = 0;
		for (int offset = 0; offset < length; ++count)
		{
			if (offset >= iFindIndex)
				return count;
			const int start = offset;
			u32 codePoint = 0;
			if (!DecodeUTF8CodePoint(bytes, length, offset, codePoint))
				offset = start + 1;
		}
		return count;
	}

	static int UTF8_OFFSET(const std::string& utf8, int iIndex, int wordCount)
	{
		const int length = static_cast<int>(utf8.size());
		if (iIndex >= length)
			return length;
		if (wordCount <= 0)
			return iIndex;

		const u8* bytes = reinterpret_cast<const u8*>(utf8.data());
		int count = 0;
		while (iIndex < length)
		{
			const int start = iIndex;
			u32 codePoint = 0;
			if (!DecodeUTF8CodePoint(bytes, length, iIndex, codePoint))
				iIndex = start + 1;
			if (++count >= wordCount)
				return iIndex;
		}
		return iIndex;
	}

	static SUtf8One UTF8_ONE(std::string& utf8Str, int& iIndex)
	{
		SUtf8One result{};
		const int length = static_cast<int>(utf8Str.size());
		if (iIndex < 0 || iIndex >= length)
			return result;

		const u8* bytes = reinterpret_cast<const u8*>(utf8Str.data());
		const int start = iIndex;
		u32 codePoint = 0;
		if (!DecodeUTF8CodePoint(bytes, length, iIndex, codePoint))
			iIndex = start + 1;
		const int byteCount = iIndex - start;
		memcpy(result.c, utf8Str.data() + start, byteCount);
		result.c[byteCount] = 0;
		return result;
	}

	static std::string UTF8_ONE_STR(std::string& utf8Str, int& iIndex)
	{
		const int length = static_cast<int>(utf8Str.size());
		if (iIndex < 0 || iIndex >= length)
			return "";

		const u8* bytes = reinterpret_cast<const u8*>(utf8Str.data());
		const int start = iIndex;
		u32 codePoint = 0;
		if (!DecodeUTF8CodePoint(bytes, length, iIndex, codePoint))
			iIndex = start + 1;
		return utf8Str.substr(static_cast<size_t>(start), static_cast<size_t>(iIndex - start));
	}

	static int ASCII_UNICODE(const char* ascii, int length, std::u16string& str)
	{
		if (ascii == nullptr)
			return length == 0 ? 0 : -1;
		if (length < 0)
			length = static_cast<int>(strlen(ascii));

		str.clear();
		for (int i = 0; i < length; ++i)
			str.push_back(static_cast<char16_t>(static_cast<u8>(ascii[i])));
		return static_cast<int>(str.size());
	}

	static int UNICODE_ASCII(const char16_t* utf16, int length, std::string& str)
	{
		if (utf16 == nullptr)
			return length == 0 ? 0 : -1;
		if (length < 0)
			length = static_cast<int>(std::char_traits<char16_t>::length(utf16));

		str.clear();
		for (int i = 0; i < length; ++i)
		{
			const u16 value = static_cast<u16>(utf16[i]);
			if (value <= 0xFF)
				str.push_back(static_cast<char>(value));
		}
		return static_cast<int>(str.size());
	}
};

};
