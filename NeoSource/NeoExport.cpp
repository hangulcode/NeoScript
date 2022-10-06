
#include <math.h>
#include "NeoParser.h"
#include "NeoTextLoader.h"
#include "NeoExport.h"

void	SetCompileError(CArchiveRdWC& ar, const char*	lpszString, ...);

u8 GetArgIndexToCode(short* n1, short* n2, short* n3)
{
	u8 r = 0;
	if (n1 == nullptr || *n1 >= 0)
		r |= (1 << 2);
	else
		*n1 = -*n1 - 1;

	if (n2 == nullptr || *n2 >= 0)
		r |= (1 << 1);
	else
		*n2 = -*n2 - 1;

	if (n3 == nullptr || *n3 >= 0)
		r |= (1 << 0);
	else
		*n3 = -*n3 - 1;

	return r;
}

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
	auto it = funs._funIDs.find(nID);
	if(it == funs._funIDs.end())
		return "Error";

	auto it2 = funs._funs.find((*it).second);
	return (*it2).second._name;
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

	int curFunStatkSize = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;

	funPos[fi._funID] = fun;

	if (fi._funType == FUNT_IMPORT)
		return;


	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	CNArchive arRead((u8*)fi._code->GetData() + fi._iCode_Begin, fi._iCode_Size);
	arRead.SetPointer(0, SEEK_SET);

	int iNewCodeBegin = ar.GetBufferOffset();

	SVMOperation v;
	memset(&v, 0, sizeof(v));

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		if (arText._debug)
		{
			int off = (arRead.GetBufferOffset() + fi._iCode_Begin) / 8;
			debug_info di = (*fi._pDebugData)[off];
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
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);

			argFlag = GetArgIndexToCode(&v.n1, &v.n2, nullptr);
			break;
		case NOP_ADD3:
		case NOP_SUB3:
		case NOP_MUL3:
		case NOP_DIV3:
		case NOP_PERSENT3:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			
			argFlag = GetArgIndexToCode(&v.n1, &v.n2, &v.n3);
			break;

		case NOP_VAR_CLEAR:
		case NOP_INC:
		case NOP_DEC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);

			argFlag = GetArgIndexToCode(&v.n1, nullptr, nullptr);
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

			argFlag = GetArgIndexToCode(&v.n1, &v.n2, &v.n3);
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

			argFlag = GetArgIndexToCode(nullptr, &v.n2, &v.n3);
			break;
		case NOP_JMP_FOREACH:	// 
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);

			argFlag = GetArgIndexToCode(nullptr, &v.n2, &v.n3);
			break;

		case NOP_TOSTRING:
		case NOP_TOINT:
		case NOP_TOFLOAT:
		case NOP_TOSIZE:
		case NOP_GETTYPE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);

			argFlag = GetArgIndexToCode(&v.n1, &v.n2, nullptr);
			break;
		case NOP_SLEEP:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			argFlag = GetArgIndexToCode(&v.n1, nullptr, nullptr);
			break;
		case NOP_JMP:
			//ar << optype << v.n1;
			argFlag = GetArgIndexToCode(nullptr, nullptr, nullptr);
			break;
		case NOP_JMP_FALSE:
		case NOP_JMP_TRUE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			argFlag = GetArgIndexToCode(&v.n1, nullptr, nullptr);
			break;

		case NOP_MOV:
		case NOP_MOV_MINUS:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			argFlag = GetArgIndexToCode(&v.n1, &v.n2, nullptr);
			break;

		case NOP_PTRCALL:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			argFlag = GetArgIndexToCode(&v.n1, &v.n2, nullptr);
			break;
		case NOP_CALL:
			//ar << optype << v.n1 << v.n2;
			argFlag = GetArgIndexToCode(nullptr, nullptr, nullptr);
			break;
		case NOP_RETURN:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			argFlag = GetArgIndexToCode(&v.n1, nullptr, nullptr);
			break;
		case NOP_FUNEND:
			//ar << optype;
			argFlag = GetArgIndexToCode(nullptr, nullptr, nullptr);
			break;
		case NOP_TABLE_ALLOC:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ar << optype << v.n1;
			argFlag = GetArgIndexToCode(&v.n1, nullptr, nullptr);
			break;
		case NOP_TABLE_INSERT:
		case NOP_TABLE_READ:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);

			argFlag = GetArgIndexToCode(&v.n1, &v.n2, &v.n3);
			break;
		case NOP_TABLE_REMOVE:
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ar << optype << v.n1 << v.n2;
			argFlag = GetArgIndexToCode(&v.n1, &v.n2, nullptr);
			break;
		default:
			SetCompileError(arText, "Error OP Type Error (%d)", v.op);
			argFlag = GetArgIndexToCode(nullptr, nullptr, nullptr);
			break;
		}

		ar << optype << argFlag << v.n1 << v.n2 << v.n3;
	}
	int iNewCodeEnd = ar.GetBufferOffset();
	fi._iCode_Begin = iNewCodeBegin;
	fi._iCode_Size = iNewCodeEnd - iNewCodeBegin;
}

