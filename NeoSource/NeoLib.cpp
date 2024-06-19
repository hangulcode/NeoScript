#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include "NeoVMImpl.h"
#include "NeoArchive.h"
//#include "NeoTime.h"
#include "UTFString.h"

#define MATH_PI				3.14159265358979323846f // Pi
NeoScript::INeoVM::IO_Print NeoScript::INeoVM::m_pFunPrint = nullptr;

namespace NeoScript
{


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
	static bool List_broadcast(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 1) return false;
		VarInfo* pArg = pN->GetStack(1);
		if (pArg->GetType() != VAR_LIST) return false;

		auto m1 = pVar->_lst->GetMatrix();
		auto m2 = pArg->_lst->GetMatrix();
		if (m1.row <= 0 || m1.col <= 0) return false;
		if (m2.row <= 0 || m2.col <= 0) return false;

		if (m1.col != m2.col && m1.col != 1 && m2.col != 1) return false;
		if (m1.col != m2.col && m1.col != 1 && m2.col != 1) return false;

		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(pRet, pR); // return value

		int row = m1.row > m2.row ? m1.row : m2.row;
		int col = m1.col > m2.col ? m1.col : m2.col;

		pR->Resize(row, col);

		for(int r = 0; r < row; r++)
		{
			ListInfo* pTar = row != 1 ? pR->GetValue(r)->_lst : pR;
			ListInfo* pS1 = m1.row != 1 ? pVar->_lst->GetValue(r)->_lst : pVar->_lst;
			ListInfo* pS2 = m2.row != 1 ? pArg->_lst->GetValue(r)->_lst : pArg->_lst;

			NS_FLOAT v10 = pS1->GetValue(0)->GetFloatNumber();
			NS_FLOAT v20 = pS2->GetValue(0)->GetFloatNumber();
			pTar->SetValue(0, v10 + v20);

			NS_FLOAT v1, v2;
			for (int c = 1; c < col; c++)
			{
				if(m1.col == 1) v1 = v10;
				else v1 = pS1->GetValue(c)->GetFloatNumber();

				if (m2.col == 1) v2 = v20;
				else v2 = pS2->GetValue(c)->GetFloatNumber();
				pTar->SetValue(c, v1 + v2);
			}
		}

