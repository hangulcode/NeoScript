#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include "NeoVMImpl.h"
#include "NeoArchive.h"
#include "UTFString.h"

#define MATH_PI				3.14159265358979323846f // Pi


INeoVM::IO_Print INeoVM::m_pFunPrint = nullptr;

void NVM_QuickSort(CNeoVMWorker* pN, int compare, std::vector<VarInfo*>& lst);

/*
		int len;
		std::string* p;
		std::string tempStr;
		if (pVar->GetType() == VAR_STRING)
		{
			len = pVar->_str->_StringLen;
			p = &pVar->_str->_str;
		}
		else if (pVar->GetType() == VAR_CHAR)
		{
			len = pVar->_c.c[0] == 0 ? 0 : 1;
			tempStr = pVar->_c.c;
			p = &tempStr;
		}
		else
			return false;
*/
struct neo_libs
{
	static bool Str_sub(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 2) return false;

		int len = pVar->_str->_StringLen;
		std::string* p = &pVar->_str->_str;
		int p1 = pN->read<int>(1);
		int p2 = pN->read<int>(2);

		if (p1 < 0 || p1 >= len) return false;

		p1 = utf_string::UTF8_OFFSET(*p, 0, p1);
		p2 = utf_string::UTF8_OFFSET(*p, p1, p2) - p1;

