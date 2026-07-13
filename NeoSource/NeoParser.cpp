#include "NeoParser.h"
#include "NeoVMImpl.h"
#include "NeoExport.h"
#include "UTFString.h"
#include <algorithm>

namespace NeoScript
{

//#define CSTYLE_FOR

void	SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...);

#define PARSER_COMPILE_ERROR_LIST(X) \
	X(PCE_INVALID_LOCAL_NAME, "Error (%d, %d): invalid local variable name in function '%s': '%s'") \
	X(PCE_DUPLICATE_LOCAL_NAME, "Error (%d, %d): local variable '%s' is already defined in function '%s'") \
	X(PCE_INVALID_EXPORT_SCOPE, "Error (%d, %d): export is only allowed at the global scope. Current function: '%s', name: '%s'") \
	X(PCE_IMPORT_FAILED, "Error (%d, %d): failed to import module '%s'") \
	X(PCE_DUPLICATE_FUNCTION_ARGUMENT, "Error (%d, %d): duplicate function argument '%s'") \
	X(PCE_EXPECTED_FUNCTION_ARGUMENT, "Error (%d, %d): expected a function argument name, but found '%s'") \
	X(PCE_EXPECTED_COMMA_BETWEEN_ARGUMENTS, "Error (%d, %d): expected ',' between function arguments") \
	X(PCE_INVALID_CALL_ARGUMENT, "Error (%d, %d): invalid function call argument") \
	X(PCE_ARGUMENT_COUNT_MISMATCH, "Error (%d, %d): function argument count mismatch. Expected %d, got %d") \
	X(PCE_EXPECTED_LEFT_PAREN, "Error (%d, %d): expected '('") \
	X(PCE_EXPECTED_RIGHT_PAREN, "Error (%d, %d): expected ')'") \
	X(PCE_EXPECTED_RIGHT_BRACKET, "Error (%d, %d): expected ']'") \
	X(PCE_INVALID_NUMBER_LITERAL, "Error (%d, %d): invalid numeric literal") \
	X(PCE_UNKNOWN_IDENTIFIER, "Error (%d, %d): unknown identifier '%s'") \
	X(PCE_UNTERMINATED_STRING, "Error (%d, %d): unterminated string literal") \
	X(PCE_EXPECTED_MEMBER_NAME, "Error (%d, %d): expected a member name after '.'") \
	X(PCE_GLOBAL_CALL_NOT_ALLOWED, "Error (%d, %d): function calls are not allowed in global variable initializers") \
	X(PCE_INVALID_FUNCTION_CALL, "Error (%d, %d): invalid function call") \
	X(PCE_EXPECTED_LOGIC_NOT_OPERAND, "Error (%d, %d): expected an expression after '!'") \
	X(PCE_EXPECTED_TYPE_LEFT_PAREN, "Error (%d, %d): expected '(' after type conversion") \
	X(PCE_EXPECTED_TYPE_RIGHT_PAREN, "Error (%d, %d): expected ')' after type conversion") \
	X(PCE_UNEXPECTED_FUNCTION_END, "Error (%d, %d): unexpected end of function '%s'") \
	X(PCE_UNEXPECTED_GLOBAL_BLOCK_END, "Error (%d, %d): unexpected '}' in global initialization") \
	X(PCE_INVALID_RETURN_STATEMENT, "Error (%d, %d): invalid return statement") \
	X(PCE_INVALID_ASSIGNMENT, "Error (%d, %d): expected an assignment value after '='") \
	X(PCE_VAR_DECLARATION_NOT_ALLOWED, "Error (%d, %d): variable declarations are not allowed here") \
	X(PCE_BREAK_OUTSIDE_LOOP, "Error (%d, %d): 'break' can only be used inside a loop") \
	X(PCE_CONTINUE_OUTSIDE_LOOP, "Error (%d, %d): 'continue' can only be used inside a loop") \
	X(PCE_EXPECTED_BREAK_SEMICOLON, "Error (%d, %d): expected ';' after 'break'") \
	X(PCE_EXPECTED_CONTINUE_SEMICOLON, "Error (%d, %d): expected ';' after 'continue'") \
	X(PCE_TEMP_VAR_UNSUPPORTED, "Error (%d, %d): temporary variables do not support '%s'") \
	X(PCE_TABLE_VAR_UNSUPPORTED, "Error (%d, %d): table values do not support '%s'") \
	X(PCE_INVALID_INCREMENT_TARGET, "Error (%d, %d): invalid target for '++' or '--': '%s'") \
	X(PCE_INVALID_OPERATOR, "Error (%d, %d): invalid operator near '%s'") \
	X(PCE_SYNTAX_ERROR, "Error (%d, %d): syntax error near '%s'") \
	X(PCE_EXPECTED_LVALUE, "Error (%d, %d): assignment target must be writable") \
	X(PCE_LOGIC_NOT_ALLOWED_GLOBAL, "Error (%d, %d): '%s' is not allowed in global initialization") \
	X(PCE_EXPECTED_TOKEN, "Error (%d, %d): expected %s, but found '%s'") \
	X(PCE_CODE_BLOCK_TOO_LARGE, "Error (%d, %d): generated %s code is too large (%d bytes)") \
	X(PCE_INVALID_FOR_INIT, "Error (%d, %d): invalid for-loop initializer") \
	X(PCE_INVALID_FOR_CONDITION, "Error (%d, %d): invalid for-loop condition") \
	X(PCE_INVALID_FOR_INCREMENT, "Error (%d, %d): invalid for-loop increment expression") \
	X(PCE_INVALID_LOOP_VARIABLE_LAYOUT, "Error (%d, %d): internal loop variable layout error") \
	X(PCE_INVALID_STATEMENT_END, "Error (%d, %d): expected ';' at the end of the statement") \
	X(PCE_INVALID_FUNCTION_NAME, "Error (%d, %d): invalid function name '%s'") \
	X(PCE_UNEXPECTED_TOKEN, "Error (%d, %d): unexpected token '%s' in function '%s'") \
	X(PCE_CONST_NOT_GLOBAL, "Error (%d, %d): 'const' is only allowed at the global scope") \
	X(PCE_CONST_DUPLICATE, "Error (%d, %d): const name '%s' is already defined (const, define, variable or function)") \
	X(PCE_CONST_INVALID_VALUE, "Error (%d, %d): const expression must be a compile-time constant, near '%s'") \
	X(PCE_CONST_INVALID_OP, "Error (%d, %d): invalid operation in const expression near '%s'") \
	X(PCE_CONST_DIV_ZERO, "Error (%d, %d): division by zero in const expression") \
	X(PCE_VM_NOT_INITIALIZED, "Please call NeoScript::INeoVM::Initialize() before compiling scripts")

enum EParserCompileError
{
#define X(name, message) name,
	PARSER_COMPILE_ERROR_LIST(X)
#undef X
	PCE_COUNT
};

static const char* g_sParserCompileErrors[PCE_COUNT] =
{
#define X(name, message) message,
	PARSER_COMPILE_ERROR_LIST(X)
#undef X
};

static_assert(sizeof(g_sParserCompileErrors) / sizeof(g_sParserCompileErrors[0]) == PCE_COUNT, "Parser compile error table mismatch");

template<typename... Args>
static void SetParserCompileError(CArchiveRdWC& ar, EParserCompileError error, Args... args)
{
	SetCompileError(ar, g_sParserCompileErrors[error], ar.CurLine(), ar.CurCol(), args...);
}

static void SetParserCompileError(CArchiveRdWC& ar, EParserCompileError error)
{
	SetCompileError(ar, g_sParserCompileErrors[error], ar.CurLine(), ar.CurCol());
}

#if 0
inline bool	IsShort(int v) { return (-32768 <= v && v <= 32767) ? true : false; }
#endif


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

enum OperandType
{
	Data_None,
	Data_Fun,
	Data_NR,	// None + [ref]
	Data_NS,	// None + Short
	Data_TR,	// Table + [ref]
	Data_TS,	// Table + Short
	Increment_Prefix,
	Increment_Postfix,
};

struct SOperand
{
	int _iVar;
	int _iArrayIndex;
	OperandType _operandType;

	SOperand()
	{
		Reset();
	}
	SOperand(int iVar, int iArrayIndex = INVALID_ERROR_PARSEJOB, OperandType operandType = Data_NR)
	{
		_iVar = iVar;
		_iArrayIndex = iArrayIndex;
		if(iVar == INVALID_ERROR_PARSEJOB && iArrayIndex == INVALID_ERROR_PARSEJOB)
			_operandType = Data_None;
		else
			_operandType = operandType;
	}
	void Reset()
	{
		_iVar = _iArrayIndex = INVALID_ERROR_PARSEJOB;
		_operandType = Data_NR;
	}

	inline bool IsInvalidValue() { return (false == IsHaveShort()) && (_iVar == INVALID_ERROR_PARSEJOB); }

	inline bool IsArray() { return _iArrayIndex != INVALID_ERROR_PARSEJOB; }
	inline bool IsShort() { return _operandType == Data_NS; }
	inline bool IsConst() { return _operandType == Data_NS; }
	inline bool IsNone() { return _operandType == Data_None; }
	inline bool IsHaveShort() { return (_operandType == Data_NS || _operandType == Data_TS); }
	inline bool IsFun() { return _operandType == Data_Fun; }
};

struct SOperationInfo
{
	std::string _str;
	eNOperation	_op;
	int			_op_length;
	eNOperation		_opType;
	SOperationInfo() {}
	SOperationInfo(const char* p, eNOperation	op, int len)
	{
		_str = p;
		_op = op;
		_op_length = len;
		_opType = op;
	}
};

#define OP_STR1(op, len) if(op > 255){ blError = true;} g_sOpInfo[op] = SOperationInfo(#op, op, len)


struct STokenValue
{
	std::string _str;
	char		_iPriority; // lower value is higher priority
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

TK_TYPE ParseJob(bool bReqReturn, SOperand& sResultStack, std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool bAllowVarDef = false, TK_TYPE tkEnd1 = TK_SEMICOLON, TK_TYPE tkEnd2 = TK_COMMA, TK_TYPE tkEnd3 = TK_R_SMALL, TK_TYPE tkEnd4 = TK_R_ARRAY, std::vector<SJumpValue>* pContinueJumps = NULL);
bool ParseVarDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool blExport);
bool ParseMiddleArea(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool* lastOPReturn = NULL, std::vector<SJumpValue>* pContinueJumps = NULL);
bool ParseFunctionBody(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool addOPFunEnd = true);

eNOperation GetOpTypeFromOp(eNOperation op)
{
	return g_sOpInfo[op]._opType;
}
eNOperation GetTableOpTypeFromOp(eNOperation op)
{
	switch (op)
	{
	case NOP_MOV: op = NOP_CLT_MOV; break;
	case NOP_ADD2: op = NOP_TABLE_ADD2; break;
	case NOP_SUB2: op = NOP_TABLE_SUB2; break;
	case NOP_MUL2: op = NOP_TABLE_MUL2; break;
	case NOP_DIV2: op = NOP_TABLE_DIV2; break;
	case NOP_PERSENT2: op = NOP_TABLE_PERSENT2; break;

	default: break;
	}
	return g_sOpInfo[op]._opType;
}
eNOperation GetListOpTypeFromOp(eNOperation op)
{
	switch (op)
	{
	case NOP_MOV: op = NOP_CLT_MOV; break;
	default: break;
	}
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
	TOKEN_STR2(TK_CONST, "const");
	TOKEN_STR2(TK_FUN, "fun");
	TOKEN_STR2(TK_CLASS, "class");
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
	TOKEN_STR2(TK_CONTINUE, "continue");
	TOKEN_STR2(TK_IF, "if");
	TOKEN_STR2(TK_ELSE, "else");
	TOKEN_STR2(TK_ELSEIF, "elif");
	TOKEN_STR2(TK_FOR, "for");
	TOKEN_STR2(TK_FOREACH, "foreach");
	TOKEN_STR2(TK_WHILE, "while");
	TOKEN_STR2(TK_TRUE, "true");
	TOKEN_STR2(TK_FALSE, "false");
	TOKEN_STR2(TK_NULL, "null");

	TOKEN_STR3(TK_PLUS2, "++", 1, NOP_INC);
	TOKEN_STR3(TK_MINUS2, "--", 1, NOP_DEC);

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

	TOKEN_STR3(TK_LSHIFT, "<<", 6, NOP_LSHIFT3);
	TOKEN_STR3(TK_LSHIFT_EQ, "<<=", 15, NOP_LSHIFT2);
	TOKEN_STR3(TK_RSHIFT, ">>", 6, NOP_RSHIFT3);
	TOKEN_STR3(TK_RSHIFT_EQ, ">>=", 15, NOP_RSHIFT2);

	TOKEN_STR2(TK_NOT, "~");
	TOKEN_STR3(TK_XOR, "^", 9, NOP_XOR3);
	TOKEN_STR3(TK_XOR_EQ, "^=", 15, NOP_XOR2);
	TOKEN_STR3(TK_EQUAL, "=", 15, NOP_MOV);
	TOKEN_STR3(TK_EQUAL_EQ, "==", 8, NOP_EQUAL2);
	TOKEN_STR3(TK_EQUAL_NOT, "!=", 8, NOP_NEQUAL);

	TOKEN_STR3(TK_AND, "&", 9, NOP_AND);
	TOKEN_STR3(TK_AND_EQ, "&=", 15, NOP_AND2);
	TOKEN_STR3(TK_AND2, "&&", 12, NOP_LOG_AND);
	TOKEN_STR3(TK_OR, "|", 11, NOP_OR);
	TOKEN_STR3(TK_OR_EQ, "|=", 15, NOP_OR2);
	TOKEN_STR3(TK_OR2, "||", 13, NOP_LOG_OR);

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
	TOKEN_STR2(TK_LOGIC_NOT, "!");

	TOKEN_STR2(TK_YIELD, "yield");

	/////////////////////////////////////////////////////

	OP_STR1(NOP_NONE, 0);
	OP_STR1(NOP_MOV, 2);
	OP_STR1(NOP_MOVI, 3);
	OP_STR1(NOP_MOV_MINUS, 2);
	OP_STR1(NOP_LOG_NOT, 2);
	OP_STR1(NOP_ADD2, 2);
	OP_STR1(NOP_SUB2, 2);
	OP_STR1(NOP_MUL2, 2);
	OP_STR1(NOP_DIV2, 2);
	OP_STR1(NOP_PERSENT2, 2);
	OP_STR1(NOP_LSHIFT2, 2);
	OP_STR1(NOP_RSHIFT2, 2);
	OP_STR1(NOP_AND2, 2);
	OP_STR1(NOP_OR2, 2);
	OP_STR1(NOP_XOR2, 2);

	OP_STR1(NOP_LSHIFT3, 3);
	OP_STR1(NOP_RSHIFT3, 3);
	

	OP_STR1(NOP_VAR_CLEAR, 1);
	OP_STR1(NOP_INC, 1);
	OP_STR1(NOP_DEC, 1);

	OP_STR1(NOP_ADD3, 3);
	OP_STR1(NOP_SUB3, 3);
	OP_STR1(NOP_MUL3, 3);
	OP_STR1(NOP_DIV3, 3);
	OP_STR1(NOP_PERSENT3, 3);
	OP_STR1(NOP_PERSENT3, 3);
	OP_STR1(NOP_LSHIFT3, 3);
	OP_STR1(NOP_AND2, 3);
	OP_STR1(NOP_OR2, 3);
	OP_STR1(NOP_XOR3, 3);

	OP_STR1(NOP_GREAT, 3);
	OP_STR1(NOP_GREAT_EQ, 3);
	OP_STR1(NOP_LESS, 3);
	OP_STR1(NOP_LESS_EQ, 3);
	OP_STR1(NOP_EQUAL2, 3);
	OP_STR1(NOP_NEQUAL, 3);
	OP_STR1(NOP_AND, 3);
	OP_STR1(NOP_OR, 3);
	OP_STR1(NOP_LOG_AND, 3);
	OP_STR1(NOP_LOG_OR, 3);

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
	OP_STR1(NOP_JMP_FOR, 3);
	OP_STR1(NOP_JMP_FOREACH, 3);

	OP_STR1(NOP_STR_ADD, 3);

	OP_STR1(NOP_TOSTRING, 2);
	OP_STR1(NOP_TOINT, 2);
	OP_STR1(NOP_TOFLOAT, 2);
	OP_STR1(NOP_TOSIZE, 2);
	OP_STR1(NOP_GETTYPE, 2);
	OP_STR1(NOP_SLEEP, 2);

	OP_STR1(NOP_FMOV1, 2);
	OP_STR1(NOP_FMOV2, 3);

	OP_STR1(NOP_JMP, 1);
	OP_STR1(NOP_JMP_FALSE, 2);
	OP_STR1(NOP_JMP_TRUE, 2);

	OP_STR1(NOP_CALL, 3);
	OP_STR1(NOP_PTRCALL, 3);
	OP_STR1(NOP_PTRCALL2, 3);
	OP_STR1(NOP_NATIVECALL, 3);
	OP_STR1(NOP_RETURN, 1);
//	OP_STR1(NOP_FUNEND, 0);

	OP_STR1(NOP_TABLE_ALLOC, 3);
	OP_STR1(NOP_CLT_READ, 3);
	OP_STR1(NOP_TABLE_REMOVE, 2);
	OP_STR1(NOP_CLT_MOV, 3);
	OP_STR1(NOP_TABLE_ADD2, 3);
	OP_STR1(NOP_TABLE_SUB2, 3);
	OP_STR1(NOP_TABLE_MUL2, 3);
	OP_STR1(NOP_TABLE_DIV2, 3);
	OP_STR1(NOP_TABLE_PERSENT2, 3);

	OP_STR1(NOP_LIST_ALLOC, 3);
	OP_STR1(NOP_LIST_REMOVE, 2);

	OP_STR1(NOP_VERIFY_TYPE, 2);
	OP_STR1(NOP_CHANGE_INT, 1);
	OP_STR1(NOP_YIELD, 1);

	if (blError)
	{
		InitDefaultData();
		return 0;
	}

	return 1;
}
static bool g_bInitVM = false;
INeoLoader* g_NeoLoader = nullptr;

