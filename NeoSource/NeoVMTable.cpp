#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMTable.h"

#define MAX_TABLE	128

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
		return GetHashCode((u8*)p->_str->_str.c_str(), (int)p->_str->_str.length());
	case VAR_TABLE:
		return GetHashCode((u8*)p->_tbl, sizeof(p->_tbl));
	case VAR_TABLEFUN:
		return GetHashCode((u8*)&p->_fun, sizeof(p->_fun));
	case VAR_FUN:
		return (u32)p->_fun_index;
		break;
	}
	return 0;
}

TableIterator TableInfo::FirstNode()
{
	TableIterator r;
	r._bocket = NULL;

	if(_bocket1 == NULL)
		return r;
	for (int hash1 = 0; hash1 < MAX_TABLE; hash1++)
	{
		TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
		if (bocket2 == NULL) continue;

		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			TableBocket3* bocket3 = bocket2[hash2]._bocket3;
			if (bocket3 == NULL) continue;

			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if (bocket2[hash2]._capa[hash3] == 0)
					continue;
				TableBocket3* bocket = &bocket3[hash3];
				if (bocket->_size_use == 0) continue;

				r._hash1 = hash1;
				r._hash2 = hash2;
				r._hash3 = hash3;
				r._offset = 0;
				r._bocket = bocket;
				return r;
			}
		}
	}
	return r;
}
TableIterator TableInfo::NextNode(TableIterator r)
{
	TableBocket3* bocket = r._bocket;
	if (bocket == NULL) return r;

	if (r._offset + 1 < bocket->_size_use)
	{
		r._offset++;
		return r;
	}
	
	{
		TableBocket2* bocket2 = _bocket1[r._hash1]._bocket2;
		TableBocket3* bocket3 = bocket2[r._hash2]._bocket3;
		for (int hash3 = r._hash3 + 1; hash3 < MAX_TABLE; hash3++)
		{
			if (bocket2[r._hash2]._capa[hash3] == 0)
				continue;
			TableBocket3* bocket = &bocket3[hash3];
			if (bocket->_size_use == 0) continue;

			//r._hash1 = hash1;
			//r._hash2 = hash2;
			r._hash3 = hash3;
			r._offset = 0;
			r._bocket = bocket;
			return r;
		}
	}

	TableBocket2* bocket2 = _bocket1[r._hash1]._bocket2;
	for (int hash2 = r._hash2 + 1; hash2 < MAX_TABLE; hash2++)
	{
		TableBocket3* bocket3 = bocket2[hash2]._bocket3;
		if (bocket3 == NULL) continue;

		for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
		{
			if (bocket2[hash2]._capa[hash3] == 0)
				continue;
			TableBocket3* bocket = &bocket3[hash3];
			if (bocket->_size_use == 0) continue;

			//r._hash1 = hash1;
			r._hash2 = hash2;
			r._hash3 = hash3;
			r._offset = 0;
			r._bocket = bocket;
			return r;
		}
	}

	for (int hash1 = r._hash1 + 1; hash1 < MAX_TABLE; hash1++)
	{
		TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
		if (bocket2 == NULL) continue;

		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			TableBocket3* bocket3 = bocket2[hash2]._bocket3;
			if (bocket3 == NULL) continue;

			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if (bocket2[hash2]._capa[hash3] == 0)
					continue;
				TableBocket3* bocket = &bocket3[hash3];
				if (bocket->_size_use == 0) continue;

				r._hash1 = hash1;
				r._hash2 = hash2;
				r._hash3 = hash3;
				r._offset = 0;
				r._bocket = bocket;
				return r;
			}
		}
	}
	r._bocket = NULL;
	return r;
}

void TableInfo::Var_Release(CNeoVM* pVM, VarInfo *d)
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
	if (_bocket1 == NULL) 
		return;
	for (int hash1 = 0; hash1 < MAX_TABLE; hash1++)
	{
		TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
		if (bocket2 == NULL) continue;

		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			TableBocket3* bocket3 = bocket2[hash2]._bocket3;
			if (bocket3 == NULL) continue;

			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if (bocket2[hash2]._capa[hash3] == 0)
					continue;
				TableBocket3* bocket = &bocket3[hash3];
				if (bocket->_size_use != 0)
				{
					for (int i = bocket->_size_use - 1; i >= 0; i--)
					{
						TableNode* pCur = &bocket->_table[i];
						Var_Release(pVM, &pCur->key);
						Var_Release(pVM, &pCur->value);
					}
				}
				if(bocket->_table && bocket2[hash2]._capa[hash3] > DefualtTableSize)
					free(bocket->_table);
			}
			free(bocket3);
		}
		free(bocket2);
	}
	free(_bocket1);
	_bocket1 = NULL;
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

