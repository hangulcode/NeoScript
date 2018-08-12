#pragma once

#define FILE_UNICODE_HEADER		WORD(0xFEFF)
#define FILE_UTF8_HEADER		WORD(0xBBEF)

#define FILE_UTF8_SUB			(BYTE)(0xBF)

#include "UTFString.h"

class CArchiveRdWC
{
private:
	WORD *	m_lpBufStart;
	int		m_iOffset;
	int		m_iSize;

	int		m_iCurLine;
	int		m_iCurCol;
public:
	CArchiveRdWC()
	{
		m_lpBufStart = NULL;
		m_iOffset = 0;
		m_iSize = 0;
		m_iCurLine = 1;
		m_iCurCol = 1;
	}
	CArchiveRdWC(WORD* pBuffer, int iBufferSize)
	{
		m_lpBufStart = (WORD*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
		m_iCurLine = 1;
		m_iCurCol = 1;
	}
	void    SetData(WORD* pBuffer, int iBufferSize)
	{
		m_lpBufStart = (WORD*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
		m_iCurLine = 1;
		m_iCurCol = 1;
	}

	inline BOOL IsEOF()
	{
		return (m_iOffset >= m_iSize);
	}

	WORD GetData(bool offsetMove = true)
	{
		if (IsEOF())
			return 0;

		WORD r = m_lpBufStart[m_iOffset];
		if (offsetMove)
		{
			m_iOffset++;
			m_iCurCol++;
		}
		return r;
	}
	void AddLine()
	{
		m_iCurLine++;
		m_iCurCol = 1;
	}

	int		CurLine() { return m_iCurLine; }
	int		CurCol() { return m_iCurCol; }


	inline int GetBufferOffset()
	{
		return m_iOffset;
	}
	inline void SetBufferOffset(int off)
	{
		m_iOffset = off;
	}
	inline int GetBufferSize()
	{
		return m_iSize;
	}

protected:

};


BOOL	ToArchiveRdWC(const char* pBuffer, int iBufferSize, CArchiveRdWC& ar);
void	OutAsm(LPCSTR	lpszString, ...);

bool	StringToDouble(double& r, const char *p);
bool	StringToDoubleLow(double& r, const char *p);
