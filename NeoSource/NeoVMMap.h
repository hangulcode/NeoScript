#pragma once

#pragma pack(1)
struct MapNode
{
	VarInfo	key;
	VarInfo	value;

	u32		hash;

	MapNode* pNext; // List In Bucket
};
#pragma pack()

struct MapSortInfo
{
	CNeoVMWorker*	_pN;
	int				_compareFunction;
};



struct MapBucket
{
	MapNode*	pFirst;
	bool	Pop_Used(MapNode* pTar);
	MapNode* Find(VarInfo* pKey, u32 hash);
	inline void Add_NoCheck(MapNode* p)
	{
		p->pNext = pFirst;
		pFirst = p;
	}
};

class CNeoVMImpl;
class CNeoVMWorker;
struct MapInfo : AllocBase
{
	MapBucket*	_Bucket;

	CNeoVMImpl*	_pVM;

	int	_HashBase;
	int _BucketCapa;

	int	_TableID;
	int _itemCount;
	void* _pUserData;

	FunctionPtrNative _fun;
	MapInfo*		_meta;

	void Free();

	void Reserve(int sz);
	VarInfo* Insert(VarInfo* pKey);
	void Insert(const std::string& pKey, VarInfo* pValue);
	bool Insert(const std::string& pKey, double value);
	void Insert(VarInfo* pKey, VarInfo* pValue);
	void Insert(VarInfo* pKey, int v);
	void Insert(int Key, VarInfo* pValue);
	void Insert(int Key, int v);
	void Remove(VarInfo* pKey);
	VarInfo* Find(VarInfo *pKey);
	VarInfo* Find(const std::string& key);

	CollectionIterator FirstNode();
	bool NextNode(CollectionIterator&);

	bool ToListKeys(std::vector<VarInfo*>& lst);
	bool ToListValues(std::vector<VarInfo*>& lst);
	inline int		GetCount() { return _itemCount; }

private:
};

