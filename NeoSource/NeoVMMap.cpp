#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVMInternal.h"
#include "NeoVMWorker.h"
#include "NeoVMMap.h"

namespace NeoScript
{

extern u32 GetHashCode(const std::string& str);
extern u32 GetHashCode(u8 *buffer, int len);
extern u32 GetHashCode(VarInfo *p);

static u32 GetMapKeyHash(VarInfo* pKey)
{
	return pKey->GetType() == VAR_STRING ? pKey->_str->GetHash() : GetHashCode(pKey);
}

static VarInfo* NormalizeMapStringKey(CNeoVM* vm, VarInfo* pKey, VarInfo& normalized, bool forInsert)
{
	if (pKey->GetType() != VAR_STRING || pKey->_str->_interned)
		return pKey;

	StringInfo* canonical = forInsert
		? vm->StringIntern(pKey->_str->_str)
		: vm->StringFind(pKey->_str);
	if (canonical == nullptr)
		return nullptr;

	normalized = *pKey;
	normalized._str = canonical;
	return &normalized;
}

void MapInfo::ClearNode(MapNode& node)
{
	node.hash = 0;
	node.next = MAP_NODE_EMPTY;
	node.data = NULL;
}

MapData* MapInfo::AllocNodeData()
{
	MapData* data = _pVM->m_sPool_TableData.Receive();
	data->key.ClearType();
	data->value.ClearType();
	return data;
}

void MapInfo::FreeNodeData(MapData* data)
{
	if (data == NULL)
		return;
	_pVM->Var_Release(&data->key);
	_pVM->Var_Release(&data->value);
	_pVM->m_sPool_TableData.Confer(data);
}

static bool MapKeyEquals(MapNode& node, VarInfo* pKey, u32 hash)
{
	MapData* data = node.data;
	if (node.hash != hash || data == NULL || data->key.GetType() != pKey->GetType())
		return false;

	switch (pKey->GetType())
	{
	case VAR_INT:       return data->key._int == pKey->_int;
	case VAR_FLOAT:     return data->key._float == pKey->_float;
	case VAR_BOOL:      return data->key._bl == pKey->_bl;
	case VAR_NONE:      return true;
	case VAR_FUN:       return data->key._fun_index == pKey->_fun_index;
	case VAR_STRING:
		return data->key._str == pKey->_str;
	case VAR_VEC:		return data->key._vec == pKey->_vec;
	case VAR_FP_NATIVE: return data->key._fpNative == pKey->_fpNative;
	case VAR_MAP:       return data->key._tbl == pKey->_tbl;
	case VAR_LIST:      return data->key._lst == pKey->_lst;
	case VAR_SET:       return data->key._set == pKey->_set;
	case VAR_COROUTINE: return data->key._cor == pKey->_cor;
	case VAR_CLOSURE:   return data->key._closure == pKey->_closure;
	default:             return false;
	}
}

int MapInfo::FindNodeIndex(VarInfo* pKey, u32 hash, int* pPrevious) const
{
	if (pPrevious) *pPrevious = MAP_NODE_END;
	if (_BucketCapa <= 0)
		return MAP_NODE_END;

	const int main = (int)(hash & _HashBase);
	if (!IsMapNodeUsed(_Bucket[main]))
		return MAP_NODE_END;

	int previous = MAP_NODE_END;
	for (int index = main; index != MAP_NODE_END; index = _Bucket[index].next)
	{
		if (MapKeyEquals(_Bucket[index], pKey, hash))
		{
			if (pPrevious) *pPrevious = previous;
			return index;
		}
		previous = index;
	}
	return MAP_NODE_END;
}

int MapInfo::FindFreeNodeIndex()
{
	// A slot is examined at most once between rehashes. Direct placements can
	// occupy slots above this cursor; they are skipped once when reached.
	while (_lastFree >= 0)
	{
		const int index = _lastFree--;
		if (!IsMapNodeUsed(_Bucket[index]))
			return index;
	}
	return MAP_NODE_END;
}

// A primary slot always remains the head for its own collision chain.
// If it holds a displaced node, move that node to a free slot first.
// This is the essential chained-scatter/Brent invariant used by Lua tables.
int MapInfo::InsertNewNode(u32 hash)
{
	const int main = (int)(hash & _HashBase);
	if (!IsMapNodeUsed(_Bucket[main]))
	{
		_Bucket[main].next = MAP_NODE_END;
		return main;
	}

	const int freeIndex = FindFreeNodeIndex();
	if (freeIndex == MAP_NODE_END)
		return MAP_NODE_END;

	const int occupiedMain = (int)(_Bucket[main].hash & _HashBase);
	if (occupiedMain != main)
	{
		// The resident node belongs to another chain. Preserve that chain by
		// relinking its predecessor, then use the freed primary slot.
		int previous = occupiedMain;
		while (_Bucket[previous].next != main)
			previous = _Bucket[previous].next;

		_Bucket[freeIndex] = _Bucket[main];
		_Bucket[previous].next = freeIndex;
		ClearNode(_Bucket[main]);
		_Bucket[main].next = MAP_NODE_END;
		return main;
	}

	// This is the primary chain; link the new node immediately after its head.
	_Bucket[freeIndex].next = _Bucket[main].next;
	_Bucket[main].next = freeIndex;
	return freeIndex;
}

bool MapInfo::ReMap(int minCapacity)
{
	int newCapacity = _BucketCapa > 0 ? _BucketCapa : 2;
	if (minCapacity <= 0)
		minCapacity = newCapacity << 1;
	while (newCapacity < minCapacity)
		newCapacity <<= 1;

	MapNode* oldBucket = _Bucket;
	const int oldCapacity = _BucketCapa;
	const int oldHashBase = _HashBase;
	const int oldLastFree = _lastFree;

	_Bucket = new MapNode[newCapacity];
	_BucketCapa = newCapacity;
	_HashBase = newCapacity - 1;
	_lastFree = newCapacity - 1;
	for (int i = 0; i < newCapacity; ++i)
		ClearNode(_Bucket[i]);

	for (int i = 0; i < oldCapacity; ++i)
	{
		if (!IsMapNodeUsed(oldBucket[i]))
			continue;

		const int target = InsertNewNode(oldBucket[i].hash);
		if (target == MAP_NODE_END)
		{
			// A failed transfer must be transactional: dropping this node would
			// lose its key/value while leaving _itemCount and refcounts stale.
			delete[] _Bucket;
			_Bucket = oldBucket;
			_BucketCapa = oldCapacity;
			_HashBase = oldHashBase;
			_lastFree = oldLastFree;
			return false;
		}
		const int next = _Bucket[target].next;
		_Bucket[target].data = oldBucket[i].data;
		_Bucket[target].hash = oldBucket[i].hash;
		_Bucket[target].next = next;
	}

	delete[] oldBucket;

	// 노드가 연속 배열로 옮겨가면서 CollectionIterator 가 배열 내부 생 포인터를 들게 됐다.
	// 재할당으로 옛 배열이 사라지므로 순회 중 여기 오면 그 포인터는 dangling 이다.
	// 버전을 올려 foreach 가 다음 스텝에서 "순회 중 변경" 으로 막게 한다
	// (Insert/Remove 와 동일한 계약). 올리지 않으면 해제된 주소로 포인터 산술을 한다.
	++_mutationVersion;
	return true;
}

CollectionIterator MapInfo::FirstNode()
{
	CollectionIterator r;
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (IsMapNodeUsed(_Bucket[i]))
		{
			r._pTableNode = &_Bucket[i];
			return r;
		}
	}
	r._pTableNode = NULL;
	return r;
}

