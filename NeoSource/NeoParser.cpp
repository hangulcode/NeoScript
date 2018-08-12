// Neo1.00.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "NeoParser.h"
#include "NeoVM.h"
#include "NeoTextLoader.h"
#include "NeoExport.h"

void	DebugLog(LPCSTR	lpszString, ...);

template<class T>
static void RemoveAt(std::vector<T>& array, int index)
{
	int cur = 0;
	for (auto it = array.begin(); it != array.end(); it++, cur++)
	{
		if (cur == index)
		{
			array.erase(it);
			break;
		}
	}
}

struct SOperand
{
	int _iVar;
	int _iArrayIndex;

	SOperand()
	{
		Reset();
	}
	SOperand(int iVar, int iArrayIndex = INVALID_ERROR_PARSEJOB)
	{
		_iVar = iVar;
		_iArrayIndex = iArrayIndex;
	}
	void Reset()
	{
		_iVar = _iArrayIndex = INVALID_ERROR_PARSEJOB;
	}
};


enum TK_TYPE
{
	TK_UNUSED,

	TK_NONE,
	TK_STRING,
	TK_VAR,
	TK_FUN,
	TK_IMPORT,
	TK_EXPORT,

	TK_TOSTRING,
	TK_TOINT,
	TK_TOFLOAT,
	TK_GETTYPE,

	TK_RETURN,
	TK_BREAK,
	TK_IF,
	TK_ELSE,
	TK_FOR,
	TK_TRUE,
	TK_FALSE,

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
	TK_TILDE, // ~
	TK_CIRCUMFLEX, // ^
	TK_EQUAL, // =
	TK_EQUAL_EQ, // ==
	TK_EQUAL_NOT, // !=

	TK_AND, // &
	TK_AND2, // &&
	TK_OR, // |
	TK_OR2, // ||

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
	TK_QUOTATION, // "
	TK_QUESTION, // ?
	TK_NOT, // !
};

std::map<TK_TYPE, std::string> g_sDefaultTokenString;

void InitDefaultTokenString()
{
	g_sDefaultTokenString.clear();

	g_sDefaultTokenString[TK_UNUSED] = "unused token";

	g_sDefaultTokenString[TK_NONE] = "none token";
	g_sDefaultTokenString[TK_STRING] = "string token";
	g_sDefaultTokenString[TK_VAR] = "var";
	g_sDefaultTokenString[TK_FUN] = "fun";
	g_sDefaultTokenString[TK_IMPORT] = "import";
	g_sDefaultTokenString[TK_EXPORT] = "export";

	g_sDefaultTokenString[TK_TOSTRING] = "tostring";
	g_sDefaultTokenString[TK_TOINT] = "toint";
	g_sDefaultTokenString[TK_TOFLOAT] = "tofloat";
	g_sDefaultTokenString[TK_GETTYPE] = "type";

	g_sDefaultTokenString[TK_RETURN] = "return";
	g_sDefaultTokenString[TK_BREAK] = "break";
	g_sDefaultTokenString[TK_IF] = "if";
	g_sDefaultTokenString[TK_ELSE] = "else";
	g_sDefaultTokenString[TK_FOR] = "for";
	g_sDefaultTokenString[TK_TRUE] = "true";
	g_sDefaultTokenString[TK_FALSE] = "false";

	g_sDefaultTokenString[TK_PLUS2] = "++";
	g_sDefaultTokenString[TK_MINUS2] = "--";

	g_sDefaultTokenString[TK_PLUS] = "+"; // +
	g_sDefaultTokenString[TK_PLUS_EQ] = "+="; // +=
	g_sDefaultTokenString[TK_MINUS] = "-"; // -
	g_sDefaultTokenString[TK_MINUS_EQ] = "-="; // -=
	g_sDefaultTokenString[TK_MUL] = "*"; // *
	g_sDefaultTokenString[TK_MUL_EQ] = "*="; // *=
	g_sDefaultTokenString[TK_DIV] = "/"; // /
	g_sDefaultTokenString[TK_DIV_EQ] = "/="; // /=
	g_sDefaultTokenString[TK_PERCENT] = "%"; // %
	g_sDefaultTokenString[TK_TILDE] = "~"; // ~
	g_sDefaultTokenString[TK_CIRCUMFLEX] = "^"; // ^
	g_sDefaultTokenString[TK_EQUAL] = "="; // =
	g_sDefaultTokenString[TK_EQUAL_EQ] = "=="; // ==
	g_sDefaultTokenString[TK_EQUAL_NOT] = "!="; // !=

	g_sDefaultTokenString[TK_AND] = "&"; // &
	g_sDefaultTokenString[TK_AND2] = "&&"; // &&
	g_sDefaultTokenString[TK_OR] = "|"; // |
	g_sDefaultTokenString[TK_OR2] = "||"; // ||

	g_sDefaultTokenString[TK_L_SMALL] = "("; // (
	g_sDefaultTokenString[TK_R_SMALL] = ")"; // )
	g_sDefaultTokenString[TK_L_MIDDLE] = "{"; // {
	g_sDefaultTokenString[TK_R_MIDDLE] = "}"; // }
	g_sDefaultTokenString[TK_L_ARRAY] = "["; // [
	g_sDefaultTokenString[TK_R_ARRAY] = "]"; // ]

	g_sDefaultTokenString[TK_GREAT] = ">";		// >
	g_sDefaultTokenString[TK_GREAT_EQ] = ">=";	// >=
	g_sDefaultTokenString[TK_LESS] = "<";		// <
	g_sDefaultTokenString[TK_LESS_EQ] = "<=";	// <=

	g_sDefaultTokenString[TK_COLON] = ":"; // :
	g_sDefaultTokenString[TK_SEMICOLON] = ";"; // ;
	g_sDefaultTokenString[TK_COMMA] = ","; // ,
	g_sDefaultTokenString[TK_DOT] = "."; // .
	g_sDefaultTokenString[TK_DOT2] = ".."; // .
	g_sDefaultTokenString[TK_SHARP] = "#"; // #
	g_sDefaultTokenString[TK_QUOTATION] = "\""; // "
	g_sDefaultTokenString[TK_QUESTION] = "?"; // ?
	g_sDefaultTokenString[TK_NOT] = "!"; // !
}
std::string GetTokenString(TK_TYPE tk)
{
	if (g_sDefaultTokenString.empty())
		InitDefaultTokenString();

	auto it = g_sDefaultTokenString.find(tk);
	if (it == g_sDefaultTokenString.end())
		return "";
	return (*it).second;
}

#define GLOBAL_INIT_FUN_NAME	"##_global_##"
BOOL ParseFunction(CArchiveRdWC& ar, SFunctions& funs, SVars& vars);


