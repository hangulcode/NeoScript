#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMList.h"

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

void ListInfo::Resize(int size)
{
	Reserve(size);
	_itemCount = size;
}
void ListInfo::Reserve(int capa)
{
	if (capa < 0) capa = 0;

	if (_BucketCapa < capa)
	{
		VarInfo* pNew = new VarInfo[capa];
		if (_itemCount > 0)
			memcpy(pNew, _Bucket, sizeof(VarInfo) * _itemCount);
		if (_BucketCapa > 0)
			delete[] _Bucket;

		_Bucket = pNew;
		for (int i = _itemCount; i < capa; i++)
			_Bucket[i].ClearType();
		_BucketCapa = capa;
	}
}
bool ListInfo::GetValue(int idx, VarInfo* pValue)
{
	if (idx < 0 || idx >= _itemCount)
		return false;
	_pVM->Move(pValue, &_Bucket[idx]);
	return true;
}
bool ListInfo::SetValue(int idx, VarInfo* pValue)
{
	if (idx < 0 || idx >= _itemCount)
		return false;
	_pVM->Move(&_Bucket[idx], pValue);
	return true;
}
bool ListInfo::SetValue(int idx, int v)
{
	if (idx < 0 || idx >= _itemCount)
		return false;
	if (_Bucket[idx].IsAllocType())
		_pVM->Var_Release(&_Bucket[idx]);
	_Bucket[idx].SetType(VAR_INT);
	_Bucket[idx]._int = v;
	return true;
}
bool ListInfo::Insert(int idx, VarInfo* pValue)
{
	if (idx < 0 || idx > _itemCount)
		return false;
	if (idx == _itemCount)
		return InsertLast(pValue);

	InsertLast(pValue);
	VarInfo tmp = _Bucket[_itemCount - 1];
	memmove(&_Bucket[idx + 1], &_Bucket[idx], sizeof(VarInfo) * (_itemCount - idx - 1)); // overlap memory
	_Bucket[idx] = tmp;
	return true;
}
bool ListInfo::InsertLast(VarInfo* pValue)
{
	if (_itemCount + 1 >= _BucketCapa)
	{
		Reserve(_BucketCapa == 0 ? 2 : _BucketCapa * 2);
	}
	_pVM->Move(&_Bucket[_itemCount++], pValue);
	return true;
}
bool ListInfo::InsertLast(const std::string& str)
{
	if (_itemCount + 1 >= _BucketCapa)
	{
		Reserve(_BucketCapa == 0 ? 2 : _BucketCapa * 2);
	}
	_pVM->Var_SetStringA(&_Bucket[_itemCount++], str);
	return true;
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
			pVM->FreeTable(d->_tbl);
		d->_tbl = NULL;
		break;
	case VAR_LIST:
		if (--d->_lst->_refCount <= 0)
			pVM->FreeList(d->_lst);
		d->_lst = NULL;
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