std::string GetTokenString(TK_TYPE tk)
{
	if(tk < 0 || tk >= TK_MAX)
		return "";
	return g_sTokenToString[tk]._str;
}

#define GLOBAL_INIT_FUN_NAME	"##_global"
int ParseFunctionBase(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, std::string fname, FUNCTION_TYPE funType); // -1 : error


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

TK_TYPE GetToken(CArchiveRdWC& ar, std::string& tk);

static TK_TYPE CompileDefineTokenToToken(const NeoCompileDefineToken& defineToken, std::string& tk)
{
	tk = defineToken.text;
	switch (defineToken.type)
	{
	case NEO_DEFINE_TOKEN_TRUE:
//		if (tk.empty()) tk = "true"; // 이렇게 하는것 보다 Define 을 전달하는 곳에서 잘하면 됨.
		return TK_TRUE;
	case NEO_DEFINE_TOKEN_FALSE:
//		if (tk.empty()) tk = "false";
		return TK_FALSE;
	case NEO_DEFINE_TOKEN_NULL:
//		if (tk.empty()) tk = "null";
		return TK_NULL;
	case NEO_DEFINE_TOKEN_STRING:
		return TK_STRING_LITERAL; // 완성된 문자열 리터럴 (따옴표 재파싱 없이 tk 가 곧 내용)
	case NEO_DEFINE_TOKEN_IDENTIFIER:
	case NEO_DEFINE_TOKEN_INT:
	case NEO_DEFINE_TOKEN_FLOAT:
	default:
		return TK_STRING;
	}
}

static TK_TYPE ApplyCompileDefine(CArchiveRdWC& ar, TK_TYPE tkType, std::string& tk)
{
	if (tkType != TK_STRING || ar.m_bSuppressDefines)
		return tkType;

	// 스크립트 const (모듈-로컬) 우선 조회 — 호스트 define 과의 충돌은 선언 시점에 에러 처리됨
	auto its = ar.m_sScriptDefines.values.find(tk);
	if (its != ar.m_sScriptDefines.values.end())
		return CompileDefineTokenToToken((*its).second, tk);

	if (ar.m_pDefines == nullptr)
		return tkType;

	auto it = ar.m_pDefines->values.find(tk);
	if (it == ar.m_pDefines->values.end())
		return tkType;

	const NeoCompileDefineToken& f = (*it).second;
	return CompileDefineTokenToToken(f, tk);
}


TK_TYPE CalcToken(TK_TYPE tkTypeOnySingleChar, CArchiveRdWC& ar, std::string& tk)
{
	if (false == tk.empty())
		return ApplyCompileDefine(ar, CalcStringToken(tk), tk);

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
		u16 c3 = ar.GetData(false);

		// check 3 char token
		s1 += (u8)c3;
		TK_TYPE tkTemp2 = CalcStringToken(s1);
		if (tkTemp2 != TK_STRING && tkTemp2 != TK_NONE)
		{
			ar.GetData(true);
			tk = GetTokenString(tkTemp2);
			return tkTemp2;
		}

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

		//str.push_back((char)c1);
		utf_string::UNICODE_UTF8_ONE(c1, str);
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
				return ApplyCompileDefine(ar, CalcStringToken(tk), tk);
			ar.GetData(true);
			ar.AddLine();
			break;
		case ' ':
		case '\r':
		case '\t':
			if (false == tk.empty())
				return ApplyCompileDefine(ar, CalcStringToken(tk), tk);
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
			return CalcToken(TK_NOT, ar, tk);
		case '^':
			return CalcToken(TK_XOR, ar, tk);
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
			return CalcToken(TK_LOGIC_NOT, ar, tk);

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

bool UseableName(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, const std::string& name, bool checkName)
{
	if (checkName && false == AbleName(name))
	{
		SetParserCompileError(ar, PCE_INVALID_LOCAL_NAME, funs.GetCurFunName().c_str(), name.c_str());
		return false;
	}
	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	if (pCurLayer->FindVarOnlyCurrentBlock(name) != -1)
	{
		SetParserCompileError(ar, PCE_DUPLICATE_LOCAL_NAME, name.c_str(), funs.GetCurFunName().c_str());
		return false;
	}
	return true;
}

int  AddLocalVarName(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool blExport, const std::string& name, bool checkName = true)
{
	if(false == UseableName(ar, funs, vars, name, checkName))
		return -1;
	int iLocalVar;
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME)
		iLocalVar = COMPILE_GLOBAL_VAR_BEGIN - vars._globalVarCount++;
	else
		iLocalVar = 1 + (int)funs._cur->_args.size() + funs._cur->_localVarCount++; // 0 번은 리턴 저장용
	SLayerVar* pCurLayer = vars.GetCurrentLayer();
	pCurLayer->AddLocalVar(name, iLocalVar);
	if (funs.GetCurFunName() != GLOBAL_INIT_FUN_NAME && iLocalVar > 0)
		funs._cur->_debugVarNames[iLocalVar] = name;
	if (blExport)
	{
		if(vars._varsFunction.size() == 1)
			vars._varsExport.push_back(name);
		else
		{
			SetParserCompileError(ar, PCE_INVALID_EXPORT_SCOPE, funs.GetCurFunName().c_str(), name.c_str());
			return -1;
		}
	}
	return iLocalVar;
}
int  AddLocalVar(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	char name[256];
	snprintf(name, _countof(name), "^_#@_temp_%d", vars._iTempVarNameIndex++);

	if (pCurLayer->FindVarOnlyCurrentBlock(name) != -1)
	{
		SetParserCompileError(ar, PCE_DUPLICATE_LOCAL_NAME, name, funs.GetCurFunName().c_str());
		return -1;
	}
	int iLocalVar;
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME)
		iLocalVar = COMPILE_GLOBAL_VAR_BEGIN - vars._globalVarCount++;
	else
		iLocalVar = 1 + (int)funs._cur->_args.size() + funs._cur->_localVarCount++; // 0 번은 리턴 저장용
	pCurLayer->AddLocalVar(name, iLocalVar);
	return iLocalVar;
}
void ClearTempVars(SFunctions& funs)
{
	funs._cur->FreeLocalTempVar();
	eNOperation opLast = funs._cur->GetLastOP();
	if (opLast == NOP_MOV || opLast == NOP_MOV_MINUS)
	{
		int iVar = funs._cur->GetN(funs._cur->_iLastOPOffset, 0);
		if (IsTempVar(iVar))
		{
			//funs._cur->_code->SetPointer(funs._cur->_iLastOPOffset - (ar._debug ? sizeof(debug_info) : 0), SEEK_SET);
			funs._cur->_code->SetPointer(funs._cur->_iLastOPOffset, SEEK_SET);
			funs._cur->ClearLastOP();
		}
	}
	else if(opLast == NOP_CALL || opLast == NOP_PTRCALL2)
	{
		SVMOperation* op = funs._cur->GetOPPointer(funs._cur->_iLastOPOffset); // 3rd
		if (IsTempVar(op->n3))
		{
			op->argFlag |= NEOS_OP_CALL_NORESULT;
		}
	}
}
void AddBuildinFunction(CArchiveRdWC& ar, SFunctions& funs, const std::string& fname, int argc)
{
	SFunctionInfo* pF = funs.NewFun(fname, nullptr, funs._cur->_pDebugData);
	pF->_funType = FUNT_BUILT_IN;
	pF->_name = "#" + fname;
	pF->_moduleName = ar.m_sModuleName;

	pF->_built_in_arg_c = argc;
	pF->_funID = -1;
	//funs._funIDs[pF->_funID] = pF;
}
bool AddBuildinModule(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, const std::list< SystemFun>* p)
{
	SFunctionLayer* pLayerBackup = funs._curModule;

	for(auto it = (*p).begin(); it != (*p).end(); it++)
	{
		const SystemFun& f = (*it);
		AddBuildinFunction(ar, funs, f.fname, f.argCount);
	}
	return true;
}
bool ParseImport(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string fileName, tk2;
	TK_TYPE tkType1, tkType2;

	tkType1 = GetToken(ar, fileName);
	if (tkType1 != TK_STRING)
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "module name after 'import'", fileName.c_str());
		return false;
	}
	std::string defName = fileName; // default equal
	std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
	tkType2 = GetToken(ar, tk2);
	if (tkType2 == TK_STRING && tk2 == "as")
	{
		tkType2 = GetToken(ar, tk2);
		if (tkType2 == TK_STRING)
		{
			defName = tk2;
			tkType2 = GetToken(ar, tk2);
		}
	}

	if (tkType2 != TK_SEMICOLON)
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "';' after import statement", tk2.c_str());
		return false;
	}

	if(AbleName(defName) == false)
	{
		SetParserCompileError(ar, PCE_INVALID_LOCAL_NAME, funs.GetCurFunName().c_str(), defName.c_str());
		return false;
	}



	auto it = vars.m_sImports.find(fileName);
	if (it != vars.m_sImports.end())
	{
		funs._curModule->_defModules[defName] = (*it).second;
		return true;
	}

	std::string fullFileName;
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (g_NeoLoader)
	{
		const char* libPath = g_NeoLoader->GetLibPath();
		if(libPath != nullptr)
		{
			fullFileName = libPath;
			//fullFileName = "../../Lib/";
			fullFileName += fileName + ".ns";

			if(false == g_NeoLoader->Load(fullFileName.c_str(), pFileBuffer, iFileLen))
			{
				//SetCompileError(ar, "Error (%d, %d): Import Error (%s)", ar.CurLine(), ar.CurCol(), tk2.c_str());
				//return false;
			}
		}
	}
	if(pFileBuffer == nullptr)
	{
		const std::list< SystemFun>* p = CNeoVMImpl::GetSystemModule(fileName);
		if (p) // Built-in Import
		{
			SFunctionLayer* pLayerBackup = funs._curModule;
			funs._curModule = funs.NewLayer();
			funs._curModule->_blBuiltInModule = true;

			AddBuildinModule(ar, funs, vars, p);

			vars.m_sImports[fileName] = funs._curModule;
			pLayerBackup->_defModules[defName] = funs._curModule;
			funs._curModule = pLayerBackup;
			return true;
		}
		SetParserCompileError(ar, PCE_IMPORT_FAILED, fileName.c_str());
		return false;
	}

	CArchiveRdWC ar2;
	ToArchiveRdWC((const char*)pFileBuffer, iFileLen, ar2);
	ar2._allowGlobalInitLogic = ar._allowGlobalInitLogic;
	ar2._debug = ar._debug;
	ar2.m_pDefines = ar.m_pDefines;
	ar2.m_sModuleName = fileName;
	ar2.m_pDebugSourceFiles = ar.m_pDebugSourceFiles;
	if (ar2.m_pDebugSourceFiles != nullptr)
	{
		size_t fileSeq = ar2.m_pDebugSourceFiles->size();
		if (fileSeq <= 0xffff)
		{
			ar2.m_iFileSeq = (u16)fileSeq;
			ar2.m_pDebugSourceFiles->push_back(fullFileName);
		}
	}

	SFunctionLayer* pLayerBackup = funs._curModule;
	funs._curModule = funs.NewLayer();
	/////////////////////////////////
	bool r = ParseFunctionBody(ar2, funs, vars, false);
	/////////////////////////////////
	vars.m_sImports[fileName] = funs._curModule;
	pLayerBackup->_defModules[defName] = funs._curModule;
	funs._curModule = pLayerBackup;

	u16* pBuffer = ar2.GetBuffer();
	if(g_NeoLoader)
		g_NeoLoader->Unload(fullFileName.c_str(), pBuffer, iFileLen);

	ar.m_sErrorString = ar2.m_sErrorString;
	return r;
//#else
//	SetCompileError(ar, "Error (%d, %d): Import Not Defined)", ar.CurLine(), ar.CurCol());
//	return false;
//#endif
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
				auto it = funs._cur->_args.find(tk2);
				if (it != funs._cur->_args.end())
				{
					SetParserCompileError(ar, PCE_DUPLICATE_FUNCTION_ARGUMENT, tk2.c_str());
					return false;
				}
				int iArg = 1 + (int)funs._cur->_args.size();
				pCurLayer->AddLocalVar(tk2, iArg);
				funs._cur->_debugVarNames[iArg] = tk2;
				funs._cur->_args.insert(tk2);
				bPreviusComa = false;
			}
			else
			{
				SetParserCompileError(ar, PCE_EXPECTED_FUNCTION_ARGUMENT, tk1.c_str());
				return false;
			}
			break;
		case TK_R_SMALL:
			if (bPreviusComa)
			{
				SetParserCompileError(ar, PCE_EXPECTED_FUNCTION_ARGUMENT, ")");
				return false;
			}
			blEnd = true;
			break;
		case TK_COMMA:
			if (funs._cur->_args.size() == 0 || bPreviusComa)
			{
				SetParserCompileError(ar, PCE_EXPECTED_COMMA_BETWEEN_ARGUMENTS);
				return false;
			}
			bPreviusComa = !bPreviusComa;
			break;
		default:
			SetParserCompileError(ar, PCE_EXPECTED_FUNCTION_ARGUMENT, tk1.c_str());
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

bool IsTempVar(int iVar)
{
	if (COMPILE_LOCALTMP_VAR_BEGIN <= iVar && iVar < COMPILE_STATIC_VAR_BEGIN)
		return true;

	return false;
}
// 토큰 텍스트 자체가 float 형태인가.
// 소스의 "1.5" 는 토크나이저가 '.' 에서 쪼개므로 여기 안 걸리고, define/const 치환으로
// 통째로 들어온 "3.0", "0.5", "1e+20" 같은 토큰을 잡는다. (없으면 (int) 캐스팅으로 강등됨)
static bool IsFloatNumberToken(const std::string& tk)
{
	if (tk.find('.') != std::string::npos)
		return true;
	if (tk.size() > 1 && tk[0] == '0' && (tk[1] == 'x' || tk[1] == 'X'))
		return tk.find('p') != std::string::npos || tk.find('P') != std::string::npos; // hex 지수
	return tk.find('e') != std::string::npos || tk.find('E') != std::string::npos;
}