bool MapInfo::NextNode(CollectionIterator& r)
{
	if (r._pTableNode == NULL || _BucketCapa <= 0)
		return false;

	const ptrdiff_t current = r._pTableNode - _Bucket;
	for (int i = (int)current + 1; i < _BucketCapa; ++i)
	{
		if (IsMapNodeUsed(_Bucket[i]))
		{
			r._pTableNode = &_Bucket[i];
			return true;
		}
	}
	r._pTableNode = NULL;
	return false;
}

void MapInfo::Free()
{
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (!IsMapNodeUsed(_Bucket[i]))
			continue;
		FreeNodeData(_Bucket[i].data);
	}

	delete[] _Bucket;
	_Bucket = NULL;
	_BucketCapa = _HashBase = 0;
	_lastFree = -1;
	_itemCount = 0;
}

void MapInfo::Reserve(int sz)
{
	if (sz < 0) sz = 0;
	int minCapacity = 2;
	// Keep the same maximum load as Insert. Nodes live directly in this array,
	// so a full table makes collision-chain lookup depend on key placement.
	while ((long long)minCapacity * 3 < (long long)sz * 4)
		minCapacity <<= 1;
	if (minCapacity > _BucketCapa)
		ReMap(minCapacity);
}

VarInfo* MapInfo::Insert(VarInfo* pKey)
{
	if (pKey->IsContainerType())
		MarkContainerChild();

	VarInfo normalizedKey;
	pKey = NormalizeMapStringKey(_pVM, pKey, normalizedKey, true);
	if (pKey == nullptr)
		return NULL;

	u32 hash = GetMapKeyHash(pKey);
	const int found = FindNodeIndex(pKey, hash);
	if (found != MAP_NODE_END)
		return &_Bucket[found].data->value;

	if (_BucketCapa == 0
		|| (long long)(_itemCount + 1) * 4 > (long long)_BucketCapa * 3)
	{
		if (!ReMap(_BucketCapa > 0 ? _BucketCapa << 1 : 2))
			return NULL;
	}

	int index = InsertNewNode(hash);
	if (index == MAP_NODE_END)
	{
		// _lastFree does not move back after removals. Rebuild at the same
		// capacity to recover those holes; grow only when the table is full.
		const int minCapacity = _itemCount + 1 >= _BucketCapa
			? _BucketCapa << 1
			: _BucketCapa;
		if (!ReMap(minCapacity))
			return NULL;
		index = InsertNewNode(hash);
	}
	if (index == MAP_NODE_END)
		return NULL;

	MapNode& node = _Bucket[index];
	node.data = AllocNodeData();
	Move_DestNoRelease(&node.data->key, pKey);
	node.hash = hash;
	++_itemCount;
	++_mutationVersion;
	return &node.data->value;
}

