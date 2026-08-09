#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVMImpl.h"
#include "NeoVMWorker.h"
#include "NeoVMSet.h"

namespace NeoScript
{

u32 GetHashCode(const std::string& str)
{
	// Stable across standard libraries and platforms. FNV-1a is the byte-wise
	// accumulator; the final avalanche also makes the low bits suitable for
	// Set's power-of-two bucket mask.
	u32 h = 2166136261u;
	for (unsigned char ch : str)
	{
		h ^= ch;
		h *= 16777619u;
	}
	h ^= h >> 16;
	h *= 0x7feb352dU;
	h ^= h >> 15;
	h *= 0x846ca68bU;
	h ^= h >> 16;
	return h;
}

u32 GetHashCode(u8* buffer, int len)
{
	u32 h = 0;
	while (len-- > 0)
		h |= buffer[len] << ((len & 0x03) * 8);
	return h;
}

u32 GetHashCode(VarInfo* p)
{
	switch (p->GetType())
	{
	case VAR_INT:        return (u32)p->_int;
	case VAR_FLOAT:      return GetHashCode((u8*)&p->_float, sizeof(p->_float));
	case VAR_BOOL:       return p->_bl;
	case VAR_NONE:       return 0;
	case VAR_FUN:        return (u32)p->_fun_index;
	case VAR_FUN_NATIVE: return (u32)(uintptr_t)p->_funPtr;
	case VAR_STRING:     return p->_str->GetHash();
	case VAR_MAP:        return GetHashCode((u8*)p->_tbl, sizeof(p->_tbl));
	case VAR_LIST:       return GetHashCode((u8*)p->_lst, sizeof(p->_lst));
	case VAR_SET:        return GetHashCode((u8*)p->_set, sizeof(p->_set));
	case VAR_COROUTINE:  return GetHashCode((u8*)p->_cor, sizeof(p->_cor));
	default:              return 0;
	}
}

void SetInfo::ClearNode(SetNode& node)
{
	node.key.ClearType();
	node.hash = 0;
	node.next = SET_NODE_EMPTY;
}

static bool SetKeyEquals(SetNode& node, VarInfo* pKey, u32 hash)
{
	if (node.hash != hash || node.key.GetType() != pKey->GetType())
		return false;

	switch (pKey->GetType())
	{
	case VAR_INT:        return node.key._int == pKey->_int;
	case VAR_FLOAT:      return node.key._float == pKey->_float;
	case VAR_BOOL:       return node.key._bl == pKey->_bl;
	case VAR_NONE:       return true;
	case VAR_FUN:        return node.key._fun_index == pKey->_fun_index;
	case VAR_FUN_NATIVE: return node.key._funPtr == pKey->_funPtr;
	case VAR_STRING:
		return node.key._str == pKey->_str;
	case VAR_MAP:       return node.key._tbl == pKey->_tbl;
	case VAR_LIST:      return node.key._lst == pKey->_lst;
	case VAR_SET:       return node.key._set == pKey->_set;
	case VAR_COROUTINE: return node.key._cor == pKey->_cor;
	default:             return false;
	}
}

static VarInfo* NormalizeSetStringKey(CNeoVMImpl* vm, VarInfo* pKey, VarInfo& normalized, bool forInsert)
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

int SetInfo::FindNodeIndex(VarInfo* pKey, u32 hash, int* pPrevious) const
{
	if (pPrevious) *pPrevious = SET_NODE_END;
	if (_BucketCapa <= 0)
		return SET_NODE_END;

	const int main = (int)(hash & _HashBase);
	if (!IsSetNodeUsed(_Bucket[main]))
		return SET_NODE_END;

	int previous = SET_NODE_END;
	for (int index = main; index != SET_NODE_END; index = _Bucket[index].next)
	{
		if (SetKeyEquals(_Bucket[index], pKey, hash))
		{
			if (pPrevious) *pPrevious = previous;
			return index;
		}
		previous = index;
	}
	return SET_NODE_END;
}

int SetInfo::FindFreeNodeIndex()
{
	// A slot is examined at most once between rehashes. Direct placements can
	// occupy slots above this cursor; they are skipped once when reached.
	while (_lastFree >= 0)
	{
		const int index = _lastFree--;
		if (!IsSetNodeUsed(_Bucket[index]))
			return index;
	}
	return SET_NODE_END;
}

int SetInfo::InsertNewNode(u32 hash)
{
	const int main = (int)(hash & _HashBase);
	if (!IsSetNodeUsed(_Bucket[main]))
	{
		_Bucket[main].next = SET_NODE_END;
		return main;
	}

	const int freeIndex = FindFreeNodeIndex();
	if (freeIndex == SET_NODE_END)
		return SET_NODE_END;

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
		_Bucket[main].next = SET_NODE_END;
		return main;
	}