		return true;
	}
	static bool List_multiply(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 1) return false;
		VarInfo* pArg = pN->GetStack(1);
		if (pArg->GetType() != VAR_LIST) return false;

		auto m1 = pVar->_lst->GetMatrix();
		auto m2 = pArg->_lst->GetMatrix();
		if (m1.row <= 0 || m1.col <= 0) return false;
		if (m2.row <= 0 || m2.col <= 0) return false;

		if (m1.col != m2.row) return false;

		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(pRet, pR); // return value

		int row = m1.row;
		int col = m2.col;

		pR->Resize(row, col);

		std::vector< ListInfo*> S2;
		S2.resize(m2.row);
		for (int r = 0; r < m2.row; r++)
		{
			ListInfo* pS2 = m2.row != 1 ? pArg->_lst->GetValue(r)->_lst : pArg->_lst;
			S2[r] = pS2;
		}
		for (int r = 0; r < row; r++)
		{
			ListInfo* pTar = row != 1 ? pR->GetValue(r)->_lst : pR;
			ListInfo* pS1 = m1.row != 1 ? pVar->_lst->GetValue(r)->_lst : pVar->_lst;

			NS_FLOAT v1, v2;
			for (int c = 0; c < col; c++)
			{
				NS_FLOAT r = 0;
				for(int i = 0; i < m1.col; i++)
				{
					v1 = pS1->GetValue(i)->GetFloatNumber();
					v2 = S2[i]->GetValue(c)->GetFloatNumber();
					r += v1 * v2;
				}
				pTar->SetValue(c, r);
			}
		}
		return true;
	}
	static bool List_dot(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 1) return false;
		VarInfo* pArg = pN->GetStack(1);
		if (pArg->GetType() != VAR_LIST) return false;

		auto m1 = pVar->_lst->GetMatrix();
		auto m2 = pArg->_lst->GetMatrix();
		if (m1.row <= 0 || m1.col <= 0) return false;
		if (m2.row <= 0 || m2.col <= 0) return false;

		if (m1.row != m2.row) return false;
		if (m1.col != m2.col) return false;

		VarInfo* pRet = pN->GetStack(0);
		ListInfo* pR = pN->GetVM()->ListAlloc();
		pN->Var_SetList(pRet, pR); // return value

		int row = 1;
		int col = m1.row;

		pR->Resize(row, col);

		ListInfo* pTar = pR;
		for (int r = 0; r < m1.row; r++)
		{
			ListInfo* pS1 = m1.row != 1 ? pVar->_lst->GetValue(r)->_lst : pVar->_lst;
			ListInfo* pS2 = m2.row != 1 ? pArg->_lst->GetValue(r)->_lst : pArg->_lst;

			NS_FLOAT sum = 0;
			for (int c = 0; c < m1.col; c++)
			{
				NS_FLOAT v1 = pS1->GetValue(c)->GetFloatNumber();
				NS_FLOAT v2 = pS2->GetValue(c)->GetFloatNumber();
				sum += v1 * v2;
			}
			pTar->SetValue(r, sum);
		}

		return true;
	}
	static bool List_sum(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_LIST) return false;
		if (args != 0) return false;

		auto m1 = pVar->_lst->GetMatrix();
		if (m1.row <= 0 || m1.col <= 0) return false;

		NS_FLOAT sum = 0;
		for (int r = 0; r < m1.row; r++)
		{
			ListInfo* pS1 = m1.row != 1 ? pVar->_lst->GetValue(r)->_lst : pVar->_lst;
			for (int c = 0; c < m1.col; c++)
				sum += pS1->GetValue(c)->GetFloatNumber();
		}
		pN->ReturnValue(sum);
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
		//::srand((u32)init);
		pN->m_sRand.seed(init);
		pN->ReturnValue();
		return true;
	}
	static bool	Math_rand(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		//pN->ReturnValue((int)::rand());
		pN->ReturnValue(pN->m_sRand.rnd());
		return true;
	}

	static bool sys_meta(CNeoVMWorker* pN, VarInfo* pVar, short args)
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
	static bool map_len(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pVar->GetType() != VAR_MAP) return false;
		if (args != 0) return false;

		pN->ReturnValue((int)pVar->_tbl->GetCount());
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
			CNeoVMWorker::Move_DestNoRelease(&dest[i], lst[i]);
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
			CNeoVMWorker::Move_DestNoRelease(&dest[i], lst[i]);
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
		pN->JumpAsyncMsg();
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

	static bool sys_time(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		auto now = std::chrono::system_clock::now();
		std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
		//std::localtime(&currentTime)

		pN->ReturnValue((int)currentTime);
		return true;
	}
	static bool sys_date(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		VarInfo* pArg1 = pN->GetStack(1);
		VarInfo* pArg2 = pN->GetStack(2);

		if (pArg1->GetType() != VAR_STRING || pArg2->GetType() != VAR_INT)
			return false;

		std::time_t currentTime = (u32)pArg2->_int;
		char buffer[80];
#ifdef _WIN32
		struct tm timeinfo;
		if (localtime_s(&timeinfo, &currentTime) != 0) 
			return false;
		std::strftime(buffer, sizeof(buffer), pArg1->_str->_str.c_str(), &timeinfo);
#else
		if(0 == std::strftime(buffer, sizeof(buffer), pArg1->_str->_str.c_str(), std::localtime(&currentTime)))
			return false;
#endif

		pN->ReturnValue(buffer);
		return true;
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

		NeoCompilerParam param(pArg1->_str->_str.c_str(), (int)pArg1->_str->_str.length());
		param.err = &err;
		param.putASM = false;
		param.debug = false;

		INeoVM* pVM = INeoVM::CompileAndLoadVM(param);

		if (false == INeoVM::Compile(arCode, param))
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
	static bool coroutine_create(CNeoVMWorker* pN, VarInfo* pVar, short args)
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
	static bool coroutine_resume(CNeoVMWorker* pN, VarInfo* pVar, short args)
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
	static bool coroutine_status(CNeoVMWorker* pN, VarInfo* pVar, short args)
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
	static bool coroutine_close(CNeoVMWorker* pN, VarInfo* pVar, short args)
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

	static bool sys_aysnc_create(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;

		AsyncInfo* p = pN->GetVM()->AsyncAlloc();
		pN->ReturnValue(p);
		return true;
	}	

	static bool sys_set(CNeoVMWorker* pN, VarInfo* pVar, short args)
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

static VMHash<TYPE_NeoLib> g_sNeoFunLib_Default;
static VMHash<TYPE_NeoLib> g_sNeoFunLib_List;
static VMHash<TYPE_NeoLib> g_sNeoFunLib_String;
static VMHash<TYPE_NeoLib> g_sNeoFunLib_Map;
static VMHash<TYPE_NeoLib> g_sNeoFunLib_Async;

bool CNeoVMImpl::_funInitLib = false;
FunctionPtrNative CNeoVMImpl::_funLib_Default;
FunctionPtrNative CNeoVMImpl::_funLib_List;
FunctionPtrNative CNeoVMImpl::_funLib_String;
FunctionPtrNative CNeoVMImpl::_funLib_Map;
FunctionPtrNative CNeoVMImpl::_funLib_Async;


static bool Fun_Default(INeoVMWorker* pN, void* pUserData, const VMString* pStr, short args)
{
	TYPE_NeoLib f;
	if(false == g_sNeoFunLib_Default.TryGetValue(pStr, &f))
		return false;

	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_String(INeoVMWorker* pN, void* pUserData, const VMString* pStr, short args)
{
	TYPE_NeoLib f;
	if (false == g_sNeoFunLib_String.TryGetValue(pStr, &f))
		return false;

	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_List(INeoVMWorker* pN, void* pUserData, const VMString* pStr, short args)
{
	TYPE_NeoLib f;
	if (false == g_sNeoFunLib_List.TryGetValue(pStr, &f))
		return false;

	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_Map(INeoVMWorker* pN, void* pUserData, const VMString* pStr, short args)
{
	TYPE_NeoLib f;
	if (false == g_sNeoFunLib_Map.TryGetValue(pStr, &f))
		return false;

	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}
static bool Fun_Async(INeoVMWorker* pN, void* pUserData, const VMString* pStr, short args)
{
	TYPE_NeoLib f;
	if (false == g_sNeoFunLib_Async.TryGetValue(pStr, &f))
		return false;

	return (*f)((CNeoVMWorker*)pN, (VarInfo*)pUserData, args);
}

std::map<std::string, std::list< SystemFun>> g_sSystemFuns;
std::string g_sCurrentSystem;

static void AddSystemFun(const std::string& fname, TYPE_NeoLib fun, int argcnt)
{
	g_sNeoFunLib_Default.Add("#" + fname, fun);
	if(g_sCurrentSystem.empty() == false)
	{
		auto it = g_sSystemFuns.find(g_sCurrentSystem);
		if(it == g_sSystemFuns.end())
		{
			std::list< SystemFun> lst;
			SystemFun v;
			v.fname = fname;
			v.argCount = argcnt;
			lst.push_back(v);

			g_sSystemFuns[g_sCurrentSystem] = lst;
		}
		else
		{
			std::list< SystemFun>& lst = (*it).second;
			SystemFun v;
			v.fname = fname;
			v.argCount = argcnt;
			lst.push_back(v);
		}
	}
}

static void AddGlobalLibFun()
{
	if (g_sNeoFunLib_Default.empty() == false)
		return;

	g_sNeoFunLib_Default.Add("print", &neo_libs::io_print);

	g_sCurrentSystem = "math";
	AddSystemFun("abs", &neo_libs::Math_abs, 1);
	AddSystemFun("acos", &neo_libs::Math_acos, 1);
	AddSystemFun("asin", &neo_libs::Math_asin, 1);
	AddSystemFun("atan", &neo_libs::Math_atan, 1);
	AddSystemFun("ceil", &neo_libs::Math_ceil, 1);
	AddSystemFun("floor", &neo_libs::Math_floor, 1);
	AddSystemFun("sin", &neo_libs::Math_sin, 1);
	AddSystemFun("cos", &neo_libs::Math_cos, 1);
	AddSystemFun("tan", &neo_libs::Math_tan, 1);
	AddSystemFun("log", &neo_libs::Math_log, 1);
	AddSystemFun("log10", &neo_libs::Math_log10, 1);
	AddSystemFun("pow", &neo_libs::Math_pow, 2);
	AddSystemFun("deg", &neo_libs::Math_deg, 1);
	AddSystemFun("rad", &neo_libs::Math_rad, 1);
	AddSystemFun("sqrt", &neo_libs::Math_sqrt, 1);
	AddSystemFun("srand", &neo_libs::Math_srand, 1);
	AddSystemFun("rand", &neo_libs::Math_rand, 0);

	g_sCurrentSystem = "system";
	AddSystemFun("time", &neo_libs::sys_time, 0);
	AddSystemFun("date", &neo_libs::sys_date, 2);
	AddSystemFun("clock", &neo_libs::sys_clock, 0);
	AddSystemFun("meta", &neo_libs::sys_meta, 2);
	AddSystemFun("load", &neo_libs::sys_load, 2);
	AddSystemFun("pcall", &neo_libs::sys_pcall, 1);
	AddSystemFun("aysnc_create", &neo_libs::sys_aysnc_create, 0);
	AddSystemFun("set", &neo_libs::sys_set, 1);


	g_sCurrentSystem = "coroutine";
	AddSystemFun("create", &neo_libs::coroutine_create, 1);
	AddSystemFun("resume", &neo_libs::coroutine_resume, -1);
	AddSystemFun("status", &neo_libs::coroutine_status, 1);
	AddSystemFun("close", &neo_libs::coroutine_close, -1);

	g_sCurrentSystem.clear();
}
bool CNeoVMImpl::IsGlobalLibFun(std::string& FunName)
{
	//InitLib();
	//return g_sNeoFunLib_Default.IsKey(FunName);
	return FunName == "print";
}
const std::list< SystemFun>* CNeoVMImpl::GetSystemModule(const std::string& module)
{
	auto it = g_sSystemFuns.find(module);
	if(it == g_sSystemFuns.end())
		return nullptr;
	return &(*it).second;
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
	g_sNeoFunLib_String.Add("sub", &neo_libs::Str_sub);
	g_sNeoFunLib_String.Add("len", &neo_libs::Str_len);
	g_sNeoFunLib_String.Add("find", &neo_libs::Str_find);
	g_sNeoFunLib_String.Add("upper", &neo_libs::Str_upper);
	g_sNeoFunLib_String.Add("lower", &neo_libs::Str_lower);
	g_sNeoFunLib_String.Add("trim", &neo_libs::Str_trim);
	g_sNeoFunLib_String.Add("ltrim", &neo_libs::Str_ltrim);
	g_sNeoFunLib_String.Add("rtrim", &neo_libs::Str_rtrim);
	g_sNeoFunLib_String.Add("replace", &neo_libs::Str_replace);
	g_sNeoFunLib_String.Add("split", &neo_libs::Str_split);

	// List Lib
	_funLib_List = CNeoVMImpl::RegisterNative(Fun_List);
	g_sNeoFunLib_List.Add("resize", &neo_libs::List_resize);
	g_sNeoFunLib_List.Add("len", &neo_libs::List_len);
	g_sNeoFunLib_List.Add("append", &neo_libs::List_append);
	g_sNeoFunLib_List.Add("broadcast", &neo_libs::List_broadcast);
	g_sNeoFunLib_List.Add("multiply", &neo_libs::List_multiply);
	g_sNeoFunLib_List.Add("dot", &neo_libs::List_dot);
	g_sNeoFunLib_List.Add("sum", &neo_libs::List_sum);

	// Map Lib
	_funLib_Map = CNeoVMImpl::RegisterNative(Fun_Map);
	g_sNeoFunLib_Map.Add("len", &neo_libs::map_len);
	g_sNeoFunLib_Map.Add("reserve", &neo_libs::map_reserve);
	g_sNeoFunLib_Map.Add("sort", &neo_libs::map_sort);
	g_sNeoFunLib_Map.Add("keys", &neo_libs::map_keys);
	g_sNeoFunLib_Map.Add("values", &neo_libs::map_values);

	// Async Lib
	_funLib_Async = CNeoVMImpl::RegisterNative(Fun_Async);
	g_sNeoFunLib_Async.Add("get", &neo_libs::async_get);
	g_sNeoFunLib_Async.Add("post", &neo_libs::async_post);
	g_sNeoFunLib_Async.Add("add_header", &neo_libs::async_add_header);
	g_sNeoFunLib_Async.Add("wait", &neo_libs::async_wait);
	g_sNeoFunLib_Async.Add("close", &neo_libs::async_close);
}

void CNeoVMImpl::InitLib()
{
/*	VarInfo* pSystem = &m_sVarGlobal[0];
	Var_SetTable(pSystem, TableAlloc());

	RegLibrary(pSystem, "sys");*/
	RegObjLibrary();
}

};