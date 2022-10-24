#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMTable.h"

#define IS_HASH_FLAG(flags, h) ((flags[h / TFLAG_BITS] & (1 << (h % TFLAG_BITS))) != 0)
#define NO_HASH_FLAG(flags, h) ((flags[h / TFLAG_BITS] & (1 << (h % TFLAG_BITS))) == 0)
#define SET_HASH_FLAG(flags, h) (flags[h / TFLAG_BITS] |= (1 << (h % TFLAG_BITS)))

#define BIT_LOOP

u32 GetHashCode(const std::string& str)
{
	static std::hash<std::string> hash{};
	const size_t       hash_value = hash(str);
	return (u32)hash_value;
}

u32 GetHashCode(u8 *buffer, int len)
{
	u32 h = 0;
	while (len-- > 0)
	{
		h |= buffer[len] << ((len & 0x03) * 8);
	}
	return h;
}

u32 GetHashCode(VarInfo *p)
{
	switch (p->GetType())
	{
	case VAR_NONE:
		return 0;
	case VAR_BOOL:
		return p->_bl;
	case VAR_INT:
		return (u32)p->_int;
	case VAR_FLOAT:
		return GetHashCode((u8*)&p->_float, sizeof(p->_float));
	case VAR_STRING:
		//return GetHashCode((u8*)p->_str->_str.c_str(), (int)p->_str->_str.length());
		return GetHashCode(p->_str->_str);
	case VAR_TABLE:
		return GetHashCode((u8*)p->_tbl, sizeof(p->_tbl));
	//case VAR_TABLEFUN:
	//	return GetHashCode((u8*)&p->_fun, sizeof(p->_fun));
	case VAR_FUN:
		return (u32)p->_fun_index;
		break;
	}
	return 0;
}

TableIterator TableInfo::FirstNode()
{
	TableIterator r;
	r._pNode = NULL;

	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		TableNode*	pCur = pBucket->pFirst;
		if(pCur)
		{
			r._bucket = iBucket;
			r._pNode = pCur;
			return r;
		}
	}
	return r;
}
bool TableInfo::NextNode(TableIterator& r)
{
	if (r._pNode == NULL)
		return false;

	{
		TableNode*	pCur = r._pNode->pBucektNext;
		if (pCur)
		{
			//r._bucket = iBucket;
			r._pNode = pCur;
			return true;
		}
	}

	for (int iBucket = r._bucket + 1; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		if(pBucket->pFirst)
		{
			r._bucket = iBucket;
			r._pNode = pBucket->pFirst;
			return true;
		}
	}
	r._pNode = NULL;
	return false;
}

void TableInfo::Var_ReleaseInternal(CNeoVM* pVM, VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			pVM->FreeString(d);
		d->_str = NULL;
		break;
	case VAR_TABLE:
		if (--d->_tbl->_refCount <= 0)
			pVM->FreeTable(d);
		d->_tbl = NULL;
		break;
	default:
		break;
	}
	d->ClearType();
}

void TableInfo::Free(CNeoVM* pVM)
{
	if (_BucketCapa <= 0)
		return;

	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		TableNode*	pFirst = pBucket->pFirst;
		if (pFirst == NULL)
			continue;

		TableNode*	pCur = pFirst;
		while (pCur)
		{
			Var_Release(pVM, &pCur->key);
			Var_Release(pVM, &pCur->value);
		
			TableNode* pPre = pCur;
			pCur = pCur->pBucektNext;
			pVM->m_sPool_TableNode.Confer(pPre);
		}
	}
	delete[] _Bucket;
	_BucketCapa = _HashBase = 0;
	_itemCount = 0;
}

void TableInfo::Insert(CNeoVM* pVM, std::string& Key, VarInfo* pValue)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = pVM->StringAlloc(Key);
	VarInfo* pKey = &var;

	Insert(NULL, &var, pValue);
}

