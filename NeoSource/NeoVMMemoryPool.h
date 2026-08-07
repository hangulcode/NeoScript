#pragma once

namespace NeoScript
{


#pragma pack(1)
// 성능 최적화를 위해서 생성자/소멸자 지원하지 않음. 
// 생성자 소멸자 처리가 필요한 풀은 CNVMInstPool 을 사용하면 됨.
template <typename T, int iBlkSize = 128>
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

	// 이 풀이 malloc 으로 잡아둔 페이지의 총 바이트. 사용중/여유 블록을 구분하지 않는
	// "확보 용량"이다 — 풀은 반납해도 페이지를 OS 에 돌려주지 않기 때문에 이 값이 곧 실제 점유량이다.
	size_t m_nReservedBytes = 0;

	void	clear()
	{
		for (auto it = m_sMemPagePool.begin(); it != m_sMemPagePool.end(); it++)
		{
			STPool& p = (*it);
			free(p.pData);
		}

		m_sMemPagePool.clear();
		m_sFreeNode.clear();
		m_nReservedBytes = 0;
	}
	void	alloc()
	{
		STPool pool;
		pool.pData = (SNodePool*)malloc(sizeof(SNodePool) * m_iBlkSize);
		SNodePool* pData = pool.pData;

		m_sMemPagePool.push_back(pool);
		m_nReservedBytes += sizeof(SNodePool) * (size_t)m_iBlkSize;

		for (int i = m_iBlkSize - 2; i >= 0; i--)
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
		// 페이지 크기는 고정이다(예전에는 2배씩 키웠다).
		// 2배 증가는 마지막 한 장이 과도하게 커져 필요량을 크게 넘겨 잡는다 —
		// 32 부터 배증하면 4만 개를 담으려고 65,504 개를 확보한다(64% 초과).
		// 고정 크기는 초과분이 최대 (페이지 크기 - 1) 이고, 나중에 빈 페이지를
		// 돌려주는 작업에서도 회수 단위가 잘게 쪼개져 유리하다.
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
	inline T*	Receive()
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

	inline void    Confer(T* buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - offsetof(SNodePool, m_sObj.data));

#ifdef _DEBUG
		__p->m_sObj.dwpFlag = 0;
#endif

		m_sFreeNode.push_head(__p);
	}

	inline size_t ReservedBytes() const { return m_nReservedBytes; }
};


template <typename T, int iBlkSize = 128>
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

	// CNVMAllocPool 과 동일한 의미의 "확보 용량". 노드 안 T 의 멤버가 따로 힙을 잡는 경우
	// (예: CoroutineInfo 의 var 스택 vector) 는 여기 포함되지 않는다 — 소유자가 따로 센다.
	size_t m_nReservedBytes = 0;

	void	clear()
	{
		for(auto it = m_sMemPagePool.begin(); it != m_sMemPagePool.end(); it++)
		{
			STPool& p = (*it);
			delete [] p.pData;
		}

		m_sMemPagePool.clear();
		m_sFreeNode.clear();
		m_nReservedBytes = 0;
	}
	void	alloc()
	{
		STPool pool;
		pool.pData = new SNodePool[m_iBlkSize];

		m_sMemPagePool.push_back(pool);
		m_nReservedBytes += sizeof(SNodePool) * (size_t)m_iBlkSize;

		for (int i = 0; i < m_iBlkSize; i++)
		{
			SNodePool* pNode = &pool.pData[i];
			pNode->m_sObj.dwpFlag = 0;

			m_sFreeNode.push_head(pNode);
		}

		// 페이지 크기는 고정이다(예전에는 2배씩 키웠다).
		// 2배 증가는 마지막 한 장이 과도하게 커져 필요량을 크게 넘겨 잡는다 —
		// 32 부터 배증하면 4만 개를 담으려고 65,504 개를 확보한다(64% 초과).
		// 고정 크기는 초과분이 최대 (페이지 크기 - 1) 이고, 나중에 빈 페이지를
		// 돌려주는 작업에서도 회수 단위가 잘게 쪼개져 유리하다.
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
	inline T*	Receive()
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

	inline void    Confer(T* buf)
	{
		//SNodePool* __p = (SNodePool*)((u8*)buf - offsetof(SNodePool, m_sObj.data));
		SNodePool* __p = (SNodePool*)((u8*)buf - ((size_t) & (((SNodePool*)0)->m_sObj.data)));
		__p->m_sObj.dwpFlag = 0;

		m_sFreeNode.push_head(__p);
	}

	inline size_t ReservedBytes() const { return m_nReservedBytes; }

	// 반납되어 free 리스트에 올라와 있는 노드를 훑는다.
	// StringInfo 처럼 노드가 std::string 같은 부속 힙을 들고 있는 타입에서,
	// "지금 놀고 있는 노드가 얼마나 붙들고 있는가" 를 재거나 놓아주는 데 쓴다.
	template <typename F>
	void ForEachFree(F fn)
	{
		for (SNodePool* n = m_sFreeNode.get_head(); n != NULL; n = n->m_pNext)
			fn(n->m_sObj.data);
	}
};

#pragma pack()


};