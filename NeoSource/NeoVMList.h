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
struct ListInfo : AllocBase
{
	VarInfo*	_Bucket;

	CNeoVMImpl*	_pVM;


	int	_ListID;
	int _itemCount;
	void* _pUserData;

	int _BucketCapa;

	VMHash<int>* _pIndexer;

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