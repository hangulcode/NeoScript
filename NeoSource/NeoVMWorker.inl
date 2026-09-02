#pragma once

NEOS_FORCEINLINE bool CNeoVMWorker::EnsureStackRange(int base, int lastOffset)
{
	// SetVarStackBase 가 유지하는 캐시. 옛 코드는 여기서 m_pVarStack_Base->size() 를
	// 읽어 의존 로드가 두 단이었다 — 호출 빈도가 높아 그 자체로 비용이었다.
	const int stackSize = m_iVarStackSize;
	if (base > stackSize - 1 - lastOffset)
	{
		SetError(RTE_CALL_STACK_OVERFLOW);
		return false;
	}
	return true;
}


NEOS_FORCEINLINE void CNeoVMWorker::MoveI(VarInfo* v1, int v)
{
	if (v1->IsAllocType())
		Var_ReleaseAlloc(v1);

	v1->SetType(VAR_INT);
	v1->_int = v;
}

NEOS_FORCEINLINE void CNeoVMWorker::MoveF(VarInfo* v1, int bits)
{
	if (v1->IsAllocType())
		Var_ReleaseAlloc(v1);

	v1->SetType(VAR_FLOAT);
	std::memcpy(&v1->_float, &bits, sizeof(bits));
}

NEOS_FORCEINLINE void ArrayReadValue(CNeoVMWorker* worker, ArrayInfo* array, int index, VarInfo* out)
{
	switch (array->_elementType)
	{
	case NeoArrayElementType::Bool:  worker->Var_SetBool(out, array->GetBool(index)); break;
	case NeoArrayElementType::Int:   worker->Var_SetInt(out, array->IntData()[index]); break;
	case NeoArrayElementType::Float: worker->Var_SetFloat(out, array->FloatData()[index]); break;
	}
}

NEOS_FORCEINLINE bool ArrayWriteValue(ArrayInfo* array, int index, VarInfo* value)
{
	switch (array->_elementType)
	{
	case NeoArrayElementType::Bool:
		if (value->GetType() != VAR_BOOL)
			return false;
		array->SetBool(index, value->_bl);
		return true;
	case NeoArrayElementType::Int:
		if (value->GetType() == VAR_INT)
		{
			array->IntData()[index] = value->_int;
			return true;
		}
		if (value->GetType() == VAR_FLOAT)
		{
			array->IntData()[index] = (int)value->_float;
			return true;
		}
		return false;
	case NeoArrayElementType::Float:
		if (value->GetType() == VAR_INT)
		{
			array->FloatData()[index] = (NS_FLOAT)value->_int;
			return true;
		}
		if (value->GetType() == VAR_FLOAT)
		{
			array->FloatData()[index] = value->_float;
			return true;
		}
		return false;
	}
	return false;
}


// 디스패치 루프에 인라인되는 것은 list[int] / array[int] / map[string] 세 경로뿐이다.
// 나머지(프로퍼티 맵, 벡터 성분 쓰기, 문자열 인덱서, 에러 포맷)는 CltInsertRare 로 밀어낸다 —
// RunInternal 의 인라인 크기가 곧 성능이라(For/Add3 에서 실측) 드문 경로를 루프에 두지 않는다.
NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, VarInfo* pKey, VarInfo* pValue)
{
	const VAR_TYPE t = pClt->GetType();
	if (t == VAR_LIST)
	{
		if (pKey->GetType() == VAR_INT)
		{
			ListInfo* lst = pClt->_lst;
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)lst->_itemCount)
			{
				if (pValue->IsContainerType())
					lst->MarkContainerChild();
				Move(&lst->_Bucket[idx], pValue);
				return;
			}
		}
	}
	else if (t == VAR_MAP)
	{
		MapInfo* tbl = pClt->_tbl;
		if (pValue->GetType() != VAR_NONE)
		{
			// 기존 키 갱신은 노드를 찾아 값만 옮기면 된다 — 구조가 안 바뀌므로
			// MapInfo::Insert 와 결과가 같다(_mutationVersion 은 신규 노드에서만 증가).
			// 새 키는 할당/ReMap 이 필요해 그대로 아웃오브라인 Insert 로 보낸다.
			const VAR_TYPE kt = pKey->GetType();
			if (kt == VAR_STRING || kt == VAR_INT)
			{
				VarInfo* pFind = (kt == VAR_STRING) ? tbl->FindString(pKey->_str)
													: tbl->FindInt(pKey->_int);
				if (pFind)
				{
					if (pValue->IsContainerType())
						tbl->MarkContainerChild();
					Move(pFind, pValue);
					return;
				}
			}
			tbl->Insert(pKey, pValue);
			return;
		}
	}
	else if (t == VAR_VEC)
	{
		// v[i] = x. 공유 중이면 VecCopyOnWrite 가 복제한다(값 의미론).
		if (pKey->GetType() == VAR_INT && pValue->IsNumber())
		{
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)pClt->VectorComponentCount())
			{
				GetVM()->VecCopyOnWrite(pClt)->v[idx] = (float)pValue->GetFloatNumber();
				return;
			}
		}
	}
	else if (t == VAR_ARRAY)
	{
		if (pKey->GetType() != VAR_INT)
		{
			SetError(RTE_ARRAY_INDEX_TYPE);
			return;
		}
		ArrayInfo* array = pClt->_array;
		const int index = pKey->_int;
		if (array->IsValidIndex(index) == false)
		{
			SetError(RTE_ARRAY_INDEX_WRITE);
			return;
		}
		if (ArrayWriteValue(array, index, pValue))
			return;
		SetErrorFormat(RTE_ARRAY_VALUE_TYPE, GetDataType(pValue->GetType()).c_str(), GetArrayElementTypeName(array->_elementType));
		return;
	}
	CltInsertRare(pClt, pKey, pValue);
}

