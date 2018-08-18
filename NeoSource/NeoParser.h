#pragma once

#include "NeoArchive.h"
#include "NeoVM.h"
#include "NeoTextLoader.h"

#define INVALID_ERROR_PARSEJOB			(-1)

#define COMPILE_LOCALTMP_VAR_BEGIN		(10000)
#define COMPILE_STATIC_VAR_BEGIN		(15000)
#define COMPILE_GLObAL_VAR_BEGIN		(20000)
#define COMPILE_CALLARG_VAR_BEGIN		(30000) // 256 개 이상 나오지 않는다.
#define STACK_POS_RETURN		(32767)

bool IsTempVar(int iVar);

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
	int							_funID; // Index
	std::string					_name;
	std::set<std::string>		_args;
	FUNCTION_TYPE				_funType = FUNT_NORMAL;


	int							_localVarCount;

	int							_localTempMax;
	int							_localTempCount;

	CNArchive					_code;

	void Clear()
	{
		_args.clear();
		_localVarCount = 0;
		_localTempMax = _localTempCount = 0;
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


	int _iLastOPOffset = 0;
	NOP_TYPE GetLastOP()
	{
		return (NOP_TYPE)((u8*)_code.GetData())[_iLastOPOffset];
	}
	NOP_TYPE GetOP(int iOffsetOP)
	{
		return (NOP_TYPE)*((u8*)_code.GetData() + iOffsetOP);
	}

	void	Push_OP(CArchiveRdWC& ar, u8 op, short r, short a1, short a2)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
		_code.Write(&a1, sizeof(a1));
		_code.Write(&a2, sizeof(a2));
	}
	void	Push_Call(CArchiveRdWC& ar, u8 op, short fun, short args)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&fun, sizeof(fun));
		_code.Write(&args, sizeof(args));
	}
	void	Push_CallPtr(CArchiveRdWC& ar, short table, short index, short args)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_PTRCALL;
		_code.Write(&op, sizeof(op));
		_code.Write(&table, sizeof(table));
		_code.Write(&index, sizeof(index));
		_code.Write(&args, sizeof(args));
	}
	void	Push_MOV(CArchiveRdWC& ar, u8 op, short r, short s)
	{
		if (op == NOP_MOV)
		{
			if (IsTempVar(s))
			{
				NOP_TYPE preOP = GetLastOP();
				u8 *pre = (u8*)_code.GetData() + 1 + _iLastOPOffset;
				short* preDest = (short*)pre;
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
				case NOP_GETTYPE:

				case NOP_GREAT:		// >
				case NOP_GREAT_EQ:	// >=
				case NOP_LESS:		// <
				case NOP_LESS_EQ:	// <=
				case NOP_EQUAL2:	// ==
				case NOP_NEQUAL:	// !=
				case NOP_AND:	// &&
				case NOP_OR:		// ||
					if (*preDest == s)
					{
						*preDest = r;
						return;
					}
					break;
				}
			}
		}

		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
		_code.Write(&s, sizeof(s));
	}
	void	Push_IncDec(CArchiveRdWC& ar, u8 op, short r)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
	}
	void	Push_RETURN(CArchiveRdWC& ar, short r)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_RETURN;
		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
	}
	void	Push_FUNEND(CArchiveRdWC& ar)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_FUNEND;
		_code.Write(&op, sizeof(op));
	}
	void	Push_JMP(CArchiveRdWC& ar, int destOffset)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_JMP;
		short add = destOffset - (_code.GetBufferOffset() + 3);
		_code.Write(&op, sizeof(op));
		_code.Write(&add, sizeof(add));
	}
	void	Push_JMPFalse(CArchiveRdWC& ar, short var, int destOffset)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_JMP_FALSE;
		short add = destOffset - (_code.GetBufferOffset() + 5);
		_code.Write(&op, sizeof(op));
		_code.Write(&var, sizeof(var));
		_code.Write(&add, sizeof(add));
	}
	void	Push_JMPTrue(CArchiveRdWC& ar, short var, int destOffset)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_JMP_TRUE;
		short add = destOffset - (_code.GetBufferOffset() + 5);
		_code.Write(&op, sizeof(op));
		_code.Write(&var, sizeof(var));
		_code.Write(&add, sizeof(add));
	}
	void	Set_JumpOffet(SJumpValue sJmp, int destOffset)
	{
		u8* p = (u8*)_code.GetData();
		*((short*)(p + sJmp._iCodePosOffset)) = (short)(destOffset - sJmp._iBaseJmpOffset);
	}
	void	Push_TableAlloc(CArchiveRdWC& ar, short r)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_TABLE_ALLOC;
		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
	}
	void	Push_TableInsert(CArchiveRdWC& ar, short nTable, short nArray, short nValue)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_TABLE_INSERT;
		_code.Write(&op, sizeof(op));
		_code.Write(&nTable, sizeof(nTable));
		_code.Write(&nArray, sizeof(nArray));
		_code.Write(&nValue, sizeof(nValue));
	}
	void	Push_TableRead(CArchiveRdWC& ar, short nTable, short nArray, short nValue)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		u8 op = NOP_TABLE_READ;
		_code.Write(&op, sizeof(op));
		_code.Write(&nTable, sizeof(nTable));
		_code.Write(&nArray, sizeof(nArray));
		_code.Write(&nValue, sizeof(nValue));
	}

	void	Push_ToType(CArchiveRdWC& ar, u8 op, short r, short s)
	{
		debug_info dbg(ar.CurFile(), ar.CurLine());
		_code.Write(&dbg, sizeof(dbg));
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
		_code.Write(&s, sizeof(s));
	}

};




struct SFunctions
{
	std::map<std::string, SFunctionInfo> _funs;
	SFunctionInfo						_cur;
	std::vector<VarInfo>				_staticVars;

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
};

