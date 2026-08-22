#include <stdio.h>
#include <math.h>
#include "NeoParser.h"
#include "NeoTextLoader.h"
#include "NeoExport.h"
#include "NeoVMProgram.h"   // DebugInstructionFormatContext / FormatDebugOperation

namespace NeoScript
{

void	SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...);

enum EExportCompileError
{
	ECE_INVALID_OPCODE,
	ECE_INVALID_YIELD_SUBTYPE,
	ECE_FUNCTION_COUNT_MISMATCH,
	ECE_INVALID_VARIABLE_TYPE,
	ECE_COUNT,
};

static const char* g_sExportCompileErrors[ECE_COUNT] =
{
	"Invalid opcode while writing bytecode (%d)",
	"Invalid yield subtype while disassembling bytecode (%d)",
	"Function count mismatch while writing bytecode",
	"Invalid variable type while writing bytecode (%d)",
};

u8 GetArgIndexToCode(u8 flag, short* n1, short* n2, short* n3)
{
/*	u8 r = 0;
	if ((flag & NEOS_ARG_N1_IMMEDIATE) == 0)
	{
		if (n1 == nullptr || *n1 >= 0)
			r |= NEOS_ARG_N1_LOCAL;
		else
			*n1 = -*n1 - 1;
	}

	if ((flag & NEOS_ARG_N2_IMMEDIATE) == 0)
	{
		if (n2 == nullptr || *n2 >= 0)
			r |= NEOS_ARG_N2_LOCAL;
		else
			*n2 = -*n2 - 1;
	}

	if ((flag & NEOS_ARG_N3_IMMEDIATE) == 0)
	{
		if (n3 == nullptr || *n3 >= 0)
			r |= NEOS_ARG_N3_LOCAL;
		else
			*n3 = -*n3 - 1;
	}
	return r;*/
	return 0;
}

u8 ChangeIndex(int staticCount, int localCount, int curFunStatkSize, SVMOperation& op, int argIndex)
{
	short* n = nullptr;
	if (argIndex == 1)		{ if (op.argFlag & NEOS_ARG_N1_IMMEDIATE) return 0; n = &op.n1; }
	else if (argIndex == 2) { if (op.argFlag & NEOS_ARG_N2_IMMEDIATE) return 0; n = &op.n2; }
	else if (argIndex == 3) { if (op.argFlag & NEOS_ARG_N3_IMMEDIATE) return 0; n = &op.n3; }

	if (*n == -1)
		return 0;

	if (*n == STACK_POS_RETURN)
	{
		*n = curFunStatkSize;
		if (argIndex == 1) return NEOS_ARG_N1_LOCAL;
		else if (argIndex == 2) return NEOS_ARG_N2_LOCAL;
		else if (argIndex == 3) return NEOS_ARG_N3_LOCAL;
	}
	if(*n <= COMPILE_GLOBAL_VAR_BEGIN)
	{
		*n = -(*n) + COMPILE_GLOBAL_VAR_BEGIN + staticCount;
		return 0;
	}

	if (*n >= COMPILE_LOCALTMP_VAR_BEGIN)
	{
		if (*n >= COMPILE_STATIC_VAR_BEGIN)
		{
			if (*n >= COMPILE_CALLARG_VAR_BEGIN)
			{
				*n = (*n - COMPILE_CALLARG_VAR_BEGIN) + curFunStatkSize;
				if (argIndex == 1) return NEOS_ARG_N1_LOCAL;
				else if (argIndex == 2) return NEOS_ARG_N2_LOCAL;
				else if (argIndex == 3) return NEOS_ARG_N3_LOCAL;
			}
			*n = (*n - COMPILE_STATIC_VAR_BEGIN);
			return 0;
		}
		*n = *n - COMPILE_LOCALTMP_VAR_BEGIN + localCount;
		if (argIndex == 1) return NEOS_ARG_N1_LOCAL;
		else if (argIndex == 2) return NEOS_ARG_N2_LOCAL;
		else if (argIndex == 3) return NEOS_ARG_N3_LOCAL;
	}
	if (argIndex == 1) return NEOS_ARG_N1_LOCAL;
	else if (argIndex == 2) return NEOS_ARG_N2_LOCAL;
	else if (argIndex == 3) return NEOS_ARG_N3_LOCAL;
	return 0;
}
struct STempDebug
{
	std::vector<VarInfo>				_staticVars;
	std::vector<std::string>			_globalVars;
	std::vector< debug_info>			debugInfo;
};

