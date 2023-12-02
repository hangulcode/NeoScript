#pragma once

#if defined(_MSC_VER) && !defined(_DEBUG)
#define NEOS_FORCEINLINE __forceinline
#elif defined(__GNUC__) && __GNUC__ >= 4 && defined(NDEBUG)
#define NEOS_FORCEINLINE __attribute__((always_inline))
#else
#define NEOS_FORCEINLINE
#endif


NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, VarInfo* pKey, VarInfo* pValue)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		if (pValue->GetType() == VAR_NONE)
		{
			TableRemove(pClt, pKey);
			return;
		}
		pClt->_tbl->Insert(pKey, pValue);
		break;
	case VAR_LIST:
		if (pKey->GetType() != VAR_INT)
		{
			SetError("Collision Insert Error");
			return;
		}
		pClt->_lst->SetValue(pKey->_int, pValue);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}
NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, int key, VarInfo* v)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pClt->_tbl->Insert(key, v);
		break;
	case VAR_LIST:
		pClt->_lst->SetValue(key, v);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}

NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, VarInfo* pKey, int v)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pClt->_tbl->Insert(pKey, v);
		break;
	case VAR_LIST:
		pClt->_lst->SetValue(pKey->_int, v);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}
NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, int key, int v)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pClt->_tbl->Insert(key, v);
		break;
	case VAR_LIST:
		pClt->_lst->SetValue(key, v);
		break;
	default:
		SetError("Collision Insert Error");
		return;
	}
}
NEOS_FORCEINLINE VarInfo* CNeoVMWorker::GetTableItem(VarInfo* pTable, VarInfo* pKey)
{
	if (pTable->GetType() != VAR_MAP)
	{
		SetError("TableRead Error");
		return NULL;
	}

	return pTable->_tbl->Find(pKey);
}
NEOS_FORCEINLINE VarInfo* CNeoVMWorker::GetTableItemValid(VarInfo* pTable, VarInfo* pKey)
{
	VarInfo* r = GetTableItem(pTable, pKey);
	if (r != NULL)
		return r;
	SetError("Table Key Not Found");
	return NULL;
}
NEOS_FORCEINLINE VarInfo* CNeoVMWorker::GetTableItemValid(VarInfo* pTable, int Array)
{
	VarInfo Key(Array);
	VarInfo* r = GetTableItem(pTable, &Key);
	if (r != NULL)
		return r;
	SetError("Table Key Not Found");
	return NULL;
}

NEOS_FORCEINLINE void CNeoVMWorker::CltRead(VarInfo* pClt, VarInfo* pKey, VarInfo* pValue)
{
	VarInfo* pFind;
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pFind = GetTableItem(pClt, pKey);
		if (pFind)
			Move(pValue, pFind);
		else
			Var_Release(pValue);
		break;
	case VAR_LIST:
		if (pKey->GetType() != VAR_INT)
		{
			SetError("Collision Read Error");
			return;
		}
		if (false == pClt->_lst->GetValue(pKey->_int, pValue))
			SetError("Collision Read Error");
		break;
	default:
		SetError("Collision Read Error");
		return;
	}
}

NEOS_FORCEINLINE void CNeoVMWorker::TableRemove(VarInfo* pTable, VarInfo* pKey)
{
	if (pTable->GetType() != VAR_MAP)
	{
		SetError("TableRead Error");
		return;
	}

	pTable->_tbl->Remove(pKey);
}

NEOS_FORCEINLINE void CNeoVMWorker::Swap(VarInfo* v1, VarInfo* v2)
{
	VarInfo* p = v1;
	v1 = v2;
	v2 = p;
}



NEOS_FORCEINLINE void CNeoVMWorker::MoveMinus(VarInfo* v1, VarInfo* v2)
{
	switch (v2->GetType())
	{
	case VAR_INT:
		Var_SetInt(v1, -v2->_int);
		return;
	case VAR_FLOAT:
		Var_SetFloat(v1, -v2->_float);
		return;
	default:
		break;
	}
	SetError("Minus Error");
}