NEOS_NOINLINE void CNeoVMWorker::CltInsertRare(VarInfo* pClt, VarInfo* pKey, VarInfo* pValue)
{
	switch (pClt->GetType())
	{
	case VAR_VEC:
		// 가변 성분 쓰기: v[i] = x. 저장소를 공유 중이면 여기서 복제해서(copy-on-write)
		// 이 VarInfo 인스턴스에만 반영되게 한다 — 값 의미론 유지.
		if (pKey->GetType() == VAR_INT && pValue->IsNumber())
		{
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)pClt->VectorComponentCount())
			{
				GetVM()->VecCopyOnWrite(pClt)->v[idx] = (float)pValue->GetFloatNumber();
				return;
			}
		}
		SetError(RTE_VECTOR_INDEX_WRITE);
		return;
	case VAR_FP_NATIVE:
	{
		if (pKey->GetType() != VAR_STRING)
		{
			SetError(RTE_NATIVE_PROPERTY_KEY);
			return;
		}
		FunctionPropertyInfo* fp = pClt->_fpNative;
		PropertyNative(fp->_fun, fp->_pUserData, pKey->_str, pValue, false);
		return;
	}
	case VAR_ARRAY:
	{
		if (pKey->GetType() != VAR_INT)
		{
			SetError(RTE_ARRAY_INDEX_TYPE);
			return;
		}
		ArrayInfo* array = pClt->_array;
		const int index = pKey->_int;
		if (array->IsValidIndex(index) == false)
		{
			SetError(RTE_ARRAY_INDEX_WRITE);
			return;
		}
		if (ArrayWriteValue(array, index, pValue))
			return;
		SetErrorFormat(RTE_ARRAY_VALUE_TYPE, GetDataType(pValue->GetType()).c_str(), GetArrayElementTypeName(array->_elementType));
		return;
	}
	case VAR_MAP:
	{
		if (pValue->GetType() == VAR_NONE)
		{
			TableRemove(pClt, pKey);
			return;
		}
		pClt->_tbl->Insert(pKey, pValue);
		return;
	}
	case VAR_LIST:
	{
		ListInfo* lst = pClt->_lst;
		if (pKey->GetType() == VAR_INT)
		{
			// fast path: CltRead 와 동일한 이유로 버킷에 직접 쓴다.
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)lst->_itemCount)
			{
				if (pValue->IsContainerType())
					lst->MarkContainerChild();
				Move(&lst->_Bucket[idx], pValue);
				return;
			}
			break;   // 범위 밖
		}
		if (lst->_pIndexer != nullptr && pKey->GetType() == VAR_STRING)
		{
			int idx;
			if(lst->_pIndexer->TryGetValue(pKey->_str, &idx))
			{
				if(lst->SetValue(idx, pValue))
					return;
			}
		}

		SetErrorFormat(RTE_INDEX_WRITE, GetDataType(pClt->GetType()).c_str());
		return;
	}
	default:
		break;
	}
	SetErrorFormat(RTE_INDEX_WRITE, GetDataType(pClt->GetType()).c_str());
}
NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, int key, VarInfo* v)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pClt->_tbl->Insert(key, v);
		return;
	case VAR_LIST:
		pClt->_lst->SetValue(key, v);
		return;
	default:
		break;
	}
	SetErrorFormat(RTE_INDEX_WRITE, GetDataType(pClt->GetType()).c_str());
}

NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, VarInfo* pKey, int v)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pClt->_tbl->Insert(pKey, v);
		return;
	case VAR_LIST:
		pClt->_lst->SetValue(pKey->_int, v);
		return;
	default:
		break;
	}
	SetErrorFormat(RTE_INDEX_WRITE, GetDataType(pClt->GetType()).c_str());
}
NEOS_FORCEINLINE void CNeoVMWorker::CltInsert(VarInfo* pClt, int key, int v)
{
	switch (pClt->GetType())
	{
	case VAR_MAP:
		pClt->_tbl->Insert(key, v);
		return;
	case VAR_LIST:
		pClt->_lst->SetValue(key, v);
		return;
	default:
		break;
	}
	SetErrorFormat(RTE_INDEX_WRITE, GetDataType(pClt->GetType()).c_str());
}
NEOS_FORCEINLINE VarInfo* CNeoVMWorker::GetTableItem(VarInfo* pClt, VarInfo* pKey)
{
	if (pClt->GetType() == VAR_MAP)
	{
		return pClt->_tbl->Find(pKey);
	}
	else if (pClt->GetType() == VAR_LIST)
	{
		if (pKey->GetType() == VAR_INT)
		{
			return pClt->_lst->GetValue(pKey->_int);
		}
		else
		{
			if (pClt->_lst->_pIndexer != nullptr && pKey->GetType() == VAR_STRING)
			{
				int idx;
				if (pClt->_lst->_pIndexer->TryGetValue(pKey->_str, &idx))
					return pClt->_lst->GetValue(idx);
			}
		}
	}

	SetErrorFormat(RTE_INDEX_READ, GetDataType(pClt->GetType()).c_str());
	return nullptr;
}
NEOS_FORCEINLINE VarInfo* CNeoVMWorker::GetTableItemValid(VarInfo* pTable, VarInfo* pKey)
{
	VarInfo* r = GetTableItem(pTable, pKey);
	if (r != NULL)
		return r;
	SetError(RTE_KEY_NOT_FOUND);
	return NULL;
}
NEOS_FORCEINLINE VarInfo* CNeoVMWorker::GetTableItemValid(VarInfo* pTable, int Array)
{
	VarInfo Key(Array);
	VarInfo* r = GetTableItem(pTable, &Key);
	if (r != NULL)
		return r;
	SetError(RTE_KEY_NOT_FOUND);
	return NULL;
}

// CltInsert 와 같은 이유로 list[int] / array[int] / map[string]만 인라인하고 나머지는 분리한다.
NEOS_FORCEINLINE void CNeoVMWorker::CltRead(VarInfo* pClt, VarInfo* pKey, VarInfo* pValue)
{
	const VAR_TYPE t = pClt->GetType();
	if (t == VAR_LIST)
	{
		if (pKey->GetType() == VAR_INT)
		{
			ListInfo* lst = pClt->_lst;
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)lst->_itemCount)
			{
				Move(pValue, &lst->_Bucket[idx]);
				return;
			}
		}
	}
	else if (t == VAR_MAP)
	{
		MapInfo* tbl = pClt->_tbl;
		const VAR_TYPE kt = pKey->GetType();
		// 문자열/정수 두 키 타입만 인라인한다. 꼬리(Move/Release)는 공유해서 본문을 작게 유지.
		if (kt == VAR_STRING || kt == VAR_INT)
		{
			VarInfo* pFind = (kt == VAR_STRING) ? tbl->FindString(pKey->_str)
												: tbl->FindInt(pKey->_int);
			if (pFind)
				Move(pValue, pFind);
			else
				Var_Release(pValue);
			return;
		}
	}
	else if (t == VAR_VEC)
	{
		// v[i] 읽기. 범위 밖이거나 정수 인덱스가 아니면 그대로 떨어뜨려 rare 가 에러를 낸다.
		if (pKey->GetType() == VAR_INT)
		{
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)pClt->VectorComponentCount())
			{
				Var_SetFloat(pValue, pClt->_vec->v[idx]);
				return;
			}
		}
	}
	else if (t == VAR_ARRAY)
	{
		if (pKey->GetType() != VAR_INT)
		{
			SetError(RTE_ARRAY_INDEX_TYPE);
			return;
		}
		ArrayInfo* array = pClt->_array;
		const int index = pKey->_int;
		if (array->IsValidIndex(index) == false)
		{
			SetError(RTE_ARRAY_INDEX_READ);
			return;
		}
		ArrayReadValue(this, array, index, pValue);
		return;
	}
	CltReadRare(pClt, pKey, pValue);
}