WORD GetNextChar(CArchiveRdWC& ar)
{
	return ar.GetData(false);
}

void SkipCurrentLine(CArchiveRdWC& ar)
{
	while (true)
	{
		WORD c = ar.GetData(true);
		switch (c)
		{
		case '\n':
			ar.AddLine();
		case 0:
			return;
		default:
			break;
		}
	}
}

void SkipMultiLine(CArchiveRdWC& ar)
{
	WORD c1, c2;
	while (true)
	{
		c1 = ar.GetData(true);
		switch (c1)
		{
		case '\n':
			ar.AddLine();
			break;
		case 0:
			return;
		case '*':
			c2 = ar.GetData(false);
			if (c2 == '/')
			{
				ar.GetData(true);
				return;
			}
			break;
		default:
			break;
		}
	}
}

TK_TYPE CalcStringToken(std::string& tk)
{
	if (true == tk.empty())
		return TK_NONE;

	if (tk == "return")
		return TK_RETURN;
	else if (tk == "break")
		return TK_BREAK;
	else if (tk == "if")
		return TK_IF;
	else if (tk == "else")
		return TK_ELSE;
	else if (tk == "for")
		return TK_FOR;
	else if (tk == "true")
		return TK_TRUE;
	else if (tk == "false")
		return TK_FALSE;
	else if (tk == "var")
		return TK_VAR;
	else if (tk == "fun")
		return TK_FUN;
	else if (tk == "import")
		return TK_IMPORT;
	else if (tk == "export")
		return TK_EXPORT;
	else if (tk == "tostring")
		return TK_TOSTRING;
	else if (tk == "toint")
		return TK_TOINT;
	else if (tk == "tofloat")
		return TK_TOFLOAT;
	else if (tk == "type")
		return TK_GETTYPE;

	return TK_STRING;
}


TK_TYPE CalcToken2(TK_TYPE tkTypeOnySingleChar, CArchiveRdWC& ar, std::string& tk)
{
	if (false == tk.empty())
		return CalcStringToken(tk);

	ar.GetData(true);

	WORD c = ar.GetData(false);
	if (tkTypeOnySingleChar == TK_LESS && c == '=')
	{
		ar.GetData(true);
		return TK_LESS_EQ;
	}
	else if (tkTypeOnySingleChar == TK_GREAT && c == '=')
	{
		ar.GetData(true);
		return TK_GREAT_EQ;
	}
	else if (tkTypeOnySingleChar == TK_EQUAL && c == '=')
	{
		ar.GetData(true);
		return TK_EQUAL_EQ;
	}
	else if (tkTypeOnySingleChar == TK_NOT && c == '=')
	{
		ar.GetData(true);
		return TK_EQUAL_NOT;
	}
	else if (tkTypeOnySingleChar == TK_PLUS && c == '=')
	{
		ar.GetData(true);
		return TK_PLUS_EQ;
	}
	else if (tkTypeOnySingleChar == TK_MINUS && c == '=')
	{
		ar.GetData(true);
		return TK_MINUS_EQ;
	}
	else if (tkTypeOnySingleChar == TK_MUL && c == '=')
	{
		ar.GetData(true);
		return TK_MUL_EQ;
	}
	else if (tkTypeOnySingleChar == TK_DIV && c == '=')
	{
		ar.GetData(true);
		return TK_DIV_EQ;
	}
	else if (tkTypeOnySingleChar == TK_PLUS && c == '+')
	{
		ar.GetData(true);
		return TK_PLUS2;
	}
	else if (tkTypeOnySingleChar == TK_MINUS && c == '-')
	{
		ar.GetData(true);
		return TK_MINUS2;
	}
	else if (tkTypeOnySingleChar == TK_AND && c == '&')
	{
		ar.GetData(true);
		return TK_AND2;
	}
	else if (tkTypeOnySingleChar == TK_OR && c == '|')
	{
		ar.GetData(true);
		return TK_OR2;
	}
	else if (tkTypeOnySingleChar == TK_DOT && c == '.')
	{
		ar.GetData(true);
		return TK_DOT2;
	}
	return tkTypeOnySingleChar;
}
TK_TYPE CalcToken(TK_TYPE tkTypeOnySingleChar, CArchiveRdWC& ar, std::string& tk)
{
	auto r = CalcToken2(tkTypeOnySingleChar, ar, tk);
	if (tk.empty())
		tk = GetTokenString(r);
	return r;
}

NOP_TYPE TokenToOP(TK_TYPE tk, int& iPriority)
{
	switch (tk)
	{
	case TK_PLUS:
		iPriority = 5;
		return NOP_ADD3;
	case TK_MINUS:
		iPriority = 5;
		return NOP_SUB3;
	case TK_MUL:
		iPriority = 4;
		return NOP_MUL3;
	case TK_DIV:
		iPriority = 4;
		return NOP_DIV3;

	case TK_EQUAL:
		iPriority = 15;
		return NOP_MOV;

	case TK_PLUS_EQ:
		iPriority = 15;
		return NOP_ADD2;
	case TK_MINUS_EQ:
		iPriority = 15;
		return NOP_SUB2;
	case TK_MUL_EQ:
		iPriority = 15;
		return NOP_MUL2;
	case TK_DIV_EQ:
		iPriority = 15;
		return NOP_DIV2;

	case TK_PLUS2:
		iPriority = 20;
		return NOP_INC;
	case TK_MINUS2:
		iPriority = 20;
		return NOP_DEC;

	case TK_GREAT:		// >
		iPriority = 7;
		return NOP_GREAT;
	case TK_GREAT_EQ:	// >=
		iPriority = 7;
		return NOP_GREAT_EQ;
	case TK_LESS:		// <
		iPriority = 7;
		return NOP_LESS;
	case TK_LESS_EQ:	// <=
		iPriority = 7;
		return NOP_LESS_EQ;

	case TK_EQUAL_EQ:	// ==
		iPriority = 8;
		return NOP_EQUAL2;
	case TK_EQUAL_NOT:	// !=
		iPriority = 8;
		return NOP_NEQUAL;

	case TK_AND2:	// &&
		iPriority = 12;
		return NOP_AND;
	case TK_OR2:	// ||
		iPriority = 13;
		return NOP_OR;

	case TK_DOT2:	// ..
		iPriority = 6; //
		return NOP_STR_ADD;

	case TK_TOSTRING:
		iPriority = 20;
		return NOP_TOSTRING;
	case TK_TOINT:
		iPriority = 20;
		return NOP_TOINT;
	case TK_TOFLOAT:
		iPriority = 20;
		return NOP_TOFLOAT;
	case TK_GETTYPE:
		iPriority = 20;
		return NOP_GETTYPE;
	}
	iPriority = 20;
	return NOP_NONE;
}

