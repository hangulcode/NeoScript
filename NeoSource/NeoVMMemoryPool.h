#pragma once

namespace NeoScript
{


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

template <typename T>
class SimpleVector
{
	T* _buffer = nullptr;
	int _cnt = 0;
	int _capa = 0;
public:
	SimpleVector()
	{
		Alloc(2);
	}
	~SimpleVector()
	{
		if(_buffer)
			delete [] _buffer;
		_buffer = nullptr;
		_capa = 0;
		_cnt = 0;
	}
	void Alloc(int size)
	{
		if(size <= _capa) return;

		T* p = new T[size];
		if (_cnt > 0)
			memcpy(p, _buffer, _capa * sizeof(T));
		if(_buffer)
			delete[] _buffer;

		_buffer = p;
		_capa = size;
	}
	NEOS_FORCEINLINE int calc_capa(int cnt)
	{
		int new_capa = _capa;
		while (cnt > new_capa)
		{
			if (new_capa >= 128)
				new_capa = new_capa + 128;
			else
				new_capa = new_capa << 1; // * 2
		}
		return new_capa;
	}
	NEOS_FORCEINLINE void push_back(T& t)
	{
		if(_cnt >= _capa)
		{
			if(_capa >= 128)
				Alloc(_capa + 128);
			else
				Alloc(_capa << 1); // * 2
		}
		_buffer[_cnt++] = t;
	}
	NEOS_FORCEINLINE T& push_back()
	{
		if (_cnt >= _capa)
		{
			if (_capa >= 128)
				Alloc(_capa + 128);
			else
				Alloc(_capa << 1); // * 2
		}
		return _buffer[_cnt++];
	}
	NEOS_FORCEINLINE int size() { return _cnt; }
	NEOS_FORCEINLINE void resize(int cnt)
	{ 
		if(cnt < 0) cnt = 0;
		if(cnt > _capa)
			Alloc(calc_capa(cnt));
		_cnt = cnt;
	}
	NEOS_FORCEINLINE T& operator[](const int idx)
	{
		return _buffer[idx];
	}
	void reserve(int cnt)
	{
		Alloc(calc_capa(cnt));
	}
	NEOS_FORCEINLINE void clear()
	{
		_cnt = 0;
	}
};

};