	// This is the primary chain; link the new node immediately after its head.
	_Bucket[freeIndex].next = _Bucket[main].next;
	_Bucket[main].next = freeIndex;
	return freeIndex;
}

bool SetInfo::ReMap(int minCapacity)
{
	int newCapacity = _BucketCapa > 0 ? _BucketCapa : 2;
	if (minCapacity <= 0)
		minCapacity = newCapacity << 1;
	while (newCapacity < minCapacity)
		newCapacity <<= 1;

	SetNode* oldBucket = _Bucket;
	const int oldCapacity = _BucketCapa;
	const int oldHashBase = _HashBase;
	const int oldLastFree = _lastFree;

	_Bucket = new SetNode[newCapacity];
	_BucketCapa = newCapacity;
	_HashBase = newCapacity - 1;
	_lastFree = newCapacity - 1;
	for (int i = 0; i < newCapacity; ++i)
		ClearNode(_Bucket[i]);

	for (int i = 0; i < oldCapacity; ++i)
	{
		if (!IsSetNodeUsed(oldBucket[i]))
			continue;

		const int target = InsertNewNode(oldBucket[i].hash);
		if (target == SET_NODE_END)
		{
			delete[] _Bucket;
			_Bucket = oldBucket;
			_BucketCapa = oldCapacity;
			_HashBase = oldHashBase;
			_lastFree = oldLastFree;
			return false;
		}
		const int next = _Bucket[target].next;
		_Bucket[target].key = oldBucket[i].key;
		_Bucket[target].hash = oldBucket[i].hash;
		_Bucket[target].next = next;
	}

	delete[] oldBucket;
	++_mutationVersion;
	return true;
}

CollectionIterator SetInfo::FirstNode()
{
	CollectionIterator r;
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (IsSetNodeUsed(_Bucket[i]))
		{
			r._pSetNode = &_Bucket[i];
			return r;
		}
	}
	r._pSetNode = NULL;
	return r;
}

bool SetInfo::NextNode(CollectionIterator& r)
{
	if (r._pSetNode == NULL || _BucketCapa <= 0)
		return false;

	const ptrdiff_t current = r._pSetNode - _Bucket;
	for (int i = (int)current + 1; i < _BucketCapa; ++i)
	{
		if (IsSetNodeUsed(_Bucket[i]))
		{
			r._pSetNode = &_Bucket[i];
			return true;
		}
	}
	r._pSetNode = NULL;
	return false;
}

void SetInfo::Free()
{
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (IsSetNodeUsed(_Bucket[i]))
			_pVM->Var_Release(&_Bucket[i].key);
	}

	delete[] _Bucket;
	_Bucket = NULL;
	_BucketCapa = _HashBase = 0;
	_lastFree = -1;
	_itemCount = 0;
}

void SetInfo::Reserve(int sz)
{
	if (sz < 0) sz = 0;
	int minCapacity = 2;
	while ((long long)minCapacity * 3 < (long long)sz * 4)
		minCapacity <<= 1;
	if (minCapacity > _BucketCapa)
		ReMap(minCapacity);
}

