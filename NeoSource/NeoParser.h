#pragma once

#include "NeoArchive.h"
#include "NeoVM.h"
#include "NeoTextLoader.h"

#define INVALID_ERROR_PARSEJOB			(-1)

#define COMPILE_LOCALTMP_VAR_BEGIN		(10000)
#define COMPILE_STATIC_VAR_BEGIN		(15000)
#define COMPILE_GLOBAL_VAR_BEGIN		(20000)
#define COMPILE_CALLARG_VAR_BEGIN		(30000) // 256 개 이상 나오지 않는다.
//#define COMPILE_VAR_NULL				(32766)
#define STACK_POS_RETURN				(32767)

bool IsTempVar(int iVar);
eNOperation	GetOpTypeFromOp(eNOperation op);
eNOperation GetTableOpTypeFromOp(eNOperation op);
int		GetOpLength(eNOperation op);

struct SJumpValue
{
	int	_iCodePosOffset;
	int _iBaseJmpOffset;

	SJumpValue()
	{
		_iCodePosOffset = _iBaseJmpOffset = 0;
	}
	SJumpValue(int iCodePosOffset, int iBaseJmpOffset)
	{
		_iCodePosOffset = iCodePosOffset;
		_iBaseJmpOffset = iBaseJmpOffset;
	}
	void Set(int iCodePosOffset, int iBaseJmpOffset)
	{
		_iCodePosOffset = iCodePosOffset;
		_iBaseJmpOffset = iBaseJmpOffset;
	}
};


struct SLocalVar
{
	std::map<std::string, int> _localVars;

	int FindVar(const std::string& name)
	{
		auto it = _localVars.find(name);
		if (it == _localVars.end())
			return -1;
		return (*it).second;
	}
};

struct SLayerVar
{
	std::vector<SLocalVar*>	_varsLayer;

	~SLayerVar()
	{
		for (int i = (int)_varsLayer.size() - 1; i >= 0; i--)
			delete _varsLayer[i];
		_varsLayer.clear();
	}

	int FindVarOnlyCurrentBlock(const std::string& name)
	{
		int i = (int)_varsLayer.size() - 1;
		if(i >= 0)
		{
			int r = _varsLayer[i]->FindVar(name);
			if (r >= 0)
				return r;
		}
		return -1;
	}

	int FindVar(const std::string& name)
	{
		for (int i = (int)_varsLayer.size() - 1; i >= 0; i--)
		{
			int r = _varsLayer[i]->FindVar(name);
			if(r >= 0)
				return r;
		}
		return -1;
	}

	void	AddLocalVar(const std::string& name, int offset)
	{
		int sz = (int)_varsLayer.size();
		if (sz <= 0)
			return; // Error

		_varsLayer[sz - 1]->_localVars[name] = offset;
	}
};

struct SVars
{
	std::vector<SLayerVar*>	_varsFunction;
	~SVars()
	{
		for (int i = (int)_varsFunction.size() - 1; i >= 0; i--)
			delete _varsFunction[i];
		_varsFunction.clear();
	}

	int FindVar(const std::string& name)
	{
		for (int i = (int)_varsFunction.size() - 1; i >= 0; i--)
		{
			int r = _varsFunction[i]->FindVar(name);
			if (r >= 0)
				return r;
		}
		return -1;
	}
	SLayerVar* GetCurrentLayer()
	{
		return _varsFunction[_varsFunction.size() - 1];
	}
};

struct SFunctionTableForWriter
{
	FUNCTION_TYPE				_funType;
	std::string					_name;
	int							_codePtr;
	short						_argsCount;
	short						_localTempMax;
	int							_localVarCount;
};


struct SFunctionInfo
{
	int							_funID; // Function Index
	int							_staticIndex;
	std::string					_name;
	std::set<std::string>		_args;
	FUNCTION_TYPE				_funType = FUNT_NORMAL;


	int							_localVarCount;

	int							_localTempMax;
	int							_localTempCount;

	CNArchive*					_code = NULL;
	int							_iCode_Begin = 0;
	int							_iCode_Size = 0;
	std::vector<debug_info>*	_pDebugData = nullptr;

	void Clear()
	{
		_args.clear();
		_localVarCount = 0;
		_localTempMax = _localTempCount = 0;

		_iCode_Begin = 0;
		_iCode_Size = 0;
	}

	int	AllocLocalTempVar()
	{
		int r = COMPILE_LOCALTMP_VAR_BEGIN + 1 + (int)_args.size() + _localTempCount++;
		if (_localTempMax < _localTempCount)
			_localTempMax = _localTempCount;
		return r;
	}
	void	FreeLocalTempVar()
	{
		_localTempCount = 0;
	}