NEOS_NOINLINE void CNeoVMWorker::CltReadRare(VarInfo* pClt, VarInfo* pKey, VarInfo* pValue)
{
	VarInfo* pFind;
	switch (pClt->GetType())
	{
	case VAR_VEC:
		if (pKey->GetType() == VAR_INT)
		{
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)pClt->VectorComponentCount())
			{
				Var_SetFloat(pValue, pClt->_vec->v[idx]);
				return;
			}
		}
		SetError(RTE_VECTOR_INDEX_READ);
		return;
	case VAR_FP_NATIVE:
		{
			if (pKey->GetType() != VAR_STRING)
			{
				SetError(RTE_NATIVE_PROPERTY_KEY);
				return;
			}
			FunctionPropertyInfo* fp = pClt->_fpNative;
			PropertyNative(fp->_fun, fp->_pUserData, pKey->_str, pValue, true);
			return;
		}
	case VAR_ARRAY:
	{
		if (pKey->GetType() != VAR_INT)
		{
			SetError(RTE_ARRAY_INDEX_TYPE);
			return;
		}
		ArrayInfo* array = pClt->_array;
		const int index = pKey->_int;
		if (array->IsValidIndex(index) == false)
		{
			SetError(RTE_ARRAY_INDEX_READ);
			return;
		}
		ArrayReadValue(this, array, index, pValue);
		return;
	}
	case VAR_MAP:
		{
			MapInfo* tbl = pClt->_tbl;
			// 문자열 키는 타입 디스패치를 생략하는 전용 경로로 직행
			if (pKey->GetType() == VAR_STRING)
				pFind = tbl->FindString(pKey->_str);
			else
				pFind = GetTableItem(pClt, pKey);
			if (pFind)
				Move(pValue, pFind);
			else
				Var_Release(pValue);
			return;
		}
	case VAR_LIST:
	{
		ListInfo* lst = pClt->_lst;
		if (pKey->GetType() == VAR_INT)
		{
			// fast path: 함수 호출(ListInfo::GetValue)과 VM 간접참조 없이 버킷을 직접 읽는다.
			// 음수는 unsigned 로 접히므로 비교 한 번으로 범위 검사가 끝난다.
			unsigned idx = (unsigned)pKey->_int;
			if (idx < (unsigned)lst->_itemCount)
			{
				Move(pValue, &lst->_Bucket[idx]);
				return;
			}
			break;   // 범위 밖
		}
		if (lst->_pIndexer != nullptr && pKey->GetType() == VAR_STRING)
		{
			int idx;
			if (lst->_pIndexer->TryGetValue(pKey->_str, &idx))
			{
				lst->GetValue(idx, pValue);
				return;
			}
		}
		SetErrorFormat(RTE_INDEX_READ, GetDataType(pClt->GetType()).c_str());
		return;
	}
	default:
		break;
	}
	SetErrorFormat(RTE_INDEX_READ, GetDataType(pClt->GetType()).c_str());
}

NEOS_FORCEINLINE void CNeoVMWorker::TableRemove(VarInfo* pTable, VarInfo* pKey)
{
	if (pTable->GetType() != VAR_MAP)
	{
		SetErrorFormat(RTE_INDEX_WRITE, GetDataType(pTable->GetType()).c_str());
		return;
	}

	pTable->_tbl->Remove(pKey);
}

bool CNeoVMWorker::ResetVarType(VarInfo* p, VAR_TYPE type, int capa)
{
	if(type == VAR_MAP)
	{
		Var_SetTable(p, GetVM()->TableAlloc(capa));
		return true;
	}
	else if (type == VAR_FP_NATIVE)
	{
		if (p->IsAllocType())
			Var_Release(p);
		FunctionPropertyInfo* fp = GetVM()->FunctionPropertyAlloc();
		p->SetType(VAR_FP_NATIVE);
		p->_fpNative = fp;
		++fp->_refCount;
		return true;
	}
	else if (type == VAR_LIST)
	{
		Var_SetList(p, GetVM()->ListAlloc(capa));
		return true;
	}
	else if (type == VAR_NONE)
	{
		Var_Release(p);
		return true;
	}
	return false;
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
	SetErrorOperator("-", v2);
}


NEOS_FORCEINLINE void CNeoVMWorker::MoveMinusI(VarInfo* v1, int v)
{
	Var_SetInt(v1, -v);
}

