#pragma once

#include <functional>
#include "NeoVMInternal.h"

namespace NeoScript
{

class CNArchive;

struct SNeoVMHeader
{
	u32		_dwFileType;
	u32		_dwNeoVersion;

	int		_iFunctionCount;
	int		_iStaticVarCount;
	int		_iGlobalVarCount;
	int		_iExportVarCount;

	int		_iMainFunctionOffset;
	int		_iCodeSize;
	int		m_iDebugCount;
	int		m_iDebugOffset;

	u32		_dwFlag;
};

#define NEO_HEADER_FLAG_DEBUG				0x00000001
#define NEO_HEADER_FLAG_SINGLE_PRECISION	0x00000002


enum FUNCTION_TYPE : u8
{
	FUNT_NORMAL = 0,
	FUNT_EXPORT,
	FUNT_ANONYMOUS,
	FUNT_BUILT_IN,
};


struct SFunctionTable
{
	int					_codePtr;
	short				_argsCount;
	short				_localTempMax;
	int					_localVarCount;
	int					_localAddCount; // No Save (로드 시 계산)
	FUNCTION_TYPE		_funType;
};

// static(상수) 슬롯 1개. VarInfo 가 아니라 원시 값으로 들고 있는다.
// VAR_STRING 의 StringInfo 는 CNeoVMImpl 별 풀에서 나오고 같은 풀로 반납되어야 하므로,
// VM 경계를 넘어 공유되는 Program 이 소유하면 안 된다. 워커가 자기 VM 할당자로 실체화한다.
struct SStaticConst
{
	VAR_TYPE	_type = VAR_NONE;
	int			_int = 0;
	NS_FLOAT	_float = 0;
	bool		_bl = false;
	std::string	_str;   // VAR_STRING 전용
};

// switch/case 테이블. SStaticConst 와 같은 이유로 Program 전용 원시 표현을 쓴다.
// (VM pool / StringInfo / Var_Release / 스크립트 Map API 에 의존하지 않음)
// key 는 컴파일 타임 상수이고 strict type 비교한다.
// float 는 정확 비교가 불안정해서 case key 로 허용하지 않는다(컴파일 에러).
struct ProgramSwitchKey
{
	VAR_TYPE	_type = VAR_NONE;   // VAR_BOOL / VAR_INT / VAR_STRING
	int			_int = 0;
	bool		_bl = false;
	std::string	_str;               // VAR_STRING 전용 (UTF-8 바이트 그대로)
};

struct ProgramSwitchEntry
{
	ProgramSwitchKey	key;
	int					jumpOffset = 0;  // op 단위 relative (NOP_SWITCH 다음 op 기준)
	int					next = -1;       // 같은 버킷의 다음 엔트리. -1 = 끝
};

// 읽기 전용 해시 테이블. Program 과 수명이 같고 여러 VM/워커가 공유한다.
struct ProgramSwitchTable
{
	std::vector<int>				buckets;   // hash & mask → entry index (-1 = 빈 버킷)
	std::vector<ProgramSwitchEntry>	entries;
	int								defaultOffset = 0;

	void Build();                             // entries 로부터 buckets 구성
	int  Find(VarInfo* pKey) const;           // 매칭 실패/지원 외 타입이면 defaultOffset
};

// 컴파일된 스크립트 이미지.
// Create() 에서 1회 파싱 + 1회 코드 패치되고 이후 완전 불변이다.
// 여러 CNeoVMWorker / 여러 CNeoVMImpl / 여러 스레드가 refcount 로 공유한다.
class CNeoVMProgram
{
	std::atomic<int> _refCount{ 1 };

	CNeoVMProgram() {}
	~CNeoVMProgram() {}
	CNeoVMProgram(const CNeoVMProgram&) = delete;
	CNeoVMProgram& operator=(const CNeoVMProgram&) = delete;

	bool Load(CNArchive& ar, std::string* err);
	// PTRCALL2(native 이름) → NATIVECALL(native index) / intrinsic opcode 치환.
	// Create() 안에서 딱 한 번만 실행된다.
	void PatchNativeCalls();
	// 오퍼랜드가 전부 로컬인 op 를 _L 변형으로 치환한다(런타임 argFlag 테스트 제거).
	// 컴파일러/이미지는 건드리지 않고 로드 후 메모리 코드만 바꾸므로 저장 바이트코드는 그대로다.
	// 이 순회에서 구성 의존 opcode도 검증한다. false면 Program 생성 실패다.
	bool PatchLocalOps(std::string* err);

public:
	SNeoVMHeader				header;
	std::vector<SVMOperation>	code;              // 패치 완료 상태
	int							localOpCount = 0;  // PatchLocalOps 가 _L 로 바꾼 op 수(진단용)
	std::vector<SFunctionTable>	functions;
	std::map<std::string, int>	exportFunctions;   // 함수명 → 함수 ID
	std::map<std::string, int>	exportVariables;   // 전역 변수명 → 전역 슬롯 인덱스
	std::vector<SStaticConst>	staticValues;
	std::vector<ProgramSwitchTable>	switchTables;      // NOP_SWITCH.n1 이 이 배열의 인덱스

