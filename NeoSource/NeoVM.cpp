// Neo1.00.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "NeoVM.h"

void	DebugLog(LPCSTR	lpszString, ...);

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
		Var_Release(d);
		d->SetType(VAR_INT);
	}
	d->_int = v;
}

void CNeoVM::Var_SetFloat(VarInfo *d, double v)
{
	if (d->GetType() != VAR_FLOAT)
	{
		Var_Release(d);
		d->SetType(VAR_FLOAT);
	}
	d->_float = v;
}
void CNeoVM::Var_SetBool(VarInfo *d, bool v)
{
	if (d->GetType() != VAR_BOOL)
	{
		Var_Release(d);
		d->SetType(VAR_BOOL);
	}
	d->_bl = v;
}



void CNeoVM::Var_SetString(VarInfo *d, const char* str)
{
	Var_Release(d);

	d->SetType(VAR_STRING);
	d->_str = StringAlloc(str);
	++d->_str->_refCount;
}
void CNeoVM::Var_SetTable(VarInfo *d, TableInfo* p)
{
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
		v1->_str->_refCount++;
		break;
	case VAR_TABLE:
		Var_Release(v1);
		v1->SetType(v2->GetType());
		v1->_tbl = v2->_tbl;
		v1->_tbl->_refCount++;
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
			r->_float += v2->_float;
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
			r->_float -= v2->_float;
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
			r->_float *= v2->_float;
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
			r->_float /= v2->_float;
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

BOOL CNeoVM::CompareEQ(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_int == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int == v2->_float;
		return FALSE;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float == v2->_float;
		return FALSE;
	}
	return FALSE;
}
BOOL CNeoVM::CompareGR(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_int > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int > v2->_float;
		return FALSE;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float > v2->_float;
		return FALSE;
	}
	return FALSE;
}
BOOL CNeoVM::CompareGE(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_INT)
	{
		if(v2->GetType() == VAR_INT)
			return v1->_int >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int >= v2->_float;
		return FALSE;
	}
	else if (v1->GetType() == VAR_FLOAT)
	{
		if (v2->GetType() == VAR_INT)
			return v1->_float >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float >= v2->_float;
		return FALSE;
	}
	return FALSE;
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
		sprintf_s(ch, _countof(ch), "%d", v1->_int);
		return ch;
	case VAR_FLOAT:
		sprintf_s(ch, _countof(ch), "%lf", v1->_float);
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
		return atoi(v1->_str->_str.c_str());
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


