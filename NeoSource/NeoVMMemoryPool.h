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
	int		m_lnSize;
public:
	_CSelfList()
	{
		m_pHead = NULL;
		m_lnSize = 0;
	}
	inline void clear()
	{
		m_pHead = NULL;
		m_lnSize = 0;
	}
	inline __node* get_head()
	{
		return m_pHead;
	}
	inline __node* pop_head()
	{
		if (m_pHead == NULL)
			return NULL;
		__node* __p = m_pHead;
		m_pHead = m_pHead->m_pNext;
		m_lnSize--;
		return __p;
	}
	inline void push_head(__node* __p)
	{
		m_lnSize++;
		__p->m_pNext = m_pHead;
		m_pHead = __p;
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
	struct STNode
	{
		u32 dwpFlag;
		u8 data[iRecSize];
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
		for (auto it = m_sMemPagePool.begin(); it != m_sMemPagePool.end(); it++)
		{
			STPool& p = (*it);
			free(p.pData);
		}

		m_sMemPagePool.clear();
		m_sFreeNode.clear();
	}
	void	alloc()
	{
		STPool pool;
		pool.pData = (SNodePool*)malloc(sizeof(SNodePool) * m_iBlkSize);

		m_sMemPagePool.push_back(pool);

		for (int i = 0; i < m_iBlkSize; i++)
		{
			SNodePool* pNode = &pool.pData[i];
			pNode->m_sObj.dwpFlag = 0;

			m_sFreeNode.push_head(pNode);
		}

		m_iBlkSize *= 2;
	}

public:
	int m_iBlkSize = 1;

public:
	CAllocPool()
	{
		m_iBlkSize = iBlkSize;
	}
	virtual ~CAllocPool()
	{
		clear();
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

		return &__p->m_sObj.data;
	}

	void    Confer(void* buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - offsetof(SNodePool, m_sObj.data));
		__p->m_sObj.dwpFlag = 0;

		m_sFreeNode.push_head(__p);
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
		pool.pData = new SNodePool[m_iBlkSize];

		m_sMemPagePool.push_back(pool);

		for (int i = 0; i < m_iBlkSize; i++)
		{
			SNodePool* pNode = &pool.pData[i];
			pNode->m_sObj.dwpFlag = 0;

			m_sFreeNode.push_head(pNode);
		}

		m_iBlkSize *= 2;
	}

public:
	int m_iBlkSize = 1;

public:
	CInstPool()
	{
		m_iBlkSize = iBlkSize;
	}
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

		m_sFreeNode.push_head(__p);
	}
};
