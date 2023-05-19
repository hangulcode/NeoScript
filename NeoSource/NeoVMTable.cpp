#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVMImpl.h"
#include "NeoVMWorker.h"
#include "NeoVMTable.h"


extern u32 GetHashCode(const std::string& str);
extern u32 GetHashCode(u8 *buffer, int len);
extern u32 GetHashCode(VarInfo *p);

CollectionIterator TableInfo::FirstNode()
{
	CollectionIterator r;
	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		TableNode*	pCur = pBucket->pFirst;
		if(pCur)
		{
			r._pTableNode = pCur;
			return r;
		}
	}
	r._pTableNode = NULL;
	return r;
}
bool TableInfo::NextNode(CollectionIterator& r)
{
	if (r._pTableNode == NULL)
		return false;

	{
		TableNode*	pCur = r._pTableNode->pNext;
		if (pCur)
		{
			r._pTableNode = pCur;
			return true;
		}
	}

	for (int iBucket = (r._pTableNode->hash & _HashBase) + 1; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		if(pBucket->pFirst)
		{
			r._pTableNode = pBucket->pFirst;
			return true;
		}
	}
	r._pTableNode = NULL;
	return false;
}
void TableInfo::Var_Release(CNeoVMImpl* pVM, VarInfo *d)
{
	if (d->IsAllocType())
		pVM->Var_ReleaseInternal(d);
	else
		d->ClearType();
}

void TableInfo::Free()
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
			TableNode*	pNext = pCur->pNext;

			Var_Release(_pVM, &pCur->key);
			Var_Release(_pVM, &pCur->value);

			_pVM->m_sPool_TableNode.Confer(pCur);

			pCur = pNext;
		}
	}

	delete[] _Bucket;
	_BucketCapa = _HashBase = 0;
	_itemCount = 0;
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
				pFirst = pCur->pNext;
			else
				pPre->pNext = pCur->pNext;
			pTar->pNext = NULL;
#ifdef _DEBUG
			//--_size_use;
#endif
			return true;
		}
		pPre = pCur;
		pCur = pCur->pNext;
	}
	return false;
}
TableNode* TableBucket::Find(VarInfo* pKey, u32 hash)
{
	TableNode* pCur = pFirst;
	switch (pKey->GetType())
	{
	case VAR_NONE:
		while(pCur)
		{
			if (pCur->key.GetType() == VAR_NONE)
				return pCur;
			pCur = pCur->pNext;
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
				pCur = pCur->pNext;
			}
		}
		break;
	case VAR_INT:
		{
			int iKey = pKey->_int;
			while (pCur)
			{
				if (pCur->hash == hash)
				{
					if (pCur->key.GetType() == VAR_INT)
					{
						if (pCur->key._int == iKey)
							return pCur;
					}
				}
				pCur = pCur->pNext;
			}
		}
		break;
	case VAR_FLOAT:
		{
			auto fKey = pKey->_float;
			while (pCur)
			{
				if (pCur->hash == hash)
				{
					if (pCur->key.GetType() == VAR_FLOAT)
					{
						if (pCur->key._float == fKey)
							return pCur;
					}
				}
				pCur = pCur->pNext;
			}
		}
		break;
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
				pCur = pCur->pNext;
			}
		}
		break;
	case VAR_STRING:
		{
			std::string& str = pKey->_str->_str;
			while (pCur)
			{
				if (pCur->hash == hash)
				{
					if (pCur->key.GetType() == VAR_STRING)
					{
						if (pCur->key._str->_str == str)
							return pCur;
					}
				}
				pCur = pCur->pNext;
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
				pCur = pCur->pNext;
			}
		}
		break;
	case VAR_LIST:
		{
			ListInfo* pInfo = pKey->_lst;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_LIST)
				{
					if (pCur->key._lst == pInfo)
						return pCur;
				}
				pCur = pCur->pNext;
			}
		}
		break;
	case VAR_SET:
		{
			SetInfo* pInfo = pKey->_set;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_SET)
				{
					if (pCur->key._set == pInfo)
						return pCur;
				}
				pCur = pCur->pNext;
			}
		}
		break;
	case VAR_COROUTINE:
		{
			auto cor = pKey->_cor;
			while (pCur)
			{
				if (pCur->hash == hash)
				{
					if (pCur->key.GetType() == VAR_COROUTINE)
					{
						if (pCur->key._cor == cor)
							return pCur;
					}
				}
				pCur = pCur->pNext;
			}
		}
		break;
	}
	return NULL;
}

void TableInfo::Reserve(int sz)
{
	if (sz < 0) sz = 0;
	if (sz <= _BucketCapa) return;

	if (sz >= _BucketCapa)
	{
		TableBucket* Old_Bucket = _Bucket;
		int Old_BucketCapa = _BucketCapa;
		int Old_HashBase = _HashBase;

		if (_BucketCapa == 0)
			_BucketCapa = 1;
		while (true)
		{
			_BucketCapa <<= 1;
			if (_BucketCapa >= sz)
				break;
		}
		_Bucket = new TableBucket[_BucketCapa];
		memset(_Bucket, 0, sizeof(TableBucket) * _BucketCapa);
		_HashBase = _BucketCapa - 1;


		for (int iBucket = 0; iBucket < Old_BucketCapa; iBucket++)
		{
			TableBucket* pBucket = &Old_Bucket[iBucket];

			TableNode*	pFirst = pBucket->pFirst;
			if (pFirst == NULL)
				continue;

			TableNode*	pCur = pFirst;
			while (pCur)
			{
				TableNode*	pNext = pCur->pNext;
				_Bucket[pCur->hash & _HashBase].Add_NoCheck(pCur);

				pCur = pNext;
			}
		}

		if (Old_BucketCapa > 0)
			delete[] Old_Bucket;
	}
}

