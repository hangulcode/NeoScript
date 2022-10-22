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
	r._Bucket = NULL;

	for (int hash1 = 0; hash1 < MAX_TABLE; hash1++)
	{
		if ((hash1 % TFLAG_BITS) == 0 && _Flag1[hash1 / TFLAG_BITS] == 0)
		{
			hash1 += (TFLAG_BITS - 1);
			continue;
		}
#ifdef BIT_LOOP
		if (NO_HASH_FLAG(_Flag1, hash1))
			continue;
#endif

		TableBucket2* Bucket2 = _Bucket1[hash1]._Bucket2;
		if (Bucket2 == NULL)
			continue;
		uTFlag* Flag2 = _Bucket1[hash1]._Flag2;
		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			if ((hash2 % TFLAG_BITS) == 0 && Flag2[hash2 / TFLAG_BITS] == 0)
			{
				hash2 += (TFLAG_BITS - 1);
				continue;
			}
#ifdef BIT_LOOP
			if (NO_HASH_FLAG(Flag2, hash2))
				continue;
#endif

			TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
			if (Bucket3 == NULL)
				continue;
			uTFlag* Flag3 = Bucket2[hash2]._Flag3;
			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if ((hash3 % TFLAG_BITS) == 0 && Flag3[hash3 / TFLAG_BITS] == 0)
				{
					hash3 += (TFLAG_BITS - 1);
					continue;
				}
#ifdef BIT_LOOP
				if (NO_HASH_FLAG(Flag3, hash3))
					continue;
#endif

				TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
				if (Bucket4 == NULL)
					continue;
				uTFlag* Flag4 = Bucket3[hash3]._Flag4;
				for (int hash4 = 0; hash4 < MAX_TABLE; hash4++)
				{
					if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
					{
						hash4 += (TFLAG_BITS - 1);
						continue;
					}
#ifdef BIT_LOOP
					if (NO_HASH_FLAG(Flag4, hash4))
						continue;
#endif

					if (Bucket4[hash4]._size_use == 0)
						continue;
					TableBucket4* Bucket = &Bucket4[hash4];

					r._hash1 = hash1;
					r._hash2 = hash2;
					r._hash3 = hash3;
					r._hash4 = hash4;
					r._offset = 0;
					r._Bucket = Bucket;
					return r;
				}
			}
		}
	}
	return r;
}
bool TableInfo::NextNode(TableIterator& r)
{
	if (++r._offset < r._Bucket->_size_use)
	{
		return true;
	}
	
	{
		TableBucket2* Bucket2 = _Bucket1[r._hash1]._Bucket2;
		TableBucket3* Bucket3 = Bucket2[r._hash2]._Bucket3;
		TableBucket4* Bucket4 = Bucket3[r._hash3]._Bucket4;
		
		uTFlag* Flag4 = Bucket3[r._hash3]._Flag4;
		for (int hash4 = r._hash4 + 1; hash4 < MAX_TABLE; hash4++)
		{
			if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
			{
				hash4 += (TFLAG_BITS - 1);
				continue;
			}
#ifdef BIT_LOOP
			if (NO_HASH_FLAG(Flag4, hash4))
				continue;
#endif
			if (Bucket4[hash4]._size_use == 0)
				continue;

			//r._hash1 = hash1;
			//r._hash2 = hash2;
			//r._hash3 = hash3;
			r._hash4 = hash4;
			r._offset = 0;
			r._Bucket = &Bucket4[hash4];
			return true;
		}
	}

	{
		TableBucket2* Bucket2 = _Bucket1[r._hash1]._Bucket2;
		TableBucket3* Bucket3 = Bucket2[r._hash2]._Bucket3;

		uTFlag* Flag3 = Bucket2[r._hash2]._Flag3;
		for (int hash3 = r._hash3 + 1; hash3 < MAX_TABLE; hash3++)
		{
			if ((hash3 % TFLAG_BITS) == 0 && Flag3[hash3 / TFLAG_BITS] == 0)
			{
				hash3 += (TFLAG_BITS - 1);
				continue;
			}
#ifdef BIT_LOOP
			if (NO_HASH_FLAG(Flag3, hash3))
				continue;
#endif

			TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
			if (Bucket4 == NULL)
				continue;
			uTFlag* Flag4 = Bucket3[hash3]._Flag4;
			for (int hash4 = 0; hash4 < MAX_TABLE; hash4++)
			{
				if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
				{
					hash4 += (TFLAG_BITS - 1);
					continue;
				}
#ifdef BIT_LOOP
				if (NO_HASH_FLAG(Flag4, hash4))
					continue;
#endif
				if (Bucket4[hash4]._size_use == 0)
					continue;

				//r._hash1 = hash1;
				//r._hash2 = hash2;
				r._hash3 = hash3;
				r._hash4 = hash4;
				r._offset = 0;
				r._Bucket = &Bucket4[hash4];
				return true;
			}
		}
	}

	TableBucket2* Bucket2 = _Bucket1[r._hash1]._Bucket2;
	uTFlag* Flag2 = _Bucket1[r._hash1]._Flag2;
	for (int hash2 = r._hash2 + 1; hash2 < MAX_TABLE; hash2++)
	{
		if ((hash2 % TFLAG_BITS) == 0 && Flag2[hash2 / TFLAG_BITS] == 0)
		{
			hash2 += (TFLAG_BITS - 1);
			continue;
		}
#ifdef BIT_LOOP
		if (NO_HASH_FLAG(Flag2, hash2))
			continue;
#endif
		TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
		if (Bucket3 == NULL)
			continue;
		uTFlag* Flag3 = Bucket2[hash2]._Flag3;
		for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
		{
			if ((hash3 % TFLAG_BITS) == 0 && Flag3[hash3 / TFLAG_BITS] == 0)
			{
				hash3 += (TFLAG_BITS - 1);
				continue;
			}
#ifdef BIT_LOOP
			if (NO_HASH_FLAG(Flag3, hash3))
				continue;
#endif
			TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
			if (Bucket4 == NULL)
				continue;
			uTFlag* Flag4 = Bucket3[hash3]._Flag4;
			for (int hash4 = 0; hash4 < MAX_TABLE; hash4++)
			{
				if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
				{
					hash4 += (TFLAG_BITS - 1);
					continue;
				}
#ifdef BIT_LOOP
				if (NO_HASH_FLAG(Flag4, hash4))
					continue;
#endif
				if (Bucket4[hash4]._size_use == 0)
					continue;

				//r._hash1 = hash1;
				r._hash2 = hash2;
				r._hash3 = hash3;
				r._hash4 = hash4;
				r._offset = 0;
				r._Bucket = &Bucket4[hash4];
				return true;
			}
		}
	}

	for (int hash1 = r._hash1 + 1; hash1 < MAX_TABLE; hash1++)
	{
		if ((hash1 % TFLAG_BITS) == 0 && _Flag1[hash1 / TFLAG_BITS] == 0)
		{
			hash1 += (TFLAG_BITS - 1);
			continue;
		}
#ifdef BIT_LOOP
		if (NO_HASH_FLAG(_Flag1, hash1))
			continue;
#endif

		TableBucket2* Bucket2 = _Bucket1[hash1]._Bucket2;
		if (Bucket2 == NULL)
			continue;
		uTFlag* Flag2 = _Bucket1[hash1]._Flag2;
		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			if ((hash2 % TFLAG_BITS) == 0 && Flag2[hash2 / TFLAG_BITS] == 0)
			{
				hash2 += (TFLAG_BITS - 1);
				continue;
			}
#ifdef BIT_LOOP
			if (NO_HASH_FLAG(Flag2, hash2))
				continue;
#endif
			TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
			if (Bucket3 == NULL)
				continue;
			uTFlag* Flag3 = Bucket2[hash2]._Flag3;
			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if ((hash3 % TFLAG_BITS) == 0 && Flag3[hash3 / TFLAG_BITS] == 0)
				{
					hash3 += (TFLAG_BITS - 1);
					continue;
				}
#ifdef BIT_LOOP
				if (NO_HASH_FLAG(Flag3, hash3))
					continue;
#endif
				TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
				if (Bucket4 == NULL)
					continue;
				uTFlag* Flag4 = Bucket3[hash3]._Flag4;
				for (int hash4 = 0; hash4 < MAX_TABLE; hash4++)
				{
					if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
					{
						hash4 += (TFLAG_BITS - 1);
						continue;
					}
					if (NO_HASH_FLAG(Flag4, hash4))
						continue;
					if (Bucket4[hash4]._size_use == 0)
						continue;

					r._hash1 = hash1;
					r._hash2 = hash2;
					r._hash3 = hash3;
					r._hash4 = hash4;
					r._offset = 0;
					r._Bucket = &Bucket4[hash4];
					return true;
				}
			}
		}
	}
	r._Bucket = NULL;
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
	for (int hash1 = 0; hash1 < MAX_TABLE; hash1++)
	{
		if ((hash1 % TFLAG_BITS) == 0 && _Flag1[hash1 / TFLAG_BITS] == 0)
		{
			hash1 += (TFLAG_BITS - 1);
			continue;
		}
		if (NO_HASH_FLAG(_Flag1, hash1))
			continue;

		TableBucket2* Bucket2 = _Bucket1[hash1]._Bucket2;
		if (Bucket2 == NULL)
			continue;
		uTFlag* Flag2 = _Bucket1[hash1]._Flag2;
		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			if ((hash2 % TFLAG_BITS) == 0 && Flag2[hash2 / TFLAG_BITS] == 0)
			{
				hash2 += (TFLAG_BITS - 1);
				continue;
			}
			if (NO_HASH_FLAG(Flag2, hash2))
				continue;

			TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
			if (Bucket3 == NULL)
				continue;
			uTFlag* Flag3 = Bucket2[hash2]._Flag3;
			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if ((hash3 % TFLAG_BITS) == 0 && Flag3[hash3 / TFLAG_BITS] == 0)
				{
					hash3 += (TFLAG_BITS - 1);
					continue;
				}
				if (NO_HASH_FLAG(Flag3, hash3))
					continue;

				TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
				if (Bucket4 == NULL)
					continue;
				uTFlag* Flag4 = Bucket3[hash3]._Flag4;
				for (int hash4 = 0; hash4 < MAX_TABLE; hash4++)
				{
					if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
					{
						hash4 += (TFLAG_BITS - 1);
						continue;
					}
					if (NO_HASH_FLAG(Flag4, hash4))
						continue;

					if (Bucket4[hash4]._capa == 0)
						continue;
					TableBucket4* Bucket = &Bucket4[hash4];
					for (int i = Bucket->_size_use - 1; i >= 0; i--)
					{
						TableNode* pCur = &Bucket->_table[i];
						Var_Release(pVM, &pCur->key);
						Var_Release(pVM, &pCur->value);
					}
					if (Bucket->_table && Bucket->_capa > DefualtTableSize)
						free(Bucket->_table);
				}
				pVM->m_sPool_Bucket4.Confer(Bucket4);
			}
			pVM->m_sPool_Bucket3.Confer(Bucket3);
		}
		pVM->m_sPool_Bucket2.Confer(Bucket2);
	}
