// Neo1.00.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "NeoParser.h"
#include "NeoVM.h"
#include "NeoTextLoader.h"
#include "NeoExport.h"

void	DebugLog(const char*	lpszString, ...);

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
	TK_WHILE,
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

struct STokenValue
{
	std::string _str;
	char		_iPriority;
	NOP_TYPE	_op;

	STokenValue() {}
	STokenValue(const char* p, char iPriority, NOP_TYPE	op)
	{
		_str = p;
		_iPriority = iPriority;
		_op = op;
	}
};

std::map<TK_TYPE, STokenValue> g_sTokenToString;
std::map<std::string, TK_TYPE> g_sStringToToken;

#define TOKEN_STR1(key, str) g_sTokenToString[key] = STokenValue(str, 20, NOP_NONE)
#define TOKEN_STR2(key, str) g_sTokenToString[key] = STokenValue(str, 20, NOP_NONE); g_sStringToToken[str] = key
#define TOKEN_STR3(key, str, pri, op) g_sTokenToString[key] = STokenValue(str, pri, op); g_sStringToToken[str] = key

void InitDefaultTokenString()
{
	g_sTokenToString.clear();
	g_sStringToToken.clear();

	TOKEN_STR1(TK_UNUSED, "unused token");

	TOKEN_STR1(TK_NONE, "none token");
	TOKEN_STR1(TK_STRING, "string token");

	TOKEN_STR2(TK_VAR, "var");
	TOKEN_STR2(TK_FUN, "fun");
	TOKEN_STR2(TK_IMPORT, "import");
	TOKEN_STR2(TK_EXPORT, "export");

	TOKEN_STR3(TK_TOSTRING, "tostring", 20, NOP_TOSTRING);
	TOKEN_STR3(TK_TOINT, "toint", 20, NOP_TOINT);
	TOKEN_STR3(TK_TOFLOAT, "tofloat", 20, NOP_TOFLOAT);
	TOKEN_STR3(TK_GETTYPE, "type", 20, NOP_GETTYPE);

	TOKEN_STR2(TK_RETURN, "return");
	TOKEN_STR2(TK_BREAK, "break");
	TOKEN_STR2(TK_IF, "if");
	TOKEN_STR2(TK_ELSE, "else");
	TOKEN_STR2(TK_FOR, "for");
	TOKEN_STR2(TK_WHILE, "while");
	TOKEN_STR2(TK_TRUE, "true");
	TOKEN_STR2(TK_FALSE, "false");

	TOKEN_STR3(TK_PLUS2, "++", 20, NOP_INC);
	TOKEN_STR3(TK_MINUS2, "--", 20, NOP_DEC);

	TOKEN_STR3(TK_PLUS, "+", 5, NOP_ADD3);
	TOKEN_STR3(TK_PLUS_EQ, "+=", 15, NOP_ADD2);
	TOKEN_STR3(TK_MINUS, "-", 5, NOP_SUB3);
	TOKEN_STR3(TK_MINUS_EQ, "-=", 15, NOP_SUB2);
	TOKEN_STR3(TK_MUL, "*", 4, NOP_MUL3);
	TOKEN_STR3(TK_MUL_EQ, "*=", 15, NOP_MUL2);
	TOKEN_STR3(TK_DIV, "/", 4, NOP_DIV3);
	TOKEN_STR3(TK_DIV_EQ, "/=", 15, NOP_DIV2);
	TOKEN_STR2(TK_PERCENT, "%");
	TOKEN_STR2(TK_TILDE, "~");
	TOKEN_STR2(TK_CIRCUMFLEX, "^");
	TOKEN_STR3(TK_EQUAL, "=", 15, NOP_MOV);
	TOKEN_STR3(TK_EQUAL_EQ, "==", 8, NOP_EQUAL2);
	TOKEN_STR3(TK_EQUAL_NOT, "!=", 8, NOP_NEQUAL);

	TOKEN_STR2(TK_AND, "&");
	TOKEN_STR3(TK_AND2, "&&", 12, NOP_AND);
	TOKEN_STR2(TK_OR, "|");
	TOKEN_STR3(TK_OR2, "||", 13, NOP_OR);

	TOKEN_STR2(TK_L_SMALL, "(");
	TOKEN_STR2(TK_R_SMALL, ")");
	TOKEN_STR2(TK_L_MIDDLE, "{");
	TOKEN_STR2(TK_R_MIDDLE, "}");
	TOKEN_STR2(TK_L_ARRAY, "[");
	TOKEN_STR2(TK_R_ARRAY, "]");

	TOKEN_STR3(TK_GREAT, ">", 7, NOP_GREAT);
	TOKEN_STR3(TK_GREAT_EQ, ">=", 7, NOP_GREAT_EQ);
	TOKEN_STR3(TK_LESS, "<", 7, NOP_LESS);
	TOKEN_STR3(TK_LESS_EQ, "<=", 7, NOP_LESS_EQ);

	TOKEN_STR2(TK_COLON, ":");
	TOKEN_STR2(TK_SEMICOLON, ";");
	TOKEN_STR2(TK_COMMA, ",");
	TOKEN_STR2(TK_DOT, ".");
	TOKEN_STR3(TK_DOT2, "..", 6, NOP_STR_ADD);
	TOKEN_STR2(TK_SHARP, "#");
	TOKEN_STR2(TK_QUOTATION, "\"");
	TOKEN_STR2(TK_QUESTION, "?");
	TOKEN_STR2(TK_NOT, "!");
}
std::string GetTokenString(TK_TYPE tk)
{
	auto it = g_sTokenToString.find(tk);
	if (it == g_sTokenToString.end())
		return "";
	return (*it).second._str;
}

