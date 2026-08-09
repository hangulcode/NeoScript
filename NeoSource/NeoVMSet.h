#pragma once

namespace NeoScript
{

// Set의 키도 메타데이터 배열 밖의 VM 풀 객체다. hash/next/data만으로
// 충돌 체인을 훑고, 실제 키 비교가 필요한 경우에만 SetData로 간다.
struct SetData
{
	VarInfo	key;
};

struct SetNode
{
	u32		hash;
	int		next; // index in the contiguous node array; -1=end, -2=empty
	SetData*	data;
};

static const int SET_NODE_END = -1;
static const int SET_NODE_EMPTY = -2;

inline bool IsSetNodeUsed(const SetNode& node)
{
	return node.next != SET_NODE_EMPTY;
}

struct SetSortInfo
{
	CNeoVMWorker*	_pN;
	int				_compareFunction;
};

class CNeoVMImpl;
class CNeoVMWorker;
struct SetInfo : AllocBase
{
	// Lua-style hash part: each primary bucket is a slot in this node array.
	SetNode*	_Bucket;

	CNeoVMImpl*	_pVM;

	int	_HashBase;
	int _BucketCapa;

	// Former _SetID slot (never used): descending free-slot cursor.
	int	_lastFree;
	int _itemCount;
	u32 _mutationVersion = 0;
	SetInfo*		_meta;

	// 살아있는 객체 추적용 intrusive 이중연결 리스트 (std::map 레지스트리 대체)
	SetInfo* _liveNext;
	SetInfo* _livePrev;

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
	void ClearNode(SetNode& node);
	SetData* AllocNodeData();
	void FreeNodeData(SetData* data);
	int FindNodeIndex(VarInfo* pKey, u32 hash, int* pPrevious = nullptr) const;
	int FindFreeNodeIndex();
	int InsertNewNode(u32 hash);
	bool ReMap(int minCapacity = 0);
};

};
