#pragma once

#include <thread>
#include <time.h>

#include "NeoConfig.h"
#include "NeoVM.h"
//#include "NeoVMWorker.h"
#include "NeoVMMemoryPool.h"
#include "NeoQueue.h"

#include "NeoVMMap.h"
#include "NeoVMSet.h"
#include "NeoVMList.h"


namespace NeoScript
{
struct neo_libs;
struct neo_DCalllibs;;
class CNArchive;
class CNeoVMWorker;

struct SystemFun
{
	std::string fname;
	int			argCount;
};


enum eNeoDefaultString
{
	NDF_NULL,
	NDF_INT,
	NDF_FLOAT,
	NDF_BOOL,
	NDF_STRING,
	NDF_TABLE,
	NDF_LIST,
	NDF_SET,
	NDF_COROUTINE,
	NDF_FUNCTION,
	NDF_MODULE,
	NDF_ASYNC,

	NDF_TRUE,
	NDF_FALSE,

	NDF_SUSPENDED,
	NDF_RUNNING,
	NDF_DEAD,
	NDF_NORMAL,

	NDF_MAX
};

typedef u8	OpType;
typedef u8	ArgFlag;

enum eNOperationSub : u8
{
	eOP_ADD,
	eOP_SUB,
	eOP_MUL,
	eOP_DIV,
	eOP_PER,
	eOP_LSH,
	eOP_RSH,
	eOP_AND,
	eOP__OR,
	eOP_XOR,
};

enum eNOperation : OpType
{
	NOP_MOV,
	NOP_MOVI,

	NOP_MOV_MINUS,
	NOP_ADD2,		// +
	NOP_SUB2,		// -
	NOP_MUL2,		// *
	NOP_DIV2,		// /
	NOP_PERSENT2,	// %
	NOP_LSHIFT2,	// <<
	NOP_RSHIFT2,	// >>
	NOP_AND2,		// &
	NOP_OR2,		// |
	NOP_XOR2,		// ^
	//NOP_NOT2,		// ~
	//NOP_LOG_NOT2,	// ! not support ( a != 0x01; if(a != b) ... bit / logic )

	NOP_VAR_CLEAR,
	NOP_INC,
	NOP_DEC,

	NOP_ADD3,
	NOP_SUB3,
	NOP_MUL3,
	NOP_DIV3,
	NOP_PERSENT3,
	NOP_LSHIFT3,
	NOP_RSHIFT3,
	NOP_AND3,
	NOP_OR3,
	NOP_XOR3,
	//NOP_NOT3,

	NOP_GREAT,		// >
	NOP_GREAT_EQ,	// >=
	NOP_LESS,		// <
	NOP_LESS_EQ,	// <=
	NOP_EQUAL2,		// ==
	NOP_NEQUAL,		// !=
	NOP_AND,		// &
	NOP_OR,			// |
	NOP_LOG_AND,	// &&
	NOP_LOG_OR,	// ||

	NOP_JMP,
	NOP_JMP_FALSE,
	NOP_JMP_TRUE,

	NOP_JMP_GREAT,		// >
	NOP_JMP_GREAT_EQ,	// >=
	NOP_JMP_LESS,		// <
	NOP_JMP_LESS_EQ,	// <=
	NOP_JMP_EQUAL2,	// ==
	NOP_JMP_NEQUAL,	// !=
	NOP_JMP_AND,	// &&
	NOP_JMP_OR,		// ||
	NOP_JMP_NAND,	// !(&&)
	NOP_JMP_NOR,	// !(||)
	NOP_JMP_FOR,	// for
	NOP_JMP_FOREACH,// foreach

	NOP_STR_ADD,	// ..

	NOP_TOSTRING,
	NOP_TOINT,
	NOP_TOFLOAT,
	NOP_TOSIZE,
	NOP_GETTYPE,
	NOP_SLEEP, // arg1 unuse

	NOP_FMOV1, // Function Info Load
	NOP_FMOV2, // Function Info Load

	NOP_CALL,
	NOP_PTRCALL,	// Multiple Call
	NOP_PTRCALL2,	// Native Call
	NOP_RETURN,
	//	NOP_FUNEND,

	NOP_TABLE_ALLOC,
	NOP_CLT_READ,
	NOP_TABLE_REMOVE,
	NOP_CLT_MOV,

	NOP_TABLE_ADD2,
	NOP_TABLE_SUB2,
	NOP_TABLE_MUL2,
	NOP_TABLE_DIV2,
	NOP_TABLE_PERSENT2,

