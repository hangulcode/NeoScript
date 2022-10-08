#pragma once

struct TableData;


#define MAX_TABLE	100

struct TableNode
{
	TableData	_data;
	TableNode*	_next;
};


struct TableBocket3
{
	int			_size;
	TableNode*	_table; // alloc pointer [array]

	TableNode*	_used;	// used begin pointer [linked list]
	TableNode*	_free;	// unused begin pointer [linked list]

#ifdef _DEBUG
	int			_size_use;
	int			_size_free;
#endif

	void Push_Use(TableNode* p)
	{
		TableNode* n = _used;
		_used = p;
		_used->_next = n;
#ifdef _DEBUG
		_size_use++;
#endif
	}

	void Push_Free(TableNode* p)
	{
		TableNode* n = _free;
		_free = p;
		_free->_next = n;
#ifdef _DEBUG
		_size_free++;
#endif
	}
	TableNode* Find(VarInfo* pKey);
	bool	Pop_Used(TableNode* pTar);
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

	void Free(CNeoVM* pVM);

	void Insert(CNeoVM* pVM, std::string& pKey, VarInfo* pValue);
	void Insert(CNeoVMWorker* pVMW, VarInfo* pKey, VarInfo* pValue);
	void Remove(CNeoVMWorker* pVMW, VarInfo* pKey);
	TableBocket3* GetTableBocket(VarInfo *pKey);
	VarInfo* GetTableItem(VarInfo *pKey);
	VarInfo* GetTableItem(std::string& key);

	TableIterator FirstNode();
	TableIterator NextNode(TableIterator);
private:
	void Var_Release(CNeoVM* pVM, VarInfo *d);
};

