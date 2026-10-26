#include <limits.h>

#include "NeoVMProgram.h"
#include "NeoVMInternal.h"
#include "NeoArchive.h"
#include "NeoTextLoader.h"   // FormatAsm / FormatBytes
#include <algorithm>
#include <memory>
#include <stdexcept>

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
	case VAR_INT:    return HashIntegerKey(k._int);
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

	size_t capa = 8;
	while (capa < entries.size() * 2)
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
	case VAR_INT:    h = HashIntegerKey(pKey->_int); break;
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

struct ImageLoadError { const char* message; };
static size_t ImageBytesRemaining(CNArchive& ar)
{
	return (size_t)(ar.GetBufferSize() - ar.GetBufferOffset());
}
static void RequireImageCount(CNArchive& ar, size_t count, size_t minimumBytes, const char* message)
{
	if (minimumBytes == 0 || count > ImageBytesRemaining(ar) / minimumBytes)
		throw ImageLoadError{message};
}
template<typename T> static void ReadRequired(CNArchive& ar, T& value)
{
	if (ar.Read(&value, sizeof(value)) != sizeof(value))
		throw ImageLoadError{"Truncated script image (required field)"};
}

static bool ReadImageBool(CNArchive& ar)
{
	u8 value = 0;
	ReadRequired(ar, value);
	if (value > 1) throw ImageLoadError{"Invalid boolean constant"};
	return value != 0;
}

static bool ReadString(CNArchive& ar, std::string& str)
{
	short nLen = 0;
	if (ar.Read(&nLen, sizeof(nLen)) == 0)
		return false;
	if (nLen < 0 || (size_t)nLen > ImageBytesRemaining(ar))
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
	{
		try { *err = msg; } catch (const std::bad_alloc&) {}
	}
}

