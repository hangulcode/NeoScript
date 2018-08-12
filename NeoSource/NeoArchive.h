#pragma once 

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>

#include <windows.h>

class CNArchive
{
private:
	BYTE*	m_lpBufStart;
	int		m_iOffset;
	int		m_iSize;
public:
	CNArchive()
	{
		m_lpBufStart = NULL;
		m_iOffset = 0;
        m_iSize = 0;
	}
	CNArchive(void* pBuffer, int iBufferSize)
	{
		m_lpBufStart = (BYTE*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
	}
    void    SetData(void* pBuffer, int iBufferSize)
    {
		m_lpBufStart = (BYTE*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
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
	UINT Write(const void* lpBuf, int nMax)
	{
		if (nMax == 0)
			return 0;

		if(nMax + m_iOffset > m_iSize)
			return 0;
		memcpy(m_lpBufStart+m_iOffset, lpBuf, nMax);
		m_iOffset += nMax;
        return nMax;
	}
	void WriteCount(DWORD dwCount)
	{
		WORD wCount;
		if (dwCount < 0xFFFF)
		{
			wCount = (WORD)dwCount;
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

	UINT Read(void* lpBuf, int nMax)
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
		int nLen = strString.length();
		*this << nLen;

		Write((char*)strString.data(), nLen);
		return *this;
	}
	CNArchive& operator << (std::wstring& strString)
	{
		int nLen = strString.length();
		*this << nLen;

		Write((wchar_t*)strString.data(), nLen*sizeof(wchar_t));
		return *this;
	}


protected:

};
