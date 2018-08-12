
#include <math.h>
#include "NeoTextLoader.h"


BOOL		ToArchiveRdWC(const char* pBuffer, int iBufferSize, CArchiveRdWC& ar)
{
	if (iBufferSize < 3)
		return FALSE;

	WORD* pWBuffer;
	int size_toUni;
	if ((*(WORD*)pBuffer) == FILE_UNICODE_HEADER)
	{
		size_toUni = iBufferSize / 2 - 1;
		pWBuffer = new WORD[size_toUni + 1];
		memcpy(pWBuffer, (WORD*)(pBuffer + 2), size_toUni * 2);
		pWBuffer[size_toUni] = NULL;
		ar.SetData(pWBuffer, size_toUni);
	}
	else
	{
		std::wstring str;
		if ((*(WORD*)pBuffer) == FILE_UTF8_HEADER && *(BYTE*)(pBuffer + 2) == FILE_UTF8_SUB)
		{
			utf_string::UTF8_UNICODE(pBuffer + 3, iBufferSize - 3, str);
			size_toUni = str.length();
			pWBuffer = new WORD[str.length() + 1];
			memcpy(pWBuffer, (WORD*)(pBuffer + 2), size_toUni * 2);
			pWBuffer[size_toUni] = NULL;
			ar.SetData(pWBuffer, str.length());
		}
		else
		{
			int size_toUni = ::MultiByteToWideChar(CP_ACP, 0, pBuffer, iBufferSize, NULL, 0);
			pWBuffer = new WORD[size_toUni + 1];

			::MultiByteToWideChar(CP_ACP, 0, pBuffer, iBufferSize, (LPWSTR)str.c_str(), size_toUni);
			pWBuffer[size_toUni] = NULL;
			ar.SetData(pWBuffer, size_toUni);
		}
		//r = Parse(str.c_str(), (int)str.length());
	}
	return TRUE;
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
		if (c == NULL)
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

void DebugLog(LPCSTR	lpszString, ...)
{
	char buff[4096];
	va_list ap;
	va_start(ap, lpszString);
	vsprintf_s(buff, _countof(buff), lpszString, ap);
	va_end(ap);

	OutputDebugStringA(buff);
	printf(buff);
}

void OutAsm(LPCSTR	lpszString, ...)
{
	char buff[4096];
	va_list ap;
	va_start(ap, lpszString);
	vsprintf_s(buff, _countof(buff), lpszString, ap);
	va_end(ap);

	strcat_s(buff, _countof(buff), "\n");

	OutputDebugStringA(buff);
	printf(buff);
}