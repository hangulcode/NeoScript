#pragma once



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

template <int iRecSize, int iBlkSize = 100>
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

	int m_iRecSize = 1;
	int m_iBlkSize = 1;

	//friend CMemPoolManager;
public:
	CAllocPool() 
	{
		m_iRecSize = iRecSize + sizeof(SNodePool);
		m_iBlkSize = iBlkSize;
	}
	virtual ~CAllocPool()
	{
		clear();
	}
	void	ForceAllFree()
	{
		clear();
	}
protected:
	void	clear()
	{
		auto pCur = m_sMemPagePool.get_head();
		while (pCur)
		{
			auto p = pCur;
			pCur = pCur->m_pNext;
			free(p);
		}

		m_sMemPagePool.clear();
		m_sFreeNode.clear();
	}
	void	alloc()
	{
		SMemPool* pNewPool = (SMemPool*)malloc(sizeof(SMemPool) + m_iRecSize * m_iBlkSize);
		pNewPool->m_sObj.m_iAllocBlkSize = m_iBlkSize;


		m_sMemPagePool.push_tail(pNewPool);

		SNodePool* pNode = (SNodePool*)((u8*)pNewPool + sizeof(SMemPool));
		for (int i = 0; i < m_iBlkSize; i++)
		{
			pNode->m_sObj.dwpFlag = 0;

			m_sFreeNode.push_head(pNode);
			pNode = (SNodePool*)((u8*)pNode + m_iRecSize);
		}
	}

public:
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

template <typename T, int iBlkSize = 100>
class CInstPool
{
private:
	struct STNode
	{
		u32 dwpFlag;
		T data;
	};



	typedef _SelfNode<STNode>	SNodePool;

	struct STPool
	{
		SNodePool* pData;
	};

	std::list<STPool> m_sMemPagePool;

	_CSelfList<STNode> m_sFreeNode;

	void	clear()
	{
		for(auto it = m_sMemPagePool.begin(); it != m_sMemPagePool.end(); it++)
		{
			STPool& p = (*it);
			delete [] p.pData;
		}

		m_sMemPagePool.clear();
		m_sFreeNode.clear();
	}
	void	alloc()
	{
		STPool pool;
		pool.pData = new SNodePool[iBlkSize];

		m_sMemPagePool.push_back(pool);

		for (int i = 0; i < iBlkSize; i++)
		{
			SNodePool* pNode = &pool.pData[i];
			pNode->m_sObj.dwpFlag = 0;

			m_sFreeNode.push_head(pNode);
		}
	}

public:
	virtual ~CInstPool()
	{
		clear();
	}
	T*	Receive()
	{
		SNodePool* __p = m_sFreeNode.pop_head();
		if (__p == NULL)
		{
			alloc();
			__p = m_sFreeNode.pop_head();
		}

		__p->m_sObj.dwpFlag = 1;
		__p->m_pNext = NULL;

		return &__p->m_sObj.data;
	}

	void    Confer(T* buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - offsetof(SNodePool, m_sObj.data));
		__p->m_sObj.dwpFlag = 0;

		m_sFreeNode.push_tail(__p);
	}
};
