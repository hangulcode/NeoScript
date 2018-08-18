#include <math.h>
#include <stdlib.h>
#include "NeoVM.h"
#include "NeoArchive.h"

void	DebugLog(const char*	lpszString, ...);

void CNeoVM::Var_AddRef(VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		++d->_str->_refCount;
		break;
	case VAR_TABLE:
		++d->_tbl->_refCount;
		break;
	}
}
void CNeoVM::Var_Release(VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			FreeString(d);
		d->_str = NULL;
		break;
	case VAR_TABLE:
		if (--d->_tbl->_refCount <= 0)
			FreeTable(d);
		d->_tbl = NULL;
		break;
	}
	d->ClearType();
}



void CNeoVM::Var_SetInt(VarInfo *d, int v)
{
	if (d->GetType() != VAR_INT)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_INT);
	}
	d->_int = v;
}

void CNeoVM::Var_SetFloat(VarInfo *d, double v)
{
	if (d->GetType() != VAR_FLOAT)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_FLOAT);
	}
	d->_float = v;
}
void CNeoVM::Var_SetBool(VarInfo *d, bool v)
{
	if (d->GetType() != VAR_BOOL)
	{
		if (d->IsAllocType())
			Var_Release(d);

		d->SetType(VAR_BOOL);
	}
	d->_bl = v;
}



void CNeoVM::Var_SetString(VarInfo *d, const char* str)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = StringAlloc(str);
	++d->_str->_refCount;
}
void CNeoVM::Var_SetTable(VarInfo *d, TableInfo* p)
{
	if (d->IsAllocType())
		Var_Release(d);

	d->SetType(VAR_TABLE);
	d->_tbl = p;
	++d->_tbl->_refCount;
}


StringInfo* CNeoVM::StringAlloc(const char* str)
{
	StringInfo* p = new StringInfo();
	while (true)
	{
		if (++iLastIDString <= 0)
			iLastIDString = 1;

		if (_sStrings.end() == _sStrings.find(iLastIDString))
		{
			break;
		}
	}

	p->_str = str;
	p->_refCount = 0;

	_sStrings[iLastIDString] = p;
	return p;
}
void CNeoVM::FreeString(VarInfo *d)
{

}
TableInfo* CNeoVM::TableAlloc()
{
	TableInfo* pTable = new TableInfo();
	while (true)
	{
		if (++iLastIDTable <= 0)
			iLastIDTable = 1;

		if (_sTables.end() == _sTables.find(iLastIDTable))
		{
			break;
		}
	}
	pTable->_TableID = iLastIDTable;
	pTable->_refCount = 0;

	_sTables[iLastIDTable] = pTable;
	return pTable;
}
void CNeoVM::FreeTable(VarInfo *d)
{
	auto it = _sTables.find(d->_tbl->_TableID);
	if (it == _sTables.end())
		return; // Error

	_sTables.erase(it);
	delete d->_tbl;
}
void CNeoVM::TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
{
	if (pTable->GetType() != VAR_TABLE)
		return;
	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			pTable->_tbl->_intMap[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			pTable->_tbl->_intMap[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			VarInfo* pDest = &(*it).second;
			Var_Release(pDest);
			*pDest = *pValue;
		}
		else
			pTable->_tbl->_strMap[n] = *pValue;
		Var_AddRef(pValue);
		break;
	}
	}
}
FunctionPtr* CNeoVM::GetPtrFunction(VarInfo *pTable, VarInfo *pArray)
{
	if (pTable->GetType() != VAR_TABLE)
		return NULL;

	VarInfo* pValue;
	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = (int)pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			pValue = &(*it).second;
			if (pValue->GetType() == VAR_PTRFUN)
				return &pValue->_fun;
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			pValue = &(*it).second;
			if (pValue->GetType() == VAR_PTRFUN)
				return &pValue->_fun;
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			pValue = &(*it).second;
			if (pValue->GetType() == VAR_PTRFUN)
				return &pValue->_fun;
		}
		break;
	}
	}
	return NULL;
}
void CNeoVM::TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue)
{
	if (pTable->GetType() != VAR_TABLE)
		return;

	Var_Release(pValue);
	switch (pArray->GetType())
	{
	case VAR_INT:
	{
		int n = (int)pArray->_int;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	case VAR_FLOAT:
	{
		int n = (int)pArray->_float;
		auto it = pTable->_tbl->_intMap.find(n);
		if (it != pTable->_tbl->_intMap.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	case VAR_STRING:
	{
		std::string& n = pArray->_str->_str;
		auto it = pTable->_tbl->_strMap.find(n);
		if (it != pTable->_tbl->_strMap.end())
		{
			*pValue = (*it).second;
			Var_AddRef(pValue);
		}
		break;
	}
	}
}


void CNeoVM::Move(VarInfo* v1, VarInfo* v2)
{
	switch (v2->GetType())
	{
	case VAR_INT:
		Var_SetInt(v1, v2->_int);
		break;
	case VAR_FLOAT:
		Var_SetFloat(v1, v2->_float);
		break;
	case VAR_BOOL:
		Var_SetBool(v1, v2->_bl);
		break;
	case VAR_STRING:
		Var_Release(v1);
		v1->SetType(v2->GetType());
		v1->_str = v2->_str;
		++v1->_str->_refCount;
		break;
	case VAR_TABLE:
		Var_Release(v1);
		v1->SetType(v2->GetType());
		v1->_tbl = v2->_tbl;
		++v1->_tbl->_refCount;
		break;
	}
}
void CNeoVM::MoveMinus(VarInfo* v1, VarInfo* v2)
{
	switch (v2->GetType())
	{
	case VAR_INT:
		Var_SetInt(v1, -v2->_int);
		break;
	case VAR_FLOAT:
		Var_SetFloat(v1, -v2->_float);
		break;
	case VAR_BOOL:
		break;
	case VAR_STRING:
		break;
	case VAR_TABLE:
		break;
	}
}
void CNeoVM::Add2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			r->_int += v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int + v2->_float;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			r->_float += v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
			r->_float += v2->_float;
	}
	else if (r->GetType() == VAR_STRING)
	{
		if (v2->GetType() == VAR_STRING)
			Var_SetString(r, (r->_str->_str + v2->_str->_str).c_str());
		return;
	}
}

void CNeoVM::Sub2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			r->_int -= v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int - v2->_float;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			r->_float -= v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
			r->_float -= v2->_float;
	}
}