#define GLOBAL_INIT_FUN_NAME	"##_global_##"
bool ParseFunction(CArchiveRdWC& ar, SFunctions& funs, SVars& vars);


void SkipCurrentLine(CArchiveRdWC& ar)
{
	while (true)
	{
		u16 c = ar.GetData(true);
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
	u16 c1, c2;
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

	auto it = g_sStringToToken.find(tk);
	if (it != g_sStringToToken.end())
	{
		return (*it).second;
	}

	return TK_STRING;
}


TK_TYPE CalcToken2(TK_TYPE tkTypeOnySingleChar, CArchiveRdWC& ar, std::string& tk)
{
	if (false == tk.empty())
		return CalcStringToken(tk);

	u16 c1 = ar.GetData(true);
	u16 c2 = ar.GetData(false);
	if(c2 > 255)
		return tkTypeOnySingleChar;

	std::string s1;
	s1 += (u8)c1;
	s1 += (u8)c2;

	TK_TYPE tkTemp = CalcStringToken(s1);
	if (tkTemp != TK_STRING && tkTemp != TK_NONE)
	{
		ar.GetData(true);
		return tkTemp;
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
	auto it = g_sTokenToString.find(tk);
	if (it == g_sTokenToString.end())
	{
		iPriority = 20;
		return NOP_NONE;
	}
	iPriority = (*it).second._iPriority;
	return (*it).second._op;
}

bool GetQuotationString(CArchiveRdWC& ar, std::string& str)
{
	str.clear();
	bool blEnd = false;
	while (blEnd == false)
	{
		u16 c1 = ar.GetData(true);
		if (c1 == NULL)
			return false;
		if (c1 == '\n')
			return false;
		if (c1 == '"')
			break;
		if (c1 == '\\')
		{
			u16 c2 = ar.GetData(false);
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
	return true;
}
static bool IsDotChar(u16 c)
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
bool GetDotString(CArchiveRdWC& ar, std::string& str)
{
	str.clear();
	bool blEnd = false;
	while (blEnd == false)
	{
		u16 c1 = ar.GetData(false);
		if (false == IsDotChar(c1))
			break;

		ar.GetData(true);
		str.push_back((char)c1);
	}
	return true;
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
		u16 c1 = ar.GetData(false);
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
			u16 c2 = ar.GetData(false);
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

bool ParseFunctionArg(CArchiveRdWC& ar, SFunctions& funs, SLayerVar* pCurLayer)
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
					return false;
				}
				pCurLayer->AddLocalVar(tk2, 1 + (int)funs._cur._args.size());
				funs._cur._args.insert(tk2);
				bPreviusComa = false;
			}
			else
			{
				DebugLog("Error (%d, %d): Function Arg (%d)", ar.CurLine(), ar.CurCol(), tk1.c_str());
				return false;
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
	return true;
}

void AddLocalVar(SLayerVar* pCurLayer)
{
	SLocalVar* pCurLocal = new SLocalVar();
	pCurLayer->_varsLayer.push_back(pCurLocal);
}

void DelLocalVar(SLayerVar* pCurLayer)
{
	int sz = (int)pCurLayer->_varsLayer.size();
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
	int cnt = (int)vars._varsFunction.size();
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

TK_TYPE ParseJob(bool bReqReturn, SOperand& sResultStack, std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool bAllowVarDef = false);
bool ParseVarDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars);
bool ParseMiddleArea(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars);
bool IsTempVar(int iVar)
{
	if (COMPILE_LOCALTMP_VAR_BEGIN <= iVar && iVar < COMPILE_STATIC_VAR_BEGIN)
		return true;

	return false;
}

bool ParseFunCall(SOperand& iResultStack, TK_TYPE tkTypePre, SFunctionInfo* pFun, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
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
			TK_TYPE r2 = ParseJob(true, iTempVar, NULL, ar, funs, vars);
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
		return false;
	}
	return true;
}

bool ParseNum(int& iResultStack, TK_TYPE tkTypePre, std::string& tk1, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk2;
	TK_TYPE tkType2;

	double num;

	if (true == StringToDouble(num, tk1.c_str()))
	{
		u16 c = ar.GetData(false);
		if (c == '.')
		{
			ar.GetData(true);

			tkType2 = GetToken(ar, tk2);
			double num2 = 0;
			if (true == StringToDoubleLow(num2, tk2.c_str()))
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
		return false;
	}
	return true;
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
	r = ParseJob(true, operand, NULL, ar, funs, vars);
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

bool ParseString(SOperand& operand, TK_TYPE tkTypePre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
	TK_TYPE r = TK_NONE;

	SOperand iTempOffset;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING)
	{
		DebugLog("Error (%d, %d): String", ar.CurLine(), ar.CurCol());
		return false;
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

				if (false == ParseFunCall(iTempOffset, tkTypePre, NULL, ar, funs, vars))
					return false;
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
				if (false == GetDotString(ar, str))
				{
					DebugLog("Error (%d, %d): . string", ar.CurLine(), ar.CurCol());
					return false;
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
			if (false == ParseFunCall(iTempOffset, tkTypePre, pFun, ar, funs, vars))
				return false;
		}
		else
		{
			if (false == ParseNum(iTempOffset._iVar, tkTypePre, tk1, ar, funs, vars))
				return false;
		}
	}
	operand = SOperand(iTempOffset._iVar, iArrayIndex);
	return true;
}

bool ParseToType(int& iResultStack, TK_TYPE tkTypePre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r = TK_NONE;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL)
	{
		DebugLog("Error (%d, %d): type begin (", ar.CurLine(), ar.CurCol());
		return false;
	}

	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
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
	return true;
}

TK_TYPE ParseJob(bool bReqReturn, SOperand& sResultStack, std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool bAllowVarDef)
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
			r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
			if (r != TK_SEMICOLON)
			{
				DebugLog("Error (%d, %d): return", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_RETURN(iTempOffset._iVar);
			blEnd = true;
			break;
		case TK_SEMICOLON:	// ;
		case TK_COMMA:		// ,
		case TK_R_SMALL:	// )
		case TK_R_ARRAY:	// ]
			r = tkType1;
			blEnd = true;
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
			if (false == ParseString(iTempOffset, TK_NONE, ar, funs, vars))
			{
				return TK_NONE;
			}

			operands.push_back(iTempOffset);
			blApperOperator = true;
			break;
		case TK_QUOTATION:
		{
			std::string str;
			if (false == GetQuotationString(ar, str))
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
				if (false == ParseString(a, tkType1, ar, funs, vars))
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
			r = ParseJob(true, iTempVar, NULL, ar, funs, vars);
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
			blEnd = true;
			break;
		}
		case TK_VAR:
			if (bAllowVarDef)
			{
				if (false == ParseVarDef(ar, funs, vars))
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
			blEnd = true;
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
			if (false == ParseToType(iTempOffset._iVar, tkType1, ar, funs, vars))
				return TK_NONE;
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		case TK_PLUS2: // ++
		case TK_MINUS2: // --
			if (blApperOperator == false)
			{	// 전위
				SOperand a;
				if (false == ParseString(a, TK_NONE, ar, funs, vars))
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
bool ParseFor(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
	u8 byTempCheck[1024];
	u8 byTempInc[1024];

	// "for (var i = 0; i < 789; i++)" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		DebugLog("Error (%d, %d): for '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());

	//==> for Init
	iTempOffset.Reset();
	r = ParseJob(false, iTempOffset, NULL, ar, funs, vars, true);
	if (TK_SEMICOLON != r) // 초기화
	{
		DebugLog("Error (%d, %d): for Init", ar.CurLine(), ar.CurCol());
		return false;
	}

	funs._cur.Push_JMP(0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset());

	int PosLoopTop = funs._cur._code.GetBufferOffset(); // Loop 의 맨위

	// For Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
	if (TK_SEMICOLON != r)
	{
		DebugLog("Error (%d, %d): for Check", ar.CurLine(), ar.CurCol());
		return false;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur._code.GetBufferOffset();
	funs._cur._code.SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = Pos2 - Pos1;
	if (iCheckCodeSize > sizeof(byTempCheck))
	{
		DebugLog("Error (%d, %d): Check Size Over %d", ar.CurLine(), ar.CurCol(), iCheckCodeSize);
		return false;
	}
	funs._cur._code.Read(byTempCheck, iCheckCodeSize);
	funs._cur._code.SetPointer(Pos1, SEEK_SET);

	// For Increase
	iTempOffset = INVALID_ERROR_PARSEJOB;
	r = ParseJob(false, iTempOffset, NULL, ar, funs, vars); // 증감
	if (TK_R_SMALL != r)
	{
		DebugLog("Error (%d, %d): for Inc", ar.CurLine(), ar.CurCol());
		return false;
	}
	Pos2 = funs._cur._code.GetBufferOffset();
	funs._cur._code.SetPointer(Pos1, SEEK_SET);
	int iIncCodeSize = Pos2 - Pos1;
	if (iIncCodeSize > sizeof(byTempInc))
	{
		DebugLog("Error (%d, %d): Inc Size Over %d", ar.CurLine(), ar.CurCol(), iIncCodeSize);
		return false;
	}
	funs._cur._code.Read(byTempInc, iIncCodeSize);
	funs._cur._code.SetPointer(Pos1, SEEK_SET);

	//	for {} Process
	std::vector<SJumpValue> sJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): ;", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars))
			return false;
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

	return true;
}

bool ParseWhile(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
	u8 byTempCheck[1024];

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		DebugLog("Error (%d, %d): while '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());


	funs._cur.Push_JMP(0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset());

	int PosLoopTop = funs._cur._code.GetBufferOffset(); // Loop 의 맨위

	// While Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
	if (TK_R_SMALL != r)
	{
		DebugLog("Error (%d, %d): while Check", ar.CurLine(), ar.CurCol());
		return false;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur._code.GetBufferOffset();
	funs._cur._code.SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = Pos2 - Pos1;
	if (iCheckCodeSize > sizeof(byTempCheck))
	{
		DebugLog("Error (%d, %d): Check Size Over %d", ar.CurLine(), ar.CurCol(), iCheckCodeSize);
		return false;
	}
	funs._cur._code.Read(byTempCheck, iCheckCodeSize);
	funs._cur._code.SetPointer(Pos1, SEEK_SET);



	//	while {} Process
	std::vector<SJumpValue> sJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): ;", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars))
			return false;
	}

	funs._cur.Set_JumpOffet(jmp1, funs._cur._code.GetBufferOffset());
	funs._cur._code.Write(byTempCheck, iCheckCodeSize);
	funs._cur.Push_JMPTrue(iStackCheckVar, PosLoopTop);

	int forEndPos = funs._cur._code.GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur.Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return true;
}
bool ParseIF(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
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
		return false;
	}

	//==> if( xxx )
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, pJumps, ar, funs, vars, true);
	if (TK_R_SMALL != r) // )
	{
		DebugLog("Error (%d, %d): for Init", ar.CurLine(), ar.CurCol());
		return false;
	}


	funs._cur.Push_JMPFalse(iTempOffset._iVar, 0);
	SJumpValue jmp1(funs._cur._code.GetBufferOffset() - 2, funs._cur._code.GetBufferOffset());
	SJumpValue jmp2;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_L_MIDDLE)
	{
		AddLocalVar(vars.GetCurrentLayer());

		if (false == ParseMiddleArea(pJumps, ar, funs, vars))
			return false;

		DelLocalVar(vars.GetCurrentLayer());
	}
	else
	{
		PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, pJumps, ar, funs, vars);

		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): if ;", ar.CurLine(), ar.CurCol());
			return false;
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
			if (false == ParseIF(pJumps, ar, funs, vars))
				return false;
		}
		else if (tkType1 == TK_L_MIDDLE)
		{
			AddLocalVar(vars.GetCurrentLayer());

			if (false == ParseMiddleArea(pJumps, ar, funs, vars))
				return false;

			DelLocalVar(vars.GetCurrentLayer());
		}
		else
		{
			PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, pJumps, ar, funs, vars);

			if (TK_SEMICOLON != r)
			{
				DebugLog("Error (%d, %d): else ;", ar.CurLine(), ar.CurCol());
				return false;
			}
		}
		funs._cur.Set_JumpOffet(jmp2, funs._cur._code.GetBufferOffset());
	}
	else
	{
		PushToken(tkType1, tk1);
		funs._cur.Set_JumpOffet(jmp1, funs._cur._code.GetBufferOffset());
	}

	return true;
}
bool ParseVarDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_STRING)
	{
		if (pCurLayer->FindVarOnlyCurrentBlock(tk1) >= 0)
		{
			DebugLog("Error (%d, %d): Function Local Var Already (%s) %s", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
			return false;
		}
		int iLocalVar = 1 + (int)funs._cur._args.size() + funs._cur._localVarCount++;
		if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
		{
			iLocalVar += COMPILE_GLObAL_VAR_BEGIN;
		}
		pCurLayer->AddLocalVar(tk1, iLocalVar);
		PushToken(tkType1, tk1);

		SOperand iTempLocalVar;
		r = ParseJob(false, iTempLocalVar, NULL, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			DebugLog("Error (%d, %d): ", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		DebugLog("Error (%d, %d): Function Local Var (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
		return false;
	}
	return true;
}

bool ParseMiddleArea(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
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
				return false;
			}
			blEnd = true;
			break;
		case TK_R_MIDDLE:
			if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
			{
				DebugLog("Error (%d, %d): Global Init (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
				return false;
			}
			blEnd = true;
			break;
		case TK_RETURN:
			PushToken(tkType1, tk1);

			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, NULL, ar, funs, vars);
			if (TK_SEMICOLON != r)
			{
				DebugLog("Error (%d, %d): return end is ;", ar.CurLine(), ar.CurCol());
				return false;
			}
			break;
		case TK_VAR:
			if (false == ParseVarDef(ar, funs, vars))
				return false;
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
					if (false == ParseFunction(ar, funs, vars))
						return false;

					funs._cur = save;
				}
				else
				{
					DebugLog("Error (%d, %d): Unknow Token(%d,%d) '%s'\n", ar.CurLine(), ar.CurCol(), tk2.c_str(), tkType3, tk2.c_str());
					return false;
				}
			}
			else
			{
				DebugLog("Error (%d, %d): Function Name (%d) '%s'\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk2.c_str());
				return false;
			}
			break;

		case TK_STRING:
			PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, NULL, ar, funs, vars);
			if (TK_NONE == r)
			{
				DebugLog("Error (%d, %d): ", ar.CurLine(), ar.CurCol());
				return false;
			}
			break;
		case TK_IF:
			if (false == ParseIF(pJumps, ar, funs, vars))
				return false;
			break;
		case TK_FOR:
			if (false == ParseFor(ar, funs, vars))
				return false;
			break;
		case TK_WHILE:
			if (false == ParseWhile(ar, funs, vars))
				return false;
			break;
		default:
			DebugLog("Error (%d, %d): Function Name (%s) '%s'\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
			return false;
			break;
		}

		if (tkType1 != TK_IMPORT && tkType1 != TK_EXPORT)
			funType = FUNT_NORMAL;
	}
	return true;
}