		std::string sTempString = p->substr(p1, p2);
		pN->ReturnValue(sTempString.c_str());
		return true;
	}
	static bool Str_len(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		pN->ReturnValue(pVar->_str->_StringLen);
		return true;
	}
	static bool Str_find(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 1) return false;

		std::string* p = &pVar->_str->_str;
		//std::string* p2 = pN->read<std::string*>(1);
		char* p2 = pN->read<char*>(1);
		if (p2 == NULL) return false;

		int iFind = (int)p->find(*p2);
		iFind = utf_string::UTF8_INDEX2OFFSET(*p, iFind);
		pN->ReturnValue((int)iFind);
		return true;
	}
	static bool Str_upper(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string str = pVar->_str->_str;
		std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_lower(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string str = pVar->_str->_str;
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_trim(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string drop = " ";
		std::string str = pVar->_str->_str;
		str = str.erase(str.find_last_not_of(drop) + 1);
		str = str.erase(0, str.find_first_not_of(drop));
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_ltrim(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string drop = " ";
		std::string str = pVar->_str->_str;
		str = str.erase(0, str.find_first_not_of(drop));
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_rtrim(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 0) return false;

		std::string drop = " ";
		std::string str = pVar->_str->_str;
		str = str.erase(str.find_last_not_of(drop) + 1);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_replace(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 2) return false;

		VarInfo *pFind = pN->GetStack(1);
		VarInfo *pReplace = pN->GetStack(2);
		if (pFind->GetType() != VAR_STRING) return false;
		if (pReplace->GetType() != VAR_STRING) return false;

		std::string str = pVar->_str->_str;
		str.replace(str.find(pFind->_str->_str), pFind->_str->_str.length(), pReplace->_str->_str);
		pN->ReturnValue(str.c_str());
		return true;
	}
	static bool Str_split(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_STRING) return false;
		if (args != 1) return false;

		VarInfo *pFind = pN->GetStack(1);
		if (pFind->GetType() != VAR_STRING) return false;
		std::string& findstr = pFind->_str->_str;
		std::string str = pVar->_str->_str;

		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pListR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(pRet, pListR); // Set Return Value

		size_t previous = 0, current;
		current = str.find(findstr);
		while (current != std::string::npos)
		{
			std::string substring = str.substr(previous, current - previous);
			pListR->InsertLast(substring);

			previous = current + 1;
			current = str.find(findstr, previous);
		}
		pListR->InsertLast(str.substr(previous, current - previous)); // Last
		return true;
	}


	static bool List_resize(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 1) return false;

		int size = pN->read<int>(1);
		pVar->_lst->Resize(size);
		pN->ReturnValue();
		return true;
	}
	static bool List_len(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 0) return false;

		pN->ReturnValue((int)pVar->_lst->GetCount());
		return true;
	}
	static bool List_append(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args == 1)
		{
			pVar->_lst->InsertLast(pN->GetStack(1));
			pN->ReturnValue();
			return true;
		}
		else if (args == 2)
		{
			VarInfo* pIndex = pN->GetStack(2);
			if (pIndex->GetType() != VAR_INT) return false;
			if (false == pVar->_lst->Insert(pIndex->_int, pN->GetStack(1)))
				return false;
			pN->ReturnValue();
			return true;
		}
		return true;
	}

	static bool Math_abs(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::abs(v));
		return true;
	}
	static bool Math_acos(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::acos(v));
		return true;
	}
	static bool Math_asin(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::asin(v));
		return true;
	}
	static bool Math_atan(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::atan(v));
		return true;
	}
	static bool Math_ceil(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::ceil(v));
		return true;
	}
	static bool Math_floor(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::floor(v));
		return true;
	}
	static bool Math_sin(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::sin(v));
		return true;
	}
	static bool Math_cos(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::cos(v));
		return true;
	}
	static bool Math_tan(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::tan(v));
		return true;
	}
	static bool Math_log(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::log(v));
		return true;
	}
	static bool Math_log10(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::log10(v));
		return true;
	}
	static bool Math_pow(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		NS_FLOAT v1 = pN->read<NS_FLOAT>(1);
		NS_FLOAT v2 = pN->read<NS_FLOAT>(2);
		pN->ReturnValue(::pow(v1, v2));
		return true;
	}
	static bool Math_deg(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT radian = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(((radian) * (180.0f / MATH_PI)));
		return true;
	}
	static bool Math_rad(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT degree = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(((degree) * (MATH_PI / 180.0f)));
		return true;
	}
	static bool Math_sqrt(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::sqrt(v));
		return true;
	}
	static bool	Math_srand(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		int init = pN->read<int>(1);
		::srand((u32)init);
		pN->ReturnValue();
		return true;
	}
	static bool	Math_rand(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		pN->ReturnValue((int)::rand());
		return true;
	}

	static bool util_meta(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false; // table, meta

		VarInfo *pTable = pN->GetStack(1);
		VarInfo *pMeta = pN->GetStack(2);
		if (pTable->GetType() != VAR_MAP) return false;
		if (pMeta->GetType() != VAR_MAP) return false;

		VarInfo var;
		var.ClearType();
		if (pTable->_tbl->_meta)
		{
			var.SetType(VAR_MAP);
			var._tbl = pTable->_tbl->_meta;
		}

		pN->Move(&var, pMeta); // for Referance
		pTable->_tbl->_meta = pMeta->_tbl;
		pN->ReturnValue();
		return true;
	}
	static bool map_reserve(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_MAP) return false;
		if (args != 1) return false;

		int size = pN->read<int>(1);
		pVar->_tbl->Reserve(size);
		pN->ReturnValue();
		return true;
	}
	static bool map_sort(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false; // fun

		VarInfo *pFun = pN->GetStack(1);
		if (pVar->GetType() != VAR_MAP) return false;

		if (pFun->GetType() != VAR_FUN) return false;

		std::vector<VarInfo*> lst;
		if (false == pVar->_tbl->ToListValues(lst)) return false;

		if (lst.size() >= 2)
		{
			std::vector<VarInfo*> lstSorted = lst;
			NVM_QuickSort(pN, pFun->_fun_index, lstSorted);

			std::vector<VarInfo> lst3;
			lst3.resize(lstSorted.size());
			for (size_t i = 0; i < lstSorted.size(); i++)
				lst3[i] = *lstSorted[i];

			for (size_t i = 0; i < lst.size(); i++)
				*lst[i] = lst3[i];
		}
		pN->ReturnValue();
		return true;
	}
	static bool map_keys(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;
		if (pVar->GetType() != VAR_MAP) return false;

		std::vector<VarInfo*> lst;
		if (false == pVar->_tbl->ToListKeys(lst)) return false;
		
		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(pRet, pR); // return value
		pR->Resize((int)lst.size());

		VarInfo* dest = pR->GetDataUnsafe();
		for (int i = 0; i < (int)lst.size(); i++)
			INeoVM::Move_DestNoRelease(&dest[i], lst[i]);
		return true;
	}
	static bool map_values(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;
		if (pVar->GetType() != VAR_MAP) return false;

		std::vector<VarInfo*> lst;
		if (false == pVar->_tbl->ToListValues(lst)) return false;

		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(pRet, pR); // return value
		pR->Resize((int)lst.size());

		VarInfo* dest = pR->GetDataUnsafe();
		for (int i = 0; i < (int)lst.size(); i++)
			INeoVM::Move_DestNoRelease(&dest[i], lst[i]);
		return true;
	}

	static bool async_get(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		if(pAsync->_state != ASYNC_READY) return false;

		VarInfo* v1 = pN->GetStack(1);
		if (v1->GetType() != VAR_INT)
			return false;

		VarInfo* v2 = pN->GetStack(2);
		if (v2->GetType() != VAR_STRING) // VAR_CHAR is error 
			return false;

		VarInfo* v3 = pN->GetStack(3);
		if (v3->GetType() != VAR_FUN) // 
			return false;

		pAsync->_type = ASYNC_GET;
		pAsync->_timeout = v1->_int;
		pAsync->_request = v2->_str->_str;
		pAsync->_fun_index = v3->_fun_index;
		if (pAsync->_timeout == -1) pAsync->_timeout = 0x7fffffff;

		pAsync->_state = ASYNC_PENDING;
		pN->Move(&pAsync->_LockReferance, pVar);
		pN->GetVM()->AddHttp_Request(pAsync);
		pN->ReturnValue();
		return true;
	}
	static bool async_post(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 4) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		if (pAsync->_state != ASYNC_READY) return false;

		VarInfo* v1 = pN->GetStack(1);
		if (v1->GetType() != VAR_INT)
			return false;

		VarInfo* v2 = pN->GetStack(2);
		if (v2->GetType() != VAR_STRING) // VAR_CHAR is error 
			return false;

		VarInfo* v3 = pN->GetStack(3);
		if (v3->GetType() != VAR_STRING) // 
			return false;

		VarInfo* v4 = pN->GetStack(4);
		if (v4->GetType() != VAR_FUN) // 
			return false;

		pAsync->_type = ASYNC_POST;
		pAsync->_timeout = v1->_int;
		pAsync->_request = v2->_str->_str;
		pAsync->_body = v3->_str->_str;
		pAsync->_fun_index = v4->_fun_index;
		if (pAsync->_timeout == -1) pAsync->_timeout = 0x7fffffff;

		pAsync->_state = ASYNC_PENDING;
		pN->Move(&pAsync->_LockReferance, pVar);
		pN->GetVM()->AddHttp_Request(pAsync);
		pN->ReturnValue();
		return true;
	}

	static bool async_add_header(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		if (pAsync->_state != ASYNC_READY) return false;

		VarInfo* v1 = pN->GetStack(1);
		if (v1->GetType() != VAR_STRING) // VAR_CHAR is error 
			return false;

		VarInfo* v2 = pN->GetStack(2);
		if (v2->GetType() != VAR_STRING) // VAR_CHAR is error 
			return false;

		std::pair<std::string, std::string> header = { v1->_str->_str, v2->_str->_str };
		pAsync->_headers.push_back(header);
		pN->ReturnValue();
		return true;
	}
	static bool async_wait(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		if (pAsync->_state != ASYNC_PENDING) return true;

		bool ok = pAsync->_event.wait(pAsync->_timeout);
		pN->SetCheckTime();
		pN->ReturnValue(ok);
		return true;
	}
	static bool async_close(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		pN->ReturnValue();
		return true;
	}

	static bool io_print(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args == 1)
		{
			VarInfo* pArg1 = pN->GetStack(1);
			std::string str = CNeoVMWorker::ToString(pArg1);
			if(INeoVM::m_pFunPrint == nullptr)
				std::cout << str.c_str() << '\n';
			else
				INeoVM::m_pFunPrint(str.c_str());
			pN->ReturnValue();
			return true;
		}
		else if (args == 2)
		{
			VarInfo* pArg1 = pN->GetStack(1);
			VarInfo* pArg2 = pN->GetStack(2);
			std::string str1 = CNeoVMWorker::ToString(pArg1);
			std::string str2 = CNeoVMWorker::ToString(pArg2);
			if (INeoVM::m_pFunPrint == nullptr)
				std::cout << str1.c_str() << str2.c_str();
			else
				INeoVM::m_pFunPrint((str1 + str2).c_str());
			pN->ReturnValue();
			return true;
		}
		return false;
	}

	static bool sys_clock(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		pN->ReturnValue(NS_FLOAT((double)clock() / (double)CLOCKS_PER_SEC));
		return true;
	}
	static bool sys_load(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		VarInfo* pArg1 = pN->GetStack(1);
		VarInfo* pArg2 = pN->GetStack(2);

		if (pArg1->GetType() != VAR_STRING) return false;
		if (pArg2->GetType() != VAR_STRING) return false;

		CNArchive arCode;
		std::string err;
		if (false == INeoVM::Compile(pArg1->_str->_str.c_str(), (int)pArg1->_str->_str.length(), arCode, err, false, false))
		{
			return false;
		}

		INeoVMWorker* pModule = pN->_pVM->LoadVM(arCode.GetData(), arCode.GetBufferOffset());
		if (pModule == NULL)
		{
			pN->ReturnValue();
			return false; // ?
		}

		VarInfo* pRet = pN->GetStack(0);
		pN->Var_SetModule(pRet, pModule);
		return true;
	}
	static bool sys_pcall(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* pArg1 = pN->GetStack(1);
		if (pArg1->GetType() != VAR_MODULE) return false;

		pN->_pVM->PCall(pArg1->_module->GetWorkerID());

		pN->ReturnValue();
		return true;
	}
	static bool sys_coroutine_create(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_FUN)
			return false;

		CNeoVMImpl* pVM = pN->GetVM();
		CoroutineInfo* pCI = pVM->CoroutineAlloc();
		pCI->_refCount = 0;
		pCI->_fun_index = v->_fun_index;
		pCI->_state = COROUTINE_STATE_SUSPENDED;

		pN->ReturnValue(pCI);
		return true;
	}
	static bool sys_coroutine_resume(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args < 1) return false; // param : index 1 ~ 

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_COROUTINE) return false;

		CoroutineInfo* pCI = v->_cor;
		if (pCI->_state != COROUTINE_STATE_SUSPENDED) return false;

		pN->m_pRegisterActive = pCI;
		pCI->_sub_state = COROUTINE_SUB_START;
		pN->ReturnValue();
		return true;
	}
	static bool sys_coroutine_status(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_COROUTINE) return false;

		CNeoVMImpl* pVM = pN->GetVM();
		switch (v->_cor->_state)
		{
		case COROUTINE_STATE_SUSPENDED:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_SUSPENDED]);
			break;
		case COROUTINE_STATE_RUNNING:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_RUNNING]);
			break;
		case COROUTINE_STATE_DEAD:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_DEAD]);
			break;
		case COROUTINE_STATE_NORMAL:
			pN->ReturnValue(&pVM->m_sDefaultValue[NDF_NORMAL]);
			break;
		default:
			pN->ReturnValue();
			return false;
		}

		return true;
	}
	static bool sys_coroutine_close(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args >= 2) return false;

		CoroutineInfo* pCI;
		if(args == 1)
		{
			VarInfo* v = pN->GetStack(1);
			if (v->GetType() != VAR_COROUTINE)
				return false;
			pCI = v->_cor;
			if(pCI->_state == COROUTINE_STATE_DEAD) 
				return true; // Already Dead State (For convenience, return true.)
		}
		else
		{
			pCI = pN->m_pCur;
		}


		pN->m_pRegisterActive = pCI;
		pCI->_sub_state = COROUTINE_SUB_CLOSE;

		pN->ReturnValue(pCI);
		return true;
	}

	static bool aysnc_create(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		AsyncInfo* p = pN->GetVM()->AsyncAlloc();
		pN->ReturnValue(p);
		return true;
	}	

	static bool set(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* pRet = pN->GetStack(0);
		VarInfo* pArg1 = pN->GetStack(1);
		switch(pArg1->GetType())
		{
		case VAR_LIST:
			{
				SetInfo* pSetR = pN->GetVM()->SetAlloc();
				pN->Var_SetSet(pRet, pSetR);

				ListInfo* pListV1 = pArg1->_lst;
				int sz = pListV1->GetCount();
				VarInfo* src = pListV1->GetDataUnsafe();
				for (int i = 0; i < sz; i++)
				{
					pSetR->Insert(&src[i]);
				}
				return true;
			}
		default:
			pN->SetErrorUnsupport("Unsupport set('%s')", pArg1);
			return false;
		}
		pN->ReturnValue();
		return false;
	}
};



