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


struct TableBocket4
{
	TableNode*	_table; // alloc pointer [array]
	TableNode	_default[DefualtTableSize];


	int			_size_use;
	int			_capa;

	int Find(VarInfo* pKey);
};

struct TableBocket3
{
	TableBocket4* _bocket4;
};

struct TableBocket2
{
	TableBocket3* _bocket3;
};


struct TableBocket1
{
	TableBocket2* _bocket2;
};

class CNeoVM;
class CNeoVMWorker;
struct TableInfo
{
	TableBocket1*	_bocket1;

	int	_TableID;
	int _refCount;
	int _itemCount;
	void* _pUserData;

	FunctionPtrNative _fun;
	TableInfo*		_meta;

	//CAllocPool m_sPool3;

	void Free(CNeoVM* pVM);

	void Insert(CNeoVM* pVM, std::string& pKey, VarInfo* pValue);
	void Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue);
	void Remove(CNeoVMWorker* pVMW, VarInfo* pKey);
	TableBocket4* GetTableBocket(VarInfo *pKey);
	VarInfo* GetTableItem(VarInfo *pKey);
	VarInfo* GetTableItem(std::string& key);

	TableIterator FirstNode();
	TableIterator NextNode(TableIterator);

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