BOOL GetQuotationString(CArchiveRdWC& ar, std::string& str)
{
	str.clear();
	bool blEnd = false;
	while (blEnd == false)
	{
		WORD c1 = ar.GetData(true);
		if (c1 == NULL)
			return FALSE;
		if (c1 == '\n')
			return FALSE;
		if (c1 == '"')
			break;
		if (c1 == '\\')
		{
			WORD c2 = ar.GetData(false);
			switch (c2)
			{
			case 'n':
				c1 = '\n';
				ar.GetData(true);
				break;
			case 'r':
				c1 = '\r';
				ar.GetData(true);
				break;
			case 't':
				c1 = '\t';
				ar.GetData(true);
				break;
			}
		}

		str.push_back((char)c1);
	}
	return TRUE;
}
static bool IsDotChar(WORD c)
{
	if ('a' <= c && c <= 'z')
		return true;
	if ('A' <= c && c <= 'Z')
		return true;
	if ('0' <= c && c <= '9')
		return true;
	switch (c)
	{
	case '_':
		return true;
	}
	return false;
}
BOOL GetDotString(CArchiveRdWC& ar, std::string& str)
{
	str.clear();
	bool blEnd = false;
	while (blEnd == false)
	{
		WORD c1 = ar.GetData(false);
		if (false == IsDotChar(c1))
			break;

		ar.GetData(true);
		str.push_back((char)c1);
	}
	return TRUE;
}
struct SToken
{
	TK_TYPE			_type;
	std::string		_tk;
};

std::list<SToken> g_sTokenQueue;

void PushToken(TK_TYPE tk, const std::string& str)
{
	SToken st;
	st._type = tk;
	st._tk = str;
	g_sTokenQueue.push_front(st);
}


TK_TYPE GetToken(CArchiveRdWC& ar, std::string& tk)
{
	if (false == g_sTokenQueue.empty())
	{
		SToken d = *g_sTokenQueue.begin();
		g_sTokenQueue.pop_front();
		tk = d._tk;
		return d._type;
	}

	if (ar.IsEOF())
		return TK_NONE;

	tk.clear();

	bool blEnd = false;
	while (blEnd == false)
	{
		WORD c1 = ar.GetData(false);
		switch (c1)
		{
		case 0:
			return CalcToken(TK_NONE, ar, tk);

		case '\n':
			if (false == tk.empty())
				return CalcStringToken(tk);
			ar.GetData(true);
			ar.AddLine();
			break;
		case ' ':
		case '\r':
		case '\t':
			if (false == tk.empty())
				return CalcStringToken(tk);
			ar.GetData(true);
			break;

		case '+':
			return CalcToken(TK_PLUS, ar, tk);
		case '-':
			return CalcToken(TK_MINUS, ar, tk);
		case '*':
			return CalcToken(TK_MUL, ar, tk);
		case '%':
			return CalcToken(TK_PERCENT, ar, tk);
		case '~':
			return CalcToken(TK_TILDE, ar, tk);
		case '^':
			return CalcToken(TK_CIRCUMFLEX, ar, tk);
		case '=':
			return CalcToken(TK_EQUAL, ar, tk);
		case '&':
			return CalcToken(TK_AND, ar, tk);
		case '|':
			return CalcToken(TK_OR, ar, tk);

		case '(':
			return CalcToken(TK_L_SMALL, ar, tk);
		case ')':
			return CalcToken(TK_R_SMALL, ar, tk);
		case '{':
			return CalcToken(TK_L_MIDDLE, ar, tk);
		case '}':
			return CalcToken(TK_R_MIDDLE, ar, tk);
		case '[':
			return CalcToken(TK_L_ARRAY, ar, tk);
		case ']':
			return CalcToken(TK_R_ARRAY, ar, tk);
		case '<':
			return CalcToken(TK_LESS, ar, tk);
		case '>':
			return CalcToken(TK_GREAT, ar, tk);

		case ':':
			return CalcToken(TK_COLON, ar, tk);
		case ';':
			return CalcToken(TK_SEMICOLON, ar, tk);
		case ',':
			return CalcToken(TK_COMMA, ar, tk);
		case '.':
			return CalcToken(TK_DOT, ar, tk);
		case '#':
			return CalcToken(TK_SHARP, ar, tk);
		case '"':
			return CalcToken(TK_QUOTATION, ar, tk);
		case '?':
			return CalcToken(TK_QUESTION, ar, tk);
		case '!':
			return CalcToken(TK_NOT, ar, tk);

		case '/':
		{
			ar.GetData(true);
			WORD c2 = ar.GetData(false);
			if (c2 == '/')	// '//'
				SkipCurrentLine(ar);
			else if (c2 == '*')	// '/*'
			{
				ar.GetData(true);
				SkipMultiLine(ar);
			}
			else
			{
				return TK_DIV;
			}
			break;
		}
		default:
			tk.push_back((char)c1);
			ar.GetData(true);
			break;
		}
	}

	return (tk.size() != 0) ? TK_STRING : TK_NONE;
}

BOOL ParseFunctionArg(CArchiveRdWC& ar, SFunctions& funs, SLayerVar* pCurLayer)
{
	std::string tk1, tk2;

	bool bPreviusComa = false;
	TK_TYPE tkType1, tkType2;
	bool blEnd = false;
	while (blEnd == false)
	{
		tkType1 = GetToken(ar, tk1);
		switch (tkType1)
		{
		case TK_VAR:
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_STRING)
			{
				auto it = funs._cur._args.find(tk2);
				if (it != funs._cur._args.end())
				{
					DebugLog("Error (%d, %d): Already Function Arg (%s)", ar.CurLine(), ar.CurCol(), tk2.c_str());
					return FALSE;
				}
				pCurLayer->AddLocalVar(tk2, 1 + funs._cur._args.size());
				funs._cur._args.insert(tk2);
				bPreviusComa = false;
			}
			else
			{
				DebugLog("Error (%d, %d): Function Arg (%d)", ar.CurLine(), ar.CurCol(), tk1.c_str());
				return FALSE;
			}
			break;
		case TK_R_SMALL:
			if (bPreviusComa)
			{
				DebugLog("Error (%d, %d): Function Arg (Coma)", ar.CurLine(), ar.CurCol());
				return false;
			}
			blEnd = true;
			break;
		case TK_COMMA:
			if (funs._cur._args.size() == 0 || bPreviusComa)
			{
				DebugLog("Error (%d, %d): Function Arg (Coma)", ar.CurLine(), ar.CurCol());
				return false;
			}
			bPreviusComa = !bPreviusComa;
			break;
		default:
			DebugLog("Error (%d, %d): Function Arg (%s)", ar.CurLine(), ar.CurCol(), tk1.c_str());
			return false;
		}
	}
	return TRUE;
}

