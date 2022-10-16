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
	TableNode	_default[DefualtTableSize];


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

/*
template<class T>
struct _SelfNode
{
	_SelfNode*	m_pNext;
	T			m_sObj;
};


template<class T>
class _CSelfList
{
public:
	typedef _SelfNode<T> __node;
private:
	__node*	m_pHead;
	__node*	m_pTail;
	int		m_lnSize;
public:
	_CSelfList()
	{
		m_pHead = m_pTail = NULL;
		m_lnSize = 0;
	}
	inline void clear()
	{
		m_pHead = m_pTail = NULL;
		m_lnSize = 0;
	}
	inline __node* get_head()
	{
		return m_pHead;
	}
	inline __node* get_tail()
	{
		return m_pTail;
	}
	inline __node* pop_head()
	{
		if (m_pHead == NULL)
			return NULL;
		__node* __p = m_pHead;
		m_pHead = m_pHead->m_pNext;
		if (m_pHead == NULL)
			m_pTail = NULL;
		m_lnSize--;
		//TRUE_DEBUGONLY_BUMB_CODE(m_lnSize >= 0);
		return __p;
	}
	inline void push_head(__node* __p)
	{
		//TRUE_DEBUGONLY_BUMB_CODE(m_lnSize < 0x7FFFFFFF);
		m_lnSize++;
		__p->m_pNext = m_pHead;
		if (m_pTail == NULL)
		{
			m_pTail = __p;
		}
		m_pHead = __p;
	}
	inline void push_tail(__node* __p)
	{
		//TRUE_DEBUGONLY_BUMB_CODE(m_lnSize < 0x7FFFFFFF);
		m_lnSize++;
		__p->m_pNext = NULL;
		if (m_pHead == NULL)
		{
			m_pHead = m_pTail = __p;
			return;
		}
		m_pTail->m_pNext = __p;
		m_pTail = __p;
	}
	inline bool empty()
	{
		return m_pHead ? false : true;
	}
	inline int size() { return m_lnSize; }
};

class CAllocPool
{
private:

protected:
	struct STPool
	{
		int			m_iAllocBlkSize;
	};
	typedef _SelfNode<STPool>	SMemPool;

	struct STNode
	{
		u32 dwpFlag;
	};
	typedef _SelfNode<STNode>	SNodePool;
	_CSelfList<STPool> m_sMemPagePool;
	_CSelfList<STNode> m_sFreeNode;

	u32 m_dwRecSize = 1;
	int m_iBlkSize = 1;

	//friend CMemPoolManager;
public:
	CAllocPool() {}
	void Init(u32 dwRecSize, int iBlkSize)
	{
		m_dwRecSize = dwRecSize + sizeof(SNodePool);
		m_iBlkSize = iBlkSize;
	}
	virtual ~CAllocPool()
	{
		clear();
	}
	void	ForceAllFree()
	{
		clear(false);
	}
protected:
	void	clear(bool blCheckLeak = true)
	{
		m_sMemPagePool.clear();
		m_sFreeNode.clear();
	}
	void	alloc()
	{
		SMemPool* pNewPool = (SMemPool*)malloc(sizeof(SMemPool) + m_dwRecSize* m_iBlkSize);
		pNewPool->m_sObj.m_iAllocBlkSize = m_iBlkSize;


		m_sMemPagePool.push_tail(pNewPool);

		SNodePool* pNode = (SNodePool*)((u8*)pNewPool + sizeof(SMemPool));
		for (int i = 0; i < m_iBlkSize; i++)
		{
			pNode->m_sObj.dwpFlag = 0;

			m_sFreeNode.push_head(pNode);
			pNode = (SNodePool*)((u8*)pNode + m_dwRecSize);
		}
	}


	void*	Receive()
	{
		SNodePool* __p = m_sFreeNode.pop_head();
		if (__p == NULL)
		{
			alloc();
			__p = m_sFreeNode.pop_head();
		}

		__p->m_sObj.dwpFlag = 1;
		__p->m_pNext = NULL;


		u8 *pObj = (u8*)(__p + 1);


		return (u8*)pObj;
	}

	void    Confer(void *buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - sizeof(SNodePool));
		__p->m_sObj.dwpFlag = 0;

		m_sFreeNode.push_tail(__p);

		//if (--m_sPI.m_dwUsedNode == 0)
		{
			//clear(false);
		}
	}
};
*/
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
	TableBocket3* GetTableBocket(VarInfo *pKey);
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

