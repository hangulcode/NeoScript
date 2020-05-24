
#include <math.h>
#include "NeoParser.h"
#include "NeoTextLoader.h"
#include "NeoExport.h"

void	SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...);

void ChangeIndex(int staticCount, int localCount, int curFunStatkSize, short& n)
{
	//if (n == COMPILE_VAR_NULL)
	//{
	//	SetCompileError(ar, "Change Index Error COMPILE_VAR_NULL");
	//	return;
	//}
	if (n == STACK_POS_RETURN)
	{
		n = curFunStatkSize;
		return;
	}

	if (n >= COMPILE_LOCALTMP_VAR_BEGIN)
	{
		if (n >= COMPILE_STATIC_VAR_BEGIN)
		{
			if (n >= COMPILE_GLOBAL_VAR_BEGIN)
			{
				if (n >= COMPILE_CALLARG_VAR_BEGIN)
				{
					n = (n - COMPILE_CALLARG_VAR_BEGIN) + curFunStatkSize;
					return;
				}
				n = -(n - COMPILE_GLOBAL_VAR_BEGIN) - 1 - staticCount;
				return;
			}
			n = -(n - COMPILE_STATIC_VAR_BEGIN) - 1;
			return;
		}
		n = n - COMPILE_LOCALTMP_VAR_BEGIN + localCount;
		return;
	}
	return;
}

std::string GetFunctionName(SFunctions& funs, short nID)
{
	for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	{
		SFunctionInfo& fi = (*it).second;
		if (fi._funID == nID)
			return fi._name;
	}
	return "Error";
}

void WriteFun(CArchiveRdWC& arText, CNArchive& ar, SFunctions& funs, SFunctionInfo& fi, SVars& vars, std::map<int, SFunctionTableForWriter>& funPos)
{
	int iOPCount = 0;

	SFunctionTableForWriter fun;
	fun._codePtr = ar.GetBufferOffset() - sizeof(SNeoVMHeader);
	fun._argsCount = (short)fi._args.size();
	fun._localTempMax = (short)fi._localTempMax;
	fun._localVarCount = fi._localVarCount;
	fun._funType = fi._funType;
	fun._name = fi._name;

	int curFunStatkSize = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;

	funPos[fi._funID] = fun;

	if (fi._funType == FUNT_IMPORT)
		return;


	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	CNArchive arRead((u8*)fi._code->GetData() + fi._iCode_Begin, fi._iCode_Size);
	arRead.SetPointer(0, SEEK_SET);

	SVMOperation v;

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		if (arText._debug)
			arRead >> v.dbg;

		OpType optype;
		arRead >> optype;

		v.op = CODE_TO_NOP(optype);
		int len = CODE_TO_LEN(optype);
		if (len)
		{
			arRead.Read(&v.n1, len * sizeof(short));
		}

		iOPCount++;

		if (arText._debug)
			ar << v.dbg;

		switch (v.op)
		{
		case NOP_ADD2:
		case NOP_SUB2:
		case NOP_MUL2:
		case NOP_DIV2:
		case NOP_PERSENT2:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ar << optype << v.n1 << v.n2;
			break;

		case NOP_ADD3:
		case NOP_SUB3:
		case NOP_MUL3:
		case NOP_DIV3:
		case NOP_PERSENT3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			ar << optype << v.n1 << v.n2 << v.n3;
			break;

		case NOP_VAR_CLEAR:
		case NOP_INC:
		case NOP_DEC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ar << optype << v.n1;
			break;

		case NOP_GREAT:		// >
		case NOP_GREAT_EQ:	// >=
		case NOP_LESS:		// <
		case NOP_LESS_EQ:	// <=
		case NOP_EQUAL2:	// ==
		case NOP_NEQUAL:	// !=
		case NOP_AND:	// &&
		case NOP_OR:	// ||
		case NOP_STR_ADD: // ..
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			ar << optype << v.n1 << v.n2 << v.n3;
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
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			ar << optype << v.n1 << v.n2 << v.n3;
			break;
		case NOP_JMP_FOREACH:	// 
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			ar << optype << v.n1 << v.n2 << v.n3;
			break;

		case NOP_TOSTRING:
		case NOP_TOINT:
		case NOP_TOFLOAT:
		case NOP_TOSIZE:
		case NOP_GETTYPE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ar << optype << v.n1 << v.n2;
			break;
		case NOP_SLEEP:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ar << optype << v.n1;
			break;
		case NOP_JMP:
			ar << optype << v.n1;
			break;
		case NOP_JMP_FALSE:
		case NOP_JMP_TRUE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ar << optype << v.n1 << v.n2;
			break;

		case NOP_MOV:
		case NOP_MOV_MINUS:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ar << optype << v.n1 << v.n2;
			break;

		case NOP_PTRCALL:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ar << optype << v.n1 << v.n2 << v.n3;
			break;
		case NOP_CALL:
			ar << optype << v.n1 << v.n2;
			break;
		case NOP_RETURN:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ar << optype << v.n1;
			break;
		case NOP_FUNEND:
			ar << optype;
			break;
		case NOP_TABLE_ALLOC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ar << optype << v.n1;
			break;
		case NOP_TABLE_INSERT:
		case NOP_TABLE_READ:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			ar << optype << v.n1 << v.n2 << v.n3;
			break;
		case NOP_TABLE_REMOVE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ar << optype << v.n1 << v.n2;
			break;
		default:
			SetCompileError(arText, "Error OP Type Error (%d)", v.op);
			break;
		}
	}
}