// This Function No Error Because Try Only
// return TK_NONE : error
TK_TYPE Try_ParseIntNum(int& iResultInt, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, TK_TYPE tkTypeEnd1 = TK_UNUSED, TK_TYPE tkTypeEnd2 = TK_UNUSED, TK_TYPE tkTypeEnd3 = TK_UNUSED)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2, tkTypePre = TK_PLUS;

	double num;

	tkType1 = GetToken(ar, tk1);

	if (true == StringToDouble(num, tk1.c_str()))
	{
		u16 c = ar.GetData(false);
		if (c == '.' || IsFloatNumberToken(tk1))
		{
			ar.PushToken(tkType1, tk1);
			return TK_NONE; // float or double is not int
		}
		else
		{
			if (tkTypePre == TK_MINUS)
				num = -num;
			int inum = (int)num;
#if 0
			if (IsShort(inum))
			{
				iResultInt = inum;
				tkType2 = GetToken(ar, tk2);
				if (tkType2 == tkTypeEnd1 || tkType2 == tkTypeEnd2 || tkType2 == tkTypeEnd3)
					return tkType2;
				ar.PushToken(tkType2, tk2);
				ar.PushToken(tkType1, tk1);
				return tkType2;
			}
#endif
			iResultInt = inum;
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == tkTypeEnd1 || tkType2 == tkTypeEnd2 || tkType2 == tkTypeEnd3)
				return tkType2;
			ar.PushToken(tkType2, tk2);
			ar.PushToken(tkType1, tk1);
			return TK_NONE;
		}
	}
	ar.PushToken(tkType1, tk1);
	return TK_NONE;
}

bool ParseFunCall(SOperand& iResultStack, TK_TYPE tkTypePre, SFunctionInfo* pFun, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2, tkType3;


	SOperand iTempVar;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_L_SMALL)
	{
		int iCallLine = ar.CurLine();
		int iParamCount = 0;
		// 각 인자를 평가하되, 호출-인자 슬롯(COMPILE_CALLARG_VAR_BEGIN)으로의 복사는
		// 모든 인자 평가가 끝난 뒤로 미룬다. 인자식이 그 자체로 함수 호출이면 내부 호출이
		// 같은 호출-인자 윈도우를 재사용하므로, 즉시 복사할 경우 먼저 평가된 인자가 덮어써진다.
		std::vector<SOperand> argOperands;
		std::vector<int> argDefOffsets;  // 인자 결과를 만든 단일 op 의 오프셋 (융합 불가면 -1)
		std::vector<int> argCallCounts;  // 이 인자 평가 완료 시점의 call emit 카운트
		while (true)
		{
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_R_SMALL)
				break;

			ar.PushToken(tkType2, tk2);

			//int iTryValue = -1;
			//tkType3 = Try_ParseIntNum(iTryValue, ar, funs, vars, TK_COMMA, TK_R_SMALL);
			//if (tkType3 != TK_NONE)
			//{
			//	funs._cur->Push_MOV(ar, NOP_MOV, COMPILE_CALLARG_VAR_BEGIN + 1 + iParamCount, iTryValue, true);
			//}
			//else
			{
				int iCodeOffBefore = funs._cur->_code->GetBufferOffset();
				iTempVar.Reset();
				tkType3 = ParseJob(true, iTempVar, NULL, ar, funs, vars);
				if (iTempVar.IsInvalidValue())
				{
					SetParserCompileError(ar, PCE_INVALID_CALL_ARGUMENT);
					return false;
				}
				argOperands.push_back(iTempVar);

				// 인자 식 전체가 "임시변수에 쓰는 단일 op" 로 끝났으면 나중에
				// dest 를 호출-인자 슬롯으로 직접 패치할 수 있게 오프셋을 기억한다.
				int iDefOffset = -1;
				if (funs._cur->_iLastOPOffset == iCodeOffBefore &&
					false == iTempVar.IsFun() &&
					iTempVar._iArrayIndex == INVALID_ERROR_PARSEJOB &&
					IsTempVar(iTempVar._iVar) &&
					funs._cur->GetN(iCodeOffBefore, 0) == iTempVar._iVar &&
					SFunctionInfo::IsRetargetableProducerOP(funs._cur->GetOP(iCodeOffBefore)))
				{
					iDefOffset = iCodeOffBefore;
				}
				argDefOffsets.push_back(iDefOffset);
				argCallCounts.push_back(funs._cur->_callOpEmitCount);
			}
			iParamCount++;


			if (tkType3 != TK_R_SMALL && tkType3 != TK_COMMA)
			{
				SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "',' or ')' after function argument", tk2.c_str());
				return false;
			}
			if (tkType3 == TK_R_SMALL)
				break;
		}

		// 모든 인자 평가가 끝난 뒤, CALL 직전에 호출-인자 슬롯으로 복사한다.
		int iFinalCallCount = funs._cur->_callOpEmitCount;
		for (int i = 0; i < iParamCount; i++)
		{
			SOperand& arg = argOperands[i];
			// 융합: 인자 producer 의 dest 를 호출-인자 슬롯으로 직접 패치하고 MOV 생략.
			// 조건: 이후 인자들의 평가에 함수 호출이 없어야 함 — 호출은 같은 인자
			// 윈도우를 재사용하므로 일찍 쓴 슬롯이 덮어써진다 (지연 복사의 이유).
			if (argDefOffsets[i] != -1 && argCallCounts[i] == iFinalCallCount)
			{
				funs._cur->SetN(argDefOffsets[i], 0, (s16)(COMPILE_CALLARG_VAR_BEGIN + 1 + i));
				continue;
			}
			if (arg.IsFun())
				funs._cur->Push_OP2(ar, NOP_FMOV1, COMPILE_CALLARG_VAR_BEGIN + 1 + i, arg._iVar, false, iCallLine);
			else if (arg._iArrayIndex == INVALID_ERROR_PARSEJOB)
				funs._cur->Push_OP2(ar, NOP_MOV, COMPILE_CALLARG_VAR_BEGIN + 1 + i, arg._iVar, arg.IsShort(), iCallLine);
			else
				funs._cur->Push_TableRead(ar, arg._iVar, arg._iArrayIndex, COMPILE_CALLARG_VAR_BEGIN + 1 + i, arg.IsHaveShort());
		}
		if (pFun != NULL)
		{
			if(pFun->_funType == FUNT_BUILT_IN)
			{
				if(pFun->_built_in_arg_c != -1)
				{
					if (pFun->_built_in_arg_c != iParamCount)
					{
						SetParserCompileError(ar, PCE_ARGUMENT_COUNT_MISMATCH, pFun->_built_in_arg_c, iParamCount);
						return false;
					}
				}
				//SetCompileError(ar, "Error (%d, %d):  Built-In", ar.CurLine(), ar.CurCol());
				int iArrayIndex = funs.AddStaticString(pFun->_name);
				iResultStack = funs._cur->AllocLocalTempVar();
				funs._cur->Push_CallPtr2(ar, iArrayIndex, iParamCount, iResultStack._iVar, iCallLine);
				//funs._cur->Push_OP2(ar, tkTypePre != TK_MINUS ? NOP_MOV : NOP_MOV_MINUS, iResultStack._iVar, STACK_POS_RETURN, false);
				if (tkTypePre == TK_MINUS)
					funs._cur->Push_OP2(ar, NOP_MOV_MINUS, iResultStack._iVar, iResultStack._iVar, false);
				return true;
			}
			else
			{
				if ((int)pFun->_args.size() != iParamCount)
				{
					SetParserCompileError(ar, PCE_ARGUMENT_COUNT_MISMATCH, (int)pFun->_args.size(), iParamCount);
					return false;
				}

				iResultStack = funs._cur->AllocLocalTempVar();
				funs._cur->Push_Call(ar, NOP_CALL, pFun->_funID, iParamCount, iResultStack._iVar, iCallLine);
			}
			if (tkTypePre == TK_MINUS)
				funs._cur->Push_OP2(ar, NOP_MOV_MINUS, iResultStack._iVar, iResultStack._iVar, false);
			return true;
		}
		else if(iResultStack._iVar != -1)
		{
			if(iResultStack._iArrayIndex != -1)
			{
				funs._cur->Push_CallPtr(ar, iResultStack._iVar, iResultStack._iArrayIndex, iParamCount, false, iCallLine);
				iResultStack = funs._cur->AllocLocalTempVar();
				funs._cur->Push_OP2(ar, tkTypePre != TK_MINUS ? NOP_MOV : NOP_MOV_MINUS, iResultStack._iVar, STACK_POS_RETURN, false);
			}
			else
			{	// TODO
				funs._cur->Push_CallPtr(ar, iResultStack._iVar, iResultStack._iArrayIndex, iParamCount, false, iCallLine);
				iResultStack = funs._cur->AllocLocalTempVar();
				funs._cur->Push_OP2(ar, tkTypePre != TK_MINUS ? NOP_MOV : NOP_MOV_MINUS, iResultStack._iVar, STACK_POS_RETURN, false);
			}
		}
		else
		{
			int iArrayIndex = iResultStack._iArrayIndex;
			iResultStack = funs._cur->AllocLocalTempVar();
			funs._cur->Push_CallPtr2(ar, iArrayIndex, iParamCount, iResultStack._iVar, iCallLine);
//			funs._cur->Push_OP2(ar, tkTypePre != TK_MINUS ? NOP_MOV : NOP_MOV_MINUS, iResultStack._iVar, STACK_POS_RETURN, false);
			if (tkTypePre == TK_MINUS)
				funs._cur->Push_OP2(ar, NOP_MOV_MINUS, iResultStack._iVar, iResultStack._iVar, false);
		}
	}
	else if (tkType1 == TK_SEMICOLON)
	{
		ar.PushToken(tkType1, tk1);
		iResultStack._iVar = pFun->_funID;
		iResultStack._operandType = Data_Fun;
	}
	else
	{
		SetParserCompileError(ar, PCE_EXPECTED_LEFT_PAREN);
		return false;
	}
	return true;
}



bool ParseNum(SOperand& iResultStack, TK_TYPE tkTypePre, std::string& tk1, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk2;
	TK_TYPE tkType2;

	double num;

	iResultStack = vars.FindVar(tk1);
	if (iResultStack._iVar != -1)
		return true;

	if (true == StringToDouble(num, tk1.c_str()))
	{
		u16 c = ar.GetData(false);
		if (IsFloatNumberToken(tk1)) // define/const 치환으로 통째로 들어온 float
		{
			if (tkTypePre == TK_MINUS)
				num = -num;
			iResultStack = funs.AddStaticNum(num);
		}
		else if (c == '.')
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
				SetParserCompileError(ar, PCE_INVALID_NUMBER_LITERAL);
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
			int inum = (int)num;
#if 0
			if (IsShort(inum))
			{
				iResultStack = inum;
				iResultStack._operandType = Data_NS;
			}
			else
#endif
				iResultStack = funs.AddStaticInt(inum);
		}
	}
	else
	{
		SetParserCompileError(ar, PCE_UNKNOWN_IDENTIFIER, tk1.c_str());
		return false;
	}
	return true;
}

bool ParseNum2(int& iResultStack, TK_TYPE tkTypePre, std::string& tk1, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk2;
	TK_TYPE tkType2;

	double num;

	iResultStack = vars.FindVar(tk1);
	if (iResultStack != -1)
		return true;

	if (true == StringToDouble(num, tk1.c_str()))
	{
		u16 c = ar.GetData(false);
		if (IsFloatNumberToken(tk1)) // define/const 치환으로 통째로 들어온 float
		{
			if (tkTypePre == TK_MINUS)
				num = -num;
			iResultStack = funs.AddStaticNum(num);
		}
		else if (c == '.')
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
				SetParserCompileError(ar, PCE_INVALID_NUMBER_LITERAL);
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
			int inum = (int)num;
			iResultStack = funs.AddStaticInt(inum);
		}
	}
	else
	{
		SetParserCompileError(ar, PCE_UNKNOWN_IDENTIFIER, tk1.c_str());
		return false;
	}
	return true;
}

