#pragma once 

#include "NeoConfig.h"

class CNArchive
{
private:
	u8*	m_lpBufStart;
	int		m_iOffset;
	int		m_iSize;
	int		m_iMaxSize;
	bool	m_bAlloc;
public:
	CNArchive(int iMaxSize = 1*1024*1024)
	{
		m_iMaxSize = iMaxSize;
		m_iOffset = 0;
		m_iSize = 10 * 1024; // 10k
		m_lpBufStart = new u8[m_iSize];
		m_bAlloc = true;
	}
	CNArchive(void* pBuffer, int iBufferSize, int iMaxSize = 1 * 1024 * 1024)
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

	inline int GetBufferOffset()
	{
		return m_iOffset;
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
		if (iSize <= 0)
			return false;
		if (iSize > m_iMaxSize)
			return false;

		m_iSize = iSize * 2;
		if (m_iSize > m_iMaxSize)
			m_iSize = m_iMaxSize;

		u8* pNew = new u8[m_iSize];
		if(m_iOffset > 0)
			memcpy(pNew, m_lpBufStart, m_iOffset);

		if (m_bAlloc)
			delete[] m_lpBufStart;

		m_lpBufStart = pNew;
		m_bAlloc = true;
		return true;
	}
	u32 Write(const void* lpBuf, int nMax)
	{
		if (nMax == 0)
			return 0;

		if (nMax + m_iOffset > m_iSize)
			if(false == Alloc(nMax + m_iOffset))
				return 0;

		memcpy(m_lpBufStart+m_iOffset, lpBuf, nMax);
		m_iOffset += nMax;
        return nMax;
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

		if (nMax + m_iOffset > m_iSize)
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
	CNArchive& operator << (const wchar_t* lpszString)
	{
		std::wstring str = lpszString;
		*this << str;

		return *this;
	}
	CNArchive& operator << (std::string& strString)
	{
		int nLen = (int)strString.length();
		*this << nLen;

		Write((char*)strString.data(), nLen);
		return *this;
	}
	CNArchive& operator << (std::wstring& strString)
	{
		int nLen = (int)strString.length();
		*this << nLen;

		Write((wchar_t*)strString.data(), nLen*sizeof(wchar_t));
		return *this;
	}


protected:

};
