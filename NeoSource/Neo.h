#pragma once


#include "NeoVM.h"

namespace NeoHelper
{
	template<typename T>
	T _read(VarInfo *V) { return T(); }


	template<typename T>
	inline static T upvalue_(void* p)
	{
		return (T)p;
	}


	template<typename T>
	static T read(CNeoVMWorker* N, int idx) { T r; N->_read(N->GetStack(idx), r); return r; }


	// 리턴값이 있는 Call
	template<typename RVal, typename T1 = void, typename T2 = void, typename T3 = void, typename T4 = void, typename T5 = void>
	struct functor
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2, T3, T4, T5)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2), read<T3>(N, 3), read<T4>(N, 4), N->read<T5>(N, 5));
			N->ReturnValue(a);
			return 1;
		}
	};
	template<typename RVal, typename T1, typename T2, typename T3, typename T4>
	struct functor<RVal, T1, T2, T3, T4>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2, T3, T4)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2), read<T3>(N, 3), read<T4>(N, 4));
			N->ReturnValue(a);
			return 1;
		}
	};
	template<typename RVal, typename T1, typename T2, typename T3>
	struct functor<RVal, T1, T2, T3>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2, T3)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2), read<T3>(N, 3));
			N->ReturnValue(a);
			return 1;
		}
	};
	template<typename RVal, typename T1, typename T2>
	struct functor<RVal, T1, T2>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			auto a = upvalue_<RVal(*)(T1, T2)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2));
			N->ReturnValue(a);
			return 1;
		}
	};
	template<typename RVal, typename T1>
	struct functor<RVal, T1>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			auto a = upvalue_<RVal(*)(T1)>(pfun->_func)(read<T1>(N, 1));
			N->ReturnValue(a);
			return 1;
		}
	};
	template<typename RVal>
	struct functor<RVal>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			auto a = upvalue_<RVal(*)()>(pfun->_func)();
			N->ReturnValue(a);
			return 1;
		}
	};

	// 리턴값이 없는 Call
	template<typename T1, typename T2, typename T3, typename T4, typename T5>
	struct functor<void, T1, T2, T3, T4, T5>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			upvalue_<void(*)(T1, T2, T3, T4, T5)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2), read<T3>(N, 3), read<T4>(N, 4), N->read<T5>(N, 5));
			N->ReturnValue();
			return 0;
		}
	};
	template<typename T1, typename T2, typename T3, typename T4>
	struct functor<void, T1, T2, T3, T4>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			upvalue_<void(*)(T1, T2, T3, T4)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2), read<T3>(N, 3), read<T4>(N, 4));
			N->ReturnValue();
			return 0;
		}
	};
	template<typename T1, typename T2, typename T3>
	struct functor<void, T1, T2, T3>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			upvalue_<void(*)(T1, T2, T3)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2), read<T3>(N, 3));
			N->ReturnValue();
			return 0;
		}
	};
	template<typename T1, typename T2>
	struct functor<void, T1, T2>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			upvalue_<void(*)(T1, T2)>(pfun->_func)(read<T1>(N, 1), read<T2>(N, 2));
			N->ReturnValue();
			return 0;
		}
	};
	template<typename T1>
	struct functor<void, T1>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			upvalue_<void(*)(T1)>(pfun->_func)(read<T1>(N, 1));
			N->ReturnValue();
			return 0;
		}
	};
	template<>
	struct functor<void>
	{
		static int invoke(CNeoVMWorker *N, FunctionPtr* pfun, short args)
		{
			if (args != pfun->_argCount)
				return -1;
			upvalue_<void(*)()>(pfun->_func)();
			N->ReturnValue();
			return 0;
		}
	};

	template<typename RVal, typename ... Types>
	static int push_functor(FunctionPtr* pOut, RVal(*func)(Types ... args))
	{
		CNeoVMWorker::neo_pushcclosure(pOut, functor<RVal, Types ...>::invoke, (void*)func);
		return sizeof ...(Types);
	}

	template<typename F>
	bool Register(CNeoVM* VM, const char* name, F func)
	{
		int iFID = VM->FindFunction(name);
		if (iFID < 0)
			return false;

		FunctionPtr fun;
		int iArgCnt = push_functor(&fun, func);
		return VM->SetFunction(iFID, fun, iArgCnt);
	}

	template<typename RVal, typename ... Types>
	bool Call(RVal* r, NeoFunction f, Types ... args)
	{
		return f._pWorker->iCall<RVal>(*r, f._fun_index, args...);
	}

	template<typename ... Types>
	bool CallN(NeoFunction f, Types ... args)
	{
		return f._pWorker->iCallN(f._fun_index, args...);
	}
};