NEOS_FORCEINLINE void CNeoVMWorker::And(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
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
	SetErrorOperator("&", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Or(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
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
	SetErrorOperator("|", v1, v2);
}
NEOS_FORCEINLINE bool CNeoVMWorker::VecArith(VarInfo* r, VarInfo* v1, VarInfo* v2, int op)
{
	// 쿼터니언은 별도 타입이 아니다 — 4성분 벡터의 성분별 연산으로 동작하며,
	// 회전 의미를 원하면 사용자가 math.RotateVectorByQuat 를 쓴다.
	const int n = v1->VectorComponentCount();
	float tmp[4];
	// 성분 수가 같아야 성분별 연산이다(vec2 + vec3 는 거부).
	if (v2->IsVector() && v2->VectorComponentCount() == n)
	{
		// Vec op Vec : 성분별
		for (int i = 0; i < n; i++)
		{
			float a = v1->_vec->v[i], b = v2->_vec->v[i];
			tmp[i] = (op == 0) ? a + b : (op == 1) ? a - b : (op == 2) ? a * b : a / b;
		}
	}
	else if ((op == 2 || op == 3) && v2->IsNumber())
	{
		// Vec * scalar, Vec / scalar
		float s = (float)v2->GetFloatNumber();
		for (int i = 0; i < n; i++)
			tmp[i] = (op == 2) ? v1->_vec->v[i] * s : v1->_vec->v[i] / s;
	}
	else
		return false;
	// tmp 로 먼저 계산했으므로 r == v1 (a = a + b) 도 안전
	VecInfo* p = VecStoreFor(r, n);
	for (int i = 0; i < n; i++)
		p->v[i] = tmp[i];
	return true;
}
NEOS_FORCEINLINE void CNeoVMWorker::Add3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	// 실사용의 대부분인 INT/FLOAT 조합만 인라인한다. 문자열/벡터/메타/에러는 Add3Rare 로 분리 —
	// 이 함수는 VM 디스패치 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For 에서 실측).
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { Var_SetInt(r, v1->_int + v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_int + v2->_float); return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { Var_SetFloat(r, v1->_float + v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_float + v2->_float); return; }
	}
	Add3Rare(r, v1, v2);
}

// 드문 경로: 문자열 연결 / 벡터 / 메타테이블 / 리스트 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Add3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
		{
			Var_SetStringA(r, v1->_str->_str + v2->_str->_str);
			return;
		}
		break;
	case VAR_VEC:
		if (VecArith(r, v1, v2, 0)) return;
		break;
	case VAR_LIST:
		if (neo_DCalllibs::List_Add(this, r, v1, v2)) 
			return;
		break;
	case VAR_SET:
		break;
	default:
		break;
	}
	SetErrorOperator("+", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Sub3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	// 실사용의 대부분인 INT/FLOAT 조합만 인라인한다. 문자열/벡터/메타/에러는 Sub3Rare 로 분리 —
	// 이 함수는 VM 디스패치 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For 에서 실측).
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { Var_SetInt(r, v1->_int - v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_int - v2->_float); return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { Var_SetFloat(r, v1->_float - v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_float - v2->_float); return; }
	}
	Sub3Rare(r, v1, v2);
}

// 드문 경로: 문자열 연결 / 벡터 / 메타테이블 / 리스트 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Sub3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_STRING:
		break;
	case VAR_VEC:
		if (VecArith(r, v1, v2, 1)) return;
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		if (neo_DCalllibs::Set_Sub(this, r, v1, v2)) 
			return;
		break;
	default:
		break;
	}
	SetErrorOperator("-", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Mul3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	// 실사용의 대부분인 INT/FLOAT 조합만 인라인한다. 문자열/벡터/메타/에러는 Mul3Rare 로 분리 —
	// 이 함수는 VM 디스패치 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For 에서 실측).
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { Var_SetInt(r, v1->_int * v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_int * v2->_float); return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { Var_SetFloat(r, v1->_float * v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_float * v2->_float); return; }
	}
	Mul3Rare(r, v1, v2);
}

// 드문 경로: 문자열 연결 / 벡터 / 메타테이블 / 리스트 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Mul3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_STRING:
		break;
	case VAR_VEC:
		if (VecArith(r, v1, v2, 2)) return;
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		break;
	default:
		break;
	}
	SetErrorOperator("*", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Div3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	// 실사용의 대부분인 INT/FLOAT 조합만 인라인한다. 문자열/벡터/메타/에러는 Div3Rare 로 분리 —
	// 이 함수는 VM 디스패치 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For 에서 실측).
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { Var_SetInt(r, v1->_int / v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_int / v2->_float); return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { Var_SetFloat(r, v1->_float / v2->_int); return; }
		if (t2 == VAR_FLOAT) { Var_SetFloat(r, v1->_float / v2->_float); return; }
	}
	Div3Rare(r, v1, v2);
}

// 드문 경로: 문자열 연결 / 벡터 / 메타테이블 / 리스트 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Div3Rare(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_STRING:
		break;
	case VAR_VEC:
		if (VecArith(r, v1, v2, 3)) return;
		break;
	case VAR_LIST:
		break;
	default:
		break;
	}
	SetErrorOperator("/", v1, v2);
}

NEOS_FORCEINLINE void CNeoVMWorker::Per3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int % v2->_int);
			return;
		}
		break;
	default:
		break;
	}
	SetErrorOperator("%", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::LSh3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		Var_SetInt(r, v1->_int << v2->_int);
		return;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	case VAR_LIST:
		break;
	case VAR_SET:
		break;
	default:
		break;
	}
	SetErrorOperator("<<", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::RSh3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int >> v2->_int);
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
	default:
		break;
	}
	SetErrorOperator(">>", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::And3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int & v2->_int);
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
		break;
	default:
		break;
	}
	SetErrorOperator("&", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Or3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int | v2->_int);
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
	default:
		break;
	}
	SetErrorOperator("|", v1, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Xor3(VarInfo* r, VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			Var_SetInt(r, v1->_int ^ v2->_int);
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
		break;
	default:
		break;
	}
	SetErrorOperator("^", v1, v2);
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
	SetErrorOperator("++", v1);
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
	SetErrorOperator("--", v1);
}