std::string GetFunctionName(SFunctions& funs, short nID)
{
	auto it = funs._funIDs.find(nID);
	if(it == funs._funIDs.end())
		return "Error";

	return (*it).second->GetFullName();
}

void WriteFun(CArchiveRdWC& arText, CNArchive& ar, SFunctions& funs, SFunctionInfo& fi, SVars& vars, std::map<int, SFunctionTableForWriter>& funPos, std::vector< debug_info>& debugInfo)
{
	int iOPCount = 0;

	SFunctionTableForWriter fun;
	fun._codePtr = ar.GetBufferOffset() - sizeof(SNeoVMHeader);
	fun._argsCount = (short)fi._args.size();
	fun._localTempMax = (short)fi._localTempMax;
	fun._localVarCount = fi._localVarCount;
	fun._funType = fi._funType;
	fun._name = fi._name;
	fun._captures.clear();
	fun._captures.reserve(fi._captures.size());
	for (const SClosureCapture& capture : fi._captures)
		fun._captures.push_back(std::make_pair(capture._sourceSlot, capture._localSlot));

	int curFunStatkSize = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;

	funPos[fi._funID] = fun;

	//if (fi._funType == FUNT_IMPORT)
	//	return;


	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	CNArchive arRead((u8*)funs._codeFinal.GetData() + fi._iCode_Begin, fi._iCode_Size);
	arRead.SetPointer(0, SEEK_SET);

	int iNewCodeBegin = ar.GetBufferOffset();

	SVMOperation v;
	memset(&v, 0, sizeof(v));

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		if (arText._debug)
		{
			int off = (arRead.GetBufferOffset() + fi._iCode_Begin) / 8;
			debug_info di = (funs.m_sDebugFinal)[off];
			debugInfo.push_back(di);
		}
		//	arRead >> v.dbg;

		OpType optype;
		arRead >> optype;

		u8 argFlag;
		arRead >> argFlag;



		v.op = CODE_TO_NOP(optype);
		int len = CODE_TO_LEN(optype);
		if (len)
		{
			arRead.Read(&v.n1, len * sizeof(short));
		}

		v.argFlag = argFlag;


		iOPCount++;

		//if (arText._debug)
		//	ar << v.dbg;

		switch (v.op)
		{
		case NOP_ADD2:
		case NOP_SUB2:
		case NOP_MUL2:
		case NOP_DIV2:
		case NOP_PERSENT2:
		case NOP_LSHIFT2:
		case NOP_RSHIFT2:
		case NOP_AND2:
		case NOP_OR2:
		case NOP_XOR2:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);

			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;

		case NOP_ADD3:
		case NOP_SUB3:
		case NOP_MUL3:
		case NOP_DIV3:
		case NOP_PERSENT3:
		case NOP_LSHIFT3:
		case NOP_RSHIFT3:
		case NOP_AND3:
		case NOP_OR3:
		case NOP_XOR3:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);
			
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, &v.n3);
			break;

		case NOP_VAR_CLEAR:
		case NOP_INC:
		case NOP_DEC:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);

			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;

		case NOP_GREAT:		// >
		case NOP_GREAT_EQ:	// >=
		case NOP_LESS:		// <
		case NOP_LESS_EQ:	// <=
		case NOP_EQUAL2:	// ==
		case NOP_NEQUAL:	// !=
		case NOP_AND:		// &
		case NOP_OR:		// |
		case NOP_LOG_AND:		// &&
		case NOP_LOG_OR:		// ||
		case NOP_STR_ADD: // ..
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);

			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, &v.n3);
			break;

		case NOP_JMP:
			//ar << optype << v.n1;
			argFlag |= GetArgIndexToCode(argFlag, nullptr, nullptr, nullptr);
			break;
		case NOP_JMP_FALSE:
		case NOP_JMP_TRUE:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= GetArgIndexToCode(argFlag, nullptr, &v.n2, nullptr);
			break;

		case NOP_JMP_GREAT:		// >
		case NOP_JMP_GREAT_EQ:	// >=
		case NOP_JMP_LESS:		// <
		case NOP_JMP_LESS_EQ:	// <=
		case NOP_JMP_EQUAL2:	// ==
		case NOP_JMP_NEQUAL:	// !=
		case NOP_JMP_AND:	// &&
		case NOP_JMP_OR:		// ||
		case NOP_JMP_NAND:	// !(&&)
		case NOP_JMP_NOR:	// !(||)
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);

			argFlag |= GetArgIndexToCode(argFlag, nullptr, &v.n2, &v.n3);
			break;
		case NOP_JMP_FOR:	// 
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);

			argFlag |= GetArgIndexToCode(argFlag, nullptr, &v.n2, &v.n3);
			break;
		case NOP_JMP_FOREACH:	//
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);

			argFlag |= GetArgIndexToCode(argFlag, nullptr, &v.n2, &v.n3);
			break;
		case NOP_SWITCH:	// n1 = switch table index (변환 금지), n2 = 조건식 위치
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= GetArgIndexToCode(argFlag, nullptr, &v.n2, nullptr);
			break;

		case NOP_TOSTRING:
		case NOP_TOINT:
		case NOP_TOFLOAT:
		case NOP_TOSIZE:
		case NOP_GETTYPE:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);

			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;
		case NOP_SLEEP:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= GetArgIndexToCode(argFlag, nullptr, &v.n2, nullptr);
			break;

		case NOP_MOV:
		case NOP_MOV_MINUS:
		case NOP_LOG_NOT:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			if ((argFlag & NEOS_ARG_N2_IMMEDIATE) == 0)
				argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;

		case NOP_MOVI:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;
		case NOP_MOVF:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;

		case NOP_FMOV1:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;
		case NOP_FMOV2:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			if ((argFlag & NEOS_ARG_N2_IMMEDIATE) == 0)
				argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;

		case NOP_PTRCALL:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;
		case NOP_PTRCALL2:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, &v.n3);
			break;
		case NOP_NATIVECALL: // 컴파일에 나오지 않고, LoadVM 에서 NOP_PTRCALL2 인경우 System 함수이름을 찾어서 함수 Index 화
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);
			argFlag |= GetArgIndexToCode(argFlag, nullptr, nullptr, &v.n3);
			break;
		case NOP_CALL:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);
			argFlag |= GetArgIndexToCode(argFlag, nullptr, nullptr, &v.n3);
			break;
		case NOP_RETURN:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			if((argFlag & NEOS_ARG_N1_LOCAL) && v.n1 == 0)
				argFlag |= NEOS_OP_CALL_NORESULT;
			break;
		//case NOP_FUNEND:
		//	//ar << optype;
		//	argFlag |= GetArgIndexToCode(argFlag, nullptr, nullptr, nullptr);
		//	break;
		case NOP_TABLE_ALLOC:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			//ar << optype << v.n1;
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;
		case NOP_CLT_MOV:
		case NOP_CLT_READ:
		case NOP_TABLE_ADD2:
		case NOP_TABLE_SUB2:
		case NOP_TABLE_MUL2:
		case NOP_TABLE_DIV2:
		case NOP_TABLE_PERSENT2:
			if ((argFlag & NEOS_ARG_N1_IMMEDIATE) == 0)
				argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			if ((argFlag & NEOS_ARG_N2_IMMEDIATE) == 0)
				argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			if ((argFlag & NEOS_ARG_N3_IMMEDIATE) == 0)
				argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);

			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, &v.n3);
			break;

		case NOP_TABLE_REMOVE:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			//ar << optype << v.n1 << v.n2;
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;

		case NOP_LIST_ALLOC:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			//ar << optype << v.n1;
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;
/*		case NOP_LIST_MOV:
		case NOP_LIST_READ:
		case NOP_LIST_ADD2:
		case NOP_LIST_SUB2:
		case NOP_LIST_MUL2:
		case NOP_LIST_DIV2:
		case NOP_LIST_PERSENT2:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 3);

			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, &v.n3);
			break;*/
		case NOP_LIST_REMOVE:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 2);
			//ar << optype << v.n1 << v.n2;
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, &v.n2, nullptr);
			break;
		case NOP_VERIFY_TYPE:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;
		case NOP_CHANGE_INT:
			argFlag |= ChangeIndex(staticCount, localCount, curFunStatkSize, v, 1);
			argFlag |= GetArgIndexToCode(argFlag, &v.n1, nullptr, nullptr);
			break;
		case NOP_YIELD:
			break;
		case NOP_ERROR:
			break;
		default:
			SetCompileError(arText, g_sExportCompileErrors[ECE_INVALID_OPCODE], v.op);
			argFlag |= GetArgIndexToCode(argFlag, nullptr, nullptr, nullptr);
			break;
		}

		ar << optype << argFlag << v.n1 << v.n2 << v.n3;
	}
	int iNewCodeEnd = ar.GetBufferOffset();
	fi._iCode_Begin = iNewCodeBegin;
	fi._iCode_Size = iNewCodeEnd - iNewCodeBegin;
}

