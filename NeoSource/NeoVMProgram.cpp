#include <limits.h>

#include "NeoVMProgram.h"
#include "NeoVMImpl.h"
#include "NeoArchive.h"

namespace NeoScript
{

extern u32 GetHashCode(u8* buffer, int len);

// ---- switch/case 테이블 ----
// 런타임 key(StringInfo)의 해시와 같은 알고리즘이어야 조회가 일치한다.
static u32 SwitchKeyHash(const ProgramSwitchKey& k)
{
	switch (k._type)
	{
	case VAR_BOOL:   return k._bl ? 1u : 0u;
	case VAR_INT:    return (u32)k._int;
	case VAR_STRING: return GetHashCode(k._str);
	default:         return 0u;
	}
}

void ProgramSwitchTable::Build()
{
	buckets.clear();
	if (entries.empty())
		return;

	int capa = 8;
	while (capa < (int)entries.size() * 2)
		capa <<= 1;
	buckets.assign(capa, -1);

	const u32 mask = (u32)(capa - 1);
	for (int i = 0; i < (int)entries.size(); ++i)
	{
		u32 b = SwitchKeyHash(entries[i].key) & mask;
		entries[i].next = buckets[b];
		buckets[b] = i;
	}
}

int ProgramSwitchTable::Find(VarInfo* pKey) const
{
	if (buckets.empty() || pKey == nullptr)
		return defaultOffset;

	// strict type match. case key 로 쓸 수 없는 타입(float/map/list/vector/null 등)은 default 로.
	VAR_TYPE t = pKey->GetType();
	u32 h;
	switch (t)
	{
	case VAR_BOOL:   h = pKey->_bl ? 1u : 0u; break;
	case VAR_INT:    h = (u32)pKey->_int; break;
	case VAR_STRING: h = pKey->_str->GetHash(); break;
	default: return defaultOffset;
	}

	for (int i = buckets[h & (u32)(buckets.size() - 1)]; i >= 0; i = entries[i].next)
	{
		const ProgramSwitchEntry& e = entries[i];
		if (e.key._type != t)
			continue;
		switch (t)
		{
		case VAR_BOOL:   if (e.key._bl == pKey->_bl) return e.jumpOffset; break;
		case VAR_INT:    if (e.key._int == pKey->_int) return e.jumpOffset; break;
		case VAR_STRING: if (e.key._str == pKey->_str->_str) return e.jumpOffset; break; // UTF-8 바이트 일치
		default: break;
		}
	}
	return defaultOffset;
}

static bool ReadString(CNArchive& ar, std::string& str)
{
	short nLen = 0;
	if (ar.Read(&nLen, sizeof(nLen)) == 0)
		return false;
	if (nLen < 0)
		return false;

	str.resize(nLen);
	if (nLen == 0)
		return true;
	return ar.Read((char*)str.data(), nLen) != 0;
}
static bool ReadCount(CNArchive& ar, u32& out)
{
	u16 wCount = 0;
	if (ar.Read(&wCount, sizeof(wCount)) == 0)
		return false;
	if (wCount < 0xFFFF)
	{
		out = wCount;
		return true;
	}
	u32 dwCount = 0;
	if (ar.Read(&dwCount, sizeof(dwCount)) == 0)
		return false;
	out = dwCount;
	return true;
}

static void SetLoadError(std::string* err, const char* msg)
{
	if (err != nullptr)
		*err = msg;
}

CNeoVMProgram* CNeoVMProgram::Create(const void* pBuffer, int iSize, std::string* err)
{
	if (pBuffer == nullptr || iSize <= (int)sizeof(SNeoVMHeader))
	{
		SetLoadError(err, "Invalid script image");
		return nullptr;
	}

	// 코드 패치가 전역 native 등록부(InitLib)에 의존한다. Initialize() 보다 먼저
	// 프로그램을 만들어도 패치가 누락되지 않도록 여기서 보장한다(재진입 무해).
	CNeoVMImpl::InitLib();

	CNArchive ar(const_cast<void*>(pBuffer), iSize);
	CNeoVMProgram* p = new CNeoVMProgram();
	p->byteSize = iSize;
	if (false == p->Load(ar, err))
	{
		p->Release();
		return nullptr;
	}
	p->PatchNativeCalls();
	return p;
}

bool CNeoVMProgram::Load(CNArchive& ar, std::string* err)
{
	memset(&header, 0, sizeof(header));
	if (ar.Read(&header, sizeof(header)) == 0)
	{
		SetLoadError(err, "Truncated script image (header)");
		return false;
	}

	if (header._dwFileType != FILE_NEOS)
	{
		SetLoadError(err, "Not a NeoScript image");
		return false;
	}
	if (header._dwNeoVersion != NEO_VER)
	{
		SetLoadError(err, "NeoScript image version mismatch");
		return false;
	}

	bool IsDataSinglePrecision = (header._dwFlag & NEO_HEADER_FLAG_SINGLE_PRECISION) ? true : false;
	if (IsDataSinglePrecision != INeoVM::IsSinglePrecision())
	{
		SetLoadError(err, "NeoScript image float precision mismatch");
		return false;
	}

	if (header._iCodeSize < 0 || (header._iCodeSize % (int)sizeof(SVMOperation)) != 0)
	{
		SetLoadError(err, "Invalid code size");
		return false;
	}
	if (header._iFunctionCount < 0 || header._iStaticVarCount < 0 ||
		header._iGlobalVarCount < 0 || header._iExportVarCount < 0)
	{
		SetLoadError(err, "Invalid script image counts");
		return false;
	}

	code.resize(header._iCodeSize / sizeof(SVMOperation));
	if (header._iCodeSize > 0 && ar.Read(code.data(), header._iCodeSize) == 0)
	{
		SetLoadError(err, "Truncated script image (code)");
		return false;
	}

	functions.resize(header._iFunctionCount);
	std::string name;
	for (int i = 0; i < header._iFunctionCount; i++)
	{
		int iID = -1;
		SFunctionTable fun;
		memset(&fun, 0, sizeof(SFunctionTable));

		if (ar.Read(&iID, sizeof(iID)) == 0)
		{
			SetLoadError(err, "Truncated script image (function table)");
			return false;
		}
		ar >> fun._codePtr >> fun._argsCount >> fun._localTempMax >> fun._localVarCount >> fun._funType;
		if (iID < 0 || iID >= header._iFunctionCount)
		{
			SetLoadError(err, "Invalid function id");
			return false;
		}
		if (fun._funType != FUNT_NORMAL && fun._funType != FUNT_ANONYMOUS)
		{
			if (false == ReadString(ar, name))
			{
				SetLoadError(err, "Truncated script image (function name)");
				return false;
			}
			exportFunctions[name] = iID;
		}

		fun._localAddCount = 1 + fun._argsCount + fun._localVarCount + fun._localTempMax;
		functions[iID] = fun;
	}

	for (int i = 0; i < header._iExportVarCount; i++)
	{
		int idx = -1;
		if (ar.Read(&idx, sizeof(idx)) == 0)
		{
			SetLoadError(err, "Truncated script image (export vars)");
			return false;
		}
		if (false == ReadString(ar, name))
		{
			SetLoadError(err, "Truncated script image (export var name)");
			return false;
		}
		exportVariables[name] = idx;
	}

	staticValues.resize(header._iStaticVarCount);
	for (int i = 0; i < header._iStaticVarCount; i++)
	{
		SStaticConst& sc = staticValues[i];
		VAR_TYPE type = VAR_NONE;
		if (ar.Read(&type, sizeof(type)) == 0)
		{
			SetLoadError(err, "Truncated script image (static values)");
			return false;
		}
		sc._type = type;
		switch (type)
		{
		case VAR_INT:
			ar >> sc._int;
			break;
		case VAR_FLOAT:
			ar >> sc._float;
			break;
		case VAR_BOOL:
			ar >> sc._bl;
			break;
		case VAR_STRING:
			if (false == ReadString(ar, sc._str))
			{
				SetLoadError(err, "Truncated script image (static string)");
				return false;
			}
			break;
		default:
			SetLoadError(err, "Error Invalid VAR Type");
			return false;
		}
	}

	if (header.m_iDebugCount > 0)
	{
		debugData.resize(header.m_iDebugCount);
		if (ar.Read(&debugData[0], sizeof(debug_info) * header.m_iDebugCount) == 0)
		{
			SetLoadError(err, "Truncated script image (debug info)");
			return false;
		}
	}

	// 선택 청크: 디버그 변수명(BVDN) / 함수명(NFDN). 미지의 magic 을 만나면 되돌리고 종료.
	while (ar.GetBufferOffset() + (int)sizeof(u32) <= ar.GetBufferSize())
	{
		u32 magic = 0;
		int oldOffset = ar.GetBufferOffset();
		ar >> magic;
		if (magic == 0x4E445642)
		{
			u32 funCount = 0;
			if (false == ReadCount(ar, funCount))
				return true;
			for (u32 i = 0; i < funCount; ++i)
			{
				int funId = -1;
				if (ar.Read(&funId, sizeof(funId)) == 0)
					return true;
				u32 nameCount = 0;
				if (false == ReadCount(ar, nameCount))
					return true;
				std::map<int, std::string>& names = (funId == -1) ? debugGlobalNames : debugVarNames[funId];
				for (u32 n = 0; n < nameCount; ++n)
				{
					int slot = -1;
					if (ar.Read(&slot, sizeof(slot)) == 0)
						return true;
					if (false == ReadString(ar, name))
						return true;
					names[slot] = name;
				}
			}
		}
		else if (magic == 0x4E44464E)
		{
			u32 funCount = 0;
			if (false == ReadCount(ar, funCount))
				return true;
			for (u32 i = 0; i < funCount; ++i)
			{
				int funId = -1;
				if (ar.Read(&funId, sizeof(funId)) == 0)
					return true;
				if (false == ReadString(ar, name))
					return true;
				debugFunctionNames[funId] = name;
			}
		}
		else if (magic == 0x54495753)   // 'SWIT' : switch/case 테이블
		{
			u32 tableCount = 0;
			if (false == ReadCount(ar, tableCount))
				return true;
			switchTables.resize(tableCount);
			for (u32 t = 0; t < tableCount; ++t)
			{
				ProgramSwitchTable& tbl = switchTables[t];
				if (ar.Read(&tbl.defaultOffset, sizeof(tbl.defaultOffset)) == 0)
					return true;
				u32 caseCount = 0;
				if (false == ReadCount(ar, caseCount))
					return true;
				tbl.entries.resize(caseCount);
				for (u32 c = 0; c < caseCount; ++c)
				{
					ProgramSwitchEntry& e = tbl.entries[c];
					u8 keyType = 0;
					if (ar.Read(&keyType, sizeof(keyType)) == 0)
						return true;
					e.key._type = (VAR_TYPE)keyType;
					switch (e.key._type)
					{
					case VAR_BOOL:
						if (ar.Read(&e.key._bl, sizeof(e.key._bl)) == 0) return true;
						break;
					case VAR_INT:
						if (ar.Read(&e.key._int, sizeof(e.key._int)) == 0) return true;
						break;
					case VAR_STRING:
						if (false == ReadString(ar, e.key._str)) return true;
						break;
					default:
						return true;   // 손상된 이미지
					}
					if (ar.Read(&e.jumpOffset, sizeof(e.jumpOffset)) == 0)
						return true;
				}
				tbl.Build();
			}
		}
		else
		{
			ar.SetBufferOffset(oldOffset);
			break;
		}
	}
	return true;
}

void CNeoVMProgram::PatchNativeCalls()
{
	const int staticCount = (int)staticValues.size();
	const int opCount = (int)code.size();
	for (int i = 0; i < opCount; ++i)
	{
		SVMOperation& op = code[i];
		if (op.op != NOP_PTRCALL2)
			continue;

		if (op.argFlag & NEOS_ARG_N1_LOCAL)
			continue;

		// n1 은 전역 슬롯 인덱스. native 이름 상수는 반드시 static 구간에 있다.
		if (op.n1 < 0 || op.n1 >= staticCount)
			continue;

		const SStaticConst& sc = staticValues[op.n1];
		if (sc._type != VAR_STRING)
			continue;

		int nativeIndex = CNeoVMImpl::FindDefaultNativeIndex(sc._str);
		if (nativeIndex < 0 || nativeIndex > SHRT_MAX)
			continue;

		// intrinsic 이 지정된 native(벡터 생성 등)는 전용 opcode 로 패치 (n2=인자수, n3=결과 유지).
		eNOperation intrinsic = (eNOperation)CNeoVMImpl::GetDefaultNativeIntrinsic(nativeIndex);
		if (intrinsic != NOP_NONE)
		{
			op.op = intrinsic;
			continue;
		}

		op.op = NOP_NATIVECALL;
		op.n1 = (short)nativeIndex;
	}
}

CNeoVMProgram* INeoVM::CreateProgram(const void* pBuffer, int iSize, std::string* err)
{
	return CNeoVMProgram::Create(pBuffer, iSize, err);
}

void INeoVM::ProgramAddRef(CNeoVMProgram* pProgram)
{
	if (pProgram != nullptr)
		pProgram->AddRef();
}

void INeoVM::ProgramRelease(CNeoVMProgram* pProgram)
{
	if (pProgram != nullptr)
		pProgram->Release();
}

CNeoVMProgram* INeoVM::CompileToProgram(const NeoCompilerParam& param)
{
	CNArchive ar;
	if (INeoVM::Compile(ar, param) == false)
		return nullptr;

	return CNeoVMProgram::Create(ar.GetData(), ar.GetBufferOffset(), param.err);
}

};
