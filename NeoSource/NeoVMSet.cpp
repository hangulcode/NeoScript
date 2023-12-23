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
	case VAR_INT:
		return (u32)p->_int;
	case VAR_FLOAT:
		return GetHashCode((u8*)&p->_float, sizeof(p->_float));
	case VAR_BOOL:
		return p->_bl;
	case VAR_NONE:
		return 0;
	case VAR_FUN:
		return (u32)p->_fun_index;
		break;
	case VAR_FUN_NATIVE:
		return (u32)(uintptr_t)p->_funPtr;
		break;
	case VAR_STRING:
		//return GetHashCode(p->_str->_str);
		return p->_str->GetHash();
	case VAR_MAP:
		return GetHashCode((u8*)p->_tbl, sizeof(p->_tbl));
	case VAR_LIST:
		return GetHashCode((u8*)p->_lst, sizeof(p->_lst));
	case VAR_SET:
		return GetHashCode((u8*)p->_set, sizeof(p->_set));
	case VAR_COROUTINE:
		//return (u32)p->_cor->_CoroutineID;
		return GetHashCode((u8*)p->_cor, sizeof(p->_cor));
	default:
		break;
	}
	return 0;
}

CollectionIterator SetInfo::FirstNode()
{
	CollectionIterator r;
	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		SetBucket* pBucket = &_Bucket[iBucket];

		SetNode*	pCur = pBucket->pFirst;
		if(pCur)
		{
			r._pSetNode = pCur;
			return r;
		}
	}
	r._pSetNode = NULL;
	return r;
}
bool SetInfo::NextNode(CollectionIterator& r)
{
	if (r._pSetNode == NULL)
		return false;

	{
		SetNode*	pCur = r._pSetNode->pNext;
		if (pCur)
		{
			r._pSetNode = pCur;
			return true;
		}
	}

	for (int iBucket = (r._pSetNode->hash & _HashBase) + 1; iBucket < _BucketCapa; iBucket++)
	{
		SetBucket* pBucket = &_Bucket[iBucket];

		if(pBucket->pFirst)
		{
			r._pSetNode = pBucket->pFirst;
			return true;
		}
	}
	r._pSetNode = NULL;
	return false;
}