bool	TableBucket::Pop_Used(TableNode* pTar)
{
	TableNode* pCur = pFirst;
	TableNode* pPre = NULL;
	while (pCur)
	{
		if (pCur == pTar)
		{
			if (pPre == NULL)
				pFirst = pCur->pBucektNext;
			else
				pPre->pBucektNext = pCur->pBucektNext;
			pTar->pBucektNext = NULL;
#ifdef _DEBUG
			//--_size_use;
#endif
			return true;
		}
		pPre = pCur;
		pCur = pCur->pBucektNext;
	}
	return false;
}
TableNode* TableBucket::Find(VarInfo* pKey)
{
	TableNode* pCur = pFirst;
	switch (pKey->GetType())
	{
	case VAR_NONE:
		while(pCur)
		{
			if (pCur->key.GetType() == VAR_NONE)
				return pCur;
			pCur = pCur->pBucektNext;
		}
		break;
	case VAR_BOOL:
		{
			bool b = pKey->_bl;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_BOOL)
				{
					if (pCur->key._bl == b)
						return pCur;
				}
				pCur = pCur->pBucektNext;
			}
		}
		break;
	case VAR_INT:
		{
			int iKey = pKey->_int;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_INT)
				{
					if (pCur->key._int == iKey)
						return pCur;
				}
				pCur = pCur->pBucektNext;
			}
		}
		break;
	case VAR_FLOAT:
		{
			auto fKey = pKey->_float;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_FLOAT)
				{
					if (pCur->key._float == fKey)
						return pCur;
				}
				pCur = pCur->pBucektNext;
			}
		}
		break;
	case VAR_STRING:
		{
			std::string& str = pKey->_str->_str;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_STRING)
				{
					if (pCur->key._str->_str == str)
						return pCur;
				}
				pCur = pCur->pBucektNext;
			}
		}
		break;
	case VAR_TABLE:
		{
			TableInfo* pTableInfo = pKey->_tbl;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_TABLE)
				{
					if (pCur->key._tbl == pTableInfo)
						return pCur;
				}
				pCur = pCur->pBucektNext;
			}
		}
		break;
	//case VAR_TABLEFUN:
	//	{
	//		Neo_NativeFunction funKey = pKey->_fun._func;
	//		for (int i = _size_use - 1; i >= 0; i--)
	//		{
	//			if (table[i].key.GetType() == VAR_TABLEFUN)
	//			{
	//				if (table[i].key._fun._func == funKey)
	//					return i;
	//			}
	//		}
	//	}
	//	break;
	case VAR_FUN:
		{
			int iFunKey = pKey->_fun_index;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_FUN)
				{
					if (pCur->key._fun_index == iFunKey)
						return pCur;
				}
				pCur = pCur->pBucektNext;
			}
		}
		break;
	}
	return NULL;
}