	int _iLastOPOffset = -1;
	void ClearLastOP()
	{
		_iLastOPOffset = -1;
	}
	eNOperation GetLastOP()
	{
		if (_iLastOPOffset < 0)
			return NOP_NONE;

		return CODE_TO_NOP(*(OpType*)((u8*)_code->GetData() + _iLastOPOffset));
	}
	eNOperation GetOP(int iOffsetOP)
	{
		return CODE_TO_NOP(*(OpType*)((u8*)_code->GetData() + iOffsetOP));
	}
	s16 GetN(int iOffsetOP, int n)
	{
		s16* pN = (s16*)((u8*)_code->GetData() + iOffsetOP + sizeof(OpType) + sizeof(ArgFlag));
		return pN[n];
	}
	void    Push_Arg(short a1, short a2, short a3)
	{
		ArgFlag arg = 0;//
		_code->Write(&arg, sizeof(arg));
		_code->Write(&a1, sizeof(a1));
		_code->Write(&a2, sizeof(a2));
		_code->Write(&a3, sizeof(a3));
	}
	void	AddDebugData(CArchiveRdWC& ar)
	{
		if (ar._debug == false) return;
		int iOff = _code->GetBufferOffset() / 8;
		if (_pDebugData->size() < iOff + 1)
			_pDebugData->resize(iOff + 1);

		(*_pDebugData)[iOff] = debug_info(ar.CurFile(), ar.CurLine());
	}
	void	Push_OP(CArchiveRdWC& ar, eNOperation op, short r, short a1, short a2)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(op);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&r, sizeof(r));
		//_code->Write(&a1, sizeof(a1));
		//_code->Write(&a2, sizeof(a2));
		Push_Arg(r, a1, a2);
	}
	void	Push_Call(CArchiveRdWC& ar, eNOperation op, short fun, short args)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(op);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&fun, sizeof(fun));
		//_code->Write(&args, sizeof(args));
		Push_Arg(fun, args, 0);
	}
	void	Push_CallPtr(CArchiveRdWC& ar, short table, short index, short args)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_PTRCALL);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&table, sizeof(table));
		//_code->Write(&index, sizeof(index));
		//_code->Write(&args, sizeof(args));
		Push_Arg(table, index, args);
	}
	void	Push_CallPtr2(CArchiveRdWC& ar, short table, short index, short args)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_PTRCALL2);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&table, sizeof(table));
		//_code->Write(&index, sizeof(index));
		//_code->Write(&args, sizeof(args));
		Push_Arg(table, index, args);
	}
	void	Push_MOV(CArchiveRdWC& ar, eNOperation op, short r, short s)
	{
		if (op == NOP_MOV)
		{
			if (IsTempVar(s))
			{
				eNOperation preOP = GetLastOP();
				switch (preOP)
				{
				case NOP_MOV:
				case NOP_ADD3:
				case NOP_SUB3:
				case NOP_MUL3:
				case NOP_DIV3:
				case NOP_TOSTRING:
				case NOP_TOINT:
				case NOP_TOFLOAT:
				case NOP_TOSIZE:
				case NOP_GETTYPE:

				case NOP_TABLE_ALLOC:
				case NOP_STR_ADD:

				case NOP_VAR_CLEAR:

				case NOP_GREAT:		// >
				case NOP_GREAT_EQ:	// >=
				case NOP_LESS:		// <
				case NOP_LESS_EQ:	// <=
				case NOP_EQUAL2:	// ==
				case NOP_NEQUAL:	// !=
				case NOP_AND:	// &&
				case NOP_OR:		// ||
				{
					u8 *pre = (u8*)_code->GetData() + sizeof(OpType) + sizeof(ArgFlag) + _iLastOPOffset;
					short* preDest = (short*)pre;
					if (*preDest == s)
					{
						*preDest = r;
						return;
					}
					break;
				}
				default:
					break;
				}
			}
		}

		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(op);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&r, sizeof(r));
		//_code->Write(&s, sizeof(s));
		Push_Arg(r, s, 0);
	}
	void	Push_OP1(CArchiveRdWC& ar, eNOperation op, short r)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(op);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&r, sizeof(r));
		Push_Arg(r, 0, 0);
	}
	void	Push_RETURN(CArchiveRdWC& ar, short r)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_RETURN);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&r, sizeof(r));
		Push_Arg(r, 0, 0);
	}
	void	Push_FUNEND(CArchiveRdWC& ar)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_FUNEND);
		_code->Write(&optype, sizeof(optype));
		Push_Arg(0, 0, 0);
	}


	void	Push_JMP(CArchiveRdWC& ar, int destOffset)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		eNOperation op = NOP_JMP;
		OpType optype = GetOpTypeFromOp(op);
		short add = destOffset - (_code->GetBufferOffset() + GetOpLength(op));
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&add, sizeof(add));
		Push_Arg(add, 0, 0);
	}
	void	Push_JMPFalse(CArchiveRdWC& ar, short var, int destOffset)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		eNOperation op = NOP_JMP_FALSE;
		OpType optype = GetOpTypeFromOp(op);
		short add = destOffset - (_code->GetBufferOffset() + GetOpLength(op));
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&var, sizeof(var));
		//_code->Write(&add, sizeof(add));
		Push_Arg(add, var, 0);
	}
	void	Push_JMPTrue(CArchiveRdWC& ar, short var, int destOffset)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		eNOperation op = NOP_JMP_TRUE;
		OpType optype = GetOpTypeFromOp(op);
		short add = destOffset - (_code->GetBufferOffset() + GetOpLength(op));
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&var, sizeof(var));
		//_code->Write(&add, sizeof(add));
		Push_Arg(add, var, 0);
	}
	void	Push_JMPFor(CArchiveRdWC& ar, int destOffset, short table, short key)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		eNOperation op = NOP_JMP_FOR;
		OpType optype = GetOpTypeFromOp(op);
		short add = destOffset - (_code->GetBufferOffset() + GetOpLength(op));
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&add, sizeof(add));
		//_code->Write(&table, sizeof(table));
		//_code->Write(&key, sizeof(key));
		Push_Arg(add, table, key);
	}
	// Always Value is Key Next Alloc ID
	void	Push_JMPForEach(CArchiveRdWC& ar, int destOffset, short table, short key)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		eNOperation op = NOP_JMP_FOREACH;
		OpType optype = GetOpTypeFromOp(op);
		short add = destOffset - (_code->GetBufferOffset() + GetOpLength(op));
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&add, sizeof(add));
		//_code->Write(&table, sizeof(table));
		//_code->Write(&key, sizeof(key));
		Push_Arg(add, table, key);
	}
	void	Set_JumpOffet(SJumpValue sJmp, int destOffset)
	{
		*((short*)((u8*)_code->GetData() + sJmp._iCodePosOffset)) = (short)(destOffset - sJmp._iBaseJmpOffset);
	}
	void	Push_TableAlloc(CArchiveRdWC& ar, short r)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_TABLE_ALLOC);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&r, sizeof(r));
		Push_Arg(r, 0, 0);
	}
	void	Push_Table_MASMDP(CArchiveRdWC& ar, eNOperation op, short nTable, short nArray, short nValue)
	{
		if (IsTempVar(nValue))
		{
			eNOperation preOP = GetLastOP();
			u8 *pre = (u8*)_code->GetData() + sizeof(OpType) + _iLastOPOffset;
			short* preDest = (short*)pre;
			switch (preOP)
			{
			case NOP_VAR_CLEAR:
				if (*preDest == nValue)
				{
					//_code->SetPointer(_iLastOPOffset - (ar._debug ? sizeof(debug_info) : 0), SEEK_SET);
					_code->SetPointer(_iLastOPOffset, SEEK_SET);
					Push_TableRemove(ar, nTable, nArray);
					return;
				}
				break;
			default:
				break;
			}
		}
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		eNOperation optype = GetTableOpTypeFromOp(op);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&nTable, sizeof(nTable));
		//_code->Write(&nArray, sizeof(nArray));
		//_code->Write(&nValue, sizeof(nValue));
		Push_Arg(nTable, nArray, nValue);
	}
	void	Push_TableRead(CArchiveRdWC& ar, short nTable, short nArray, short nValue)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_TABLE_READ);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&nTable, sizeof(nTable));
		//_code->Write(&nArray, sizeof(nArray));
		//_code->Write(&nValue, sizeof(nValue));
		Push_Arg(nTable, nArray, nValue);
	}
	void	Push_TableRemove(CArchiveRdWC& ar, short nTable, short nArray)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(NOP_TABLE_REMOVE);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&nTable, sizeof(nTable));
		//_code->Write(&nArray, sizeof(nArray));
		Push_Arg(nTable, nArray, 0);
	}
	void	Push_ToType(CArchiveRdWC& ar, eNOperation op, short r, short s)
	{
		AddDebugData(ar);
		_iLastOPOffset = _code->GetBufferOffset();

		OpType optype = GetOpTypeFromOp(op);
		_code->Write(&optype, sizeof(optype));
		//_code->Write(&r, sizeof(r));
		//_code->Write(&s, sizeof(s));
		Push_Arg(r, s, 0);
	}
};




