#pragma once


struct TableNode
{
	VarInfo	key;
	VarInfo	value;
};

struct TableSortInfo
{
	CNeoVMWorker*	_pN;
	int				_compareFunction;
};


const int DefualtTableSize = 4;
#define MAX_TABLE	64

typedef 	u16	uTFlag;
#define TFLAG_BITS	(sizeof(uTFlag) * 8)
#define TFLAG_CNT	(MAX_TABLE / TFLAG_BITS)


struct TableBucket4
{
	TableNode*	_table; // alloc pointer [array]
	TableNode	_default[DefualtTableSize];


	int			_capa;
	int			_size_use;

	int Find(VarInfo* pKey);
};

struct TableBucket3
{
	TableBucket4* _Bucket4;
	uTFlag _Flag4[TFLAG_CNT];
};

struct TableBucket2
{
	TableBucket3* _Bucket3;
	uTFlag _Flag3[TFLAG_CNT];
};


struct TableBucket1
{
	TableBucket2* _Bucket2;
	uTFlag _Flag2[TFLAG_CNT];
};

class CNeoVM;
class CNeoVMWorker;
struct TableInfo
{
	TableBucket1	_Bucket1[MAX_TABLE];
	uTFlag _Flag1[TFLAG_CNT];

	int	_TableID;
	int _refCount;
	int _itemCount;
	void* _pUserData;

	FunctionPtrNative _fun;
	TableInfo*		_meta;

	void Free(CNeoVM* pVM);

	void Insert(CNeoVM* pVM, std::string& pKey, VarInfo* pValue);
	void Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue);
	void Remove(CNeoVMWorker* pVMW, VarInfo* pKey);
	TableBucket4* GetTableBucket(VarInfo *pKey);
	VarInfo* GetTableItem(VarInfo *pKey);
	VarInfo* GetTableItem(std::string& key);

	TableIterator FirstNode();
	bool NextNode(TableIterator&);

	bool ToList(std::vector<VarInfo*>& lst);

private:
	inline void Var_AddRef(VarInfo *d)
	{
		switch (d->GetType())
		{
		case VAR_STRING:
			++d->_str->_refCount;
			break;
		case VAR_TABLE:
			++d->_tbl->_refCount;
			break;
		default:
			break;
		}
	}
	void Var_ReleaseInternal(CNeoVM* pVM, VarInfo *d);
	inline void Var_Release(CNeoVM* pVM, VarInfo *d)
	{
		if (d->IsAllocType())
			Var_ReleaseInternal(pVM, d);
		else
			d->ClearType();
	}

};