CNeoVMProgram* CNeoVMProgram::Create(const void* pBuffer, int iSize, std::string* err)
{
	if (pBuffer == nullptr || iSize <= (int)sizeof(SNeoVMHeader))
	{
		SetLoadError(err, "Invalid script image");
		return nullptr;
	}

	try
	{
		CNeoVM::InitLib();
		CNArchive ar(const_cast<void*>(pBuffer), iSize);
		auto release = [](CNeoVMProgram* program) { if (program) program->Release(); };
		std::unique_ptr<CNeoVMProgram, decltype(release)> p(new CNeoVMProgram(), release);
		p->byteSize = iSize;
		if (!p->Load(ar, err)) return nullptr;
		p->PatchNativeCalls();
		p->PatchStaticStringReads();
		if (!p->PatchLocalOps(err)) return nullptr;
		return p.release();
	}
	catch (const ImageLoadError& error) { SetLoadError(err, error.message); }
	catch (const std::length_error&) { SetLoadError(err, "Invalid script image allocation size"); }
	catch (const std::bad_alloc&)
	{
		// 메모리 부족 메시지 작성 중에도 예외가 로더 API 밖으로 새어 나가지 않게 한다.
		try { SetLoadError(err, "Out of memory while loading script image"); } catch (...) {}
	}
	return nullptr;
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
	if (IsDataSinglePrecision != NeoVMSystem::IsSinglePrecision())
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
		header._iGlobalVarCount < 0 || header._iGlobalVarCount > 32767 || header._iExportVarCount < 0 ||
		header._iStaticVarCount > INT_MAX - header._iGlobalVarCount - 1 ||
		header._iFunctionCount > SHRT_MAX + 1 || header.m_iDebugCount < 0)
	{
		SetLoadError(err, "Invalid script image counts");
		return false;
	}

	RequireImageCount(ar, header._iCodeSize, 1, "Truncated script image (code size)");
	code.resize(header._iCodeSize / sizeof(SVMOperation));
	if (header._iCodeSize > 0 && ar.Read(code.data(), header._iCodeSize) == 0)
	{
		SetLoadError(err, "Truncated script image (code)");
		return false;
	}

	RequireImageCount(ar, header._iFunctionCount, 19, "Truncated script image (function count)");
	functions.resize(header._iFunctionCount);
	std::vector<bool> functionIDs(header._iFunctionCount, false);
	std::string name;
	for (int i = 0; i < header._iFunctionCount; i++)
	{
		int iID = -1;
		SFunctionTable fun{};

		if (ar.Read(&iID, sizeof(iID)) == 0)
		{
			SetLoadError(err, "Truncated script image (function table)");
			return false;
		}
		ReadRequired(ar, fun._codePtr); ReadRequired(ar, fun._argsCount);
		ReadRequired(ar, fun._localTempMax); ReadRequired(ar, fun._localVarCount); ReadRequired(ar, fun._funType);
		if (fun._argsCount < 0 || fun._localTempMax < 0 || fun._localVarCount < 0 || fun._funType > FUNT_BUILT_IN ||
			(int64_t)fun._argsCount + fun._localTempMax + fun._localVarCount + 1 > SHRT_MAX)
		{
			SetLoadError(err, "Invalid function local counts");
			return false;
		}
		u16 captureCount = 0;
		ReadRequired(ar, captureCount);
		RequireImageCount(ar, captureCount, 2 * sizeof(short), "Truncated script image (captures)");
		if (captureCount > (u16)fun._localVarCount)
		{
			SetLoadError(err, "Invalid function closure capture count");
			return false;
		}
		fun._captures.resize(captureCount);
		for (u16 c = 0; c < captureCount; ++c)
		{
			ReadRequired(ar, fun._captures[c].first); ReadRequired(ar, fun._captures[c].second);
			if (fun._captures[c].first <= 0 || fun._captures[c].second <= 0)
			{
				SetLoadError(err, "Invalid function closure capture slot");
				return false;
			}
		}
		if (iID < 0 || iID >= header._iFunctionCount || functionIDs[iID])
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
		if (fun._localAddCount <= 0)
		{
			SetLoadError(err, "Invalid function local range");
			return false;
		}
		for (const std::pair<short, short>& capture : fun._captures)
		{
			if (capture.second >= fun._localAddCount)
			{
				SetLoadError(err, "Invalid function closure local slot");
				return false;
			}
		}
		functionIDs[iID] = true;
		functions[iID] = std::move(fun);
	}

	// FMOV가 읽는 capture sourceSlot은 생성한 함수(직속 부모)의 스택 기준이다.
	// 이미지가 손상돼 슬롯/함수 번호가 어긋나면 호출 전에 거부해 프레임 밖 읽기나
	// 쓰기로 이어지지 않게 한다.
	std::vector<std::pair<int, int>> functionStarts;
	functionStarts.reserve(functions.size());
	for (int i = 0; i < (int)functions.size(); ++i)
	{
		const int codePtr = functions[i]._codePtr;
		if (codePtr < 2 * (int)sizeof(SVMOperation) || codePtr >= header._iCodeSize ||
			(codePtr % (int)sizeof(SVMOperation)) != 0)
		{
			SetLoadError(err, "Invalid function code offset");
			return false;
		}
		functionStarts.push_back(std::make_pair(codePtr / (int)sizeof(SVMOperation), i));
	}
	std::sort(functionStarts.begin(), functionStarts.end());
	std::vector<int> codeOwners(code.size(), -1);
	for (size_t i = 0; i < functionStarts.size(); ++i)
	{
		const int begin = functionStarts[i].first;
		const int end = (i + 1 < functionStarts.size()) ? functionStarts[i + 1].first : (int)code.size();
		if (begin >= end)
		{
			SetLoadError(err, "Invalid function code range");
			return false;
		}
		for (int opIndex = begin; opIndex < end; ++opIndex)
			codeOwners[opIndex] = functionStarts[i].second;
	}
	// 호출 인자를 만드는 명령은 CALL보다 먼저 실행된다. 호출자 진입 시 전체 인자
	// 범위를 확보하되, 다음 프레임의 시작 위치는 기존 _localAddCount로 유지한다.
	for (SFunctionTable& fun : functions) fun._stackSlotCount = fun._localAddCount;
	for (size_t i = 0; i < code.size(); ++i)
	{
		if (codeOwners[i] < 0) continue;
		SFunctionTable& fun = functions[codeOwners[i]];
		const SVMOperation& op = code[i];
		int outgoing = -1;
		switch (op.op)
		{
		case NOP_CALL:
		case NOP_CALL_L: outgoing = op.n2; break;
		case NOP_PTRCALL:
		case NOP_PTRCALL_L: outgoing = op.n3; break;
		case NOP_PTRCALL2:
		case NOP_PTRCALL2_L:
		case NOP_NATIVECALL:
		case NOP_VEC_MAKE: outgoing = op.n2; break;
		default: break;
		}
		if (outgoing >= 0)
			fun._stackSlotCount = (std::max)(fun._stackSlotCount, fun._localAddCount + outgoing + 1);
	}
	// 피연산자 값으로 프레임을 늘리면 잘못된 큰 인덱스도 정상으로 인정하게 된다.
	// 호출 인자 수로 범위를 먼저 확정하고, 접힌 getter가 쓰는 반환 슬롯만 따로 허용한다.
	for (size_t i = 0; i < code.size(); ++i)
	{
		if (codeOwners[i] < 0) continue;
		SFunctionTable& fun = functions[codeOwners[i]];
		const SVMOperation& op = code[i];
		const short slots[] = { op.n1, op.n2, op.n3 };
		const u8 flags[] = { NEOS_ARG_N1_LOCAL, NEOS_ARG_N2_LOCAL, NEOS_ARG_N3_LOCAL };
		for (int n = 0; n < 3; ++n)
			if ((op.argFlag & flags[n]) && slots[n] == fun._localAddCount)
				fun._stackSlotCount = (std::max)(fun._stackSlotCount, fun._localAddCount + 1);
	}
	// WREF consumes its next instruction only for stored vectors. Validate the
	// immutable pair once here, so ordinary collection writes need no hot checks.
	auto IsRefWriteOpcode = [](eNOperation op) {
		switch (op) {
		case NOP_CLT_MOV:
		case NOP_CLT_MOV_L:
		case NOP_TABLE_ADD2:
		case NOP_TABLE_SUB2:
		case NOP_TABLE_MUL2:
		case NOP_TABLE_DIV2:
		case NOP_TABLE_PERSENT2: return true;
		default: return false;
		}
	};
	for (size_t i = 0; i < code.size(); ++i)
	{
		const SVMOperation& op = code[i];
		if (IsRefWriteOpcode(op.op) && (op.argFlag & NEOS_OP_REF_WRITE) &&
			(i == 0 || code[i - 1].op != NOP_CLT_WRITE_REF))
		{
			SetLoadError(err, "Orphan chained write");
			return false;
		}
		if (op.op != NOP_CLT_WRITE_REF) continue;
		if (i + 1 >= code.size() || codeOwners[i] < 0 || codeOwners[i] != codeOwners[i + 1])
		{
			SetLoadError(err, "Incomplete chained write");
			return false;
		}
		const SVMOperation& write = code[i + 1];
		const u8 locals = NEOS_ARG_N1_LOCAL | NEOS_ARG_N2_LOCAL | NEOS_ARG_N3_LOCAL;
		const auto& fun = functions[codeOwners[i]];
		auto ValidSlot = [&](short slot, bool local, bool writable) {
			return local ? slot >= 0 && slot < fun._localAddCount :
				(writable ? IsGlobalVariable(slot) : IsGlobalOperand(slot));
		};
		if (!IsRefWriteOpcode(write.op) || !(write.argFlag & NEOS_OP_REF_WRITE) ||
			(op.argFlag & ~(locals | NEOS_OP_REF_LOADED)) ||
			(write.argFlag & ~(locals | NEOS_OP_REF_WRITE)) ||
			write.n1 != op.n3 ||
			((write.argFlag & NEOS_ARG_N1_LOCAL) != 0) != ((op.argFlag & NEOS_ARG_N3_LOCAL) != 0) ||
			(write.op == NOP_CLT_MOV_L && (write.argFlag & locals) != locals) ||
			!ValidSlot(op.n1, op.argFlag & NEOS_ARG_N1_LOCAL, false) ||
			!ValidSlot(op.n2, op.argFlag & NEOS_ARG_N2_LOCAL, false) ||
			!ValidSlot(op.n3, op.argFlag & NEOS_ARG_N3_LOCAL, true) ||
			!ValidSlot(write.n2, write.argFlag & NEOS_ARG_N2_LOCAL, false) ||
			!ValidSlot(write.n3, write.argFlag & NEOS_ARG_N3_LOCAL, false))
		{
			SetLoadError(err, "Invalid chained write operands");
			return false;
		}
	}
	for (int opIndex = 0; opIndex < (int)code.size(); ++opIndex)
	{
		const SVMOperation& op = code[opIndex];
		int functionID = -1;
		if (op.op == NOP_FMOV1) functionID = op.n2;
		else if (op.op == NOP_FMOV2) functionID = op.n3;
		else continue;
		if (functionID < 0 || functionID >= (int)functions.size() || codeOwners[opIndex] < 0)
		{
			SetLoadError(err, "Invalid closure function reference");
			return false;
		}
		const SFunctionTable& parent = functions[codeOwners[opIndex]];
		for (const std::pair<short, short>& capture : functions[functionID]._captures)
		{
			if (capture.first >= parent._localAddCount)
			{
				SetLoadError(err, "Invalid closure source slot");
				return false;
			}
		}
	}

	RequireImageCount(ar, header._iExportVarCount, sizeof(int) + sizeof(short), "Truncated script image (export count)");
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
		if (!IsGlobalVariable(idx))
		{
			SetLoadError(err, "Invalid exported global slot");
			return false;
		}
		exportVariables[name] = idx;
	}

	RequireImageCount(ar, header._iStaticVarCount, sizeof(VAR_TYPE) + sizeof(bool), "Truncated script image (constant count)");
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
			ReadRequired(ar, sc._int);
			break;
		case VAR_FLOAT:
			ReadRequired(ar, sc._float);
			break;
		case VAR_BOOL:
			sc._bl = ReadImageBool(ar);
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
		RequireImageCount(ar, header.m_iDebugCount, sizeof(debug_info), "Truncated script image (debug count)");
		debugData.resize(header.m_iDebugCount);
		if (ar.Read(&debugData[0], (int)(sizeof(debug_info) * (size_t)header.m_iDebugCount)) == 0)
		{
			SetLoadError(err, "Truncated script image (debug info)");
			return false;
		}
	}

	// 선택 청크: 디버그 변수명(BVDN) / 함수명(NFDN). 미지의 magic 을 만나면 되돌리고 종료.
	while (ImageBytesRemaining(ar) != 0)
	{
		u32 magic = 0;
		int oldOffset = ar.GetBufferOffset();
		ReadRequired(ar, magic);
		if (magic == 0x4E445642)
		{
			u32 funCount = 0;
			if (false == ReadCount(ar, funCount))
				throw ImageLoadError{"Invalid or truncated script image chunk"};
			RequireImageCount(ar, funCount, sizeof(int) + sizeof(short), "Invalid debug function count");
			for (u32 i = 0; i < funCount; ++i)
			{
				int funId = -1;
				if (ar.Read(&funId, sizeof(funId)) == 0)
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				u32 nameCount = 0;
				if (false == ReadCount(ar, nameCount))
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				RequireImageCount(ar, nameCount, sizeof(int) + sizeof(short), "Invalid debug variable count");
				if (funId < -1 || funId >= (int)functions.size()) throw ImageLoadError{"Invalid debug function id"};
				std::map<int, std::string>& names = (funId == -1) ? debugGlobalNames : debugVarNames[funId];
				for (u32 n = 0; n < nameCount; ++n)
				{
					int slot = -1;
					if (ar.Read(&slot, sizeof(slot)) == 0)
						throw ImageLoadError{"Invalid or truncated script image chunk"};
					if (false == ReadString(ar, name))
						throw ImageLoadError{"Invalid or truncated script image chunk"};
					names[slot] = name;
				}
			}
		}
		else if (magic == 0x4E44464E)
		{
			u32 funCount = 0;
			if (false == ReadCount(ar, funCount))
				throw ImageLoadError{"Invalid or truncated script image chunk"};
			RequireImageCount(ar, funCount, sizeof(int) + sizeof(short), "Invalid debug function count");
			for (u32 i = 0; i < funCount; ++i)
			{
				int funId = -1;
				if (ar.Read(&funId, sizeof(funId)) == 0)
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				if (false == ReadString(ar, name))
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				if (funId < 0 || funId >= (int)functions.size()) throw ImageLoadError{"Invalid debug function id"};
				debugFunctionNames[funId] = name;
			}
		}
		else if (magic == 0x54495753)   // 'SWIT' : switch/case 테이블
		{
			u32 tableCount = 0;
			if (false == ReadCount(ar, tableCount))
				throw ImageLoadError{"Invalid or truncated script image chunk"};
			RequireImageCount(ar, tableCount, sizeof(int) + sizeof(u16), "Invalid switch table count");
			if (tableCount > (u32)USHRT_MAX + 1) throw ImageLoadError{"Invalid switch table count"};
			switchTables.resize(tableCount);
			for (u32 t = 0; t < tableCount; ++t)
			{
				ProgramSwitchTable& tbl = switchTables[t];
				if (ar.Read(&tbl.defaultOffset, sizeof(tbl.defaultOffset)) == 0)
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				u32 caseCount = 0;
				if (false == ReadCount(ar, caseCount))
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				RequireImageCount(ar, caseCount, sizeof(u8) + sizeof(bool) + sizeof(int), "Invalid switch case count");
				tbl.entries.resize(caseCount);
				for (u32 c = 0; c < caseCount; ++c)
				{
					ProgramSwitchEntry& e = tbl.entries[c];
					u8 keyType = 0;
					if (ar.Read(&keyType, sizeof(keyType)) == 0)
						throw ImageLoadError{"Invalid or truncated script image chunk"};
					e.key._type = (VAR_TYPE)keyType;
					switch (e.key._type)
					{
					case VAR_INT:
						if (ar.Read(&e.key._int, sizeof(e.key._int)) == 0) throw ImageLoadError{"Invalid or truncated script image chunk"};
						break;
					case VAR_BOOL:
						e.key._bl = ReadImageBool(ar);
						break;
					case VAR_STRING:
						if (false == ReadString(ar, e.key._str)) throw ImageLoadError{"Invalid or truncated script image chunk"};
						break;
					default:
						throw ImageLoadError{"Invalid or truncated script image chunk"};   // 손상된 이미지
					}
					if (ar.Read(&e.jumpOffset, sizeof(e.jumpOffset)) == 0)
						throw ImageLoadError{"Invalid or truncated script image chunk"};
				}
				tbl.Build();
			}
		}
		else if (magic == 0x45474E52)   // 'RNGE' : range jump descriptors
		{
			u32 rangeCount = 0;
			if (false == ReadCount(ar, rangeCount) || rangeCount > (u32)USHRT_MAX + 1)
				throw ImageLoadError{"Invalid or truncated script image chunk"};
			RequireImageCount(ar, rangeCount, 2 * sizeof(short) + sizeof(u8), "Invalid range descriptor count");
			rangeJumps.resize(rangeCount);
			for (u32 i = 0; i < rangeCount; ++i)
			{
				ProgramRangeJump& range = rangeJumps[i];
				if (ar.Read(&range.lower, sizeof(range.lower)) == 0 ||
					ar.Read(&range.upper, sizeof(range.upper)) == 0 ||
					ar.Read(&range.flags, sizeof(range.flags)) == 0)
				{
					throw ImageLoadError{"Invalid or truncated script image chunk"};
				}
				if (range.flags & ~(NEOS_RANGE_LOWER_LOCAL | NEOS_RANGE_UPPER_LOCAL |
					NEOS_RANGE_LOWER_INCLUSIVE | NEOS_RANGE_UPPER_INCLUSIVE | NEOS_RANGE_UPPER_FIRST))
					throw ImageLoadError{"Invalid or truncated script image chunk"};
			}
		}
		else
		{
			ar.SetBufferOffset(oldOffset);
			break;
		}
	}

	// range jump은 descriptor를 반드시 참조해야 한다. descriptor의 local slot은 해당
	// opcode를 소유한 함수 프레임 기준이고, global slot은 상수 0 기준 signed offset이다.
	for (int opIndex = 0; opIndex < (int)code.size(); ++opIndex)
	{
		const SVMOperation& op = code[opIndex];
		if (op.op == NOP_LOADK)
		{
			const bool local = (op.argFlag & NEOS_ARG_N1_LOCAL) != 0;
			if (op.n23 < 0 || op.n23 >= header._iStaticVarCount || codeOwners[opIndex] < 0 ||
				(local ? op.n1 < 0 : !IsGlobalVariable(op.n1)))
			{
				SetLoadError(err, "Invalid LOADK operand");
				return false;
			}
		}
		if (op.op != NOP_JMP_RANGE_INSIDE && op.op != NOP_JMP_RANGE_OUTSIDE)
			continue;
		const int rangeIndex = (int)(u16)op.n3;
		if (rangeIndex < 0 || rangeIndex >= (int)rangeJumps.size() || codeOwners[opIndex] < 0)
		{
			SetLoadError(err, "Invalid range jump descriptor");
			return false;
		}
		const SFunctionTable& fun = functions[codeOwners[opIndex]];
		const ProgramRangeJump& range = rangeJumps[rangeIndex];
		auto IsValidRangeOperand = [&](short slot, bool isLocal)
		{
			if (isLocal)
				return slot >= 0 && slot < fun._localAddCount;
			return IsGlobalOperand(slot);
		};
		if (IsValidRangeOperand(op.n2, (op.argFlag & NEOS_ARG_N2_LOCAL) != 0) == false ||
			IsValidRangeOperand(range.lower, (range.flags & NEOS_RANGE_LOWER_LOCAL) != 0) == false ||
			IsValidRangeOperand(range.upper, (range.flags & NEOS_RANGE_UPPER_LOCAL) != 0) == false)
		{
			SetLoadError(err, "Invalid range jump operand");
			return false;
		}
	}
	return VerifyCode(codeOwners, err);
}