static std::string ToPtringString(const std::string & str)
{
	std::string r;

	for (auto it = str.begin(); it != str.end(); it++)
	{
		char c = (*it);
		if (c == '\n')
			r += "\\n";
		else if (c == '\r')
			r += "\\r";
		else if (c == '\t')
			r += "\\t";
		else
			r += c;
	}
	return r;
}
#ifdef NS_SINGLE_PRECISION
#define FLOAT_FORMAT	"%f"
#else
#define FLOAT_FORMAT	"%lf"
#endif
std::string GetValueString(VarInfo& vi, std::map<int, std::string>* pFunTable = NULL)
{
	char ch[64] = { 0, };
	switch (vi.GetType())
	{
	case VAR_INT:
		snprintf(ch, _countof(ch), "%d", vi._int);
		break;
	case VAR_FLOAT:
		snprintf(ch, _countof(ch), FLOAT_FORMAT, vi._float); // double ?
		break;
	case VAR_BOOL:
		snprintf(ch, _countof(ch), "'%s'", vi._bl ? "true" : "false");
		break;
	case VAR_FUN:
		//snprintf(ch, _countof(ch), "%s", mapFun[vi._fun_index].c_str());
		if(pFunTable == NULL)
			snprintf(ch, _countof(ch), "%d:fun", vi._fun_index);
		else
		{
			auto it = pFunTable->find(vi._fun_index);
			if(it != pFunTable->end())
				snprintf(ch, _countof(ch), "%d:'%s'", vi._fun_index, (*it).second.c_str());
			else
				snprintf(ch, _countof(ch), "%d:fun", vi._fun_index);
		}
		break;
	case VAR_STRING:
		snprintf(ch, _countof(ch), "'%s'", ToPtringString(vi._str->_str).c_str());
		break;
	default:
		snprintf(ch, _countof(ch), "unknown type (%d)", vi.GetType());
		break;
	}
	return ch;
}

