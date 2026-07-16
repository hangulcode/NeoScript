#pragma once

namespace NeoScript
{
struct MatrixInfo
{
	int	row; // V
	int	col; // U
};

class CNeoVMImpl;
class CNeoVMWorker;

// 작은 리스트 인라인 버킷(SBO): 원소 수가 이 값 이하면 힙 할당 없이 struct 내부에 담는다.
// 게임 벡터 리터럴 [x,y,z](3), [u,v](2), 쿼터니언 [qw,qx,qy,qz](4) 를 모두 커버.
#define NEOS_LIST_INLINE_CAP 4

struct ListInfo : AllocBase
{
	VarInfo*	_Bucket;

	CNeoVMImpl*	_pVM;


	int	_ListID;
	int _itemCount;
	u32 _mutationVersion = 0;
	void* _pUserData;

	int _BucketCapa;

	VMHash<int>* _pIndexer;

	// 살아있는 객체 추적용 intrusive 이중연결 리스트 (std::map 레지스트리 대체)
	ListInfo* _liveNext;
	ListInfo* _livePrev;

	// SBO 인라인 버킷. 작은 리스트는 _Bucket 이 여기를 가리킨다(_Bucket == _inlineBucket).
	VarInfo _inlineBucket[NEOS_LIST_INLINE_CAP];

	NEOS_FORCEINLINE bool IsInlineBucket() { return _Bucket == _inlineBucket; }
	NEOS_FORCEINLINE void InitInlineBucket()
	{
		_Bucket = _inlineBucket;
		_BucketCapa = NEOS_LIST_INLINE_CAP;
		_itemCount = 0;
		for (int i = 0; i < NEOS_LIST_INLINE_CAP; i++)
			_inlineBucket[i].ClearType();
	}

	void Free();
	void Resize(int size);
	void Resize(int row, int col);
	void Reserve(int capa);


//	CollectionIterator FirstNode();
//	bool NextNode(CollectionIterator&);

	inline int		GetCount() { return _itemCount; }
	bool GetValue(int idx, VarInfo* pValue);
	VarInfo* GetValue(int idx);
	bool SetValue(int idx, VarInfo* pValue);
	bool SetValue(int idx, int v);
	bool SetValue(int idx, NS_FLOAT v);

	bool Insert(int idx, VarInfo* pValue);
	bool InsertLast(VarInfo* pValue);
	bool InsertLast(const std::string& str);

	MatrixInfo GetMatrix();

	inline VarInfo* GetDataUnsafe() { return _Bucket; }
private:
};

};