int TableBocket3::Find(VarInfo* pKey)
{
	if (_size_use == 0) return -1;
	TableNode*	table = _table;
	switch (pKey->GetType())
	{
	case VAR_NONE:
		for (int i = _size_use - 1; i >= 0; i--)
		{
			if (table[i].key.GetType() == VAR_NONE)
				return i;
		}
		break;
	case VAR_BOOL:
		{
			bool b = pKey->_bl;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_BOOL)
				{
					if (table[i].key._bl == b)
						return i;
				}
			}
		}
		break;
	case VAR_INT:
		{
			int iKey = pKey->_int;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_INT)
				{
					if (table[i].key._int == iKey)
						return i;
				}
			}
		}
		break;
	case VAR_FLOAT:
		{
			auto fKey = pKey->_float;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_FLOAT)
				{
					if (table[i].key._float == fKey)
						return i;
				}
			}
		}
		break;
	case VAR_STRING:
		{
			std::string& str = pKey->_str->_str;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_STRING)
				{
					if (table[i].key._str->_str == str)
						return i;
				}
			}
		}
		break;
	case VAR_TABLE:
		{
			TableInfo* pTableInfo = pKey->_tbl;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_TABLE)
				{
					if (table[i].key._tbl == pTableInfo)
						return i;
				}
			}
		}
		break;
	case VAR_TABLEFUN:
		{
			Neo_NativeFunction funKey = pKey->_fun._func;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_TABLEFUN)
				{
					if (table[i].key._fun._func == funKey)
						return i;
				}
			}
		}
		break;
	case VAR_FUN:
		{
			int iFunKey = pKey->_fun_index;
			for (int i = _size_use - 1; i >= 0; i--)
			{
				if (table[i].key.GetType() == VAR_FUN)
				{
					if (table[i].key._fun_index == iFunKey)
						return i;
				}
			}
		}
		break;
	}
	return -1;
}

void TableInfo::Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue)
{
	u32 hash = GetHashCode(pKey);
	u32 hash3 = hash % MAX_TABLE;
	u32 hash2 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	if (_bocket1 == NULL)
	{
		_bocket1 = (TableBocket1*)malloc(sizeof(TableBocket1) * MAX_TABLE);
		memset(_bocket1, 0, sizeof(TableBocket1) * MAX_TABLE);

//		m_sPool3.Init(sizeof(TableNode) * DefualtTableSize, 100);
	}

	TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
	if (bocket2 == NULL)
	{
		bocket2 = (TableBocket2*)malloc(sizeof(TableBocket2) * MAX_TABLE);
		memset(bocket2, 0, sizeof(TableBocket2) * MAX_TABLE);
		_bocket1[hash1]._bocket2 = bocket2;
	}

	TableBocket3* bocket3 = bocket2[hash2]._bocket3;
	if (bocket3 == NULL)
	{
		bocket3 = (TableBocket3*)malloc(sizeof(TableBocket3) * MAX_TABLE);
		memset(bocket3, 0, sizeof(TableBocket3) * MAX_TABLE);
		bocket2[hash2]._bocket3 = bocket3;

		bocket2[hash2]._capa = (int*)malloc(sizeof(int) * MAX_TABLE);
		memset(bocket2[hash2]._capa, 0, sizeof(int) * MAX_TABLE);
	}

	TableBocket3* bocket = &bocket3[hash3];
	int iSelect = -1;
	if (bocket2[hash2]._capa[hash3] == 0)
	{
		//TableNode* table = (TableNode*)malloc(sizeof(TableNode) * DefualtTableSize);
		TableNode* table = bocket->_default;
		for (int i = 0; i < DefualtTableSize; i++) { table[i].key.SetType(VAR_NONE); table[i].value.SetType(VAR_NONE); }
		bocket->_table = table;
		bocket2[hash2]._capa[hash3] = DefualtTableSize;
		bocket->_size_use = 0;
	}
	else if (bocket->_size_use > 0)
	{
		iSelect = bocket->Find(pKey);
	}

	if (iSelect == -1)
	{
		TableNode* table;
		//if (bocket->_size_use + 1 >= bocket2[hash2]._capa[hash3])
		//{
		//	table = (TableNode*)malloc(sizeof(TableNode) * DefualtTableSize);
		//	for (int i = 0; i < DefualtTableSize; i++) { table[i].key.SetType(VAR_NONE); table[i].value.SetType(VAR_NONE); }
		//	bocket->_table = table;
		//	bocket->_size = DefualtTableSize;
		//	bocket->_size_use = 0;

		//	for (int i = 0; i < DefualtTableSize; i++)
		//		bocket->Push_Free(&bocket->_table[i]);
		//}

		if (bocket->_size_use + 1 >= bocket2[hash2]._capa[hash3])
		{
			int iPreTableSize = bocket2[hash2]._capa[hash3];
			int iNewTableSize = iPreTableSize * 2;

			table = (TableNode*)malloc(sizeof(TableNode) * iNewTableSize);
			memcpy(table, bocket->_table, sizeof(TableNode) * iPreTableSize);
			if (bocket->_table && iPreTableSize > DefualtTableSize) free(bocket->_table);

			for (int i = iPreTableSize; i < iNewTableSize; i++) { table[i].key.SetType(VAR_NONE); table[i].value.SetType(VAR_NONE); }

			bocket->_table = table;
			bocket2[hash2]._capa[hash3] = iNewTableSize;
		}
		iSelect = bocket->_size_use++;
		_itemCount++;
	}

	TableNode* pCur = &bocket->_table[iSelect];
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
void TableInfo::Remove(CNeoVMWorker* pVMW, VarInfo* pKey)
{
	TableBocket3* bocket = GetTableBocket(pKey);
	if (bocket == NULL)
		return;

	int idx = bocket->Find(pKey);
	if(idx < 0)
		return;

	TableNode* pCur = &bocket->_table[idx];
	pVMW->Var_Release(&pCur->key);
	pVMW->Var_Release(&pCur->value);

	int move_cnt = bocket->_size_use - idx - 1;
	if (move_cnt > 0)
	{
		memmove(&bocket->_table[idx], &bocket->_table[idx + 1], sizeof(TableNode) * move_cnt);
		int last = bocket->_size_use - 1;
		bocket->_table[last].key.SetType(VAR_NONE); bocket->_table[last].value.SetType(VAR_NONE);
	}

	bocket->_size_use--;
	_itemCount--;
}

TableBocket3* TableInfo::GetTableBocket(VarInfo *pKey)
{
	if (_bocket1 == NULL)
		return NULL;

	u32 hash = GetHashCode(pKey);
	u32 hash3 = hash % MAX_TABLE;
	u32 hash2 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
	if (bocket2 == NULL)
		return NULL;

	TableBocket3* bocket3 = bocket2[hash2]._bocket3;
	if (bocket3 == NULL)
		return NULL;

	return &bocket3[hash3];
}


VarInfo* TableInfo::GetTableItem(VarInfo *pKey)
{
	TableBocket3* bocket = GetTableBocket(pKey);
	if (bocket == NULL)
		return NULL;

	int idx = bocket->Find(pKey);
	if (idx < 0)
		return NULL;

	return &bocket->_table[idx].value;
}
VarInfo* TableInfo::GetTableItem(std::string& key)
{
	if (_bocket1 == NULL)
		return NULL;

	u32 hash = GetHashCode((u8*)key.c_str(), (int)key.length());
	u32 hash3 = hash % MAX_TABLE;
	u32 hash2 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
	if (bocket2 == NULL)
		return NULL;

	TableBocket3* bocket3 = bocket2[hash2]._bocket3;
	if (bocket3 == NULL)
		return NULL;

	TableBocket3* bocket = &bocket3[hash3];
	TableNode* table = bocket->_table;
	for (int i = bocket->_size_use - 1; i >= 0; i--)
	{
		if (table[i].key.GetType() == VAR_STRING)
		{
			if (table[i].key._str->_str == key)
				return &table[i].value;
		}
	}
	return NULL;
}

bool TableInfo::ToList(std::vector<VarInfo*>& lst)
{
	lst.resize(_itemCount);
	int cnt = 0;

	if (_bocket1 == NULL)
		return true;
	for (int hash1 = 0; hash1 < MAX_TABLE; hash1++)
	{
		TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
		if (bocket2 == NULL) continue;

		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			TableBocket3* bocket3 = bocket2[hash2]._bocket3;
			if (bocket3 == NULL) continue;

			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if (bocket2[hash2]._capa[hash3] == 0)
					continue;
				TableBocket3* bocket = &bocket3[hash3];
				if (bocket->_size_use == 0) continue;

				TableNode* table = bocket->_table;
				for (int i = bocket->_size_use - 1; i >= 0; i--)
				{
					lst[cnt++] = &table[i].value;
				}
			}
		}
	}
	if (cnt != _itemCount)
		return false;
	return true;
}


