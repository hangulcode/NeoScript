#pragma once


#pragma pack(1)


template <typename T, int iBlkSize = 100>
class CNVMAllocPool
{
	template<class T1>
	struct _SelfNode
	{
		union
		{
			_SelfNode*	m_pNext;
			T1			m_sObj;
		};
	};


	template<class T1>
	class _CSelfList
	{
	public:
		typedef _SelfNode<T1> __node;
	private:
		__node*	m_pHead;
#ifdef _DEBUG
		int		m_lnSize;
#endif
	public:
		_CSelfList()
		{
			m_pHead = NULL;
#ifdef _DEBUG
			m_lnSize = 0;
#endif
		}
		inline void clear()
		{
			m_pHead = NULL;
#ifdef _DEBUG
			m_lnSize = 0;
#endif
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
#ifdef _DEBUG
			m_lnSize--;
#endif
			return __p;
		}
		inline void push_head(__node* __p)
		{
#ifdef _DEBUG
			m_lnSize++;
#endif
			__p->m_pNext = m_pHead;
			m_pHead = __p;
		}
		inline bool empty()
		{
			return m_pHead ? false : true;
		}
		inline void set_head(__node* __p)
		{
			m_pHead = __p;
		}
#ifdef _DEBUG
		inline int size() { return m_lnSize; }
#endif
	};
private:
	struct STNode
	{
#ifdef _DEBUG
		u32 dwpFlag;
#endif
		u8 data[sizeof(T)];
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
		SNodePool* pData = pool.pData;

		m_sMemPagePool.push_back(pool);

		for (int i = 0; i < m_iBlkSize; i++)
		{
#ifdef _DEBUG
			SNodePool* pNode = &pData[i];
			pNode->m_sObj.dwpFlag = 0;
			m_sFreeNode.push_head(pNode);
#else
			pData[i].m_pNext = &pData[i + 1];
#endif
		}

#ifdef _DEBUG
#else
		m_sFreeNode.set_head(&pData[0]);
		pData[m_iBlkSize - 1].m_pNext = NULL;
#endif
		m_iBlkSize *= 2;
	}

public:
	int m_iBlkSize = 1;
	u32 _dwLastID = 0;
public:
	CNVMAllocPool()
	{
		m_iBlkSize = iBlkSize;
	}
	virtual ~CNVMAllocPool()
	{
		clear();
	}
	T*	Receive()
	{
		if (m_sFreeNode.empty())
			alloc();

		SNodePool* __p = m_sFreeNode.pop_head();
#ifdef _DEBUG
		__p->m_sObj.dwpFlag = 1;
		__p->m_pNext = NULL;
#endif

		return (T*)&__p->m_sObj.data;
	}

	void    Confer(T* buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - offsetof(SNodePool, m_sObj.data));

#ifdef _DEBUG
		__p->m_sObj.dwpFlag = 0;
#endif

		m_sFreeNode.push_head(__p);
	}
};


template <typename T, int iBlkSize = 100>
class CNVMInstPool
{
	template<class T1>
	struct _SelfNode
	{
		_SelfNode*	m_pNext;
		T1			m_sObj;
	};


	template<class T1>
	class _CSelfList
	{
	public:
		typedef _SelfNode<T1> __node;
	private:
		__node*	m_pHead;
#ifdef _DEBUG
		int		m_lnSize;
#endif
	public:
		_CSelfList()
		{
			m_pHead = NULL;
#ifdef _DEBUG
			m_lnSize = 0;
#endif
		}
		inline void clear()
		{
			m_pHead = NULL;
#ifdef _DEBUG
			m_lnSize = 0;
#endif
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
#ifdef _DEBUG
			m_lnSize--;
#endif
			return __p;
		}
		inline void push_head(__node* __p)
		{
#ifdef _DEBUG
			m_lnSize++;
#endif
			__p->m_pNext = m_pHead;
			m_pHead = __p;
		}
		inline bool empty()
		{
			return m_pHead ? false : true;
		}
		inline void set_head(__node* __p)
		{
			m_pHead = __p;
		}
#ifdef _DEBUG
		inline int size() { return m_lnSize; }
#endif
	};
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
	u32 _dwLastID = 0;
public:
	CNVMInstPool()
	{
		m_iBlkSize = iBlkSize;
	}
	virtual ~CNVMInstPool()
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
		//SNodePool* __p = (SNodePool*)((u8*)buf - offsetof(SNodePool, m_sObj.data));
		SNodePool* __p = (SNodePool*)((u8*)buf - ((size_t) & (((SNodePool*)0)->m_sObj.data)));
		__p->m_sObj.dwpFlag = 0;

		m_sFreeNode.push_head(__p);
	}
};

#pragma pack()