bool ParseFunctionBody(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	//OutAsm("%s -> Begin", funs._cur._name.c_str());
	int iLenTemp = 1 * 1024 * 1024;
	u8* pCodeTemp = new u8[iLenTemp];
	funs._cur._code.SetData(pCodeTemp, iLenTemp);

	if (false == ParseMiddleArea(NULL, ar, funs, vars))
		return false;

	funs._cur.Push_RETURN(0);

	int iLenCode = funs._cur._code.GetBufferOffset();
	u8* pCode = new u8[iLenCode + 1];
	memcpy(pCode, pCodeTemp, iLenCode);

	funs._cur._code.SetData(pCode, iLenCode);
	funs._cur._code.SetPointer(iLenCode, SEEK_SET);

	delete pCodeTemp;

	//OutAsm("%s <- End [Arg:%d, Vars:%d, Temp:%d]", funs._cur._name.c_str(), (int)funs._cur._args.size(), funs._cur._localVarCount, funs._cur._localTempMax);
	return true;
}
bool ParseFunction(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	funs._cur.Clear();

	std::string tk1;
	TK_TYPE tkType1;

	SLayerVar* pCurLayer = AddVarsFunction(vars);
	if (false == ParseFunctionArg(ar, funs, pCurLayer))
		return false;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE)
	{
		if (tkType1 == TK_SEMICOLON)
		{
			funs._cur._funID = (int)funs._funs.size() + 1;
			funs._funs[funs._cur._name] = funs._cur; // 이름 먼저 등록

			DelVarsFunction(vars);
			return true;
		}
		DebugLog("Error (%d, %d): Function Start (%s) %d\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
		return false;
	}

	funs._cur._funID = (int)funs._funs.size() + 1;
	funs._funs[funs._cur._name] = funs._cur; // 이름 먼저 등록

	if (false == ParseFunctionBody(ar, funs, vars))
		return false;

	auto it = funs._funs.find(funs._cur._name);
	(*it).second = funs._cur;

	DelVarsFunction(vars);

	return true;
}