struct SFunctions
{
	std::map<std::string, SFunctionInfo>_funs;
	std::map<int, std::string>			_funIDs;
	std::list<std::string>				_funSequence;

	SFunctionInfo						_cur;
	std::vector<VarInfo>				_staticVars;

	CNArchive		_codeFinal;
	CNArchive		_codeTemp;

	std::vector<debug_info> m_sDebugFinal;
	std::vector<debug_info> m_sDebugTemp;

	~SFunctions()
	{
		for (int i = (int)_staticVars.size() - 1; i >= 0; i--)
		{
			VarInfo& v2 = _staticVars[i];
			if (VAR_STRING == v2.GetType())
			{
				delete v2._str;
			}

		}
		_staticVars.clear();
	}

	SFunctionInfo*	FindFun(const std::string& name)
	{
		auto it = _funs.find(name);
		if (it == _funs.end())
			return NULL;
		return &(*it).second;
	}
	SFunctionInfo*	FindFun(int ifun)
	{
		auto it = _funIDs.find(ifun);
		if (it == _funIDs.end())
			return NULL;
		return FindFun((*it).second);
	}

	int	AddStaticInt(int num)
	{
		for (int i = (int)_staticVars.size() - 1; i >= 0; i--)
		{
			VarInfo& v2 = _staticVars[i];
			if (VAR_INT == v2.GetType())
			{
				if (num == v2._int)
					return i + COMPILE_STATIC_VAR_BEGIN;
			}
		}
		VarInfo v;
		v.SetType(VAR_INT);
		v._int = num;

		int idx = (int)_staticVars.size() + COMPILE_STATIC_VAR_BEGIN;
		_staticVars.push_back(v);
		return idx;
	}
	bool GetStaticNum(int var, int* value)
	{
		for (int i = (int)_staticVars.size() - 1; i >= 0; i--)
		{
			if (i + COMPILE_STATIC_VAR_BEGIN == var)
			{
				VarInfo& v2 = _staticVars[i];
				if (VAR_INT == v2.GetType())
				{
					*value = v2._int;
					return true;
				}
				if (VAR_FLOAT == v2.GetType())
				{
					*value = (int)v2._float;
					return true;
				}
			}
		}
		return false;
	}