void CNeoVM::Mul2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			r->_int *= v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int * v2->_float;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			r->_float *= v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
			r->_float *= v2->_float;
	}
}

void CNeoVM::Div2(VarInfo* r, VarInfo* v2)
{
	if (r->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			r->_int /= v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int / v2->_float;
		}
	}
	else if (r->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			r->_float /= v2->_int;
		else if (v2->GetType() == VAR_FLOAT)
			r->_float /= v2->_float;
	}
}
void CNeoVM::Add(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetInt(r, v1->_int + v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_int + v2->_float);
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetFloat(r, v1->_float + v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_float + v2->_float);
	}
	else if (v1->GetType() == VAR_STRING)
	{
		if (v2->GetType() == VAR_STRING)
			Var_SetString(r, (r->_str->_str + v2->_str->_str).c_str());
		return;
	}
}

void CNeoVM::Sub(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetInt(r, v1->_int - v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_int - v2->_float);
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetFloat(r, v1->_float - v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_float - v2->_float);
	}
}

void CNeoVM::Mul(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetInt(r, v1->_int * v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_int * v2->_float);
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetFloat(r, v1->_float * v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_float * v2->_float);
	}
}

void CNeoVM::Div(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetInt(r, v1->_int / v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_int / v2->_float);
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			Var_SetFloat(r, v1->_float / v2->_int);
		else if (v2->GetType() == VAR_FLOAT)
			Var_SetFloat(r, v1->_float / v2->_float);
	}
}
void CNeoVM::Inc(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		++v1->_int;
		break;
	case VAR_FLOAT:
		++v1->_float;
		break;
	case VAR_BOOL:
		break;
	case VAR_STRING:
		break;
	}
}
void CNeoVM::Dec(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		--v1->_int;
		break;
	case VAR_FLOAT:
		--v1->_float;
		break;
	case VAR_BOOL:
		break;
	case VAR_STRING:
		break;
	}
}

