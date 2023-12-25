#pragma once

namespace NeoScript
{

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
#if 0
	u32			flag;// 이 비트가 off 이면 반드시 없음. on 이면 있을수도 없을수도 있음 (추가 될때는 on 하고, 삭제시 off 하지 않음)
#endif
	bool	Pop_Used(MapNode* pTar);
	MapNode* Find(VarInfo* pKey, u32 hash);
	#if 0
	inline void Add_NoCheck(MapNode* p, int bit)
	{
		p->pNext = pFirst;
		pFirst = p;
		flag |= (1 << ((p->hash >> bit) & 0x1F));
	}
	inline bool IsNoHaveKey(u32 hash, int bit)
	{
		return (flag & (1 << ((hash >> bit) & 0x1F))) == 0;
	}
	#else
	inline void Add_NoCheck(MapNode* p)
	{
		p->pNext = pFirst;
		pFirst = p;
	}
#endif
};

class CNeoVMImpl;
class CNeoVMWorker;
struct MapInfo : AllocBase
{
	MapBucket*	_Bucket;

	CNeoVMImpl*	_pVM;
	int _HashCheckBit;
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
	bool Insert(const std::string& pKey, NS_FLOAT value);
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

};