std::string GetLog(STempDebug& td, SVMOperation& op, int argIndex)
{
	char ch[64] = {0,};
	int v = 0;
	const char* c = ""; // N.
	bool isNum = false;

	if (argIndex == 1) { v = op.n1; isNum = (op.argFlag & NEOS_ARG_N1_IMMEDIATE); }
	if (argIndex == 2) { v = op.n2; isNum = (op.argFlag & NEOS_ARG_N2_IMMEDIATE); }
	if (argIndex == 3) { v = op.n3; isNum = (op.argFlag & NEOS_ARG_N3_IMMEDIATE); }

	if (false == isNum)
	{
		if (argIndex == 1) { c = (op.argFlag & NEOS_ARG_N1_LOCAL) ? "S." : "G."; }
		if (argIndex == 2) { c = (op.argFlag & NEOS_ARG_N2_LOCAL) ? "S." : "G."; }
		if (argIndex == 3) { c = (op.argFlag & NEOS_ARG_N3_LOCAL) ? "S." : "G."; }
	}

	if (c[0] == 'G')
	{
		if(v < (int)td._staticVars.size())
			snprintf(ch, _countof(ch), "[%s%d %s]", c, v, GetValueString(td._staticVars[v]).c_str());
		else
		{
			if(v - (int)td._staticVars.size() < (int)td._globalVars.size())
				snprintf(ch, _countof(ch), "[%s%d %s]", c, v, td._globalVars[v - (int)td._staticVars.size()].c_str());
			else
				snprintf(ch, _countof(ch), "[%s%d ???]", c, v);
		}
	}
	else
	{
		if(isNum)
			snprintf(ch, _countof(ch), "%d", v);
		else
			snprintf(ch, _countof(ch), "[%s%d]", c, v);
	}
	return ch;
}

