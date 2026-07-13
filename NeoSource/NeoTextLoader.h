#pragma once

#include "NeoConfig.h"
#include "NeoVM.h" // NeoCompileDefines (스크립트 const 테이블 소유용)

namespace NeoScript
{

struct NeoGlobalSymbolTable;

#define FILE_UNICODE_HEADER_LE	u16(0xFEFF)
#define FILE_UNICODE_HEADER_BE	u16(0xFFFE)
#define FILE_UTF8_HEADER		u16(0xBBEF)

#define FILE_UTF8_SUB			(u8)(0xBF)


enum TK_TYPE
{
	TK_UNUSED,

	TK_NONE,
	TK_STRING,
	TK_VAR,
	TK_FUN,
	TK_CLASS,
	TK_IMPORT,
	TK_EXPORT,

	TK_TOSTRING,
	TK_TOINT,
	TK_TOFLOAT,
	TK_TOSIZE,
	TK_GETTYPE,
	TK_SLEEP,

	TK_RETURN,
	TK_BREAK,
	TK_CONTINUE,
	TK_IF,
	TK_ELSE,
	TK_ELSEIF,
	TK_FOR,
	TK_FOREACH,
	TK_WHILE,
	TK_TRUE,
	TK_FALSE,
	TK_NULL,

	TK_PLUS2, // ++
	TK_MINUS2, // --

	TK_PLUS, // +
	TK_PLUS_EQ, // +=
	TK_MINUS, // -
	TK_MINUS_EQ, // -=
	TK_MUL, // *
	TK_MUL_EQ, // *=
	TK_DIV, // /
	TK_DIV_EQ, // /=
	TK_PERCENT, // %
	TK_PERCENT_EQ, // %=
	TK_NOT,		// ~ TILDE
	TK_XOR,		// ^ CIRCUMFLEX
	TK_XOR_EQ,	// ^=
	TK_EQUAL, // =
	TK_EQUAL_EQ, // ==
	TK_EQUAL_NOT, // !=

	TK_LSHIFT, // <<
	TK_LSHIFT_EQ, // <<=
	TK_RSHIFT, // >>
	TK_RSHIFT_EQ, // >>=

	TK_AND,		// &
	TK_AND_EQ,	// &=
	TK_AND2,	// &&
	TK_OR,		// |
	TK_OR_EQ,	// |=
	TK_OR2,		// ||

	TK_L_SMALL, // (
	TK_R_SMALL, // )
	TK_L_MIDDLE, // {
	TK_R_MIDDLE, // }
	TK_L_ARRAY, // [
	TK_R_ARRAY, // ]

	TK_GREAT,		// >
	TK_GREAT_EQ,	// >=
	TK_LESS,		// <
	TK_LESS_EQ,		// <=

	TK_COLON, // :
	TK_SEMICOLON, // ;
	TK_COMMA, // ,
	TK_DOT, // .
	TK_DOT2, // ..
	TK_SHARP, // #
	TK_QUOTE2, // "
	TK_QUOTE1, // '
	TK_QUESTION, // ?
	TK_LOGIC_NOT, // !

	TK_YIELD,

	TK_CONST,			// const (스크립트 컴파일타임 상수 선언)
	TK_STRING_LITERAL,	// define/const 치환으로 만들어진 완성된 문자열 리터럴 (tk = 내용)

	TK_MAX
};

struct SToken
{
	TK_TYPE			_type;
	std::string		_tk;
};

class CArchiveRdWC
{
private:
	u16 *	m_lpBufStart;
	int		m_iOffset;
	int		m_iSize;

	u16		m_iCurLine;
	u16		m_iCurCol;
public:
	bool	_allowGlobalInitLogic = true;
	bool	_debug = false;

	int		_iTableDeep = 0;

	std::list<SToken> m_sTokenQueue;
	std::string m_sErrorString;
//	std::set<std::string> m_sImports; // Document Load State
	std::string	m_sModuleName;
	u16 m_iFileSeq = 0;
	std::vector<std::string>* m_pDebugSourceFiles = nullptr;
	const NeoCompileDefines* m_pDefines = nullptr;
	NeoCompileDefines m_sScriptDefines;      // 스크립트 내부 const 선언 (모듈-로컬, import 시 전파 안 함)
	bool m_bSuppressDefines = false;         // const 이름 토큰을 raw 로 읽기 위한 치환 억제 플래그
	const NeoGlobalSymbolTable* m_pGlobalSymbols = nullptr;

	CArchiveRdWC()
	{
		m_lpBufStart = NULL;
		m_iOffset = 0;
		m_iSize = 0;
		m_iCurLine = 1;
		m_iCurCol = 1;
	}
	CArchiveRdWC(u16* pBuffer, int iBufferSize)
	{
		m_lpBufStart = (u16*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
		m_iCurLine = 1;
		m_iCurCol = 1;
	}
	void    SetData(u16* pBuffer, int iBufferSize)
	{
		m_lpBufStart = (u16*)pBuffer;
		m_iOffset = 0;
		m_iSize = iBufferSize;
		m_iCurLine = 1;
		m_iCurCol = 1;
	}
	u16* GetBuffer() { return m_lpBufStart; }

	inline bool IsEOF()
	{
		return (m_iOffset >= m_iSize);
	}

	u16 GetData(bool offsetMove = true)
	{
		if (IsEOF())
			return 0;

		u16 r = m_lpBufStart[m_iOffset];
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

	u16		CurFile() { return m_iFileSeq; }
	u16		CurLine() { return m_iCurLine; }
	u16		CurCol() { return m_iCurCol; }


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

	void PushToken(TK_TYPE tk, const std::string& str)
	{
		SToken st;
		st._type = tk;
		st._tk = str;
		m_sTokenQueue.push_front(st);
	}

protected:

};


bool	ToArchiveRdWC(const char* pBuffer, int iBufferSize, CArchiveRdWC& ar);
void	OutAsm(const char* lpszString, ...);
void	OutString(const char* lpszString);
void	OutBytes(const u8* pBuffer, int iCount, int iMaxCount);

bool	StringToDouble(double& r, const char *p);
bool	StringToDoubleLow(double& r, const char *p);

};
