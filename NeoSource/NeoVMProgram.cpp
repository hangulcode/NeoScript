#include <limits.h>

#include "NeoVMProgram.h"
#include "NeoVMImpl.h"
#include "NeoArchive.h"
#include "NeoTextLoader.h"   // FormatAsm / FormatBytes
#include <algorithm>

namespace NeoScript
{

extern u32 GetHashCode(u8* buffer, int len);
extern std::string GetDataType(VAR_TYPE t);   // NeoVMWorker.cpp

// ---- switch/case 테이블 ----
// 런타임 key(StringInfo)의 해시와 같은 알고리즘이어야 조회가 일치한다.
static u32 SwitchKeyHash(const ProgramSwitchKey& k)
{
	switch (k._type)
	{
	case VAR_INT:    return (u32)k._int;
	case VAR_BOOL:   return k._bl ? 1u : 0u;
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
	case VAR_INT:    h = (u32)pKey->_int; break;
	case VAR_BOOL:   h = pKey->_bl ? 1u : 0u; break;
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
		case VAR_INT:    if (e.key._int == pKey->_int) return e.jumpOffset; break;
		case VAR_BOOL:   if (e.key._bl == pKey->_bl) return e.jumpOffset; break;
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
	// NATIVECALL/intrinsic 치환 뒤에 돌린다 — PTRCALL2 가 먼저 확정되어야 한다.
	if (p->PatchLocalOps(err) == false)
	{
		p->Release();
		return nullptr;
	}
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
					case VAR_INT:
						if (ar.Read(&e.key._int, sizeof(e.key._int)) == 0) return true;
						break;
					case VAR_BOOL:
						if (ar.Read(&e.key._bl, sizeof(e.key._bl)) == 0) return true;
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

// 오퍼랜드를 전부 로컬 스택에서 뽑는 op 를 _L 변형으로 바꾼다.
// 판정 기준은 "VM 이 실제로 GetVarPtr* 로 페치하는 슬롯"이며, 점프 offset / 인자 수 /
// 함수 인덱스 / 즉값처럼 변수 참조가 아닌 필드는 검사 대상이 아니다.
// NOP_JMP(페치 0개)와 NOP_JMP_FOR(이미 GetVarPtr_L 고정)은 _L 이 원본과 동일해 치환하지 않는다.
bool CNeoVMProgram::PatchLocalOps(std::string* err)
{
	const u8 L1 = NEOS_ARG_N1_LOCAL, L2 = NEOS_ARG_N2_LOCAL, L3 = NEOS_ARG_N3_LOCAL;
	const int opCount = (int)code.size();
	int patched = 0;
	for (int i = 0; i < opCount; ++i)
	{
		SVMOperation& op = code[i];
		const u8 f = op.argFlag;
		const bool noResult = (f & NEOS_OP_CALL_NORESULT) != 0;
		eNOperation to = NOP_NONE;
		bool candidate = true;

		switch (op.op)
		{
		// n1, n2
		case NOP_MOV:          if ((f & (L1|L2)) == (L1|L2)) to = NOP_MOV_L;        break;
		case NOP_MOV_MINUS:    if ((f & (L1|L2)) == (L1|L2)) to = NOP_MOV_MINUS_L;  break;
		case NOP_ADD2:         if ((f & (L1|L2)) == (L1|L2)) to = NOP_ADD2_L;       break;
		case NOP_TOINT:        if ((f & (L1|L2)) == (L1|L2)) to = NOP_TOINT_L;      break;
		// n1 만
		case NOP_MOVI:         if (f & L1) to = NOP_MOVI_L;                         break;
		case NOP_MOVF:
			if (NEOS_CAN_EMBED_FLOAT_IMMEDIATE == false)
			{
				SetLoadError(err, "NOP_MOVF requires 4-byte NS_FLOAT");
				return false;
			}
			if (f & L1) to = NOP_MOVF_L;
			break;
		case NOP_MOVF_L:
			if (NEOS_CAN_EMBED_FLOAT_IMMEDIATE == false)
			{
				SetLoadError(err, "NOP_MOVF requires 4-byte NS_FLOAT");
				return false;
			}
			break;
		case NOP_LIST_ALLOC:   if (f & L1) to = NOP_LIST_ALLOC_L;                   break;
		case NOP_CHANGE_INT:   if (f & L1) to = NOP_CHANGE_INT_L;                   break;
		// n1, n2, n3
		case NOP_ADD3:         if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_ADD3_L;      break;
		case NOP_SUB3:         if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_SUB3_L;      break;
		case NOP_MUL3:         if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_MUL3_L;      break;
		case NOP_AND:          if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_AND_L;       break;
		case NOP_CLT_READ:     if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_CLT_READ_L;  break;
		case NOP_CLT_MOV:      if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_CLT_MOV_L;   break;
		// n2, n3 (n1 은 점프 offset)
		case NOP_JMP_LESS:     if ((f & (L2|L3)) == (L2|L3)) to = NOP_JMP_LESS_L;     break;
		case NOP_JMP_LESS_EQ:  if ((f & (L2|L3)) == (L2|L3)) to = NOP_JMP_LESS_EQ_L;  break;
		case NOP_JMP_GREAT:    if ((f & (L2|L3)) == (L2|L3)) to = NOP_JMP_GREAT_L;    break;
		case NOP_JMP_GREAT_EQ: if ((f & (L2|L3)) == (L2|L3)) to = NOP_JMP_GREAT_EQ_L; break;
		case NOP_JMP_EQUAL2:   if ((f & (L2|L3)) == (L2|L3)) to = NOP_JMP_EQUAL2_L;   break;
		case NOP_JMP_NEQUAL:   if ((f & (L2|L3)) == (L2|L3)) to = NOP_JMP_NEQUAL_L;   break;
		// n1, n2, n3
		case NOP_STR_ADD:      if ((f & (L1|L2|L3)) == (L1|L2|L3)) to = NOP_STR_ADD_L; break;
		// 호출/복귀 — NORESULT 면 슬롯을 아예 안 뽑으므로 바꿔봐야 이득이 없다.
		case NOP_CALL:         if (!noResult && (f & L3)) to = NOP_CALL_L;           break;
		case NOP_RETURN:       if (!noResult && (f & L1)) to = NOP_RETURN_L;         break;
		case NOP_PTRCALL:      if ((f & (L1|L2)) == (L1|L2)) to = NOP_PTRCALL_L;     break;
		case NOP_PTRCALL2:
			if ((f & L1) && (noResult || (f & L3))) to = NOP_PTRCALL2_L;
			break;
		default:
			candidate = false;
			break;
		}

		if (to != NOP_NONE)
		{
			op.op = to;
			++patched;
		}
	}
	localOpCount = patched;
	return true;
}

CNeoVMProgram* INeoVM::CreateProgram(const void* pBuffer, int iSize, std::string* err)
{
	return CNeoVMProgram::Create(pBuffer, iSize, err);
}

// ---------------------------------------------------------------------------
// 어셈블리 표기 (컴파일 리스팅 / 디버거 어셈블리 뷰 공용)
//
// opcode 별 표기는 이 파일의 FormatDebugOperation 한 곳에만 있다. 심볼(오퍼랜드 이름,
// 함수 이름, 점프 라벨)은 컴파일 타임과 런타임이 서로 다른 표에서 오므로 resolver 로 받는다.
// 예전엔 이 switch 가 NeoExport.cpp 안에 있었고 std::cout 으로 직접 찍었다.
// ---------------------------------------------------------------------------

// 로드 후 PatchLocalOps 가 만든 _L 변형을 원래 opcode 로 되돌린다.
// _L 은 "오퍼랜드가 전부 로컬"이라는 실행 최적화 표시일 뿐 의미가 같으므로, 표기도 같아야
// 한다. 이 정규화가 없으면 런타임 코드의 상당수가 default 로 빠져 빈 줄이 된다
// (치환 대상이 MOV/ADD/CALL/RETURN 같은 최빈 op 이다 — PatchLocalOps 참조).
static eNOperation NormalizeLocalOp(eNOperation op, bool* outIsLocal)
{
	eNOperation base = op;
	switch (op)
	{
	case NOP_MOV_L:          base = NOP_MOV;          break;
	case NOP_MOVI_L:         base = NOP_MOVI;         break;
	case NOP_MOVF_L:         base = NOP_MOVF;         break;
	case NOP_MOV_MINUS_L:    base = NOP_MOV_MINUS;    break;
	case NOP_ADD2_L:         base = NOP_ADD2;         break;
	case NOP_ADD3_L:         base = NOP_ADD3;         break;
	case NOP_SUB3_L:         base = NOP_SUB3;         break;
	case NOP_MUL3_L:         base = NOP_MUL3;         break;
	case NOP_AND_L:          base = NOP_AND;          break;
	case NOP_JMP_GREAT_L:    base = NOP_JMP_GREAT;    break;
	case NOP_JMP_GREAT_EQ_L: base = NOP_JMP_GREAT_EQ; break;
	case NOP_JMP_LESS_L:     base = NOP_JMP_LESS;     break;
	case NOP_JMP_LESS_EQ_L:  base = NOP_JMP_LESS_EQ;  break;
	case NOP_JMP_EQUAL2_L:   base = NOP_JMP_EQUAL2;   break;
	case NOP_JMP_NEQUAL_L:   base = NOP_JMP_NEQUAL;   break;
	case NOP_STR_ADD_L:      base = NOP_STR_ADD;      break;
	case NOP_TOINT_L:        base = NOP_TOINT;        break;
	case NOP_CALL_L:         base = NOP_CALL;         break;
	case NOP_PTRCALL_L:      base = NOP_PTRCALL;      break;
	case NOP_PTRCALL2_L:     base = NOP_PTRCALL2;     break;
	case NOP_RETURN_L:       base = NOP_RETURN;       break;
	case NOP_CLT_READ_L:     base = NOP_CLT_READ;     break;
	case NOP_CLT_MOV_L:      base = NOP_CLT_MOV;      break;
	case NOP_LIST_ALLOC_L:   base = NOP_LIST_ALLOC;   break;
	case NOP_CHANGE_INT_L:   base = NOP_CHANGE_INT;   break;
	default: break;
	}
	if (outIsLocal != nullptr)
		*outIsLocal = (base != op);
	return base;
}

bool FormatDebugOperation(const DebugInstructionFormatContext& context, std::string& outAssembly,
	int* outByteCount)
{
	outAssembly.clear();
	if (outByteCount != nullptr)
		*outByteCount = 0;
	if (context.operation == nullptr)
		return false;

	// 원본 switch 가 v 를 쓰고 있어 이름을 유지한다(치환 실수를 줄인다).
	SVMOperation v = *context.operation;
	bool isLocalVariant = false;
	v.op = NormalizeLocalOp((eNOperation)v.op, &isLocalVariant);

	// 원본 WriteFunLog 의 지역 상수 그대로.
	const int OpFlagByteChars = 2;
	int byteCount = 0;
	bool known = true;

	switch (v.op)
	{
	case NOP_ADD2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("ADD  %s += %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_SUB2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("SUB  %s -= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_MUL2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("MUL  %s *= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_DIV2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("DIV  %s /= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_PERSENT2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("PER  %s %%%%= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_LSHIFT2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("LSH  %s <<= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_RSHIFT2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("RSH  %s >>= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_AND2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("AND  %s &= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_OR2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("OR   %s |= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_XOR2:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("XOR  %s ^= %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_LOG_NOT:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("NOT  %s = !%s", context.operand(1).c_str(), context.operand(2).c_str());
		break;

	case NOP_ADD3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("ADD  %s = %s + %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_SUB3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("SUB  %s = %s - %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_MUL3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("MUL  %s = %s * %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_DIV3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("DIV  %s = %s / %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_PERSENT3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("PER  %s = %s %%%% %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_LSHIFT3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("LSHF  %s = %s << %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_RSHIFT3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("RSHF  %s = %s >> %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_AND3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("AND  %s = %s & %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_OR3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("OR   %s = %s | %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_XOR3:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("XOR  %s = %s ^ %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;

	case NOP_VAR_CLEAR:
		byteCount = OpFlagByteChars + 2 * 1;
		outAssembly = FormatAsm("CLR  %s", context.operand(1).c_str());
		break;
	case NOP_INC:
		byteCount = OpFlagByteChars + 2 * 1;
		outAssembly = FormatAsm("INC  %s", context.operand(1).c_str());
		break;
	case NOP_DEC:
		byteCount = OpFlagByteChars + 2 * 1;
		outAssembly = FormatAsm("DEC  %s", context.operand(1).c_str());
		break;

	case NOP_GREAT:		// >
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("GR   %s = %s > %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_GREAT_EQ:	// >=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("GE   %s = %s >= %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_LESS:		// <
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("LS   %s = %s < %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_LESS_EQ:	// <=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("LE   %s = %s <= %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_EQUAL2:	// ==
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("EQ   %s = %s == %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_NEQUAL:	// !=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("NE   %s = %s != %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_AND:	// &
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("AND  %s = %s & %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_OR:	// |
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("OR   %s = %s | %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_LOG_AND:	// &&
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("AND2 %s = %s && %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_LOG_OR:	// ||
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("NE   %s = %s || %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;

	case NOP_STR_ADD:	// ..
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("SADD %s = ToStr%s + ToStr%s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;

	case NOP_JMP:
		byteCount = OpFlagByteChars + 2 * 1;
		outAssembly = FormatAsm("JMP  %d%s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str());
		break;
	case NOP_JMP_FALSE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("JF   %d%s %s is False", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str());
		break;
	case NOP_JMP_TRUE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("JT   %d%s %s is True", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str());
		break;

	case NOP_JMP_GREAT:		// >
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JGR  %d%s,  %s > %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_GREAT_EQ:	// >=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JGE  %d%s,  %s >= %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_LESS:		// <
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JLS  %d%s,  %s < %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_LESS_EQ:	// <=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JLE  %d%s,  %s <= %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_EQUAL2:	// ==
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JEQ  %d%s,  %s == %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_NEQUAL:	// !=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JNE  %d%s,  %s != %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_AND:	// &&
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JAND %d%s,  %s && %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_OR:		// ||
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JOR  %d%s,  %s || %s", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_NAND:	// !(&&)
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JNAND %d%s,  !(%s && %s)", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_NOR:	// !(||)
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JNOR %d%s,  !(%s || %s)", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_FOR:	// for
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JFOR %d%s,  C[S.%d] < E[S.%d]", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), v.n2, v.n3 + 1);
		break;
	case NOP_JMP_FOREACH:	// foreach
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JFRE %d%s,  T%s, K[S.%d], V[S.%d]", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), v.n3, v.n3+1);
		break;
	case NOP_SWITCH:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("SWTC table[%u], key %s", (unsigned)(u16)v.n1, context.operand(2).c_str());
		break;

	case NOP_TOSTRING:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("ToStr %s = %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_TOINT:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("ToInt %s = %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_TOFLOAT:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("ToFlo %s = %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_TOSIZE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("ToSize %s = %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_GETTYPE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("GetType %s = %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_SLEEP:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("Sleep %s", context.operand(2).c_str());
		break;

	case NOP_MOV:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("MOV  %s = %s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_MOVI:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("MOVI %s = %d", context.operand(1).c_str(), v.n23);
		break;
	case NOP_MOVF:
	{
		byteCount = OpFlagByteChars + 2 * 3;
		if (NEOS_CAN_EMBED_FLOAT_IMMEDIATE)
		{
			NS_FLOAT value = 0;
			std::memcpy(&value, &v.n23, sizeof(v.n23));
			outAssembly = FormatAsm("MOVF %s = %g", context.operand(1).c_str(), (double)value);
		}
		else
			outAssembly = FormatAsm("MOVF %s = <invalid precision>", context.operand(1).c_str());
		break;
	}
	case NOP_MOV_MINUS:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("MOV  %s = -%s", context.operand(1).c_str(), context.operand(2).c_str());
		break;

	case NOP_FMOV1:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("MOV  %s = &%s", context.operand(1).c_str(), context.functionShort(v.n2).c_str());
		break;
	case NOP_FMOV2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("MOV  %s.%s = &%s", context.operand(1).c_str(), context.operand(2).c_str(), context.functionShort(v.n3).c_str());
		break;

	case NOP_PTRCALL:
		byteCount = OpFlagByteChars + 2 * 3;
		if(v.n2 != -1)
			outAssembly = FormatAsm("PCAL %s.%s arg:%d", context.operand(1).c_str(), context.operand(2).c_str(), v.n3);
		else
			outAssembly = FormatAsm("PCAL %s arg:%d", context.operand(1).c_str(), v.n3);
		break;
	case NOP_PTRCALL2:
		if (v.argFlag & NEOS_OP_CALL_NORESULT)
		{
			byteCount = OpFlagByteChars + 2 * 2;
			outAssembly = FormatAsm("PCA2 %s arg:%d", context.operand(1).c_str(), v.n2);
		}
		else
		{
			byteCount = OpFlagByteChars + 2 * 3;
			outAssembly = FormatAsm("PCA2 %s = %s arg:%d", context.operand(3).c_str(), context.operand(1).c_str(), v.n2);
		}
		break;
	case NOP_NATIVECALL: // 컴파일에 나오지 않고, LoadVM 에서 NOP_PTRCALL2 인경우 System 함수이름을 찾어서 함수 Index 화
		if (v.argFlag & NEOS_OP_CALL_NORESULT)
		{
			byteCount = OpFlagByteChars + 2 * 2;
			outAssembly = FormatAsm("NCAL native:%d arg:%d", v.n1, v.n2);
		}
		else
		{
			byteCount = OpFlagByteChars + 2 * 3;
			outAssembly = FormatAsm("NCAL %s = native:%d arg:%d", context.operand(3).c_str(), v.n1, v.n2);
		}
		break;
	case NOP_CALL:
		if(v.argFlag & NEOS_OP_CALL_NORESULT)
		{
			byteCount = OpFlagByteChars + 2 * 2;
			outAssembly = FormatAsm("CALL %s arg:%d", context.functionFull(v.n1).c_str(), v.n2);
		}
		else
		{
			byteCount = OpFlagByteChars + 2 * 3;
			outAssembly = FormatAsm("CALL %s = %s arg:%d", context.operand(3).c_str(), context.functionFull(v.n1).c_str(), v.n2);
		}
		break;
	case NOP_RETURN:
		// NORESULT = 값 없는 'return;'. n1(반환값 슬롯)을 쓰지 않으므로 op/flag 2바이트만 덤프한다.
		if (v.argFlag & NEOS_OP_CALL_NORESULT)
		{
			byteCount = OpFlagByteChars;
			outAssembly = FormatAsm("RET");
		}
		else
		{
			byteCount = OpFlagByteChars + 2 * 1;
			outAssembly = FormatAsm("RET  %s", context.operand(1).c_str());
		}
		break;
	//case NOP_FUNEND:
	//	outAssembly = FormatAsm("- End -");
	//	break;
	case NOP_TABLE_ALLOC:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("Table Alloc %s, %d", context.operand(1).c_str(), v.n23);
		break;
	case NOP_CLT_MOV:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("MOV  %s.%s = %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;

	case NOP_TABLE_ADD2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("TADD %s.%s += %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_TABLE_SUB2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("TSUB %s.%s -= %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_TABLE_MUL2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("TMUL %s.%s *= %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_TABLE_DIV2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("TDIV %s.%s /= %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_TABLE_PERSENT2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("TPER %s.%s %= %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;

	case NOP_CLT_READ:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("READ %s = %s.%s", context.operand(3).c_str(), context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_TABLE_REMOVE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("Table Remove %s.%s", context.operand(1).c_str(), context.operand(2).c_str());
		break;

	case NOP_LIST_ALLOC:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("List Alloc %s, %d", context.operand(1).c_str(), v.n23);
		break;
	case NOP_LIST_REMOVE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("List Remove %s.%s", context.operand(1).c_str(), context.operand(2).c_str());
		break;

	case NOP_VERIFY_TYPE:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("Verify %s %s", context.operand(1).c_str(), GetDataType((VAR_TYPE)v.n2).c_str());
		break;
	case NOP_CHANGE_INT:
		byteCount = OpFlagByteChars + 2 * 1;
		outAssembly = FormatAsm("Change Int %s", context.operand(1).c_str());
		break;
	case NOP_YIELD:
		byteCount = OpFlagByteChars + 2 * 2;
		if(v.n1 == YILED_RETURN)
			outAssembly = FormatAsm("yield return");
		else
			{ outAssembly = FormatAsm("?? yield sub(%d)", v.n1); known = false; }
		break;
	case NOP_ERROR:
		outAssembly = FormatAsm("Begin");
		break;
	default:
		{ outAssembly = FormatAsm("?? op(%d)", (int)v.op); known = false; }
		break;
	}

	if (outByteCount != nullptr)
		*outByteCount = byteCount;
	// 로컬 전용 변형이었다는 표시. 같은 명령이지만 실행 경로가 다르다는 걸 볼 수 있게 한다.
	if (isLocalVariant)
		outAssembly += "  ;L";
	return known;
}

// 컴파일 타임 GetValueString(NeoExport.cpp)의 런타임 짝. static 상수 하나를 표기한다.
// 컴파일 타임은 VarInfo, 런타임은 SStaticConst 라 타입이 달라 본문을 공유할 수 없다.
static std::string StaticConstToString(const SStaticConst& c)
{
	char ch[128] = { 0, };
	switch (c._type)
	{
	case VAR_INT:
		snprintf(ch, sizeof(ch), "%d", c._int);
		break;
	case VAR_FLOAT:
#ifdef NS_SINGLE_PRECISION
		snprintf(ch, sizeof(ch), "%f", c._float);
#else
		snprintf(ch, sizeof(ch), "%lf", c._float);
#endif
		break;
	case VAR_BOOL:
		snprintf(ch, sizeof(ch), "'%s'", c._bl ? "true" : "false");
		break;
	case VAR_STRING:
	{
		// 제어문자는 컴파일 리스팅과 같은 방식으로 이스케이프한다.
		std::string r = "'";
		for (auto it = c._str.begin(); it != c._str.end(); ++it)
		{
			char x = *it;
			if (x == '\n')       r += "\\n";
			else if (x == '\r')  r += "\\r";
			else if (x == '\t')  r += "\\t";
			else                  r += x;
		}
		r += "'";
		return r;
	}
	default:
		snprintf(ch, sizeof(ch), "unknown type (%d)", (int)c._type);
		break;
	}
	return ch;
}

// 프로그램 이미지용 심볼 캐시. GetDebugInstructions 는 code 전체를 훑으므로
// 역맵과 함수 경계를 op 마다 다시 만들면 O(N*F) 가 된다. 한 번 만들어 재사용한다.
struct ProgramSymbolCache
{
	const CNeoVMProgram* program = nullptr;
	std::map<int, std::string> globalNames;       // 전역 슬롯 -> 이름
	std::vector<std::pair<int, int>> funStarts;   // (시작 op 인덱스, 함수 ID) 오름차순

	void Build(const CNeoVMProgram& p)
	{
		program = &p;
		globalNames = p.debugGlobalNames;
		// 디버그 정보가 없으면 export 된 것만이라도 이름을 붙인다.
		for (auto it = p.exportVariables.begin(); it != p.exportVariables.end(); ++it)
		{
			if (globalNames.find((*it).second) == globalNames.end())
				globalNames[(*it).second] = (*it).first;
		}
		funStarts.reserve(p.functions.size());
		for (int i = 0; i < (int)p.functions.size(); ++i)
			funStarts.push_back(std::make_pair(p.functions[i]._codePtr / (int)sizeof(SVMOperation), i));
		std::sort(funStarts.begin(), funStarts.end());
	}

	// op 가 속한 함수 ID. 프롤로그(NOP_ERROR/NOP_IDLE)는 어느 함수에도 안 속하므로 -1.
	int FunctionOf(int opIndex) const
	{
		int found = -1;
		for (size_t i = 0; i < funStarts.size(); ++i)
		{
			if (funStarts[i].first > opIndex)
				break;
			found = funStarts[i].second;
		}
		return found;
	}

	std::string FunctionName(int id) const
	{
		if (program == nullptr)
			return "";
		auto it = program->debugFunctionNames.find(id);
		if (it != program->debugFunctionNames.end())
			return (*it).second;
		for (auto e = program->exportFunctions.begin(); e != program->exportFunctions.end(); ++e)
		{
			if ((*e).second == id)
				return (*e).first;
		}
		char ch[64];
		snprintf(ch, sizeof(ch), "function#%d", id);
		return ch;
	}

	// 컴파일 타임 GetLog 와 같은 표기를 낸다.
	std::string Operand(const SVMOperation& op, int funId, int argIndex) const
	{
		if (program == nullptr)
			return "";
		int v = 0;
		bool isLocal = false;
		if (argIndex == 1)      { v = op.n1; isLocal = (op.argFlag & NEOS_ARG_N1_LOCAL) != 0; }
		else if (argIndex == 2) { v = op.n2; isLocal = (op.argFlag & NEOS_ARG_N2_LOCAL) != 0; }
		else if (argIndex == 3) { v = op.n3; isLocal = (op.argFlag & NEOS_ARG_N3_LOCAL) != 0; }

		char ch[256];
		if (isLocal)
		{
			// 로컬 슬롯 이름은 함수별 표에 있다. 임시/인자 슬롯은 이름이 없다.
			if (funId >= 0)
			{
				auto f = program->debugVarNames.find(funId);
				if (f != program->debugVarNames.end())
				{
					auto n = (*f).second.find(v);
					if (n != (*f).second.end())
					{
						snprintf(ch, sizeof(ch), "[S.%d %s]", v, (*n).second.c_str());
						return ch;
					}
				}
			}
			snprintf(ch, sizeof(ch), "[S.%d]", v);
			return ch;
		}

		// 전역 영역. 음수 오퍼랜드(-1 = 사용 안 함)가 이미지에 남을 수 있으므로 하한도 막는다.
		const int staticCount = (int)program->staticValues.size();
		if (v >= 0 && v < staticCount)
		{
			snprintf(ch, sizeof(ch), "[G.%d %s]", v, StaticConstToString(program->staticValues[v]).c_str());
			return ch;
		}
		if (v >= 0 && v < program->GetGlobalSlotCount())
		{
			auto it = globalNames.find(v);
			if (it != globalNames.end())
			{
				snprintf(ch, sizeof(ch), "[G.%d %s]", v, (*it).second.c_str());
				return ch;
			}
		}
		snprintf(ch, sizeof(ch), "[G.%d ???]", v);
		return ch;
	}
};

void FormatDebugInstruction(const CNeoVMProgram& program, int opIndex,
	std::string& outByteCode, std::string& outAssembly)
{
	outByteCode.clear();
	outAssembly.clear();
	if (opIndex < 0 || opIndex >= (int)program.code.size())
		return;

	// 같은 프로그램을 연속 조회하는 것이 정상 사용이라 마지막 것만 들고 있는다.
	static thread_local ProgramSymbolCache s_cache;
	if (s_cache.program != &program)
		s_cache.Build(program);

	const SVMOperation& v = program.code[opIndex];
	const int funId = s_cache.FunctionOf(opIndex);

	DebugInstructionFormatContext context;
	context.operation = &v;
	context.opIndex = opIndex;
	context.operand = [&](int argIndex) { return s_cache.Operand(v, funId, argIndex); };
	context.functionFull = [&](int id) { return s_cache.FunctionName(id); };
	context.functionShort = context.functionFull;
	// 런타임은 함수 단위 라벨을 만들지 않는다. 대신 절대 목적지를 보여준다.
	context.jumpLabel = [&](int target) { return FormatAsm(" -> #%d", target); };

	int byteCount = 0;
	FormatDebugOperation(context, outAssembly, &byteCount);

	// 컴파일 리스팅의 바이트 덤프와 같은 폭/형식.
	const int kDumpBytes = 12;
	if (byteCount > (int)sizeof(SVMOperation))
		byteCount = (int)sizeof(SVMOperation);
	outByteCode = FormatBytes((const u8*)&v, byteCount, kDumpBytes);
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

	// 옛 CompileAndLoadVM 과 동일: putASM(진단) 시 컴파일 성공 + 코드 용량 출력.
	// (ANSI 색상 리터럴 인라인 — NeoParser.h 의존 회피, "\x1b[32m"=green, "\x1b[0m"=reset)
	if (param.putASM)
		printf("\x1b[32m" "Compile Success. Code : %d bytes !!" "\x1b[0m" "\n", ar.GetBufferOffset());

	return CNeoVMProgram::Create(ar.GetData(), ar.GetBufferOffset(), param.err);
}



};
