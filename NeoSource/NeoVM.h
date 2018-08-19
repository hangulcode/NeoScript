#pragma once

#include "NeoVMWorker.h"

class CNeoVM
{
	friend					CNeoVMWorker;
private:
	u8 *					_pCodePtr;
	int						_iCodeLen;

	void	SetCodeData(u8* p, int sz)
	{
		_pCodePtr = p;
		_iCodeLen = sz;
	}

	int						_iSP_Vars;
	int						_iSP_Vars_Max2;
	std::vector<VarInfo>	m_sVarGlobal;
	std::vector<VarInfo>	m_sVarStack;
	std::vector<SCallStack>	m_sCallStack;
	std::vector<SFunctionTable> m_sFunctionPtr;

	std::map<u32, TableInfo*> _sTables;
	std::map<u32, StringInfo*> _sStrings;
	u32 _dwLastIDTable = 0;
	u32 _dwLastIDString = 0;
	u32 _dwLastIDVMWorker = 0;

	SNeoVMHeader			_header;
	std::map<std::string, int> m_sImExportTable;

	CNeoVMWorker*			_pMainWorker;
	std::map<u32, CNeoVMWorker*> _sVMWorkers;

	inline VarInfo* GetVarPtr(short n)
	{
		if (n >= 0)
		{
			return &m_sVarStack[_iSP_Vars + n];
		}
		return &m_sVarGlobal[-n - 1];
	}


	void Var_AddRef(VarInfo *d);
	void Var_Release(VarInfo *d);
	void Var_SetInt(VarInfo *d, int v);
	void Var_SetFloat(VarInfo *d, double v);
	void Var_SetBool(VarInfo *d, bool v);
	void Var_SetString(VarInfo *d, const char* str);
	void Var_SetTable(VarInfo *d, TableInfo* p);


	void Move(VarInfo* v1, VarInfo* v2);
	void MoveMinus(VarInfo* v1, VarInfo* v2);
	void Add2(VarInfo* r, VarInfo* v2);
	void Sub2(VarInfo* r, VarInfo* v2);
	void Mul2(VarInfo* r, VarInfo* v2);
	void Div2(VarInfo* r, VarInfo* v2);
	void Add(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Sub(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Mul(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Div(VarInfo* r, VarInfo* v1, VarInfo* v2);
	void Inc(VarInfo* v1);
	void Dec(VarInfo* v1);
	bool CompareEQ(VarInfo* v1, VarInfo* v2);
	bool CompareGR(VarInfo* v1, VarInfo* v2);
	bool CompareGE(VarInfo* v1, VarInfo* v2);

	std::string ToString(VarInfo* v1);
	int ToInt(VarInfo* v1);
	double ToFloat(VarInfo* v1);
	std::string GetType(VarInfo* v1);

	void TableInsert(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	void TableRead(VarInfo *pTable, VarInfo *pArray, VarInfo *pValue);
	FunctionPtr* GetPtrFunction(VarInfo *pTable, VarInfo *pArray);

	void ClearArgs()
	{
		_args.clear();
	}
	void PushInt(int v)
	{
		VarInfo d;
		d.SetType(VAR_INT);
		d._int = v;
		_args.push_back(d);
	}
	void PushFloat(double v)
	{
		VarInfo d;
		d.SetType(VAR_FLOAT);
		d._float = v;
		_args.push_back(d);
	}
	void PushString(const char* p)
	{
		VarInfo d;
		d.SetType(VAR_STRING);
		d._str = StringAlloc(p);
		_args.push_back(d);
	}
	void PushBool(bool b)
	{
		VarInfo d;
		d.SetType(VAR_BOOL);
		d._bl = b;
		_args.push_back(d);
	}

	int PopInt(VarInfo *V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return V->_int;
		case VAR_FLOAT:
			return (int)V->_float;
		}
		return -1;
	}
	double PopFloat(VarInfo *V)
	{
		switch (V->GetType())
		{
		case VAR_INT:
			return V->_int;
		case VAR_FLOAT:
			return V->_float;
		}
		return -1;
	}
	const char* PopString(VarInfo *V)
	{
		if (V->GetType() == VAR_STRING)
			return V->_str->_str.c_str();

		return NULL;
	}
	bool PopBool(VarInfo *V)
	{
		if (V->GetType() == VAR_BOOL)
			return V->_bl;

		return false;
	}


	CNeoVMWorker* WorkerAlloc(int iStackSize);
	void FreeWorker(CNeoVMWorker *d);

	StringInfo* StringAlloc(const char* str);
	void FreeString(VarInfo *d);

	TableInfo* TableAlloc();
	void FreeTable(VarInfo *d);

public:

	template<typename T>
	T _read(VarInfo *V) { return T(); }



	template<typename T>
	void write(VarInfo *V, T) { }



	template<typename T>
	T read(int idx) { return _read<T>(&m_sVarStack[_iSP_Vars + idx]); }

	template<typename T>
	void push(T ret) {}


	std::vector<VarInfo> _args;
	bool RunFunction(const std::string& funName);

	void GC()
	{
		for (int i = _iSP_Vars + 1; i < _iSP_Vars_Max2; i++)
			Var_Release(&m_sVarStack[i]);
		_iSP_Vars_Max2 = _iSP_Vars;
	}

	std::string _sErrorMsgDetail;
	const char* _pErrorMsg = NULL;

	void RegLibrary(VarInfo* pSystem, const char* pLibName, SFunLib* pFuns);
	void InitLib();
	bool Init(void* pBuffer, int iSize);
	inline void SetError(const char* pErrMsg);
public:
	CNeoVM(int iStackSize);
	virtual ~CNeoVM();



	template<typename RVal, typename ... Types>
	static int push_functor(FunctionPtr* pOut, RVal(*func)(Types ... args))
	{
		CNeoVMWorker::neo_pushcclosure(pOut, CNeoVMWorker::functor<RVal, Types ...>::invoke, (void*)func);
		return sizeof ...(Types);
	}

	template<typename F>
	bool Register(const char* name, F func)
	{
		auto it = m_sImExportTable.find(name);
		if (it == m_sImExportTable.end())
			return false;

		int iFID = (*it).second;
		FunctionPtr fun;
		int iArgCnt = push_functor(&fun, func);
		if (m_sFunctionPtr[iFID]._argsCount != iArgCnt)
			return false;

		m_sFunctionPtr[iFID]._fun = fun;
		return true;
	}


	template<typename F>
	static FunctionPtr Register(F func)
	{
		FunctionPtr fun;
		push_functor(&fun, func);
		return fun;
	}

	template<typename RVal, typename ... Types>
	RVal Call(const std::string& funName, Types ... args)
	{
		return _pMainWorker->Call<RVal>(funName, args...);
	}


	inline const char* GetLastErrorMsg() { return _sErrorMsgDetail.c_str();  }
	inline bool IsLastErrorMsg() { return (_sErrorMsgDetail.empty() == false); }
	void ClearLastErrorMsg() { _pErrorMsg = NULL; _sErrorMsgDetail.clear(); }

	static CNeoVM*	LoadVM(void* pBuffer, int iSize);
	static void		ReleaseVM(CNeoVM* pVM);
	static bool		Compile(void* pBufferSrc, int iLenSrc, void* pBufferCode, int iLenCode, int* pLenCode, bool putASM);
};

