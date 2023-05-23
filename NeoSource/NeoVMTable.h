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

class CNeoVMImpl;
class CNeoVMWorker;
struct TableInfo : AllocBase
{
	TableBucket*	_Bucket;

	CNeoVMImpl*	_pVM;

	int	_HashBase;
	int _BucketCapa;

	int	_TableID;
	int _itemCount;
	void* _pUserData;

	FunctionPtrNative _fun;
	TableInfo*		_meta;

	void Free();

	void Reserve(int sz);
	VarInfo* Insert(VarInfo* pKey);
	void Insert(std::string& pKey, VarInfo* pValue);
	void Insert(VarInfo* pKey, VarInfo* pValue);
	void Insert(VarInfo* pKey, int v);
	void Insert(int Key, VarInfo* pValue);
	void Insert(int Key, int v);
	void Remove(VarInfo* pKey);
	VarInfo* Find(VarInfo *pKey);
	VarInfo* Find(std::string& key);

	CollectionIterator FirstNode();
	bool NextNode(CollectionIterator&);

	bool ToListKeys(std::vector<VarInfo*>& lst);
	bool ToListValues(std::vector<VarInfo*>& lst);
	inline int		GetCount() { return _itemCount; }

private:
};