std::string GetFunName(SFunctions& funs, int id)
{
	SFunctionInfo* pFun = funs.FindFun(id);
	if(pFun == NULL)
	{
		char ch[128];
		snprintf(ch, _countof(ch), "fun(%d)'", id);
		return ch;
	}
	return pFun->_name;
}

// opIndex: 점프 대상의 op 인덱스 (= 현재 op 인덱스 + 1 + n1).
std::string JumpMark(std::map<int, int>& sJumpMark, int opIndex)
{
	std::string r;
	auto it = sJumpMark.find(opIndex);
	if (it == sJumpMark.end())
		return r;
	char ch[128];
	snprintf(ch, _countof(ch), " 'go_%d'", (*it).second);
	r = ch;
	return r;
}


void WriteFunLog(CArchiveRdWC& arText, CNArchive& arw, SFunctions& funs, SFunctionInfo& fi, SVars& vars, STempDebug& td)
{
	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	int curFunStatkSize = 1 + (int)fi._args.size() + localCount + fi._localTempMax;

	OutAsm("\n");
	OutAsm(ANSI_COLOR_YELLOW "Fun - %s [T:%d ID:%d] arg:%d var:%d varTemp:%d" ANSI_RESET_ALL "\n", fi.GetFullName().c_str(), fi._funType, fi._funID, (int)fi._args.size(), fi._localVarCount, fi._localTempMax);

	//if (fi._funType == FUNT_IMPORT)
	//	return;

	//CNArchive arRead((u8*)fi._code->GetData() + fi._iCode_Begin, fi._iCode_Size);
	CNArchive arRead((u8*)arw.GetData() + fi._iCode_Begin, fi._iCode_Size);
	arRead.SetPointer(0, SEEK_SET);

	std::map<int, int> sJumpMark;
	SVMOperation v;
	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		int off = arRead.GetBufferOffset();
		arRead >> v;

		switch (v.op)
		{
		case NOP_JMP:
		case NOP_JMP_FALSE:
		case NOP_JMP_TRUE:
		case NOP_JMP_GREAT:		// >
		case NOP_JMP_GREAT_EQ:	// >=
		case NOP_JMP_LESS:		// <
		case NOP_JMP_LESS_EQ:	// <=
		case NOP_JMP_EQUAL2:	// ==
		case NOP_JMP_NEQUAL:	// !=
		case NOP_JMP_AND:	// &&
		case NOP_JMP_OR:		// ||
		case NOP_JMP_NAND:	// !(&&)
		case NOP_JMP_NOR:	// !(||)
		case NOP_JMP_FOR:	// for
		case NOP_JMP_FOREACH:	// foreach
			// op 인덱스로 기록 (다음 op = off/8 + 1, 거기에 op 단위 상대 offset n1)
			sJumpMark[off / (int)sizeof(SVMOperation) + 1 + v.n1] = 0;
			break;
		default:
			break;
		}
	}
	int idx = 0;
	for (auto it = sJumpMark.begin(); it != sJumpMark.end(); it++)
	{
		(*it).second = idx++;
	}

	arRead.SetBufferOffset(0);

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		int off = arRead.GetBufferOffset();
		auto it = sJumpMark.find(off / (int)sizeof(SVMOperation));
		if (it != sJumpMark.end())
		{
			OutAsm("\t\t\t\t\tgo_%d:\n", (*it).second);
		}

		if (arText._debug)
		{
			int off = fi._iCode_Begin - sizeof(SNeoVMHeader) + arRead.GetBufferOffset();
			OutAsm("%4d %6d : ", off/8, td.debugInfo[off/8]._lineseq);
		}

		arRead >> v;

		const int skipByteChars = 12;
		const int OpFlagByteChars = 2;

		// opcode 표기는 공용 포맷터 한 곳에만 있다(NeoVMProgram.cpp: FormatDebugOperation).
		// 여기서는 컴파일 타임 심볼 표를 resolver 로 공급하고 결과를 콘솔로 내보내기만 한다.
		DebugInstructionFormatContext context;
		context.operation = &v;
		context.opIndex = off / (int)sizeof(SVMOperation);
		context.operand = [&](int argIndex) { return GetLog(td, v, argIndex); };
		context.functionFull = [&](int id) { return GetFunctionName(funs, id); };
		context.functionShort = [&](int id) { return GetFunName(funs, id); };
		context.jumpLabel = [&](int target) { return JumpMark(sJumpMark, target); };

		std::string asmText;
		int byteCount = 0;
		if (false == FormatDebugOperation(context, asmText, &byteCount))
			SetCompileError(arText, g_sExportCompileErrors[ECE_INVALID_OPCODE], v.op);

		// NOP_ERROR 등 바이트를 안 찍던 op 는 byteCount 가 0 이라 빈 칸으로 나온다(기존과 동일).
		OutBytes((const u8*)&v, byteCount, skipByteChars);
		OutAsm("%s\n", asmText.c_str());
	}
