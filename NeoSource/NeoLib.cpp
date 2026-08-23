#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "NeoVMInternal.h"
#include "NeoVMWorker.h"
#include "NeoArchive.h"
//#include "NeoTime.h"
#include "UTFString.h"
#include <chrono>

#define MATH_PI				3.14159265358979323846f // Pi
NeoScript::NeoVMSystem::IO_Print NeoScript::NeoVMSystem::m_pFunPrint = nullptr;
NeoScript::NeoVMSystem::IO_Print NeoScript::NeoVMSystem::m_pFunError = nullptr;

namespace NeoScript
{


void NVM_QuickSort(CNeoVMWorker* pN, VarInfo* compare, std::vector<VarInfo*>& lst);

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

		int iFind = (int)p->find(p2);
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

			// 구분자 길이만큼 넘어가야 한다. 1 씩 넘기면 두 글자 이상 구분자에서
			// 남은 글자가 다음 토큰 앞에 붙는다("a::b".split("::") -> ["a", ":b"]).
			previous = current + findstr.length();
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
	static bool Math_round(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::round(v));
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
	static bool Math_exp(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		NS_FLOAT v = pN->read<NS_FLOAT>(1);
		pN->ReturnValue(::exp(v));
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
	static bool Math_Vector2(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;
		pN->Var_SetVec2(pN->GetReturnVar(), (float)pN->read<NS_FLOAT>(1), (float)pN->read<NS_FLOAT>(2));
		return true;
	}
	static bool Math_Vector3(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;
		pN->Var_SetVec3(pN->GetReturnVar(), (float)pN->read<NS_FLOAT>(1), (float)pN->read<NS_FLOAT>(2), (float)pN->read<NS_FLOAT>(3));
		return true;
	}
	static bool Math_Vector4(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 4) return false;
		pN->Var_SetVec4(pN->GetReturnVar(), (float)pN->read<NS_FLOAT>(1), (float)pN->read<NS_FLOAT>(2),
			(float)pN->read<NS_FLOAT>(3), (float)pN->read<NS_FLOAT>(4));
		return true;
	}
	static bool Math_Quaternion(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 4) return false;
		pN->Var_SetQuat(pN->GetReturnVar(), (float)pN->read<NS_FLOAT>(1), (float)pN->read<NS_FLOAT>(2),
			(float)pN->read<NS_FLOAT>(3), (float)pN->read<NS_FLOAT>(4));
		return true;
	}
	static NS_FLOAT MathClamp01Value(NS_FLOAT v)
	{
		if (v < (NS_FLOAT)0.0) return (NS_FLOAT)0.0;
		if (v > (NS_FLOAT)1.0) return (NS_FLOAT)1.0;
		return v;
	}
	static NS_FLOAT MathClampValue(NS_FLOAT v, NS_FLOAT minValue, NS_FLOAT maxValue)
	{
		if (v < minValue) return minValue;
		if (v > maxValue) return maxValue;
		return v;
	}
	static bool Math_Clamp01(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;
		pN->ReturnValue(MathClamp01Value(pN->read<NS_FLOAT>(1)));
		return true;
	}
	static bool Math_Clamp(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;
		pN->ReturnValue(MathClampValue(pN->read<NS_FLOAT>(1), pN->read<NS_FLOAT>(2), pN->read<NS_FLOAT>(3)));
		return true;
	}
	static bool Math_SmoothStep01(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;
		NS_FLOAT x = MathClamp01Value(pN->read<NS_FLOAT>(1));
		pN->ReturnValue(x * x * ((NS_FLOAT)3.0 - (NS_FLOAT)2.0 * x));
		return true;
	}
	static bool Math_Lerp(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;
		NS_FLOAT a = pN->read<NS_FLOAT>(1);
		NS_FLOAT b = pN->read<NS_FLOAT>(2);
		NS_FLOAT t = pN->read<NS_FLOAT>(3);
		pN->ReturnValue(a + (b - a) * t);
		return true;
	}
	// Read 는 VarInfo::GetVec* 로 벡터 값타입(VAR_VEC*)만 받는다 (리스트 폴백 없음).
	static bool ReadVec3(VarInfo* pVar, float out[3])
	{
		return pVar != nullptr && pVar->GetVec3(out);
	}
	// Write 는 워커 Var_SetVec3(release 처리)로 인라인 값타입을 반환한다(힙 리스트 할당 제거).
	static bool WriteVec3(CNeoVMWorker* pN, float x, float y, float z)
	{
		pN->Var_SetVec3(pN->GetReturnVar(), x, y, z);
		return true;
	}
	static bool ReadQuat(VarInfo* pVar, float out[4])
	{
		return pVar != nullptr && pVar->GetQuat(out);
	}
	// q 는 wxyz. 정규화한 뒤 반환한다 (인자는 건드리지 않는다 — 호출측이 계속 쓸 수 있게).
	static bool WriteQuat(CNeoVMWorker* pN, float w, float x, float y, float z)
	{
		NS_FLOAT lenSq = w * w + x * x + y * y + z * z;
		if (lenSq < (NS_FLOAT)0.000001)
		{
			pN->Var_SetQuat(pN->GetReturnVar(), 1.0f, 0.0f, 0.0f, 0.0f);
		}
		else
		{
			float invLen = (float)((NS_FLOAT)1.0 / (NS_FLOAT)::sqrt(lenSq));
			pN->Var_SetQuat(pN->GetReturnVar(), w * invLen, x * invLen, y * invLen, z * invLen);
		}
		return true;
	}
	static bool Math_Lerp3(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;

		float a[3], b[3];
		if (ReadVec3(pN->GetStackVar(1), a) == false) return false;
		if (ReadVec3(pN->GetStackVar(2), b) == false) return false;
		NS_FLOAT t = pN->read<NS_FLOAT>(3);

		return WriteVec3(pN,
			(float)(a[0] + (b[0] - a[0]) * t),
			(float)(a[1] + (b[1] - a[1]) * t),
			(float)(a[2] + (b[2] - a[2]) * t));
	}
	static bool Math_DistanceSquared3(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		float a[3], b[3];
		if (ReadVec3(pN->GetStackVar(1), a) == false) return false;
		if (ReadVec3(pN->GetStackVar(2), b) == false) return false;

		NS_FLOAT dx = a[0] - b[0];
		NS_FLOAT dy = a[1] - b[1];
		NS_FLOAT dz = a[2] - b[2];
		pN->ReturnValue(dx * dx + dy * dy + dz * dz);
		return true;
	}
	static bool Math_Normalize3(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		float v[3], fallback[3];
		if (ReadVec3(pN->GetStackVar(1), v) == false) return false;
		if (ReadVec3(pN->GetStackVar(2), fallback) == false) return false;
		NS_FLOAT lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
		if (lenSq < (NS_FLOAT)0.00000001)
			return WriteVec3(pN, fallback[0], fallback[1], fallback[2]);

		float invLen = (float)((NS_FLOAT)1.0 / (NS_FLOAT)::sqrt(lenSq));
		return WriteVec3(pN, v[0] * invLen, v[1] * invLen, v[2] * invLen);
	}
	static bool Math_Cross3(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		float a[3], b[3];
		if (ReadVec3(pN->GetStackVar(1), a) == false) return false;
		if (ReadVec3(pN->GetStackVar(2), b) == false) return false;
		return WriteVec3(pN,
			a[1] * b[2] - a[2] * b[1],
			a[2] * b[0] - a[0] * b[2],
			a[0] * b[1] - a[1] * b[0]);
	}
	static bool Math_RotateVectorByQuat(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;

		float q[4];   // wxyz
		if (ReadQuat(pN->GetStackVar(1), q) == false) return false;

		float v[3];
		if (ReadVec3(pN->GetStackVar(2), v) == false) return false;
		const float qw = q[0], qx = q[1], qy = q[2], qz = q[3];
		const float x = v[0], y = v[1], z = v[2];
		NS_FLOAT tx = (NS_FLOAT)2.0 * (qy * z - qz * y);
		NS_FLOAT ty = (NS_FLOAT)2.0 * (qz * x - qx * z);
		NS_FLOAT tz = (NS_FLOAT)2.0 * (qx * y - qy * x);
		return WriteVec3(pN,
			(float)(x + qw * tx + (qy * tz - qz * ty)),
			(float)(y + qw * ty + (qz * tx - qx * tz)),
			(float)(z + qw * tz + (qx * ty - qy * tx)));
	}
	// v[0..2] 를 제자리 정규화. 길이가 0 에 가까우면 실패(값은 그대로).
	static bool NormalizeVec3(float v[3])
	{
		NS_FLOAT lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
		if (lenSq < (NS_FLOAT)0.000001) return false;
		float invLen = (float)((NS_FLOAT)1.0 / (NS_FLOAT)::sqrt(lenSq));
		v[0] *= invLen;
		v[1] *= invLen;
		v[2] *= invLen;
		return true;
	}
	static bool Math_quat_from_basis(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;

		float right[3], up[3], fwd[3];
		if (ReadVec3(pN->GetStackVar(1), right) == false) return false;
		if (ReadVec3(pN->GetStackVar(2), up) == false) return false;
		if (ReadVec3(pN->GetStackVar(3), fwd) == false) return false;
		if (NormalizeVec3(right) == false) return false;
		if (NormalizeVec3(up) == false) return false;
		if (NormalizeVec3(fwd) == false) return false;

		NS_FLOAT m00 = right[0], m01 = up[0], m02 = fwd[0];
		NS_FLOAT m10 = right[1], m11 = up[1], m12 = fwd[1];
		NS_FLOAT m20 = right[2], m21 = up[2], m22 = fwd[2];
		NS_FLOAT w, x, y, z;
		NS_FLOAT trace = m00 + m11 + m22;
		if (trace > (NS_FLOAT)0.0)
		{
			NS_FLOAT s = (NS_FLOAT)::sqrt(trace + (NS_FLOAT)1.0) * (NS_FLOAT)2.0;
			w = (NS_FLOAT)0.25 * s;
			x = (m21 - m12) / s;
			y = (m02 - m20) / s;
			z = (m10 - m01) / s;
		}
		else if (m00 > m11 && m00 > m22)
		{
			NS_FLOAT s = (NS_FLOAT)::sqrt((NS_FLOAT)1.0 + m00 - m11 - m22) * (NS_FLOAT)2.0;
			w = (m21 - m12) / s;
			x = (NS_FLOAT)0.25 * s;
			y = (m01 + m10) / s;
			z = (m02 + m20) / s;
		}
		else if (m11 > m22)
		{
			NS_FLOAT s = (NS_FLOAT)::sqrt((NS_FLOAT)1.0 + m11 - m00 - m22) * (NS_FLOAT)2.0;
			w = (m02 - m20) / s;
			x = (m01 + m10) / s;
			y = (NS_FLOAT)0.25 * s;
			z = (m12 + m21) / s;
		}
		else
		{
			NS_FLOAT s = (NS_FLOAT)::sqrt((NS_FLOAT)1.0 + m22 - m00 - m11) * (NS_FLOAT)2.0;
			w = (m10 - m01) / s;
			x = (m02 + m20) / s;
			y = (m12 + m21) / s;
			z = (NS_FLOAT)0.25 * s;
		}

		return WriteQuat(pN, (float)w, (float)x, (float)y, (float)z);
	}
	static bool Math_quat_slerp(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;

		float a[4], b[4];   // wxyz
		if (ReadQuat(pN->GetStackVar(1), a) == false) return false;
		if (ReadQuat(pN->GetStackVar(2), b) == false) return false;
		NS_FLOAT t = MathClamp01Value(pN->read<NS_FLOAT>(3));

		NS_FLOAT dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
		if (dot < (NS_FLOAT)0.0)
		{
			dot = -dot;
			b[0] = -b[0];
			b[1] = -b[1];
			b[2] = -b[2];
			b[3] = -b[3];
		}

		NS_FLOAT scaleA;
		NS_FLOAT scaleB;
		if (dot > (NS_FLOAT)0.9995)
		{
			scaleA = (NS_FLOAT)1.0 - t;
			scaleB = t;
		}
		else
		{
			NS_FLOAT theta = (NS_FLOAT)::acos(MathClampValue(dot, (NS_FLOAT)-1.0, (NS_FLOAT)1.0));
			NS_FLOAT sinTheta = (NS_FLOAT)::sin(theta);
			if (::fabs(sinTheta) < (NS_FLOAT)0.000001)
			{
				scaleA = (NS_FLOAT)1.0 - t;
				scaleB = t;
			}
			else
			{
				scaleA = (NS_FLOAT)::sin(((NS_FLOAT)1.0 - t) * theta) / sinTheta;
				scaleB = (NS_FLOAT)::sin(t * theta) / sinTheta;
			}
		}

		return WriteQuat(pN,
			(float)(a[0] * scaleA + b[0] * scaleB),
			(float)(a[1] * scaleA + b[1] * scaleB),
			(float)(a[2] * scaleA + b[2] * scaleB),
			(float)(a[3] * scaleA + b[3] * scaleB));
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
	// 실수를 왕복 가능한 문자열로 만든다.
	//
	// 보통의 문자열 변환(".." 연결 포함)은 "%g" 라 유효숫자 6자리에서 잘린다. 화면과 로그에는
	// 그게 읽기 좋지만, 데이터 파일로 내보내면 원래 값을 되찾을 수 없다. float 는 9자리를
	// 적어야 정확히 왕복하므로 여기서는 "%.9g" 를 쓴다.
	//
	// 필요할 때만 지수 표기로 넘어가고(1.5e-07), 그 형태도 JSON 숫자 문법에 맞으므로
	// 그대로 파일에 넣어도 된다.
	static bool	Math_ToStr(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		char ch[64];
		snprintf(ch, sizeof(ch), "%.9g", (double)pN->read<NS_FLOAT>(1));
		pN->ReturnValue(ch);
		return true;
	}
	static bool Math_Rand01(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 0) return false;
		pN->ReturnValue((NS_FLOAT)pN->m_sRand.rnd() / (NS_FLOAT)32767.0);
		return true;
	}
	static bool Math_RandRange(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 2) return false;
		NS_FLOAT a = pN->read<NS_FLOAT>(1);
		NS_FLOAT b = pN->read<NS_FLOAT>(2);
		NS_FLOAT t = (NS_FLOAT)pN->m_sRand.rnd() / (NS_FLOAT)32767.0;
		pN->ReturnValue(a + (b - a) * t);
		return true;
	}
	static bool Math_Hash32(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		// Keep the exact unsigned 32-bit overflow behavior used by native gameplay code.
		std::uint32_t value = static_cast<std::uint32_t>(pN->read<int>(1));
		value ^= value >> 16;
		value *= 0x7feb352du;
		value ^= value >> 15;
		value *= 0x846ca68bu;
		value ^= value >> 16;
		pN->ReturnValue(static_cast<int>(value));
		return true;
	}
	static bool Math_ColorRGB(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 3) return false;
		NS_FLOAT r = MathClamp01Value(pN->read<NS_FLOAT>(1));
		NS_FLOAT g = MathClamp01Value(pN->read<NS_FLOAT>(2));
		NS_FLOAT b = MathClamp01Value(pN->read<NS_FLOAT>(3));
		int color = -16777216 + (int)(b * (NS_FLOAT)255.0) * 65536 + (int)(g * (NS_FLOAT)255.0) * 256 + (int)(r * (NS_FLOAT)255.0);
		pN->ReturnValue(color);
		return true;
	}
	static bool Math_ColorARGB(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 4) return false;
		NS_FLOAT a = MathClamp01Value(pN->read<NS_FLOAT>(1));
		NS_FLOAT r = MathClamp01Value(pN->read<NS_FLOAT>(2));
		NS_FLOAT g = MathClamp01Value(pN->read<NS_FLOAT>(3));
		NS_FLOAT b = MathClamp01Value(pN->read<NS_FLOAT>(4));
		int ai = (int)(a * (NS_FLOAT)255.0);
		int rgb = (int)(b * (NS_FLOAT)255.0) * 65536 + (int)(g * (NS_FLOAT)255.0) * 256 + (int)(r * (NS_FLOAT)255.0);
		int color = ai < 128 ? ai * 16777216 + rgb : (ai - 256) * 16777216 + rgb;
		pN->ReturnValue(color);
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

		if (pFun->GetType() != VAR_FUN && pFun->GetType() != VAR_CLOSURE) return false;

		// Keep the receiver alive across script callbacks. The callback can replace
		// the variable that originally held this map.
		VarInfo mapHold;
		Move_DestNoRelease(&mapHold, pVar);
		VarInfo callbackHold;
		Move_DestNoRelease(&callbackHold, pFun);
		MapInfo* const table = mapHold._tbl;

		// [주의] _Bucket 내부 포인터를 스크립트 콜백 너머로 들고 있으면 안 된다.
		// 맵 노드가 연속 배열로 바뀌면서, 비교 함수 안의 삽입이 노드를 다른 슬롯으로
		// 옮기거나(InsertNewNode) 배열을 통째로 재할당(ReMap)할 수 있게 됐다.
		// 그래서 정렬 전에 값을 지역 벡터로 복사해 두고, 정렬이 끝난 뒤 슬롯을 다시 얻어 쓴다.
		std::vector<VarInfo*> slots;
		if (false == table->ToListValues(slots))
		{
			pN->Var_Release(&mapHold);
			pN->Var_Release(&callbackHold);
			return false;
		}

		if (slots.size() >= 2)
		{
			const u32 mutationVersion = table->_mutationVersion;
			// Move_DestNoRelease 는 참조계수를 올리는 복사다(원본은 그대로 둔다).
			std::vector<VarInfo> snapshot;
			snapshot.resize(slots.size());
			for (size_t i = 0; i < slots.size(); i++)
				Move_DestNoRelease(&snapshot[i], slots[i]);
			slots.clear();   // 콜백 전에 버린다

			std::vector<VarInfo*> sorted;
			sorted.resize(snapshot.size());
			for (size_t i = 0; i < snapshot.size(); i++)
				sorted[i] = &snapshot[i];
			NVM_QuickSort(pN, &callbackHold, sorted);      // 지역 벡터만 만진다

			// A structural mutation makes the previous ordering meaningless. Do not
			// overwrite values (including values inserted by the callback) with a
			// stale snapshot.
			if (table->_mutationVersion != mutationVersion)
			{
				for (size_t i = 0; i < snapshot.size(); i++)
					pN->Var_Release(&snapshot[i]);
				pN->Var_Release(&mapHold);
				pN->Var_Release(&callbackHold);
				pN->SetError("map was modified during sort");
				return true;
			}

			// No structural mutation: reacquire slots only after every callback has returned.
			std::vector<VarInfo*> slotsAfter;
			if (table->ToListValues(slotsAfter) && slotsAfter.size() == sorted.size())
			{
				for (size_t i = 0; i < sorted.size(); i++)
					pN->Move(slotsAfter[i], sorted[i]);
			}
			for (size_t i = 0; i < snapshot.size(); i++)
				pN->Var_Release(&snapshot[i]);
		}
		pN->Var_Release(&mapHold);
		pN->Var_Release(&callbackHold);
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
		bool mayContainContainerChild = false;
		for (int i = 0; i < (int)lst.size(); i++)
		{
			mayContainContainerChild |= lst[i]->IsContainerType();
			Move_DestNoRelease(&dest[i], lst[i]);
		}
		if (mayContainContainerChild)
			pR->MarkContainerChild();
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
		bool mayContainContainerChild = false;
		for (int i = 0; i < (int)lst.size(); i++)
		{
			mayContainContainerChild |= lst[i]->IsContainerType();
			Move_DestNoRelease(&dest[i], lst[i]);
		}
		if (mayContainContainerChild)
			pR->MarkContainerChild();
		return true;
	}

	static bool async_get(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pN->IsNativeScriptCallActive())
		{
			pN->SetErrorFormat(RTE_NESTED_NOT_ALLOWED, "async.get");
			return false;
		}
		if (args != 3) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		if(pAsync->_state != ASYNC_READY) return false;

		VarInfo* v1 = pN->GetStack(1);
		if (v1->GetType() != VAR_INT)
			return false;

		VarInfo* v2 = pN->GetStack(2);
		if (v2->GetType() != VAR_STRING)
			return false;

		VarInfo* v3 = pN->GetStack(3);
		if (v3->GetType() != VAR_FUN && v3->GetType() != VAR_CLOSURE)
			return false;

		pAsync->_type = ASYNC_GET;
		pAsync->_timeout = v1->_int;
		pAsync->_request = v2->_str->_str;
		pAsync->_fun_index = (v3->GetType() == VAR_FUN) ? v3->_fun_index : v3->_closure->_funIndex;
		pN->Move(&pAsync->_callback, v3);
		if (pAsync->_timeout == -1) pAsync->_timeout = 0x7fffffff;

		pAsync->_ownerWorkerId = pN->GetWorkerID();
		++pN->_asyncPendingCount;
		pAsync->_event.reset();
		pAsync->_state = ASYNC_PENDING;
		if (pVar->IsContainerType() || v3->IsContainerType()) pAsync->_cycleState._mayContainContainerChild = true;
		pN->Move(&pAsync->_LockReferance, pVar);
		pN->GetVM()->AddHttp_Request(pAsync);
		pN->ReturnValue();
		return true;
	}
	static bool async_post(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pN->IsNativeScriptCallActive())
		{
			pN->SetErrorFormat(RTE_NESTED_NOT_ALLOWED, "async.post");
			return false;
		}
		if (args != 4) return false;
		if (pVar->GetType() != VAR_ASYNC) return false;
		AsyncInfo* pAsync = pVar->_async;
		if (pAsync->_state != ASYNC_READY) return false;

		VarInfo* v1 = pN->GetStack(1);
		if (v1->GetType() != VAR_INT)
			return false;

		VarInfo* v2 = pN->GetStack(2);
		if (v2->GetType() != VAR_STRING)
			return false;

		VarInfo* v3 = pN->GetStack(3);
		if (v3->GetType() != VAR_STRING) // 
			return false;

		VarInfo* v4 = pN->GetStack(4);
		if (v4->GetType() != VAR_FUN && v4->GetType() != VAR_CLOSURE)
			return false;

		pAsync->_type = ASYNC_POST;
		pAsync->_timeout = v1->_int;
		pAsync->_request = v2->_str->_str;
		pAsync->_body = v3->_str->_str;
		pAsync->_fun_index = (v4->GetType() == VAR_FUN) ? v4->_fun_index : v4->_closure->_funIndex;
		pN->Move(&pAsync->_callback, v4);
		if (pAsync->_timeout == -1) pAsync->_timeout = 0x7fffffff;

		pAsync->_ownerWorkerId = pN->GetWorkerID();
		++pN->_asyncPendingCount;
		pAsync->_event.reset();
		pAsync->_state = ASYNC_PENDING;
		if (pVar->IsContainerType() || v4->IsContainerType()) pAsync->_cycleState._mayContainContainerChild = true;
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
		if (v1->GetType() != VAR_STRING)
			return false;

		VarInfo* v2 = pN->GetStack(2);
		if (v2->GetType() != VAR_STRING)
			return false;

		std::pair<std::string, std::string> header = { v1->_str->_str, v2->_str->_str };
		pAsync->_headers.push_back(header);
		pN->ReturnValue();
		return true;
	}
	static bool async_wait(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (pN->IsNativeScriptCallActive())
		{
			pN->SetErrorFormat(RTE_NESTED_NOT_ALLOWED, "async.wait");
			return false;
		}
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
			if(NeoVMSystem::m_pFunPrint == nullptr)
				std::cout << str.c_str() << '\n';
			else
				NeoVMSystem::m_pFunPrint(str.c_str());
			pN->ReturnValue();
			return true;
		}
		else if (args == 2)
		{
			VarInfo* pArg1 = pN->GetStack(1);
			VarInfo* pArg2 = pN->GetStack(2);
			std::string str1 = CNeoVMWorker::ToString(pArg1);
			std::string str2 = CNeoVMWorker::ToString(pArg2);
			if (NeoVMSystem::m_pFunPrint == nullptr)
				std::cout << str1.c_str() << str2.c_str();
			else
				NeoVMSystem::m_pFunPrint((str1 + str2).c_str());
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
	// system.load(source, name) — arg1 = 컴파일할 소스 텍스트, arg2 = 청크 이름(현재 미사용).
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

		if (false == NeoVMSystem::Compile(arCode, param))
		{
			return false;
		}

		INeoVMWorker* pModule = pN->_pVM->LoadVM(nullptr, arCode.GetData(), arCode.GetBufferOffset());
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

		std::vector<VarInfo> moduleArgs;
		if (pArg1->_module->ExecuteTop(0, moduleArgs) == NEOEXEC_ERROR)
			return false;

		pN->ReturnValue();
		return true;
	}
	static bool coroutine_create(CNeoVMWorker* pN, VarInfo* pVar, short args)
	{
		if (args != 1) return false;

		VarInfo* v = pN->GetStack(1);
		if (v->GetType() != VAR_FUN && v->GetType() != VAR_CLOSURE)
			return false;

		CNeoVM* pVM = pN->GetVM();
		CoroutineInfo* pCI = pVM->CoroutineAlloc();
		pCI->_refCount = 0;
		pCI->_fun_index = (v->GetType() == VAR_FUN) ? v->_fun_index : v->_closure->_funIndex;
		pN->Move(&pCI->_function, v);
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

		CNeoVM* pVM = pN->GetVM();
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
			pN->SetErrorFormat(RTE_SET_UNSUPPORTED, GetDataType(pArg1->GetType()).c_str());
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
static std::vector<TYPE_NeoLib> g_sNeoFunLib_DefaultNative;
static std::vector<u8> g_sNeoFunLib_DefaultIntrinsic; // native index 별 intrinsic opcode (기본 NOP_NONE)
static std::unordered_map<std::string, int> g_sNeoFunLib_DefaultNativeIndex;

bool CNeoVM::_funInitLib = false;
FunctionPtrNative CNeoVM::_funLib_Default;
FunctionPtrNative CNeoVM::_funLib_List;
FunctionPtrNative CNeoVM::_funLib_String;
FunctionPtrNative CNeoVM::_funLib_Map;
FunctionPtrNative CNeoVM::_funLib_Async;


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

static int AddDefaultNativeFun(const std::string& nativeName, TYPE_NeoLib fun)
{
	auto it = g_sNeoFunLib_DefaultNativeIndex.find(nativeName);
	if (it != g_sNeoFunLib_DefaultNativeIndex.end())
		return it->second;

	int nativeIndex = (int)g_sNeoFunLib_DefaultNative.size();
	g_sNeoFunLib_DefaultNative.push_back(fun);
	g_sNeoFunLib_DefaultIntrinsic.push_back(NOP_NONE);
	g_sNeoFunLib_DefaultNativeIndex[nativeName] = nativeIndex;
	g_sNeoFunLib_Default.Add(nativeName, fun);
	return nativeIndex;
}
// 특정 native 함수를 로드 시 전용 opcode 로 대체하도록 표시 (문자열 하드코딩 대신 등록부에 명시).
static void SetSystemFunIntrinsic(const std::string& fname, eNOperation op)
{
	auto it = g_sNeoFunLib_DefaultNativeIndex.find("#" + fname);
	if (it != g_sNeoFunLib_DefaultNativeIndex.end())
		g_sNeoFunLib_DefaultIntrinsic[it->second] = (u8)op;
}

// 첫 항목 = 리턴 타입(필수, 없으면 "void"), 이후 파라미터를 "타입 이름" 문자열로 하나씩 나열.
// 인자 수는 파라미터 나열 개수에서 유도된다(별도 count 없음).
// 마지막에 "..." 을 넣으면 가변 인자(컴파일 타임 인자 수 검사 안 함 = argCount -1).
// 예) AddSystemFun("pow", &Math_pow, "float", "float base", "float exp");
//     AddSystemFun("rand", &Math_rand, "int");                                   // 인자 0개
//     AddSystemFun("resume", &coroutine_resume, "var", "coroutine co", "...");   // 가변
static void AddSystemFunImpl(const std::string& fname, TYPE_NeoLib fun, std::initializer_list<const char*> retAndParams)
{
	const std::string nativeName = "#" + fname;
	int nativeIndex = AddDefaultNativeFun(nativeName, fun);
	if (g_sCurrentSystem.empty())
		return;

	SystemFun v;
	v.fname = fname;
	v.nativeIndex = nativeIndex;
	bool variadic = false;
	bool first = true;
	for (const char* p : retAndParams)
	{
		if (first)
		{
			v.ret = (p != nullptr) ? p : "void";   // 첫 항목 = 리턴 타입
			first = false;
			continue;
		}
		if (p != nullptr && strcmp(p, "...") == 0)
			variadic = true;                 // 표시용으로도 넣고, 인자 수 검사는 끈다
		v.params.push_back(p);
	}
	if (v.ret.empty())
		v.ret = "void";
	v.argCount = variadic ? -1 : (int)v.params.size();

	auto it = g_sSystemFuns.find(g_sCurrentSystem);
	if (it == g_sSystemFuns.end())
	{
		std::list<SystemFun> lst;
		lst.push_back(v);
		g_sSystemFuns[g_sCurrentSystem] = lst;
	}
	else
	{
		(*it).second.push_back(v);
	}
}
template<typename... TParams>
static void AddSystemFun(const std::string& fname, TYPE_NeoLib fun, const char* ret, TParams... params)
{
	AddSystemFunImpl(fname, fun, { ret, params... });
}

int CNeoVM::FindDefaultNativeIndex(const VMString* pStr)
{
	if (pStr == nullptr)
		return -1;

	TYPE_NeoLib f;
	if (g_sNeoFunLib_Default.TryGetValue(pStr, &f) == false)
		return -1;

	auto it = g_sNeoFunLib_DefaultNativeIndex.find(pStr->_str);
	if (it == g_sNeoFunLib_DefaultNativeIndex.end())
		return -1;
	return it->second;
}

int CNeoVM::FindDefaultNativeIndex(const std::string& name)
{
	auto it = g_sNeoFunLib_DefaultNativeIndex.find(name);
	if (it == g_sNeoFunLib_DefaultNativeIndex.end())
		return -1;
	return it->second;
}

bool CNeoVM::CallDefaultNativeByIndex(int nativeIndex, CNeoVMWorker* pWorker, short args)
{
	if (nativeIndex < 0 || nativeIndex >= (int)g_sNeoFunLib_DefaultNative.size())
		return false;
	return (*g_sNeoFunLib_DefaultNative[nativeIndex])(pWorker, nullptr, args);
}
// 로드 시 PTRCALL2 를 대체할 intrinsic opcode. 없으면 NOP_NONE.
int CNeoVM::GetDefaultNativeIntrinsic(int nativeIndex)
{
	if (nativeIndex < 0 || nativeIndex >= (int)g_sNeoFunLib_DefaultIntrinsic.size())
		return NOP_NONE;
	return g_sNeoFunLib_DefaultIntrinsic[nativeIndex];
}

static void AddGlobalLibFun()
{
	if (g_sNeoFunLib_Default.empty() == false)
		return;

	AddDefaultNativeFun("print", &neo_libs::io_print);

	g_sCurrentSystem = "math";
	AddSystemFun("abs", &neo_libs::Math_abs, "float", "float x");
	AddSystemFun("acos", &neo_libs::Math_acos, "float", "float x");
	AddSystemFun("asin", &neo_libs::Math_asin, "float", "float x");
	AddSystemFun("atan", &neo_libs::Math_atan, "float", "float x");
	AddSystemFun("ceil", &neo_libs::Math_ceil, "float", "float x");
	AddSystemFun("floor", &neo_libs::Math_floor, "float", "float x");
	AddSystemFun("round", &neo_libs::Math_round, "float", "float x");
	AddSystemFun("sin", &neo_libs::Math_sin, "float", "float radian");
	AddSystemFun("cos", &neo_libs::Math_cos, "float", "float radian");
	AddSystemFun("tan", &neo_libs::Math_tan, "float", "float radian");
	AddSystemFun("log", &neo_libs::Math_log, "float", "float x");
	AddSystemFun("log10", &neo_libs::Math_log10, "float", "float x");
	AddSystemFun("exp", &neo_libs::Math_exp, "float", "float x");
	AddSystemFun("pow", &neo_libs::Math_pow, "float", "float base", "float exp");
	AddSystemFun("deg", &neo_libs::Math_deg, "float", "float radian");
	AddSystemFun("rad", &neo_libs::Math_rad, "float", "float degree");
	AddSystemFun("sqrt", &neo_libs::Math_sqrt, "float", "float x");
	AddSystemFun("Vector2", &neo_libs::Math_Vector2, "Vector2", "float x", "float y");
	AddSystemFun("Vector3", &neo_libs::Math_Vector3, "Vector3", "float x", "float y", "float z");
	AddSystemFun("Vector4", &neo_libs::Math_Vector4, "Vector4", "float x", "float y", "float z", "float w");
	AddSystemFun("Quaternion", &neo_libs::Math_Quaternion, "Quaternion", "float w", "float x", "float y", "float z");
	// 벡터 생성자는 LoadVM 에서 전용 opcode 로 대체 (native 호출 오버헤드 제거). native 구현은 폴백으로 유지.
	// 넷 다 같은 op. 성분 수는 호출 인자 수(n2)에서 나온다.
	SetSystemFunIntrinsic("Vector2", NOP_VEC_MAKE);
	SetSystemFunIntrinsic("Vector3", NOP_VEC_MAKE);
	SetSystemFunIntrinsic("Vector4", NOP_VEC_MAKE);
	SetSystemFunIntrinsic("Quaternion", NOP_VEC_MAKE);
	AddSystemFun("Clamp01", &neo_libs::Math_Clamp01, "float", "float x");
	AddSystemFun("Clamp", &neo_libs::Math_Clamp, "float", "float x", "float min", "float max");
	AddSystemFun("SmoothStep01", &neo_libs::Math_SmoothStep01, "float", "float t");
	AddSystemFun("Lerp", &neo_libs::Math_Lerp, "float", "float a", "float b", "float t");
	AddSystemFun("Lerp3", &neo_libs::Math_Lerp3, "Vector3", "Vector3 a", "Vector3 b", "float t");
	AddSystemFun("DistanceSquared3", &neo_libs::Math_DistanceSquared3, "float", "Vector3 a", "Vector3 b");
	AddSystemFun("Normalize3", &neo_libs::Math_Normalize3, "Vector3", "Vector3 v", "Vector3 fallback");
	AddSystemFun("Cross3", &neo_libs::Math_Cross3, "Vector3", "Vector3 a", "Vector3 b");
	AddSystemFun("RotateVectorByQuat", &neo_libs::Math_RotateVectorByQuat, "Vector3", "Quaternion quat", "Vector3 v");
	AddSystemFun("quat_from_basis", &neo_libs::Math_quat_from_basis, "Quaternion", "Vector3 right", "Vector3 up", "Vector3 forward");
	AddSystemFun("quat_slerp", &neo_libs::Math_quat_slerp, "Quaternion", "Quaternion a", "Quaternion b", "float t");
	AddSystemFun("srand", &neo_libs::Math_srand, "void", "int seed");
	AddSystemFun("rand", &neo_libs::Math_rand, "int");
	AddSystemFun("tostr", &neo_libs::Math_ToStr, "string", "float value");
	AddSystemFun("Rand01", &neo_libs::Math_Rand01, "float");
	AddSystemFun("RandRange", &neo_libs::Math_RandRange, "float", "float min", "float max");
	AddSystemFun("Hash32", &neo_libs::Math_Hash32, "int", "int value");
	AddSystemFun("ColorRGB", &neo_libs::Math_ColorRGB, "int", "int r", "int g", "int b");
	AddSystemFun("ColorARGB", &neo_libs::Math_ColorARGB, "int", "int a", "int r", "int g", "int b");

	g_sCurrentSystem = "system";
	AddSystemFun("time", &neo_libs::sys_time, "int");
	AddSystemFun("date", &neo_libs::sys_date, "string", "string format", "int time");
	AddSystemFun("clock", &neo_libs::sys_clock, "float");
	// 인자 순서는 sys_load 구현 기준: arg1=컴파일할 소스, arg2=청크 이름.
	// name 은 현재 타입 검사만 하고 사용하지 않는다(진단 메시지/모듈 식별용 예약).
	AddSystemFun("load", &neo_libs::sys_load, "module", "string source", "string name");
	AddSystemFun("pcall", &neo_libs::sys_pcall, "void", "module m");
	AddSystemFun("aysnc_create", &neo_libs::sys_aysnc_create, "async");
	AddSystemFun("set", &neo_libs::sys_set, "set", "list l");


	g_sCurrentSystem = "coroutine";
	AddSystemFun("create", &neo_libs::coroutine_create, "coroutine", "fun f");
	AddSystemFun("resume", &neo_libs::coroutine_resume, "var", "coroutine co", "...");
	AddSystemFun("status", &neo_libs::coroutine_status, "string", "coroutine co");
	AddSystemFun("close", &neo_libs::coroutine_close, "coroutine", "...");

	g_sCurrentSystem.clear();
}
bool CNeoVM::IsGlobalLibFun(std::string& FunName)
{
	//InitLib();
	//return g_sNeoFunLib_Default.IsKey(FunName);
	return FunName == "print";
}
const std::list< SystemFun>* CNeoVM::GetSystemModule(const std::string& module)
{
	auto it = g_sSystemFuns.find(module);
	if(it == g_sSystemFuns.end())
		return nullptr;
	return &(*it).second;
}
void CNeoVM::RegLibrary(VarInfo* pSystem, const char* pLibName)
{
	if (pSystem && pSystem->GetType() == VAR_FP_NATIVE)
		NeoVMSystem::RegisterTableCallBack(pSystem, nullptr, Fun_Default, nullptr);
	//AddGlobalLibFun();

	//_funDefaultLib = CNeoVM::RegisterNative(Fun);
}

void CNeoVM::RegObjLibrary()
{
	if (_funInitLib) return;
	_funInitLib = true;

	AddGlobalLibFun();
	_funLib_Default = NeoVMSystem::RegisterNative(Fun_Default);

	// String Lib
	_funLib_String = NeoVMSystem::RegisterNative(Fun_String);
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
	_funLib_List = NeoVMSystem::RegisterNative(Fun_List);
	g_sNeoFunLib_List.Add("resize", &neo_libs::List_resize);
	g_sNeoFunLib_List.Add("len", &neo_libs::List_len);
	g_sNeoFunLib_List.Add("append", &neo_libs::List_append);

	// Map Lib
	_funLib_Map = NeoVMSystem::RegisterNative(Fun_Map);
	g_sNeoFunLib_Map.Add("len", &neo_libs::map_len);
	g_sNeoFunLib_Map.Add("reserve", &neo_libs::map_reserve);
	g_sNeoFunLib_Map.Add("sort", &neo_libs::map_sort);
	g_sNeoFunLib_Map.Add("keys", &neo_libs::map_keys);
	g_sNeoFunLib_Map.Add("values", &neo_libs::map_values);

	// Async Lib
	_funLib_Async = NeoVMSystem::RegisterNative(Fun_Async);
	g_sNeoFunLib_Async.Add("get", &neo_libs::async_get);
	g_sNeoFunLib_Async.Add("post", &neo_libs::async_post);
	g_sNeoFunLib_Async.Add("add_header", &neo_libs::async_add_header);
	g_sNeoFunLib_Async.Add("wait", &neo_libs::async_wait);
	g_sNeoFunLib_Async.Add("close", &neo_libs::async_close);
}

void CNeoVM::InitLib()
{
/*	VarInfo* pSystem = &m_sVarGlobal[0];
	Var_SetTable(pSystem, TableAlloc());

	RegLibrary(pSystem, "sys");*/
	RegObjLibrary();
}

// 등록된 빌트인 전체 열거 — 완성 데이터의 단일 진실원천은 위 등록 코드 그 자체.
// AddSystemFun / g_sNeoFunLib_*.Add 한 줄만 추가하면 여기 열거에 자동 반영된다.
void NeoVMSystem::GetBuiltins(std::vector<NeoBuiltinInfo>& out)
{
	out.clear();

	// 1) namespaced module 함수 (math / system / coroutine …) — argCount/타입+이름 파라미터 포함
	for (auto& mod : g_sSystemFuns)
	{
		for (auto& f : mod.second)
		{
			NeoBuiltinInfo info;
			info.module = mod.first;
			info.name = f.fname;
			info.argCount = f.argCount;
			info.ret = f.ret;         // 리턴 타입
			info.params = f.params;   // "float x" 목록 그대로
			out.push_back(info);
		}
	}

	// 2) 타입 메서드 테이블 (string / list / map / async) — 이름만(argCount 미상)
	auto emitTable = [&out](const char* module, VMHash<TYPE_NeoLib>& tbl)
	{
		tbl.Enumerate([&out, module](const std::string& key, TYPE_NeoLib)
		{
			NeoBuiltinInfo info;
			info.module = module;
			info.name = key;
			info.argCount = -1;
			out.push_back(info);
		});
	};
	emitTable("string", g_sNeoFunLib_String);
	emitTable("list", g_sNeoFunLib_List);
	emitTable("map", g_sNeoFunLib_Map);
	emitTable("async", g_sNeoFunLib_Async);
}

};
