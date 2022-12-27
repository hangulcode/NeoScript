#pragma once

#pragma pack(1)
struct TableNode
{
	VarInfo	key;
	VarInfo	value;

	u32		hash;

	TableNode* pNext; // List In Bucket
};
#pragma pack()

struct TableSortInfo
{
	CNeoVMWorker*	_pN;
	int				_compareFunction;
};



struct TableBucket
{
	TableNode*	pFirst;
	bool	Pop_Used(TableNode* pTar);
	TableNode* Find(VarInfo* pKey, u32 hash);
	inline void Add_NoCheck(TableNode* p)
	{
		p->pNext = pFirst;
		pFirst = p;
	}
};

class CNeoVM;
class CNeoVMWorker;
struct TableInfo
{
	TableBucket*	_Bucket;

	CNeoVM*	_pVM;

	int	_HashBase;
	int _BucketCapa;

	int	_TableID;
	int _refCount;
	int _itemCount;
	void* _pUserData;

	FunctionPtrNative _fun;
	TableInfo*		_meta;

	void Free();

	void Insert(std::string& pKey, VarInfo* pValue);
	void Insert(VarInfo* pKey, VarInfo* pValue);
	void Remove(VarInfo* pKey);
	VarInfo* GetTableItem(VarInfo *pKey);
	VarInfo* GetTableItem(std::string& key);

	TableIterator FirstNode();
	bool NextNode(TableIterator&);

	bool ToList(std::vector<VarInfo*>& lst);
	int		GetCount() 
	{
		return _itemCount;
	}

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