//	pVM->m_sPool_Bucket1.Confer(_Bucket1);
	memset(_Flag1, 0, sizeof(_Flag1));
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

int TableBucket4::Find(VarInfo* pKey)
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

//int g_MaxList = 0;
void TableInfo::Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue)
{
	u32 hash = GetHashCode(pKey);
	u32 hash4 = hash % MAX_TABLE;
	u32 hash3 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash2 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	TableBucket1* Bucket1 = &_Bucket1[hash1];
	if (NO_HASH_FLAG(_Flag1, hash1))
	{
		SET_HASH_FLAG(_Flag1, hash1);
//		_Bucket1 = (TableBucket1*)pVMW->_pVM->m_sPool_Bucket1.Receive();
		memset(Bucket1, 0, sizeof(TableBucket1));
	}

	if (Bucket1->_Bucket2 == NULL)
	{
		auto Bucket2T = (TableBucket2*)pVMW->_pVM->m_sPool_Bucket2.Receive();
		memset(Bucket2T, 0, sizeof(TableBucket2) * MAX_TABLE);
		Bucket1->_Bucket2 = Bucket2T;
	}
	TableBucket2* Bucket2 = &Bucket1->_Bucket2[hash2];
	SET_HASH_FLAG(Bucket1->_Flag2, hash2);

	if (Bucket2->_Bucket3 == NULL)
	{
		auto Bucket3T = (TableBucket3*)pVMW->_pVM->m_sPool_Bucket3.Receive();;
		memset(Bucket3T, 0, sizeof(TableBucket3) * MAX_TABLE);
		Bucket2->_Bucket3 = Bucket3T;
	}
	TableBucket3* Bucket3 = &Bucket2->_Bucket3[hash3];
	SET_HASH_FLAG(Bucket2->_Flag3, hash3);

	if (Bucket3->_Bucket4 == NULL)
	{
		auto Bucket4T = (TableBucket4*)pVMW->_pVM->m_sPool_Bucket4.Receive();
		//memset(Bucket3, 0, sizeof(TableBucket3) * MAX_TABLE);
		Bucket3->_Bucket4 = Bucket4T;
	}
	TableBucket4* Bucket = &Bucket3->_Bucket4[hash4];
	if (NO_HASH_FLAG(Bucket3->_Flag4, hash4))
	{
		SET_HASH_FLAG(Bucket3->_Flag4, hash4);
		//Bucket->_capa = 0;
		Bucket->_capa = DefualtTableSize;
		//TableNode* table = (TableNode*)malloc(sizeof(TableNode) * DefualtTableSize);
//		TableNode* table = Bucket->_default;
//		for (int i = 0; i < DefualtTableSize; i++) { table[i].key.SetType(VAR_NONE); table[i].value.SetType(VAR_NONE); }
		Bucket->_table = Bucket->_default;
		Bucket->_size_use = 1;

		_itemCount++;

		TableNode* pCur = &Bucket->_table[0];
		pCur->key = *pKey;
		pCur->value = *pValue;

		if (pKey->IsAllocType()) Var_AddRef(pKey);
		if (pValue->IsAllocType()) Var_AddRef(pValue);
		return;
	}

	int iSelect = -1;
/*	if (Bucket->_capa == 0)
	{
		Bucket->_capa = DefualtTableSize;
		//TableNode* table = (TableNode*)malloc(sizeof(TableNode) * DefualtTableSize);
//		TableNode* table = Bucket->_default;
//		for (int i = 0; i < DefualtTableSize; i++) { table[i].key.SetType(VAR_NONE); table[i].value.SetType(VAR_NONE); }
		Bucket->_table = Bucket->_default;
		Bucket->_size_use = 0;
	}
	else */if (Bucket->_size_use > 0)
	{
		iSelect = Bucket->Find(pKey);
	}

	if (iSelect == -1)
	{
		TableNode* table;
		if (Bucket->_size_use + 1 >= Bucket->_capa)
		{
			int iPreTableSize = Bucket->_capa;
			int iNewTableSize = iPreTableSize * 2;

			table = (TableNode*)malloc(sizeof(TableNode) * iNewTableSize);
			memcpy(table, Bucket->_table, sizeof(TableNode) * iPreTableSize);
			if (Bucket->_table && iPreTableSize > DefualtTableSize) free(Bucket->_table);

			for (int i = iPreTableSize; i < iNewTableSize; i++) { table[i].key.SetType(VAR_NONE); table[i].value.SetType(VAR_NONE); }

			Bucket->_table = table;
			Bucket->_capa = iNewTableSize;
		}
		iSelect = Bucket->_size_use++;
		//if (g_MaxList < Bucket->_size_use)
		//	g_MaxList = Bucket->_size_use;
		_itemCount++;

		TableNode* pCur = &Bucket->_table[iSelect];
		pCur->key = *pKey;
		pCur->value = *pValue;

		if (pKey->IsAllocType()) Var_AddRef(pKey);
		if (pValue->IsAllocType()) Var_AddRef(pValue);
	}
	else
	{
		TableNode* pCur = &Bucket->_table[iSelect];
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
	TableBucket4* Bucket = GetTableBucket(pKey);
	if (Bucket == NULL)
		return;

	int idx = Bucket->Find(pKey);
	if(idx < 0)
		return;

	TableNode* pCur = &Bucket->_table[idx];
	pVMW->Var_Release(&pCur->key);
	pVMW->Var_Release(&pCur->value);

	int move_cnt = Bucket->_size_use - idx - 1;
	if (move_cnt > 0)
	{
		memmove(&Bucket->_table[idx], &Bucket->_table[idx + 1], sizeof(TableNode) * move_cnt);
		int last = Bucket->_size_use - 1;
		Bucket->_table[last].key.SetType(VAR_NONE); Bucket->_table[last].value.SetType(VAR_NONE);
	}

	Bucket->_size_use--;
	_itemCount--;
}

TableBucket4* TableInfo::GetTableBucket(VarInfo *pKey)
{
	u32 hash = GetHashCode(pKey);
	u32 hash4 = hash % MAX_TABLE;
	u32 hash3 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash2 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	if (NO_HASH_FLAG(_Flag1, hash1))
		return NULL;

	TableBucket2* Bucket2 = _Bucket1[hash1]._Bucket2;
	if (NO_HASH_FLAG(_Bucket1[hash1]._Flag2, hash2))
		return NULL;

	TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
	if (NO_HASH_FLAG(Bucket2[hash2]._Flag3, hash3))
		return NULL;

	TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
	if (NO_HASH_FLAG(Bucket3[hash3]._Flag4, hash4))
		return NULL;

	return &Bucket4[hash4];
}


VarInfo* TableInfo::GetTableItem(VarInfo *pKey)
{
	TableBucket4* Bucket = GetTableBucket(pKey);
	if (Bucket == NULL)
		return NULL;

	int idx = Bucket->Find(pKey);
	if (idx < 0)
		return NULL;

	return &Bucket->_table[idx].value;
}
VarInfo* TableInfo::GetTableItem(std::string& key)
{
	if (_Bucket1 == NULL)
		return NULL;

	u32 hash = GetHashCode(key);
	u32 hash4 = hash % MAX_TABLE;
	u32 hash3 = (hash / MAX_TABLE) % MAX_TABLE;
	u32 hash2 = (hash / (MAX_TABLE * MAX_TABLE)) % MAX_TABLE;
	u32 hash1 = (hash / (MAX_TABLE * MAX_TABLE * MAX_TABLE)) % MAX_TABLE;

	if (NO_HASH_FLAG(_Flag1, hash1))
		return NULL;

	TableBucket2* Bucket2 = _Bucket1[hash1]._Bucket2;
	if (NO_HASH_FLAG(_Bucket1[hash1]._Flag2, hash2))
		return NULL;

	TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
	if (NO_HASH_FLAG(Bucket2[hash2]._Flag3, hash3))
		return NULL;

	TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
	if (NO_HASH_FLAG(Bucket3[hash3]._Flag4, hash4))
		return NULL;

	TableBucket4* Bucket = &Bucket4[hash4];
	TableNode* table = Bucket->_table;
	for (int i = Bucket->_size_use - 1; i >= 0; i--)
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

	for (int hash1 = 0; hash1 < MAX_TABLE; hash1++)
	{
		if ((hash1 % TFLAG_BITS) == 0 && _Flag1[hash1 / TFLAG_BITS] == 0)
		{
			hash1 += (TFLAG_BITS - 1);
			continue;
		}
#ifdef BIT_LOOP
		if (NO_HASH_FLAG(_Flag1, hash1))
			continue;
#endif

		TableBucket2* Bucket2 = _Bucket1[hash1]._Bucket2;
		if (Bucket2 == NULL)
			continue;
		uTFlag* Flag2 = _Bucket1[hash1]._Flag2;
		for (int hash2 = 0; hash2 < MAX_TABLE; hash2++)
		{
			if ((hash2 % TFLAG_BITS) == 0 && Flag2[hash2 / TFLAG_BITS] == 0)
			{
				hash2 += (TFLAG_BITS - 1);
				continue;
			}
#ifdef BIT_LOOP
			if (NO_HASH_FLAG(Flag2, hash2))
				continue;
#endif

			TableBucket3* Bucket3 = Bucket2[hash2]._Bucket3;
			if (Bucket3 == NULL)
				continue;
			uTFlag* Flag3 = Bucket2[hash2]._Flag3;
			for (int hash3 = 0; hash3 < MAX_TABLE; hash3++)
			{
				if ((hash3 % TFLAG_BITS) == 0 && Flag3[hash3 / TFLAG_BITS] == 0)
				{
					hash3 += (TFLAG_BITS - 1);
					continue;
				}
#ifdef BIT_LOOP
				if (NO_HASH_FLAG(Flag3, hash3))
					continue;
#endif

				TableBucket4* Bucket4 = Bucket3[hash3]._Bucket4;
				if (Bucket4 == NULL)
					continue;
				uTFlag* Flag4 = Bucket3[hash3]._Flag4;
				for (int hash4 = 0; hash4 < MAX_TABLE; hash4++)
				{
					if ((hash4 % TFLAG_BITS) == 0 && Flag4[hash4 / TFLAG_BITS] == 0)
					{
						hash4 += (TFLAG_BITS - 1);
						continue;
					}
#ifdef BIT_LOOP
					if (NO_HASH_FLAG(Flag4, hash4))
						continue;
#endif

					TableBucket4* Bucket = &Bucket4[hash4];
					TableNode* table = Bucket->_table;
					for (int i = Bucket->_size_use - 1; i >= 0; i--)
					{
						lst[cnt++] = &table[i].value;
					}
				}
			}
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

