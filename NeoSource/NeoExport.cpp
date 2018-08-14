
#include <math.h>
#include "NeoParser.h"
#include "NeoTextLoader.h"
#include "NeoExport.h"

void	DebugLog(LPCSTR	lpszString, ...);

void ChangeIndex(int staticCount, int localCount, int curFunStatkSize, short& n)
{
	if (n == STACK_POS_RETURN)
	{
		n = curFunStatkSize;
		return;
	}

	if (n >= COMPILE_LOCALTMP_VAR_BEGIN)
	{
		if (n >= COMPILE_STATIC_VAR_BEGIN)
		{
			if (n >= COMPILE_GLObAL_VAR_BEGIN)
			{
				if (n >= COMPILE_CALLARG_VAR_BEGIN)
				{
					n = n - COMPILE_CALLARG_VAR_BEGIN + curFunStatkSize;
					return;
				}
				n = -(n - COMPILE_GLObAL_VAR_BEGIN) - 1 - staticCount;
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

void WriteFun(CNArchive& ar, SFunctions& funs, SFunctionInfo& fi, SVars& vars, std::map<int, SFunctionTableForWriter>& funPos)
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

	CNArchive arRead(fi._code.GetData(), fi._code.GetBufferSize());
	arRead.SetPointer(0, SEEK_SET);

	short n1, n2, n3;

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		NOP_TYPE op;
		arRead >> op;
		iOPCount++;

		switch (op)
		{
		case NOP_ADD2:
		case NOP_SUB2:
		case NOP_MUL2:
		case NOP_DIV2:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ar << op << n1 << n2;
			break;

		case NOP_ADD3:
		case NOP_SUB3:
		case NOP_MUL3:
		case NOP_DIV3:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			ar << op << n1 << n2 << n3;
			break;

		case NOP_INC:
		case NOP_DEC:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ar << op << n1;
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
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			ar << op << n1 << n2 << n3;
			break;

		case NOP_TOSTRING:
		case NOP_TOINT:
		case NOP_TOFLOAT:
		case NOP_GETTYPE:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ar << op << n1 << n2;
			break;
		case NOP_JMP:
			arRead >> n1;
			ar << op << n1;
			break;
		case NOP_JMP_FALSE:
		case NOP_JMP_TRUE:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ar << op << n1 << n2;
			break;

		case NOP_MOV:
		case NOP_MOV_MINUS:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ar << op << n1 << n2;
			break;
		case NOP_FARCALL:
			arRead >> n1 >> n2;
			ar << op << n1 << n2;
			break;
		case NOP_PTRCALL:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ar << op << n1 << n2 << n3;
			break;
		case NOP_CALL:
			arRead >> n1 >> n2;
			ar << op << n1 << n2;
			break;
		case NOP_RETURN:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ar << op << n1;
			break;
		case NOP_TABLE_ALLOC:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ar << op << n1;
			break;
		case NOP_TABLE_INSERT:
		case NOP_TABLE_READ:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			ar << op << n1 << n2 << n3;
			break;
		default:
			DebugLog("Error OP Type Error (%d)", op);
			break;
		}
	}
}


void WriteFunLog(SFunctions& funs, SFunctionInfo& fi, SVars& vars)
{
	int staticCount = (int)funs._staticVars.size();
	int localCount = fi._localVarCount;

	int curFunStatkSize = 1 + (int)fi._args.size() + localCount + fi._localTempMax;

	OutAsm("");
	OutAsm("Fun - %s [T:%d ID:%d] arg:%d var:%d varTemp:%d", fi._name.c_str(), fi._funType, fi._funID, (int)fi._args.size(), fi._localVarCount, fi._localTempMax);

	if (fi._funType == FUNT_IMPORT)
		return;

	CNArchive arRead(fi._code.GetData(), fi._code.GetBufferSize());
	arRead.SetPointer(0, SEEK_SET);

	short n1, n2, n3;

	while (arRead.GetBufferOffset() < arRead.GetBufferSize())
	{
		NOP_TYPE op;
		arRead >> op;

		switch (op)
		{
		case NOP_ADD2:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("ADD [%d] += [%d]", n1, n2);
			break;
		case NOP_SUB2:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("SUB [%d] -= [%d]", n1, n2);
			break;
		case NOP_MUL2:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("MUL [%d] *= [%d]", n1, n2);
			break;
		case NOP_DIV2:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("DIV [%d] /= [%d]", n1, n2);
			break;

		case NOP_ADD3:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("ADD [%d] = [%d] + [%d]", n1, n2, n3);
			break;
		case NOP_SUB3:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("SUB [%d] = [%d] - [%d]", n1, n2, n3);
			break;
		case NOP_MUL3:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("MUL [%d] = [%d] * [%d]", n1, n2, n3);
			break;
		case NOP_DIV3:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("DIV [%d] = [%d] / [%d]", n1, n2, n3);
			break;

		case NOP_INC:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			OutAsm("Inc [%d]", n1);
			break;
		case NOP_DEC:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			OutAsm("Dec [%d]", n1);
			break;

		case NOP_GREAT:		// >
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("GR [%d] = [%d] > [%d]", n1, n2, n3);
			break;
		case NOP_GREAT_EQ:	// >=
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("GE [%d] = [%d] >= [%d]", n1, n2, n3);
			break;
		case NOP_LESS:		// <
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("LS [%d] = [%d] < [%d]", n1, n2, n3);
			break;
		case NOP_LESS_EQ:	// <=
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("LE [%d] = [%d] <= [%d]", n1, n2, n3);
			break;
		case NOP_EQUAL2:	// ==
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("EQ [%d] = [%d] == [%d]", n1, n2, n3);
			break;
		case NOP_NEQUAL:	// !=
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("NE [%d] = [%d] != [%d]", n1, n2, n3);
			break;
		case NOP_AND:	// &&
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("AND [%d] = [%d] && [%d]", n1, n2, n3);
			break;
		case NOP_OR:	// ||
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("NE [%d] = [%d] || [%d]", n1, n2, n3);
			break;

		case NOP_STR_ADD:	// ..
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("String [%d] = ToStr[%d] + ToStr[%d]", n1, n2, n3);
			break;

		case NOP_TOSTRING:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("ToString [%d] = [%d]", n1, n2);
			break;
		case NOP_TOINT:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("ToInt [%d] = [%d]", n1, n2);
			break;
		case NOP_TOFLOAT:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("ToFloat [%d] = [%d]", n1, n2);
			break;
		case NOP_GETTYPE:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("GetType [%d] = [%d]", n1, n2);
			break;

		case NOP_JMP:
			arRead >> n1;
			OutAsm("JMP  %d", n1);
			break;
		case NOP_JMP_FALSE:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			OutAsm("JMP is False [%d]  %d", n1, n2);
			break;
		case NOP_JMP_TRUE:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			OutAsm("JMP is True [%d]  %d", n1, n2);
			break;

		case NOP_MOV:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("MOV [%d] = [%d]", n1, n2);
			break;
		case NOP_MOV_MINUS:
			arRead >> n1 >> n2;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("MOVI [%d] = -[%d]", n1, n2);
			break;
		case NOP_FARCALL:
			arRead >> n1 >> n2;
			OutAsm("Far CALL %s", GetFunctionName(funs, n1).c_str());
			break;
		case NOP_PTRCALL:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			OutAsm("Ptr CALL [%d].[%d] arg:%d", n1, n2, n3);
			break;
		case NOP_CALL:
			arRead >> n1 >> n2;
			OutAsm("CALL %s", GetFunctionName(funs, n1).c_str());
			break;
		case NOP_RETURN:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			OutAsm("RET [%d]", n1);
			break;
		case NOP_TABLE_ALLOC:
			arRead >> n1;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			OutAsm("Table Alloc [%d]", n1);
			break;
		case NOP_TABLE_INSERT:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("Table Insert [%d].[%d] = [%d]", n1, n2, n3);
			break;
		case NOP_TABLE_READ:
			arRead >> n1 >> n2 >> n3;
			ChangeIndex(staticCount, localCount, curFunStatkSize, n1);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n2);
			ChangeIndex(staticCount, localCount, curFunStatkSize, n3);
			OutAsm("Table Read [%d] = [%d].[%d]", n3, n1, n2);
			break;
		default:
			DebugLog("Error OP Type Error (%d)", op);
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

BOOL Write(CNArchive& ar, SFunctions& funs, SVars& vars)
{
	int iSaveOffset1 = ar.GetBufferOffset();
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));
	ar << header;

	header._dwFileType = FILE_NEOS;
	header._dwNeoVersion = NEO_VER;
	header._iFunctionCount = (int)(funs._funs.size() + 1);
	
	std::map<int, SFunctionTableForWriter> funPos;

	// Main 함수 코드 저장
	header._iMainFunctionOffset = ar.GetBufferOffset();
	WriteFun(ar, funs, funs._cur, vars, funPos);
	// Sub 함수 코드 저장
	for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	{
		SFunctionInfo& fi = (*it).second;
		WriteFun(ar, funs, fi, vars, funPos);
	}

	header._iCodeSize = ar.GetBufferOffset() - sizeof(SNeoVMHeader);

	// 함수 포인터 저장
	if ((int)funPos.size() != header._iFunctionCount)
		return FALSE;

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
		default:
			DebugLog("Error VAR Type Error (%d)", vi.GetType());
			break;
		}
	}

	int iSaveOffset2 = ar.GetBufferOffset();
	ar.SetPointer(iSaveOffset1, SEEK_SET);
	ar << header;
	ar.SetPointer(iSaveOffset2, SEEK_SET);

	return TRUE;
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

BOOL WriteLog(SFunctions& funs, SVars& vars)
{
	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));

	header._iFunctionCount = (int)(funs._funs.size() + 1);
	header._iStaticVarCount = (int)funs._staticVars.size();

	// Static 변수 저장
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = funs._staticVars[i];
		switch (vi.GetType())
		{
		case VAR_INT:
			OutAsm("Static [%d] i:%d", i, vi._int);
			break;
		case VAR_FLOAT:
			OutAsm("Static [%d] f:%lf", i, vi._float); // double
			break;
		case VAR_BOOL:
			OutAsm("Static [%d] b:%s", i, vi._bl ? "true" : "false");
			break;
		case VAR_STRING:
			OutAsm("Static [%d] s:%s", i, ToPtringString(vi._str->_str).c_str());
			break;
		default:
			DebugLog("Error VAR Type Error (%d)", vi.GetType());
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
				OutAsm("Global [%d] %s", (*it2).second - COMPILE_GLObAL_VAR_BEGIN + header._iStaticVarCount, (*it2).first.c_str());
			}
		}
	}

	// Main 함수 코드 저장
	WriteFunLog(funs, funs._cur, vars);
	// Sub 함수 코드 저장
	for (auto it = funs._funs.begin(); it != funs._funs.end(); it++)
	{
		SFunctionInfo& fi = (*it).second;
		WriteFunLog(funs, fi, vars);
	}

	//// 함수 포인터 저장

	return TRUE;
}