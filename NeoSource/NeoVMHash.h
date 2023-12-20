#pragma once

namespace NeoScript
{
extern u32 GetHashCode(const std::string& str);

struct VMString
{
	std::string _str;
	int	_StringID;
	int _StringLen;

	mutable u32	_hash;
	mutable void* _container;
	mutable void* _value;

	NEOS_FORCEINLINE u32 GetHash() const
	{
		if (_hash != 0) return _hash;
		_hash = GetHashCode(_str);
		return _hash;
	}
};


template <typename T>
class SimpleVector
{
	T* _buffer = nullptr;
	int _cnt = 0;
	int _capa = 0;
public:
	SimpleVector()
	{
//		Alloc(2);
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
		for(int i = 0; i < _capa; i++)
			p[i] = _buffer[i];
		if(_buffer)
			delete[] _buffer;

		_buffer = p;
		_capa = size;
	}
	NEOS_FORCEINLINE int calc_capa(int cnt)
	{
		int new_capa = _capa;
		if(new_capa <= 0) new_capa = 1;

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
				Alloc(_capa > 0 ? (_capa << 1) : 1); // * 2
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
	void RemoveIndex(int idx)
	{
		if(idx < 0 || idx >= _cnt) return;
		for(int i = idx + 1; i < _cnt; i++)
		{
			_buffer[i - 1] = _buffer[i];
		}
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

template <typename T, int iBlkSize = 256>
class VMHash
{
	struct HNode
	{
		std::string	key;
		T			value;

		u32		hash;
	};

	SimpleVector<HNode>* _bucket = nullptr;
	int _cnt = 0;
	int _capa = 0;
public:
	VMHash()
	{
		_capa = iBlkSize;
		_bucket = new SimpleVector<HNode>[_capa];
	}
	~VMHash()
	{
		if (_bucket)
			delete[] _bucket;
		_bucket = nullptr;
		_capa = 0;
		_cnt = 0;
	}

	NEOS_FORCEINLINE void clear()
	{
		_cnt = 0;
		for(int i = 0; i < _capa; i++)
			_bucket[i].clear();
	}
	NEOS_FORCEINLINE bool empty() { return _cnt == 0; }

	void Add(const std::string& key, T d)
	{
		Remove(key);

		u32 hkey = GetHashCode(key);
		u32 bkIndex = hkey % _capa;
		HNode n;
		n.hash = hkey;
		n.key = key;
		n.value = d;
		_bucket[bkIndex].push_back(n);
		++_cnt;
	}
	bool TryGetValue(const std::string& key, T* d)
	{
		u32 hkey = GetHashCode(key);
		u32 bkIndex = hkey % _capa;
		SimpleVector<HNode>& bk = _bucket[bkIndex];
		for (int idx = bk.size() - 1; idx >= 0; --idx)
		{
			HNode& n = bk[idx];
			if (n.hash == hkey)
			{
				if (n.key == key)
				{
					*d = n.value;
					return true;
				}
			}
		}
		return false;
	}
	bool TryGetValueForce(const VMString* pStr, T* d)
	{
		u32 hkey = pStr->GetHash();
		u32 bkIndex = hkey % _capa;
		SimpleVector<HNode>& bk = _bucket[bkIndex];
		for (int idx = bk.size() - 1; idx >= 0; --idx)
		{
			HNode& n = bk[idx];
			if (n.hash == hkey)
			{
				if (pStr->_str == n.key)
				{
					pStr->_container = this;
					//pStr->_value = reinterpret_cast<void*>((uintptr_t)n.value);
					pStr->_value = *((void**)&n.value);

					*d = n.value;
					return true;
				}
			}
		}
		return false;
	}

	NEOS_FORCEINLINE bool TryGetValue(const VMString* pStr, T* d)
	{
		if(pStr->_container == this)
		{
			//*d = (T)reinterpret_cast<uintptr_t>(pStr->_value);
			*d = *((T*)&pStr->_value);
			return true;
		}
		return TryGetValueForce(pStr, d);
	}
	bool IsKey(const std::string& key)
	{
		T d;
		return TryGetValue(key, &d);
	}
	bool Remove(const std::string& key)
	{
		u32 hkey = GetHashCode(key);
		u32 bkIndex = hkey % _capa;
		SimpleVector<HNode>& bk = _bucket[bkIndex];
		for(int idx = bk.size() - 1; idx >= 0; --idx)
		{
			HNode& n = bk[idx];
			if(n.hash == hkey)
			{
				if(n.key == key)
				{
					bk.RemoveIndex(idx);
					--_cnt;
					return true;
				}
			}
		}
		return false;
	}
};


};