//	OutAsm("Fun End [%s]", fi._name.c_str());
}

static void WriteString(CNArchive& ar, const std::string& str)
{
	short nLen = (short)str.length();
	ar << nLen;
	ar.Write((char*)str.data(), nLen);
}

// switch/case 테이블 청크 ('SWIT'). debug 여부와 무관하게 저장한다.
static void WriteSwitchTables(CNArchive& ar, SFunctions& funs)
{
	if (funs._switchTables.empty())
		return;

	const u32 magic = 0x54495753; // SWIT
	ar << magic;
	ar.WriteCount((u32)funs._switchTables.size());
	for (size_t t = 0; t < funs._switchTables.size(); ++t)
	{
		const SSwitchTableCompile& tbl = funs._switchTables[t];
		ar.Write(&tbl._defaultOffset, sizeof(tbl._defaultOffset));
		ar.WriteCount((u32)tbl._cases.size());
		for (size_t c = 0; c < tbl._cases.size(); ++c)
		{
			const SSwitchCaseCompile& e = tbl._cases[c];
			u8 keyType = (u8)e._type;
			ar.Write(&keyType, sizeof(keyType));
			switch (e._type)
			{
			case VAR_INT:    ar.Write((void*)&e._int, sizeof(e._int)); break;
			case VAR_BOOL:   ar.Write((void*)&e._bl, sizeof(e._bl)); break;
			case VAR_STRING: WriteString(ar, e._str); break;
			default: break;
			}
			ar.Write((void*)&e._jumpOffset, sizeof(e._jumpOffset));
		}
	}
}

static void WriteDebugVarNames(CNArchive& ar, SFunctions& funs, SVars& vars, int staticVarCount)
{
	const u32 magic = 0x4E445642; // NDVB
	ar << magic;
	u32 debugScopeCount = (u32)funs._funSequence.size();
	if (vars._varsFunction.size() == 1 && vars._varsFunction[0]->_varsLayer.size() == 1)
		++debugScopeCount;
	ar.WriteCount(debugScopeCount);
	if (vars._varsFunction.size() == 1 && vars._varsFunction[0]->_varsLayer.size() == 1)
	{
		SLocalVar* pLocalLayer = vars._varsFunction[0]->_varsLayer[0];
		ar << -1;
		ar.WriteCount((u32)pLocalLayer->_localVars.size());
		for (auto it = pLocalLayer->_localVars.begin(); it != pLocalLayer->_localVars.end(); ++it)
		{
			int idx = -it->second + COMPILE_GLOBAL_VAR_BEGIN + staticVarCount;
			ar << idx;
			WriteString(ar, it->first);
		}
	}
	for (auto it = funs._funSequence.begin(); it != funs._funSequence.end(); it++)
	{
		SFunctionInfo& fi = *(*it);
		ar << fi._funID;
		ar.WriteCount((u32)fi._debugVarNames.size());
		for (auto itName = fi._debugVarNames.begin(); itName != fi._debugVarNames.end(); ++itName)
		{
			ar << itName->first;
			WriteString(ar, itName->second);
		}
	}
}

static void WriteDebugFunctionNames(CNArchive& ar, SFunctions& funs)
{
	const u32 magic = 0x4E44464E; // NDFN
	ar << magic;
	ar.WriteCount((u32)funs._funSequence.size());
	for (auto it = funs._funSequence.begin(); it != funs._funSequence.end(); it++)
	{
		SFunctionInfo& fi = *(*it);
		ar << fi._funID;
		WriteString(ar, fi.GetFullName());
	}
}