bool ParseStringOrNum(SOperand& iResultStack, TK_TYPE tkTypePre, std::string& tkPre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (tkTypePre == TK_QUOTE2 || tkTypePre == TK_QUOTE1)
	{
		std::string str;
		if (false == GetQuotationString(ar, str, tkTypePre == TK_QUOTE2 ? '"' : '\''))
		{
			SetParserCompileError(ar, PCE_UNTERMINATED_STRING);
			return false;
		}
		iResultStack = funs.AddStaticString(str);
		return true;
	}
	else if (tkTypePre == TK_STRING_LITERAL) // define/const 치환으로 완성된 문자열
	{
		iResultStack = funs.AddStaticString(tkPre);
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

TK_TYPE ParseListDef(SOperand& iResultStack, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, int iTableDeep = 0)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r = TK_NONE;

	// // List alloc
	iResultStack = funs._cur->AllocLocalTempVar();
	funs._cur->Push_ListAlloc(ar, iResultStack._iVar);

	int off_cnt = funs._cur->_code->GetBufferOffset() - sizeof(int);

/*	int off_resize = funs._cur->_code->GetBufferOffset();

	// resize (1.param set cnt, 2.call resize)
	funs._cur->Push_MOV(ar, NOP_MOV, COMPILE_CALLARG_VAR_BEGIN + 1 + 0, 0, true);
	int off_cnt = funs._cur->_code->GetBufferOffset() - (sizeof(short) * 2);

	int iStaticString = -1;// funs.AddStaticString("resize");
	funs._cur->Push_CallPtr(ar, iResultStack._iVar, iStaticString, 1); // list resize
	int off_resize_str = funs._cur->_code->GetBufferOffset() - (sizeof(short) * 2);
	*/
	SOperand iTempOffsetValue;
	std::string str;

	int iItemCount = 0;

	bool blEnd = false;
	while (blEnd == false)
	{
		iTempOffsetValue = INVALID_ERROR_PARSEJOB;

		tkType1 = ParseJob(false, iTempOffsetValue, NULL, ar, funs, vars, false, TK_COMMA, TK_R_ARRAY, TK_NONE, TK_NONE);

		if (iTempOffsetValue.IsNone() == true)
		{
			break;
		}
#if 0
		if (IsShort(iItemCount))
			funs._cur->Push_Table_MASMDP(ar, NOP_CLT_MOV, iResultStack._iVar, iItemCount, iTempOffsetValue._iVar, false, true, iTempOffsetValue.IsShort());
		else
#endif
		{
			if (iTempOffsetValue.IsArray())
			{
				int iTempOffsetRead = funs._cur->AllocLocalTempVar();
				funs._cur->Push_TableRead(ar, iTempOffsetValue._iVar, iTempOffsetValue._iArrayIndex, iTempOffsetRead, iTempOffsetValue.IsHaveShort());
				iTempOffsetValue = SOperand(iTempOffsetRead);
			}

			int iTempOffsetKey = funs.AddStaticInt(iItemCount);
			funs._cur->Push_Table_MASMDP(ar, NOP_CLT_MOV, iResultStack._iVar, iTempOffsetKey, iTempOffsetValue._iVar, false, false, iTempOffsetValue.IsShort());
		}
		iItemCount++;

		if (iTempOffsetValue.IsNone() == false)
		{
		}

		if (tkType1 == TK_R_ARRAY)
			break;
	}
	// 리스트 뒤의 토큰(`;`, `)`, `,` 등)은 소비해서 반환만 하고, 유효성은 호출자가 검증한다.
	// 문장이면 상위에서 `;`를, 함수 인자면 ParseFunCall이 `)`/`,`를 확인한다. 여기서 `;`를
	// 강제하면 리스트 리터럴을 함수 인자로 쓸 때(예: x.append([1,2,3])) 잘못된 에러가 난다.
	tkType1 = GetToken(ar, tk1);
	if (iItemCount > 0)
	{
	//	iStaticString = funs.AddStaticString("resize");
		*((int*)((u8*)funs._cur->_code->GetData() + off_cnt)) = iItemCount;
	//	*((short*)((u8*)funs._cur->_code->GetData() + off_resize_str)) = iStaticString;
	}
	else
	{
//		funs._cur->_code->SetBufferOffset(off_resize);
//		funs._cur->ClearLastOP();
	}
	return tkType1;
}

TK_TYPE ParseTableDef(SOperand& iResultStack, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, int iTableDeep = 0)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
	TK_TYPE r = TK_NONE;

	// Table alloc
	iResultStack = funs._cur->AllocLocalTempVar();
	funs._cur->Push_TableAlloc(ar, iResultStack._iVar);

	int off_cnt = funs._cur->_code->GetBufferOffset() - sizeof(int);

	/*	int off_reserve = funs._cur->_code->GetBufferOffset();

	// reserve (1.param set cnt, 2.call reserve)
	funs._cur->Push_MOV(ar, NOP_MOV, COMPILE_CALLARG_VAR_BEGIN + 1 + 0, 0, true);
	int off_cnt = funs._cur->_code->GetBufferOffset() - (sizeof(short) * 2);

	int iStaticString = -1;// funs.AddStaticString("reserve");
	funs._cur->Push_CallPtr(ar, iResultStack._iVar, iStaticString, 1); // map reserve
	int off_reserve_str = funs._cur->_code->GetBufferOffset() - (sizeof(short) * 2);
	*/
	SOperand iTempOffsetKey;
	SOperand iTempOffsetValue;
	std::string str;

	int iItemCount = 0;
	int iCurArrayOffset = -1;

	bool blEnd = false;
	while (blEnd == false)
	{
		iTempOffsetKey = iTempOffsetValue = INVALID_ERROR_PARSEJOB;

		tkType1 = ParseJob(false, iTempOffsetKey, NULL, ar, funs, vars, false, TK_COLON, TK_COMMA, TK_R_MIDDLE, TK_NONE);

		if (iTempOffsetKey.IsNone() == true)
		{
			break;
		}

		if (tkType1 == TK_COLON)
		{
			tkType2 = ParseJob(false, iTempOffsetValue, NULL, ar, funs, vars, false, TK_COMMA, TK_R_MIDDLE, TK_NONE, TK_NONE);
			if (tkType2 == TK_NONE)
			{
				return TK_NONE;
			}
			funs._cur->Push_Table_MASMDP(ar, NOP_CLT_MOV, iResultStack._iVar, iTempOffsetKey._iVar, iTempOffsetValue._iVar, false, iTempOffsetKey.IsShort(), iTempOffsetValue.IsShort());
			tkType1 = tkType2;
			iItemCount++;
		}
		else
		{
			iTempOffsetValue = iTempOffsetKey;
			++iCurArrayOffset;
#if 0
			if(IsShort(iCurArrayOffset))
				funs._cur->Push_Table_MASMDP(ar, NOP_CLT_MOV, iResultStack._iVar, iCurArrayOffset, iTempOffsetValue._iVar, false, true, iTempOffsetValue.IsShort());
			else
#endif
			{
				iTempOffsetKey = funs.AddStaticInt(iCurArrayOffset);
				funs._cur->Push_Table_MASMDP(ar, NOP_CLT_MOV, iResultStack._iVar, iTempOffsetKey._iVar, iTempOffsetValue._iVar, false, false, iTempOffsetValue.IsShort());
			}
			iItemCount++;
		}

		if (iTempOffsetKey.IsNone() == false)
		{
		}

		if (tkType1 == TK_R_MIDDLE)
			break;
	}
	// 맵/테이블 뒤의 토큰은 소비해서 반환만 하고, 유효성은 호출자가 검증한다(ParseListDef와 동일).
	// 여기서 `;`를 강제하면 맵 리터럴을 함수 인자로 쓸 때 잘못된 에러가 난다.
	tkType1 = GetToken(ar, tk1);
	if (iItemCount > 0)
	{
		//iStaticString = funs.AddStaticString("reserve");
		*((int*)((u8*)funs._cur->_code->GetData() + off_cnt)) = iItemCount;
		//*((short*)((u8*)funs._cur->_code->GetData() + off_reserve_str)) = iStaticString;
	}
	else
	{
		//funs._cur->_code->SetBufferOffset(off_reserve);
		//funs._cur->ClearLastOP();
	}
	return tkType1;
}

TK_TYPE ParseTable(SOperand& operand, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	TK_TYPE r = TK_NONE;

	SOperand op;
	r = ParseJob(true, op, NULL, ar, funs, vars);
	if (r != TK_R_ARRAY)
	{
		SetParserCompileError(ar, PCE_EXPECTED_RIGHT_BRACKET);
		return TK_NONE;
	}

	/*if (op.IsArray())
	{
		operand = funs._cur->AllocLocalTempVar();
		funs._cur->Push_TableRead(ar, op._iVar, op._iArrayIndex, operand._iVar);
	}
	else*/
		operand = op;
	return r;
}

bool ParseString(SOperand& operand, TK_TYPE tkTypePre, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
	TK_TYPE r = TK_NONE;


	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING)
	{
		SetParserCompileError(ar, PCE_UNKNOWN_IDENTIFIER, tk1.c_str());
		return false;
	}

	SFunctionLayer* pOtherModule = nullptr;
	auto it = funs._curModule->_defModules.find(tk1);
	if (it != funs._curModule->_defModules.end())
	{
		tkType2 = GetToken(ar, tk2);
		if(tkType2 != TK_DOT)
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'.' after module name", tk2.c_str());
			return false;
		}
		tkType2 = GetToken(ar, tk2);
		//tk1 = tk1 + "." + tk2;
		tk1 = tk2;
		pOtherModule = (*it).second;
	}

	SOperand iTempOffset;
	SOperand iTempOffset2;
	//SOperand iArrayIndex = INVALID_ERROR_PARSEJOB;
	if(pOtherModule == nullptr)
		iTempOffset._iVar = vars.FindVar(tk1);

	if (iTempOffset._iVar != -1)
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

				//iTempOffset._iArrayIndex = iArrayIndex._iVar;
				//iArrayIndex = INVALID_ERROR_PARSEJOB;

				if (false == ParseFunCall(iTempOffset, tkTypePre, NULL, ar, funs, vars))
					return false;
				break;
			}
			else
			{
				ar.PushToken(tkType2, tk2);
				break;
			}

			iTempOffset2 = INVALID_ERROR_PARSEJOB;
			if (tkType2 == TK_L_ARRAY)
			{
				r = ParseTable(iTempOffset2, ar, funs, vars);
			}
			else
			{
				std::string str;
				if (false == GetDotString(ar, str))
				{
					SetParserCompileError(ar, PCE_EXPECTED_MEMBER_NAME);
					return false;
				}
				iTempOffset2 = funs.AddStaticString(str);
			}

			if (iTempOffset2.IsArray())
			{
				SetParserCompileError(ar, PCE_EXPECTED_MEMBER_NAME);
				return false;
				//int iTempOffset2 = funs._cur->AllocLocalTempVar();
				//funs._cur->Push_TableRead(ar, iTempOffset._iVar, iTempOffset._iArrayIndex, iTempOffset2);
				//iTempOffset = iTempOffset2;
			}
			else
			{
				if (iTempOffset._iArrayIndex == INVALID_ERROR_PARSEJOB)
				{
					iTempOffset._iArrayIndex = iTempOffset2._iVar;
					if (iTempOffset2._operandType == Data_NS)
					{
						iTempOffset._iArrayIndex = iTempOffset2._iVar;
						iTempOffset._operandType = Data_TS;
						//iTempOffset._iArrayIndex = funs.AddStaticInt(iTempOffset2._iVar);
						//iTempOffset._operandType = Data_TR;
					}
				}
				else
				{
					if (iTempOffset2._iArrayIndex == INVALID_ERROR_PARSEJOB)
					{
						int iTempOffset3 = funs._cur->AllocLocalTempVar();
						funs._cur->Push_TableRead(ar, iTempOffset._iVar, iTempOffset._iArrayIndex, iTempOffset3, iTempOffset.IsHaveShort());
						iTempOffset._iVar = iTempOffset3;
						iTempOffset._iArrayIndex = iTempOffset2._iVar;
						iTempOffset._operandType = iTempOffset2.IsShort() ? Data_TS : Data_TR;
					}
					else
					{
						SetParserCompileError(ar, PCE_EXPECTED_MEMBER_NAME);
						return false;
					}
				}
			}

		}
		if (tkTypePre == TK_MINUS)
		{
			if (iTempOffset.IsArray())
			{
				int iTempOffset2 = funs._cur->AllocLocalTempVar();
				funs._cur->Push_TableRead(ar, iTempOffset._iVar, iTempOffset._iArrayIndex, iTempOffset2, iTempOffset.IsHaveShort());
				iTempOffset = iTempOffset2;

				//iTempOffset._iArrayIndex = INVALID_ERROR_PARSEJOB;
			}

			int iTempOffset3 = funs._cur->AllocLocalTempVar();
			funs._cur->Push_OP2(ar, NOP_MOV_MINUS, iTempOffset3, iTempOffset._iVar, false);
			iTempOffset = iTempOffset3;
		}
	}
	else
	{
		SFunctionInfo* pFun = nullptr;
		if(pOtherModule == nullptr)
			pFun = funs.FindFun(tk1);
		else
			pFun = funs.FindFun(tk1, pOtherModule);
		if (pFun != NULL)
		{
			if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
			{
				SetParserCompileError(ar, PCE_GLOBAL_CALL_NOT_ALLOWED);
				return false;
			}

			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_L_SMALL)
			{
				ar.PushToken(tkType2, tk2);
				if (false == ParseFunCall(iTempOffset, tkTypePre, pFun, ar, funs, vars))
					return false;
			}
			else
			{
				ar.PushToken(tkType2, tk2);
				//iTempOffset._iVar = pFun->_staticIndex;
				iTempOffset._iVar = pFun->_funID;
				iTempOffset._operandType = Data_Fun;
			}
		}
		else if (pOtherModule == nullptr && CNeoVMImpl::IsGlobalLibFun(tk1))
		{
			tkType2 = GetToken(ar, tk2);
			if (tkType2 == TK_L_SMALL)
			{
				iTempOffset = SOperand(-1, funs.AddStaticString(tk1));
				ar.PushToken(tkType2, tk2);
				if (false == ParseFunCall(iTempOffset, tkTypePre, pFun, ar, funs, vars))
					return false;
			}
			else
			{
				SetParserCompileError(ar, PCE_INVALID_FUNCTION_CALL);
				return false;
			}
		}
		else
		{
			if (false == ParseNum(iTempOffset, tkTypePre, tk1, ar, funs, vars))
				return false;
		}
	}
	operand = iTempOffset;// SOperand(iTempOffset._iVar, iArrayIndex, iTempOffset._operandType);
	return true;
}

bool ParseLogicNotOperand(SOperand& operand, CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tkNext;
	TK_TYPE tkNextType = GetToken(ar, tkNext);
	if (tkNextType == TK_LOGIC_NOT)
	{
		if (false == ParseLogicNotOperand(operand, ar, funs, vars))
			return false;
	}
	else if (tkNextType == TK_L_SMALL)
	{
		TK_TYPE r = ParseJob(true, operand, NULL, ar, funs, vars);
		if (TK_R_SMALL != r)
		{
			SetParserCompileError(ar, PCE_EXPECTED_RIGHT_PAREN);
			return false;
		}
	}
	else
	{
		ar.PushToken(tkNextType, tkNext);
		if (false == ParseString(operand, TK_NONE, ar, funs, vars))
			return false;
	}

	if (operand.IsInvalidValue())
	{
		SetParserCompileError(ar, PCE_EXPECTED_LOGIC_NOT_OPERAND);
		return false;
	}
	if (operand.IsArray())
	{
		int iRead = funs._cur->AllocLocalTempVar();
		funs._cur->Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iRead, operand.IsHaveShort());
		operand = SOperand(iRead);
	}

	int iNot = funs._cur->AllocLocalTempVar();
	funs._cur->Push_OP2(ar, NOP_LOG_NOT, iNot, operand._iVar, operand.IsShort());
	operand = SOperand(iNot);
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
		SetParserCompileError(ar, PCE_EXPECTED_TYPE_LEFT_PAREN);
		return false;
	}

	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
	if (r != TK_R_SMALL)
	{
		SetParserCompileError(ar, PCE_EXPECTED_TYPE_RIGHT_PAREN);
		return TK_NONE;
	}

	int iTempOffset2;
	if (operand._iArrayIndex != INVALID_ERROR_PARSEJOB)
	{
		iTempOffset2 = funs._cur->AllocLocalTempVar();
		funs._cur->Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iTempOffset2, operand.IsHaveShort());
	}
	else
		iTempOffset2 = operand._iVar;

	int iTempOffset3 = funs._cur->AllocLocalTempVar();
	int iPriority = 0;
	funs._cur->Push_ToType(ar, TokenToOP(tkTypePre, iPriority), iTempOffset3, iTempOffset2);
	iResultStack = iTempOffset3;
	return true;
}

