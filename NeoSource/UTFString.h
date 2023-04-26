#pragma once

#include "NeoArchive.h"

//	0000 0000 ~ 0000 007F:	 0xxxxxxx
//	0000 0080 ~ 0000 07FF:	 110xxxxx 10xxxxxx
//	0000 0800 ~ 0000 FFFF:	 1110xxxx 10xxxxxx 10xxxxxx
//	0001 0000 ~ 001F FFFF:	 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
//	0020 0000 ~ 03FF FFFF:	 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
//	0400 0000 ~ 7FFF FFFF:	 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

struct utf_string
{

/////////////////////////////////////////////////////////////////////////
//						UNICODE	-> UTF8
/////////////////////////////////////////////////////////////////////////
static int UNICODE_UTF8( const wchar_t* pUniStr, int iLength, u8* pUtf8Str )
{
	wchar_t wChar;
	u8 szBytes[6];
	int nbytes;
	int i, j;
	int iTotelLen = 0;

	if(iLength < 0)
		iLength = (int)wcslen(pUniStr);

	if(pUtf8Str)
	{
		for( i = 0; i < iLength; i++ )
		{
			wChar = pUniStr[i];
			if( wChar <= 0x7f)
			{
				nbytes = 1;
				szBytes[0] = (u8)wChar;
			} 
			else if( wChar <= 0x7ff)
			{
				nbytes = 2;
				szBytes[0] = ((wChar >> 6) & 0x1f) | 0xc0;
				szBytes[1] = (wChar & 0x3f) | 0x80;
			}
			else if( wChar <= 0xffff)
			{
				nbytes = 3;
				szBytes[0] = ((wChar >> 12) & 0x0f) | 0xe0;
				szBytes[1] = ((wChar >> 6) & 0x3f) | 0x80;
				szBytes[2] = (wChar & 0x3f) | 0x80;
			}
			else
				nbytes = 0;


			for( j = 0; j < nbytes; j++ )
			{
				pUtf8Str[iTotelLen] = szBytes[j];
				iTotelLen++;
			}
		}

		pUtf8Str[iTotelLen] = '\0';
	}
	else
	{
		for( i = 0; i < iLength; i++ )
		{
			wChar = pUniStr[i];
			if( wChar <= 0x7f)
			{
				nbytes = 1;
			} 
			else if( wChar <= 0x7ff)
			{
				nbytes = 2;
			}
			else if( wChar <= 0xffff)
			{
				nbytes = 3;
			}
			else
				nbytes = 0;

			iTotelLen += nbytes;
		}
	}

	return iTotelLen;
}
static int UNICODE_UTF8( const wchar_t* pUniStr, int iLength, std::string& str )
{
	wchar_t wChar;
	u8 szBytes[6];
	int nbytes;
	int i, j;

	if(iLength < 0)
		iLength = (int)wcslen(pUniStr);

	str.clear();
	for( i = 0; i < iLength; i++ )
	{
		wChar = pUniStr[i];
		if( wChar <= 0x7f)
		{
			nbytes = 1;
			szBytes[0] = (u8)wChar;
		} 
		else if( wChar <= 0x7ff)
		{
			nbytes = 2;
			szBytes[0] = ((wChar >> 6) & 0x1f) | 0xc0;
			szBytes[1] = (wChar & 0x3f) | 0x80;
		}
		else if( wChar <= 0xffff)
		{
			nbytes = 3;
			szBytes[0] = ((wChar >> 12) & 0x0f) | 0xe0;
			szBytes[1] = ((wChar >> 6) & 0x3f) | 0x80;
			szBytes[2] = (wChar & 0x3f) | 0x80;
		}
		else
			nbytes = 0;


		for( j = 0; j < nbytes; j++ )
		{
			str.push_back(szBytes[j]);
		}
	}
	return (int)str.size();
}
static int UNICODE_UTF8_ONE(const wchar_t wChar, std::string& str)
{
	u8 szBytes[6];
	int nbytes;
	int j;

	if (wChar <= 0x7f)
	{
		nbytes = 1;
		szBytes[0] = (u8)wChar;
	}
	else if (wChar <= 0x7ff)
	{
		nbytes = 2;
		szBytes[0] = ((wChar >> 6) & 0x1f) | 0xc0;
		szBytes[1] = (wChar & 0x3f) | 0x80;
	}
	else if (wChar <= 0xffff)
	{
		nbytes = 3;
		szBytes[0] = ((wChar >> 12) & 0x0f) | 0xe0;
		szBytes[1] = ((wChar >> 6) & 0x3f) | 0x80;
		szBytes[2] = (wChar & 0x3f) | 0x80;
	}
	else
		nbytes = 0;


	for (j = 0; j < nbytes; j++)
	{
		str.push_back(szBytes[j]);
	}
	return nbytes;
}
/////////////////////////////////////////////////////////////////////////
//						UTF8	-> UNICODE
/////////////////////////////////////////////////////////////////////////
static const char* UTF8_GETWCHAR_FROMUTF8(const char* p, wchar_t& wChar)
{
	u8* pUtf8Str = (u8*)p;

	u8 byHeader = *pUtf8Str;
	int iIndex = 0;
	if( ( 0xE0 == ( byHeader & 0xE0 ) ) )
	{
		wChar = ( ( byHeader & 0x0f ) << 12 ) | 
			( ( pUtf8Str[1]&0x3F ) << 6 ) | 
			( pUtf8Str[2] & 0x3F );

		iIndex += 3;
	} 
	else if( 0xC0 == ( byHeader & 0xC0 ) )
	{
		wChar = ( ( byHeader & 0x1F ) << 6 ) |
			( pUtf8Str[1] & 0x3F );

		iIndex += 2;
	} 
	else
	{
		wChar = byHeader & 0x7F;

		iIndex++;
	}
	return p + iIndex;
}
static int UTF8_UNICODE( const char* _pUtf8Str, int iLength, wchar_t* pUniStr )
{
	int iIndex = 0;
	int iCount = 0;
	wchar_t wChar;
	u8 byHeader;

	const u8* pUtf8Str = (const u8*)_pUtf8Str;

	if(iLength < 0)
		iLength = (int)strlen(_pUtf8Str);

	if(pUniStr)
	{
		while( iIndex < iLength )
		{
			byHeader = pUtf8Str[iIndex];
			if( ( 0xE0 == ( byHeader & 0xE0 ) ) )
			{
				wChar = ( ( byHeader & 0x0f ) << 12 ) | 
					( ( pUtf8Str[iIndex+1]&0x3F ) << 6 ) | 
					( pUtf8Str[iIndex+2] & 0x3F );

				iIndex += 3;
			} 
			else if( 0xC0 == ( byHeader & 0xC0 ) )
			{
				wChar = ( ( byHeader & 0x1F ) << 6 ) |
					( pUtf8Str[iIndex+1] & 0x3F );

				iIndex += 2;
			} 
			else
			{
				wChar = byHeader & 0x7F;

				iIndex++;
			}

			pUniStr[iCount++] = wChar;
		}

		pUniStr[iCount] = 0;
	}
	else
	{
		while( iIndex < iLength )
		{
			byHeader = pUtf8Str[iIndex];
			if( ( 0xE0 == ( byHeader & 0xE0 ) ) )
			{
				iIndex += 3;
			} 
			else if( 0xC0 == ( byHeader & 0xC0 ) )
			{
				iIndex += 2;
			} 
			else
			{
				iIndex++;
			}
			iCount++;
		}
	}
	return iCount;
}
static int UTF8_UNICODE( const char* _pUtf8Str, int iLength, std::wstring& str )
{
	int iIndex = 0;
	wchar_t wChar;
	u8 byHeader;

	const u8* pUtf8Str = (const u8*)_pUtf8Str;

	if(iLength < 0)
		iLength = (int)strlen(_pUtf8Str);

	str.clear();
	while( iIndex < iLength )
	{
		byHeader = pUtf8Str[iIndex];
		if( ( 0xE0 == ( byHeader & 0xE0 ) ) )
		{
			wChar = ( ( byHeader & 0x0f ) << 12 ) | 
				( ( pUtf8Str[iIndex+1]&0x3F ) << 6 ) | 
				( pUtf8Str[iIndex+2] & 0x3F );

			iIndex += 3;
		} 
		else if( 0xC0 == ( byHeader & 0xC0 ) )
		{
			wChar = ( ( byHeader & 0x1F ) << 6 ) |
				( pUtf8Str[iIndex+1] & 0x3F );

			iIndex += 2;
		} 
		else
		{
			wChar = byHeader & 0x7F;

			iIndex++;
		}
		str.push_back(wChar);
	}
	return (int)str.size();
}
static int UTF8_LENGTH(const std::string& utf8)
{
	int cnt = 0;
	int iIndex = 0;
	u8 byHeader;

	const u8* pUtf8Str = (const u8*)utf8.c_str();
	int iLength = (int)utf8.length();
	while (iIndex < iLength)
	{
		byHeader = pUtf8Str[iIndex];
		if ((0xE0 == (byHeader & 0xE0)))
		{
			iIndex += 3;
		}
		else if (0xC0 == (byHeader & 0xC0))
		{
			iIndex += 2;
		}
		else
		{
			iIndex++;
		}
		cnt++;
	}
	return cnt;
}
static std::string UTF8_ONE(std::string& Utf8Str, int& iIndex)
{
	u8 byHeader;

	const u8* pUtf8Str = (const u8*)Utf8Str.c_str();

	std::string r;
	if (iIndex >= (int)Utf8Str.length())
		return "";
	byHeader = pUtf8Str[iIndex];
	if ((0xE0 == (byHeader & 0xE0)))
	{
		r.push_back(pUtf8Str[iIndex]);
		r.push_back(pUtf8Str[iIndex + 1]);
		r.push_back(pUtf8Str[iIndex + 2]);
		iIndex += 3;
	}
	else if (0xC0 == (byHeader & 0xC0))
	{
		r.push_back(pUtf8Str[iIndex]);
		r.push_back(pUtf8Str[iIndex + 1]);

		iIndex += 2;
	}
	else
	{
		r.push_back(pUtf8Str[iIndex]);

		iIndex++;
	}
	return r;
}
static int ASCII_UNICODE(const char* _pAsciiStr, int iLength, std::wstring& str)
{
	int iIndex = 0;
	wchar_t wChar;
	u8 byHeader;

	const u8* pAsciiStr = (const u8*)_pAsciiStr;

	if (iLength < 0)
		iLength = (int)strlen((const char*)pAsciiStr);

	str.clear();
	while (iIndex < iLength)
	{
		byHeader = pAsciiStr[iIndex];
		if ((0x80 == (byHeader & 0x80)))
		{
			wChar = ((byHeader & 0x1F) << 6) |
				(pAsciiStr[iIndex + 1] & 0x3F);

			iIndex += 2;
		}
		else
		{
			wChar = byHeader & 0x7F;

			iIndex++;
		}
		str.push_back(wChar);
	}
	return (int)str.size();
}
static int UNICODE_ASCII(const wchar_t* _pStr, int iLength, std::string& str)
{
	int iIndex = 0;
	wchar_t wChar;

	const wchar_t* pUniStr = (const wchar_t*)_pStr;

	if (iLength < 0)
		iLength = (int)wcslen(pUniStr);

	str.clear();
	while (iIndex < iLength)
	{
		wChar = pUniStr[iIndex];
		if ((wChar & 0xff00) == 0)
		{
			str.push_back(wChar & 0xff);
		}
		iIndex += 1;
	}
	return (int)str.size();
}
};

