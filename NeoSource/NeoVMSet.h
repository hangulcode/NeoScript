#pragma once

#pragma pack(1)
struct SetNode
{
	VarInfo	key;

	u32		hash;

	SetNode* pNext; // List In Bucket
};
#pragma pack()

struct SetSortInfo
{
	CNeoVMWorker*	_pN;
	int				_compareFunction;
};



struct SetBucket
{
	SetNode*	pFirst;
	bool	Pop_Used(SetNode* pTar);
	SetNode* Find(VarInfo* pKey, u32 hash);
	inline void Add_NoCheck(SetNode* p)
	{
		p->pNext = pFirst;
		pFirst = p;
	}
};

class CNeoVMImpl;
class CNeoVMWorker;
struct SetInfo : AllocBase
{
	SetBucket*	_Bucket;

	CNeoVMImpl*	_pVM;

	int	_HashBase;
	int _BucketCapa;

	int	_SetID;
	int _itemCount;
	void* _pUserData;

	FunctionPtrNative _fun;
	SetInfo*		_meta;

	void Free();

	void Reserve(int sz);
	bool Insert(VarInfo* pKey);
	void Insert(std::string& pKey);
	void Insert(int Key);
	void Remove(VarInfo* pKey);

	bool Find(VarInfo *pKey);
	bool Find(std::string& key);

	CollectionIterator FirstNode();
	bool NextNode(CollectionIterator&);

	bool ToList(std::vector<VarInfo*>& lst);
	inline int		GetCount() { return _itemCount; }

private:
	void Var_Release(CNeoVMImpl* pVM, VarInfo *d);
};

