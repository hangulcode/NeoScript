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
struct TableInfo : AllocBase
{
	TableBucket*	_Bucket;

	CNeoVM*	_pVM;

	int	_HashBase;
	int _BucketCapa;

	int	_TableID;
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

	CollectionIterator FirstNode();
	bool NextNode(CollectionIterator&);

	bool ToList(std::vector<VarInfo*>& lst);
	inline int		GetCount() { return _itemCount; }

private:
	void Var_ReleaseInternal(CNeoVM* pVM, VarInfo *d);
	inline void Var_Release(CNeoVM* pVM, VarInfo *d)
	{
		if (d->IsAllocType())
			Var_ReleaseInternal(pVM, d);
		else
			d->ClearType();
	}
};