BOOL	CNeoVM::Run(int iFunctionID)
{
	SFunctionTable fun = m_sFunctionPtr[iFunctionID];
	int iArgs = (int)_args.size();
	if (iArgs != fun._argsCount)
		return FALSE;

	m_sCode.SetPointer(fun._codePtr, SEEK_SET);

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
		m_sCode >> op;
		switch (op)
		{
		case NOP_ADD2:
			m_sCode >> n1 >> n2;
			Add2(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_SUB2:
			m_sCode >> n1 >> n2;
			Sub2(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_MUL2:
			m_sCode >> n1 >> n2;
			Mul2(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_DIV2:
			m_sCode >> n1 >> n2;
			Div2(GetVarPtr(n1), GetVarPtr(n2));
			break;

		case NOP_ADD3:
			m_sCode >> n1 >> n2 >> n3;
			Add(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		case NOP_SUB3:
			m_sCode >> n1 >> n2 >> n3;
			Sub(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		case NOP_MUL3:
			m_sCode >> n1 >> n2 >> n3;
			Mul(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		case NOP_DIV3:
			m_sCode >> n1 >> n2 >> n3;
			Div(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;

		case NOP_INC:
			m_sCode >> n1;
			Inc(GetVarPtr(n1));
			break;
		case NOP_DEC:
			m_sCode >> n1;
			Dec(GetVarPtr(n1));
			break;

		case NOP_GREAT:		// >
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), CompareGR(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_GREAT_EQ:	// >=
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), CompareGE(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_LESS:		// <
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), CompareGR(GetVarPtr(n3), GetVarPtr(n2)));
			break;
		case NOP_LESS_EQ:	// <=
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), CompareGE(GetVarPtr(n3), GetVarPtr(n2)));
			break;
		case NOP_EQUAL2:	// ==
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), CompareEQ(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_NEQUAL:	// !=
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), !CompareEQ(GetVarPtr(n2), GetVarPtr(n3)));
			break;
		case NOP_AND:	// &&
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), GetVarPtr(n3)->IsTrue() && GetVarPtr(n2)->IsTrue());
			break;
		case NOP_OR:	// ||
			m_sCode >> n1 >> n2 >> n3;
			Var_SetBool(GetVarPtr(n1), GetVarPtr(n2)->IsTrue() || GetVarPtr(n3)->IsTrue());
			break;
		case NOP_STR_ADD:	// ..
			m_sCode >> n1 >> n2 >> n3;
			Var_SetString(GetVarPtr(n1), (ToString(GetVarPtr(n2)) + ToString(GetVarPtr(n3))).c_str());
			break;

		case NOP_TOSTRING:
			m_sCode >> n1 >> n2;
			Var_SetString(GetVarPtr(n1), ToString(GetVarPtr(n2)).c_str());
			break;
		case NOP_TOINT:
			m_sCode >> n1 >> n2;
			Var_SetInt(GetVarPtr(n1), ToInt(GetVarPtr(n2)));
			break;
		case NOP_TOFLOAT:
			m_sCode >> n1 >> n2;
			Var_SetFloat(GetVarPtr(n1), ToFloat(GetVarPtr(n2)));
			break;
		case NOP_GETTYPE:
			m_sCode >> n1 >> n2;
			Var_SetString(GetVarPtr(n1), GetType(GetVarPtr(n2)).c_str());
			break;

		case NOP_JMP:
			m_sCode >> n1;
			m_sCode.SetPointer(n1, SEEK_CUR);
			break;
		case NOP_JMP_FALSE:
			m_sCode >> n1 >> n2;
			if (FALSE == GetVarPtr(n1)->IsTrue())
				m_sCode.SetPointer(n2, SEEK_CUR);
			break;
		case NOP_JMP_TRUE:
			m_sCode >> n1 >> n2;
			if (TRUE == GetVarPtr(n1)->IsTrue())
				m_sCode.SetPointer(n2, SEEK_CUR);
			break;

		case NOP_MOV:
			m_sCode >> n1 >> n2;
			Move(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_MOV_MINUS:
			m_sCode >> n1 >> n2;
			MoveMinus(GetVarPtr(n1), GetVarPtr(n2));
			break;
		case NOP_FARCALL:
		{
			m_sCode >> n1 >> n2;

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
				return FALSE;
			}

			_iSP_Vars = iSave;
			break;
		}
		case NOP_CALL:
			m_sCode >> n1 >> n2;
			callStack._iReturnOffset = m_sCode.GetBufferOffset();
			callStack._iSP_Vars = _iSP_Vars;
			callStack._iSP_VarsMax = iSP_VarsMax;
			m_sCallStack.push_back(callStack);

			fun = m_sFunctionPtr[n1];
			m_sCode.SetPointer(fun._codePtr, SEEK_SET);
			_iSP_Vars = iSP_VarsMax;
			iSP_VarsMax = _iSP_Vars + fun._localAddCount;
			if (_iSP_Vars_Max2 < iSP_VarsMax)
				_iSP_Vars_Max2 = iSP_VarsMax;
			break;
		case NOP_PTRCALL:
			m_sCode >> n1 >> n2 >> n3;
			pFunctionPtr = GetPtrFunction(GetVarPtr(n1), GetVarPtr(n2));
			if (pFunctionPtr != NULL)
			{
				int iSave = _iSP_Vars;
				_iSP_Vars = iSP_VarsMax;

				if ((pFunctionPtr->_fn)(this, pFunctionPtr->_func, n3) < 0)
				{
					SetError("Ptr Call Argument Count Error");
					return FALSE;
				}

				_iSP_Vars = iSave;
			}
			else
			{
				SetError("Ptr Call Not Found");
				return FALSE;
			}
			break;
		case NOP_RETURN:
			m_sCode >> n1;
			if (n1 == 0)
				Var_Release(&m_sVarStack[_iSP_Vars]); // Clear
			else
				Move(&m_sVarStack[_iSP_Vars], GetVarPtr(n1));

			if (m_sCallStack.empty())
			{
				return TRUE;
			}
			iTemp = (int)m_sCallStack.size() - 1;
			callStack = m_sCallStack[iTemp];
			m_sCallStack.resize(iTemp);

			m_sCode.SetPointer(callStack._iReturnOffset, SEEK_SET);
			_iSP_Vars = callStack._iSP_Vars;
			iSP_VarsMax = callStack._iSP_VarsMax;
			break;
		case NOP_TABLE_ALLOC:
			m_sCode >> n1;
			Var_SetTable(GetVarPtr(n1), TableAlloc());
			break;
		case NOP_TABLE_INSERT:
		{
			m_sCode >> n1 >> n2 >> n3;
			TableInsert(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		}
		case NOP_TABLE_READ:
		{
			m_sCode >> n1 >> n2 >> n3;
			TableRead(GetVarPtr(n1), GetVarPtr(n2), GetVarPtr(n3));
			break;
		}
		default:
			DebugLog("Error OP Type Error (%d)", op);
			break;
		}
	}
	return TRUE;
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


	BYTE* pCode = new BYTE[header._iCodeSize];
	pVM->m_sCode.SetData(pCode, header._iCodeSize);
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



BOOL CNeoVM::RunFunction(const std::string& funName)
{
	auto it = m_sImExportTable.find(funName);
	if (it == m_sImExportTable.end())
		return FALSE;

	int iID = (*it).second;
	Run(iID);

	return TRUE;
}