std::string GetLog(SVMOperation& op, int argIndex)
{
	char ch[64] = {0,};
	int v = 0;
	char c = 'N';
	if (argIndex == 1) { v = op.n1; c = (op.argFlag & 0x04) ? 'S' : 'G'; }
	if (argIndex == 2) { v = op.n2; c = (op.argFlag & 0x02) ? 'S' : 'G'; }
	if (argIndex == 3) { v = op.n3; c = (op.argFlag & 0x01) ? 'S' : 'G'; }

	sprintf_s(ch, _countof(ch), "%c.%d", c, v);
	return ch;
}


void WriteFunLog(CArchiveRdWC& arText, CNArchive& arw, SFunctions& funs, SFunctionInfo& fi, SVars& vars)
{
	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	int curFunStatkSize = 1 + (int)fi._args.size() + localCount + fi._localTempMax;

	OutAsm("\n");
	OutAsm("Fun - %s [T:%d ID:%d] arg:%d var:%d varTemp:%d\n", fi._name.c_str(), fi._funType, fi._funID, (int)fi._args.size(), fi._localVarCount, fi._localTempMax);

	if (fi._funType == FUNT_IMPORT)
		return;

	//CNArchive arRead((u8*)fi._code->GetData() + fi._iCode_Begin, fi._iCode_Size);
	CNArchive arRead((u8*)arw.GetData() + fi._iCode_Begin, fi._iCode_Size);
	arRead.SetPointer(0, SEEK_SET);

	SVMOperation v;

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		//if(arText._debug)
		//	arRead >> v.dbg;

		//OpType optype;
		//arRead >> optype;

		//u8 argFlag;
		//arRead >> argFlag;

		//v.op = CODE_TO_NOP(optype);
		//int len = CODE_TO_LEN(optype);
		//if (len)
		//{
		//	arRead.Read(&v.n1, len * sizeof(short));
		//}

		arRead >> v;

		//if (arText._debug)
		//	OutAsm("%6d : ", v.dbg._lineseq);

		switch (v.op)
		{
		case NOP_ADD2:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ADD [%s] += [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_SUB2:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("SUB [%s] -= [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_MUL2:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("MUL [%s] *= [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_DIV2:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("DIV [%s] /= [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_PERSENT2:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("PER [%s] %%= [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;

		case NOP_ADD3:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("ADD [%s] = [%s] + [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_SUB3:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("SUB [%s] = [%s] - [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_MUL3:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("MUL [%s] = [%s] * [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_DIV3:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("DIV [%s] = [%s] / [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_PERSENT3:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("PER [%s] = [%s] %% [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;

		case NOP_VAR_CLEAR:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("CLR [%s]\n", GetLog(v, 1).c_str());
			break;
		case NOP_INC:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("INC [%s]\n", GetLog(v, 1).c_str());
			break;
		case NOP_DEC:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("DEC [%s]\n", GetLog(v, 1).c_str());
			break;

		case NOP_GREAT:		// >
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("GR [%s] = [%s] > [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_GREAT_EQ:	// >=
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("GE [%s] = [%s] >= [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_LESS:		// <
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("LS [%s] = [%s] < [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_LESS_EQ:	// <=
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("LE [%s] = [%s] <= [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_EQUAL2:	// ==
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("EQ [%s] = [%s] == [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_NEQUAL:	// !=
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("NE [%s] = [%s] != [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_AND:	// &&
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("AND [%s] = [%s] && [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_OR:	// ||
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("NE [%s] = [%s] || [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;

		case NOP_STR_ADD:	// ..
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("STR_ADD Str[%s] = ToStr[%s] + ToStr[%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;

		case NOP_JMP_GREAT:		// >
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JGR %d,  [%s] > [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_GREAT_EQ:	// >=
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JGE %d,  [%s] >= [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_LESS:		// <
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JLS %d,  [%s] < [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_LESS_EQ:	// <=
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JLE %d,  [%s] <= [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_EQUAL2:	// ==
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JEQ %d,  [%s] == [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_NEQUAL:	// !=
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JNE %d,  [%s] != [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_AND:	// &&
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JAND %d,  [%s] && [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_OR:		// ||
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JOR %d,  [%s] || [%s]\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_NAND:	// !(&&)
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JNAND %d,  !([%s] && [%s])\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_NOR:	// !(||)
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JNOR %d,  !([%s] || [%s])\n", v.n1, GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_JMP_FOREACH:	// foreach
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("JFRE %d,  T[%s], K[S.%d], V[S.%d]\n", v.n1, GetLog(v, 2).c_str(), v.n3, v.n3+1);
			break;

		case NOP_TOSTRING:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToString [%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_TOINT:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToInt [%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_TOFLOAT:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToFloat [%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_TOSIZE:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("ToSize [%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_GETTYPE:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("GetType [%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_SLEEP:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("Sleep [%s]\n", GetLog(v, 1).c_str());
			break;
		case NOP_JMP:
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("JMP  %d\n", v.n1);
			break;
		case NOP_JMP_FALSE:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("JMP is False [%s]  %d\n", GetLog(v, 1).c_str(), v.n2);
			break;
		case NOP_JMP_TRUE:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("JMP is True [%s]  %d\n", GetLog(v, 1).c_str(), v.n2);
			break;

		case NOP_MOV:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("MOV [%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_MOV_MINUS:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("MOVI [%s] = -[%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;

		case NOP_PTRCALL:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("Ptr CALL [%s].[%s] arg:%d\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), v.n3);
			break;
		case NOP_CALL:
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("CALL %s\n", GetFunctionName(funs, v.n1).c_str());
			break;
		case NOP_RETURN:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("RET [%s]\n", GetLog(v, 1).c_str());
			break;
		case NOP_FUNEND:
			OutAsm("- End -\n");
			break;
		case NOP_TABLE_ALLOC:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			OutBytes((const u8*)&v, 1 + 2 * 1, 8);
			OutAsm("Table Alloc [%s]\n", GetLog(v, 1).c_str());
			break;
		case NOP_TABLE_INSERT:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("Table Insert [%s].[%s] = [%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str(), GetLog(v, 3).c_str());
			break;
		case NOP_TABLE_READ:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n3);
			OutBytes((const u8*)&v, 1 + 2 * 3, 8);
			OutAsm("Table Read [%s] = [%s].[%s]\n", GetLog(v, 3).c_str(), GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
			break;
		case NOP_TABLE_REMOVE:
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n1);
			//ChangeIndex(staticCount, localCount, curFunStatkSize, v.n2);
			OutBytes((const u8*)&v, 1 + 2 * 2, 8);
			OutAsm("Table Remove [%s].[%s]\n", GetLog(v, 1).c_str(), GetLog(v, 2).c_str());
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

	std::vector< debug_info> debugInfo;

	// Main 함수 코드 저장
	header._iMainFunctionOffset = ar.GetBufferOffset();
	header._iGlobalVarCount = funs._cur._localVarCount;
	WriteFun(arText, ar, funs, funs._cur, vars, funPos, debugInfo);
	// Sub 함수 코드 저장
	for (auto it = funs._funIDs.begin(); it != funs._funIDs.end(); it++)
	{
		auto it2 = funs._funs.find((*it).second);
		SFunctionInfo& fi = (*it2).second;
		WriteFun(arText, ar, funs, fi, vars, funPos, debugInfo);
	}

	header._iCodeSize = ar.GetBufferOffset() - sizeof(SNeoVMHeader);
	header.m_iDebugCount = (int)debugInfo.size();

	// 함수 포인터 저장
	if ((int)funPos.size() != header._iFunctionCount)
	{
		SetCompileError(arText, "Function Count Miss");
		return false;
	}

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

	// Debug 정보 Save
	if (header.m_iDebugCount)
	{
		ar.Write(&debugInfo[0], sizeof(debug_info) * header.m_iDebugCount);
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

bool WriteLog(CArchiveRdWC& arText, CNArchive& arw, SFunctions& funs, SVars& vars)
{
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));

	header._iFunctionCount = (int)(funs._funs.size() + 1);
	header._iStaticVarCount = (int)funs._staticVars.size();

	std::map<int, std::string> mapFun;
	for (auto it = funs._funIDs.begin(); it != funs._funIDs.end(); it++)
	{
		auto it2 = funs._funs.find((*it).second);
		SFunctionInfo& fi = (*it2).second;
		mapFun[fi._funID] = fi._name;
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
	WriteFunLog(arText, arw, funs, funs._cur, vars);
	// Sub 함수 코드 저장
	for (auto it = funs._funIDs.begin(); it != funs._funIDs.end(); it++)
	{
		auto it2 = funs._funs.find((*it).second);
		SFunctionInfo& fi = (*it2).second;
		WriteFunLog(arText, arw, funs, fi, vars);
	}

	//// 함수 포인터 저장

	return true;
}