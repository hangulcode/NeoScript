#include "NeoParser.h"
#include "NeoVM.h"
#include "NeoExport.h"

void	SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...);

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

enum IncrementOperator
{
	Increment_None,
	Increment_Prefix,
	Increment_Postfix,
};

struct SOperand
{
	int _iVar;
	int _iArrayIndex;
	IncrementOperator _incementType;

	SOperand()
	{
		Reset();
	}
	SOperand(int iVar, int iArrayIndex = INVALID_ERROR_PARSEJOB)
	{
		_iVar = iVar;
		_iArrayIndex = iArrayIndex;
		_incementType = Increment_None;
	}
	void Reset()
	{
		_iVar = _iArrayIndex = INVALID_ERROR_PARSEJOB;
		_incementType = Increment_None;
	}

	inline bool IsArray() { return _iArrayIndex  != INVALID_ERROR_PARSEJOB; }
};

struct SOperationInfo
{
	std::string _str;
	eNOperation	_op;
	int			_op_length;
	OpType		_opType;
	SOperationInfo() {}
	SOperationInfo(const char* p, eNOperation	op, int len)
	{
		_str = p;
		_op = op;
		_op_length = len;
		_opType = op;
	}
};

#define OP_STR1(op, len) if(op > 63){ blError = true;} g_sOpInfo[op] = SOperationInfo(#op, op, len)


struct STokenValue
{
	std::string _str;
	char		_iPriority;
	eNOperation	_op;

	STokenValue() {}
	STokenValue(const char* p, char iPriority, eNOperation	op)
	{
		_str = p;
		_iPriority = iPriority;
		_op = op;
	}
};

std::vector<SOperationInfo> g_sOpInfo;
std::vector<STokenValue> g_sTokenToString;
std::map<std::string, TK_TYPE> g_sStringToToken;

#define TOKEN_STR1(key, str) g_sTokenToString[key] = STokenValue(str, 20, NOP_NONE)
#define TOKEN_STR2(key, str) g_sTokenToString[key] = STokenValue(str, 20, NOP_NONE); g_sStringToToken[str] = key
#define TOKEN_STR3(key, str, pri, op) g_sTokenToString[key] = STokenValue(str, pri, op); g_sStringToToken[str] = key

OpType GetOpTypeFromOp(eNOperation op)
{
	return g_sOpInfo[op]._opType;
}
int GetOpLength(eNOperation op)
{
	//return sizeof(OpType) + g_sOpInfo[op]._op_length*sizeof(short);
	return sizeof(OpType) + sizeof(ArgFlag) + (3 * sizeof(short));
}

void InitDefaultData()
{
	g_sOpInfo.clear();
	g_sTokenToString.clear();
	g_sStringToToken.clear();
}
int InitDefaultTokenString()
{
	InitDefaultData();
	bool blError = false;

	g_sTokenToString.resize(TK_MAX);
	g_sOpInfo.resize(NOP_MAX);

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
	TOKEN_STR3(TK_TOSIZE, "tosize", 20, NOP_TOSIZE);
	TOKEN_STR3(TK_GETTYPE, "type", 20, NOP_GETTYPE);
	TOKEN_STR3(TK_SLEEP, "sleep", 20, NOP_SLEEP);

	TOKEN_STR2(TK_RETURN, "return");
	TOKEN_STR2(TK_BREAK, "break");
	TOKEN_STR2(TK_IF, "if");
	TOKEN_STR2(TK_ELSE, "else");
	TOKEN_STR2(TK_FOR, "for");
	TOKEN_STR2(TK_FOREACH, "foreach");
	TOKEN_STR2(TK_WHILE, "while");
	TOKEN_STR2(TK_TRUE, "true");
	TOKEN_STR2(TK_FALSE, "false");
	TOKEN_STR2(TK_NULL, "null");

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
	TOKEN_STR3(TK_PERCENT, "%", 4, NOP_PERSENT3);
	TOKEN_STR3(TK_PERCENT_EQ, "%=", 15, NOP_PERSENT2);

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
	TOKEN_STR2(TK_QUOTE2, "\"");
	TOKEN_STR2(TK_QUOTE1, "'");
	TOKEN_STR2(TK_QUESTION, "?");
	TOKEN_STR2(TK_NOT, "!");

	/////////////////////////////////////////////////////

	OP_STR1(NOP_NONE, 0);
	OP_STR1(NOP_MOV, 2);
	OP_STR1(NOP_MOV_MINUS, 2);
	OP_STR1(NOP_ADD2, 2);
	OP_STR1(NOP_SUB2, 2);
	OP_STR1(NOP_MUL2, 2);
	OP_STR1(NOP_DIV2, 2);
	OP_STR1(NOP_PERSENT2, 2);

	OP_STR1(NOP_VAR_CLEAR, 1);
	OP_STR1(NOP_INC, 1);
	OP_STR1(NOP_DEC, 1);

	OP_STR1(NOP_ADD3, 3);
	OP_STR1(NOP_SUB3, 3);
	OP_STR1(NOP_MUL3, 3);
	OP_STR1(NOP_DIV3, 3);
	OP_STR1(NOP_PERSENT3, 3);

	OP_STR1(NOP_GREAT, 3);
	OP_STR1(NOP_GREAT_EQ, 3);
	OP_STR1(NOP_LESS, 3);
	OP_STR1(NOP_LESS_EQ, 3);
	OP_STR1(NOP_EQUAL2, 3);
	OP_STR1(NOP_NEQUAL, 3);
	OP_STR1(NOP_AND, 3);
	OP_STR1(NOP_OR, 3);

	OP_STR1(NOP_JMP_GREAT, 3);
	OP_STR1(NOP_JMP_GREAT_EQ, 3);
	OP_STR1(NOP_JMP_LESS, 3);
	OP_STR1(NOP_JMP_LESS_EQ, 3);
	OP_STR1(NOP_JMP_EQUAL2, 3);
	OP_STR1(NOP_JMP_NEQUAL, 3);
	OP_STR1(NOP_JMP_AND, 3);
	OP_STR1(NOP_JMP_OR, 3);
	OP_STR1(NOP_JMP_NAND, 3);
	OP_STR1(NOP_JMP_NOR, 3);
	OP_STR1(NOP_JMP_FOREACH, 3);

	OP_STR1(NOP_STR_ADD, 3);

	OP_STR1(NOP_TOSTRING, 2);
	OP_STR1(NOP_TOINT, 2);
	OP_STR1(NOP_TOFLOAT, 2);
	OP_STR1(NOP_TOSIZE, 2);
	OP_STR1(NOP_GETTYPE, 2);
	OP_STR1(NOP_SLEEP, 1);

	OP_STR1(NOP_JMP, 1);
	OP_STR1(NOP_JMP_FALSE, 2);
	OP_STR1(NOP_JMP_TRUE, 2);

	OP_STR1(NOP_CALL, 2);
	OP_STR1(NOP_PTRCALL, 3);
	OP_STR1(NOP_RETURN, 1);
	OP_STR1(NOP_FUNEND, 0);

	OP_STR1(NOP_TABLE_ALLOC, 1);
	OP_STR1(NOP_TABLE_INSERT, 3);
	OP_STR1(NOP_TABLE_READ, 3);
	OP_STR1(NOP_TABLE_REMOVE, 2);

	if (blError)
	{
		InitDefaultData();
		return 0;
	}

	return 1;
}
static int staticTempData = InitDefaultTokenString();