NEOS_FORCEINLINE bool CNeoVMWorker::CompareEQ(VarInfo* v1, VarInfo* v2)
{
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)
			return v1->_int == v2->_int;
		if (t2 == VAR_FLOAT)
			return v1->_int == v2->_float;
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)
			return v1->_float == v2->_int;
		if (t2 == VAR_FLOAT)
			return v1->_float == v2->_float;
	}
	return CompareEQRare(v1, v2);
}

NEOS_NOINLINE bool CNeoVMWorker::CompareEQRare(VarInfo* v1, VarInfo* v2)
{
	switch (v1->GetType())
	{
	case VAR_BOOL:
		if (v2->GetType() == VAR_BOOL)
			return v1->_bl == v2->_bl;
		break;
	case VAR_NONE:
		if (v2->GetType() == VAR_NONE)
			return true;
		break;
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
			return v1->_str->_str == v2->_str->_str;
		break;
	case VAR_VEC:
		// 같은 벡터 타입 + 모든 성분 정확 일치
		if (v1->GetType() == v2->GetType())
		{
			int n = v1->VectorComponentCount();
			for (int i = 0; i < n; i++)
				if (v1->_vec->v[i] != v2->_vec->v[i]) return false;
			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

NEOS_FORCEINLINE bool CNeoVMWorker::CompareGR(VarInfo* v1, VarInfo* v2)
{
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)
			return v1->_int > v2->_int;
		if (t2 == VAR_FLOAT)
			return v1->_int > v2->_float;
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)
			return v1->_float > v2->_int;
		if (t2 == VAR_FLOAT)
			return v1->_float > v2->_float;
	}
	return CompareGRRare(v1, v2);
}

NEOS_NOINLINE bool CNeoVMWorker::CompareGRRare(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_STRING && v2->GetType() == VAR_STRING)
		return v1->_str->_str > v2->_str->_str;
	SetErrorOperator(">", v1, v2);
	return false;
}

NEOS_FORCEINLINE bool CNeoVMWorker::CompareGE(VarInfo* v1, VarInfo* v2)
{
	const VAR_TYPE t1 = v1->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)
			return v1->_int >= v2->_int;
		if (t2 == VAR_FLOAT)
			return v1->_int >= v2->_float;
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)
			return v1->_float >= v2->_int;
		if (t2 == VAR_FLOAT)
			return v1->_float >= v2->_float;
	}
	return CompareGERare(v1, v2);
}

NEOS_NOINLINE bool CNeoVMWorker::CompareGERare(VarInfo* v1, VarInfo* v2)
{
	if (v1->GetType() == VAR_STRING && v2->GetType() == VAR_STRING)
		return v1->_str->_str >= v2->_str->_str;
	SetErrorOperator(">=", v1, v2);
	return false;
}

NEOS_FORCEINLINE void CNeoVMWorker::Add2(VarInfo* r, VarInfo* v2)
{
	// INT/FLOAT 조합만 인라인. 문자열/메타/에러는 Add2Rare 로 — 이 함수는 VM 디스패치
	// 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For/Add3 에서 실측).
	const VAR_TYPE t1 = r->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { r->_int += v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->SetType(VAR_FLOAT); r->_float = (NS_FLOAT)r->_int + v2->_float; return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { r->_float += v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->_float += v2->_float; return; }
	}
	Add2Rare(r, v2);
}

