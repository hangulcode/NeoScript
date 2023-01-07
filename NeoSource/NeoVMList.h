#pragma once

#pragma pack(1)
#pragma pack()



class CNeoVM;
class CNeoVMWorker;
struct ListInfo
{
	VarInfo*	_Bucket;

	CNeoVM*	_pVM;


	int	_ListID;
	int _refCount;
	int _itemCount;
	void* _pUserData;

	int _BucketCapa;

	void Free();
	void Resize(int size);
	void Reserve(int capa);


//	CollectionIterator FirstNode();
//	bool NextNode(CollectionIterator&);

	inline int		GetCount() { return _itemCount; }
	bool GetValue(int idx, VarInfo* pValue);
	bool SetValue(int idx, VarInfo* pValue);
	bool InsertLast(VarInfo* pValue);

	inline VarInfo* GetDataUnsafe() { return _Bucket; }
private:
	inline void Var_AddRef(VarInfo *d)
	{
		switch (d->GetType())
		{
		case VAR_STRING:
			++d->_str->_refCount;
			break;
		case VAR_TABLE:
			++d->_tbl->_refCount;
			break;
		case VAR_LIST:
			++d->_lst->_refCount;
			break;
		case VAR_COROUTINE:
			++d->_cor->_refCount;
			break;
		default:
			break;
		}
	}
	void Var_ReleaseInternal(CNeoVM* pVM, VarInfo *d);
	inline void Var_Release(CNeoVM* pVM, VarInfo *d)
	{
		if (d->IsAllocType())
			Var_ReleaseInternal(pVM, d);
		else
			d->ClearType();
	}
};