bool SetInfo::Insert(VarInfo* pKey)
{
	VarInfo normalizedKey;
	pKey = NormalizeSetStringKey(_pVM, pKey, normalizedKey, true);
	if (pKey == nullptr)
		return false;

	const u32 hash = GetHashCode(pKey);
	if (FindNodeIndex(pKey, hash) != SET_NODE_END)
		return true;

	if (_BucketCapa == 0
		|| (long long)(_itemCount + 1) * 4 > (long long)_BucketCapa * 3)
	{
		if (!ReMap(_BucketCapa > 0 ? _BucketCapa << 1 : 2))
			return false;
	}

	int index = InsertNewNode(hash);
	if (index == SET_NODE_END)
	{
		// _lastFree does not move back after removals. Rebuild at the same
		// capacity to recover those holes; grow only when the table is full.
		const int minCapacity = _itemCount + 1 >= _BucketCapa
			? _BucketCapa << 1
			: _BucketCapa;
		if (!ReMap(minCapacity))
			return false;
		index = InsertNewNode(hash);
	}
	if (index == SET_NODE_END)
		return false;

	SetNode& node = _Bucket[index];
	Move_DestNoRelease(&node.key, pKey);
	node.hash = hash;
	++_itemCount;
	++_mutationVersion;
	return true;
}

void SetInfo::Insert(std::string& key)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = _pVM->StringIntern(key);
	Insert(&var);
	if (var._str->_refCount <= 0)
		_pVM->FreeString(&var);
}

void SetInfo::Insert(int key)
{
	VarInfo var;
	var.SetType(VAR_INT);
	var._int = key;
	Insert(&var);
}

void SetInfo::Remove(VarInfo* pKey)
{
	if (_BucketCapa <= 0)
		return;
	VarInfo normalizedKey;
	pKey = NormalizeSetStringKey(_pVM, pKey, normalizedKey, false);
	if (pKey == nullptr)
		return;

	const u32 hash = GetHashCode(pKey);
	int previous = SET_NODE_END;
	const int index = FindNodeIndex(pKey, hash, &previous);
	if (index == SET_NODE_END)
		return;

	SetNode& node = _Bucket[index];
	_pVM->Var_Release(&node.key);
	if (previous != SET_NODE_END)
	{
		_Bucket[previous].next = node.next;
		ClearNode(node);
	}
	else if (node.next == SET_NODE_END)
	{
		ClearNode(node);
	}
	else
	{
		// Move the next node into the head slot to retain the primary-slot invariant.
		const int next = node.next;
		const SetNode moved = _Bucket[next];
		node.key = moved.key;
		node.hash = moved.hash;
		node.next = moved.next;
		ClearNode(_Bucket[next]);
	}

	--_itemCount;
	++_mutationVersion;
}

bool SetInfo::Find(VarInfo* pKey)
{
	VarInfo normalizedKey;
	pKey = NormalizeSetStringKey(_pVM, pKey, normalizedKey, false);
	if (pKey == nullptr)
		return false;

	return FindNodeIndex(pKey, GetHashCode(pKey)) != SET_NODE_END;
}

bool SetInfo::Find(std::string& key)
{
	if (_BucketCapa <= 0)
		return false;

	StringInfo* pKey = _pVM->StringFind(key);
	if (pKey == nullptr)
		return false;

	const u32 hash = pKey->GetHash();
	const int main = (int)(hash & _HashBase);
	if (!IsSetNodeUsed(_Bucket[main]))
		return false;
	for (int index = main; index != SET_NODE_END; index = _Bucket[index].next)
	{
		SetNode& node = _Bucket[index];
		if (node.hash == hash && node.key.GetType() == VAR_STRING && node.key._str == pKey)
			return true;
	}
	return false;
}

bool SetInfo::ToList(std::vector<VarInfo*>& lst)
{
	lst.resize(_itemCount);
	int count = 0;
	for (int i = 0; i < _BucketCapa; ++i)
	{
		if (IsSetNodeUsed(_Bucket[i]))
			lst[count++] = &_Bucket[i].key;
	}
	return count == _itemCount;
}

};