// 드문 경로: 문자열 연결 / 메타테이블 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Add2Rare(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_STRING:
		if (v2->GetType() == VAR_STRING)
		{
			Var_SetStringA(r, r->_str->_str + v2->_str->_str);
			return;
		}
		break;
	default:
		break;
	}
	SetErrorOperator("+=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Sub2(VarInfo* r, VarInfo* v2)
{
	// INT/FLOAT 조합만 인라인. 문자열/메타/에러는 Sub2Rare 로 — 이 함수는 VM 디스패치
	// 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For/Add3 에서 실측).
	const VAR_TYPE t1 = r->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { r->_int -= v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->SetType(VAR_FLOAT); r->_float = (NS_FLOAT)r->_int - v2->_float; return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { r->_float -= v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->_float -= v2->_float; return; }
	}
	Sub2Rare(r, v2);
}

// 드문 경로: 문자열 연결 / 메타테이블 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Sub2Rare(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_STRING:
		break;
	default:
		break;
	}
	SetErrorOperator("-=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Mul2(VarInfo* r, VarInfo* v2)
{
	// INT/FLOAT 조합만 인라인. 문자열/메타/에러는 Mul2Rare 로 — 이 함수는 VM 디스패치
	// 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For/Add3 에서 실측).
	const VAR_TYPE t1 = r->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { r->_int *= v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->SetType(VAR_FLOAT); r->_float = (NS_FLOAT)r->_int * v2->_float; return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { r->_float *= v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->_float *= v2->_float; return; }
	}
	Mul2Rare(r, v2);
}

// 드문 경로: 문자열 연결 / 메타테이블 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Mul2Rare(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_STRING:
		break;
	default:
		break;
	}
	SetErrorOperator("*=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Div2(VarInfo* r, VarInfo* v2)
{
	// INT/FLOAT 조합만 인라인. 문자열/메타/에러는 Div2Rare 로 — 이 함수는 VM 디스패치
	// 루프에 인라인되므로 본문 크기가 곧 전체 성능이다(For/Add3 에서 실측).
	const VAR_TYPE t1 = r->GetType();
	const VAR_TYPE t2 = v2->GetType();
	if (t1 == VAR_INT)
	{
		if (t2 == VAR_INT)   { r->_int /= v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->SetType(VAR_FLOAT); r->_float = (NS_FLOAT)r->_int / v2->_float; return; }
	}
	else if (t1 == VAR_FLOAT)
	{
		if (t2 == VAR_INT)   { r->_float /= v2->_int; return; }
		if (t2 == VAR_FLOAT) { r->_float /= v2->_float; return; }
	}
	Div2Rare(r, v2);
}

// 드문 경로: 문자열 연결 / 메타테이블 / 타입 에러.
NEOS_NOINLINE void CNeoVMWorker::Div2Rare(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_STRING:
		break;
	default:
		break;
	}
	SetErrorOperator("/=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Per2(VarInfo* r, VarInfo* v2) 
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int %= v2->_int;
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	default:
		break;
	}
	SetErrorOperator("%=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::LSh2(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int <<= v2->_int;
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	default:
		break;
	}
	SetErrorOperator("<<=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::RSh2(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int >>= v2->_int;
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	default:
		break;
	}
	SetErrorOperator(">>=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::And2(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int &= v2->_int;
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	default:
		break;
	}
	SetErrorOperator("&=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Or2(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int |= v2->_int;
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	default:
		break;
	}
	SetErrorOperator("|=", r, v2);
}
NEOS_FORCEINLINE void CNeoVMWorker::Xor2(VarInfo* r, VarInfo* v2)
{
	switch (r->GetType())
	{
	case VAR_INT:
		if (v2->GetType() == VAR_INT)
		{
			r->_int ^= v2->_int;
			return;
		}
		break;
	case VAR_FLOAT:
		break;
	case VAR_STRING:
		break;
	case VAR_MAP:
		break;
	default:
		break;
	}
	SetErrorOperator("^=", r, v2);
}

