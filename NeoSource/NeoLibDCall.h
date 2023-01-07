struct neo_DCalllibs
{
	static bool List_Plus(CNeoVMWorker* pN, VarInfo* r, VarInfo* v1, VarInfo* v2)
	{
		if (v1->GetType() != VAR_LIST)
			return false;
		if (v2->GetType() != VAR_LIST)
			return false;

		ListInfo* pListR = pN->_pVM->ListAlloc();
		pN->Var_SetList(r, pListR);

		ListInfo* pListV1 = v1->_lst;
		ListInfo* pListV2 = v2->_lst;

		int s1 = pListV1->GetCount();
		int s2 = pListV2->GetCount();
		int sz = s1 + s2;
		pListR->Resize(sz);

		VarInfo* dest = pListR->GetDataUnsafe();
		VarInfo* src1 = pListV1->GetDataUnsafe();
		VarInfo* src2 = pListV2->GetDataUnsafe();

		for (int i = 0; i < s1; i++)
			CNeoVMWorker::Move_DestNoRelease(&dest[i], &src1[i]);
		for (int i = 0; i < s2; i++)
			CNeoVMWorker::Move_DestNoRelease(&dest[i + s1], &src2[i]);

		return true;
	}

};
