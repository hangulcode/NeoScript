struct neo_DCalllibs
{
	static bool List_Add(CNeoVMWorker* pN, VarInfo* r, VarInfo* v1, VarInfo* v2)
	{
		if (v1->GetType() != VAR_LIST) return false;
		if (v2->GetType() != VAR_LIST) return false;

		ListInfo* pR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(r, pR);

		ListInfo* pV1 = v1->_lst;
		ListInfo* pV2 = v2->_lst;

		int s1 = pV1->GetCount();
		int s2 = pV2->GetCount();
		int sz = s1 + s2;
		pR->Resize(sz);

		VarInfo* dest = pR->GetDataUnsafe();
		VarInfo* src1 = pV1->GetDataUnsafe();
		VarInfo* src2 = pV2->GetDataUnsafe();

		for (int i = 0; i < s1; i++)
			INeoVM::Move_DestNoRelease(&dest[i], &src1[i]);
		for (int i = 0; i < s2; i++)
			INeoVM::Move_DestNoRelease(&dest[i + s1], &src2[i]);

		return true;
	}
	static bool Set_And(CNeoVMWorker* pN, VarInfo* r, VarInfo* v1, VarInfo* v2) // 교집합
	{
		if (v1->GetType() != VAR_SET) return false;
		if (v2->GetType() != VAR_SET) return false;

		SetInfo* pR = pN->GetVM()->SetAlloc();
		pN->Var_SetSet(r, pR);

		SetInfo* pV1 = v1->_set;
		SetInfo* pV2 = v2->_set;

		pR->Reserve(pV1->GetCount() < pV2->GetCount() ? pV1->GetCount() : pV2->GetCount());

		CollectionIterator it = pV1->FirstNode();
		while (it._pSetNode)
		{
			if (true == pV2->Find(&it._pSetNode->key))
				pR->Insert(&it._pSetNode->key);
			pV1->NextNode(it);
		}
		return true;
	}
	static bool Set_Or(CNeoVMWorker* pN, VarInfo* r, VarInfo* v1, VarInfo* v2) // 합집합
	{
		if (v1->GetType() != VAR_SET) return false;
		if (v2->GetType() != VAR_SET) return false;

		SetInfo* pR = pN->GetVM()->SetAlloc();
		pN->Var_SetSet(r, pR);

		SetInfo* pV1 = v1->_set;
		SetInfo* pV2 = v2->_set;

		pR->Reserve(pV1->GetCount() + pV2->GetCount());

		CollectionIterator it = pV1->FirstNode();
		while (it._pSetNode)
		{
			pR->Insert(&it._pSetNode->key);
			pV1->NextNode(it);
		}

		it = pV2->FirstNode();
		while (it._pSetNode)
		{
			if (false == pV1->Find(&it._pSetNode->key))
				pR->Insert(&it._pSetNode->key);
			pV2->NextNode(it);
		}
		return true;
	}
	static bool Set_Sub(CNeoVMWorker* pN, VarInfo* r, VarInfo* v1, VarInfo* v2) // 차집합
	{
		if (v1->GetType() != VAR_SET) return false;
		if (v2->GetType() != VAR_SET) return false;

		SetInfo* pR = pN->GetVM()->SetAlloc();
		pN->Var_SetSet(r, pR);

		SetInfo* pV1 = v1->_set;
		SetInfo* pV2 = v2->_set;

		pR->Reserve(pV1->GetCount());

		CollectionIterator it = pV1->FirstNode();
		while (it._pSetNode)
		{
			if(false == pV2->Find(&it._pSetNode->key))
				pR->Insert(&it._pSetNode->key);
			pV1->NextNode(it);
		}
		return true;
	}
};