bool CNeoVM::CompareEQ(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_int == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int == v2->_float;
		return false;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float == v2->_float;
		return false;
	}
	return false;
}
bool CNeoVM::CompareGR(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_int > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int > v2->_float;
		return false;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float > v2->_float;
		return false;
	}
	return false;
}
bool CNeoVM::CompareGE(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if(v2->GetType() == VAR_INT)
			return v1->_int >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int >= v2->_float;
		return false;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float >= v2->_float;
		return false;
	}
	return false;
}
std::string CNeoVM::ToString(VarInfo* v1)
{
	char ch[256];
	switch (v1->GetType())
	{
	case VAR_NONE:
		return "null";
	case VAR_BOOL:
		return v1->_bl ? "true" : "false";
	case VAR_INT:
#ifdef _WIN32
		sprintf_s(ch, _countof(ch), "%d", v1->_int);
#else
		sprintf(ch, "%d", v1->_int);
#endif
		return ch;
	case VAR_FLOAT:
#ifdef _WIN32
		sprintf_s(ch, _countof(ch), "%lf", v1->_float);
#else
		sprintf(ch, "%lf", v1->_float);
#endif
		return ch;
	case VAR_STRING:
		return v1->_str->_str;
	case VAR_TABLE:
		return "table";
	case VAR_PTRFUN:
		return "function";
	}
	return "null";
}
int CNeoVM::ToInt(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return -1;
	case VAR_BOOL:
		return v1->_bl ? 1 : 0;
	case VAR_INT:
		return v1->_int;
	case VAR_FLOAT:
		return (int)v1->_float;
	case VAR_STRING:
		return ::atoi(v1->_str->_str.c_str());
	case VAR_TABLE:
		return -1;
	case VAR_PTRFUN:
		return -1;
	}
	return -1;
}
double CNeoVM::ToFloat(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return -1;
	case VAR_BOOL:
		return v1->_bl ? 1 : 0;
	case VAR_INT:
		return v1->_int;
	case VAR_FLOAT:
		return v1->_float;
	case VAR_STRING:
		return atof(v1->_str->_str.c_str());
	case VAR_TABLE:
		return -1;
	case VAR_PTRFUN:
		return -1;
	}
	return -1;
}
std::string CNeoVM::GetType(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		return "null";
	case VAR_BOOL:
		return "bool";
	case VAR_INT:
		return "int";
	case VAR_FLOAT:
		return "float";
	case VAR_STRING:
		return "string";
	case VAR_TABLE:
		return "table";
	case VAR_PTRFUN:
		return "function";
	}
	return "null";
}


CNeoVM::CNeoVM(int iStackSize)
{
	m_sVarStack.resize(iStackSize);
	m_sCallStack.reserve(1000);
}
CNeoVM::~CNeoVM()
{

}

void CNeoVM::SetError(const char* pErrMsg)
{
	_pErrorMsg = pErrMsg;
	if (_pErrorMsg != NULL)
	{
		m_sCallStack.clear();
		_iSP_Vars = _header._iStaticVarCount;
	}
}