//typedef bool (ClassName::*TYPE_NeoLib)(CNeoVMWorker* pN, short args);
//typedef bool(*TYPE_NeoLib)(CNeoVMWorker* pN, short args);
typedef bool(*TYPE_NeoLib)(CNeoVMWorker* pN, VarInfo* pVar, short args);
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib_Default;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib_List;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib_String;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib_Map;
static std::map<std::string, TYPE_NeoLib> g_sNeoFunLib_Async;

bool CNeoVMImpl::_funInitLib = false;
FunctionPtrNative CNeoVMImpl::_funLib_Default;
FunctionPtrNative CNeoVMImpl::_funLib_List;
FunctionPtrNative CNeoVMImpl::_funLib_String;
FunctionPtrNative CNeoVMImpl::_funLib_Map;
FunctionPtrNative CNeoVMImpl::_funLib_Async;


static bool Fun_Default(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib_Default.find(fun);
	if (it == g_sNeoFunLib_Default.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_String(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib_String.find(fun);
	if (it == g_sNeoFunLib_String.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_List(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib_List.find(fun);
	if (it == g_sNeoFunLib_List.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_Map(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib_Map.find(fun);
	if (it == g_sNeoFunLib_Map.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_Async(INeoVMWorker* pN, void* pUserData, const std::string& fun, short args)
{
	auto it = g_sNeoFunLib_Async.find(fun);
	if (it == g_sNeoFunLib_Async.end())
		return false;

	TYPE_NeoLib f = (*it).second;
	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static void AddGlobalLibFun()
{
	if (g_sNeoFunLib_Default.empty() == false)
		return;

	g_sNeoFunLib_Default["abs"] = &neo_libs::Math_abs;
	g_sNeoFunLib_Default["acos"] = &neo_libs::Math_acos;
	g_sNeoFunLib_Default["asin"] = &neo_libs::Math_asin;
	g_sNeoFunLib_Default["atan"] = &neo_libs::Math_atan;
	g_sNeoFunLib_Default["ceil"] = &neo_libs::Math_ceil;
	g_sNeoFunLib_Default["floor"] = &neo_libs::Math_floor;
	g_sNeoFunLib_Default["sin"] = &neo_libs::Math_sin;
	g_sNeoFunLib_Default["cos"] = &neo_libs::Math_cos;
	g_sNeoFunLib_Default["tan"] = &neo_libs::Math_tan;
	g_sNeoFunLib_Default["log"] = &neo_libs::Math_log;
	g_sNeoFunLib_Default["log10"] = &neo_libs::Math_log10;
	g_sNeoFunLib_Default["pow"] = &neo_libs::Math_pow;
	g_sNeoFunLib_Default["deg"] = &neo_libs::Math_deg;
	g_sNeoFunLib_Default["rad"] = &neo_libs::Math_rad;
	g_sNeoFunLib_Default["sqrt"] = &neo_libs::Math_sqrt;
	g_sNeoFunLib_Default["srand"] = &neo_libs::Math_srand;
	g_sNeoFunLib_Default["rand"] = &neo_libs::Math_rand;

	g_sNeoFunLib_Default["sys_meta"] = &neo_libs::util_meta;
	g_sNeoFunLib_Default["print"] = &neo_libs::io_print;
	g_sNeoFunLib_Default["clock"] = &neo_libs::sys_clock;

	g_sNeoFunLib_Default["load"] = &neo_libs::sys_load;
	g_sNeoFunLib_Default["pcall"] = &neo_libs::sys_pcall;

	g_sNeoFunLib_Default["set"] = &neo_libs::set;

	g_sNeoFunLib_Default["coroutine_create"] = &neo_libs::sys_coroutine_create;
	g_sNeoFunLib_Default["coroutine_resume"] = &neo_libs::sys_coroutine_resume;
	g_sNeoFunLib_Default["coroutine_status"] = &neo_libs::sys_coroutine_status;
	g_sNeoFunLib_Default["coroutine_close"] = &neo_libs::sys_coroutine_close;

	g_sNeoFunLib_Default["aysnc_create"] = &neo_libs::aysnc_create;
}
bool CNeoVMImpl::IsGlobalLibFun(std::string& FunName)
{
	//AddGlobalLibFun();
	InitLib();
	auto it = g_sNeoFunLib_Default.find(FunName);
	if(it == g_sNeoFunLib_Default.end())
		return false;
	return true;
}
void CNeoVMImpl::RegLibrary(VarInfo* pSystem, const char* pLibName)
{
	MapInfo* pTable = pSystem->_tbl;
	pTable->_fun = CNeoVMImpl::RegisterNative(Fun_Default);
	//AddGlobalLibFun();

	//_funDefaultLib = CNeoVM::RegisterNative(Fun);
}

void CNeoVMImpl::RegObjLibrary()
{
	if (_funInitLib) return;
	_funInitLib = true;

	AddGlobalLibFun();
	_funLib_Default = CNeoVMImpl::RegisterNative(Fun_Default);

	// String Lib
	_funLib_String = CNeoVMImpl::RegisterNative(Fun_String);
	g_sNeoFunLib_String["sub"] = &neo_libs::Str_sub;
	g_sNeoFunLib_String["len"] = &neo_libs::Str_len;
	g_sNeoFunLib_String["find"] = &neo_libs::Str_find;
	g_sNeoFunLib_String["upper"] = &neo_libs::Str_upper;
	g_sNeoFunLib_String["lower"] = &neo_libs::Str_lower;
	g_sNeoFunLib_String["trim"] = &neo_libs::Str_trim;
	g_sNeoFunLib_String["ltrim"] = &neo_libs::Str_ltrim;
	g_sNeoFunLib_String["rtrim"] = &neo_libs::Str_rtrim;
	g_sNeoFunLib_String["replace"] = &neo_libs::Str_replace;
	g_sNeoFunLib_String["split"] = &neo_libs::Str_split;

	// List Lib
	_funLib_List = CNeoVMImpl::RegisterNative(Fun_List);
	g_sNeoFunLib_List["resize"] = &neo_libs::List_resize;
	g_sNeoFunLib_List["len"] = &neo_libs::List_len;
	g_sNeoFunLib_List["append"] = &neo_libs::List_append;

	// Map Lib
	_funLib_Map = CNeoVMImpl::RegisterNative(Fun_Map);
	g_sNeoFunLib_Map["reserve"] = &neo_libs::map_reserve;
	g_sNeoFunLib_Map["sort"] = &neo_libs::map_sort;
	g_sNeoFunLib_Map["keys"] = &neo_libs::map_keys;
	g_sNeoFunLib_Map["values"] = &neo_libs::map_values;

	// Async Lib
	_funLib_Async = CNeoVMImpl::RegisterNative(Fun_Async);
	g_sNeoFunLib_Async["get"] = &neo_libs::async_get;
	g_sNeoFunLib_Async["post"] = &neo_libs::async_post;
	g_sNeoFunLib_Async["add_header"] = &neo_libs::async_add_header;
	g_sNeoFunLib_Async["wait"] = &neo_libs::async_wait;
	g_sNeoFunLib_Async["close"] = &neo_libs::async_close;
}

void CNeoVMImpl::InitLib()
{
/*	VarInfo* pSystem = &m_sVarGlobal[0];
	Var_SetTable(pSystem, TableAlloc());

	RegLibrary(pSystem, "sys");*/
	RegObjLibrary();
}

