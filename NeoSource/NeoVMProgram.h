#pragma once

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

public:
	SNeoVMHeader				header;
	std::vector<SVMOperation>	code;              // 패치 완료 상태
	std::vector<SFunctionTable>	functions;
	std::map<std::string, int>	exportFunctions;   // 함수명 → 함수 ID
	std::map<std::string, int>	exportVariables;   // 전역 변수명 → 전역 슬롯 인덱스
	std::vector<SStaticConst>	staticValues;

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

};