// 저장된 명령 형식을 한 번 검증한다. 로드 후 치환용 opcode는 파일에서 받지 않고,
// 아래의 패치 단계에서 원본 명령 검증을 마친 뒤에만 만든다.
bool CNeoVMProgram::VerifyCode(const std::vector<int>& owners, std::string* err)
{
    auto reject = [&](const char* reason) { SetLoadError(err, reason); return false; };
    if (functions.empty() || code.size() < 3 || code[0].op != NOP_ERROR || code[1].op != NOP_IDLE ||
        owners[2] < 0)
        return reject("Invalid script entry instructions");
    const u8 L1 = NEOS_ARG_N1_LOCAL, L2 = NEOS_ARG_N2_LOCAL, L3 = NEOS_ARG_N3_LOCAL;
    for (size_t i = 2; i < code.size(); ++i)
    {
        if (owners[i] < 0) return reject("Instruction outside function");
        const SVMOperation& op = code[i];
        const SFunctionTable& fun = functions[owners[i]];
        unsigned slots = 0, writes = 0;
        int argumentCount = -1;
        bool jump = false, terminal = false;
        const bool noResult = (op.argFlag & NEOS_OP_CALL_NORESULT) != 0;
        switch (op.op)
        {
        case NOP_MOV: slots = 3; writes = 1; break;
        case NOP_MOVI:
        case NOP_MOVF: slots = writes = 1; break;
        case NOP_LOADK:
            slots = writes = 1;
            if (op.n23 < 0 || op.n23 >= header._iStaticVarCount) return reject("Invalid constant operand");
            break;
        case NOP_MOV_MINUS:
        case NOP_LOG_NOT:
        case NOP_ADD2:
        case NOP_SUB2:
        case NOP_MUL2:
        case NOP_DIV2:
        case NOP_PERSENT2:
        case NOP_LSHIFT2:
        case NOP_RSHIFT2:
        case NOP_AND2:
        case NOP_OR2:
        case NOP_XOR2: slots = 3; writes = 1; break;
        case NOP_VAR_CLEAR:
        case NOP_INC:
        case NOP_DEC: slots = writes = 1; break;
        case NOP_ADD3:
        case NOP_SUB3:
        case NOP_MUL3:
        case NOP_DIV3:
        case NOP_PERSENT3:
        case NOP_LSHIFT3:
        case NOP_RSHIFT3:
        case NOP_AND3:
        case NOP_OR3:
        case NOP_XOR3:
        case NOP_GREAT:
        case NOP_GREAT_EQ:
        case NOP_LESS:
        case NOP_LESS_EQ:
        case NOP_EQUAL2:
        case NOP_NEQUAL:
        case NOP_AND:
        case NOP_OR:
        case NOP_LOG_AND:
        case NOP_LOG_OR: slots = 7; writes = 1; break;
        case NOP_JMP: jump = terminal = true; break;
        case NOP_JMP_FALSE:
        case NOP_JMP_TRUE: slots = 2; jump = true; break;
        case NOP_JMP_GREAT:
        case NOP_JMP_GREAT_EQ:
        case NOP_JMP_LESS:
        case NOP_JMP_LESS_EQ:
        case NOP_JMP_EQUAL2:
        case NOP_JMP_NEQUAL:
        case NOP_JMP_AND:
        case NOP_JMP_OR:
        case NOP_JMP_NAND:
        case NOP_JMP_NOR: slots = 6; jump = true; break;
        case NOP_JMP_FOR:
            slots = writes = 2; jump = true;
            if (!(op.argFlag & L2) || op.n2 < 0 || op.n2 > fun._localAddCount - 4)
                return reject("Invalid for frame span");
            break;
        case NOP_JMP_FOREACH:
            slots = 6; writes = 4; jump = true;
            if (op.argFlag & L3) {
                if (op.n3 < 0 || op.n3 > fun._localAddCount - 4) return reject("Invalid foreach frame span");
            } else if (!IsGlobalVariable(op.n3) || !IsGlobalVariable((int)op.n3 + 3))
                return reject("Invalid foreach global span");
            break;
        case NOP_SWITCH: slots = 2; terminal = true; break;
        case NOP_STR_ADD: slots = 7; writes = 1; break;
        case NOP_TOSTRING:
        case NOP_TOINT:
        case NOP_TOFLOAT:
        case NOP_TOSIZE:
        case NOP_GETTYPE: slots = 3; writes = 1; break;
        case NOP_SLEEP: slots = 2; break;
        case NOP_FMOV1: slots = writes = 1; break;
        case NOP_FMOV2: slots = 3; writes = 1; break;
        case NOP_CALL:
            slots = writes = noResult ? 0 : 4;
            argumentCount = op.n2;
            if (op.n1 < 0 || op.n1 >= (int)functions.size() || argumentCount != functions[op.n1]._argsCount)
                return reject("Invalid call target or argument count");
            break;
        case NOP_PTRCALL:
            slots = ((op.argFlag & L2) || op.n2 != -1) ? 3 : 1;
            argumentCount = op.n3;
            break;
        case NOP_PTRCALL2:
            slots = noResult ? 1 : 5; writes = noResult ? 0 : 4; argumentCount = op.n2; break;
        case NOP_RETURN: slots = noResult ? 0 : 1; terminal = true; break;
        case NOP_TABLE_ALLOC: slots = writes = 1; break;
        case NOP_CLT_READ: slots = 7; writes = 4; break;
        case NOP_CLT_MOV:
        case NOP_TABLE_ADD2:
        case NOP_TABLE_SUB2:
        case NOP_TABLE_MUL2:
        case NOP_TABLE_DIV2:
        case NOP_TABLE_PERSENT2: slots = 7; writes = 1; break;
        case NOP_LIST_ALLOC: slots = writes = 1; break;
        case NOP_VERIFY_TYPE: slots = 1; break;
        case NOP_CHANGE_INT: slots = writes = 1; break;
        case NOP_YIELD: break;
        case NOP_JMP_RANGE_INSIDE:
        case NOP_JMP_RANGE_OUTSIDE: slots = 2; jump = true; break;
        case NOP_CLT_WRITE_REF: slots = 7; writes = 5; break;
        case NOP_CLT_READ_RESULT: slots = 7; writes = 4; break;
        case NOP_READ_APPLY: slots = 7; writes = 4; break;
        case NOP_NONE: break;
        case NOP_ERROR: terminal = true; break;
        default: return reject("Invalid saved instruction opcode");
        }
        if ((op.op == NOP_CALL || op.op == NOP_PTRCALL || op.op == NOP_PTRCALL2) && argumentCount < 0)
            return reject("Invalid call argument count");
        if ((op.op == NOP_TABLE_ALLOC || op.op == NOP_LIST_ALLOC) && op.n23 < 0)
            return reject("Invalid collection allocation count");
        const short values[] = {op.n1, op.n2, op.n3};
        const u8 flags[] = {L1, L2, L3};
        for (unsigned n = 0; n < 3; ++n)
        {
            if (!(slots & (1u << n))) continue;
            if (op.argFlag & (flags[n] << 3)) return reject("Immediate flag on variable operand");
            const int value = values[n];
            if (op.argFlag & flags[n]) {
                if (value < 0 || value >= fun._stackSlotCount) return reject("Invalid local operand");
            } else if ((writes & (1u << n)) ? !IsGlobalVariable(value) : !IsGlobalOperand(value))
                return reject("Invalid global operand");
        }
        if (op.op == NOP_READ_APPLY)
        {
            if (i + 1 >= code.size() || owners[i + 1] != owners[i])
                return reject("Incomplete read-apply pair");
            const SVMOperation& tail = code[i + 1];
            const int arity = ReadApplyArity(tail.op);
            // 후속 명령은 일반 명령이다. 로컬 임시값을 실제로 소비하는 허용 연산만 짝지을 수 있다.
            const bool second = (tail.argFlag & L2) && tail.n2 == op.n3;
            const bool third = arity == 3 && (tail.argFlag & L3) && tail.n3 == op.n3;
            if (!arity || !(op.argFlag & L3) || (op.argFlag & ~(L1 | L2 | L3)) ||
                (tail.argFlag & ~(L1 | L2 | L3)) || (!second && !third) ||
                (arity == 2 && (tail.argFlag & L1) && tail.n1 == op.n3))
                return reject("Invalid read-apply pair");
        }
        auto validJump = [&](int offset) {
            const int64_t target = (int64_t)i + 1 + offset;
            if (target < 2 || target >= (int64_t)code.size() || owners[(size_t)target] != owners[i]) return false;
            if (code[(size_t)target - 1].op == NOP_READ_APPLY) return false;
            const auto targetOp = code[(size_t)target].op;
            const bool writeTail = targetOp == NOP_CLT_MOV || (targetOp >= NOP_TABLE_ADD2 && targetOp <= NOP_TABLE_PERSENT2);
            return !(writeTail && (code[(size_t)target].argFlag & NEOS_OP_REF_WRITE));
        };
        if (jump && !validJump(op.n1)) return reject("Invalid branch target");
        if (op.op == NOP_SWITCH)
        {
            const size_t tableIndex = (u16)op.n1;
            if (tableIndex >= switchTables.size()) return reject("Invalid switch table reference");
            const auto& table = switchTables[tableIndex];
            if (!validJump(table.defaultOffset)) return reject("Invalid switch default target");
            for (const auto& entry : table.entries)
                if (!validJump(entry.jumpOffset)) return reject("Invalid switch case target");
        }
        if (!terminal && (i + 1 >= code.size() || owners[i + 1] != owners[i]))
            return reject("Instruction falls through function boundary");
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

		int nativeIndex = CNeoVM::FindDefaultNativeIndex(sc._str);
		if (nativeIndex < 0 || nativeIndex > SHRT_MAX)
			continue;

		// intrinsic 이 지정된 native(벡터 생성 등)는 전용 opcode 로 패치 (n2=인자수, n3=결과 유지).
		eNOperation intrinsic = (eNOperation)CNeoVM::GetDefaultNativeIntrinsic(nativeIndex);
		if (intrinsic != NOP_NONE)
		{
			op.op = intrinsic;
			continue;
		}

		op.op = NOP_NATIVECALL;
		op.n1 = (short)nativeIndex;
	}
}