//case TK_SEMICOLON:	// ;
//case TK_COMMA:		// ,
//case TK_R_SMALL:	// )
//case TK_R_ARRAY:	// ]
TK_TYPE ParseJob(bool bReqReturn, SOperand& sResultStack, std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool bAllowVarDef,
	TK_TYPE tkEnd1, TK_TYPE tkEnd2, TK_TYPE tkEnd3, TK_TYPE tkEnd4, std::vector<SJumpValue>* pContinueJumps)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
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
		if (tkType1 == tkEnd1 || tkType1 == tkEnd2 || tkType1 == tkEnd3 || tkType1 == tkEnd4)
		{
			r = tkType1;
			blEnd = true;
			break;
		}
		switch (tkType1)
		{
		case TK_NONE:
			SetParserCompileError(ar, PCE_UNEXPECTED_FUNCTION_END, funs.GetCurFunName().c_str());
			return TK_NONE;
		case TK_RETURN:
			if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME)
			{
				SetParserCompileError(ar, PCE_UNEXPECTED_GLOBAL_BLOCK_END);
				return TK_NONE;
			}
			iTempOffset.Reset();
			r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
			if (r != TK_SEMICOLON)
			{
				SetParserCompileError(ar, PCE_INVALID_RETURN_STATEMENT);
				return TK_NONE;
			}
			if(iTempOffset.IsInvalidValue() == false)
			{
				if (iTempOffset.IsArray())
				{
					int iTempOffset2 = funs._cur->AllocLocalTempVar();
					funs._cur->Push_TableRead(ar, iTempOffset._iVar, iTempOffset._iArrayIndex, iTempOffset2, iTempOffset.IsHaveShort());
					funs._cur->Push_RETURN(ar, iTempOffset2, false);
				}
				else
				{
					funs._cur->Push_RETURN(ar, iTempOffset._iVar, iTempOffset.IsShort());
				}
			}
			else 
				funs._cur->Push_RETURN(ar, 0, true);
			blEnd = true;
			break;
		case TK_L_SMALL:
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(bReqReturn, iTempOffset, NULL, ar, funs, vars);
			if (TK_R_SMALL != r)
			{
				SetParserCompileError(ar, PCE_EXPECTED_RIGHT_PAREN);
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
				SetParserCompileError(ar, PCE_UNTERMINATED_STRING);
				return TK_NONE;
			}
			iTempOffset = funs.AddStaticString(str);
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		}
		case TK_STRING_LITERAL: // define/const 치환으로 완성된 문자열
			iTempOffset = funs.AddStaticString(tk1);
			operands.push_back(SOperand(iTempOffset));
			blApperOperator = true;
			break;
		case TK_PLUS:		// +
		case TK_MINUS:		// -
			if (blApperOperator == false)
			{
				// 단항 +/-. 다음 토큰이 '(' 이면 괄호식을 평가한 뒤 부호를 적용한다.
				// ParseString은 식별자/숫자만 처리하므로 -(expr) 형태는 여기서 직접 다룬다.
				std::string tkNext;
				TK_TYPE tkNextType = GetToken(ar, tkNext);
				if (tkNextType == TK_L_SMALL)
				{
					SOperand a;
					r = ParseJob(bReqReturn, a, NULL, ar, funs, vars);
					if (TK_R_SMALL != r)
					{
						SetParserCompileError(ar, PCE_EXPECTED_RIGHT_PAREN);
						return TK_NONE;
					}
					if (tkType1 == TK_MINUS)
					{
						if (a.IsArray())
						{
							int iRead = funs._cur->AllocLocalTempVar();
							funs._cur->Push_TableRead(ar, a._iVar, a._iArrayIndex, iRead, a.IsHaveShort());
							a = SOperand(iRead);
						}
						int iNeg = funs._cur->AllocLocalTempVar();
						funs._cur->Push_OP2(ar, NOP_MOV_MINUS, iNeg, a._iVar, false);
						a = SOperand(iNeg);
					}
					operands.push_back(a);
					blApperOperator = true;
					break;
				}
				ar.PushToken(tkNextType, tkNext);

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
		case TK_LOGIC_NOT:	// !
			if (blApperOperator == false)
			{
				SOperand a;
				if (false == ParseLogicNotOperand(a, ar, funs, vars))
					return TK_NONE;
				operands.push_back(a);
				blApperOperator = true;
				break;
			}
			SetParserCompileError(ar, PCE_INVALID_OPERATOR, tk1.c_str());
			return TK_NONE;
		case TK_MUL:		// *
		case TK_DIV:		// /
		case TK_PERCENT:	// %
		case TK_LSHIFT:		// <<
		case TK_RSHIFT:		// >>
		case TK_GREAT:		// >
		case TK_GREAT_EQ:	// >=
		case TK_LESS:		// <
		case TK_LESS_EQ:	// <=
		case TK_EQUAL_EQ:	// ==
		case TK_EQUAL_NOT:	// !=
		case TK_AND:		// &
		case TK_OR:			// |
		case TK_AND2:		// &&
		case TK_OR2:		// ||
		case TK_DOT2:		// ..
		{
			if (blApperOperator == false)
			{
				SetParserCompileError(ar, PCE_INVALID_OPERATOR, tk1.c_str());
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
		case TK_LSHIFT_EQ:	// <<=
		case TK_RSHIFT_EQ:	// >>=
		case TK_AND_EQ:		// &=
		case TK_OR_EQ:		// |=
		case TK_XOR_EQ:		// ^=
		{
			SOperand iTempVar;
			r = ParseJob(true, iTempVar, NULL, ar, funs, vars);
			if (TK_NONE == r)
				return TK_NONE;
			if (iTempVar._iVar == INVALID_ERROR_PARSEJOB)
			{
				SetParserCompileError(ar, PCE_INVALID_ASSIGNMENT);
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
				if (false == ParseVarDef(ar, funs, vars, false))
					return TK_NONE;
				return TK_SEMICOLON; // ㅡㅡ;
			}
			else
			{
				SetParserCompileError(ar, PCE_VAR_DECLARATION_NOT_ALLOWED);
				return TK_NONE;
			}
			break;
		case TK_BREAK:
			if(pJumps == NULL)
			{
				SetParserCompileError(ar, PCE_BREAK_OUTSIDE_LOOP);
				return TK_NONE;
			}
			funs._cur->Push_JMP(ar, 0);
			pJumps->push_back(SJumpValue(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset()));
			break;
		case TK_CONTINUE:
			if (pContinueJumps == NULL)
			{
				SetParserCompileError(ar, PCE_CONTINUE_OUTSIDE_LOOP);
				return TK_NONE;
			}
			funs._cur->Push_JMP(ar, 0);
			pContinueJumps->push_back(SJumpValue(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset()));
			break;
		case TK_L_MIDDLE:
			iTempOffset.Reset();
			ar._iTableDeep++;
			r = ParseTableDef(iTempOffset, ar, funs, vars);
			ar._iTableDeep--;
			operands.push_back(iTempOffset);
			blApperOperator = true;
			blEnd = true;
			break;
		case TK_L_ARRAY:
			iTempOffset.Reset();
			ar._iTableDeep++;
			r = ParseListDef(iTempOffset, ar, funs, vars);
			ar._iTableDeep--;
			operands.push_back(iTempOffset);
			blApperOperator = true;
			blEnd = true;
			break;

		case TK_NULL:
			//iTempOffset._iVar = COMPILE_VAR_NULL;
			iTempOffset._iVar = funs._cur->AllocLocalTempVar();
			funs._cur->Push_OP1(ar, NOP_VAR_CLEAR, iTempOffset._iVar);
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
				if (a._iVar >= COMPILE_STATIC_VAR_BEGIN && a._iVar < COMPILE_CALLARG_VAR_BEGIN)
				{	// 상수 풀(리터럴/const) 증감 불가
					SetParserCompileError(ar, PCE_INVALID_INCREMENT_TARGET, tk1.c_str());
					return TK_NONE;
				}
				a._operandType = Increment_Prefix;
				funs._cur->Push_OP1(ar, tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				operands.push_back(a);
				blApperOperator = true;
			}
			else
			{	// 후위
				SOperand& a = operands[operands.size() - 1];
				if (IsTempVar(a._iVar))
				{
					SetParserCompileError(ar, PCE_TEMP_VAR_UNSUPPORTED, tk1.c_str());
					return TK_NONE;
				}
				if (a._iArrayIndex != INVALID_ERROR_PARSEJOB)
				{
					SetParserCompileError(ar, PCE_TABLE_VAR_UNSUPPORTED, tk1.c_str());
					return TK_NONE;
				}
				if (a.IsConst() ||
					(a._iVar >= COMPILE_STATIC_VAR_BEGIN && a._iVar < COMPILE_CALLARG_VAR_BEGIN))
				{	// 상수 풀(리터럴/const) 증감 불가
					SetParserCompileError(ar, PCE_INVALID_INCREMENT_TARGET, tk1.c_str());
					return TK_NONE;
				}
				int iTempOffset2;
				if (bReqReturn)
				{
					iTempOffset2 = funs._cur->AllocLocalTempVar();
					funs._cur->Push_OP2(ar, NOP_MOV, iTempOffset2, a._iVar, false);
				}
				funs._cur->Push_OP1(ar, tkType1 == TK_PLUS2 ? NOP_INC : NOP_DEC, a._iVar);

				if (bReqReturn)
					a = SOperand(iTempOffset2);
			}
			break;
		case TK_FUN: // FUNT_ANONYMOUS ?
		{
			int r = ParseFunctionBase(ar, funs, vars, "", FUNT_ANONYMOUS);
			if(r == -1)
				return TK_NONE; // error
			SFunctionInfo* pFun = funs.FindFun(r);
			if (pFun == NULL)
				return TK_NONE; // error
			operands.push_back(SOperand(pFun->_funID, INVALID_ERROR_PARSEJOB, Data_Fun));
			blApperOperator = true;
			break;
		}
		case TK_YIELD: // 
		{
			if(operands.empty() == false || operators.empty() == false)
			{
				SetParserCompileError(ar, PCE_INVALID_OPERATOR, tk1.c_str());
				return TK_NONE;
			}
			blEnd = true;

			//tkType2 = GetToken(ar, tk2);
			//if (tkType2 != TK_RETURN)
			//{
			//	SetCompileError(ar, "Error (%d, %d): Invalid Operator", ar.CurLine(), ar.CurCol(), tk1.c_str());
			//	return TK_NONE;
			//}

			tkType2 = GetToken(ar, tk2);
			if (tkType2 != TK_SEMICOLON)
			{
				SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "';' after 'yield'", tk2.c_str());
				return TK_NONE;
			}

			funs._cur->Push_OP(ar, NOP_YIELD, YILED_RETURN, 0, 0);
			r = TK_SEMICOLON;
			break;
		}
		default:
			SetParserCompileError(ar, PCE_SYNTAX_ERROR, tk1.c_str());
			return TK_NONE;
		}
	}
	if (operands.empty() == true)
		return r;

	if (operands.size() != operators.size() + 1)
	{
		SetParserCompileError(ar, PCE_INVALID_OPERATOR, tk1.c_str());
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
		if (b.IsFun())
		{
			if (op == NOP_MOV)
			{
				if (a.IsArray() == false)
				{
					funs._cur->Push_OP2(ar, NOP_FMOV1, a._iVar, b._iVar, false);
				}
				else
				{
					funs._cur->Push_Table_MASMDP(ar, NOP_FMOV2, a._iVar, a._iArrayIndex, b._iVar, false, a.IsHaveShort(), false);
				}
			}
			else
			{
				SetParserCompileError(ar, PCE_EXPECTED_LVALUE);
				return TK_NONE;
			}
		}
		else if (op < NOP_VAR_CLEAR)
		{
			if (a.IsArray() == false)
			{
				if (IsTempVar(a._iVar))
				{
					SetParserCompileError(ar, PCE_EXPECTED_LVALUE);
					return TK_NONE;
				}
				// 상수 풀(리터럴/const 치환 결과)에 쓰면 같은 값을 쓰는 모든 코드가 오염된다
				if (a._iVar >= COMPILE_STATIC_VAR_BEGIN && a._iVar < COMPILE_CALLARG_VAR_BEGIN)
				{
					SetParserCompileError(ar, PCE_EXPECTED_LVALUE);
					return TK_NONE;
				}
			}
			if (b.IsArray() == false)
			{
				if (a.IsArray() == false)
				{
					funs._cur->Push_OP2(ar, op, a._iVar, b._iVar, b.IsShort());
				}
				else
				{
					funs._cur->Push_Table_MASMDP(ar, op, a._iVar, a._iArrayIndex, b._iVar, false, a.IsHaveShort(), b.IsShort());
				}
			}
			else
			{
				if (a.IsArray() == false)
					funs._cur->Push_TableRead(ar, b._iVar, b._iArrayIndex, a._iVar, b.IsHaveShort());
				else
				{
					int iTempOffset2 = funs._cur->AllocLocalTempVar();
					funs._cur->Push_TableRead(ar, b._iVar, b._iArrayIndex, iTempOffset2, b.IsHaveShort());
					funs._cur->Push_Table_MASMDP(ar, op, a._iVar, a._iArrayIndex, iTempOffset2, false, false, false);
				}
			}
		}
		else
		{
			int iTempOffset2;

			if (a.IsArray() == true)
			{
				iTempOffset2 = funs._cur->AllocLocalTempVar();
				funs._cur->Push_TableRead(ar, a._iVar, a._iArrayIndex, iTempOffset2, a.IsHaveShort());
				a._iVar = iTempOffset2;
			}
			//else if(a.IsShort())
			//	a = funs.AddStaticInt(a._iVar);
			if (b.IsArray() == true)
			{
				iTempOffset2 = funs._cur->AllocLocalTempVar();
				funs._cur->Push_TableRead(ar, b._iVar, b._iArrayIndex, iTempOffset2, b.IsHaveShort());
				b._iVar = iTempOffset2;
			}
			//else if (b.IsShort())
			//	b = funs.AddStaticInt(b._iVar);

			iTempOffset2 = funs._cur->AllocLocalTempVar();
			funs._cur->Push_OP(ar, op, iTempOffset2, a._iVar, b._iVar, a.IsShort(), b.IsShort());
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
	case NOP_LOG_AND:	// &&
		return NOP_JMP_AND;
	case NOP_LOG_OR:	// ||
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
	case NOP_LOG_AND:	// &&
		return NOP_JMP_NAND;
	case NOP_LOG_OR:	// ||
		return NOP_JMP_NOR;
	default:
		break;
	}
	return NOP_NONE;
}



#ifdef CSTYLE_FOR
//	for Init
//	for {} Process
//	for Increase
//	for Check
bool ParseFor(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetParserCompileError(ar, PCE_LOGIC_NOT_ALLOWED_GLOBAL, "for");
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
	u8 byTempCheck[128 * 8];
	u8 byTempInc[128 * 8];
	debug_info DebugCheck[128];
	debug_info DebugInc[128];

	// "for (var i = 0; i < 789; i++)" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'(' after 'for'", tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());

	//==> for Init
	iTempOffset.Reset();
	r = ParseJob(false, iTempOffset, NULL, ar, funs, vars, true);
	if (TK_SEMICOLON != r) // 초기화
	{
		SetParserCompileError(ar, PCE_INVALID_FOR_INIT);
		return false;
	}

	funs._cur->Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur->_code->GetBufferOffset() - (sizeof(short)*3), funs._cur->_code->GetBufferOffset());

	int PosLoopTop = funs._cur->_code->GetBufferOffset(); // Loop 의 맨위

	// For Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
	if (TK_SEMICOLON != r)
	{
		SetParserCompileError(ar, PCE_INVALID_FOR_CONDITION);
		return false;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur->_code->GetBufferOffset();
	eNOperation opCheck = funs._cur->GetLastOP();
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
	funs._cur->_code->SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = Pos2 - Pos1;
	if (iCheckCodeSize > sizeof(byTempCheck))
	{
		SetParserCompileError(ar, PCE_CODE_BLOCK_TOO_LARGE, "for condition", iCheckCodeSize);
		return false;
	}
	funs._cur->_code->Read(byTempCheck, iCheckCodeSize);
	funs._cur->_code->SetPointer(Pos1, SEEK_SET);
	for (int i = 0; i < iCheckCodeSize / 8; i++)
		DebugCheck[i] = (*funs._cur->_pDebugData)[Pos1 / 8 + i];

	// For Increase
	iTempOffset = INVALID_ERROR_PARSEJOB;
	r = ParseJob(false, iTempOffset, NULL, ar, funs, vars); // 증감
	if (TK_R_SMALL != r)
	{
		SetParserCompileError(ar, PCE_INVALID_FOR_INCREMENT);
		return false;
	}
	Pos2 = funs._cur->_code->GetBufferOffset();
	funs._cur->_code->SetPointer(Pos1, SEEK_SET);
	int iIncCodeSize = Pos2 - Pos1;
	if (iIncCodeSize > sizeof(byTempInc))
	{
		SetParserCompileError(ar, PCE_CODE_BLOCK_TOO_LARGE, "for increment", iIncCodeSize);
		return false;
	}
	funs._cur->_code->Read(byTempInc, iIncCodeSize);
	funs._cur->_code->SetPointer(Pos1, SEEK_SET);
	for (int i = 0; i < iIncCodeSize / 8; i++)
		DebugInc[i] = (*funs._cur->_pDebugData)[Pos1 / 8 + i];

	//	for {} Process
	std::vector<SJumpValue> sJumps;
	std::vector<SJumpValue> sContinueJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars, false, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, &sContinueJumps);
		if (TK_SEMICOLON != r)
		{
			SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
			return false;
		}
		ClearTempVars(funs);
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars, NULL, &sContinueJumps))
			return false;
	}

	// Write Inc Code
	int continuePos = funs._cur->_code->GetBufferOffset();
	for (int i = 0; i < (int)sContinueJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sContinueJumps[i], continuePos);
	}
	funs._cur->_code->Write(byTempInc, iIncCodeSize);

	funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());
	// Write Check Code
	funs._cur->_code->Write(byTempCheck, iCheckCodeSize);
	if(false == isCheckOPOpt)
		funs._cur->Push_JMPTrue(ar, iStackCheckVar, PosLoopTop);
	else
	{
		int argLen = sizeof(short) * 3;
		funs._cur->_code->SetPointer(-((int)sizeof(OpType) + (int)sizeof(ArgFlag) + argLen), SEEK_CUR); // OP + flag + n1, n2, n3
		OpType optype = GetOpTypeFromOp(opOpz);
		funs._cur->_code->Write(&optype, sizeof(optype));
		ArgFlag flag = 0;
		funs._cur->_code->Write(&flag, sizeof(flag));
		int cur = funs._cur->_code->GetBufferOffset();
		funs._cur->Set_JumpOffet(SJumpValue(cur, cur + argLen), PosLoopTop);
		funs._cur->_code->SetPointer(argLen, SEEK_CUR);
	}
	funs._cur->ClearLastOP();


	int forEndPos = funs._cur->_code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	// Debug info Inc, check restore
	int iAddDebugCnt = iIncCodeSize / 8 + iCheckCodeSize / 8;
	int iOffDebug = forEndPos / 8 - iAddDebugCnt;
	funs._cur->_pDebugData->resize(iOffDebug + iAddDebugCnt);

	for (int i = 0; i < iIncCodeSize / 8; i++)
		(*funs._cur->_pDebugData)[iOffDebug + i] = DebugInc[i];
	for (int i = 0; i < iCheckCodeSize / 8; i++)
		(*funs._cur->_pDebugData)[iOffDebug + (iIncCodeSize / 8) + i] = DebugCheck[i];

	return true;
}
#else
bool ParseFor(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetParserCompileError(ar, PCE_LOGIC_NOT_ALLOWED_GLOBAL, "for");
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempVar;

	// "for(var a in range(begin,end,step))" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'(' after 'for'", tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_VAR) // var
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'var' in for-loop variable declaration", tk1.c_str());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING) // key
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "loop variable name", tk1.c_str());
		return false;
	}
	int iKey = AddLocalVarName(ar, funs, vars, false, tk1);
	if (iKey == -1)
		return false;

	//int iIterator1 = AddLocalVar(ar, funs, vars); // Current Save
	//if (iIterator1 == -1)
	//	return false;
	int i_Begin = AddLocalVar(ar, funs, vars); // Begin & iIterator1
	if (i_Begin == -1)
		return false;
	int i_End = AddLocalVar(ar, funs, vars); // End
	if (i_End == -1)
		return false;
	int i_Step = AddLocalVar(ar, funs, vars); // Step
	if (i_Step == -1)
		return false;


	//if (iKey + 1 != iIterator1)
	if (iKey + 1 != i_Begin)
	{
		SetParserCompileError(ar, PCE_INVALID_LOOP_VARIABLE_LAYOUT);
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING || tk1 != "in") // in
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'in' in for-loop", tk1.c_str());
		return false;
	}

	int iTryValue = -1;
	if (Try_ParseIntNum(iTryValue, ar, funs, vars, TK_COMMA) != TK_NONE)
	{
#if 0
		if (IsShort(iTryValue))
			funs._cur->Push_OP2(ar, NOP_MOV, i_Begin, iTryValue, true);
		else
#endif
			funs._cur->Push_MOVI(ar, i_Begin, iTryValue);
	}
	else
	{
		iTempVar.Reset();
		r = ParseJob(true, iTempVar, NULL, ar, funs, vars, true); // begin value
		if (TK_COMMA != r)
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "',' after for-loop begin value", tk1.c_str());
			return false;
		}
		if (iTempVar._iArrayIndex == INVALID_ERROR_PARSEJOB)
			funs._cur->Push_OP2(ar, NOP_MOV, i_Begin, iTempVar._iVar, false);
		else
			funs._cur->Push_TableRead(ar, iTempVar._iVar, iTempVar._iArrayIndex, i_Begin, iTempVar.IsHaveShort());
		//funs._cur->Push_OP(ar, NOP_VERIFY_TYPE, i_Begin, VAR_INT, 0);
		funs._cur->Push_OP(ar, NOP_CHANGE_INT, i_Begin, 0, 0);
	}

	int iDebugLoopLine = ar.CurLine();

	iTryValue = -1;
	if (Try_ParseIntNum(iTryValue, ar, funs, vars, TK_COMMA, TK_R_SMALL) != TK_NONE)
	{
#if 0
		if(IsShort(iTryValue))
			funs._cur->Push_OP2(ar, NOP_MOV, i_End, iTryValue, true);
		else
#endif
			funs._cur->Push_MOVI(ar, i_End, iTryValue);
	}
	else
	{
		iTempVar.Reset();
		r = ParseJob(true, iTempVar, NULL, ar, funs, vars, true); // end value
		if (TK_COMMA != r)
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "',' after for-loop end value", tk1.c_str());
			return false;
		}
		if (iTempVar._iArrayIndex == INVALID_ERROR_PARSEJOB)
			funs._cur->Push_OP2(ar, NOP_MOV, i_End, iTempVar._iVar, false);
		else
			funs._cur->Push_TableRead(ar, iTempVar._iVar, iTempVar._iArrayIndex, i_End, iTempVar.IsHaveShort());
		//funs._cur->Push_OP(ar, NOP_VERIFY_TYPE, i_End, VAR_INT, 0);
		funs._cur->Push_OP(ar, NOP_CHANGE_INT, i_End, 0, 0);
	}

	iTryValue = -1;
	if (Try_ParseIntNum(iTryValue, ar, funs, vars, TK_COMMA, TK_R_SMALL) != TK_NONE)
	{
#if 0
		if (IsShort(iTryValue))
			funs._cur->Push_OP2(ar, NOP_MOV, i_Step, iTryValue, true);
		else
#endif
			funs._cur->Push_MOVI(ar, i_Step, iTryValue);
	}
	else
	{
		iTempVar.Reset();
		r = ParseJob(true, iTempVar, NULL, ar, funs, vars, true); // increase value
		if (TK_R_SMALL != r)
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "')' after for-loop step value", tk1.c_str());
			return false;
		}
		if (iTempVar._iArrayIndex == INVALID_ERROR_PARSEJOB)
			funs._cur->Push_OP2(ar, NOP_MOV, i_Step, iTempVar._iVar, false);
		else
			funs._cur->Push_TableRead(ar, iTempVar._iVar, iTempVar._iArrayIndex, i_Step, iTempVar.IsHaveShort());
		//funs._cur->Push_OP(ar, NOP_VERIFY_TYPE, i_Step, VAR_INT, 0);
		funs._cur->Push_OP(ar, NOP_CHANGE_INT, i_Step, 0, 0);
	}


	//funs._cur->Push_OP2(ar, NOP_MOV, iIterator1, i_Begin, false); // Cur_inter = Begin

	funs._cur->Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset());

	int PosLoopTop = funs._cur->_code->GetBufferOffset(); // Loop 의 맨위


	//	for {} Process
	std::vector<SJumpValue> sJumps;
	std::vector<SJumpValue> sContinueJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempVar = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempVar, &sJumps, ar, funs, vars, false, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, &sContinueJumps);
		if (TK_SEMICOLON != r)
		{
			SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
			return false;
		}
		ClearTempVars(funs);
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars, NULL, &sContinueJumps))
			return false;
	}

	funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());
	int continuePos = funs._cur->_code->GetBufferOffset();
	for (int i = 0; i < (int)sContinueJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sContinueJumps[i], continuePos);
	}
	funs._cur->Push_JMPFor(ar, PosLoopTop, iKey, iKey + 2, iDebugLoopLine);

	funs._cur->ClearLastOP();


	int forEndPos = funs._cur->_code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return true;
}
#endif