NEOS_FORCEINLINE void CNeoVMWorker::Add2(eNOperationSub op, VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: r->_int += v2->_int; break;
			case eOP_SUB: r->_int -= v2->_int; break;
			case eOP_MUL: r->_int *= v2->_int; break;
			case eOP_DIV: r->_int /= v2->_int; break;
			case eOP_PER: r->_int %= v2->_int; break;
			case eOP_LSH: r->_int <<= v2->_int; break;
			case eOP_RSH: r->_int >>= v2->_int; break;
			case eOP_AND: r->_int &= v2->_int; break;
			case eOP__OR: r->_int |= v2->_int; break;
			case eOP_XOR: r->_int ^= v2->_int; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			switch (op)
			{
			case eOP_ADD: r->_float = (double)r->_int + v2->_float; break;
			case eOP_SUB: r->_float = (double)r->_int - v2->_float; break;
			case eOP_MUL: r->_float = (double)r->_int * v2->_float; break;
			case eOP_DIV: r->_float = (double)r->_int / v2->_float; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: r->_float += v2->_int; break;
			case eOP_SUB: r->_float -= v2->_int; break;
			case eOP_MUL: r->_float *= v2->_int; break;
			case eOP_DIV: r->_float /= v2->_int; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			switch (op)
			{
			case eOP_ADD: r->_float += v2->_float; break;
			case eOP_SUB: r->_float -= v2->_float; break;
			case eOP_MUL: r->_float *= v2->_float; break;
			case eOP_DIV: r->_float /= v2->_float; break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_CHAR:
		if (v2->GetType() == VAR_CHAR)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, std::string(r->_c.c) + std::string(v2->_c.c)); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_STRING)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, std::string(r->_c.c) + v2->_str->_str); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_CHAR)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, r->_str->_str + v2->_c.c); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_STRING)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, r->_str->_str + v2->_str->_str); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_MAP:
		switch (op)
		{
		case eOP_ADD: if (Call_MetaTable(r, g_meta_Add2, r, r, v2)) return; break;
		case eOP_SUB: if (Call_MetaTable(r, g_meta_Sub2, r, r, v2)) return; break;
		case eOP_MUL: if (Call_MetaTable(r, g_meta_Div2, r, r, v2)) return; break;
		case eOP_DIV: if (Call_MetaTable(r, g_meta_Per2, r, r, v2)) return; break;
		default: SetError("operator Error"); break;
		}
		break;
	default:
		break;
	}
	SetError("+= Error");
}

NEOS_FORCEINLINE void CNeoVMWorker::MoveMinusI(VarInfo* v1, int v)
{
	Var_SetInt(v1, -v);
}