void WriteFunLog(CArchiveRdWC& arText, SFunctions& funs, SFunctionInfo& fi, SVars& vars)
{
	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	int curFunStatkSize = 1 + (int)fi._args.size() + localCount + fi._localTempMax;

	OutAsm("\n");
	OutAsm("Fun - %s [T:%d ID:%d] arg:%d var:%d varTemp:%d\n", fi._name.c_str(), fi._funType, fi._funID, (int)fi._args.size(), fi._localVarCount, fi._localTempMax);

	if (fi._funType == FUNT_IMPORT)
		return;

	CNArchive arRead((u8*)fi._code->GetData() + fi._iCode_Begin, fi._iCode_Size);
	arRead.SetPointer(0, SEEK_SET);

	SVMOperation v;

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		if(arText._debug)
			arRead >> v.dbg;

		OpType optype;
		arRead >> optype;

		v.op = CODE_TO_NOP(optype);
		int len = CODE_TO_LEN(optype);
		if (len)
		{
			arRead.Read(&v.n1, len * sizeof(short));
		}

		if (arText._debug)
			OutAsm("%6d : ", v.dbg._lineseq);

		switch (v.op)
		{
		case NOP_ADD2:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ADD [%d] += [%d]\n", v.n1, v.n2);
			break;
		case NOP_SUB2:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("SUB [%d] -= [%d]\n", v.n1, v.n2);
			break;
		case NOP_MUL2:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("MUL [%d] *= [%d]\n", v.n1, v.n2);
			break;
		case NOP_DIV2:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("DIV [%d] /= [%d]\n", v.n1, v.n2);
			break;
		case NOP_PERSENT2:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("PER [%d] %%= [%d]\n", v.n1, v.n2);
			break;

		case NOP_ADD3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("ADD [%d] = [%d] + [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_SUB3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("SUB [%d] = [%d] - [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_MUL3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("MUL [%d] = [%d] * [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_DIV3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("DIV [%d] = [%d] / [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_PERSENT3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("PER [%d] = [%d] %% [%d]\n", v.n1, v.n2, v.n3);
			break;

		case NOP_VAR_CLEAR:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("CLR [%d]\n", v.n1);
			break;
		case NOP_INC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("INC [%d]\n", v.n1);
			break;
		case NOP_DEC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("DEC [%d]\n", v.n1);
			break;

		case NOP_GREAT:		// >
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("GR [%d] = [%d] > [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_GREAT_EQ:	// >=
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("GE [%d] = [%d] >= [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_LESS:		// <
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("LS [%d] = [%d] < [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_LESS_EQ:	// <=
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("LE [%d] = [%d] <= [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_EQUAL2:	// ==
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("EQ [%d] = [%d] == [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_NEQUAL:	// !=
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("NE [%d] = [%d] != [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_AND:	// &&
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("AND [%d] = [%d] && [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_OR:	// ||
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("NE [%d] = [%d] || [%d]\n", v.n1, v.n2, v.n3);
			break;

		case NOP_STR_ADD:	// ..
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("STR_ADD Str[%d] = ToStr[%d] + ToStr[%d]\n", v.n1, v.n2, v.n3);
			break;

		case NOP_JMP_GREAT:		// >
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JGR %d,  [%d] > [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_GREAT_EQ:	// >=
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JGE %d,  [%d] >= [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_LESS:		// <
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JLS %d,  [%d] < [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_LESS_EQ:	// <=
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JLE %d,  [%d] <= [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_EQUAL2:	// ==
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JEQ %d,  [%d] == [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_NEQUAL:	// !=
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JNE %d,  [%d] != [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_AND:	// &&
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JAND %d,  [%d] && [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_OR:		// ||
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JOR %d,  [%d] || [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_NAND:	// !(&&)
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JNAND %d,  !([%d] && [%d])\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_NOR:	// !(||)
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JNOR %d,  !([%d] || [%d])\n", v.n1, v.n2, v.n3);
			break;
		case NOP_JMP_FOREACH:	// foreach
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JFRE %d,  T[%d], K[%d], V[%d]\n", v.n1, v.n2, v.n3, v.n3+1);
			break;

		case NOP_TOSTRING:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToString [%d] = [%d]\n", v.n1, v.n2);
			break;
		case NOP_TOINT:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToInt [%d] = [%d]\n", v.n1, v.n2);
			break;
		case NOP_TOFLOAT:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToFloat [%d] = [%d]\n", v.n1, v.n2);
			break;
		case NOP_TOSIZE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToSize [%d] = [%d]\n", v.n1, v.n2);
			break;
		case NOP_GETTYPE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("GetType [%d] = [%d]\n", v.n1, v.n2);
			break;
		case NOP_SLEEP:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("Sleep [%d]\n", v.n1);
			break;
		case NOP_JMP:
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("JMP  %d\n", v.n1);
			break;
		case NOP_JMP_FALSE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("JMP is False [%d]  %d\n", v.n1, v.n2);
			break;
		case NOP_JMP_TRUE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("JMP is True [%d]  %d\n", v.n1, v.n2);
			break;

		case NOP_MOV:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("MOV [%d] = [%d]\n", v.n1, v.n2);
			break;
		case NOP_MOV_MINUS:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("MOVI [%d] = -[%d]\n", v.n1, v.n2);
			break;

		case NOP_PTRCALL:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("Ptr CALL [%d].[%d] arg:%d\n", v.n1, v.n2, v.n3);
			break;
		case NOP_CALL:
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("CALL %s\n", GetFunctionName(funs, v.n1).c_str());
			break;
		case NOP_RETURN:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("RET [%d]\n", v.n1);
			break;
		case NOP_FUNEND:
			OutAsm("- End -\n");
			break;
		case NOP_TABLE_ALLOC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("Table Alloc [%d]\n", v.n1);
			break;
		case NOP_TABLE_INSERT:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("Table Insert [%d].[%d] = [%d]\n", v.n1, v.n2, v.n3);
			break;
		case NOP_TABLE_READ:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("Table Read [%d] = [%d].[%d]\n", v.n3, v.n1, v.n2);
			break;
		case NOP_TABLE_REMOVE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("Table Remove [%d].[%d]\n", v.n1, v.n2);
			break;
		default:
			SetCompileError(arText, "Error OP Type Error (%d)", v.op);
			break;
		}
	}
//	OutAsm("Fun End [%s]", fi._name.c_str());
}

static void WriteString(CNArchive& ar, const std::string& str)
{
	short nLen = (short)str.length();
	ar << nLen;
	ar.Write((char*)str.data(), nLen);
}

bool Write(CArchiveRdWC& arText, CNArchive& ar, SFunctions& funs, SVars& vars)
{
	int iSaveOffset1 = ar.GetBufferOffset();
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));
	ar << header;

	header._dwFileType = FILE_NEOS;
	header._dwNeoVersion = NEO_VER;
	header._iFunctionCount = (int)(funs._funs.size() + 1);
	header._dwFlag = arText._debug ? NEO_HEADER_FLAG_DEBUG : 0;
	
	std::map<int, SFunctionTableForWriter> funPos;

	// Main 함수 코드 저장
	header._iMainFunctionOffset = ar.GetBufferOffset();
	header._iGlobalVarCount = funs._cur._localVarCount;
	WriteFun(arText, ar, funs, funs._cur, vars, funPos);
	// Sub 함수 코드 저장
	for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	{
		SFunctionInfo& fi = (*it).second;
		WriteFun(arText, ar, funs, fi, vars, funPos);
	}

	header._iCodeSize = ar.GetBufferOffset() - sizeof(SNeoVMHeader);

	// 함수 포인터 저장
	if ((int)funPos.size() != header._iFunctionCount)
		return false;

	for (auto it = funPos.begin(); it != funPos.end(); it++)
	{
		int iID = (*it).first;
		SFunctionTableForWriter fun = (*it).second;
		ar << iID << fun._codePtr << fun._argsCount << fun._localTempMax << fun._localVarCount << fun._funType;
		if (fun._funType != FUNT_NORMAL)
		{
			WriteString(ar, fun._name);
		}
	}

	// Static 변수 저장
	header._iStaticVarCount = (int)funs._staticVars.size();

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
		case VAR_FUN:
			ar << vi._fun_index;
			break;
		default:
			SetCompileError(arText, "Error VAR Type Error (%d)", vi.GetType());
			return false;
		}
	}

	int iSaveOffset2 = ar.GetBufferOffset();
	ar.SetPointer(iSaveOffset1, SEEK_SET);
	ar << header;
	ar.SetPointer(iSaveOffset2, SEEK_SET);

	return true;
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

bool WriteLog(CArchiveRdWC& arText, SFunctions& funs, SVars& vars)
{
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));

	header._iFunctionCount = (int)(funs._funs.size() + 1);
	header._iStaticVarCount = (int)funs._staticVars.size();

	std::map<int, std::string> mapFun;
	for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	{
		SFunctionInfo& f = (*it).second;
		mapFun[f._funID] = f._name;
	}


	// Static 변수 저장
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = funs._staticVars[i];
		switch (vi.GetType())
		{
		case VAR_INT:
			OutAsm("Static [%d] %d\n", i, vi._int);
			break;
		case VAR_FLOAT:
			OutAsm("Static [%d] %lf\n", i, vi._float); // double
			break;
		case VAR_BOOL:
			OutAsm("Static [%d] %s\n", i, vi._bl ? "true" : "false");
			break;
		case VAR_STRING:
			OutAsm("Static [%d] '%s'\n", i, ToPtringString(vi._str->_str).c_str());
			break;
		case VAR_FUN:
			OutAsm("Static [%d] Fun %d = '%s'\n", i, vi._fun_index, mapFun[vi._fun_index].c_str());
			break;
		default:
			SetCompileError(arText, "Error VAR Type Error (%d)", vi.GetType());
			break;
		}
	}
	// Global 변수
	for (auto it = vars._varsFunction.begin(); it != vars._varsFunction.end(); it++)
	{
		SLayerVar* pLayerVar = (*it);
		for (auto it1 = pLayerVar->_varsLayer.begin(); it1 != pLayerVar->_varsLayer.end(); it1++)
		{
			SLocalVar* pLocalLayer = (*it1);
			for (auto it2 = pLocalLayer->_localVars.begin(); it2 != pLocalLayer->_localVars.end(); it2++)
			{
				OutAsm("Global [%d] %s\n", (*it2).second - COMPILE_GLOBAL_VAR_BEGIN + header._iStaticVarCount, (*it2).first.c_str());
			}
		}
	}

	// Main 함수 코드 저장
	WriteFunLog(arText, funs, funs._cur, vars);
	// Sub 함수 코드 저장
	for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	{
		SFunctionInfo& fi = (*it).second;
		WriteFunLog(arText, funs, fi, vars);
	}

	//// 함수 포인터 저장

	return true;
}