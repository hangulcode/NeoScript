#pragma once 

#include "NeoConfig.h"
#include "NeoCompileLimits.h"
#include <algorithm>
#include <memory>
#include <string>

namespace NeoScript
{

class CNArchive
{
private:
	u8*	m_lpBufStart;
	int		m_iOffset;
	int		m_iSize;
	int		m_iMaxSize;
	bool	m_bAlloc;
public:
	CNArchive(int iMaxSize = INT_MAX)
	{
		m_iMaxSize = iMaxSize;
		m_iOffset = 0;
		CheckCompileRange("archive capacity", iMaxSize, 1, INT_MAX);
		m_iSize = (std::min)(10 * 1024, iMaxSize);
		m_lpBufStart = new u8[m_iSize];
		m_bAlloc = true;
	}
	CNArchive(void* pBuffer, int iBufferSize, int iMaxSize = INT_MAX)
	{
		m_iMaxSize = iMaxSize;
		m_lpBufStart = (u8*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
		m_bAlloc = false;
	}
	~CNArchive()
	{
		if (m_bAlloc)
		{
			delete [] m_lpBufStart;
			m_bAlloc = false;
		}
		m_lpBufStart = NULL;
		m_iOffset = m_iSize = 0;
	}

    void*   GetData()
    {
        return (void*)m_lpBufStart;
    }
	void*   GetDataCurrent()
	{
		return (void*)((u8*)m_lpBufStart + m_iOffset);
	}

	inline int GetBufferOffset()
	{
		return m_iOffset;
	}
	inline void SetBufferOffset(int offset)
	{
		m_iOffset = offset;
		if (m_iOffset < 0)
			m_iOffset = 0;
		if (m_iOffset > m_iSize)
			m_iOffset = m_iSize;
	}
	inline int GetBufferSize()
	{
		return m_iSize;
	}
	void operator+=(int ln)
	{
		if(ln >= 0)
		{
			if(ln <= (int)(m_iSize - m_iOffset))
			{
				m_iOffset += ln;
				return;
			}
		}
	}
	inline int SetPointer(int lDistanceToMove,int dwMoveMethod)
	{
		switch(dwMoveMethod)
		{
			case SEEK_SET:
				m_iOffset = lDistanceToMove;
				break;
			case SEEK_CUR:
                m_iOffset += lDistanceToMove;
				break;
			case SEEK_END:
				m_iOffset = m_iSize + lDistanceToMove;
				break;
		}
        if(m_iOffset < 0)
            m_iOffset = 0;
        if(m_iOffset > m_iSize)
            m_iOffset = m_iSize;
        return m_iOffset;
	}

	template<class T>
	CNArchive& operator<<(const T& t)
	{
		Write(&t,sizeof(T));
		return *this;
	}
	bool Alloc(int iSize)
	{
		if (iSize <= 0 || iSize > m_iMaxSize) return false;
		const int capacity = (int)(std::min)((int64_t)m_iMaxSize, (int64_t)iSize * 2);
		std::unique_ptr<u8[]> buffer(new u8[capacity]);
		if (m_iOffset > 0) memcpy(buffer.get(), m_lpBufStart, m_iOffset);
		if (m_bAlloc) delete[] m_lpBufStart;
		m_lpBufStart = buffer.release();
		m_iSize = capacity;
		m_bAlloc = true;
		return true;
	}
	u32 Write(const void* lpBuf, int64_t nMax)
	{
		// A failed write must stop compilation, including operator<< and temporary
		// code archives. Never return a successfully truncated image to the caller.
		CheckCompileRange("archive write size", nMax, 0, INT_MAX);
		const int end = (int)CheckCompileRange("script image/code bytes",
			(int64_t)m_iOffset + nMax, 0, m_iMaxSize);
		if (nMax == 0) return 0;
		if (end > m_iSize && !Alloc(end))
			throw CompileLimitError("script image/code bytes", end, 0, m_iMaxSize);
		memcpy(m_lpBufStart + m_iOffset, lpBuf, nMax);
		m_iOffset = end;
		return (u32)nMax;
	}
	void WriteCount(u32 dwCount)
	{
		u16 wCount;
		if (dwCount < 0xFFFF)
		{
			wCount = (u16)dwCount;
			*this << wCount;
		}
		else
		{
			wCount = 0xFFFF;
			*this << wCount;
			*this << dwCount;
		}
	}
	template<class T>
	CNArchive& operator>>(T& t)
	{
		Read(&t, sizeof(T));
		return *this;
	}

	u32 Read(void* lpBuf, int nMax)
	{
		if (nMax == 0)
			return 0;

		if (nMax < 0 || m_iOffset < 0 || nMax > m_iSize - m_iOffset)
			return 0;
		memcpy(lpBuf, m_lpBufStart + m_iOffset, nMax);
		m_iOffset += nMax;
		return nMax;
	}

	CNArchive& operator << (const char* lpszString)
	{
		std::string str = lpszString;
		*this << str;

		return *this;
	}
	CNArchive& operator << (const char16_t* lpszString)
	{
		std::u16string str = lpszString ? lpszString : u"";
		*this << str;

		return *this;
	}
	// wchar_t has a platform-dependent width.  Do not let the generic template
	// serialize a pointer/std::wstring object by mistake after moving to UTF-16.
	CNArchive& operator << (const wchar_t*) = delete;
	CNArchive& operator << (const std::wstring&) = delete;
	CNArchive& operator << (std::string& strString)
	{
		int nLen = (int)CheckCompileRange("archive string bytes", (int64_t)strString.size(), 0, INT_MAX);
		*this << nLen;

		Write((char*)strString.data(), nLen);
		return *this;
	}
	CNArchive& operator << (const std::u16string& strString)
	{
		u32 nLen = (u32)CheckCompileRange("archive UTF-16 units", (int64_t)strString.size(), 0, INT_MAX / 2);
		*this << nLen;

		Write(strString.data(), static_cast<int>(nLen * sizeof(char16_t)));
		return *this;
	}


protected:

};

};