bool ParseForEach(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetParserCompileError(ar, PCE_LOGIC_NOT_ALLOWED_GLOBAL, "foreach");
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;

	int iDebugLoopLine = ar.CurLine();

	// "foreach(var a, b in table)" 로직 처리
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'(' after 'foreach'", tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_VAR) // var
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'var' in foreach variable declaration", tk1.c_str());
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING) // key
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "foreach key variable name", tk1.c_str());
		return false;
	}
	int iKey = AddLocalVarName(ar, funs, vars, false, tk1);
	if(iKey == -1)
		return false;

	int iValue = -1;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_COMMA) // ,
	{
		if (tkType1 == TK_STRING && tk1 == "in")
		{
			iValue = AddLocalVar(ar, funs, vars);
			ar.PushToken(tkType1, tk1);
		}
		else
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "',' or 'in' in foreach declaration", tk1.c_str());
			return false;
		}
	}
	else
	{
		tkType1 = GetToken(ar, tk1);
		if (tkType1 != TK_STRING) // value
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "foreach value variable name", tk1.c_str());
			return false;
		}
		iValue = AddLocalVarName(ar, funs, vars, false, tk1);
	}
	if (iValue == -1)
		return false;

	if (iKey + 1 != iValue)
	{
		SetParserCompileError(ar, PCE_INVALID_LOOP_VARIABLE_LAYOUT);
		return false;
	}

	// Iterator를 저장할 임시 자동 생성 변수
	int iIterator = AddLocalVar(ar, funs, vars);
	if (iIterator == -1)
		return false;

	if (iKey + 2 != iIterator)
	{
		SetParserCompileError(ar, PCE_INVALID_LOOP_VARIABLE_LAYOUT);
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING || tk1 != "in") // in
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'in' in foreach declaration", tk1.c_str());
		return false;
	}
	/*
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING) // Table Name
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'table_name' != %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}
	int iTable = vars.FindVar(tk1);
	if(iTable == -1)
	{
		SetCompileError(ar, "Error (%d, %d): foreach 'talbe' Not Found %s", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}
	*/
	int iTable = -1;
	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
	if (iTempOffset._iArrayIndex != INVALID_ERROR_PARSEJOB)
	{
		iTable = AddLocalVar(ar, funs, vars);
		funs._cur->Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iTable, operand.IsHaveShort());
	}
	else
		iTable = operand._iVar;


	//tkType1 = GetToken(ar, tk1);
	if (r != TK_R_SMALL) // )
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "')' after foreach source expression", tk1.c_str());
		return false;
	}


	funs._cur->Push_OP1(ar, NOP_VAR_CLEAR, iKey);

	funs._cur->Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset());

	int PosLoopTop = funs._cur->_code->GetBufferOffset(); // Loop 의 맨위


	//	foreach {} Process
	std::vector<SJumpValue> sJumps;
	std::vector<SJumpValue> sContinueJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars, false, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, &sContinueJumps);
		if (TK_SEMICOLON != r)
		{
			SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
			return false;
		}
		ClearTempVars(funs);
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars, NULL, &sContinueJumps))
			return false;
	}

	funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());
	int continuePos = funs._cur->_code->GetBufferOffset();
	for (int i = 0; i < (int)sContinueJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sContinueJumps[i], continuePos);
	}
	funs._cur->Push_JMPForEach(ar, PosLoopTop, iTable, iKey, iDebugLoopLine);

	funs._cur->ClearLastOP();


	int forEndPos = funs._cur->_code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	return true;
}
bool ParseWhile(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetParserCompileError(ar, PCE_LOGIC_NOT_ALLOWED_GLOBAL, "while");
		return false;
	}

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;
	u8 byTempCheck[128 * 8];
	debug_info DebugCheck[128];

	int iDebugLoopLine = ar.CurLine();

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'(' after 'while'", tk1.c_str());
		return false;
	}

	AddLocalVar(vars.GetCurrentLayer());


	funs._cur->Push_JMP(ar, 0); // for Check 위치로 JMP(일단은 위치만 확보)
	SJumpValue jmp1(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset());

	int PosLoopTop = funs._cur->_code->GetBufferOffset(); // Loop 의 맨위

	// While Check
	int Pos1 = PosLoopTop;
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, NULL, ar, funs, vars);
	if (TK_R_SMALL != r)
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "')' after while condition", tk1.c_str());
		return false;
	}
	int iStackCheckVar = iTempOffset._iVar;
	int Pos2 = funs._cur->_code->GetBufferOffset();
	eNOperation opCheck = funs._cur->GetLastOP();
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
	funs._cur->_code->SetPointer(Pos1, SEEK_SET);
	int iCheckCodeSize = (int)Pos2 - Pos1;
	if (iCheckCodeSize > (int)sizeof(byTempCheck))
	{
		SetParserCompileError(ar, PCE_CODE_BLOCK_TOO_LARGE, "while condition", iCheckCodeSize);
		return false;
	}
	funs._cur->_code->Read(byTempCheck, iCheckCodeSize);
	funs._cur->_code->SetPointer(Pos1, SEEK_SET);
	if (ar._debug)
	{
		for (int i = 0; i < iCheckCodeSize / 8; i++)
			DebugCheck[i] = (*funs._cur->_pDebugData)[Pos1 / 8 + i];
	}


	//	while {} Process
	std::vector<SJumpValue> sJumps;
	std::vector<SJumpValue> sContinueJumps;
	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE) // {
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, &sJumps, ar, funs, vars, false, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, &sContinueJumps);
		if (TK_SEMICOLON != r)
		{
			SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
			return false;
		}
		ClearTempVars(funs);
	}
	else
	{
		if (false == ParseMiddleArea(&sJumps, ar, funs, vars, NULL, &sContinueJumps))
			return false;
	}

	funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());
	int continuePos = funs._cur->_code->GetBufferOffset();
	for (int i = 0; i < (int)sContinueJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sContinueJumps[i], continuePos);
	}
	funs._cur->_code->Write(byTempCheck, iCheckCodeSize);
	if (false == isCheckOPOpt)
		funs._cur->Push_JMPTrue(ar, iStackCheckVar, PosLoopTop, iDebugLoopLine);
	else
	{
		int argLen = sizeof(short) * 3;
		funs._cur->_code->SetPointer(-((int)sizeof(OpType) + (int)sizeof(ArgFlag) + argLen), SEEK_CUR); // OP + n1, n2, n3
		OpType optype = GetOpTypeFromOp(opOpz);
		funs._cur->_code->Write(&optype, sizeof(optype));
		ArgFlag flag = 0;
		funs._cur->_code->Write(&flag, sizeof(flag));
		int cur = funs._cur->_code->GetBufferOffset();
		funs._cur->Set_JumpOffet(SJumpValue(cur, cur + argLen), PosLoopTop);
		funs._cur->_code->SetPointer(argLen, SEEK_CUR);
	}
	funs._cur->ClearLastOP();

	int forEndPos = funs._cur->_code->GetBufferOffset();

	for (int i = 0; i < (int)sJumps.size(); i++)
	{
		funs._cur->Set_JumpOffet(sJumps[i], forEndPos);
	}

	DelLocalVar(vars.GetCurrentLayer());

	// Debug info Inc, check restore
	if(ar._debug)
	{
		int iAddDebugCnt = iCheckCodeSize / 8;
		int iOffDebug = forEndPos / 8 - iAddDebugCnt;
		funs._cur->_pDebugData->resize(iOffDebug + iAddDebugCnt);

		for (int i = 0; i < iCheckCodeSize / 8; i++)
			(*funs._cur->_pDebugData)[iOffDebug + i] = DebugCheck[i];
	}
	return true;
}
bool ParseIF(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool* lastOPReturn, std::vector<SJumpValue>* pContinueJumps = NULL)
{
	if (funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME && false == ar._allowGlobalInitLogic)
	{
		SetParserCompileError(ar, PCE_LOGIC_NOT_ALLOWED_GLOBAL, "if");
		return false;
	}
	if(lastOPReturn) *lastOPReturn = false;

	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SOperand iTempOffset;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_SMALL) // (
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'(' after 'if'", tk1.c_str());
		return false;
	}

	//==> if( xxx )
	iTempOffset.Reset();
	r = ParseJob(true, iTempOffset, pJumps, ar, funs, vars, true, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, pContinueJumps);
	if (TK_R_SMALL != r) // )
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "')' after if condition", tk1.c_str());
		return false;
	}

	SJumpValue jmp1;
	SJumpValue jmp2;
	bool blJmp1 = true;
	bool blJmp2 = true;

	eNOperation opCheck = funs._cur->GetLastOP();
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
		funs._cur->Push_JMPFalse(ar, iTempOffset._iVar, 0);
		jmp1.Set(funs._cur->_code->GetBufferOffset() - 6, funs._cur->_code->GetBufferOffset()); // before "- 2"
	}
	else
	{
		int argLen = sizeof(short) * 3;
		funs._cur->_code->SetPointer(-((int)sizeof(OpType) + (int)sizeof(ArgFlag) + argLen), SEEK_CUR); // OP + flag +  n1, n2, n3
		
		eNOperation optype = GetOpTypeFromOp(opOpz);
		funs._cur->_code->Write(&optype, sizeof(optype));
		funs._cur->_code->SetPointer((int)sizeof(ArgFlag), SEEK_CUR);

		int cur = funs._cur->_code->GetBufferOffset();
		funs._cur->Set_JumpOffet(SJumpValue(cur, cur + argLen), 0);
		funs._cur->_code->SetPointer(argLen, SEEK_CUR);

		jmp1.Set(funs._cur->_code->GetBufferOffset() - 6, funs._cur->_code->GetBufferOffset());
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_L_MIDDLE)
	{
		AddLocalVar(vars.GetCurrentLayer());

		if (false == ParseMiddleArea(pJumps, ar, funs, vars, NULL, pContinueJumps))
			return false;

		DelLocalVar(vars.GetCurrentLayer());
	}
	else
	{
		ar.PushToken(tkType1, tk1);
		iTempOffset = INVALID_ERROR_PARSEJOB;
		r = ParseJob(false, iTempOffset, pJumps, ar, funs, vars, false, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, pContinueJumps);

		if (TK_SEMICOLON != r)
		{
			SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
			return false;
		}
	}

	ClearTempVars(funs);

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_ELSEIF)
	{
		if (funs._cur->GetLastOP() == NOP_RETURN) // Code Size OPT TODO !!
			blJmp2 = false;
		if (blJmp2)
		{
			funs._cur->Push_JMP(ar, 0);
			jmp2.Set(funs._cur->_code->GetBufferOffset() - 6, funs._cur->_code->GetBufferOffset());
		}

		funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());

		if (false == ParseIF(pJumps, ar, funs, vars, lastOPReturn, pContinueJumps))
			return false;

		ClearTempVars(funs);
		if (blJmp2) // Code Size OPT TODO !!
			funs._cur->Set_JumpOffet(jmp2, funs._cur->_code->GetBufferOffset());
	}	
	else if (tkType1 == TK_ELSE)
	{
		if (funs._cur->GetLastOP() == NOP_RETURN) // Code Size OPT TODO !!
			blJmp2 = false;
		if(blJmp2)
		{
			funs._cur->Push_JMP(ar, 0);
			jmp2.Set(funs._cur->_code->GetBufferOffset() - 6, funs._cur->_code->GetBufferOffset());
		}
		funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());

		tkType1 = GetToken(ar, tk1);
		/*if (tkType1 == TK_IF)
		{
			if (false == ParseIF(pJumps, ar, funs, vars, NULL, pContinueJumps))
				return false;
		}
		else*/ if (tkType1 == TK_L_MIDDLE)
		{
			AddLocalVar(vars.GetCurrentLayer());

			if (false == ParseMiddleArea(pJumps, ar, funs, vars, NULL, pContinueJumps))
				return false;

			DelLocalVar(vars.GetCurrentLayer());
		}
		else
		{
			ar.PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, pJumps, ar, funs, vars, false, TK_SEMICOLON, TK_COMMA, TK_R_SMALL, TK_R_ARRAY, pContinueJumps);

			if (TK_SEMICOLON != r)
			{
				SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
				return false;
			}
		}
		ClearTempVars(funs);
		if(blJmp2) // Code Size OPT TODO !!
			funs._cur->Set_JumpOffet(jmp2, funs._cur->_code->GetBufferOffset());
		else if (funs._cur->GetLastOP() == NOP_RETURN)
		{
			if (lastOPReturn) *lastOPReturn = true;
		}
	}
	else
	{
		ar.PushToken(tkType1, tk1);
		//if (funs._cur->GetLastOP() != NOP_RETURN) // Code Size OPT TODO !!
			funs._cur->Set_JumpOffet(jmp1, funs._cur->_code->GetBufferOffset());
	}
	return true;
}