bool Write(CArchiveRdWC& arText, CNArchive& ar, SFunctions& funs, SVars& vars)
{
	int iSaveOffset1 = ar.GetBufferOffset();
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));
	ar << header;

	header._dwFileType = FILE_NEOS;
	header._dwNeoVersion = NEO_VER;
	header._iFunctionCount = (int)(funs.GetFunCountAll());
	header._dwFlag = arText._debug ? NEO_HEADER_FLAG_DEBUG : 0;
	if (INeoVM::IsSinglePrecision())
		header._dwFlag |= NEO_HEADER_FLAG_SINGLE_PRECISION;
	
	std::map<int, SFunctionTableForWriter> funPos;

	std::vector< debug_info> debugInfo;

	// Main 함수 코드 저장
	header._iMainFunctionOffset = ar.GetBufferOffset();
	//header._iGlobalVarCount = funs._cur->_localVarCount;
	header._iGlobalVarCount = vars._globalVarCount;

	//WriteFun(arText, ar, funs, funs._cur, vars, funPos, debugInfo);
	// Sub 함수 코드 저장
	//for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	//{
	//	SFunctionInfo& fi = (*it).second;
	//	if (fi._funType == FUNT_IMPORT)
	//		WriteFun(arText, ar, funs, fi, vars, funPos, debugInfo);
	//}

	SVMOperation op;
	memset(&op, 0, sizeof(op));
	op.op =  NOP_ERROR;
	ar << op;
	op.op = NOP_IDLE;
	ar << op;

	for (auto it = funs._funSequence.begin(); it != funs._funSequence.end(); it++)
	{
		SFunctionInfo* fi = (*it);
		WriteFun(arText, ar, funs, *fi, vars, funPos, debugInfo);
	}



	header._iCodeSize = ar.GetBufferOffset() - sizeof(SNeoVMHeader);
	header.m_iDebugCount = (int)debugInfo.size();

	// 함수 포인터 저장
	if ((int)funPos.size() != header._iFunctionCount)
	{
		SetCompileError(arText, g_sExportCompileErrors[ECE_FUNCTION_COUNT_MISMATCH]);
		return false;
	}

	for (auto it = funPos.begin(); it != funPos.end(); it++)
	{
		int iID = (*it).first;
		SFunctionTableForWriter fun = (*it).second;
		ar << iID << fun._codePtr << fun._argsCount << fun._localTempMax << fun._localVarCount << fun._funType;
		const u16 captureCount = (u16)fun._captures.size();
		ar << captureCount;
		for (const auto& capture : fun._captures)
			ar << capture.first << capture.second;
		if (fun._funType != FUNT_NORMAL && fun._funType != FUNT_ANONYMOUS)
		{
			WriteString(ar, fun._name);
		}
	}

	header._iStaticVarCount = (int)funs._staticVars.size();

	if (vars._varsFunction.size() != 1) return false;
	SLayerVar* pLayerVar = vars._varsFunction[0];
	if (pLayerVar->_varsLayer.size() != 1) return false;
	SLocalVar* pLocalLayer = pLayerVar->_varsLayer[0];
	header._iExportVarCount = (int)vars._varsExport.size();
	for(int i = 0; i < header._iExportVarCount; i++)
	{
		auto it = pLocalLayer->_localVars.find(vars._varsExport[i]);
		int idx = COMPILE_GLOBAL_VAR_BEGIN - (*it).second + header._iStaticVarCount;
		ar << idx;
		WriteString(ar, (*it).first);
	}


	// Static 변수 저장

	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = funs._staticVars[i];
		ar << vi.GetType();
		switch (vi.GetType())
		{
		case VAR_INT:
			ar << vi._int;
			break;
		case VAR_FLOAT:
			ar << vi._float; // double
			break;
		case VAR_BOOL:
			ar << vi._bl;
			break;
		case VAR_STRING:
			WriteString(ar, vi._str->_str);
			break;
		//case VAR_FUN:
		//	ar << vi._fun_index;
		//	break;
		default:
			SetCompileError(arText, g_sExportCompileErrors[ECE_INVALID_VARIABLE_TYPE], vi.GetType());
			return false;
		}
	}

	// Debug 정보 Save
	if (header.m_iDebugCount)
	{
		header.m_iDebugOffset = ar.GetBufferOffset();
		debug_info di(0, 0);
		ar.Write(&di, sizeof(debug_info)); // First Error OPeration Insert
		ar.Write(&di, sizeof(debug_info)); // Second Idle OPeration Insert

		ar.Write(&debugInfo[0], sizeof(debug_info) * header.m_iDebugCount);
		header.m_iDebugCount += 2;
		WriteDebugVarNames(ar, funs, vars, header._iStaticVarCount);
		WriteDebugFunctionNames(ar, funs);
	}

	WriteSwitchTables(ar, funs);

	int iSaveOffset2 = ar.GetBufferOffset();
	ar.SetPointer(iSaveOffset1, SEEK_SET);
	ar << header;
	ar.SetPointer(iSaveOffset2, SEEK_SET);

	return true;
}