	// 디버그 정보 (IsDebugInfo() 일 때만 채워진다)
	std::vector<debug_info>		debugData;
	std::map<int, std::map<int, std::string>>	debugVarNames;   // 함수 ID → (스택 슬롯 → 이름)
	std::map<int, std::string>	debugGlobalNames;                // 전역 슬롯 → 이름
	std::map<int, std::string>	debugFunctionNames;              // 함수 ID → 이름

	int							byteSize = 0;      // 원본 이미지 바이트 수

	// 실패 시 nullptr. err 이 있으면 사유를 담는다. 반환된 프로그램의 refCount 는 1.
	static CNeoVMProgram* Create(const void* pBuffer, int iSize, std::string* err = nullptr);

	void AddRef()
	{
		_refCount.fetch_add(1, std::memory_order_relaxed);
	}
	void Release()
	{
		if (_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
			delete this;
	}

	NEOS_FORCEINLINE bool IsDebugInfo() const { return (header._dwFlag & NEO_HEADER_FLAG_DEBUG) != 0; }
	NEOS_FORCEINLINE const u8* GetCodeBegin() const { return (const u8*)code.data(); }
	NEOS_FORCEINLINE int GetCodeSize() const { return (int)(code.size() * sizeof(SVMOperation)); }
	NEOS_FORCEINLINE int GetGlobalSlotCount() const { return header._iStaticVarCount + header._iGlobalVarCount; }

	int FindFunction(const std::string& name) const
	{
		auto it = exportFunctions.find(name);
		if (it == exportFunctions.end())
			return -1;
		return (*it).second;
	}
	int FindGlobalVar(const std::string& name) const
	{
		auto it = exportVariables.find(name);
		if (it == exportVariables.end())
			return -1;
		return (*it).second;
	}
};

// opcode별 어셈블리 표기는 이 공통 포맷터만 사용한다. 컴파일러와 런타임은
// 각각 자기 심볼 표를 operand/function resolver로 공급한다.
struct DebugInstructionFormatContext
{
	const SVMOperation* operation = nullptr;
	// 점프 대상 계산의 기준점. 컴파일 타임은 함수 블록 기준 상대 인덱스, 런타임은 프로그램
	// 절대 인덱스다. 포맷터는 opIndex + 1 + n1 만 계산하고 표기는 jumpLabel 에 맡기므로
	// 어느 쪽이든 그대로 동작한다.
	int opIndex = 0;
	// argIndex(1,2,3) -> "[S.3]" / "[G.7 'text']" 같은 오퍼랜드 표기.
	std::function<std::string(int)> operand;
	// 함수 ID -> 이름. 컴파일 타임이 두 가지 표기를 쓰고 있어(전체이름 vs 짧은 이름)
	// 출력을 그대로 보존하려고 둘로 나눠 둔다. 런타임은 같은 것을 넣어도 된다.
	std::function<std::string(int)> functionFull;    // NOP_CALL 용
	std::function<std::string(int)> functionShort;   // NOP_FMOV1/FMOV2 용
	// 점프 대상 op 인덱스 -> " 'go_3'" 같은 라벨. 라벨이 없으면 빈 문자열.
	std::function<std::string(int)> jumpLabel;
};

// 어셈블리 한 줄(개행 없음)을 만든다. outByteCount 는 바이트 덤프에 표시할
// opcode + arg flag + 유효 operand 바이트 수다.
// 반환값 false = 이 포맷터가 모르는 opcode(문자열에는 "?? op(N)" 이 들어간다).
// 에러로 볼지 말지는 호출자가 정한다 — 컴파일 타임은 컴파일 에러, 런타임은 표시만.
bool FormatDebugOperation(const DebugInstructionFormatContext& context, std::string& outAssembly,
	int* outByteCount = nullptr);

// 로드/패치가 끝난 Program 이미지용 공통 포맷터 어댑터.
void FormatDebugInstruction(const CNeoVMProgram& program, int opIndex,
	std::string& outByteCode, std::string& outAssembly);

};
