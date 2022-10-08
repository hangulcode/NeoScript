#include <math.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include "NeoVM.h"
#include "NeoVMWorker.h"
#include "NeoVMTable.h"

const int DefualtTableSize = 4;

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
	r._node = NULL;

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
				TableBocket3* bocket = &bocket3[hash3];

				if (bocket->_used)
				{
					r._hash1 = hash1;
					r._hash2 = hash2;
					r._hash3 = hash3;
					r._node = bocket->_used;
					return r;
				}
			}
		}
	}
	return r;
}
TableIterator TableInfo::NextNode(TableIterator r)
{
	TableNode* TN = r._node;
	if (TN == NULL) return r;

	if (TN->_next)
	{
		r._node = TN->_next;
		return r;
	}
	
	{
		TableBocket2* bocket2 = _bocket1[r._hash1]._bocket2;
		TableBocket3* bocket3 = bocket2[r._hash2]._bocket3;
		for (int hash3 = r._hash3 + 1; hash3 < MAX_TABLE; hash3++)
		{
			TableBocket3* bocket = &bocket3[hash3];

			if (bocket->_used)
			{
				//r._hash1 = hash1;
				//r._hash2 = hash2;
				r._hash3 = hash3;
				r._node = bocket->_used;
				return r;
			}
		}
	}

	TableBocket2* bocket2 = _bocket1[r._hash1]._bocket2;
	for (int hash2 = r._hash2 + 1; hash2 < MAX_TABLE; hash2++)
	{
		TableBocket3* bocket3 = bocket2[hash2]._bocket3;
		if (bocket3 == NULL) continue;

		for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
		{
			TableBocket3* bocket = &bocket3[hash3];

			if (bocket->_used)
			{
				//r._hash1 = hash1;
				r._hash2 = hash2;
				r._hash3 = hash3;
				r._node = bocket->_used;
				return r;
			}
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
				TableBocket3* bocket = &bocket3[hash3];

				if (bocket->_used)
				{
					r._hash1 = hash1;
					r._hash2 = hash2;
					r._hash3 = hash3;
					r._node = bocket->_used;
					return r;
				}
			}
		}
	}
	r._node = NULL;
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
				TableBocket3* bocket = &bocket3[hash3];
				if (bocket->_size == 0)
					continue;

				TableNode* pCur = bocket->_used;
				while (pCur)
				{
					Var_Release(pVM, &pCur->_data.key);
					Var_Release(pVM, &pCur->_data.value);
					pCur = pCur->_next;
				}
				if(bocket->_table)
					delete [] bocket->_table;
			}
			delete [] bocket3;
		}
		delete[] bocket2;
	}
	delete[] _bocket1;
	_bocket1 = NULL;
	_itemCount = 0;
}

void TableInfo::Insert(CNeoVM* pVM, std::string& Key, VarInfo* pValue)
{
	VarInfo var;
	var.SetType(VAR_STRING);
	var._str = pVM->StringAlloc(Key.c_str());
	VarInfo* pKey = &var;

	Insert(NULL, &var, pValue);
}

TableNode* FindString(TableNode* pCur, std::string& key)
{
	while (pCur)
	{
		if (pCur->_data.key.GetType() == VAR_STRING)
		{
			if (pCur->_data.key._str->_str == key)
				return pCur;
		}
		pCur = pCur->_next;
	}
	return NULL;
}
bool	TableBocket3::Pop_Used(TableNode* pTar)
{
	TableNode* pCur = _used;
	TableNode* pPre = NULL;
	while (pCur)
	{
		if (pCur == pTar)
		{
			if (pPre == NULL)
				_used = pCur->_next;
			else
				pPre->_next = pCur->_next;
			pTar->_next = NULL;
			return true;
		}
		pPre = pCur;
		pCur = pCur->_next;
	}
	return false;
}