	int	AddStaticNum(double num)
	{
		for (int i = (int)_staticVars.size() - 1; i >= 0; i--)
		{
			VarInfo& v2 = _staticVars[i];
			if (VAR_FLOAT == v2.GetType())
			{
				if (num == v2._float)
					return i + COMPILE_STATIC_VAR_BEGIN;
			}
		}
		VarInfo v;
		v.SetType(VAR_FLOAT);
		v._float = num;

		int idx = (int)_staticVars.size() + COMPILE_STATIC_VAR_BEGIN;
		_staticVars.push_back(v);
		return idx;
	}
	int	AddStaticString(const std::string& str)
	{
		for (int i = (int)_staticVars.size() - 1; i >= 1; i--) // 0 is System
		{
			VarInfo& v2 = _staticVars[i];
			if (VAR_STRING == v2.GetType())
			{
				if (str == v2._str->_str)
					return i + COMPILE_STATIC_VAR_BEGIN;
			}
		}

		VarInfo v;
		v.SetType(VAR_STRING);
		v._str = new StringInfo();
		v._str->_str = str;

		int idx = (int)_staticVars.size() + COMPILE_STATIC_VAR_BEGIN;
		_staticVars.push_back(v);
		return idx;
	}
	int	AddStaticBool(bool b)
	{
		for (int i = (int)_staticVars.size() - 1; i >= 0; i--)
		{
			VarInfo& v2 = _staticVars[i];
			if (VAR_BOOL == v2.GetType())
			{
				if (b == v2._bl)
					return i + COMPILE_STATIC_VAR_BEGIN;
			}
		}
		VarInfo v;
		v.SetType(VAR_BOOL);
		v._bl = b;

		int idx = (int)_staticVars.size() + COMPILE_STATIC_VAR_BEGIN;
		_staticVars.push_back(v);
		return idx;
	}
	int	AddStaticFunction(int iIndex)
	{
		for (int i = (int)_staticVars.size() - 1; i >= 0; i--)
		{
			VarInfo& v2 = _staticVars[i];
			if (VAR_FUN == v2.GetType())
			{
				//if (b == v2._bl)
				//	return i + COMPILE_STATIC_VAR_BEGIN;
			}
		}
		VarInfo v;
		v.SetType(VAR_FUN);
		v._fun_index = iIndex;

		int idx = (int)_staticVars.size() + COMPILE_STATIC_VAR_BEGIN;
		_staticVars.push_back(v);
		return idx;
	}
};

