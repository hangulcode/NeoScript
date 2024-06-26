#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <algorithm>

#include "NeoVMImpl.h"
#include "NeoVMWorker.h"
#include "NeoVMList.h"

namespace NeoScript
{

void ListInfo::Free()
{
	if (_BucketCapa <= 0)
		return;

	for (int i = 0; i < _itemCount; i++)
	{
		_pVM->Var_Release(&_Bucket[i]);
	}

	delete[] _Bucket;
	_BucketCapa = 0;
	_itemCount = 0;
}

void ListInfo::Resize(int size)
{
	if (size < 0) size = 0;
	Reserve(size);
	_itemCount = size;
}
void ListInfo::Resize(int row, int col)
{
	if(row == 1)
	{
		Resize(col);
		return;
	}
	Resize(row);
	for(int i = 0; i < row; i++)
	{
		_pVM->Var_Release(&_Bucket[i]);
		ListInfo* p = _pVM->ListAlloc();

		_Bucket[i].SetType(VAR_LIST);
		_Bucket[i]._lst = p;
		++p->_refCount;
		p->Resize(col);
	}
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

VarInfo* ListInfo::GetValue(int idx)
{
	if (idx < 0 || idx >= _itemCount)
		return nullptr;
	return &_Bucket[idx];
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
bool ListInfo::SetValue(int idx, NS_FLOAT v)
{
	if (idx < 0 || idx >= _itemCount)
		return false;
	if (_Bucket[idx].IsAllocType())
		_pVM->Var_Release(&_Bucket[idx]);
	_Bucket[idx].SetType(VAR_FLOAT);
	_Bucket[idx]._float = v;
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

MatrixInfo ListInfo::GetMatrix()
{
	MatrixInfo info;
	info.row = info.col = -1;
	if(_itemCount <= 0)
		return info;

	if(_Bucket[0].GetType() != VAR_LIST)
	{
		for (int c = 0; c < _itemCount; c++)
		{
			if (_Bucket[c].IsNumber() == false)
				return info;
		}
		info.row = 1;
		info.col = _itemCount;
	}
	else
	{
		int row = _itemCount;
		int col = _Bucket[0]._lst->GetCount();
		for(int r = 0; r < row; r++)
		{
			if (_Bucket[r].GetType() != VAR_LIST)
				return info;
			ListInfo* pC = _Bucket[r]._lst;
			if(col != pC->GetCount())
				return info;
			for (int c = 0; c < col; c++)
			{
				if (pC->_Bucket[c].IsNumber() == false)
					return info;
			}
		}
		info.row = _itemCount;
		info.col = col;
	}
	return info;
}


};