	NOP_LIST_ALLOC,
	NOP_LIST_REMOVE,

	NOP_VERIFY_TYPE,
	NOP_CHANGE_INT,
	NOP_YIELD,
	NOP_IDLE,

	NOP_NONE,
	NOP_ERROR,
	NOP_MAX,
}; // Operation length

enum eYield_Type : s16
{
	YILED_RETURN,
};

#define CODE_TO_NOP(op) (eNOperation)(op)// >> 2)
#define CODE_TO_LEN(op) 3 //(op & 0x03)

#pragma pack(1)
struct SVMOperation
{
	eNOperation	  op;
	u8		argFlag;
	short	n1;

	union
	{
		struct
		{
			short	n2;
			short	n3;
		};
		int	n23;
	};
};
#pragma pack()

struct VarInfo;
struct SVarWrapper;
class CNeoVMWorker;
struct FunctionPtr;



struct SCallStack
{
	int		_iSP_Vars;
	int		_iSP_VarsMax;
	int		_iReturnOffset;
	VarInfo* _pReturnValue;
};

enum COROUTINE_STATE
{
	COROUTINE_STATE_SUSPENDED,
	COROUTINE_STATE_RUNNING,
	COROUTINE_STATE_DEAD,
	COROUTINE_STATE_NORMAL,
};
enum COROUTINE_SUB_STATE
{
	COROUTINE_SUB_NORMAL,
	COROUTINE_SUB_START,
	COROUTINE_SUB_CLOSE,
};

enum ASYNC_STATE
{
	ASYNC_READY,
	ASYNC_PENDING,
	ASYNC_COMPLETED,
};

enum ASYNC_COMMAND
{
	ASYNC_GET,
	ASYNC_POST,
	ASYNC_POST_JSON,
};



struct CoroutineBase
{
	int			_iSP_Vars;
	int			_iSP_Vars_Max2;
	int			_iSP_VarsMax;
	SVMOperation* _pCodeCurrent;
	void ClearSP()
	{
		_iSP_Vars = 0;
		_iSP_Vars_Max2 = 0;
		_iSP_VarsMax = 0;
	}
};

struct CoroutineInfo : AllocBase
{
	//	int	_CoroutineID;
	int	_fun_index;
	COROUTINE_STATE _state;
	COROUTINE_SUB_STATE _sub_state;

	CoroutineBase _info;

	std::vector<VarInfo>	m_sVarStack;
	SimpleVector<SCallStack>	m_sCallStack;
};

struct StringInfo : AllocBase, VMString
{
};


struct AsyncInfo : AllocBase
{
	std::string _request;
	std::string _body;
	std::vector< std::pair<std::string, std::string> > _headers;

	int			_fun_index;
	int			_timeout;

	ASYNC_STATE _state;
	ASYNC_COMMAND _type;
	bool		_success;
	std::string _resultValue;

	VarInfo		_LockReferance;
	NeoEvent	_event;
};


struct MapInfo;
struct MapNode;
struct ListInfo;
struct SetInfo;
struct SetNode;

int& GetModuleRefCount(VarInfo* p);

NEOS_FORCEINLINE void Move_DestNoRelease(VarInfo* v1, VarInfo* v2)
{
	v1->SetType(v2->GetType());
	switch (v2->GetType())
	{
	case VAR_INT: v1->_int = v2->_int; break;
	case VAR_FLOAT: v1->_float = v2->_float; break;
	case VAR_BOOL: v1->_bl = v2->_bl; break;
	case VAR_NONE: break;
	case VAR_FUN: v1->_fun_index = v2->_fun_index; break;
	case VAR_FUN_NATIVE: v1->_funPtr = v2->_funPtr; break;
	case VAR_CHAR: v1->_c = v2->_c; break;

	case VAR_STRING: v1->_str = v2->_str; ++v1->_str->_refCount; break;
	case VAR_MAP: v1->_tbl = v2->_tbl; ++v1->_tbl->_refCount; break;
	case VAR_LIST: v1->_lst = v2->_lst; ++v1->_lst->_refCount; break;
	case VAR_SET: v1->_set = v2->_set; ++v1->_set->_refCount; break;
	case VAR_COROUTINE: v1->_cor = v2->_cor; ++v1->_cor->_refCount; break;
	case VAR_MODULE: v1->_module = v2->_module; ++GetModuleRefCount(v1); break;
	case VAR_ASYNC: v1->_async = v2->_async; ++v1->_async->_refCount; break;
	default: break;
	}
}

};