void AddLocalVar(SLayerVar* pCurLayer)
{
	SLocalVar* pCurLocal = new SLocalVar();
	pCurLayer->_varsLayer.push_back(pCurLocal);
}

void DelLocalVar(SLayerVar* pCurLayer)
{
	int sz = pCurLayer->_varsLayer.size();
	if (sz > 0)
	{
		SLocalVar* pCurLocal = pCurLayer->_varsLayer[sz - 1];
		delete pCurLocal;
		pCurLayer->_varsLayer.resize(sz - 1);
	}
}

SLayerVar* AddVarsFunction(SVars& vars)
{
	SLayerVar* pCurLayer = new SLayerVar();
	vars._varsFunction.push_back(pCurLayer);

	SLocalVar* pCurLocal = new SLocalVar();
	pCurLayer->_varsLayer.push_back(pCurLocal);

	return pCurLayer;
}

SLayerVar* DelVarsFunction(SVars& vars)
{
	int cnt = vars._varsFunction.size();
	if (cnt > 0)
	{
		cnt -= 1;
		SLayerVar* p = vars._varsFunction[cnt];
		delete p;
		vars._varsFunction.resize(cnt);

		if(cnt > 0)
			return vars._varsFunction[cnt - 1];
	}
	return NULL;
}

TK_TYPE ParseJob(BOOL bReqReturn, SOperand& sResultStack, std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, BOOL bAllowVarDef = FALSE);
BOOL ParseVarDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars);
BOOL ParseMiddleArea(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars);
BOOL IsTempVar(int iVar)
{
	if (COMPILE_LOCALTMP_VAR_BEGIN <= iVar && iVar < COMPILE_STATIC_VAR_BEGIN)
		return TRUE;

	return FALSE;
}