bool ParseVarDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool blExport)
{
	std::string tk1;
	TK_TYPE tkType1;
	TK_TYPE r;

	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_STRING)
	{
		int iLocalVar = AddLocalVarName(ar, funs, vars, blExport, tk1);
		if (iLocalVar == -1)
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
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "variable name after 'var'", tk1.c_str());
		return false;
	}
	return true;
}

// ---------------- 스크립트 const (컴파일타임 상수 선언) ----------------
// const NAME = <상수 표현식>;  →  모듈-로컬 define 으로 등록되어 이후 토큰 치환.
// 표현식은 컴파일 타임에 평가 (리터럴, 기존 define/const, 산술/비트 연산, 괄호).
struct SConstValue
{
	NeoCompileDefineTokenType type = NEO_DEFINE_TOKEN_NULL;
	int i = 0;
	double f = 0;
	std::string s;

	bool IsNum() const { return type == NEO_DEFINE_TOKEN_INT || type == NEO_DEFINE_TOKEN_FLOAT; }
	double Num() const { return type == NEO_DEFINE_TOKEN_INT ? (double)i : f; }
};

static bool ParseConstExpr(SConstValue& out, int minPrec, CArchiveRdWC& ar);

static bool ParseConstPrimary(SConstValue& out, CArchiveRdWC& ar)
{
	std::string tk;
	TK_TYPE tkType = GetToken(ar, tk);

	switch (tkType)
	{
	case TK_PLUS:
		if (false == ParseConstPrimary(out, ar))
			return false;
		if (false == out.IsNum())
		{
			SetParserCompileError(ar, PCE_CONST_INVALID_OP, "+");
			return false;
		}
		return true;
	case TK_MINUS:
		if (false == ParseConstPrimary(out, ar))
			return false;
		if (out.type == NEO_DEFINE_TOKEN_INT) { out.i = -out.i; return true; }
		if (out.type == NEO_DEFINE_TOKEN_FLOAT) { out.f = -out.f; return true; }
		SetParserCompileError(ar, PCE_CONST_INVALID_OP, "-");
		return false;
	case TK_NOT: // ~
		if (false == ParseConstPrimary(out, ar))
			return false;
		if (out.type != NEO_DEFINE_TOKEN_INT)
		{
			SetParserCompileError(ar, PCE_CONST_INVALID_OP, "~");
			return false;
		}
		out.i = ~out.i;
		return true;
	case TK_L_SMALL:
		if (false == ParseConstExpr(out, 0, ar))
			return false;
		tkType = GetToken(ar, tk);
		if (tkType != TK_R_SMALL)
		{
			SetParserCompileError(ar, PCE_EXPECTED_RIGHT_PAREN);
			return false;
		}
		return true;
	case TK_QUOTE2:
	case TK_QUOTE1:
		if (false == GetQuotationString(ar, out.s, tkType == TK_QUOTE2 ? '"' : '\''))
		{
			SetParserCompileError(ar, PCE_UNTERMINATED_STRING);
			return false;
		}
		out.type = NEO_DEFINE_TOKEN_STRING;
		return true;
	case TK_STRING_LITERAL: // define/const 치환으로 완성된 문자열
		out.type = NEO_DEFINE_TOKEN_STRING;
		out.s = tk;
		return true;
	case TK_TRUE:  out.type = NEO_DEFINE_TOKEN_TRUE;  return true;
	case TK_FALSE: out.type = NEO_DEFINE_TOKEN_FALSE; return true;
	case TK_NULL:  out.type = NEO_DEFINE_TOKEN_NULL;  return true;
	case TK_STRING:
	{
		double num;
		if (false == StringToDouble(num, tk.c_str()))
		{
			// define/const 로 치환되지 않은 식별자 → 컴파일 타임 상수가 아님
			SetParserCompileError(ar, PCE_CONST_INVALID_VALUE, tk.c_str());
			return false;
		}
		bool isFloat = IsFloatNumberToken(tk); // 치환으로 통째로 들어온 "1.5", "1e+20" 등
		if (false == isFloat && ar.GetData(false) == '.')
		{
			// 토크나이저는 "1.5" 를 "1" '.' "5" 로 쪼갠다 (ParseNum 과 동일 처리)
			ar.GetData(true);
			std::string tk2;
			GetToken(ar, tk2);
			double num2 = 0;
			if (false == StringToDoubleLow(num2, tk2.c_str()))
			{
				SetParserCompileError(ar, PCE_INVALID_NUMBER_LITERAL);
				return false;
			}
			num += num2;
			isFloat = true;
		}
		if (isFloat) { out.type = NEO_DEFINE_TOKEN_FLOAT; out.f = num; }
		else         { out.type = NEO_DEFINE_TOKEN_INT;   out.i = (int)num; }
		return true;
	}
	default:
		SetParserCompileError(ar, PCE_CONST_INVALID_VALUE, tk.c_str());
		return false;
	}
}

static int ConstBinOpPrec(TK_TYPE t)
{
	switch (t)
	{
	case TK_MUL: case TK_DIV: case TK_PERCENT: return 6;
	case TK_PLUS: case TK_MINUS: return 5;
	case TK_LSHIFT: case TK_RSHIFT: return 4;
	case TK_AND: return 3;
	case TK_XOR: return 2;
	case TK_OR: return 1;
	default: return 0;
	}
}

static bool EvalConstBinOp(SConstValue& a, TK_TYPE op, const SConstValue& b, CArchiveRdWC& ar)
{
	if (false == a.IsNum() || false == b.IsNum())
	{
		SetParserCompileError(ar, PCE_CONST_INVALID_OP, GetTokenString(op).c_str());
		return false;
	}
	bool bothInt = (a.type == NEO_DEFINE_TOKEN_INT && b.type == NEO_DEFINE_TOKEN_INT);
	switch (op)
	{
	case TK_PLUS:
	case TK_MINUS:
	case TK_MUL:
		if (bothInt)
			a.i = (op == TK_PLUS) ? (a.i + b.i) : (op == TK_MINUS) ? (a.i - b.i) : (a.i * b.i);
		else
		{
			double x = a.Num(), y = b.Num();
			a.type = NEO_DEFINE_TOKEN_FLOAT;
			a.f = (op == TK_PLUS) ? (x + y) : (op == TK_MINUS) ? (x - y) : (x * y);
		}
		return true;
	case TK_DIV:
	case TK_PERCENT:
		if ((bothInt && b.i == 0) || (false == bothInt && b.Num() == 0))
		{
			SetParserCompileError(ar, PCE_CONST_DIV_ZERO);
			return false;
		}
		if (bothInt)
			a.i = (op == TK_DIV) ? (a.i / b.i) : (a.i % b.i);
		else
		{
			double x = a.Num(), y = b.Num();
			a.type = NEO_DEFINE_TOKEN_FLOAT;
			a.f = (op == TK_DIV) ? (x / y) : fmod(x, y);
		}
		return true;
	case TK_LSHIFT: case TK_RSHIFT: case TK_AND: case TK_XOR: case TK_OR:
		if (false == bothInt)
		{
			SetParserCompileError(ar, PCE_CONST_INVALID_OP, GetTokenString(op).c_str());
			return false;
		}
		switch (op)
		{
		case TK_LSHIFT: a.i <<= b.i; break;
		case TK_RSHIFT: a.i >>= b.i; break;
		case TK_AND:    a.i &= b.i;  break;
		case TK_XOR:    a.i ^= b.i;  break;
		default:        a.i |= b.i;  break;
		}
		return true;
	default:
		SetParserCompileError(ar, PCE_CONST_INVALID_OP, GetTokenString(op).c_str());
		return false;
	}
}

static bool ParseConstExpr(SConstValue& out, int minPrec, CArchiveRdWC& ar)
{
	if (false == ParseConstPrimary(out, ar))
		return false;

	while (true)
	{
		std::string tk;
		TK_TYPE tkType = GetToken(ar, tk);
		int prec = ConstBinOpPrec(tkType);
		if (prec == 0 || prec < minPrec)
		{
			ar.PushToken(tkType, tk); // 종결자(';', ')')는 호출자가 소비
			return true;
		}
		SConstValue rhs;
		if (false == ParseConstExpr(rhs, prec + 1, ar))
			return false;
		if (false == EvalConstBinOp(out, tkType, rhs, ar))
			return false;
	}
}

bool ParseConstDef(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string name, tk;
	TK_TYPE tkType;

	ar.m_bSuppressDefines = true; // 이름은 치환 없이 raw 로 읽는다 (중복 선언 감지)
	tkType = GetToken(ar, name);
	ar.m_bSuppressDefines = false;

	if (tkType != TK_STRING || false == AbleName(name))
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "const name", name.c_str());
		return false;
	}
	if (ar.m_sScriptDefines.values.find(name) != ar.m_sScriptDefines.values.end() ||
		(ar.m_pDefines != nullptr && ar.m_pDefines->values.find(name) != ar.m_pDefines->values.end()) ||
		vars.FindVar(name) != -1 ||
		funs.FindFun(name) != NULL)
	{
		SetParserCompileError(ar, PCE_CONST_DUPLICATE, name.c_str());
		return false;
	}

	tkType = GetToken(ar, tk);
	if (tkType != TK_EQUAL)
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'=' after const name", tk.c_str());
		return false;
	}

	SConstValue v;
	if (false == ParseConstExpr(v, 0, ar))
		return false;

	tkType = GetToken(ar, tk);
	if (tkType != TK_SEMICOLON)
	{
		SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
		return false;
	}

	NeoCompileDefineToken d;
	switch (v.type)
	{
	case NEO_DEFINE_TOKEN_INT:
		d.type = NEO_DEFINE_TOKEN_INT;
		d.text = std::to_string(v.i);
		break;
	case NEO_DEFINE_TOKEN_FLOAT:
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.17g", v.f);
		d.type = NEO_DEFINE_TOKEN_FLOAT;
		d.text = buf;
		// "2" 처럼 정수 형태로 떨어지면 int 로 재해석되지 않게 소수점을 보존
		if (d.text.find('.') == std::string::npos &&
			d.text.find('e') == std::string::npos && d.text.find('E') == std::string::npos)
			d.text += ".0";
		break;
	}
	case NEO_DEFINE_TOKEN_STRING:
		d.type = NEO_DEFINE_TOKEN_STRING;
		d.text = v.s;
		break;
	case NEO_DEFINE_TOKEN_TRUE:  d.type = NEO_DEFINE_TOKEN_TRUE;  d.text = "true";  break;
	case NEO_DEFINE_TOKEN_FALSE: d.type = NEO_DEFINE_TOKEN_FALSE; d.text = "false"; break;
	default:                     d.type = NEO_DEFINE_TOKEN_NULL;  d.text = "null";  break;
	}
	ar.m_sScriptDefines.values[name] = d;
	return true;
}

bool ParseClass(CArchiveRdWC& ar, SFunctions& funs, SVars& vars)
{
	std::string tk1;
	TK_TYPE tkType1;
//	TK_TYPE r;

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_STRING)
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "class name", tk1.c_str());
		return false;
	}
	if (false == UseableName(ar, funs, vars, tk1, true))
	{
		//SetCompileError(ar, "Error (%d, %d): Ununable Class Name (%s)", ar.CurLine(), ar.CurCol(), tk1.c_str());
		return false;
	}

	SLayerVar* pCurLayer = vars.GetCurrentLayer();
	/*
	tkType1 = GetToken(ar, tk1);
	if (tkType1 == TK_STRING)
	{
		int iLocalVar = AddLocalVarName(ar, funs, vars, blExport, tk1);
		if (iLocalVar == -1)
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
		SetCompileError(ar, "Error (%d, %d): Function Local Var (%s) %d", ar.CurLine(), ar.CurCol(), funs.GetCurFunName().c_str(), tk1.c_str());
		return false;
	}*/
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
		SetParserCompileError(ar, PCE_EXPECTED_LEFT_PAREN);
		return false;
	}
	SOperand operand;
	r = ParseJob(true, operand, NULL, ar, funs, vars);
	if (TK_R_SMALL != r)
	{
		SetParserCompileError(ar, PCE_EXPECTED_RIGHT_PAREN);
		return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_SEMICOLON)
	{
		SetParserCompileError(ar, PCE_INVALID_STATEMENT_END);
		return false;
	}

	if (operand.IsShort())
		funs._cur->Push_OP2(ar, NOP_SLEEP, 0, operand._iVar, true);
	else
	{
		if (operand._iArrayIndex == INVALID_ERROR_PARSEJOB)
			funs._cur->Push_OP2(ar, NOP_SLEEP, 0, operand._iVar, false);
		else
		{
			int iTempVar = funs._cur->AllocLocalTempVar();
			funs._cur->Push_TableRead(ar, operand._iVar, operand._iArrayIndex, iTempVar, operand.IsHaveShort());
			funs._cur->Push_OP2(ar, NOP_SLEEP, 0, operand._iVar, false);
		}
	}
	return true;
}