bool WriteLog(CArchiveRdWC& arText, CNArchive& arw, SFunctions& funs, SVars& vars)
{
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));

	//header._iFunctionCount = (int)(funs._funs.size() + 1);
	//header._iStaticVarCount = (int)funs._staticVars.size();

	arw.SetBufferOffset(0);
	arw >> header;

	SVMOperation op;
	arw >> op;	// NOP_ERROR
	arw >> op;  // NOP_IDLE

	std::map<int, std::string> mapFun;
	for (auto it = funs._funIDs.begin(); it != funs._funIDs.end(); it++)
	{
		//auto it2 = funs._funs.find((*it));
		SFunctionInfo& fi = *(*it).second;
		mapFun[fi._funID] = fi.GetFullName();

		//OutAsm("Fun [%d] %s\n", fi._funID, fi._name.c_str());
	}

	for (auto it = mapFun.begin(); it != mapFun.end(); it++)
	{
		OutAsm("Fun [%d] %s\n", (*it).first, (*it).second.c_str());
	}

	STempDebug td;
	td._staticVars = funs._staticVars;
	// Static 변수
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = funs._staticVars[i];
		OutAsm("Static   [%d] %s\n", i, GetValueString(vi, &mapFun).c_str());
	}

	std::set<std::string> tempExportVars;
	for (int i = 0; i < header._iExportVarCount; i++)
	{
		tempExportVars.insert(vars._varsExport[i]);
	}

	// Global 변수
	if (vars._varsFunction.size() != 1) return false;
	SLayerVar* pLayerVar = vars._varsFunction[0];
	if (pLayerVar->_varsLayer.size() != 1) return false;
	SLocalVar* pLocalLayer = pLayerVar->_varsLayer[0];
	std::map<int, std::string> global;
	for (auto it2 = pLocalLayer->_localVars.begin(); it2 != pLocalLayer->_localVars.end(); it2++)
	{
		int idx = -(*it2).second + COMPILE_GLOBAL_VAR_BEGIN + header._iStaticVarCount;
		global[idx] = (*it2).first;
	}
	for (auto it2 = global.begin(); it2 != global.end(); it2++)
	{
		int idx = (*it2).first;
		if (tempExportVars.end() == tempExportVars.find((*it2).second))
			OutAsm("Global   [%d] %s\n", idx, (*it2).second.c_str());
		else
			OutAsm("Global E [%d] %s\n", idx, (*it2).second.c_str());
		td._globalVars.push_back((*it2).second);
	}
	if (header.m_iDebugCount > 0 && header.m_iDebugOffset > 0)
	{
		td.debugInfo.resize(header.m_iDebugCount);
		int off = arw.GetBufferOffset();
		arw.SetBufferOffset(header.m_iDebugOffset);
		arw.Read(&td.debugInfo[0], header.m_iDebugCount * sizeof(debug_info));
		arw.SetBufferOffset(off);
	}


	// Main 함수 코드 저장
	//WriteFunLog(arText, arw, funs, funs._cur, vars);
	// Sub 함수 코드 저장
	for (auto it = funs._funSequence.begin(); it != funs._funSequence.end(); it++)
	{
		SFunctionInfo& fi = *(*it);
		WriteFunLog(arText, arw, funs, fi, vars, td);
	}

	//// 함수 포인터 저장

	return true;
}

};
