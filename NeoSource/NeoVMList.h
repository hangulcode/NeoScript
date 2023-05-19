#pragma once

#pragma pack(1)
#pragma pack()



class CNeoVMImpl;
class CNeoVMWorker;
struct ListInfo
{
	VarInfo*	_Bucket;

	CNeoVMImpl*	_pVM;


	int	_ListID;
	int _refCount;
	int _itemCount;
	void* _pUserData;

	int _BucketCapa;

	void Free();
	void Resize(int size);
	void Reserve(int capa);


//	CollectionIterator FirstNode();
//	bool NextNode(CollectionIterator&);

	inline int		GetCount() { return _itemCount; }
	bool GetValue(int idx, VarInfo* pValue);
	bool SetValue(int idx, VarInfo* pValue);
	bool SetValue(int idx, int v);

	bool Insert(int idx, VarInfo* pValue);
	bool InsertLast(VarInfo* pValue);
	bool InsertLast(const std::string& str);

	inline VarInfo* GetDataUnsafe() { return _Bucket; }
private:
};