bool	CNeoVM::Run(int iFunctionID)
{
	SFunctionTable fun = m_sFunctionPtr[iFunctionID];
	int iArgs = (int)_args.size();
	if (iArgs != fun._argsCount)
		return false;

	SetCodePtr(fun._codePtr);

	_iSP_Vars = 0;// _header._iStaticVarCount;
	int iSP_VarsMax = _iSP_Vars + fun._localAddCount;

	_iSP_Vars_Max2 = iSP_VarsMax;

	if (iArgs > fun._argsCount)
		iArgs = fun._argsCount;

	int iCur;
	for (iCur = 0; iCur < iArgs; iCur++)
		m_sVarStack[1 + _iSP_Vars + iCur] = _args[iCur];
	for (; iCur < fun._argsCount; iCur++)
		Var_Release(&m_sVarStack[1 + _iSP_Vars + iCur]);

	NOP_TYPE op;
	short n1, n2, n3;
//	VarInfo* v1;
//	VarInfo* v2;
//	VarInfo* v3;

	FunctionPtr* pFunctionPtr;
	SCallStack callStack;
	int iTemp;

	while (true)
	{
		op = (NOP_TYPE)GetU8();
		switch (op)
		{
		case NOP_MOV:
			n1 = GetS16(); n2 = GetS16();
			Move(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_MOV_MINUS:
			n1 = GetS16(); n2 = GetS16();
			MoveMinus(GetVarPtr(n1), GetVarPtr(n2));
			break;

		case NOP_ADD2:
			n1 = GetS16(); n2 = GetS16();
			Add2(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_SUB2:
			n1 = GetS16(); n2 = GetS16();
			Sub2(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_MUL2:
			n1 = GetS16(); n2 = GetS16();
			Mul2(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_DIV2:
			n1 = GetS16(); n2 = GetS16();
			Div2(GetVarPtr(n1), GetVarPtr(n2));
			break;

		case NOP_INC:
			n1 = GetS16();
			Inc(GetVarPtr(n1));
			break;
		case NOP_DEC:
			n1 = GetS16();
			Dec(GetVarPtr(n1));
			break;

		case NOP_ADD3:
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Add(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		case NOP_SUB3:
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Sub(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		case NOP_MUL3:
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Mul(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		case NOP_DIV3:
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Div(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;

		case NOP_GREAT:		// >
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), CompareGR(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_GREAT_EQ:	// >=
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), CompareGE(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_LESS:		// <
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), CompareGR(GetVarPtr(n3), GetVarPtr(n2)));
			break;
		case NOP_LESS_EQ:	// <=
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), CompareGE(GetVarPtr(n3), GetVarPtr(n2)));
			break;
		case NOP_EQUAL2:	// ==
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), CompareEQ(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_NEQUAL:	// !=
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), !CompareEQ(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_AND:	// &&
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), GetVarPtr(n3)->IsTrue() && GetVarPtr(n2)->IsTrue());
			break;
		case NOP_OR:	// ||
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetBool(GetVarPtr(n1), GetVarPtr(n2)->IsTrue() || GetVarPtr(n3)->IsTrue());
			break;



		case NOP_JMP_GREAT:		// >
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (true == CompareGR(GetVarPtr(n2), GetVarPtr(n3)))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_GREAT_EQ:	// >=
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (true == CompareGE(GetVarPtr(n2), GetVarPtr(n3)))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_LESS:		// <
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (true == CompareGR(GetVarPtr(n3), GetVarPtr(n2)))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_LESS_EQ:	// <=
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (true == CompareGE(GetVarPtr(n3), GetVarPtr(n2)))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_EQUAL2:	// ==
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (true == CompareEQ(GetVarPtr(n2), GetVarPtr(n3)))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_NEQUAL:	// !=
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (false == CompareEQ(GetVarPtr(n2), GetVarPtr(n3)))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_AND:	// &&
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (GetVarPtr(n3)->IsTrue() && GetVarPtr(n2)->IsTrue())
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_OR:		// ||
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (GetVarPtr(n3)->IsTrue() || GetVarPtr(n2)->IsTrue())
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_NAND:	// !(&&)
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (false == (GetVarPtr(n3)->IsTrue() && GetVarPtr(n2)->IsTrue()))
				SetCodeIncPtr(n1);
			break;
		case NOP_JMP_NOR:	// !(||)
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			if (false == (GetVarPtr(n3)->IsTrue() || GetVarPtr(n2)->IsTrue()))
				SetCodeIncPtr(n1);
			break;



		case NOP_STR_ADD:	// ..
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			Var_SetString(GetVarPtr(n1), (ToString(GetVarPtr(n2)) + ToString(GetVarPtr(n3))).c_str());
			break;

		case NOP_TOSTRING:
			n1 = GetS16(); n2 = GetS16();
			Var_SetString(GetVarPtr(n1), ToString(GetVarPtr(n2)).c_str());
			break;
		case NOP_TOINT:
			n1 = GetS16(); n2 = GetS16();
			Var_SetInt(GetVarPtr(n1), ToInt(GetVarPtr(n2)));
			break;
		case NOP_TOFLOAT:
			n1 = GetS16(); n2 = GetS16();
			Var_SetFloat(GetVarPtr(n1), ToFloat(GetVarPtr(n2)));
			break;
		case NOP_GETTYPE:
			n1 = GetS16(); n2 = GetS16();
			Var_SetString(GetVarPtr(n1), GetType(GetVarPtr(n2)).c_str());
			break;

		case NOP_JMP:
			n1 = GetS16();
			SetCodeIncPtr(n1);
			break;
		case NOP_JMP_FALSE:
			n1 = GetS16(); n2 = GetS16();
			if (false == GetVarPtr(n1)->IsTrue())
				SetCodeIncPtr(n2);
			break;
		case NOP_JMP_TRUE:
			n1 = GetS16(); n2 = GetS16();
			if (true == GetVarPtr(n1)->IsTrue())
				SetCodeIncPtr(n2);
			break;

		case NOP_CALL:
			n1 = GetS16(); n2 = GetS16();
			callStack._iReturnOffset = GetCodeptr();
			callStack._iSP_Vars = _iSP_Vars;
			callStack._iSP_VarsMax = iSP_VarsMax;
			m_sCallStack.push_back(callStack);

			fun = m_sFunctionPtr[n1];
			SetCodePtr(fun._codePtr);
			_iSP_Vars = iSP_VarsMax;
			iSP_VarsMax = _iSP_Vars + fun._localAddCount;
			if (_iSP_Vars_Max2 < iSP_VarsMax)
				_iSP_Vars_Max2 = iSP_VarsMax;
			break;
		case NOP_FARCALL:
		{
			n1 = GetS16(); n2 = GetS16();

			fun = m_sFunctionPtr[n1];
			if (fun._fun._func == NULL)
			{	// Error
				break;
			}

			int iSave = _iSP_Vars;
			_iSP_Vars = iSP_VarsMax;

			if((*fun._fun._fn)(this, fun._fun._func, n2) < 0)
			{
				SetError("Ptr Call Argument Count Error");
				return false;
			}

			_iSP_Vars = iSave;
			break;
		}
		case NOP_PTRCALL:
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			pFunctionPtr = GetPtrFunction(GetVarPtr(n1), GetVarPtr(n2));
			if (pFunctionPtr != NULL)
			{
				int iSave = _iSP_Vars;
				_iSP_Vars = iSP_VarsMax;

				if ((pFunctionPtr->_fn)(this, pFunctionPtr->_func, n3) < 0)
				{
					SetError("Ptr Call Argument Count Error");
					return false;
				}

				_iSP_Vars = iSave;
			}
			else
			{
				SetError("Ptr Call Not Found");
				return false;
			}
			break;
		case NOP_RETURN:
			n1 = GetS16();
			if (n1 == 0)
				Var_Release(&m_sVarStack[_iSP_Vars]); // Clear
			else
				Move(&m_sVarStack[_iSP_Vars], GetVarPtr(n1));

			if (m_sCallStack.empty())
			{
				return true;
			}
			iTemp = (int)m_sCallStack.size() - 1;
			callStack = m_sCallStack[iTemp];
			m_sCallStack.resize(iTemp);

			SetCodePtr(callStack._iReturnOffset);
			_iSP_Vars = callStack._iSP_Vars;
			iSP_VarsMax = callStack._iSP_VarsMax;
			break;
		case NOP_TABLE_ALLOC:
			n1 = GetS16();
			Var_SetTable(GetVarPtr(n1), TableAlloc());
			break;
		case NOP_TABLE_INSERT:
		{
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			TableInsert(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		}
		case NOP_TABLE_READ:
		{
			n1 = GetS16(); n2 = GetS16(); n3 = GetS16();
			TableRead(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		}
		default:
			DebugLog("Error OP Type Error (%d)", op);
			break;
		}
	}
	return true;
}

static void ReadString(CNArchive& ar, std::string& str)
{
	short nLen;
	ar >> nLen;
	str.resize(nLen);

	ar.Read((char*)str.data(), nLen);
}

void		CNeoVM::ReleaseVM(CNeoVM* pVM)
{
	delete pVM;
}

CNeoVM* CNeoVM::LoadVM(void* pBuffer, int iSize)
{
	CNArchive ar(pBuffer, iSize);
	CNeoVM* pVM = new CNeoVM(50 * 1024);

	SNeoVMHeader header;
	memset(&header, 0, sizeof(header));
	ar >> header;
	pVM->_header = header;

	if (header._dwFileType != FILE_NEOS)
	{
		return NULL;
	}
	if (header._dwNeoVersion != NEO_VER)
	{
		return NULL;
	}


	u8* pCode = new u8[header._iCodeSize];
	pVM->SetCodeData(pCode, header._iCodeSize);
	ar.Read(pCode, header._iCodeSize);

	pVM->m_sFunctionPtr.resize(header._iFunctionCount);

	int iID;
	SFunctionTable fun;
	std::string funName;
	for (int i = 0; i < header._iFunctionCount; i++)
	{
		memset(&fun, 0, sizeof(SFunctionTable));

		ar >> iID >> fun._codePtr >> fun._argsCount >> fun._localTempMax >> fun._localVarCount >> fun._funType;
		if (fun._funType != FUNT_NORMAL)
		{
			ReadString(ar, funName);
			pVM->m_sImExportTable[funName] = iID;
		}

		fun._localAddCount = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;
		pVM->m_sFunctionPtr[iID] = fun;
	}

	std::string tempStr;
	pVM->m_sVarGlobal.resize(header._iStaticVarCount);
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		VarInfo& vi = pVM->m_sVarGlobal[i];
		pVM->Var_Release(&vi);

		VAR_TYPE type;
		ar >> type;
		vi.SetType(type);
		switch (type)
		{
		case VAR_INT:
			ar >> vi._int;
			break;
		case VAR_FLOAT:
			ar >> vi._float;
			break;
		case VAR_BOOL:
			ar >> vi._bl;
			break;
		case VAR_STRING:
			ReadString(ar, tempStr);
			vi._str = pVM->StringAlloc(tempStr.c_str());
			break;
		default:
			DebugLog("Error VAR Type Error (%d)", type);
			break;
		}
	}

	pVM->Init();

	return pVM;
}



bool CNeoVM::RunFunction(const std::string& funName)
{
	auto it = m_sImExportTable.find(funName);
	if (it == m_sImExportTable.end())
		return false;

	int iID = (*it).second;
	Run(iID);

	return true;
}