//int g_MaxList = 0;
VarInfo* TableInfo::Insert(VarInfo* pKey)
{
	if (_itemCount >= _BucketCapa * 4)
	{
		TableBucket* Old_Bucket = _Bucket;
		int Old_BucketCapa = _BucketCapa;
		int Old_HashBase = _HashBase;

		if (_BucketCapa == 0)
			_BucketCapa = 1;
		while (true)
		{
			_BucketCapa <<= 1;
			if (_BucketCapa > _itemCount)
				break;
		}
		_Bucket = new TableBucket[_BucketCapa];
		memset(_Bucket, 0, sizeof(TableBucket) * _BucketCapa);
		_HashBase = _BucketCapa - 1;


		for (int iBucket = 0; iBucket < Old_BucketCapa; iBucket++)
		{
			TableBucket* pBucket = &Old_Bucket[iBucket];

			TableNode*	pFirst = pBucket->pFirst;
			if (pFirst == NULL)
				continue;

			TableNode*	pCur = pFirst;
			while (pCur)
			{
				TableNode*	pNext = pCur->pNext;
				_Bucket[pCur->hash & _HashBase].Add_NoCheck(pCur);

				pCur = pNext;
			}
		}

		if (Old_BucketCapa > 0)
			delete[] Old_Bucket;
	}

	u32 hash = GetHashCode(pKey);
	TableBucket* pBucket = &_Bucket[hash & _HashBase];
	TableNode* pFindNode;
	if (pBucket->pFirst)
		pFindNode = pBucket->Find(pKey, hash);
	else
		pFindNode = NULL;
	if (pFindNode == NULL)
	{
		_itemCount++;

		TableNode* pNew = _pVM->m_sPool_TableNode.Receive();
		INeoVM::Move_DestNoRelease(&pNew->key, pKey);
		pNew->value.ClearType();
		//INeoVM::Move_DestNoRelease(&pNew->value, pValue);
		pNew->hash = hash;

		pBucket->Add_NoCheck(pNew);
		return &pNew->value;
	}
	else // Replace
	{
		//_pVM->Move(&pFindNode->value, pValue);
		return &pFindNode->value;
	}
}
void TableInfo::Insert(std::string& Key, VarInfo* pValue)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = _pVM->StringAlloc(Key);
	VarInfo* pKey = &var;

	Insert(&var, pValue);
}
void TableInfo::Insert(VarInfo* pKey, VarInfo* pValue)
{
	VarInfo* pDest = Insert(pKey);
	if (pDest == NULL) return;
	_pVM->Move(pDest, pValue);
}
void TableInfo::Insert(VarInfo* pKey, int v)
{
	VarInfo* pDest = Insert(pKey);
	if (pDest == NULL) return;
	pDest->SetType(VAR_INT);
	pDest->_int = v;
}
void TableInfo::Insert(int Key, VarInfo* pValue)
{
	VarInfo var;
	var.SetType(VAR_INT);
	var._int = Key;
	VarInfo* pKey = &var;

	Insert(pKey, pValue);
}

void TableInfo::Insert(int Key, int v)
{
	VarInfo var;
	var.SetType(VAR_INT);
	var._int = Key;
	VarInfo* pKey = &var;

	VarInfo* pDest = Insert(pKey);
	if (pDest == NULL) return;
	pDest->SetType(VAR_INT);
	pDest->_int = v;
}

void TableInfo::Remove(VarInfo* pKey)
{
	if (_BucketCapa <= 0)
		return;
	u32 hash = GetHashCode(pKey);
	TableBucket* pBucket = &_Bucket[hash & _HashBase];

	TableNode* pCur = pBucket->Find(pKey, hash);
	if (pCur == NULL)
		return;

	_pVM->Var_Release(&pCur->key);
	_pVM->Var_Release(&pCur->value);

	pBucket->Pop_Used(pCur);

	_pVM->m_sPool_TableNode.Confer(pCur);

	_itemCount--;
}

VarInfo* TableInfo::Find(VarInfo *pKey)
{
	if (_BucketCapa <= 0)
		return NULL;
	u32 hash = GetHashCode(pKey);

	TableBucket* Bucket = &_Bucket[hash & _HashBase];


	TableNode* pCur = Bucket->Find(pKey, hash);
	if (pCur == 0)
		return NULL;

	return &pCur->value;
}
VarInfo* TableInfo::Find(std::string& key)
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
		pCur = pCur->pNext;
	}
	return NULL;
}

bool TableInfo::ToListKeys(std::vector<VarInfo*>& lst)
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
			lst[cnt++] = &pCur->key;
			pCur = pCur->pNext;
		}
	}

	if (cnt != _itemCount)
		return false;
	return true;
}
bool TableInfo::ToListValues(std::vector<VarInfo*>& lst)
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
			pCur = pCur->pNext;
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
		VarInfo args[2];
		args[0] = *a;
		args[1] = *b;
		VarInfo* r = m_pN->testCall(m_compare, args, 2);
		if (r && r->GetType() == VAR_BOOL)
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