void SetInfo::Free()
{
	if (_BucketCapa <= 0)
		return;

	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		SetBucket* pBucket = &_Bucket[iBucket];

		SetNode*	pFirst = pBucket->pFirst;
		if (pFirst == NULL)
			continue;

		SetNode*	pCur = pFirst;
		while (pCur)
		{
			SetNode*	pNext = pCur->pNext;

			_pVM->Var_Release(&pCur->key);

			_pVM->m_sPool_SetNode.Confer(pCur);

			pCur = pNext;
		}
	}

	delete[] _Bucket;
	_BucketCapa = _HashBase = 0;
	_itemCount = 0;
}
bool	SetBucket::Pop_Used(SetNode* pTar)
{
	SetNode* pCur = pFirst;
	SetNode* pPre = NULL;
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
SetNode* SetBucket::Find(VarInfo* pKey, u32 hash)
{
	SetNode* pCur = pFirst;
	switch (pKey->GetType())
	{
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
	case VAR_NONE:
		while (pCur)
		{
			if (pCur->key.GetType() == VAR_NONE)
				return pCur;
			pCur = pCur->pNext;
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
	case VAR_MAP:
		{
			MapInfo* pInfo = pKey->_tbl;
			while (pCur)
			{
				if (pCur->key.GetType() == VAR_MAP)
				{
					if (pCur->key._tbl == pInfo)
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
	default:
		break;
	}
	return NULL;
}


bool SetInfo::Find(VarInfo *pKey)
{
	if (_BucketCapa <= 0)
		return false;
	u32 hash = GetHashCode(pKey);

	SetBucket* Bucket = &_Bucket[hash & _HashBase];


	SetNode* pCur = Bucket->Find(pKey, hash);
	if (pCur == 0)
		return false;

	return true;
}
bool SetInfo::Find(std::string& key)
{
	if (_BucketCapa <= 0)
		return false;
	u32 hash = GetHashCode(key);

	SetBucket* pBucket = &_Bucket[hash & _HashBase];
	SetNode* pCur = pBucket->pFirst;
	while (pCur)
	{
		if (pCur->key.GetType() == VAR_STRING)
		{
			if (pCur->key._str->_str == key)
				return true;
		}
		pCur = pCur->pNext;
	}
	return false;
}
void SetInfo::Reserve(int sz)
{
	if (sz < 0) sz = 0;
	if (sz <= _BucketCapa) return;

	if (sz >= _BucketCapa)
	{
		SetBucket* Old_Bucket = _Bucket;
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
		_Bucket = new SetBucket[_BucketCapa];
		memset(_Bucket, 0, sizeof(SetBucket) * _BucketCapa);
		_HashBase = _BucketCapa - 1;


		for (int iBucket = 0; iBucket < Old_BucketCapa; iBucket++)
		{
			SetBucket* pBucket = &Old_Bucket[iBucket];

			SetNode*	pFirst = pBucket->pFirst;
			if (pFirst == NULL)
				continue;

			SetNode*	pCur = pFirst;
			while (pCur)
			{
				SetNode*	pNext = pCur->pNext;
				_Bucket[pCur->hash & _HashBase].Add_NoCheck(pCur);

				pCur = pNext;
			}
		}

		if (Old_BucketCapa > 0)
			delete[] Old_Bucket;
	}
}


//int g_MaxList = 0;
bool SetInfo::Insert(VarInfo* pKey)
{
	if (_itemCount >= _BucketCapa * 4)
	{
		SetBucket* Old_Bucket = _Bucket;
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
		_Bucket = new SetBucket[_BucketCapa];
		memset(_Bucket, 0, sizeof(SetBucket) * _BucketCapa);
		_HashBase = _BucketCapa - 1;


		for (int iBucket = 0; iBucket < Old_BucketCapa; iBucket++)
		{
			SetBucket* pBucket = &Old_Bucket[iBucket];

			SetNode*	pFirst = pBucket->pFirst;
			if (pFirst == NULL)
				continue;

			SetNode*	pCur = pFirst;
			while (pCur)
			{
				SetNode*	pNext = pCur->pNext;
				_Bucket[pCur->hash & _HashBase].Add_NoCheck(pCur);

				pCur = pNext;
			}
		}

		if (Old_BucketCapa > 0)
			delete[] Old_Bucket;
	}

	u32 hash = GetHashCode(pKey);
	SetBucket* pBucket = &_Bucket[hash & _HashBase];
	SetNode* pFindNode;
	if (pBucket->pFirst)
		pFindNode = pBucket->Find(pKey, hash);
	else
		pFindNode = NULL;
	if (pFindNode == NULL)
	{
		_itemCount++;

		SetNode* pNew = _pVM->m_sPool_SetNode.Receive();
		Move_DestNoRelease(&pNew->key, pKey);
		pNew->hash = hash;

		pBucket->Add_NoCheck(pNew);
		return true;
	}
	else // Replace
	{
		//_pVM->Move(&pFindNode->value, pValue);
		return true;
	}
}
void SetInfo::Insert(std::string& Key)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = _pVM->StringAlloc(Key);
	VarInfo* pKey = &var;

	Insert(&var);
}
void SetInfo::Insert(int Key)
{
	VarInfo var;
	var.SetType(VAR_INT);
	var._int = Key;
	VarInfo* pKey = &var;

	Insert(pKey);
}

void SetInfo::Remove(VarInfo* pKey)
{
	if (_BucketCapa <= 0)
		return;
	u32 hash = GetHashCode(pKey);
	SetBucket* pBucket = &_Bucket[hash & _HashBase];

	SetNode* pCur = pBucket->Find(pKey, hash);
	if (pCur == NULL)
		return;

	_pVM->Var_Release(&pCur->key);
//	_pVM->Var_Release(&pCur->value);

	pBucket->Pop_Used(pCur);

	_pVM->m_sPool_SetNode.Confer(pCur);

	_itemCount--;
}


bool SetInfo::ToList(std::vector<VarInfo*>& lst)
{
	lst.resize(_itemCount);
	int cnt = 0;

	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		SetBucket* pBucket = &_Bucket[iBucket];

		SetNode*	pFirst = pBucket->pFirst;
		if (pFirst == NULL)
			continue;

		SetNode*	pCur = pFirst;
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

};
