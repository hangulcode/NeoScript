#pragma once

namespace NeoScript
{
class CNeoVMWorker;
class CNeoVMImpl;

#pragma pack(1)
struct MapNode
{
	VarInfo	key;
	VarInfo	value;

	u32		hash;
	int		next; // 같은 연속 노드 배열 안의 다음 슬롯. -1=끝, -2=빈 슬롯
};
#pragma pack()

static const int MAP_NODE_END = -1;
static const int MAP_NODE_EMPTY = -2;

inline bool IsMapNodeUsed(const MapNode& node)
{
	return node.next != MAP_NODE_EMPTY;
}

struct MapSortInfo
{
	CNeoVMWorker*	_pN;
	int				_compareFunction;
};

struct AllocBase
{
	int _refCount;
};

struct MapInfo : AllocBase
{
	// Lua식 hash part: 주 버킷 자체가 연속 MapNode 배열의 슬롯이다.
	// 별도 pFirst 포인터를 없애고 충돌은 next 인덱스로 연결한다.
	MapNode*	_Bucket;

	CNeoVMImpl*	_pVM;
	int	_HashBase;
	int _BucketCapa;
	// Former _TableID slot (never used): next collision-node candidate. It moves
	// only downward; -1 means exhausted. Reusing the slot preserves MapInfo's
	// hot-member layout.
	int	_lastFree;
	int _itemCount;
	u32 _mutationVersion = 0;
	void* _pUserData;

	FunctionPtrNative _fun;
	MapInfo*		_meta;

	// 살아있는 객체 추적용 intrusive 이중연결 리스트 (std::map 레지스트리 대체)
	MapInfo* _liveNext;
	MapInfo* _livePrev;

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
	VarInfo* FindString(StringInfo* pKeyStr); // 문자열 키 전용 fast path (NeoVMInternal.h 에 인라인 정의)
	VarInfo* FindInt(int key);                // 정수 키 전용 fast path (NeoVMInternal.h 에 인라인 정의)

	CollectionIterator FirstNode();
	bool NextNode(CollectionIterator&);

	bool ToListKeys(std::vector<VarInfo*>& lst);
	bool ToListValues(std::vector<VarInfo*>& lst);
	inline int		GetCount() { return _itemCount; }

private:
	void ClearNode(MapNode& node);
	int FindNodeIndex(VarInfo* pKey, u32 hash, int* pPrevious = nullptr) const;
	int FindFreeNodeIndex();
	int InsertNewNode(u32 hash);
	bool ReMap(int minCapacity = 0);
};

};
