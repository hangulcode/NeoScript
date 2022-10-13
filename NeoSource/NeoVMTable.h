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


struct TableBocket3
{
	TableNode*	_table; // alloc pointer [array]


	int			_size_use;

	int Find(VarInfo* pKey);
};

struct TableBocket2
{
	int*			_capa;
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

	void Free(CNeoVM* pVM);

	void Insert(CNeoVM* pVM, std::string& pKey, VarInfo* pValue);
	void Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue);
	void Remove(CNeoVMWorker* pVMW, VarInfo* pKey);
	TableBocket3* GetTableBocket(VarInfo *pKey);
	VarInfo* GetTableItem(VarInfo *pKey);
	VarInfo* GetTableItem(std::string& key);

	TableIterator FirstNode();
	TableIterator NextNode(TableIterator);

	bool ToList(std::vector<VarInfo*>& lst);

private:
	void Var_Release(CNeoVM* pVM, VarInfo *d);
};