std::string GetTokenString(TK_TYPE tk)
{
	if(tk < 0 || tk >= TK_MAX)
		return "";
	return g_sTokenToString[tk]._str;
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


TK_TYPE CalcToken(TK_TYPE tkTypeOnySingleChar, CArchiveRdWC& ar, std::string& tk)
{
	if (false == tk.empty())
		return CalcStringToken(tk);

	u16 c1 = ar.GetData(true);
	u16 c2 = ar.GetData(false);
	if (c2 > 255)
	{
		tk = GetTokenString(tkTypeOnySingleChar);
		return tkTypeOnySingleChar;
	}

	std::string s1;
	s1 += (u8)c1;
	s1 += (u8)c2;

	TK_TYPE tkTemp = CalcStringToken(s1);
	if (tkTemp != TK_STRING && tkTemp != TK_NONE)
	{
		ar.GetData(true);
		tk = GetTokenString(tkTemp);
		return tkTemp;
	}

	tk = GetTokenString(tkTypeOnySingleChar);
	return tkTypeOnySingleChar;
}

eNOperation TokenToOP(TK_TYPE tk, int& iPriority)
{
	if (tk < 0 || tk >= TK_MAX)
	{
		iPriority = 20;
		return NOP_NONE;
	}
	
	STokenValue& tv = g_sTokenToString[tk];

	iPriority = tv._iPriority;
	return tv._op;
}

bool GetQuotationString(CArchiveRdWC& ar, std::string& str, u16 quote)
{
	str.clear();
	bool blEnd = false;
	while (blEnd == false)
	{
		u16 c1 = ar.GetData(true);
		if (c1 == 0) // null
			return false;
		if (c1 == '\n')
			return false;
		if (c1 == quote)
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
static inline bool IsAlphabet(char c)
{
	if ('a' <= c && c <= 'z')
		return true;
	if ('A' <= c && c <= 'Z')
		return true;
	return false;
}
static inline bool IsDotChar(u16 c)
{
	if ('a' <= c && c <= 'z')
		return true;
	if ('A' <= c && c <= 'Z')
		return true;
	if ('0' <= c && c <= '9')
		return true;
	if(c == '_')
		return true;

	return false;
}
void SkipSpace(CArchiveRdWC& ar)
{
	while (true)
	{
		u16 c1 = ar.GetData(false);
		if (c1 == ' ' || c1 == '\t')
		{
			ar.GetData(true);
			continue;
		}
		return;
	}
}
bool GetDotString(CArchiveRdWC& ar, std::string& str)
{
	SkipSpace(ar);

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
bool AbleName(const std::string& str)
{
	if (str.empty())
		return false;

	const char* p = str.c_str();
	for (int i = 0; i < (int)str.length(); i++)
	{
		char c = p[i];
		if (i == 0)
		{	// first is Alphabet or '_'
			if (IsAlphabet(c) == false && c != '_')
				return false;
		}
		else
		{
			if (IsDotChar(c) == false)
				return false;
		}
	}
	return true;
}




TK_TYPE GetToken(CArchiveRdWC& ar, std::string& tk)
{
	if (false == ar.m_sTokenQueue.empty())
	{
		SToken d = *ar.m_sTokenQueue.begin();
		ar.m_sTokenQueue.pop_front();
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
			return CalcToken(TK_QUOTE2, ar, tk);
		case '\'':
			return CalcToken(TK_QUOTE1, ar, tk);
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
					SetCompileError(ar, "Error (%d, %d): Already Function Arg (%s)", ar.CurLine(), ar.CurCol(), tk2.c_str());
					return false;
				}
				pCurLayer->AddLocalVar(tk2, 1 + (int)funs._cur._args.size());
				funs._cur._args.insert(tk2);
				bPreviusComa = false;
			}
			else
			{
				SetCompileError(ar, "Error (%d, %d): Function Arg (%d)", ar.CurLine(), ar.CurCol(), tk1.c_str());
				return false;
			}
			break;
		case TK_R_SMALL:
			if (bPreviusComa)
			{
				SetCompileError(ar, "Error (%d, %d): Function Arg (Coma)", ar.CurLine(), ar.CurCol());
				return false;
			}
			blEnd = true;
			break;
		case TK_COMMA:
			if (funs._cur._args.size() == 0 || bPreviusComa)
			{
				SetCompileError(ar, "Error (%d, %d): Function Arg (Coma)", ar.CurLine(), ar.CurCol());
				return false;
			}
			bPreviusComa = !bPreviusComa;
			break;
		default:
			SetCompileError(ar, "Error (%d, %d): Function Arg (%s)", ar.CurLine(), ar.CurCol(), tk1.c_str());
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

			ar.PushToken(tkType2, tk2);

			iTempVar.Reset();
			TK_TYPE r2 = ParseJob(true, iTempVar, NULL, ar, funs, vars);
			if(iTempVar._iArrayIndex == INVALID_ERROR_PARSEJOB)
				funs._cur.Push_MOV(ar, NOP_MOV, COMPILE_CALLARG_VAR_BEGIN + 1 + iParamCount, iTempVar._iVar);
			else
				funs._cur.Push_TableRead(ar, iTempVar._iVar, iTempVar._iArrayIndex, COMPILE_CALLARG_VAR_BEGIN + 1 + iParamCount);
			iParamCount++;

			if (iTempVar._iVar == INVALID_ERROR_PARSEJOB)
			{
				SetCompileError(ar, "Error (%d, %d): Call Param\n", ar.CurLine(), ar.CurCol());
				return false;
			}

			if (r2 != TK_R_SMALL && r2 != TK_COMMA)
			{
				SetCompileError(ar, "Error (%d, %d): Call Param\n", ar.CurLine(), ar.CurCol());
				return false;
			}
			if (r2 == TK_R_SMALL)
				break;
		}
		if (pFun != NULL)
		{
			if ((int)pFun->_args.size() != iParamCount)
			{
				SetCompileError(ar, "Error (%d, %d): Arg Count Invalid (%d != %d)", ar.CurLine(), ar.CurCol(), (int)pFun->_args.size(), iParamCount);
				return false;
			}
			//funs._cur.Push_Call(ar, pFun->_funType == FUNT_IMPORT ? NOP_FARCALL : NOP_CALL, pFun->_funID, iParamCount);
			funs._cur.Push_Call(ar, NOP_CALL, pFun->_funID, iParamCount);
		}
		else
			funs._cur.Push_CallPtr(ar, iResultStack._iVar, iResultStack._iArrayIndex, iParamCount);
		iResultStack = funs._cur.AllocLocalTempVar();
		funs._cur.Push_MOV(ar, tkTypePre != TK_MINUS ? NOP_MOV : NOP_MOV_MINUS, iResultStack._iVar, STACK_POS_RETURN);
	}
	else if (tkType1 == TK_SEMICOLON)
	{
		ar.PushToken(tkType1, tk1);
		iResultStack._iVar = pFun->_staticIndex;
	}
	else
	{
		SetCompileError(ar, "Error (%d, %d): ( Not Found\n", ar.CurLine(), ar.CurCol());
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
				SetCompileError(ar, "Error (%d, %d): num data invalid\n", ar.CurLine(), ar.CurCol());
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
		SetCompileError(ar, "Error (%d, %d): Unknown String (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}
	return true;
}

bool ParseStringOrNum(int& iResultStack, TK_TYPE tkTypePre, std::string& tkPre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (tkTypePre == TK_QUOTE2 || tkTypePre == TK_QUOTE1)
	{
		std::string str;
		if (false == GetQuotationString(ar, str, tkTypePre == TK_QUOTE2 ? '"' : '\''))
		{
			SetCompileError(ar, "Error (%d, %d): String End Not Found\n", ar.CurLine(), ar.CurCol());
			return false;
		}
		iResultStack = funs.AddStaticString(str);
		return true;
	}
	else if (tkTypePre == TK_STRING)
	{
		if (false == ParseNum(iResultStack, TK_NONE, tkPre, ar, funs, vars))
			return false;
		return true;
	}
	return false;
}


TK_TYPE ParseTableDef(int& iResultStack, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, int iTableDeep = 0)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
	TK_TYPE r = TK_NONE;

	iResultStack = funs._cur.AllocLocalTempVar();
	funs._cur.Push_TableAlloc(ar, iResultStack);

	int iTempOffsetKey;
	int iTempOffsetValue;
	std::string str;

	int iCurArrayOffset = 0;
	bool bPreviusComa = false;

	bool blEnd = false;
	while (blEnd == false)
	{
		iTempOffsetKey = iTempOffsetValue = INVALID_ERROR_PARSEJOB;
		tkType1 = GetToken(ar, tk1);
		
		if (tkType1 == TK_QUOTE2 || tkType1 == TK_QUOTE1 || tkType1 == TK_STRING)
		{
			if (false == ParseStringOrNum(iTempOffsetKey, tkType1, tk1, ar, funs, vars))
				return TK_NONE;
		}
		else if (tkType1 == TK_L_MIDDLE)
		{
			if (TK_NONE == ParseTableDef(iTempOffsetKey, ar, funs, vars, iTableDeep + 1))
			{
				return TK_NONE;
			}
		}
		else if (tkType1 == TK_R_MIDDLE)
			break;

		tkType2 = GetToken(ar, tk2);
		if (tkType2 == TK_COMMA || tkType2 == TK_R_MIDDLE)
		{
			iTempOffsetValue = iTempOffsetKey;
			iTempOffsetKey = funs.AddStaticInt(iCurArrayOffset++);
			ar.PushToken(tkType2, tk2);
		}
		else if (tkType2 == TK_EQUAL)
		{
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_QUOTE2 || tkType2 == TK_QUOTE1 || tkType2 == TK_STRING)
			{
				if (false == ParseStringOrNum(iTempOffsetValue, tkType2, tk2, ar, funs, vars))
					return TK_NONE;
			}
			else if (tkType2 == TK_L_MIDDLE)
			{
				if (TK_NONE == ParseTableDef(iTempOffsetValue, ar, funs, vars, iTableDeep + 1))
				{
					return TK_NONE;
				}
			}

			int iIntValue = -1;
			if (funs.GetStaticNum(iTempOffsetKey, &iIntValue))
				iCurArrayOffset = iIntValue + 1;
		}
		else
		{
			SetCompileError(ar, "Error (%d, %d): Table Init Error %s\n", ar.CurLine(), ar.CurCol(), tk2.c_str());
			return TK_NONE;
		}

		funs._cur.Push_TableInsert(ar, iResultStack, iTempOffsetKey, iTempOffsetValue);

		tkType2 = GetToken(ar, tk2);
		if (tkType2 == TK_R_MIDDLE)
			break;
		if (tkType2 != TK_COMMA)
		{
			SetCompileError(ar, "Error (%d, %d): Table Init Error %s\n", ar.CurLine(), ar.CurCol(), tk2.c_str());
			return TK_NONE;
		}
	}
	if (iTableDeep == 0)
	{
		tkType1 = GetToken(ar, tk1);
		if (tkType1 != TK_SEMICOLON)
		{
			SetCompileError(ar, "Error (%d, %d): Table ; \n", ar.CurLine(), ar.CurCol());
			return TK_NONE;
		}
	}
	
	return tkType1;
}
TK_TYPE ParseTable(int& iArrayOffset, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	TK_TYPE r = TK_NONE;

	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
	if (r != TK_R_ARRAY)
	{
		SetCompileError(ar, "Error (%d, %d): Table ] \n", ar.CurLine(), ar.CurCol());
		return TK_NONE;
	}

	if (operand._iArrayIndex == INVALID_ERROR_PARSEJOB)
		iArrayOffset = operand._iVar;
	else
	{
		iArrayOffset = funs._cur.AllocLocalTempVar();
		funs._cur.Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iArrayOffset);
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
		SetCompileError(ar, "Error (%d, %d): String", ar.CurLine(), ar.CurCol());
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
				ar.PushToken(tkType2, tk2);

				iTempOffset._iArrayIndex = iArrayIndex;
				iArrayIndex = INVALID_ERROR_PARSEJOB;

				if (false == ParseFunCall(iTempOffset, tkTypePre, NULL, ar, funs, vars))
					return false;
				break;
			}
			else
			{
				ar.PushToken(tkType2, tk2);
				break;
			}
			if (iArrayIndex != INVALID_ERROR_PARSEJOB)
			{
				int iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(ar, iTempOffset._iVar, iArrayIndex, iTempOffset2);
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
					SetCompileError(ar, "Error (%d, %d): . string", ar.CurLine(), ar.CurCol());
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
				funs._cur.Push_TableRead(ar, iTempOffset._iVar, iArrayIndex, iTempOffset2);
				iTempOffset = iTempOffset2;

				iArrayIndex = INVALID_ERROR_PARSEJOB;
			}

			int iTempOffset3 = funs._cur.AllocLocalTempVar();
			funs._cur.Push_MOV(ar, NOP_MOV_MINUS, iTempOffset3, iTempOffset._iVar);
			iTempOffset = iTempOffset3;
		}
	}
	else
	{
		SFunctionInfo* pFun = funs.FindFun(tk1);
		if (pFun != NULL)
		{
			if (funs._cur._name == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
			{
				SetCompileError(ar, "Error (%d, %d): Call is Not Allow From Global Var", ar.CurLine(), ar.CurCol());
				return false;
			}
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
		SetCompileError(ar, "Error (%d, %d): type begin (", ar.CurLine(), ar.CurCol());
		return false;
	}

	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
	if (r != TK_R_SMALL)
	{
		SetCompileError(ar, "Error (%d, %d): type end )", ar.CurLine(), ar.CurCol());
		return TK_NONE;
	}

	int iTempOffset2;
	if (operand._iArrayIndex != INVALID_ERROR_PARSEJOB)
	{
		iTempOffset2 = funs._cur.AllocLocalTempVar();
		funs._cur.Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iTempOffset2);
	}
	else
		iTempOffset2 = operand._iVar;

	int iTempOffset3 = funs._cur.AllocLocalTempVar();
	int iPriority = 0;
	funs._cur.Push_ToType(ar, TokenToOP(tkTypePre, iPriority), iTempOffset3, iTempOffset2);
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
			SetCompileError(ar, "Error (%d, %d): Function End (%s)\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str());
			return TK_NONE;
		case TK_RETURN:
			if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
			{
				SetCompileError(ar, "Error (%d, %d): Global Init (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
				return TK_NONE;
			}
			iTempOffset.Reset();
			r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
			if (r != TK_SEMICOLON)
			{
				SetCompileError(ar, "Error (%d, %d): return", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_RETURN(ar, iTempOffset._iVar);
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
				SetCompileError(ar, "Error (%d, %d): )\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		case TK_STRING:
			ar.PushToken(tkType1, tk1);
			if (false == ParseString(iTempOffset, TK_NONE, ar, funs, vars))
			{
				return TK_NONE;
			}

			operands.push_back(iTempOffset);
			blApperOperator = true;
			break;
		case TK_QUOTE2:
		case TK_QUOTE1:
		{
			std::string str;
			if (false == GetQuotationString(ar, str, tkType1 == TK_QUOTE2 ? '"' : '\''))
			{
				SetCompileError(ar, "Error (%d, %d): String End Not Found\n", ar.CurLine(), ar.CurCol());
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

				operands.push_back(a);
				blApperOperator = true;
				break;
			}
			operators.push_back(tkType1);
			blApperOperator = false;
			break;
		case TK_MUL:		// *
		case TK_DIV:		// /
		case TK_PERCENT:	// %
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
				SetCompileError(ar, "Error (%d, %d): Invalide Operator", ar.CurLine(), ar.CurCol());
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
		case TK_PERCENT_EQ:	// %=
		{
			SOperand iTempVar;
			r = ParseJob(true, iTempVar, NULL, ar, funs, vars);
			if (TK_NONE == r)
				return TK_NONE;
			if (iTempVar._iVar == INVALID_ERROR_PARSEJOB)
			{
				SetCompileError(ar, "Error (%d, %d): = \n", ar.CurLine(), ar.CurCol());
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
				SetCompileError(ar, "Error (%d, %d): Syntex Var Def Error\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			break;
		case TK_BREAK:
			if(pJumps == NULL)
			{
				SetCompileError(ar, "Error (%d, %d): break error\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_JMP(ar, 0);
			pJumps->push_back(SJumpValue(funs._cur._code->GetBufferOffset() - 2, funs._cur._code->GetBufferOffset()));
			break;
		case TK_L_MIDDLE:
			iTempOffset.Reset();
			r = ParseTableDef(iTempOffset._iVar, ar, funs, vars);
			operands.push_back(iTempOffset);
			blApperOperator = true;
			blEnd = true;
			break;

		case TK_NULL:
			//iTempOffset._iVar = COMPILE_VAR_NULL;
			iTempOffset._iVar = funs._cur.AllocLocalTempVar();
			funs._cur.Push_OP1(ar, NOP_VAR_CLEAR, iTempOffset._iVar);
			operands.push_back(SOperand(iTempOffset._iVar));
			blApperOperator = true;
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
		case TK_TOSIZE:
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
				a._incementType = Increment_Prefix;
				funs._cur.Push_OP1(ar, tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				operands.push_back(a);
				blApperOperator = true;
			}
			else
			{	// 후위
				SOperand& a = operands[operands.size() - 1];
				if (IsTempVar(a._iVar))
				{
					SetCompileError(ar, "Error (%d, %d): Temp Var not Support (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
					return TK_NONE;
				}
				if (a._iArrayIndex != INVALID_ERROR_PARSEJOB)
				{
					SetCompileError(ar, "Error (%d, %d): Table Var not Support (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
					return TK_NONE;
				}
				if (a._incementType != Increment_None)
				{
					SetCompileError(ar, "Error (%d, %d): ++/-- invalid (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
					return TK_NONE;
				}
				int iTempOffset2;
				if (bReqReturn)
				{
					iTempOffset2 = funs._cur.AllocLocalTempVar();
					funs._cur.Push_MOV(ar, NOP_MOV, iTempOffset2, a._iVar);
				}
				funs._cur.Push_OP1(ar, tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				if (bReqReturn)
					a = SOperand(iTempOffset2);
			}
			break;
		default:
			SetCompileError(ar, "Error (%d, %d): Syntex (%s)\n", ar.CurLine(), ar.CurCol(), tk1.c_str());
			return TK_NONE;
		}
	}
	if (operands.empty() == true)
		return r;

	if (operands.size() != operators.size() + 1)
	{
		SetCompileError(ar, "Error (%d, %d): Invalid Operator", ar.CurLine(), ar.CurCol(), tk1.c_str());
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

		if (op <= NOP_PERSENT2)
		{
			if (a.IsArray() == false)
			{
				if (IsTempVar(a._iVar))
				{
					SetCompileError(ar, "Error (%d, %d): = lvalue \n", ar.CurLine(), ar.CurCol());
					return TK_NONE;
				}
			}
			if (b.IsArray() == false)
			{
				if (a.IsArray() == false)
				{
					funs._cur.Push_MOV(ar, op, a._iVar, b._iVar);
					//else
					//{
					//	if(op == NOP_MOV)
					//		funs._cur.Push_MOV(ar, NOP_MOV_FUNADDR, a._iVar, b._iVar - COMPILE_FUN_INDEX_BEGIN);
					//	else
					//	{
					//		SetCompileError(ar, "Operation Error : Function Address\n", ar.CurLine(), ar.CurCol());
					//		return TK_NONE;
					//	}
					//}
				}
				else
					funs._cur.Push_TableInsert(ar, a._iVar, a._iArrayIndex, b._iVar);
			}
			else
			{
				if (a.IsArray() == false)
					funs._cur.Push_TableRead(ar, b._iVar, b._iArrayIndex, a._iVar);
				else
				{
					int iTempOffset2 = funs._cur.AllocLocalTempVar();
					funs._cur.Push_TableRead(ar, b._iVar, b._iArrayIndex, iTempOffset2);
					funs._cur.Push_TableInsert(ar, a._iVar, a._iArrayIndex, iTempOffset2);
				}
			}
		}
		else
		{
			int iTempOffset2;

			if (a.IsArray() == true)
			{
				iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(ar, a._iVar, a._iArrayIndex, iTempOffset2);
				a._iVar = iTempOffset2;
			}
			if (b.IsArray() == true)
			{
				iTempOffset2 = funs._cur.AllocLocalTempVar();
				funs._cur.Push_TableRead(ar, b._iVar, b._iArrayIndex, iTempOffset2);
				b._iVar = iTempOffset2;
			}

			iTempOffset2 = funs._cur.AllocLocalTempVar();
			funs._cur.Push_OP(ar, op, iTempOffset2, a._iVar, b._iVar);
			operands[iFindOffset] = iTempOffset2;
		}
		RemoveAt<SOperand>(operands, iFindOffset + 1);
	}

	sResultStack = operands[0];
	return r;
}

eNOperation ConvertCheckOPToOptimize(eNOperation n)
{
	switch (n)
	{
	case NOP_GREAT:		// >
		return NOP_JMP_GREAT;
	case NOP_GREAT_EQ:	// >=
		return NOP_JMP_GREAT_EQ;
	case NOP_LESS:		// <
		return NOP_JMP_LESS;
	case NOP_LESS_EQ:	// <=
		return NOP_JMP_LESS_EQ;
	case NOP_EQUAL2:	// ==
		return NOP_JMP_EQUAL2;
	case NOP_NEQUAL:	// !=
		return NOP_JMP_NEQUAL;
	case NOP_AND:	// &&
		return NOP_JMP_AND;
	case NOP_OR:	// ||
		return NOP_JMP_OR;
	default:
		break;
	}
	return NOP_NONE;
}
eNOperation ConvertCheckOPToOptimizeInv(eNOperation n)
{
	switch (n)
	{
	case NOP_GREAT:		// >
		return NOP_JMP_LESS_EQ;
	case NOP_GREAT_EQ:	// >=
		return NOP_JMP_LESS;
	case NOP_LESS:		// <
		return NOP_JMP_GREAT_EQ;
	case NOP_LESS_EQ:	// <=
		return NOP_JMP_GREAT;
	case NOP_EQUAL2:	// ==
		return NOP_JMP_NEQUAL;
	case NOP_NEQUAL:	// !=
		return NOP_JMP_EQUAL2;
	case NOP_AND:	// &&
		return NOP_JMP_NAND;
	case NOP_OR:	// ||
		return NOP_JMP_NOR;
	default:
		break;
	}
	return NOP_NONE;
}

int  AddLocalVarName(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, const std::string& name, bool checkName = true)
{
	if (checkName && false == AbleName(name))
	{
		SetCompileError(ar, "Error (%d, %d): Function Local Var Unable (%s) %s", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), name.c_str());
		return -1;
	}
	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	if (pCurLayer->FindVarOnlyCurrentBlock(name) >= 0)
	{
		SetCompileError(ar, "Error (%d, %d): Function Local Var Already (%s) %s", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), name.c_str());
		return -1;
	}
	int iLocalVar;
	if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
		iLocalVar = COMPILE_GLOBAL_VAR_BEGIN + funs._cur._localVarCount++;
	else
		iLocalVar = 1 + (int)funs._cur._args.size() + funs._cur._localVarCount++; // 0 번은 리턴 저장용
	pCurLayer->AddLocalVar(name, iLocalVar);
	return iLocalVar;
}

//	for Init
//	for {} Process
//	for Increase
//	for Check
bool ParseFor(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs._cur._name == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetCompileError(ar, "Error (%d, %d): \"for\" is Not Allow From Global Var", ar.CurLine(), ar.CurCol());
		return false;
	}

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
		SetCompileError(ar, "Error (%d, %d): for '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());

	//==> for Init
	iTempOffset.Reset();
	r = ParseJob(false, iTempOffset, NULL, ar, funs, vars, true);
	if (TK_SEMICOLON != r) // 초기화
	{
		SetCompileError(ar, "Error (%d, %d): for Init", ar.CurLine(), ar.CurCol());
		return false;
	}

	funs._cur.Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur._code->GetBufferOffset() - (sizeof(short)*3), funs._cur._code->GetBufferOffset());

	int PosLoopTop = funs._cur._code->GetBufferOffset(); // Loop 의 맨위

	// For Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
	if (TK_SEMICOLON != r)
	{
		SetCompileError(ar, "Error (%d, %d): for Check", ar.CurLine(), ar.CurCol());
		return false;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur._code->GetBufferOffset();
	eNOperation opCheck = funs._cur.GetLastOP();
	bool isCheckOPOpt = false;
	eNOperation opOpz = NOP_NONE;
	if (IsTempVar(iStackCheckVar))
	{
		opOpz = ConvertCheckOPToOptimize(opCheck);
		if (opOpz != NOP_NONE)
		{
			isCheckOPOpt = true;
		}
	}
	funs._cur._code->SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = Pos2 - Pos1;
	if (iCheckCodeSize > sizeof(byTempCheck))
	{
		SetCompileError(ar, "Error (%d, %d): Check Size Over %d", ar.CurLine(), ar.CurCol(), iCheckCodeSize);
		return false;
	}
	funs._cur._code->Read(byTempCheck, iCheckCodeSize);
	funs._cur._code->SetPointer(Pos1, SEEK_SET);

	// For Increase
	iTempOffset = INVALID_ERROR_PARSEJOB;
	r = ParseJob(false, iTempOffset, NULL, ar, funs, vars); // 증감
	if (TK_R_SMALL != r)
	{
		SetCompileError(ar, "Error (%d, %d): for Inc", ar.CurLine(), ar.CurCol());
		return false;
	}
	Pos2 = funs._cur._code->GetBufferOffset();
	funs._cur._code->SetPointer(Pos1, SEEK_SET);
	int iIncCodeSize = Pos2 - Pos1;
	if (iIncCodeSize > sizeof(byTempInc))
	{
		SetCompileError(ar, "Error (%d, %d): Inc Size Over %d", ar.CurLine(), ar.CurCol(), iIncCodeSize);
		return false;
	}
	funs._cur._code->Read(byTempInc, iIncCodeSize);
	funs._cur._code->SetPointer(Pos1, SEEK_SET);

	//	for {} Process
	std::vector<SJumpValue> sJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			SetCompileError(ar, "Error (%d, %d): ;", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars))
			return false;
	}

	funs._cur._code->Write(byTempInc, iIncCodeSize);

	funs._cur.Set_JumpOffet(jmp1, funs._cur._code->GetBufferOffset());
	funs._cur._code->Write(byTempCheck, iCheckCodeSize);
	if(false == isCheckOPOpt)
		funs._cur.Push_JMPTrue(ar, iStackCheckVar, PosLoopTop);
	else
	{
		int argLen = sizeof(short) * 3;
		funs._cur._code->SetPointer(-((int)sizeof(OpType) + (int)sizeof(ArgFlag) + argLen), SEEK_CUR); // OP + flag + n1, n2, n3
		OpType optype = GetOpTypeFromOp(opOpz);
		funs._cur._code->Write(&optype, sizeof(optype));
		ArgFlag flag = 0;
		funs._cur._code->Write(&flag, sizeof(flag));
		int cur = funs._cur._code->GetBufferOffset();
		funs._cur.Set_JumpOffet(SJumpValue(cur, cur + argLen), PosLoopTop);
		funs._cur._code->SetPointer(argLen, SEEK_CUR);
	}
	funs._cur.ClearLastOP();


	int forEndPos = funs._cur._code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur.Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return true;
}
bool ParseForEach(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs._cur._name == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetCompileError(ar, "Error (%d, %d): \"foreach\" is Not Allow From Global Var", ar.CurLine(), ar.CurCol());
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
//	u8 byTempCheck[1024];

	// "foreach(var a, b in table)" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetCompileError(ar, "Error (%d, %d): foreach '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_VAR) // var
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'var' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING) // key
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'key' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}
	int iKey = AddLocalVarName(ar, funs, vars, tk1);
	if(iKey < 0)
		return false;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_COMMA) // ,
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'comma' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING) // value
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'value' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}
	int iValue = AddLocalVarName(ar, funs, vars, tk1);
	if (iValue < 0)
		return false;

	if (iKey + 1 != iValue)
	{
		SetCompileError(ar, "Error (%d, %d): foreach key / Value Var Alloc Error", ar.CurLine(), ar.CurCol());
		return false;
	}

	// Iterator를 저장할 임시 자동 생성 변수
	int iIterator = AddLocalVarName(ar, funs, vars, tk1 + "^_#@_temp_", false);
	if (iIterator < 0)
		return false;

	if (iKey + 2 != iIterator)
	{
		SetCompileError(ar, "Error (%d, %d): foreach key / Value Var Alloc Error", ar.CurLine(), ar.CurCol());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING || tk1 != "in") // in
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'in' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING) // Table Name
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'table_name' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}
	int iTable = vars.FindVar(tk1);
	if(iTable < 0)
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'talbe' Not Found %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_R_SMALL) // (
	{
		SetCompileError(ar, "Error (%d, %d): foreach ')' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}


	funs._cur.Push_OP1(ar, NOP_VAR_CLEAR, iKey);

	funs._cur.Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur._code->GetBufferOffset() - (sizeof(short) * 3), funs._cur._code->GetBufferOffset());

	int PosLoopTop = funs._cur._code->GetBufferOffset(); // Loop 의 맨위


	//	foreach {} Process
	std::vector<SJumpValue> sJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			SetCompileError(ar, "Error (%d, %d): ;", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars))
			return false;
	}

	funs._cur.Set_JumpOffet(jmp1, funs._cur._code->GetBufferOffset());
	funs._cur.Push_JMPForEach(ar, PosLoopTop, iTable, iKey);

	funs._cur.ClearLastOP();


	int forEndPos = funs._cur._code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur.Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return true;
}
bool ParseWhile(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs._cur._name == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetCompileError(ar, "Error (%d, %d): \"while\" is Not Allow From Global Var", ar.CurLine(), ar.CurCol());
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
	u8 byTempCheck[1024];

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetCompileError(ar, "Error (%d, %d): while '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());


	funs._cur.Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur._code->GetBufferOffset() - (sizeof(short) * 3), funs._cur._code->GetBufferOffset());

	int PosLoopTop = funs._cur._code->GetBufferOffset(); // Loop 의 맨위

	// While Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
	if (TK_R_SMALL != r)
	{
		SetCompileError(ar, "Error (%d, %d): while Check", ar.CurLine(), ar.CurCol());
		return false;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur._code->GetBufferOffset();
	eNOperation opCheck = funs._cur.GetLastOP();
	bool isCheckOPOpt = false;
	eNOperation opOpz = NOP_NONE;
	if (IsTempVar(iStackCheckVar))
	{
		opOpz = ConvertCheckOPToOptimize(opCheck);
		if (opOpz != NOP_NONE)
		{
			isCheckOPOpt = true;
		}
	}
	funs._cur._code->SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = Pos2 - Pos1;
	if (iCheckCodeSize > sizeof(byTempCheck))
	{
		SetCompileError(ar, "Error (%d, %d): Check Size Over %d", ar.CurLine(), ar.CurCol(), iCheckCodeSize);
		return false;
	}
	funs._cur._code->Read(byTempCheck, iCheckCodeSize);
	funs._cur._code->SetPointer(Pos1, SEEK_SET);



	//	while {} Process
	std::vector<SJumpValue> sJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			SetCompileError(ar, "Error (%d, %d): ;", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars))
			return false;
	}

	funs._cur.Set_JumpOffet(jmp1, funs._cur._code->GetBufferOffset());
	funs._cur._code->Write(byTempCheck, iCheckCodeSize);
	if (false == isCheckOPOpt)
		funs._cur.Push_JMPTrue(ar, iStackCheckVar, PosLoopTop);
	else
	{
		int argLen = sizeof(short) * 3;
		funs._cur._code->SetPointer(-((int)sizeof(OpType) + argLen), SEEK_CUR); // OP + n1, n2, n3
		OpType optype = GetOpTypeFromOp(opOpz);
		funs._cur._code->Write(&optype, sizeof(optype));
		int cur = funs._cur._code->GetBufferOffset();
		funs._cur.Set_JumpOffet(SJumpValue(cur, cur + argLen), PosLoopTop);
		funs._cur._code->SetPointer(argLen, SEEK_CUR);
	}
	funs._cur.ClearLastOP();

	int forEndPos = funs._cur._code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur.Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return true;
}
bool ParseIF(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs._cur._name == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetCompileError(ar, "Error (%d, %d): \"if\" is Not Allow From Global Var", ar.CurLine(), ar.CurCol());
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetCompileError(ar, "Error (%d, %d): for '(' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	//==> if( xxx )
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, pJumps, ar, funs, vars, true);
	if (TK_R_SMALL != r) // )
	{
		SetCompileError(ar, "Error (%d, %d): for Init", ar.CurLine(), ar.CurCol());
		return false;
	}

	SJumpValue jmp1;
	SJumpValue jmp2;

	eNOperation opCheck = funs._cur.GetLastOP();
	bool isCheckOPOpt = false;
	eNOperation opOpz = NOP_NONE;
	if (IsTempVar(iTempOffset._iVar))
	{
		opOpz = ConvertCheckOPToOptimizeInv(opCheck);
		if (opOpz != NOP_NONE)
		{
			isCheckOPOpt = true;
		}
	}
	if (false == isCheckOPOpt)
	{
		funs._cur.Push_JMPFalse(ar, iTempOffset._iVar, 0);
		jmp1.Set(funs._cur._code->GetBufferOffset() - 2, funs._cur._code->GetBufferOffset());
	}
	else
	{
		int argLen = sizeof(short) * 3;
		funs._cur._code->SetPointer(-((int)sizeof(OpType) + argLen), SEEK_CUR); // OP + n1, n2, n3
		OpType optype = GetOpTypeFromOp(opOpz);
		funs._cur._code->Write(&optype, sizeof(optype));
		int cur = funs._cur._code->GetBufferOffset();
		funs._cur.Set_JumpOffet(SJumpValue(cur, cur + argLen), 0);
		funs._cur._code->SetPointer(argLen, SEEK_CUR);

		jmp1.Set(funs._cur._code->GetBufferOffset() - 6, funs._cur._code->GetBufferOffset());
	}

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
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, pJumps, ar, funs, vars);

		if (TK_SEMICOLON != r)
		{
			SetCompileError(ar, "Error (%d, %d): if ;", ar.CurLine(), ar.CurCol());
			return false;
		}
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_ELSE)
	{
		funs._cur.Push_JMP(ar, 0);
		jmp2.Set(funs._cur._code->GetBufferOffset() - 2, funs._cur._code->GetBufferOffset());

		funs._cur.Set_JumpOffet(jmp1, funs._cur._code->GetBufferOffset());

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
			ar.PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, pJumps, ar, funs, vars);

			if (TK_SEMICOLON != r)
			{
				SetCompileError(ar, "Error (%d, %d): else ;", ar.CurLine(), ar.CurCol());
				return false;
			}
		}
		funs._cur.Set_JumpOffet(jmp2, funs._cur._code->GetBufferOffset());
	}
	else
	{
		ar.PushToken(tkType1, tk1);
		funs._cur.Set_JumpOffet(jmp1, funs._cur._code->GetBufferOffset());
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
		int iLocalVar = AddLocalVarName(ar, funs, vars, tk1);
		if (iLocalVar < 0)
			return false;

		ar.PushToken(tkType1, tk1);

		SOperand iTempLocalVar;
		r = ParseJob(false, iTempLocalVar, NULL, ar, funs, vars);
		if (TK_SEMICOLON != r)
		{
			//SetCompileError(ar, "Error (%d, %d): ", ar.CurLine(), ar.CurCol());
			return false;
		}
	}
	else
	{
		SetCompileError(ar, "Error (%d, %d): Function Local Var (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
		return false;
	}
	return true;
}
bool ParseSleep(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL)
	{
		SetCompileError(ar, "Error (%d, %d): '('", ar.CurLine(), ar.CurCol());
		return false;
	}
	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
	if (TK_R_SMALL != r)
	{
		SetCompileError(ar, "Error (%d, %d): ')'", ar.CurLine(), ar.CurCol());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_SEMICOLON)
	{
		SetCompileError(ar, "Error (%d, %d): ';'", ar.CurLine(), ar.CurCol());
		return false;
	}

	int iTempVar;
	if (operand._iArrayIndex == INVALID_ERROR_PARSEJOB)
		iTempVar = operand._iVar;
	else
	{
		iTempVar = funs._cur.AllocLocalTempVar();
		funs._cur.Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iTempVar);
	}
	funs._cur.Push_OP1(ar, NOP_SLEEP, iTempVar);

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
		eNOperation opLast = funs._cur.GetLastOP();
		if (opLast == NOP_MOV || opLast == NOP_MOV_MINUS)
		{
			int iVar = funs._cur.GetN(funs._cur._iLastOPOffset, 0);
			if (IsTempVar(iVar))
			{
				//funs._cur._code->SetPointer(funs._cur._iLastOPOffset - (ar._debug ? sizeof(debug_info) : 0), SEEK_SET);
				funs._cur._code->SetPointer(funs._cur._iLastOPOffset, SEEK_SET);
				funs._cur.ClearLastOP();
			}
		}

		tkType1 = GetToken(ar, tk1);
		switch (tkType1)
		{
		case TK_NONE:
			if (funs._cur._name != GLOBAL_INIT_FUN_NAME)
			{
				SetCompileError(ar, "Error (%d, %d): Function End (%s)", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str());
				return false;
			}
			blEnd = true;
			break;
		case TK_R_MIDDLE:
			if (funs._cur._name == GLOBAL_INIT_FUN_NAME)
			{
				SetCompileError(ar, "Error (%d, %d): Global Init (%s) %d", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
				return false;
			}
			blEnd = true;
			break;
		case TK_RETURN:
			ar.PushToken(tkType1, tk1);

			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, NULL, ar, funs, vars);
			if (TK_SEMICOLON != r)
			{
				SetCompileError(ar, "Error (%d, %d): return end is ;", ar.CurLine(), ar.CurCol());
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
				SetCompileError(ar, "Error (%d, %d): break error\n", ar.CurLine(), ar.CurCol());
				return TK_NONE;
			}
			funs._cur.Push_JMP(ar, 0);
			pJumps->push_back(SJumpValue(funs._cur._code->GetBufferOffset() - 2, funs._cur._code->GetBufferOffset()));
			break;
		case TK_IMPORT:
			funType = FUNT_IMPORT;
			break;
		case TK_EXPORT:
			funType = FUNT_EXPORT;
			break;
		case TK_FUN:
			tkType2 = GetToken(ar, tk2);
			if (false == AbleName(tk2))
			{
				SetCompileError(ar, "Error (%d, %d): Unable Fun Name %s\n", ar.CurLine(), ar.CurCol(), tk2.c_str());
				return false;
			}
			if (tkType2 == TK_STRING)
			{
				tkType3 = GetToken(ar, tk3);
				if (tkType3 == TK_L_SMALL) // 함수
				{
					SFunctionInfo save = funs._cur;
					funs._cur.Clear();

					funs._cur._name = tk2;
					funs._cur._funType = funType;
					funs._cur._code = &funs._codeLocal;
					funs._cur._pDebugData = &funs.m_sDebugLocal;
					if (false == ParseFunction(ar, funs, vars))
						return false;

					funs._cur = save;
				}
				else
				{
					SetCompileError(ar, "Error (%d, %d): Unknow Token(%d,%d) '%s'\n", ar.CurLine(), ar.CurCol(), tk2.c_str(), tkType3, tk2.c_str());
					return false;
				}
			}
			else
			{
				SetCompileError(ar, "Error (%d, %d): Function Name (%d) '%s'\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk2.c_str());
				return false;
			}
			break;

		case TK_SLEEP:
			if (false == ParseSleep(ar, funs, vars))
				return false;
			break;
		case TK_STRING:
			ar.PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, NULL, ar, funs, vars);
			if (TK_NONE == r)
			{
				//SetCompileError(ar, "Error (%d, %d): ", ar.CurLine(), ar.CurCol());
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
		case TK_FOREACH:
			if (false == ParseForEach(ar, funs, vars))
				return false;
			break;
		case TK_WHILE:
			if (false == ParseWhile(ar, funs, vars))
				return false;
			break;
		default:
			SetCompileError(ar, "Error (%d, %d): Function Name (%s) '%s'\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
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
	if (false == ParseMiddleArea(NULL, ar, funs, vars))
		return false;

	funs._cur.Push_FUNEND(ar);

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

	funs._cur._funID = (int)funs._funs.size() + 1;

	auto itF = funs._funs.find(funs._cur._name);
	if (itF != funs._funs.end())
	{
		funs._cur._funID = (*itF).second._funID;
	}

	funs._funs[funs._cur._name] = funs._cur; // 이름 먼저 등록
	funs._funIDs[funs._cur._funID] = funs._cur._name;
	funs._cur._staticIndex = funs.AddStaticFunction(funs._cur._funID);

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE)
	{
		if (tkType1 == TK_SEMICOLON)
		{

			DelVarsFunction(vars);
			return true;
		}
		SetCompileError(ar, "Error (%d, %d): Function Start (%s) %d\n", ar.CurLine(), ar.CurCol(), funs._cur._name.c_str(), tk1.c_str());
		return false;
	}

	funs._cur._iCode_Begin = funs._cur._code->GetBufferOffset();
	if (false == ParseFunctionBody(ar, funs, vars))
		return false;

	funs._cur._iCode_Size = funs._cur._code->GetBufferOffset() - funs._cur._iCode_Begin;
	auto it = funs._funs.find(funs._cur._name);
	(*it).second = funs._cur;

	DelVarsFunction(vars);

	return true;
}

bool Parse(CArchiveRdWC& ar, CNArchive&arw, bool putASM)
{
	ar.m_sTokenQueue.clear();

	SVars	vars;
	SFunctions funs;
	funs._cur._funID = 0;
	funs._cur._name = GLOBAL_INIT_FUN_NAME;
	funs._cur._code = &funs._codeGlobal;
	funs._cur._pDebugData = &funs.m_sDebugGlobal;
	funs._cur.Clear();

	SLayerVar* pCurLayer = AddVarsFunction(vars);

	funs.AddStaticString("system");

	bool r = ParseFunctionBody(ar, funs, vars);
	funs._cur._iCode_Size = funs._cur._code->GetBufferOffset();

	if (true == r)
	{
		if(false == Write(ar, arw, funs, vars))
			return false;
		if(putASM)
			WriteLog(ar, funs, vars);
	}

	DelVarsFunction(vars);

	while (vars._varsFunction.empty() == false)
		DelVarsFunction(vars);

	return r;
}

bool CNeoVM::Compile(void* pBufferSrc, int iLenSrc, CNArchive& arw, std::string& err, bool putASM, bool debug, bool allowGlobalInitLogic)
{
	CArchiveRdWC ar2;
	ToArchiveRdWC((const char*)pBufferSrc, iLenSrc, ar2);
	ar2._allowGlobalInitLogic = allowGlobalInitLogic;
	ar2._debug = debug;

	bool b = Parse(ar2, arw, putASM);
	if(b == false)
		err = ar2.m_sErrorString;
	return b;
}
CNeoVM*	CNeoVM::CompileAndLoadVM(void* pBufferSrc, int iLenSrc, std::string& err, bool putASM, bool debug, bool allowGlobalInitLogic, int iStackSize)
{
	CNArchive arCode;

	if (false == Compile(pBufferSrc, iLenSrc, arCode, err, putASM, debug, allowGlobalInitLogic))
	{
		return NULL;
	}

	//if(putASM)
	//	SetCompileError(ar, "Comile Success. Code : %d bytes !!\n\n", arCode.GetBufferOffset());

	CNeoVM* pVM = CNeoVM::LoadVM(arCode.GetData(), arCode.GetBufferOffset(), iStackSize);

	return pVM;
}