void CNeoVMProgram::PatchStaticStringReads()
{
	const int staticCount = (int)staticValues.size();
	const int opCount = (int)code.size();
	for (int i = 0; i < opCount; ++i)
	{
		SVMOperation& op = code[i];
		if (op.op != NOP_CLT_READ)
			continue;

		// READ의 n2가 static 영역의 문자열 상수여야 한다. local/global 변수와
		// immediate key는 값이 바뀔 수 있으므로 범용 경로를 유지한다.
		if ((op.argFlag & (NEOS_ARG_N2_LOCAL | NEOS_ARG_N2_IMMEDIATE)) != 0)
			continue;
		if (op.n2 < 0 || op.n2 >= staticCount)
			continue;
		if (staticValues[op.n2]._type != VAR_STRING)
			continue;

		op.op = NOP_CLT_READ_STATIC_STRING;
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
			if (f & L1) to = NOP_MOVF_L;
			break;
		case NOP_MOVF_L:
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
		case NOP_JMP_RANGE_INSIDE:
			if ((f & L2) && (size_t)(u16)op.n3 < rangeJumps.size())
			{
				const ProgramRangeJump& range = rangeJumps[(u16)op.n3];
				if ((range.flags & (NEOS_RANGE_LOWER_LOCAL | NEOS_RANGE_UPPER_LOCAL)) ==
					(NEOS_RANGE_LOWER_LOCAL | NEOS_RANGE_UPPER_LOCAL))
					to = NOP_JMP_RANGE_INSIDE_L;
			}
			break;
		case NOP_JMP_RANGE_OUTSIDE:
			if ((f & L2) && (size_t)(u16)op.n3 < rangeJumps.size())
			{
				const ProgramRangeJump& range = rangeJumps[(u16)op.n3];
				if ((range.flags & (NEOS_RANGE_LOWER_LOCAL | NEOS_RANGE_UPPER_LOCAL)) ==
					(NEOS_RANGE_LOWER_LOCAL | NEOS_RANGE_UPPER_LOCAL))
					to = NOP_JMP_RANGE_OUTSIDE_L;
			}
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

CNeoVMProgram* NeoVMSystem::CreateProgram(const void* pBuffer, int iSize, std::string* err)
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
	case NOP_JMP_RANGE_INSIDE_L:  base = NOP_JMP_RANGE_INSIDE;  break;
	case NOP_JMP_RANGE_OUTSIDE_L: base = NOP_JMP_RANGE_OUTSIDE; break;
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
		float value;
		std::memcpy(&value, &v.n23, sizeof(value));
		outAssembly = FormatAsm("MOVF %s = %.17g", context.operand(1).c_str(), static_cast<double>(value));
		break;
	}
	case NOP_LOADK:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("LOADK %s = K[%d]", context.operand(1).c_str(), v.n23);
		break;
	case NOP_MOV_MINUS:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("MOV  %s = -%s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_LOG_NOT:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("NOT  %s = !%s", context.operand(1).c_str(), context.operand(2).c_str());
		break;
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
		outAssembly = FormatAsm("%s %d%s,  %s > %s", (v.argFlag & NEOS_OP_COMPARE_INVERT) ? "JGR.NOT" : "JGR", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_GREAT_EQ:	// >=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("%s %d%s,  %s >= %s", (v.argFlag & NEOS_OP_COMPARE_INVERT) ? "JGE.NOT" : "JGE", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_LESS:		// <
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("%s %d%s,  %s < %s", (v.argFlag & NEOS_OP_COMPARE_INVERT) ? "JLS.NOT" : "JLS", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
		break;
	case NOP_JMP_LESS_EQ:	// <=
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("%s %d%s,  %s <= %s", (v.argFlag & NEOS_OP_COMPARE_INVERT) ? "JLE.NOT" : "JLE", v.n1, context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
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
	case NOP_STR_ADD:	// ..
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("SADD %s = ToStr%s + ToStr%s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
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
	case NOP_FMOV1:
		byteCount = OpFlagByteChars + 2 * 2;
		outAssembly = FormatAsm("MOV  %s = &%s", context.operand(1).c_str(), context.functionShort(v.n2).c_str());
		break;
	case NOP_FMOV2:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("MOV  %s.%s = &%s", context.operand(1).c_str(), context.operand(2).c_str(), context.functionShort(v.n3).c_str());
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
	case NOP_CLT_READ:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("READ %s = %s.%s", context.operand(3).c_str(), context.operand(1).c_str(), context.operand(2).c_str());
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
	case NOP_LIST_ALLOC:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("List Alloc %s, %d", context.operand(1).c_str(), v.n23);
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
	case NOP_CLT_READ_STATIC_STRING:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("READ.S %s = %s.%s", context.operand(3).c_str(), context.operand(1).c_str(), context.operand(2).c_str());
		break;
	case NOP_JMP_RANGE_INSIDE:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JRIN %d%s,  %s in range[%u]", v.n1,
			context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), (unsigned)(u16)v.n3);
		break;
	case NOP_JMP_RANGE_OUTSIDE:
		byteCount = OpFlagByteChars + 2 * 3;
		outAssembly = FormatAsm("JROT %d%s,  %s outside range[%u]", v.n1,
			context.jumpLabel(context.opIndex + 1 + v.n1).c_str(), context.operand(2).c_str(), (unsigned)(u16)v.n3);
		break;
	case NOP_CLT_WRITE_REF:
        byteCount = OpFlagByteChars + 2 * 3;
        outAssembly = FormatAsm("WREF %s.%s -> %s", context.operand(1).c_str(), context.operand(2).c_str(), context.operand(3).c_str());
        break;
	case NOP_CLT_READ_RESULT:
        byteCount = OpFlagByteChars + 2 * 3;
        outAssembly = FormatAsm("RRES %s = %s.%s", context.operand(3).c_str(), context.operand(1).c_str(), context.operand(2).c_str());
        break;
    case NOP_READ_APPLY:
        byteCount = OpFlagByteChars + 2 * 3;
        outAssembly = FormatAsm("READ_APPLY %s = %s.%s", context.operand(3).c_str(), context.operand(1).c_str(), context.operand(2).c_str());
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
		snprintf(ch, sizeof(ch), "%lld", (long long)c._int);
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
		if (program->IsGlobalVariable(v))
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


void NeoVMSystem::ProgramAddRef(CNeoVMProgram* pProgram)
{
	if (pProgram != nullptr)
		pProgram->AddRef();
}

void NeoVMSystem::ProgramRelease(CNeoVMProgram* pProgram)
{
	if (pProgram != nullptr)
		pProgram->Release();
}

CNeoVMProgram* NeoVMSystem::CompileToProgram(const NeoCompilerParam& param)
{
	CNArchive ar;
	if (NeoVMSystem::Compile(ar, param) == false)
		return nullptr;

	// 옛 CompileAndLoadVM 과 동일: putASM(진단) 시 컴파일 성공 + 코드 용량 출력.
	// (ANSI 색상 리터럴 인라인 — NeoParser.h 의존 회피, "\x1b[32m"=green, "\x1b[0m"=reset)
	if (param.putASM)
		printf("\x1b[32m" "Compile Success. Code : %d bytes !!" "\x1b[0m" "\n", ar.GetBufferOffset());

	return CNeoVMProgram::Create(ar.GetData(), ar.GetBufferOffset(), param.err);
}



};