NEOS_FORCEINLINE void CNeoVMWorker::And(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_BOOL:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, (int)v1->_bl & v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetBool(r, v1->_bl & v2->_bl);
			return;
		}
		break;
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int & v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetInt(r, v1->_int & (int)v2->_bl);
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		if (neo_DCalllibs::Set_And(this, r, v1, v2))
			return;
		break;
	default:
		break;
	}
	SetError("& Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Or(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_BOOL:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, (int)v1->_bl | v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetBool(r, v1->_bl | v2->_bl);
			return;
		}
		break;
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int | v2->_int);
			return;
		}
		else if (v2->GetType() == VAR_BOOL)
		{
			Var_SetInt(r, v1->_int | (int)v2->_bl);
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		if (neo_DCalllibs::Set_Or(this, r, v1, v2))
			return;
		break;
	default:
		break;
	}
	SetError("| Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetInt(r, v1->_int + v2->_int); break;
			case eOP_SUB: Var_SetInt(r, v1->_int - v2->_int); break;
			case eOP_MUL: Var_SetInt(r, v1->_int * v2->_int); break;
			case eOP_DIV: Var_SetInt(r, v1->_int / v2->_int); break;
			case eOP_PER: Var_SetInt(r, v1->_int % v2->_int); break;
			case eOP_LSH: Var_SetInt(r, v1->_int << v2->_int); break;
			case eOP_RSH: Var_SetInt(r, v1->_int >> v2->_int); break;
			case eOP_AND: Var_SetInt(r, v1->_int & v2->_int); break;
			case eOP__OR: Var_SetInt(r, v1->_int | v2->_int); break;
			case eOP_XOR: Var_SetInt(r, v1->_int ^ v2->_int); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetFloat(r, v1->_int + v2->_float); break;
			case eOP_SUB: Var_SetFloat(r, v1->_int - v2->_float); break;
			case eOP_MUL: Var_SetFloat(r, v1->_int * v2->_float); break;
			case eOP_DIV: Var_SetFloat(r, v1->_int / v2->_float); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetFloat(r, v1->_float + v2->_int); break;
			case eOP_SUB: Var_SetFloat(r, v1->_float - v2->_int); break;
			case eOP_MUL: Var_SetFloat(r, v1->_float * v2->_int); break;
			case eOP_DIV: Var_SetFloat(r, v1->_float / v2->_int); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetFloat(r, v1->_float + v2->_float); break;
			case eOP_SUB: Var_SetFloat(r, v1->_float - v2->_float); break;
			case eOP_MUL: Var_SetFloat(r, v1->_float * v2->_float); break;
			case eOP_DIV: Var_SetFloat(r, v1->_float / v2->_float); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_CHAR:
		if (v2->GetType() == VAR_CHAR)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, std::string(r->_c.c) + v2->_c.c); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_STRING)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, std::string(r->_c.c) + v2->_str->_str); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_CHAR)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, r->_str->_str + v2->_c.c); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		else if (v2->GetType() == VAR_STRING)
		{
			switch (op)
			{
			case eOP_ADD: Var_SetStringA(r, r->_str->_str + v2->_str->_str); break;
			default: SetError("operator Error"); break;
			}
			return;
		}
		break;
	case VAR_MAP:
		switch (op)
		{
		case eOP_ADD:
			if (Call_MetaTable(v1, g_meta_Add3, r, v1, v2)) return;
			SetError("unsupported operand + Error"); break;
		case eOP_SUB:
			if (Call_MetaTable(v1, g_meta_Sub3, r, v1, v2)) return;
			SetError("unsupported operand - Error"); break;
		case eOP_MUL:
			if (Call_MetaTable(v1, g_meta_Mul3, r, v1, v2)) return;
			SetError("unsupported operand * Error"); break;
		case eOP_DIV:
			if (Call_MetaTable(v1, g_meta_Div3, r, v1, v2)) return;
			SetError("unsupported operand / Error"); break;
		case eOP_PER:
			if (Call_MetaTable(v1, g_meta_Per3, r, v1, v2)) return;
			SetError("unsupported operand % Error"); break;
		case eOP_LSH: SetError("unsupported operand << Error"); break;
		case eOP_RSH: SetError("unsupported operand >> Error"); break;
		default:
			break;
		}
		break;
	case VAR_LIST:
		switch (op)
		{
		case eOP_ADD:
			if (neo_DCalllibs::List_Add(this, r, v1, v2)) return;
			SetError("unsupported operand + Error"); break;
		default: SetError("operator Error"); break;
		}
		break;
	case VAR_SET:
		switch (op)
		{
		case eOP_ADD:
			SetError("unsupported operand + Error"); break;
		case eOP_SUB:
			if (neo_DCalllibs::Set_Sub(this, r, v1, v2)) return;
			SetError("unsupported operand - Error"); break;
		default: SetError("operator Error"); break;
		}
		return;
		break;
	default:
		break;
	}
	SetError("unsupported operand Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, VarInfo* v1, int v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		switch (op)
		{
		case eOP_ADD: Var_SetInt(r, v1->_int + v2); break;
		case eOP_SUB: Var_SetInt(r, v1->_int - v2); break;
		case eOP_MUL: Var_SetInt(r, v1->_int * v2); break;
		case eOP_DIV: Var_SetInt(r, v1->_int / v2); break;
		case eOP_PER: Var_SetInt(r, v1->_int % v2); break;
		case eOP_LSH: Var_SetInt(r, v1->_int << v2); break;
		case eOP_RSH: Var_SetInt(r, v1->_int >> v2); break;
		case eOP_AND: Var_SetInt(r, v1->_int & v2); break;
		case eOP__OR: Var_SetInt(r, v1->_int | v2); break;
		case eOP_XOR: Var_SetInt(r, v1->_int ^ v2); break;
		default: SetError("operator Error"); break;
		}
		return;
	case VAR_FLOAT:
		switch (op)
		{
		case eOP_ADD: Var_SetFloat(r, v1->_float + v2); break;
		case eOP_SUB: Var_SetFloat(r, v1->_float - v2); break;
		case eOP_MUL: Var_SetFloat(r, v1->_float * v2); break;
		case eOP_DIV: Var_SetFloat(r, v1->_float / v2); break;
		default: SetError("operator Error"); break;
		}
		return;
	case VAR_STRING:
		break;
	case VAR_MAP:
	{
		VarInfo vv2 = v2;
		switch (op)
		{
		case eOP_ADD:
			if (Call_MetaTable(v1, g_meta_Add3, r, v1, &vv2)) return;
			SetError("unsupported operand + Error"); break;
		case eOP_SUB:
			if (Call_MetaTable(v1, g_meta_Sub3, r, v1, &vv2)) return;
			SetError("unsupported operand - Error"); break;
		case eOP_MUL:
			if (Call_MetaTable(v1, g_meta_Mul3, r, v1, &vv2)) return;
			SetError("unsupported operand * Error"); break;
		case eOP_DIV:
			if (Call_MetaTable(v1, g_meta_Div3, r, v1, &vv2)) return;
			SetError("unsupported operand / Error"); break;
		case eOP_PER:
			if (Call_MetaTable(v1, g_meta_Per3, r, v1, &vv2)) return;
			SetError("unsupported operand % Error"); break;
		default: SetError("operator Error"); break;
		}
		break;
	}
	case VAR_LIST:
	{
		VarInfo vv2 = v2;
		switch (op)
		{
		case eOP_ADD:
			if (neo_DCalllibs::List_Add(this, r, v1, &vv2)) return;
			SetError("unsupported operand + Error"); break;
		default: SetError("operator Error"); break;
		}
		break;
	}
	case VAR_SET:
	{
		VarInfo vv2 = v2;
		switch (op)
		{
		case eOP_ADD:
			SetError("unsupported operand + Error"); break;
		case eOP_SUB:
			if (neo_DCalllibs::Set_Sub(this, r, v1, &vv2)) return;
			SetError("unsupported operand - Error"); break;
		default: SetError("operator Error"); break;
		}
		return;
	}
	default:
		break;
	}
	SetError("unsupported operand Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, int v1, VarInfo* v2)
{
	if (v2->GetType() == VAR_INT)
	{
		switch (op)
		{
		case eOP_ADD: Var_SetInt(r, v1 + v2->_int); break;
		case eOP_SUB: Var_SetInt(r, v1 - v2->_int); break;
		case eOP_MUL: Var_SetInt(r, v1 * v2->_int); break;
		case eOP_DIV: Var_SetInt(r, v1 / v2->_int); break;
		case eOP_PER: Var_SetInt(r, v1 % v2->_int); break;
		case eOP_LSH: Var_SetInt(r, v1 << v2->_int); break;
		case eOP_RSH: Var_SetInt(r, v1 >> v2->_int); break;
		case eOP_AND: Var_SetInt(r, v1 & v2->_int); break;
		case eOP__OR: Var_SetInt(r, v1 | v2->_int); break;
		case eOP_XOR: Var_SetInt(r, v1 ^ v2->_int); break;
		default: SetError("operator Error"); break;
		}
		return;
	}
	else if (v2->GetType() == VAR_FLOAT)
	{
		switch (op)
		{
		case eOP_ADD: Var_SetFloat(r, v1 + v2->_float); break;
		case eOP_SUB: Var_SetFloat(r, v1 - v2->_float); break;
		case eOP_MUL: Var_SetFloat(r, v1 * v2->_float); break;
		case eOP_DIV: Var_SetFloat(r, v1 / v2->_float); break;
		default: SetError("operator Error"); break;
		}
		return;
	}
	SetError("unsupported operand Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Add(eNOperationSub op, VarInfo* r, int v1, int v2)
{
	switch (op)
	{
	case eOP_ADD: Var_SetInt(r, v1 + v2); return;
	case eOP_SUB: Var_SetInt(r, v1 - v2); return;
	case eOP_MUL: Var_SetInt(r, v1 * v2); return;
	case eOP_DIV: Var_SetInt(r, v1 / v2); return;
	case eOP_PER: Var_SetInt(r, v1 % v2); return;
	case eOP_LSH: Var_SetInt(r, v1 << v2); return;
	case eOP_RSH: Var_SetInt(r, v1 >> v2); return;
	case eOP_AND: Var_SetInt(r, v1 & v2); break;
	case eOP__OR: Var_SetInt(r, v1 | v2); break;
	case eOP_XOR: Var_SetInt(r, v1 ^ v2); break;
	default: SetError("operator Error"); break;
	}
	SetError("unsupported operand Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Inc(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		++v1->_int;
		return;
	case VAR_FLOAT:
		++v1->_float;
		return;
	default:
		break;
	}
	SetError("++ Error");
}
NEOS_FORCEINLINE void CNeoVMWorker::Dec(VarInfo* v1)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		--v1->_int;
		return;
	case VAR_FLOAT:
		--v1->_float;
		return;
	default:
		break;
	}
	SetError("-- Error");
}

NEOS_FORCEINLINE bool CNeoVMWorker::CompareEQ(VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_NONE:
		if (v2->GetType() == VAR_NONE)
			return true;
		break;
	case VAR_BOOL:
		if (v2->GetType() == VAR_BOOL)
			return v1->_bl == v2->_bl;
		break;
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
			return v1->_int == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int == v2->_float;
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
			return v1->_float == v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float == v2->_float;
		break;
	case VAR_CHAR:
		if (v2->GetType() == VAR_CHAR)
			return 0 == strcmp(v1->_c.c, v2->_c.c);
		else if (v2->GetType() == VAR_STRING)
			return 0 == strcmp(v1->_c.c, v2->_str->_str.c_str());
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_CHAR)
			return 0 == strcmp(v1->_str->_str.c_str(), v2->_c.c);
		else if (v2->GetType() == VAR_STRING)
			return v1->_str->_str == v2->_str->_str;
		break;
	default:
		break;
	}
	return false;
}
NEOS_FORCEINLINE bool CNeoVMWorker::CompareGR(VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
			return v1->_int > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int > v2->_float;
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
			return v1->_float > v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float > v2->_float;
		break;
	case VAR_CHAR:
		if (v2->GetType() == VAR_CHAR)
			return std::string(v1->_c.c) > std::string(v2->_c.c);
		else if (v2->GetType() == VAR_STRING)
			return std::string(v1->_c.c) > v2->_str->_str;
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_CHAR)
			return v1->_str->_str > std::string(v2->_c.c);
		else if (v2->GetType() == VAR_STRING)
			return v1->_str->_str > v2->_str->_str;
		break;
	default:
		break;
	}
	SetError("CompareGR Error");
	return false;
}
NEOS_FORCEINLINE bool CNeoVMWorker::CompareGE(VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
			return v1->_int >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_int >= v2->_float;
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
			return v1->_float >= v2->_int;
		if (v2->GetType() == VAR_FLOAT)
			return v1->_float >= v2->_float;
		break;
	case VAR_CHAR:
		if (v2->GetType() == VAR_CHAR)
			return std::string(v1->_c.c) >= std::string(v2->_c.c);
		else if (v2->GetType() == VAR_STRING)
			return std::string(v1->_c.c) >= v2->_str->_str;
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_CHAR)
			return v1->_str->_str >= std::string(v2->_c.c);
		else if (v2->GetType() == VAR_STRING)
			return v1->_str->_str >= v2->_str->_str;
		break;
	default:
		break;
	}
	SetError("CompareGE Error");
	return false;
}