bool Parse(CArchiveRdWC& ar, CNArchive&arw, bool putASM)
{
	if (g_sTokenToString.empty())
		InitDefaultTokenString();

	g_sTokenQueue.clear();

	SVars	vars;
	SFunctions funs;
	funs._cur._funID = 0;
	funs._cur._name = GLOBAL_INIT_FUN_NAME;
	funs._cur.Clear();

	SLayerVar* pCurLayer = AddVarsFunction(vars);

	funs.AddStaticString("system");

	bool r = ParseFunctionBody(ar, funs, vars);

	if (true == r)
	{
		Write(arw, funs, vars);
		if(putASM)
			WriteLog(funs, vars);
	}

	DelVarsFunction(vars);

	while (vars._varsFunction.empty() == false)
		DelVarsFunction(vars);

	return r;
}

bool CNeoVM::Compile(void* pBufferSrc, int iLenSrc, void* pBufferCode, int iLenCode, int* pLenCode, bool putASM)
{
	CArchiveRdWC ar2;
	ToArchiveRdWC((const char*)pBufferSrc, iLenSrc, ar2);

	CNArchive arw;
	arw.SetData(pBufferCode, iLenCode);

	bool r = Parse(ar2, arw, putASM);
	if (r)
		*pLenCode = arw.GetBufferOffset();
	return r;
}
