#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMList.h"


/*
TableIterator ListInfo::FirstNode()
{
	TableIterator r;
	for (int iBucket = 0; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		TableNode*	pCur = pBucket->pFirst;
		if(pCur)
		{
			r._pNode = pCur;
			return r;
		}
	}
	r._pNode = NULL;
	return r;
}
bool ListInfo::NextNode(TableIterator& r)
{
	if (r._pNode == NULL)
		return false;

	{
		TableNode*	pCur = r._pNode->pNext;
		if (pCur)
		{
			r._pNode = pCur;
			return true;
		}
	}

	for (int iBucket = (r._pNode->hash & _HashBase) + 1; iBucket < _BucketCapa; iBucket++)
	{
		TableBucket* pBucket = &_Bucket[iBucket];

		if(pBucket->pFirst)
		{
			r._pNode = pBucket->pFirst;
			return true;
		}
	}
	r._pNode = NULL;
	return false;
}

void ListInfo::Var_ReleaseInternal(CNeoVM* pVM, VarInfo *d)
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
	case VAR_COROUTINE:
		if (--d->_cor->_refCount <= 0)
			pVM->FreeCoroutine(d);
		d->_cor = NULL;
		break;
	default:
		break;
	}
	d->ClearType();
}
*/
void ListInfo::Resize(int size)
{
	if (size < 0) size = 0;

	if (_BucketCapa < size)
	{
		VarInfo* pNew = new VarInfo[size];
		if (_itemCount > 0)
			memcpy(pNew, _Bucket, sizeof(VarInfo) * _itemCount);
		if(_BucketCapa > 0)
			delete[] _Bucket;

		_Bucket = pNew;
		for (int i = _itemCount; i < size; i++)
			_Bucket[i].ClearType();
	}
	_itemCount = size;
}

void ListInfo::Free()
{
	if (_BucketCapa <= 0)
		return;

	for (int i = 0; i < _itemCount; i++)
	{
		Var_Release(_pVM, &_Bucket[i]);
	}

	delete[] _Bucket;
	_BucketCapa = 0;
	_itemCount = 0;
}


//int g_MaxList = 0;
void ListInfo::Insert(VarInfo* pValue)
{
	if (_itemCount >= _BucketCapa * 4)
	{
	}

}

void ListInfo::Var_ReleaseInternal(CNeoVM* pVM, VarInfo *d)
{
	switch (d->GetType())
	{
	case VAR_STRING:
		if (--d->_str->_refCount <= 0)
			pVM->FreeString(d);
		d->_str = NULL;
		break;
	case VAR_LIST:
		if (--d->_lst->_refCount <= 0)
			pVM->FreeList(d->_lst);
		d->_lst = NULL;
		break;
	case VAR_TABLE:
		if (--d->_tbl->_refCount <= 0)
			pVM->FreeTable(d->_tbl);
		d->_tbl = NULL;
		break;
	case VAR_COROUTINE:
		if (--d->_cor->_refCount <= 0)
			pVM->FreeCoroutine(d);
		d->_cor = NULL;
		break;
	default:
		break;
	}
	d->ClearType();
}