NEOS_FORCEINLINE void CNeoVMWorker::Add22(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int += v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->SetType(VAR_FLOAT);
			r->_float = (double)r->_int + v2->_float;
			return;
		}
		break;
	case VAR_FLOAT:
		if (v2->GetType() == VAR_INT)
		{
			r->_float += v2->_int;
			return;
		}
		else if (v2->GetType() == VAR_FLOAT)
		{
			r->_float += v2->_float;
			return;
		}
		break;
	case VAR_CHAR:
		if (v2->GetType() == VAR_CHAR)
		{
			Var_SetStringA(r, std::string(r->_c.c) + std::string(v2->_c.c));
			return;
		}
		else if (v2->GetType() == VAR_STRING)
		{
			Var_SetStringA(r, std::string(r->_c.c) + v2->_str->_str);
			return;
		}
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_CHAR)
		{
			Var_SetStringA(r, r->_str->_str + v2->_c.c);
			return;
		}
		else if (v2->GetType() == VAR_STRING)
		{
			Var_SetStringA(r, r->_str->_str + v2->_str->_str);
			return;
		}
		break;
	case VAR_MAP:
		if (Call_MetaTable(r, g_meta_Add2, r, r, v2)) 
		return;
//		break;
	default:
		break;
	}
	SetError("+= Error");
}