bool ParseMiddleArea(std::vector<SJumpValue>* pJumps, CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool* lastOPReturn, std::vector<SJumpValue>* pContinueJumps)
{
	std::string tk1, tk2;
	TK_TYPE tkType1, tkType2;
	TK_TYPE r;

	SOperand iTempOffset;
	FUNCTION_TYPE funType = FUNT_NORMAL;

	SLayerVar* pCurLayer = vars.GetCurrentLayer();

	bool bGlobalLocal;
	bool blEnd = false;
	while (blEnd == false)
	{
		ClearTempVars(funs);

		bGlobalLocal = false;
		tkType1 = GetToken(ar, tk1);
		switch (tkType1)
		{
		case TK_NONE:
			if (funs.GetCurFunName() != GLOBAL_INIT_FUN_NAME)
			{
				SetParserCompileError(ar, PCE_UNEXPECTED_FUNCTION_END, funs.GetCurFunName().c_str());
				return false;
			}
			blEnd = true;
			break;
		case TK_L_MIDDLE:
			AddLocalVar(vars.GetCurrentLayer());

			if (false == ParseMiddleArea(pJumps, ar, funs, vars, NULL, pContinueJumps))
				return false;

			DelLocalVar(vars.GetCurrentLayer());
			if(lastOPReturn) *lastOPReturn = false;
			break;
		case TK_R_MIDDLE:
			if (ar._allowGlobalInitLogic == false && funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME)
			{
				SetParserCompileError(ar, PCE_UNEXPECTED_GLOBAL_BLOCK_END);
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
				SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "';' after 'return'", tk1.c_str());
				return false;
			}
			if (lastOPReturn) *lastOPReturn = true;
			break;
		case TK_VAR:
			if (false == ParseVarDef(ar, funs, vars, funType == FUNT_EXPORT))
				return false;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_CONST:
			if (funs.GetCurFunName() != GLOBAL_INIT_FUN_NAME)
			{
				SetParserCompileError(ar, PCE_CONST_NOT_GLOBAL);
				return false;
			}
			if (false == ParseConstDef(ar, funs, vars))
				return false;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_CLASS:
			if (false == ParseClass(ar, funs, vars))
				return false;
			break;
		case TK_BREAK:
			if (pJumps == NULL)
			{
				SetParserCompileError(ar, PCE_BREAK_OUTSIDE_LOOP);
				return TK_NONE;
			}
			funs._cur->Push_JMP(ar, 0);
			pJumps->push_back(SJumpValue(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset()));
			tkType2 = GetToken(ar, tk2);
			if (tkType2 != TK_SEMICOLON)
			{
				SetParserCompileError(ar, PCE_EXPECTED_BREAK_SEMICOLON);
				return false;
			}
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_CONTINUE:
			if (pContinueJumps == NULL)
			{
				SetParserCompileError(ar, PCE_CONTINUE_OUTSIDE_LOOP);
				return TK_NONE;
			}
			funs._cur->Push_JMP(ar, 0);
			pContinueJumps->push_back(SJumpValue(funs._cur->_code->GetBufferOffset() - (sizeof(short) * 3), funs._cur->_code->GetBufferOffset()));
			tkType2 = GetToken(ar, tk2);
			if (tkType2 != TK_SEMICOLON)
			{
				SetParserCompileError(ar, PCE_EXPECTED_CONTINUE_SEMICOLON);
				return false;
			}
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_IMPORT:
			if (ParseImport(ar, funs, vars) == false)
				return false;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_EXPORT:
			funType = FUNT_EXPORT;
			break;
		case TK_FUN:
			tkType2 = GetToken(ar, tk2);
			if (false == AbleName(tk2))
			{
				SetParserCompileError(ar, PCE_INVALID_FUNCTION_NAME, tk2.c_str());
				return false;
			}
			if (tkType2 == TK_STRING)
			{
				if (-1 == ParseFunctionBase(ar, funs, vars, tk2, funType))
					return false;
			}
			else
			{
				SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "function name", tk2.c_str());
				return false;
			}
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_SLEEP:
			if (false == ParseSleep(ar, funs, vars))
				return false;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_STRING:
		case TK_MINUS2:
		case TK_PLUS2:
		case TK_YIELD:
			ar.PushToken(tkType1, tk1);
			iTempOffset = INVALID_ERROR_PARSEJOB;
			r = ParseJob(false, iTempOffset, NULL, ar, funs, vars);
			if (TK_NONE == r)
			{
				//SetCompileError(ar, "Error (%d, %d): ", ar.CurLine(), ar.CurCol());
				return false;
			}
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_IF:
			bGlobalLocal = funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME;
			if(bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME "IF";
			if (false == ParseIF(pJumps, ar, funs, vars, lastOPReturn, pContinueJumps))
				return false;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME;
			break;
		case TK_FOR:
			bGlobalLocal = funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME "FOR";
			if (false == ParseFor(ar, funs, vars))
				return false;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_FOREACH:
			bGlobalLocal = funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME "FOREACH";
			if (false == ParseForEach(ar, funs, vars))
				return false;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		case TK_WHILE:
			bGlobalLocal = funs.GetCurFunName() == GLOBAL_INIT_FUN_NAME;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME "WHILE";
			if (false == ParseWhile(ar, funs, vars))
				return false;
			if (bGlobalLocal) funs._cur->_name = GLOBAL_INIT_FUN_NAME;
			if (lastOPReturn) *lastOPReturn = false;
			break;
		default:
			SetParserCompileError(ar, PCE_UNEXPECTED_TOKEN, tk1.c_str(), funs.GetCurFunName().c_str());
			return false;
			break;
		}

		if (tkType1 != TK_EXPORT)
			funType = FUNT_NORMAL;
	}
	return true;
}

void FinalizeFuction(SFunctions& funs)
{
	int iCode_Begin = funs._cur->_iCode_Begin;
	funs._cur->_iCode_Begin = funs._codeFinal.GetBufferOffset();

	funs._codeTemp.SetBufferOffset(iCode_Begin);
	u8* pSrc = (u8*)funs._codeTemp.GetDataCurrent();
	funs._codeFinal.Write(pSrc, funs._cur->_iCode_Size);

	if (funs._cur->_pDebugData)
	{
		int iSrcBeginDebugOff = iCode_Begin / 8;
		int base = (int)funs.m_sDebugFinal.size();
		funs.m_sDebugFinal.resize(funs._codeFinal.GetBufferOffset() / 8);
		for (int i = base; i < (int)funs.m_sDebugFinal.size(); i++)
			funs.m_sDebugFinal[i] = (*funs._cur->_pDebugData)[i + iSrcBeginDebugOff - base];

		funs._cur->_pDebugData->resize(iSrcBeginDebugOff);
	}
	
	funs._funSequence.push_back(funs.FindFun(funs.GetCurFunName()));
}

bool ParseFunctionBody(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, bool addOPFunEnd)
{
	bool LastOPReturn = false;
	if (false == ParseMiddleArea(NULL, ar, funs, vars, &LastOPReturn))
		return false;

	if (ar.m_sErrorString.empty() == false)
		return false;

	if(addOPFunEnd && LastOPReturn == false)
		//funs._cur->Push_FUNEND(ar);
		funs._cur->Push_RETURN(ar, 0, true); // 

	return true;
}
bool ParseFunction(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, std::string& fname, FUNCTION_TYPE funType)
{
	//funs._cur->Clear();
	//funs._cur->_name = fname;
	bool fowardDeclaration = false;
	int fowardArgCnt = 0;

	SFunctionInfo* pF = funs.FindFun(fname);
	if(pF == nullptr)
	{
		int funID = funs.GetFunCountAll();

		if (funType == FUNT_ANONYMOUS)
		{
			char ch[128];
			snprintf(ch, _countof(ch), "#@_%d", funID);
			fname = ch;
		}

		pF = funs.NewFun(fname, funs._cur->_code, funs._cur->_pDebugData);
		pF->_funType = funType;
		pF->_moduleName = ar.m_sModuleName;

		pF->_funID = funID;
		funs._funIDs[pF->_funID] = pF;
	}
	else
	{
		fowardDeclaration = true;
		fowardArgCnt = (int)pF->_args.size();
		pF->_args.clear();
	}
	funs._cur = pF;

	std::string tk1;
	TK_TYPE tkType1;

	SLayerVar* pCurLayer = AddVarsFunction(vars);
	if (false == ParseFunctionArg(ar, funs, pCurLayer))
		return false;

	if(fowardDeclaration)
	{
		if(fowardArgCnt != (int)pF->_args.size())
			return false;
	}

	tkType1 = GetToken(ar, tk1);
	if (tkType1 != TK_L_MIDDLE)
	{
		if (funs._cur->_funType == FUNT_ANONYMOUS)
		{
			SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'{' to start anonymous function body", tk1.c_str());
			return false;
		}
		if (tkType1 == TK_SEMICOLON)
		{
			DelVarsFunction(vars);

//			SFunctionInfo* pF = funs.FindFun(funs.GetCurFunName());
//			*pF = funs._cur;
			return true;
		}
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'{' to start function body", tk1.c_str());
		return false;
	}

	funs._cur->_iCode_Begin = funs._cur->_code->GetBufferOffset();
	if (false == ParseFunctionBody(ar, funs, vars))
		return false;

	funs._cur->_iCode_Size = funs._cur->_code->GetBufferOffset() - funs._cur->_iCode_Begin;
	if (funs._cur->_iCode_Size)
	{
		FinalizeFuction(funs);
	}

//	SFunctionInfo* pF = funs.FindFun(funs.GetCurFunName());
//	*pF = funs._cur;

	DelVarsFunction(vars);

	return true;
}
int ParseFunctionBase(CArchiveRdWC& ar, SFunctions& funs, SVars& vars, std::string fname, FUNCTION_TYPE funType)
{
	std::string tk3;
	auto tkType3 = GetToken(ar, tk3);
	if (tkType3 == TK_L_SMALL) // 함수
	{
		SFunctionInfo* save = funs._cur;
//		funs._cur->Clear();

		//funs._cur->_name = fname;
		//funs._cur->_funType = funType;
		if (false == ParseFunction(ar, funs, vars, fname, funType))
			return -1;

		int r = funs._cur->_funID;
		funs._cur = save;
		return r;
	}
	else
	{
		SetParserCompileError(ar, PCE_EXPECTED_TOKEN, "'(' after function name", tk3.c_str());
	}
	return -1;
}
bool Parse(CArchiveRdWC& ar, CNArchive&arw, bool putASM)
{
	if(g_bInitVM == false)
	{
		SetCompileError(ar, g_sParserCompileErrors[PCE_VM_NOT_INITIALIZED]);
		return false;
	}
//	ar.m_sTokenQueue.clear();

	SVars	vars;
	SFunctions funs;

	funs._curModule = funs.NewLayer();

	funs._cur = funs.NewFun(GLOBAL_INIT_FUN_NAME, &funs._codeTemp, ar._debug ? &funs.m_sDebugTemp : nullptr);
/*	funs._cur->_funID = 0;
	funs._cur->_name = GLOBAL_INIT_FUN_NAME;
	funs._cur->_code = &funs._codeTemp;
	if (ar._debug)
		funs._cur->_pDebugData = &funs.m_sDebugTemp;
	else
		funs._cur->_pDebugData = NULL;
	funs._cur->Clear();*/

	SLayerVar* pCurLayer = AddVarsFunction(vars);

//	funs.AddStaticString("system");

	// 호스트가 넘긴 네이티브 전역 심볼을 본문 파싱 전에 사전 선언한다.
	// (기존 preCompileHeader "export var X;" 텍스트 주입을 구조화 테이블로 대체)
	if (ar.m_pGlobalSymbols && ar.m_pGlobalSymbols->symbols)
	{
		for (int i = 0; i < ar.m_pGlobalSymbols->count; i++)
		{
			const NeoGlobalSymbol& sym = ar.m_pGlobalSymbols->symbols[i];
			if (sym.name == nullptr || sym.name[0] == 0)
				continue;
			if (-1 == AddLocalVarName(ar, funs, vars, sym.exported, sym.name, true))
				return false; // 중복/예약어 등은 AddLocalVarName 이 에러 세팅
		}
	}

	bool r = ParseFunctionBody(ar, funs, vars);
	if (true == r)
	{
		funs._cur->_iCode_Size = funs._cur->_code->GetBufferOffset();

		FinalizeFuction(funs);
//		SFunctionInfo* pF = funs.FindFun(funs.GetCurFunName());
//		*pF = funs._cur;

		if(false == Write(ar, arw, funs, vars))
			return false;
		if (putASM)
		{
			int off = arw.GetBufferOffset();
			WriteLog(ar, arw, funs, vars);
			arw.SetBufferOffset(off);
		}
	}

	DelVarsFunction(vars);

	while (vars._varsFunction.empty() == false)
		DelVarsFunction(vars);

	return r;
}

bool INeoVM::Compile(CNArchive& arw, const NeoCompilerParam& param)
{
	//CNeoVMImpl::InitLib();

	CArchiveRdWC ar2;
	ar2._allowGlobalInitLogic = param.allowGlobalInitLogic;
	ar2._debug = param.debug;
	ar2.m_pDefines = param.defines;
	ar2.m_pGlobalSymbols = param.globalSymbols;
	if (param.debugSourceFiles != nullptr)
	{
		param.debugSourceFiles->clear();
		param.debugSourceFiles->push_back(param.debugSourcePath != nullptr ? param.debugSourcePath : "");
		ar2.m_pDebugSourceFiles = param.debugSourceFiles;
	}

	ToArchiveRdWC((const char*)param.pBufferSrc, param.iLenSrc, ar2); // memory alloc


	bool b = Parse(ar2, arw, param.putASM);
	if(b == false && param.err)
		*(param.err) = ar2.m_sErrorString;

	u16* pBuffer = ar2.GetBuffer();
	if (pBuffer) delete[] pBuffer;

	return b;
}
bool INeoVM::Initialize(INeoLoader* loader)
{
	InitDefaultTokenString();
	CNeoVMImpl::InitLib();
	g_bInitVM = true;
	g_NeoLoader = loader;
	return true;
}
bool	INeoVM::Shutdown()
{
	g_bInitVM = false;
	g_NeoLoader = nullptr;
	return true;
}

INeoVM* INeoVM::CompileAndLoadVM(const NeoCompilerParam& param, const NeoLoadVMParam* vparam)
{
	if (vparam == nullptr || vparam->execPool == nullptr)
	{
		if (param.err != nullptr)
			*param.err = "NeoExecContextPool is required.\n";
#ifdef _WIN32
		if (param.putASM && param.err != nullptr)
			printf((ANSI_COLOR_RED + *(param.err) + ANSI_RESET_ALL).c_str());
#endif
		return NULL;
	}

	CNArchive arCode;

	if (false == Compile(arCode, param))
	{
#ifdef _WIN32
		if(param.putASM && param.err != nullptr)
			printf((ANSI_COLOR_RED + *(param.err) + ANSI_RESET_ALL).c_str());
#endif
		return NULL;
	}

	//if(putASM)
	//	SetCompileError(ar, "Comile Success. Code : %d bytes !!\n\n", arCode.GetBufferOffset());

	INeoVM* pVM = INeoVM::CreateVM();
	// (실행 스택은 더 이상 워커가 소유하지 않으므로 iStackSize 로 크기를 주지 않는다.)
	if (pVM->LoadVM(vparam, arCode.GetData(), arCode.GetBufferOffset()) == NULL)
	{
		INeoVM::ReleaseVM(pVM);
		return NULL;
	}

	if(param.putASM)
		printf(ANSI_COLOR_GREEN "Compile Success. Code : %d bytes !!" ANSI_RESET_ALL "\n", pVM->GetBytesSize());

	return pVM;
}
INeoVM* INeoVM::CompileAndLoadRunVM(const NeoCompilerParam& param, const NeoLoadVMParam* vparam)
{
	auto pVM = CompileAndLoadVM(param, vparam);

	if(pVM == nullptr)
		return nullptr;

	pVM->PCall(pVM->GetMainWorkerID());

	return pVM;
}
};