//int g_MaxList = 0;
void TableInfo::Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue)
{
	u32 hash = GetHashCode(pKey);
	if (_itemCount >= _BucketCapa * 2)
	{
		if(_BucketCapa > 0)
			delete[] _Bucket;
		if (_BucketCapa == 0)
			_BucketCapa = 1;
		_BucketCapa *= 2;
		_Bucket = new TableBucket[_BucketCapa];
		memset(_Bucket, 0, sizeof(TableBucket) * _BucketCapa);
		_HashBase = _BucketCapa - 1;

		TableNode* pCur = _pHead;
		while (pCur)
		{
			_Bucket[pCur->hash & _HashBase].Add_NoCheck(pCur);
			pCur = pCur->pNext;
		}
	}

	TableBucket* pBucket = &_Bucket[hash & _HashBase];
	TableNode* pFindNode;
	if (pBucket->pFirst)
		pFindNode = pBucket->Find(pKey);
	else
		pFindNode = NULL;
	if(pFindNode == NULL)
	{
		_itemCount++;

		TableNode* pCur = (TableNode*)pVMW->_pVM->m_sPool_TableNode.Receive();
		if (_pHead == NULL)
		{
			_pHead = pCur;
			pCur->pPre = NULL;
			pCur->pNext = NULL;
		}
		else
		{	// Insert To Head
			_pHead->pPre = pCur;
			pCur->pNext = _pHead;
			_pHead = pCur;
		}

		pBucket->Add_NoCheck(pCur);

		pCur->hash = hash;

		pCur->key = *pKey;
		pCur->value = *pValue;

		if (pKey->IsAllocType()) Var_AddRef(pKey);
		if (pValue->IsAllocType()) Var_AddRef(pValue);
	}
	else // Replace
	{
		TableNode* pCur = pFindNode;
		if (pVMW)
		{
			pVMW->Move(&pCur->key, pKey);
			pVMW->Move(&pCur->value, pValue);
		}
		else
		{
			pCur->key = *pKey;
			pCur->value = *pValue;
		}
	}
}
void TableInfo::Remove(CNeoVMWorker* pVMW, VarInfo* pKey)
{
	if (_BucketCapa <= 0)
		return;
	u32 hash = GetHashCode(pKey);
	TableBucket* pBucket = &_Bucket[hash & _HashBase];

	TableNode* pCur = pBucket->Find(pKey);
	if (pCur == NULL)
		return;

	pVMW->Var_Release(&pCur->key);
	pVMW->Var_Release(&pCur->value);

	pBucket->Pop_Used(pCur);

	if (_pHead == pCur)
	{
		_pHead = pCur->pNext;
		if (pCur->pNext)
			pCur->pNext->pPre = NULL;
	}
	else
	{
		if (pCur->pNext)
			pCur->pNext->pPre = pCur->pPre;
		pCur->pPre->pNext = pCur->pNext;
	}

	pVMW->_pVM->m_sPool_TableNode.Confer(pCur);

	_itemCount--;
}

TableBucket* TableInfo::GetTableBucket(VarInfo *pKey)
{
	u32 hash = GetHashCode(pKey);

	return &_Bucket[hash & _HashBase];
}


VarInfo* TableInfo::GetTableItem(VarInfo *pKey)
{
	if (_BucketCapa <= 0)
		return NULL;
	TableBucket* Bucket = GetTableBucket(pKey);
	if (Bucket == NULL)
		return NULL;

	TableNode* pCur = Bucket->Find(pKey);
	if (pCur == 0)
		return NULL;

	return &pCur->value;
}
VarInfo* TableInfo::GetTableItem(std::string& key)
{
	if (_BucketCapa <= 0)
		return NULL;
	u32 hash = GetHashCode(key);

	TableBucket* pBucket = &_Bucket[hash & _HashBase];
	TableNode* pCur = pBucket->pFirst;
	while (pCur)
	{
		if (pCur->key.GetType() == VAR_STRING)
		{
			if (pCur->key._str->_str == key)
				return &pCur->value;
		}
		pCur = pCur->pBucektNext;
	}
	return NULL;
}

bool TableInfo::ToList(std::vector<VarInfo*>& lst)
{
	lst.resize(_itemCount);
	int cnt = 0;

	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		TableNode*	pFirst = pBucket->pFirst;
		if (pFirst == NULL)
			continue;

		TableNode*	pCur = pFirst;
		while (pCur)
		{
			lst[cnt++] = &pCur->value;
			pCur = pCur->pBucektNext;
		}
	}

	if (cnt != _itemCount)
		return false;
	return true;
}

struct NeoSortLocal
{
	CNeoVMWorker*	m_pN;
	int				m_compare;

	NeoSortLocal(CNeoVMWorker* pN, int compare) : m_pN(pN), m_compare(compare) {}
	bool operator () (VarInfo* a, VarInfo* b)
	{
		VarInfo* args[2];
		VarInfo* r;
		args[0] = a;
		args[1] = b;
		m_pN->testCall(&r, m_compare, args, 2);
		if (r->GetType() == VAR_BOOL)
		{
			return r->_bl;
		}
		return false;
	}
};

void NVM_QuickSort(CNeoVMWorker* pN, int compare, std::vector<VarInfo*>& lst)
{
	std::sort(lst.begin(), lst.end(), NeoSortLocal(pN, compare));
}