NEOS_FORCEINLINE bool CNeoVMWorker::For(VarInfo* pCur)
{
#ifdef _DEBUG
	VarInfo* pCur_Inter = pCur + 1;
	VarInfo* pBegin = pCur + 2;
	VarInfo* pEnd = pCur + 3;
	VarInfo* pStep = pCur + 4;

	Move(pCur, pCur_Inter);
	if (pCur->_int < pEnd->_int)
	{
		pCur_Inter->_int += pStep->_int;
		return true;
	}
	return false;
#else
	VarInfo* pCur_Inter = pCur + 1;

	if (pCur->GetType() == VAR_INT)
		pCur->_int = pCur_Inter->_int;
	else
		Move(pCur, pCur_Inter);
	if (pCur->_int < pCur[3]._int)
	{
		pCur_Inter->_int += pCur[4]._int;
		return true;
	}
	return false;
#endif
}
NEOS_FORCEINLINE bool CNeoVMWorker::ForEach(VarInfo* pClt, VarInfo* pKey)
{
	VarInfo* pValue = pKey + 1;
	VarInfo* pIterator = pKey + 2;

	switch (pClt->GetType())
	{
	case VAR_CHAR:
	{
		int str_len = (pClt->_c.c[0] == 0) ? 0 : 1;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < str_len)
			{
				pIterator->_it._iStringOffset = 0;
				pIterator->SetType(VAR_ITERATOR);
			}
			else
				return false;
		}
		if (pIterator->_it._iStringOffset < str_len)
		{
			Var_SetString(pKey, pClt->_c);
			return true;
		}
		else
		{
			pIterator->ClearType();
			return false;
		}
		break;
	}
	case VAR_STRING:
	{
		std::string* str = &pClt->_str->_str;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < (int)str->length())
			{
				pIterator->_it._iStringOffset = 0;
				pIterator->SetType(VAR_ITERATOR);
			}
			else
				return false;
		}

		if (pIterator->_it._iStringOffset < (int)str->length())
		{
			SUtf8One s = utf_string::UTF8_ONE(*str, pIterator->_it._iStringOffset);
			Var_SetString(pKey, s);
			return true;
		}
		else
		{
			pIterator->ClearType();
			return false;
		}
		break;
	}
	case VAR_MAP:
	{
		MapInfo* tbl = pClt->_tbl;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < tbl->GetCount())
			{
				pIterator->_it = tbl->FirstNode();
				pIterator->SetType(VAR_ITERATOR);
			}
			else
				return false;
		}
		else
			tbl->NextNode(pIterator->_it);

		MapNode* n = pIterator->_it._pTableNode;
		if (n)
		{
			Move(pKey, &n->key);
			Move(pValue, &n->value);
			return true;
		}
		else
		{
			pIterator->ClearType();
			return false;
		}
		break;
	}
	case VAR_LIST:
	{
		ListInfo* lst = pClt->_lst;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < lst->GetCount())
			{
				pIterator->_it._iListOffset = 0;
				pIterator->SetType(VAR_ITERATOR);
			}
			else
				return false;
		}
		else
			++pIterator->_it._iListOffset;

		if (pIterator->_it._iListOffset < lst->GetCount())
		{
			lst->GetValue(pIterator->_it._iListOffset, pKey);
			//Move(pValue, &n->value);
			return true;
		}
		else
		{
			pIterator->ClearType();
			return false;
		}
		break;
	}
	case VAR_SET:
	{
		SetInfo* set = pClt->_set;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < set->GetCount())
			{
				pIterator->_it = set->FirstNode();
				pIterator->SetType(VAR_ITERATOR);
			}
			else
				return false;
		}
		else
			set->NextNode(pIterator->_it);

		SetNode* n = pIterator->_it._pSetNode;
		if (n)
		{
			Move(pKey, &n->key);
			return true;
		}
		else
		{
			pIterator->ClearType();
			return false;
		}
		break;
	}
	default:
		break;
	}
	SetErrorFormat("error : foreach not support '%s'", GetDataType(pClt->GetType()).c_str());
	return false;

	//	SetError("foreach table key Error");
	//	return false;
}