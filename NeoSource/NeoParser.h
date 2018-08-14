#pragma once

#include "NeoVM.h"

#define INVALID_ERROR_PARSEJOB			(-1)

#define COMPILE_LOCALTMP_VAR_BEGIN		(10000)
#define COMPILE_STATIC_VAR_BEGIN		(15000)
#define COMPILE_GLObAL_VAR_BEGIN		(20000)
#define COMPILE_CALLARG_VAR_BEGIN		(30000) // 256 개 이상 나오지 않는다.
#define STACK_POS_RETURN		(32767)

BOOL IsTempVar(int iVar);

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
		return (NOP_TYPE)((BYTE*)_code.GetData())[_iLastOPOffset];
	}

	void	Push_OP(BYTE op, short r, short a1, short a2)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
		_code.Write(&a1, sizeof(a1));
		_code.Write(&a2, sizeof(a2));
	}
	void	Push_Call(BYTE op, short fun, short args)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&fun, sizeof(fun));
		_code.Write(&args, sizeof(args));
	}
	void	Push_CallPtr(short table, short index, short args)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_PTRCALL;
		_code.Write(&op, sizeof(op));
		_code.Write(&table, sizeof(table));
		_code.Write(&index, sizeof(index));
		_code.Write(&args, sizeof(args));
	}
	void	Push_MOV(BYTE op, short r, short s)
	{
		if (op == NOP_MOV)
		{
			if (IsTempVar(s))
			{
				NOP_TYPE preOP = GetLastOP();
				BYTE *pre = (BYTE*)_code.GetData() + 1 + _iLastOPOffset;
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

		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
		_code.Write(&s, sizeof(s));
	}
	void	Push_IncDec(BYTE op, short r)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
	}
	void	Push_RETURN(short r)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_RETURN;
		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
	}

	void	Push_JMP(int destOffset)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_JMP;
		short add = destOffset - (_code.GetBufferOffset() + 3);
		_code.Write(&op, sizeof(op));
		_code.Write(&add, sizeof(add));
	}
	void	Push_JMPFalse(short var, int destOffset)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_JMP_FALSE;
		short add = destOffset - (_code.GetBufferOffset() + 5);
		_code.Write(&op, sizeof(op));
		_code.Write(&var, sizeof(var));
		_code.Write(&add, sizeof(add));
	}
	void	Push_JMPTrue(short var, int destOffset)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_JMP_TRUE;
		short add = destOffset - (_code.GetBufferOffset() + 5);
		_code.Write(&op, sizeof(op));
		_code.Write(&var, sizeof(var));
		_code.Write(&add, sizeof(add));
	}
	void	Set_JumpOffet(SJumpValue sJmp, int destOffset)
	{
		BYTE* p = (BYTE*)_code.GetData();
		*((short*)(p + sJmp._iCodePosOffset)) = (short)(destOffset - sJmp._iBaseJmpOffset);
	}
	void	Push_TableAlloc(short r)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_TABLE_ALLOC;
		_code.Write(&op, sizeof(op));
		_code.Write(&r, sizeof(r));
	}
	void	Push_TableInsert(short nTable, short nArray, short nValue)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_TABLE_INSERT;
		_code.Write(&op, sizeof(op));
		_code.Write(&nTable, sizeof(nTable));
		_code.Write(&nArray, sizeof(nArray));
		_code.Write(&nValue, sizeof(nValue));
	}
	void	Push_TableRead(short nTable, short nArray, short nValue)
	{
		_iLastOPOffset = _code.GetBufferOffset();

		BYTE op = NOP_TABLE_READ;
		_code.Write(&op, sizeof(op));
		_code.Write(&nTable, sizeof(nTable));
		_code.Write(&nArray, sizeof(nArray));
		_code.Write(&nValue, sizeof(nValue));
	}

	void	Push_ToType(BYTE op, short r, short s)
	{
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