BOOL ParseFunCall(SOperand& iResultStack, TK_TYPE tkTypePre, SFunctionInfo* pFun, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;


	SOperand iTempVar;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_L_SMALL)
	{
		int iParamCount = 0;
		while (true)
		{
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_R_SMALL)
				break;

			PushToken(tkType2, tk2);

			iTempVar.Reset();
			TK_TYPE r2 = ParseJob(TRUE, iTempVar, NULL, ar, funs, vars);
			if(iTempVar._iArrayIndex == INVALID_ERROR_PARSEJOB)
				funs._cur.Push_MOV(NOP_MOV, COMPILE_CALLARG_VAR_BEGIN + 1 + iParamCount, iTempVar._iVar);
			else
				funs._cur.Push_TableRead(iTempVar._iVar, iTempVar._iArrayIndex, COMPILE_CALLARG_VAR_BEGIN + 1 + iParamCount);
			iParamCount++;

			if (iTempVar._iVar == INVALID_ERROR_PARSEJOB)
			{
				DebugLog("Error (%d, %d): Call Param\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}

			if (r2 != TK_R_SMALL && r2 != TK_COMMA)
			{
				DebugLog("Error (%d, %d): Call Param\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			if (r2 == TK_R_SMALL)
				break;
		}
		if(pFun != NULL)
			funs._cur.Push_Call(pFun->_funType == FUNT_IMPORT ? NOP_FARCALL : NOP_CALL, pFun->_funID, iParamCount);
		else
			funs._cur.Push_CallPtr(iResultStack._iVar, iResultStack._iArrayIndex, iParamCount);
		iResultStack = funs._cur.AllocLocalTempVar();
		if(tkTypePre != TK_MINUS)
			funs._cur.Push_MOV(NOP_MOV, iResultStack._iVar, STACK_POS_RETURN);
		else
			funs._cur.Push_MOV(NOP_MOV_MINUS, iResultStack._iVar, STACK_POS_RETURN);
	}
	else
	{
		DebugLog("Error (%d, %d): ( Not Found\n", ar.CurLine(), ar.CurCol());
		return FALSE;
	}
	return TRUE;
}

BOOL ParseNum(int& iResultStack, TK_TYPE tkTypePre, std::string& tk1, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk2;
	TK_TYPE tkType2;

	double num;

	if (TRUE == StringToDouble(num, tk1.c_str()))
	{
		WORD c = ar.GetData(false);
		if (c == '.')
		{
			ar.GetData(true);

			tkType2 = GetToken(ar, tk2);
			double num2 = 0;
			if (TRUE == StringToDoubleLow(num2, tk2.c_str()))
			{
				num += num2;
			}
			else
			{
				DebugLog("Error (%d, %d): num data invalid\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			if (tkTypePre == TK_MINUS)
				num = -num;
			iResultStack = funs.AddStaticNum(num);
		}
		else
		{
			if (tkTypePre == TK_MINUS)
				num = -num;
			iResultStack = funs.AddStaticInt((int)num);
		}
	}
	else
	{
		DebugLog("Error (%d, %d): Unknown String (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return FALSE;
	}
	return TRUE;
}

TK_TYPE ParseTableDef(int& iResultStack, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r = TK_NONE;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_R_MIDDLE)
	{
		DebugLog("Error (%d, %d): Table } \n", ar.CurLine(), ar.CurCol());
		return TK_NONE;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_SEMICOLON)
	{
		DebugLog("Error (%d, %d): Table ; \n", ar.CurLine(), ar.CurCol());
		return TK_NONE;
	}
	
	iResultStack = funs._cur.AllocLocalTempVar();
	funs._cur.Push_TableAlloc(iResultStack);
	return tkType1;
}
TK_TYPE ParseTable(int& iArrayOffset, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	TK_TYPE r = TK_NONE;

	SOperand operand;
	r = ParseJob(TRUE, operand, NULL, ar, funs, vars);
	if (r != TK_R_ARRAY)
	{
		DebugLog("Error (%d, %d): Table ] \n", ar.CurLine(), ar.CurCol());
		return TK_NONE;
	}

	if (operand._iArrayIndex == INVALID_ERROR_PARSEJOB)
		iArrayOffset = operand._iVar;
	else
	{
		iArrayOffset = funs._cur.AllocLocalTempVar();
		funs._cur.Push_TableRead(operand._iVar, operand._iArrayIndex, iArrayOffset);
	}
	return r;
}

BOOL ParseString(SOperand& operand, TK_TYPE tkTypePre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
	TK_TYPE r = TK_NONE;

	SOperand iTempOffset;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING)
	{
		DebugLog("Error (%d, %d): String", ar.CurLine(), ar.CurCol());
		return FALSE;
	}

	int iArrayIndex = INVALID_ERROR_PARSEJOB;
	if (tk1 == "system")
		iTempOffset._iVar = COMPILE_STATIC_VAR_BEGIN;
	else
		iTempOffset._iVar = vars.FindVar(tk1);

	if (iTempOffset._iVar >= 0)
	{
		while (true)
		{
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_L_ARRAY || tkType2 == TK_DOT)
			{
			}
			else if (tkType2 == TK_L_SMALL)
			{
				PushToken(tkType2, tk2);

				//int iTempOffset2 = funs._cur.AllocLocalTempVar();
				//funs._cur.Push_TableRead(iTempOffset._iVar, iArrayIndex, iTempOffset2);
				//iTempOffset = iTempOffset2;

				//iArrayIndex = INVALID_ERROR_PARSEJOB;
				iTempOffset._iArrayIndex = iArrayIndex;
				iArrayIndex = INVALID_ERROR_PARSEJOB;

				if (FALSE == ParseFunCall(iTempOffset, tkTypePre, NULL, ar, funs, vars))
					return FALSE;
				break;
			}
			else
			{
				PushToken(tkType2, tk2);
				break;
			}
			if (iArrayIndex != INVALID_ERROR_PARSEJOB)
			{
				int iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(iTempOffset._iVar, iArrayIndex, iTempOffset2);
				iTempOffset = iTempOffset2;
			}

			iArrayIndex = INVALID_ERROR_PARSEJOB;
			if (tkType2 == TK_L_ARRAY)
				r = ParseTable(iArrayIndex, ar, funs, vars);
			else
			{
				std::string str;
				if (FALSE == GetDotString(ar, str))
				{
					DebugLog("Error (%d, %d): . string", ar.CurLine(), ar.CurCol());
					return FALSE;
				}
				iArrayIndex = funs.AddStaticString(str);
			}
		}
		if (tkTypePre == TK_MINUS)
		{
			if (iArrayIndex != INVALID_ERROR_PARSEJOB)
			{
				int iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(iTempOffset._iVar, iArrayIndex, iTempOffset2);
				iTempOffset = iTempOffset2;

				iArrayIndex = INVALID_ERROR_PARSEJOB;
			}

			int iTempOffset3 = funs._cur.AllocLocalTempVar();
			funs._cur.Push_MOV(NOP_MOV_MINUS, iTempOffset3, iTempOffset._iVar);
			iTempOffset = iTempOffset3;
		}
	}
	else
	{
		SFunctionInfo* pFun = funs.FindFun(tk1);
		if (pFun != NULL)
		{
			if (FALSE == ParseFunCall(iTempOffset, tkTypePre, pFun, ar, funs, vars))
				return FALSE;
		}
		else
		{
			if (FALSE == ParseNum(iTempOffset._iVar, tkTypePre, tk1, ar, funs, vars))
				return FALSE;
		}
	}
	operand = SOperand(iTempOffset._iVar, iArrayIndex);
	return TRUE;
}

BOOL ParseToType(int& iResultStack, TK_TYPE tkTypePre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r = TK_NONE;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL)
	{
		DebugLog("Error (%d, %d): type begin (", ar.CurLine(), ar.CurCol());
		return FALSE;
	}

	SOperand operand;
	r = ParseJob(TRUE, operand, NULL, ar, funs, vars);
	if (r != TK_R_SMALL)
	{
		DebugLog("Error (%d, %d): type end )", ar.CurLine(), ar.CurCol());
		return TK_NONE;
	}

	int iTempOffset2;
	if (operand._iArrayIndex != INVALID_ERROR_PARSEJOB)
	{
		iTempOffset2 = funs._cur.AllocLocalTempVar();
		funs._cur.Push_TableRead(operand._iVar, operand._iArrayIndex, iTempOffset2);
	}
	else
		iTempOffset2 = operand._iVar;

	int iTempOffset3 = funs._cur.AllocLocalTempVar();
	int iPriority = 0;
	funs._cur.Push_ToType(TokenToOP(tkTypePre, iPriority), iTempOffset3, iTempOffset2);
	iResultStack = iTempOffset3;
	return TRUE;
}

TK_TYPE ParseJob(BOOL bReqReturn, SOperand& sResultStack, std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, BOOL bAllowVarDef)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r = TK_NONE;

	SOperand iCurrentStack = INVALID_ERROR_PARSEJOB;
	SOperand iTempOffset;
	int iArrayIndex = INVALID_ERROR_PARSEJOB;

	std::vector<SOperand> operands;
	std::vector<TK_TYPE> operators;
	bool blApperOperator = false;

	bool blEnd = false;
	while (blEnd == false)
	{
		iTempOffset.Reset();
		iArrayIndex = INVALID_ERROR_PARSEJOB;

		tkType1 = GetToken(ar, tk1);
		switch (tkType1)
		{
		case TK_NONE:
			DebugLog("Error (%d, %d): Function End (%s)\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str());
			return TK_NONE;
		case TK_RETURN:
			if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
			{
				DebugLog("Error (%d, %d): Global Init (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
				return TK_NONE;
			}
			iTempOffset.Reset();
			r = ParseJob(TRUE, iTempOffset, NULL, ar, funs, vars);
			if (r != TK_SEMICOLON)
			{
				DebugLog("Error (%d, %d): return", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_RETURN(iTempOffset._iVar);
			blEnd = TRUE;
			break;
		case TK_SEMICOLON:	// ;
		case TK_COMMA:		// ,
		case TK_R_SMALL:	// )
		case TK_R_ARRAY:	// ]
			r = tkType1;
			blEnd = TRUE;
			break;
		case TK_L_SMALL:
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(bReqReturn, iTempOffset, NULL, ar, funs, vars);
			if (TK_R_SMALL != r)
			{
				DebugLog("Error (%d, %d): )\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		case TK_STRING:
			PushToken(tkType1, tk1);
			if (FALSE == ParseString(iTempOffset, TK_NONE, ar, funs, vars))
			{
				return TK_NONE;
			}

			operands.push_back(iTempOffset);
			blApperOperator = true;
			break;
		case TK_QUOTATION:
		{
			std::string str;
			if (FALSE == GetQuotationString(ar, str))
			{
				DebugLog("Error (%d, %d): String End Not Found\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			iTempOffset = funs.AddStaticString(str);
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		}
		case TK_PLUS:		// +
		case TK_MINUS:		// -
			if (blApperOperator == false)
			{
				SOperand a;
				if (FALSE == ParseString(a, tkType1, ar, funs, vars))
				{
					return TK_NONE;
				}

				//funs._cur.Push_IncDec(tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				operands.push_back(a);
				blApperOperator = true;
				break;
			}
			operators.push_back(tkType1);
			blApperOperator = false;
			break;
		case TK_MUL:		// *
		case TK_DIV:		// /
		case TK_GREAT:		// >
		case TK_GREAT_EQ:	// >=
		case TK_LESS:		// <
		case TK_LESS_EQ:	// <=
		case TK_EQUAL_EQ:	// ==
		case TK_EQUAL_NOT:	// !=
		case TK_AND2:		// &&
		case TK_OR2:		// ||
		case TK_DOT2:		// ..
		{
			if (blApperOperator == false)
			{
				DebugLog("Error (%d, %d): Invalide Operator", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			operators.push_back(tkType1);
			blApperOperator = false;
			break;
		}
		case TK_EQUAL:		// =
		case TK_PLUS_EQ:	// +=
		case TK_MINUS_EQ:	// -=
		case TK_MUL_EQ:		// *=
		case TK_DIV_EQ:		// /=
		{
			SOperand iTempVar;
			r = ParseJob(TRUE, iTempVar, NULL, ar, funs, vars);
			if (TK_NONE == r)
				return TK_NONE;
			if (iTempVar._iVar == INVALID_ERROR_PARSEJOB)
			{
				DebugLog("Error (%d, %d): = \n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			operators.push_back(tkType1);

			operands.push_back(SOperand(iTempVar));
			blApperOperator = true;
			blEnd = TRUE;
			break;
		}
		case TK_VAR:
			if (bAllowVarDef)
			{
				if (FALSE == ParseVarDef(ar, funs, vars))
					return TK_NONE;
				return TK_SEMICOLON; // ㅡㅡ;
			}
			else
			{
				DebugLog("Error (%d, %d): Syntex Var Def Error\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			break;
		case TK_BREAK:
			if(pJumps == NULL)
			{
				DebugLog("Error (%d, %d): break error\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_JMP(0);
			pJumps->push_back(SJumpValue(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset()));
			break;
		case TK_L_MIDDLE:
			iTempOffset.Reset();
			r = ParseTableDef(iTempOffset._iVar, ar, funs, vars);
			operands.push_back(iTempOffset);
			blApperOperator = true;
			blEnd = TRUE;
			break;

		case TK_TRUE:
		case TK_FALSE:
			iTempOffset._iVar = funs.AddStaticBool(tkType1 == TK_TRUE);
			operands.push_back(SOperand(iTempOffset._iVar));
			blApperOperator = true;
			break;
		case TK_TOSTRING:
		case TK_TOINT:
		case TK_TOFLOAT:
		case TK_GETTYPE:
			iTempOffset = INVALID_ERROR_PARSEJOB;
			if (FALSE == ParseToType(iTempOffset._iVar, tkType1, ar, funs, vars))
				return TK_NONE;
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		case TK_PLUS2: // ++
		case TK_MINUS2: // --
			if (blApperOperator == false)
			{	// 전위
				SOperand a;
				if (FALSE == ParseString(a, TK_NONE, ar, funs, vars))
				{
					return TK_NONE;
				}

				funs._cur.Push_IncDec(tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				operands.push_back(a);
				blApperOperator = true;
			}
			else
			{	// 후위
				SOperand& a = operands[operands.size() - 1];
				if (IsTempVar(a._iVar))
				{
					DebugLog("Error (%d, %d): Temp Var not Support (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
					return TK_NONE;
				}
				if (a._iArrayIndex != INVALID_ERROR_PARSEJOB)
				{
					DebugLog("Error (%d, %d): Table Var not Support (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
					return TK_NONE;
				}
				int iTempOffset2;
				if (bReqReturn)
				{
					iTempOffset2 = funs._cur.AllocLocalTempVar();
					funs._cur.Push_MOV(NOP_MOV, iTempOffset2, a._iVar);
				}
				funs._cur.Push_IncDec(tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				if (bReqReturn)
					a = SOperand(iTempOffset2);
			}
			break;
		default:
			DebugLog("Error (%d, %d): Syntex (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
			return TK_NONE;
		}
	}
	if (operands.empty() == true)
		return r;

	if (operands.size() != operators.size() + 1)
	{
		DebugLog("Error (%d, %d): Invalid Operator", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return TK_NONE;
	}
	while (operators.empty() == false)
	{
		int iTempPriority;
		int iFindOffset = -1;
		int iFindPriority = 100;
		for (int i = 0; i < (int)operators.size(); i++)
		{
			TokenToOP(operators[i], iTempPriority);
			if (i == 0 || iFindPriority > iTempPriority)
			{
				iFindPriority = iTempPriority;
				iFindOffset = i;
			}
		}
		auto op = TokenToOP(operators[iFindOffset], iTempPriority);
		RemoveAt<TK_TYPE>(operators, iFindOffset);

		auto a = operands[iFindOffset];
		auto b = operands[iFindOffset + 1];

		if (op <= NOP_DIV2)
		{
			if (a._iArrayIndex == INVALID_ERROR_PARSEJOB)
			{
				if (IsTempVar(a._iVar))
				{
					DebugLog("Error (%d, %d): = lvalue \n", ar.CurLine(), ar.CurCol());
					return TK_NONE;
				}
			}

			if (b._iArrayIndex == INVALID_ERROR_PARSEJOB)
			{
				if(a._iArrayIndex == INVALID_ERROR_PARSEJOB)
					funs._cur.Push_MOV(op, a._iVar, b._iVar);
				else
					funs._cur.Push_TableInsert(a._iVar, a._iArrayIndex, b._iVar);
			}
			else
			{
				if (a._iArrayIndex == INVALID_ERROR_PARSEJOB)
					funs._cur.Push_TableRead(b._iVar, b._iArrayIndex, a._iVar);
				else
				{
					int iTempOffset2 = funs._cur.AllocLocalTempVar();
					funs._cur.Push_TableRead(b._iVar, b._iArrayIndex, iTempOffset2);
					funs._cur.Push_TableInsert(a._iVar, a._iArrayIndex, iTempOffset2);
				}
			}
		}
		else
		{
			int iTempOffset2;

			if (a._iArrayIndex != INVALID_ERROR_PARSEJOB)
			{
				iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(a._iVar, a._iArrayIndex, iTempOffset2);
				a._iVar = iTempOffset2;
			}
			if (b._iArrayIndex != INVALID_ERROR_PARSEJOB)
			{
				iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(b._iVar, b._iArrayIndex, iTempOffset2);
				b._iVar = iTempOffset2;
			}

			iTempOffset2 = funs._cur.AllocLocalTempVar();
			funs._cur.Push_OP(op, iTempOffset2, a._iVar, b._iVar);
			operands[iFindOffset] = iTempOffset2;
		}
		RemoveAt<SOperand>(operands, iFindOffset + 1);
	}

	sResultStack = operands[0];
	return r;
}

//	for Init
//	for {} Process
//	for Increase
//	for Check
BOOL ParseFor(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
	BYTE byTempCheck[1024];
	BYTE byTempInc[1024];

	// "for (var i = 0; i < 789; i++)" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		DebugLog("Error (%d, %d): for '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return FALSE;
	}

	AddLocalVar(vars.GetCurrentLayer());

	//==> for Init
	iTempOffset.Reset();
	r = ParseJob(FALSE, iTempOffset, NULL, ar, funs, vars, TRUE);
	if (TK_SEMICOLON != r) // 초기화
	{
		DebugLog("Error (%d, %d): for Init", ar.CurLine(), ar.CurCol());
		return FALSE;
	}

	funs._cur.Push_JMP(0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset());

	int PosLoopTop = funs._cur._code.GetBufferOffset(); // Loop 의 맨위

	// For Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(TRUE, iTempOffset, NULL, ar, funs, vars);
	if (TK_SEMICOLON != r)
	{
		DebugLog("Error (%d, %d): for Check", ar.CurLine(), ar.CurCol());
		return FALSE;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur._code.GetBufferOffset();
	funs._cur._code.SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = Pos2 - Pos1;
	if (iCheckCodeSize > sizeof(byTempCheck))
	{
		DebugLog("Error (%d, %d): Check Size Over %d", ar.CurLine(), ar.CurCol(), iCheckCodeSize);
		return FALSE;
	}
	funs._cur._code.Read(byTempCheck, iCheckCodeSize);
	funs._cur._code.SetPointer(Pos1, SEEK_SET);

	// For Increase
	iTempOffset = INVALID_ERROR_PARSEJOB;
	r = ParseJob(FALSE, iTempOffset, NULL, ar, funs, vars); // 증감
	if (TK_R_SMALL != r)
	{
		DebugLog("Error (%d, %d): for Inc", ar.CurLine(), ar.CurCol());
		return FALSE;
	}
	Pos2 = funs._cur._code.GetBufferOffset();
	funs._cur._code.SetPointer(Pos1, SEEK_SET);
	int iIncCodeSize = Pos2 - Pos1;
	if (iIncCodeSize > sizeof(byTempInc))
	{
		DebugLog("Error (%d, %d): Inc Size Over %d", ar.CurLine(), ar.CurCol(), iIncCodeSize);
		return FALSE;
	}
	funs._cur._code.Read(byTempInc, iIncCodeSize);
	funs._cur._code.SetPointer(Pos1, SEEK_SET);

	//	for {} Process
	std::vector<SJumpValue> sJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(FALSE, iTempOffset, &sJumps, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): ;", ar.CurLine(), ar.CurCol());
			return FALSE;
		}
	}
	else
	{
		if (FALSE == ParseMiddleArea(&sJumps, ar, funs, vars))
			return FALSE;
	}

	funs._cur._code.Write(byTempInc, iIncCodeSize);

	funs._cur.Set_JumpOffet(jmp1, funs._cur._code.GetBufferOffset());
	funs._cur._code.Write(byTempCheck, iCheckCodeSize);
	funs._cur.Push_JMPTrue(iStackCheckVar, PosLoopTop);

	int forEndPos = funs._cur._code.GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur.Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return TRUE;
}
BOOL ParseIF(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;

	// "for (var i = 0; i < 789; i++)" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		DebugLog("Error (%d, %d): for '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return FALSE;
	}

	//==> if( xxx )
	iTempOffset.Reset();
	r = ParseJob(TRUE, iTempOffset, pJumps, ar, funs, vars, TRUE);
	if (TK_R_SMALL != r) // )
	{
		DebugLog("Error (%d, %d): for Init", ar.CurLine(), ar.CurCol());
		return FALSE;
	}


	funs._cur.Push_JMPFalse(iTempOffset._iVar, 0);
	SJumpValue jmp1(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset());
	SJumpValue jmp2;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_L_MIDDLE)
	{
		AddLocalVar(vars.GetCurrentLayer());

		if (FALSE == ParseMiddleArea(pJumps, ar, funs, vars))
			return FALSE;

		DelLocalVar(vars.GetCurrentLayer());
	}
	else
	{
		PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(FALSE, iTempOffset, pJumps, ar, funs, vars);

		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): if ;", ar.CurLine(), ar.CurCol());
			return FALSE;
		}
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_ELSE)
	{
		funs._cur.Push_JMP(0);
		jmp2.Set(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset());

		funs._cur.Set_JumpOffet(jmp1, funs._cur._code.GetBufferOffset());

		tkType1 = GetToken(ar, tk1);
		if (tkType1 == TK_IF)
		{
			if (FALSE == ParseIF(pJumps, ar, funs, vars))
				return FALSE;
		}
		else if (tkType1 == TK_L_MIDDLE)
		{
			AddLocalVar(vars.GetCurrentLayer());

			if (FALSE == ParseMiddleArea(pJumps, ar, funs, vars))
				return FALSE;

			DelLocalVar(vars.GetCurrentLayer());
		}
		else
		{
			PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(FALSE, iTempOffset, pJumps, ar, funs, vars);

			if (TK_SEMICOLON != r)
			{
				DebugLog("Error (%d, %d): else ;", ar.CurLine(), ar.CurCol());
				return FALSE;
			}
		}
		funs._cur.Set_JumpOffet(jmp2, funs._cur._code.GetBufferOffset());
	}
	else
	{
		PushToken(tkType1, tk1);
		funs._cur.Set_JumpOffet(jmp1, funs._cur._code.GetBufferOffset());
	}

	return TRUE;
}
BOOL ParseVarDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_STRING)
	{
		if (pCurLayer->FindVar(tk1) >= 0)
		{
			DebugLog("Error (%d, %d): Function Local Var Already (%s) %s", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
			return FALSE;
		}
		int iLocalVar = 1 + (int)funs._cur._args.size() + funs._cur._localVarCount++;
		if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
		{
			iLocalVar += COMPILE_GLObAL_VAR_BEGIN;
		}
		pCurLayer->AddLocalVar(tk1, iLocalVar);
		PushToken(tkType1, tk1);

		SOperand iTempLocalVar;
		r = ParseJob(FALSE, iTempLocalVar, NULL, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): ", ar.CurLine(), ar.CurCol());
			return FALSE;
		}
	}
	else
	{
		DebugLog("Error (%d, %d): Function Local Var (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
		return FALSE;
	}
	return TRUE;
}

BOOL ParseMiddleArea(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1, tk2, tk3;
	TK_TYPE tkType1, tkType2, tkType3;
	TK_TYPE r;

	SOperand iTempOffset;
	FUNCTION_TYPE funType = FUNT_NORMAL;

	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	bool blEnd = false;
	while (blEnd == false)
	{
		funs._cur.FreeLocalTempVar();

		tkType1 = GetToken(ar, tk1);
		//if (32 == ar.CurLine())
		//{
		//	int a = 3;
		//}
		switch (tkType1)
		{
		case TK_NONE:
			if (funs._cur._name != GLOBAL_INIT_FUN_NAME)
			{
				DebugLog("Error (%d, %d): Function End (%s)", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str());
				return FALSE;
			}
			blEnd = TRUE;
			break;
		case TK_R_MIDDLE:
			if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
			{
				DebugLog("Error (%d, %d): Global Init (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
				return FALSE;
			}
			blEnd = TRUE;
			break;
		case TK_RETURN:
			PushToken(tkType1, tk1);

			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(FALSE, iTempOffset, NULL, ar, funs, vars);
			if (TK_SEMICOLON != r)
			{
				DebugLog("Error (%d, %d): return end is ;", ar.CurLine(), ar.CurCol());
				return FALSE;
			}
			break;
		case TK_VAR:
			if (FALSE == ParseVarDef(ar, funs, vars))
				return FALSE;
			break;
		case TK_BREAK:
			if (pJumps == NULL)
			{
				DebugLog("Error (%d, %d): break error\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_JMP(0);
			pJumps->push_back(SJumpValue(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset()));
			break;
		case TK_IMPORT:
			funType = FUNT_IMPORT;
			break;
		case TK_EXPORT:
			funType = FUNT_EXPORT;
			break;
		case TK_FUN:
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_STRING)
			{
				tkType3 = GetToken(ar, tk3);
				if (tkType3 == TK_L_SMALL) // 함수
				{
					SFunctionInfo save = funs._cur;
					funs._cur.Clear();

					funs._cur._name = tk2;
					funs._cur._funType = funType;
					if (FALSE == ParseFunction(ar, funs, vars))
						return FALSE;

					funs._cur = save;
				}
				else
				{
					DebugLog("Error (%d, %d): Unknow Token(%d,%d) '%s'\n", ar.CurLine(), ar.CurCol(), tk2.c_str(), tkType3, tk2.c_str());
					return FALSE;
				}
			}
			else
			{
				DebugLog("Error (%d, %d): Function Name (%d) '%s'\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk2.c_str());
				return FALSE;
			}
			break;

		case TK_STRING:
			PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(FALSE, iTempOffset, NULL, ar, funs, vars);
			if (TK_NONE == r)
			{
				DebugLog("Error (%d, %d): ", ar.CurLine(), ar.CurCol());
				return FALSE;
			}
			break;
		case TK_IF:
			if (FALSE == ParseIF(pJumps, ar, funs, vars))
				return FALSE;
			break;
		case TK_FOR:
			if (FALSE == ParseFor(ar, funs, vars))
				return FALSE;
			break;
		default:
			DebugLog("Error (%d, %d): Function Name (%s) '%s'\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
			return FALSE;
			break;
		}

		if (tkType1 != TK_IMPORT && tkType1 != TK_EXPORT)
			funType = FUNT_NORMAL;
	}
	return TRUE;
}

BOOL ParseFunctionBody(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	//OutAsm("%s -> Begin", funs._cur._name.c_str());
	int iLenTemp = 1 * 1024 * 1024;
	BYTE* pCodeTemp = new BYTE[iLenTemp];
	funs._cur._code.SetData(pCodeTemp, iLenTemp);

	if (FALSE == ParseMiddleArea(NULL, ar, funs, vars))
		return FALSE;

	funs._cur.Push_RETURN(0);

	int iLenCode = funs._cur._code.GetBufferOffset();
	BYTE* pCode = new BYTE[iLenCode + 1];
	memcpy(pCode, pCodeTemp, iLenCode);

	funs._cur._code.SetData(pCode, iLenCode);
	funs._cur._code.SetPointer(iLenCode, SEEK_SET);

	delete pCodeTemp;

	//OutAsm("%s <- End [Arg:%d, Vars:%d, Temp:%d]", funs._cur._name.c_str(), (int)funs._cur._args.size(), funs._cur._localVarCount, funs._cur._localTempMax);
	return TRUE;
}
BOOL ParseFunction(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	funs._cur.Clear();

	std::string tk1;
	TK_TYPE tkType1;

	SLayerVar* pCurLayer = AddVarsFunction(vars);
	if (FALSE == ParseFunctionArg(ar, funs, pCurLayer))
		return FALSE;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE)
	{
		if (tkType1 == TK_SEMICOLON)
		{
			funs._cur._funID = funs._funs.size() + 1;
			funs._funs[funs._cur._name] = funs._cur; // 이름 먼저 등록

			DelVarsFunction(vars);
			return TRUE;
		}
		DebugLog("Error (%d, %d): Function Start (%s) %d\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
		return FALSE;
	}

	funs._cur._funID = funs._funs.size() + 1;
	funs._funs[funs._cur._name] = funs._cur; // 이름 먼저 등록

	if (FALSE == ParseFunctionBody(ar, funs, vars))
		return FALSE;

	auto it = funs._funs.find(funs._cur._name);
	(*it).second = funs._cur;

	DelVarsFunction(vars);

	return TRUE;
}

BOOL Parse(CArchiveRdWC& ar, CNArchive&arw)
{
	g_sTokenQueue.clear();

	SVars	vars;
	SFunctions funs;
	funs._cur._funID = 0;
	funs._cur._name = GLOBAL_INIT_FUN_NAME;
	funs._cur.Clear();

	SLayerVar* pCurLayer = AddVarsFunction(vars);

	funs.AddStaticString("system");

	BOOL r = ParseFunctionBody(ar, funs, vars);

	if (TRUE == r)
	{
		Write(arw, funs, vars);
		WriteLog(funs, vars);
	}

	DelVarsFunction(vars);

	while (vars._varsFunction.empty() == false)
		DelVarsFunction(vars);

	return r;
}

BOOL CNeoVM::Compile(void* pBufferSrc, int iLenSrc, void* pBufferCode, int iLenCode, int* pLenCode)
{
	CArchiveRdWC ar2;
	ToArchiveRdWC((const char*)pBufferSrc, iLenSrc, ar2);

	CNArchive arw;
	arw.SetData(pBufferCode, iLenCode);

	BOOL r = Parse(ar2, arw);
	if (r)
		*pLenCode = arw.GetBufferOffset();
	return r;
}