void MapInfo::Insert(const std::string& Key, VarInfo* pValue)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = _pVM->StringIntern(Key);
	Insert(&var, pValue);
	if (var._str->_refCount <= 0)
		_pVM->FreeString(&var);
}

bool MapInfo::Insert(const std::string& key, NS_FLOAT value)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = _pVM->StringIntern(key);
	VarInfo* pDest = Insert(&var);
	if (pDest == NULL)
	{
		_pVM->FreeString(&var);
		return false;
	}
	_pVM->Var_Release(pDest);
	pDest->SetType(VAR_FLOAT);
	pDest->_float = value;
	if (var._str->_refCount <= 0)
		_pVM->FreeString(&var);
	return true;
}

void MapInfo::Insert(VarInfo* pKey, VarInfo* pValue)
{
	VarInfo* pDest = Insert(pKey);
	if (pDest != NULL)
	{
		if (pValue->IsContainerType())
			MarkContainerChild();
		_pVM->Move(pDest, pValue);
	}
}

void MapInfo::Insert(VarInfo* pKey, int v)
{
	VarInfo* pDest = Insert(pKey);
	if (pDest == NULL) return;
	if (pDest->IsAllocType()) _pVM->Var_Release(pDest);
	pDest->SetType(VAR_INT);
	pDest->_int = v;
}

void MapInfo::Insert(int Key, VarInfo* pValue)
{
	VarInfo var;
	var.SetType(VAR_INT);
	var._int = Key;
	Insert(&var, pValue);
}

void MapInfo::Insert(int Key, int v)
{
	VarInfo var;
	var.SetType(VAR_INT);
	var._int = Key;
	Insert(&var, v);
}

void MapInfo::Remove(VarInfo* pKey)
{
	if (_BucketCapa <= 0)
		return;
	VarInfo normalizedKey;
	pKey = NormalizeMapStringKey(_pVM, pKey, normalizedKey, false);
	if (pKey == nullptr)
		return;

	const u32 hash = GetMapKeyHash(pKey);
	int previous = MAP_NODE_END;
	const int index = FindNodeIndex(pKey, hash, &previous);
	if (index == MAP_NODE_END)
		return;

	MapNode& node = _Bucket[index];
	MapData* removedData = node.data;

	if (previous != MAP_NODE_END)
	{
		_Bucket[previous].next = node.next;
		ClearNode(node);
	}
	else if (node.next == MAP_NODE_END)
	{
		ClearNode(node);
	}
	else
	{
		// Move the next node into the head slot to retain the primary-slot invariant.
		const int next = node.next;
		const MapNode moved = _Bucket[next];
		node = moved;
		ClearNode(_Bucket[next]);
	}
	FreeNodeData(removedData);

	--_itemCount;
	++_mutationVersion;
}

VarInfo* MapInfo::Find(VarInfo *pKey)
{
	if (pKey->GetType() == VAR_STRING)
		return FindString(pKey->_str);

	const int index = FindNodeIndex(pKey, GetMapKeyHash(pKey));
	return index == MAP_NODE_END ? NULL : &_Bucket[index].data->value;
}

VarInfo* MapInfo::Find(const std::string& key)
{
	StringInfo* pKey = _pVM->StringFind(key);
	return pKey ? FindString(pKey) : NULL;
}

bool MapInfo::ToListKeys(std::vector<VarInfo*>& lst)
{
	lst.resize(_itemCount);
	int cnt = 0;
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (IsMapNodeUsed(_Bucket[i]))
			lst[cnt++] = &_Bucket[i].data->key;
	}
	return cnt == _itemCount;
}

bool MapInfo::ToListValues(std::vector<VarInfo*>& lst)
{
	lst.resize(_itemCount);
	int cnt = 0;
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (IsMapNodeUsed(_Bucket[i]))
			lst[cnt++] = &_Bucket[i].data->value;
	}
	return cnt == _itemCount;
}

struct NeoSortLocal
{
	CNeoVMWorker* m_pN;
	VarInfo* m_compare;

	NeoSortLocal(CNeoVMWorker* pN, VarInfo* compare) : m_pN(pN), m_compare(compare) {}
	bool operator () (VarInfo* a, VarInfo* b)
	{
		VarInfo args[2];
		args[0] = *a;
		args[1] = *b;
		VarInfo* r = m_pN->testCall(m_compare, args, 2);
		return r && r->GetType() == VAR_BOOL && r->_bl;
	}
};

void NVM_QuickSort(CNeoVMWorker* pN, VarInfo* compare, std::vector<VarInfo*>& lst)
{
	std::sort(lst.begin(), lst.end(), NeoSortLocal(pN, compare));
}

};