TableNode* TableBocket3::Find(VarInfo* pKey)
{
	if (_size == 0) return NULL;
	TableNode* pCur = _used;
	switch (pKey->GetType())
	{
	case VAR_NONE:
		while (pCur)
		{
			if (pCur->_data.key.GetType() == VAR_NONE)
				return pCur;
			pCur = pCur->_next;
		}
		break;
	case VAR_BOOL:
		{
			bool b = pKey->_bl;
			while (pCur)
			{
				if (pCur->_data.key.GetType() == VAR_BOOL)
				{
					if (pCur->_data.key._bl == b)
						return pCur;
				}
				pCur = pCur->_next;
			}
		}
		break;
	case VAR_INT:
		{
			int iKey = pKey->_int;
			while (pCur)
			{
				if (pCur->_data.key.GetType() == VAR_INT)
				{
					if (pCur->_data.key._int == iKey)
						return pCur;
				}
				pCur = pCur->_next;
			}
		}
		break;
	case VAR_FLOAT:
		{
			auto fKey = pKey->_float;
			while (pCur)
			{
				if (pCur->_data.key.GetType() == VAR_FLOAT)
				{
					if (pCur->_data.key._float == fKey)
						return pCur;
				}
				pCur = pCur->_next;
			}
		}
		break;
	case VAR_STRING:
		{
			return FindString(pCur, pKey->_str->_str);
		}
		break;
	case VAR_TABLE:
		{
			TableInfo* pTableInfo = pKey->_tbl;
			while (pCur)
			{
				if (pCur->_data.key.GetType() == VAR_TABLE)
				{
					if (pCur->_data.key._tbl == pTableInfo)
						return pCur;
				}
				pCur = pCur->_next;
			}
		}
		break;
	case VAR_TABLEFUN:
		{
			Neo_NativeFunction funKey = pKey->_fun._func;
			while (pCur)
			{
				if (pCur->_data.key.GetType() == VAR_TABLEFUN)
				{
					if (pCur->_data.key._fun._func == funKey)
						return pCur;
				}
				pCur = pCur->_next;
			}
		}
		break;
	case VAR_FUN:
		{
			int iFunKey = pKey->_fun_index;
			while (pCur)
			{
				if (pCur->_data.key.GetType() == VAR_FUN)
				{
					if (pCur->_data.key._fun_index == iFunKey)
						return pCur;
				}
				pCur = pCur->_next;
			}
		}
		break;
	}
	return NULL;
}

void TableInfo::Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue)
{
	u32 hash = GetHashCode(pKey);
	u32 hash3 = hash % MAX_TABLE;
	u32 hash2 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	if (_bocket1 == NULL)
	{
		_bocket1 = new TableBocket1[MAX_TABLE];
		memset(_bocket1, 0, sizeof(TableBocket1) * MAX_TABLE);
	}

	TableBocket2* bocket2 = _bocket1[hash1]._bocket2;
	if (bocket2 == NULL)
	{
		bocket2 = new TableBocket2[MAX_TABLE];
		memset(bocket2, 0, sizeof(TableBocket2) * MAX_TABLE);
		_bocket1[hash1]._bocket2 = bocket2;
	}

	TableBocket3* bocket3 = bocket2[hash2]._bocket3;
	if (bocket3 == NULL)
	{
		bocket3 = new TableBocket3[MAX_TABLE];
		memset(bocket3, 0, sizeof(TableBocket3) * MAX_TABLE);
		bocket2[hash2]._bocket3 = bocket3;
	}

	TableBocket3* bocket = &bocket3[hash3];

	TableNode* pCur = bocket->Find(pKey);
	if (pCur == NULL)
	{
		if (bocket->_size == 0)
		{
			bocket->_table = new TableNode[DefualtTableSize];
			bocket->_size = DefualtTableSize;
			for (int i = 0; i < DefualtTableSize; i++)
				bocket->Push_Free(&bocket->_table[i]);
		}

		if (bocket->_free == NULL)
		{
			int iPreTableSize = bocket->_size;
			int iNewTableSize = iPreTableSize * 2;

			TableNode* table = new TableNode[iNewTableSize];
			memcpy(table, bocket->_table, sizeof(TableNode) * iPreTableSize);
			if (bocket->_table) delete[] bocket->_table;
			bocket->_table = table;
			bocket->_size = iNewTableSize;

			bocket->_free = NULL;
			bocket->_used = NULL;

			for (int i = 0; i < iPreTableSize; i++)
				bocket->Push_Use(&table[i]);

			for (int i = iPreTableSize; i < iNewTableSize; i++)
				bocket->Push_Free(&table[i]);

		}
		pCur = bocket->_free;
		bocket->_free = pCur->_next;
		bocket->Push_Use(pCur);
		_itemCount++;
	}

	if (pVMW)
	{
		pVMW->Move(&pCur->_data.key, pKey);
		pVMW->Move(&pCur->_data.value, pValue);
	}
	else
	{
		pCur->_data.key = *pKey;
		pCur->_data.value = *pValue;
	}
}
void TableInfo::Remove(CNeoVMWorker* pVMW, VarInfo* pKey)
{
	TableBocket3* bocket = GetTableBocket(pKey);
	if (bocket == NULL)
		return;

	TableNode* pCur = bocket->Find(pKey);
	if (pCur)
	{
		if (bocket->Pop_Used(pCur))
		{
			pVMW->Var_Release(&pCur->_data.key);
			pVMW->Var_Release(&pCur->_data.value);

			bocket->Push_Free(pCur);
			_itemCount--;
		}
	}
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

	TableNode* pCur = bocket->Find(pKey);
	if (pCur) 
		return &pCur->_data.value;
	return NULL;
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

	TableNode* pCur = FindString(bocket->_used, key);
	if (pCur) 
		return &pCur->_data.value;
	return NULL;
}