NEOS_FORCEINLINE bool CNeoVMWorker::For(VarInfo* pCur)
{
//	VarInfo* pCurrent	= pCur + 1;
//	VarInfo* pEnd		= pCur + 2;
//	VarInfo* pStep		= pCur + 3;

	VarInfo* pCur_Inter = pCur + 1;

	if (pCur->GetType() == VAR_INT)
		pCur->_int = pCur_Inter->_int;
	else
		Move(pCur, pCur_Inter);
	// step 의 부호로 비교 방향을 정한다(begin/end/step 이 런타임 값일 수 있어 컴파일 타임 확정 불가).
	// 압도적 다수인 step>0 만 여기서 처리하고, 음수/0 은 ForRare 로 분리한다 —
	// 에러 처리까지 이 인라인 함수에 넣으면 코드가 커져 for 를 쓰는 모든 루프가 느려진다(실측 10~29%).
	const int step = pCur[3]._int;
	if (step > 0)
	{
		if (pCur->_int < pCur[2]._int)
		{
			pCur_Inter->_int += step;
			return true;
		}
		return false;
	}
	return ForRare(pCur, step);
}
// 드문 경로: step<0(역방향) 과 step==0(에러). 인라인하지 않아 For 의 hot path 를 작게 유지한다.
NEOS_NOINLINE bool CNeoVMWorker::ForRare(VarInfo* pCur, int step)
{
	if (step == 0)
	{
		SetErrorFormat(RTE_FOR_STEP_ZERO);   // step==0 은 영원히 끝나지 않는다
		return false;
	}
	if (pCur->_int > pCur[2]._int)           // 역방향은 비교 방향이 반대
	{
		pCur[1]._int += step;
		return true;
	}
	return false;
}
// foreach 의 드문 컨테이너. string/set 순회는 스크립트에서 거의 안 쓰이는데 본문이 커서
// 디스패치 루프만 키운다 — CltInsert/PTRCALL 과 같은 fast/rare 분리다.
NEOS_NOINLINE bool CNeoVMWorker::ForEachRare(VarInfo* pClt, VarInfo* pKey, bool bTwoVar)
{
	VarInfo* pValue = pKey + 1;
	VarInfo* pIterator = pKey + 2;
	VarInfo* pMutationVersion = pKey + 3;

	switch (pClt->GetType())
	{
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
			Var_SetStringA(pKey, utf_string::UTF8_ONE(*str, pIterator->_it._iStringOffset));
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
		// set 도 값만 순회한다. foreach(var k, v in set) 는 미지원.
		if (bTwoVar)
		{
			pIterator->ClearType();
			SetError(RTE_FOREACH_SET_TWOVAR);
			return false;
		}
		SetInfo* set = pClt->_set;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < set->GetCount())
			{
				pIterator->_it = set->FirstNode();
				pIterator->SetType(VAR_ITERATOR);
				Var_SetInt(pMutationVersion, (int)set->_mutationVersion);
			}
			else
				return false;
		}
		else
		{
			if (pMutationVersion->GetType() != VAR_INT || (u32)pMutationVersion->_int != set->_mutationVersion)
			{
				pIterator->ClearType();
				SetError(RTE_FOREACH_MODIFIED);
				return false;
			}
			set->NextNode(pIterator->_it);
		}

		SetNode* n = pIterator->_it._pSetNode;
		if (n)
		{
			Move(pKey, &n->data->key);
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
	SetErrorFormat(RTE_FOREACH_UNSUPPORTED, GetDataType(pClt->GetType()).c_str());
	return false;
}

NEOS_FORCEINLINE bool CNeoVMWorker::ForEach(VarInfo* pClt, VarInfo* pKey, bool bTwoVar)
{
	VarInfo* pValue = pKey + 1;
	VarInfo* pIterator = pKey + 2;
	// 컴파일러가 foreach마다 key/value/iterator/version 슬롯을 연속 배치한다.
	VarInfo* pMutationVersion = pKey + 3;

	switch (pClt->GetType())
	{
	case VAR_ARRAY:
	{
		// array도 list처럼 값 하나만 순회한다. 고정 길이라 원소 대입은 iterator를 무효화하지 않는다.
		if (bTwoVar)
		{
			pIterator->ClearType();
			SetError(RTE_FOREACH_ARRAY_TWOVAR);
			return false;
		}
		ArrayInfo* array = pClt->_array;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (array->_count > 0)
			{
				pIterator->_it._iListOffset = 0;
				pIterator->SetType(VAR_ITERATOR);
			}
			else
				return false;
		}
		else
			++pIterator->_it._iListOffset;

		if (pIterator->_it._iListOffset < array->_count)
		{
			ArrayReadValue(this, array, pIterator->_it._iListOffset, pKey);
			return true;
		}
		pIterator->ClearType();
		return false;
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
				Var_SetInt(pMutationVersion, (int)tbl->_mutationVersion);
			}
			else
				return false;
		}
		else
		{
			if (pMutationVersion->GetType() != VAR_INT || (u32)pMutationVersion->_int != tbl->_mutationVersion)
			{
				pIterator->ClearType();
				SetError(RTE_FOREACH_MODIFIED);
				return false;
			}
			tbl->NextNode(pIterator->_it);
		}

		MapNode* n = pIterator->_it._pTableNode;
		if (n)
		{
			Move(pKey, &n->data->key);
			Move(pValue, &n->data->value);
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
		// list 는 값만 순회한다. foreach(var k, v in list) 는 미지원.
		if (bTwoVar)
		{
			pIterator->ClearType();
			SetError(RTE_FOREACH_LIST_TWOVAR);
			return false;
		}
		ListInfo* lst = pClt->_lst;
		if (pIterator->GetType() != VAR_ITERATOR)
		{
			Var_Release(pIterator);
			if (0 < lst->GetCount())
			{
				pIterator->_it._iListOffset = 0;
				pIterator->SetType(VAR_ITERATOR);
				Var_SetInt(pMutationVersion, (int)lst->_mutationVersion);
			}
			else
				return false;
		}
		else
		{
			if (pMutationVersion->GetType() != VAR_INT || (u32)pMutationVersion->_int != lst->_mutationVersion)
			{
				pIterator->ClearType();
				SetError(RTE_FOREACH_MODIFIED);
				return false;
			}
			++pIterator->_it._iListOffset;
		}

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
	default:
		break;
	}
	// string/set 과 미지원 타입은 콜드 경로로.
	return ForEachRare(pClt, pKey, bTwoVar);
}