static void Swap(VarInfo *a, VarInfo *b)
{
	VarInfo t = *a;
	*a = *b;
	*b = t;
}
//
//static bool CompareGE(TableSortInfo* tsi, VarInfo *a, VarInfo *b)
//{
//	VarInfo* args[2];
//	VarInfo* r;
//	args[0] = a;
//	args[1] = b;
//	tsi->_pN->testCall(&r, tsi->_compareFunction, args, 2);
//	if (r->GetType() == VAR_BOOL)
//	{
//		return r->_bl;
//	}
//	return false;
//}

void quickSort(CNeoVMWorker* pN, int compare, VarInfo** array, int start, int end)
{
	int left = start + 1;
	int right = end;
	VarInfo* pivot = array[start];

	VarInfo* args[2];
	VarInfo* r;
	args[1] = pivot;

	while (left <= right)
	{
		while (left <= end)
		{
			args[0] = array[left];
			pN->testCall(&r, compare, args, 2);
			if (r->GetType() != VAR_BOOL) break; // error
			if (!r->_bl) break;
			left++;
		}
		while (right > start)
		{
			args[0] = array[right];
			pN->testCall(&r, compare, args, 2);
			if (r->GetType() != VAR_BOOL) break; // error
			if (r->_bl) break;
			right--;
		}

		if (right < left)	// ¾ù°¥¸²
			Swap(array[right], pivot);
		else
			Swap(array[right], array[left]);
	}
	
	if(start < right - 1)
		quickSort(pN, compare, array, start, right - 1);
	if(right + 1 < end)
		quickSort(pN, compare, array, right + 1, end);
}

