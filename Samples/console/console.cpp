#include "stdafx.h"
#include "HttpTransport.h"
#include "console.h"
#include "../../NeoSource/Neo.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API (RunFile 등이 사용; Neo.h 와 공존)
#include "../../ThirdParty/yyjson/yyjson.h"
#include <cctype>
#include <limits>
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <cstdlib>
#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

using namespace NeoScript;

static bool Utf8PathToWide(const std::string& path, std::wstring& wide)
{
    if (path.empty())
    {
        wide.clear();
        return true;
    }
    if (path.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        return false;
    const int size = static_cast<int>(path.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), size, nullptr, 0);
    if (count == 0)
        return false;
    wide.resize(count);
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), size, &wide[0], count) == count;
}

static std::string FullPathOrSelf(const std::string& path)
{
    std::wstring wide;
    if (!Utf8PathToWide(path, wide))
        return path;
    // UTF-8 경로를 유지하면서 현재 디렉터리와 . / .. 를 Windows 규칙으로 해석한다.
    const DWORD capacity = GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
    if (capacity == 0)
        return path;
    std::vector<wchar_t> buffer(capacity);
    const DWORD length = GetFullPathNameW(wide.c_str(), capacity, buffer.data(), nullptr);
    if (length == 0 || length >= capacity || length > static_cast<DWORD>((std::numeric_limits<int>::max)()))
        return path;
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(length),
        nullptr, 0, nullptr, nullptr);
    if (count == 0)
        return path;
    std::string full(count, '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(length),
        &full[0], count, nullptr, nullptr) != count)
        return path;
    return full;
}

class CNeoLoader : public INeoLoader
{
	std::string m_libPath = "../../Lib/";
public:
	void SetLibPath(const std::string& libPath)
	{
		if (libPath.empty())
			return;
		m_libPath = FullPathOrSelf(libPath);
		char last = m_libPath[m_libPath.size() - 1];
		if (last != '/' && last != '\\')
			m_libPath += "/";
	}
	virtual bool        Load(const char* pFileName, void*& pBuffer, int& iLen) override
	{
		std::wstring widePath;
		if (pFileName == nullptr || !Utf8PathToWide(pFileName, widePath))
			return false;
		FILE* fp = NULL;
		int error_t = _wfopen_s(&fp, widePath.c_str(), L"rb");
		if (error_t != 0)
			return false;

		fseek(fp, 0, SEEK_END);
		int iFileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		pBuffer = new BYTE[iFileSize + 2];
		fread(pBuffer, iFileSize, 1, fp);
		fclose(fp);

		iLen = iFileSize;
		return true;
	}
	virtual void        Unload(const char* pFileName, void* pBuffer, int iLen) override
	{
		delete [] pBuffer;
	}
	virtual const char* GetLibPath() override
	{
		return m_libPath.c_str();
	}
};

std::string getKeyString()
{
	std::string str;
	while (true)
	{
		int r = _getche();
		if (r == '\r' || r == '\n') // Enter
		{
			if(str.empty() == false)
				return str;
			continue;
		}
		if (r == 27) // Escape
			return "";

		str += (char)r;
	}
	return 0;
}

int SAMPLE_callback(INeoLoader* pLoader, std::string filename);
int SAMPLE_map_callback(INeoLoader* pLoader, std::string filename);
int SAMPLE_9_times(INeoLoader* pLoader, std::string filename);
int SAMPLE_slice_run(INeoLoader* pLoader, std::string filename);
int SAMPLE_time_limit(INeoLoader* pLoader, std::string filename);
int SAMPLE_etc(INeoLoader* pLoader, std::string filename, const char* pFunctionName);
int SAMPLE_cycle_ref(INeoLoader* pLoader, std::string filename);

static void PrintSampleList()
{
	printf("hello\n");
	printf("performance\n");
	printf("callback\n");
	printf("map_callback\n");
	printf("9_times\n");
	printf("string\n");
	printf("list\n");
	printf("map\n");
	printf("contailer\n");
	printf("slice_run\n");
	printf("time_limit\n");
	printf("divide_by_zero\n");
	printf("delegate\n");
	printf("coroutine\n");
	printf("module\n");
	printf("http\n");
	printf("regression\n");
	printf("literal_totype\n");
	printf("cycle\n");
	printf("regex\n");
}

static std::string s_path = "../../TestScript/";
static int RunSample(INeoLoader* pLoader, const std::string& key)
{
	if (key == "0" || key == "hello") return SAMPLE_etc(pLoader, s_path + "hello.ns", nullptr);
	if (key == "1" || key == "performance" || key == "performace") return SAMPLE_etc(pLoader, s_path + "performance.ns", nullptr);
	if (key == "2" || key == "callback") return SAMPLE_callback(pLoader, s_path + "callback.ns");
	if (key == "3" || key == "map_callback") return SAMPLE_map_callback(pLoader, s_path + "map_callback.ns");
	if (key == "4" || key == "9_times") return SAMPLE_9_times(pLoader, s_path + "9_times.ns");
	if (key == "5" || key == "string") return SAMPLE_etc(pLoader, s_path + "string.ns", nullptr);
	if (key == "6" || key == "list") return SAMPLE_etc(pLoader, s_path + "list.ns", nullptr);
	if (key == "7" || key == "map") return SAMPLE_etc(pLoader, s_path + "map.ns", nullptr);
	if (key == "8" || key == "contailer") return SAMPLE_etc(pLoader, s_path + "contailer.ns", nullptr);
	if (key == "9" || key == "slice_run") return SAMPLE_slice_run(pLoader, s_path + "slice_run.ns");
	if (key == "10" || key == "time_limit") return SAMPLE_time_limit(pLoader, s_path + "time_limit.ns");
	if (key == "11" || key == "divide_by_zero") return SAMPLE_etc(pLoader, s_path + "etc.ns", "divide_by_zero");
	if (key == "12" || key == "delegate") return SAMPLE_etc(pLoader, s_path + "delegate.ns", nullptr);
	if (key == "13" || key == "coroutine") return SAMPLE_etc(pLoader, s_path + "coroutine.ns", "test");
	if (key == "14" || key == "module") return SAMPLE_etc(pLoader, s_path + "module.ns", nullptr);
	if (key == "15" || key == "http") return SAMPLE_etc(pLoader, s_path + "http.ns", nullptr);
	if (key == "16" || key == "regression") return SAMPLE_etc(pLoader, s_path + "compiler_regression.ns", nullptr);
	if (key == "17" || key == "literal_totype") return SAMPLE_etc(pLoader, s_path + "literal_totype.ns", nullptr);
	if (key == "18" || key == "cycle") return SAMPLE_cycle_ref(pLoader, s_path + "cycle_ref.ns");

	if (key == "19" || key == "regex") return SAMPLE_etc(pLoader, s_path + "regex.ns", nullptr);

	printf("unknown sample: %s\n", key.c_str());
	return -1;
}

static int RunSmokeSamples(INeoLoader* pLoader)
{
	const char* samples[] = { "hello", "string", "list", "map", "delegate", "coroutine", "regression", "literal_totype" };
	for (const char* sample : samples)
	{
		printf("\n[smoke] %s\n", sample);
		int r = RunSample(pLoader, sample);
		if (r != 0)
			return r;
	}
	return 0;
}

static double ElapsedMs(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
{
	return std::chrono::duration<double, std::milli>(end - start).count();
}

static int RunFile(CNeoLoader* pLoader, const std::string& filename, bool putASM, bool debug,
	NeoScript::SNeoVMAllocStats* liveStats = nullptr, const std::vector<std::string>& hostGlobals = {})
{
	void* pFileBuffer = nullptr;
	int iFileLen = 0;
	if (pLoader->Load(filename.c_str(), pFileBuffer, iFileLen) == false)
	{
		printf("file read error: %s\n", filename.c_str());
		return -1;
	}

	RuntimeDesc rd;
	rd.httpTransport = CreateConsoleHttpTransport();
	// print 핸들러 미설정: 임의 스크립트를 그대로 실행하므로 엔진 내장 io_print 에 맡긴다.
	// (1인자 print 는 개행, 2인자 print(x,"") 는 개행 없이 이어붙임 — printFn 을 걸면 이 구분 소실)
	rd.nativeLoader = pLoader;                 // import 해석
	IRuntime* rt = CreateRuntime(rd);
	// 헤드리스 호스트의 명시적 전역만 모든 모듈에 제공한다. 스크립트의
	// 일반 var를 다시 공유하지 않고, 본문에서는 이 슬롯에 시험 대역을 대입한다.
	for (const std::string& name : hostGlobals)
	{
		NativeObjectDesc object;
		object.name = name;
		if (!rt->RegisterObject(object))
		{
			printf("invalid host global: %s\n", name.c_str());
			pLoader->Unload(filename.c_str(), pFileBuffer, iFileLen);
			DestroyRuntime(rt);
			return -1;
		}
	}
	rt->FreezeBindings();

	CompileDesc cd;
	cd.source = StringView((const char*)pFileBuffer, (size_t)iFileLen);
	cd.sourceName = filename.c_str();
	cd.includeDebugInfo = debug;
	cd.emitAsm = putASM;   // --asm 이면 컴파일 시 디스어셈블 덤프
	CompileResult cr = rt->Compile(cd);
	pLoader->Unload(filename.c_str(), pFileBuffer, iFileLen);

	if (!cr.program)
	{
		if (!cr.error.message.empty())
			printf("%s\n", cr.error.message.c_str());
		DestroyRuntime(rt);
		return -1;
	}

	int exitCode = 0;
	InstanceHandle inst = rt->CreateInstance(cr.program); // 최상위(전역 초기화) 실행
	StringView err;
	if (rt->TakeLastError(err))
	{
		printf("Error - VM Call : %.*s\n", (int)err.size(), err.data());
		exitCode = -1;
	}

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	// Runtime 파괴 전의 live registry를 읽는다. 그래야 실행 컨텍스트 풀에 남은
	// closure 같은 장기 누수가 VM 소멸자의 강제 sweep으로 가려지지 않는다.
	if (liveStats)
		NeoScript::GetNeoVMAllocStats(*liveStats);
	DestroyRuntime(rt);
	return exitCode;
}

static int RunBenchCase(const char* name, const char* source, const char* functionName, int arg, int iterations, void* nativeLoader)
{
	std::string src = source;

	auto compileStart = std::chrono::steady_clock::now();
	RuntimeDesc rd;
	rd.httpTransport = CreateConsoleHttpTransport();
	rd.printFn = [](StringView s) { printf("%.*s\n", (int)s.size(), s.data()); };
	rd.nativeLoader = nativeLoader;                 // import math 해석
	IRuntime* rt = CreateRuntime(rd);
	rt->FreezeBindings();

	CompileDesc cd;
	cd.source = StringView(src.data(), src.size());
	cd.sourceName = name;
	CompileResult cr = rt->Compile(cd);
	if (!cr.program)
	{
		printf("[bench] %-14s compile failed: %s\n", name, cr.error.message.c_str());
		DestroyRuntime(rt);
		return -1;
	}
	InstanceHandle inst = rt->CreateInstance(cr.program); // 최상위(import 해석)
	auto compileEnd = std::chrono::steady_clock::now();

	double result = 0;
	StringView err;
	auto runStart = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i)
	{
		Invocation call = rt->Call(inst, functionName);   // 스코프 = 반복마다 소멸
		call.argInt(arg).invoke();
		if (rt->TakeLastError(err))
		{
			printf("[bench] %-14s runtime error: %.*s\n", name, (int)err.size(), err.data());
			rt->DestroyInstance(inst);
			rt->DestroyProgram(cr.program);
			DestroyRuntime(rt);
			return -1;
		}
		result = call.retFloat();
	}
	auto runEnd = std::chrono::steady_clock::now();

	printf("[bench] %-14s compile=%8.3f ms run=%8.3f ms iter=%d arg=%d result=%g\n",
		name,
		ElapsedMs(compileStart, compileEnd),
		ElapsedMs(runStart, runEnd),
		iterations,
		arg,
		result);

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	return 0;
}

static int RunBenchmarks(CNeoLoader* pLoader)
{
	struct BenchCase
	{
		const char* name;
		const char* source;
		const char* functionName;
		int arg;
		int iterations;
	};

	const BenchCase cases[] = {
		{
			"vec_churn",
			R"(
fun V3(var x, var y, var z) { return [x, y, z]; }
fun AddV3(var a, var b) { return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]; }
export fun VecChurn(var n)
{
	var sum = 0.0;
	for(var i in 0, n, 1)
	{
		var a = V3(i, i + 1.0, i + 2.0);
		var b = V3(i * 0.5, i * 0.25, i * 0.125);
		var c = AddV3(a, b);
		sum = sum + c[0] + c[1] + c[2];
	}
	return sum;
}
)",
			"VecChurn",
			200000,
			5
		},
		{
			// vec_churn 과 동일하지만 힙 리스트 대신 math.Vector3 인라인 값타입 사용.
			// 값타입 할당 제거 효과의 실측 (vec_churn 대비 배율 = 값타입 이득).
			"vec_churn_vt",
			R"(
import math;
fun V3(var x, var y, var z) { return math.Vector3(x, y, z); }
fun AddV3(var a, var b) { return a + b; }
export fun VecChurnVT(var n)
{
	var sum = 0.0;
	for(var i in 0, n, 1)
	{
		var a = V3(i, i + 1.0, i + 2.0);
		var b = V3(i * 0.5, i * 0.25, i * 0.125);
		var c = AddV3(a, b);
		sum = sum + c[0] + c[1] + c[2];
	}
	return sum;
}
)",
			"VecChurnVT",
			200000,
			5
		},
		{
			"loop_sum",
			R"(
export fun LoopSum(var n)
{
	var sum = 0.0;
	for(var i in 0, n, 1)
		sum += i;
	return sum;
}
)",
			"LoopSum",
			200000,
			5
		},
		{
			"math_sqrt",
			R"(
import math;
export fun MathSqrt(var n)
{
	var sum = 0.0;
	for(var i in 0, n, 1)
		sum += math.sqrt(i);
	return sum;
}
)",
			"MathSqrt",
			100000,
			3
		},
		{
			"call_loop",
			R"(
fun inc(var x)
{
	return x + 1;
}
export fun CallLoop(var n)
{
	var sum = 0.0;
	for(var i in 0, n, 1)
		sum += inc(i);
	return sum;
}
)",
			"CallLoop",
			100000,
			5
		},
		{
			"map_loop",
			R"(
export fun MapLoop(var n)
{
	var m = {};
	for(var i in 0, n, 1)
		m[i] = i;
	var sum = 0.0;
	foreach(var k, v in m)
		sum += v;
	return sum;
}
)",
			"MapLoop",
			30000,
			3
		},
		{
			"map_str_read",
			R"(
export fun MapStringRead(var n)
{
	var m = {};
	m["alpha"] = 1;
	m["bravo"] = 2;
	m["charlie"] = 3;
	m["delta"] = 4;
	m["echo"] = 5;
	m["foxtrot"] = 6;
	m["golf"] = 7;
	m["hotel"] = 8;
	var sum = 0.0;
	for(var i in 0, n, 1)
	{
		sum = sum + m["alpha"];
		sum = sum + m["hotel"];
		sum = sum + m["charlie"];
		sum = sum + m["foxtrot"];
	}
	return sum;
}
)",
			"MapStringRead",
			50000,
			5
		},
		{
			"add4_only", // 분해측정: map_str_read 에서 read 를 뺀 것 (루프 + add 4회)
			R"(
export fun Add4Only(var n)
{
	var sum = 0.0;
	for(var i in 0, n, 1)
	{
		sum = sum + 1;
		sum = sum + 8;
		sum = sum + 3;
		sum = sum + 6;
	}
	return sum;
}
)",
			"Add4Only",
			50000,
			5
		},
		{
			"read4_only", // 분해측정: map read 4회만 (add 없음)
			R"(
export fun Read4Only(var n)
{
	var m = {};
	m["alpha"] = 1;
	m["bravo"] = 2;
	m["charlie"] = 3;
	m["delta"] = 4;
	m["echo"] = 5;
	m["foxtrot"] = 6;
	m["golf"] = 7;
	m["hotel"] = 8;
	var t = 0;
	for(var i in 0, n, 1)
	{
		t = m["alpha"];
		t = m["hotel"];
		t = m["charlie"];
		t = m["foxtrot"];
	}
	return t;
}
)",
			"Read4Only",
			50000,
			5
		},
	};

	for (const BenchCase& bench : cases)
	{
		if (RunBenchCase(bench.name, bench.source, bench.functionName, bench.arg, bench.iterations, pLoader) != 0)
			return -1;
	}
	return 0;
}

// v2 IDebugger 로 이전한 디버그 스모크. 모든 정지는 top-level 실행(RunGlobalInit)이
// Suspended 를 반환하면 사후 검사 → Continue+Run 으로 재개하는 모델을 쓴다.
// IDebugger는 ExecuteTop/ResumeTop 실행 모델을 사용한다.
struct V2DebugSmokeListener : IDebugListener
{
	int stopCount = 0;
	DebugLocation lastLocation;
	DebugStopReason lastReason = DebugStopReason::Breakpoint;
	void OnStopped(InstanceHandle, const DebugLocation& location, DebugStopReason reason) override
	{
		++stopCount;
		lastLocation = location;
		lastReason = reason;
		printf("[debug-smoke] stopped line=%d op=%d reason=%d\n", location.line, location.opIndex, (int)reason);
	}
};

// 빈 페이지 회수 측정. 대량 생성 -> 전량 소멸 -> TrimMemory 로 페이지가 실제로
// OS 에 돌아가는지 본다. (예전 전역 free 리스트 구조에서는 0 바이트만 나온다)
static int RunPoolTrim(INeoLoader* pLoader)
{
	RuntimeDesc rd;
	rd.httpTransport = CreateConsoleHttpTransport();
	rd.nativeLoader = pLoader;
	rd.printFn = [](StringView s) { printf("%.*s\n", (int)s.size(), s.data()); };
	IRuntime* rt = CreateRuntime(rd);
	rt->FreezeBindings();

	const char* source =
		"var keep = [];\n"
		"export fun Build()\n"
		"{\n"
		"    for(var i in 0, 30000, 1)\n"
		"    {\n"
		"        var m = { \"path\": \"map/area/segment/\" .. toint(i) .. \"/mesh.bin\", \"id\": toint(i) };\n"
		"        keep.append(m);\n"
		"    }\n"
		"    return keep.len();\n"
		"}\n"
		"export fun Drop()\n"
		"{\n"
		"    keep = [];\n"
		"    return 0;\n"
		"}\n";

	CompileDesc cd; cd.source = source; cd.sourceName = "pooltrim.ns";
	CompileResult cr = rt->Compile(cd);
	if (!cr.program) { printf("[pooltrim] compile failed: %s\n", cr.error.message.c_str()); DestroyRuntime(rt); return -1; }
	InstanceHandle inst = rt->CreateInstance(cr.program);

	auto dump = [&](const char* tag)
	{
		AllocStats s; GetAllocStats(s);
		printf("[pooltrim] %-14s pool=%8lld KB  stringIdle=%7lld KB  (str=%d map=%d list=%d)\n",
			tag, s.poolBytes / 1024, s.stringIdleBytes / 1024, s.strings, s.maps, s.lists);
		return s.poolBytes;
	};

	auto PoolBytesNow = [&]() { AllocStats s; GetAllocStats(s); return s.poolBytes; };

	dump("start");
	{ Invocation c = rt->Call(inst, "Build"); c.invoke(); }
	const long long peak = dump("after build");
	{ Invocation c = rt->Call(inst, "Drop"); c.invoke(); }
	const long long dropped = dump("after drop");

	// force=true 는 보유 시간과 스로틀을 모두 무시한다 — 5초로 두고도 즉시 회수돼야 한다.
	rt->SetEmptyPageHoldSeconds(5.0f);
	const long long freed = rt->TrimMemory(true);
	const long long trimmed = dump("after trim(force)");

	printf("[pooltrim] peak=%lld KB -> trim=%lld KB  (freed %lld KB, %.1f%% of peak)\n",
		peak / 1024, trimmed / 1024, freed / 1024,
		peak > 0 ? (100.0 * (double)freed / (double)peak) : 0.0);

	// 보유 시간은 force=false 경로에서 지켜져야 한다. hold=1s 로 두고
	// (a) 스로틀만 지난 시점에는 회수 0, (b) 보유 시간까지 지나면 회수 > 0.
	{ Invocation c = rt->Call(inst, "Build"); c.invoke(); }
	{ Invocation c = rt->Call(inst, "Drop"); c.invoke(); }
	rt->SetEmptyPageHoldSeconds(1.0f);
	std::this_thread::sleep_for(std::chrono::milliseconds(300));   // 스로틀(250ms) 초과, 보유(1s) 미만
	const long long tooEarly = rt->TrimMemory(false);
	// 보유 시계는 "Collect 가 빈 걸 처음 본 시점"부터 흐른다(Confer 에서 시계를 안 읽으려는
	// 지연 기록). 위 호출이 도장을 찍었으므로 거기서부터 1초를 더 기다린다.
	std::this_thread::sleep_for(std::chrono::milliseconds(1100));
	const long long afterHold = rt->TrimMemory(false);
	printf("[pooltrim] hold=1s  보유 전 회수=%lld bytes (0 이어야 정상), 보유 후 회수=%lld bytes\n",
		tooEarly, afterHold);

	// --- 증분 회수: 매 프레임 조금씩 돌려주는 경로 ---
	// 다시 채웠다 비우고, 보유 시간 0 + 프레임 루프로 TrimMemory(false) 를 돌린다.
	// 확인할 것: (a) 결국 다 돌아오는가, (b) 한 호출이 예산 장수를 넘지 않는가,
	//            (c) 그래서 호출 하나의 시간이 튀지 않는가.
	{ Invocation c = rt->Call(inst, "Build"); c.invoke(); }
	{ Invocation c = rt->Call(inst, "Drop"); c.invoke(); }
	rt->SetEmptyPageHoldSeconds(0.0f);       // 보유 대기 없이 회수 속도만 본다
	rt->SetTrimPagesPerCall(4);

	const long long incStart = PoolBytesNow();
	long long worstCall = 0;                  // 한 호출이 해제한 최대 바이트
	double worstUs = 0.0;                     // 한 호출의 최대 소요 시간
	double totalUs = 0.0;
	int calls = 0;
	while (calls < 100000)
	{
		const auto t0 = std::chrono::steady_clock::now();
		const long long f = rt->TrimMemory(false);
		const double us = std::chrono::duration<double, std::micro>(
			std::chrono::steady_clock::now() - t0).count();
		++calls;
		totalUs += us;
		if (us > worstUs) worstUs = us;
		if (f > worstCall) worstCall = f;
		if (f == 0) break;                    // 더 돌려줄 게 없다
	}
	const long long incEnd = PoolBytesNow();

	// 예산 4장 * 페이지 최대 크기(가장 큰 풀 기준)를 넘는 호출이 있으면 증분이 깨진 것이다.
	const long long kMaxPageBytes = 16 * 1024;      // 현재 설정에서 가장 큰 페이지가 이보다 작다
	const bool budgetKept = worstCall <= 4 * kMaxPageBytes;
	printf("[pooltrim] 증분: %lld KB -> %lld KB, 호출 %d 회, 최대 1회 해제=%lld bytes,\n"
	       "           최대 1회 시간=%.1f us, 평균=%.2f us, 예산준수=%s\n",
		incStart / 1024, incEnd / 1024, calls, worstCall,
		worstUs, totalUs / (calls > 0 ? calls : 1), budgetKept ? "OK" : "FAIL");

	// 빈 페이지가 없을 때의 호출 비용(엔진이 매 프레임 부르는 정상 상태).
	double idleUs = 0.0;
	for (int i = 0; i < 1000; ++i)
	{
		const auto t0 = std::chrono::steady_clock::now();
		rt->TrimMemory(false);
		idleUs += std::chrono::duration<double, std::micro>(
			std::chrono::steady_clock::now() - t0).count();
	}
	printf("[pooltrim] 정상 상태(빈 페이지 없음) 1회 비용 = %.4f us\n", idleUs / 1000.0);

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	return (dropped >= peak && freed > 0 && tooEarly == 0 && afterHold > 0
		&& budgetKept && incEnd < incStart) ? 0 : -1;
}

static int RunDebugSmoke()
{
	RuntimeDesc rd;
	rd.httpTransport = CreateConsoleHttpTransport();
	// import 를 쓰는 케이스가 있어 loader 를 붙인다(없으면 모듈 해석이 실패한다).
	CNeoLoader debugSmokeLoader;
	rd.nativeLoader = &debugSmokeLoader;
	IRuntime* rt = CreateRuntime(rd);
	rt->FreezeBindings();
	IDebugger* dbg = rt->GetDebugger();

	// 소스 컴파일 + 인스턴스 생성(전역 init 미실행: BP 걸고 RunGlobalInit 으로 진입).
	auto compileInst = [&](const char* src, const char* name, ProgramHandle& prog, InstanceHandle& inst) -> bool
	{
		CompileDesc cd; cd.source = src; cd.sourceName = name; cd.includeDebugInfo = true;
		CompileResult cr = rt->Compile(cd);
		if (!cr.program) { printf("[debug-smoke] compile failed: %s\n", cr.error.message.c_str()); return false; }
		prog = cr.program;
		InstanceDesc idesc; idesc.runGlobalInit = false;
		inst = rt->CreateInstance(prog, idesc);
		return true;
	};
	auto setBps = [&](InstanceHandle inst, std::initializer_list<int> lines)
	{
		std::vector<DebugBreakpoint> bps;
		for (int l : lines) { DebugBreakpoint b; b.file = 0; b.line = l; bps.push_back(b); }
		dbg->SetBreakpoints(inst, bps);
	};
	auto resumeToEnd = [&](InstanceHandle inst) // BP 제거 후 완주
	{
		setBps(inst, {});
		dbg->Continue(inst);
		dbg->Run(inst);
	};
	auto fail = [&](const char* msg) -> int { printf("%s\n", msg); DestroyRuntime(rt); return -1; };

	// 1) 기본 브레이크포인트 + 프레임/변수 조회
	{
		const char* source =
			"var a = 1;\n"
			"var b = 2;\n"
			"var c = a + b;\n"
			"print(c);\n";
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(source, "basic.ns", prog, inst)) { DestroyRuntime(rt); return -1; }

		std::vector<int> executableLines;
		dbg->GetExecutableLines(prog, executableLines);
		if (std::find(executableLines.begin(), executableLines.end(), 1) == executableLines.end() ||
			std::find(executableLines.begin(), executableLines.end(), 4) == executableLines.end())
			return fail("[debug-smoke] executable line metadata failed");

		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 3 });
		rt->RunGlobalInit(inst);
		if (listener.stopCount != 1 || dbg->IsPaused(inst) == false || listener.lastLocation.line != 3)
			return fail("[debug-smoke] breakpoint failed");

		std::vector<DebugStackFrame> frames;
		dbg->GetStackTrace(inst, frames);
		std::vector<DebugVariable> vars;
		dbg->GetFrameVariables(inst, 0, vars);
		printf("[debug-smoke] frames=%d vars=%d\n", (int)frames.size(), (int)vars.size());
		if (frames.empty() || vars.empty()) return fail("[debug-smoke] frames/vars empty");
		if (frames[0].functionName.empty()) return fail("[debug-smoke] function name failed");
		bool foundA = false, foundB = false;
		for (const DebugVariable& var : vars) { if (var.name == "a") foundA = true; if (var.name == "b") foundB = true; }
		if (!foundA || !foundB) return fail("[debug-smoke] variable names failed");

		dbg->Continue(inst);
		dbg->Run(inst);
		if (dbg->IsPaused(inst)) return fail("[debug-smoke] continue failed");
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 2) 컬렉션 변수(맵) children 확장
	{
		const char* collectionSource =
			"var data = { \"name\": \"Neo\", \"items\": [1, 2] };\n"
			"var marker = 1;\n";
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(collectionSource, "collection.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 2 });
		rt->RunGlobalInit(inst);
		std::vector<DebugVariable> collectionVars;
		dbg->GetFrameVariables(inst, 0, collectionVars);
		const DebugVariable* data = nullptr;
		for (const DebugVariable& var : collectionVars) { if (var.name == "data") { data = &var; break; } }
		bool foundName = false, foundItems = false, itemsExpanded = false;
		if (data != nullptr)
		{
			for (const DebugVariable& child : data->children)
			{
				if (child.name == "[\"name\"]" && child.value == "Neo") foundName = true;
				if (child.name == "[\"items\"]") { foundItems = true; itemsExpanded = child.children.size() == 2; }
			}
		}
		if (dbg->IsPaused(inst) == false || data == nullptr || !foundName || !foundItems || !itemsExpanded)
		{
			printf("[debug-smoke] collection variables failed paused=%d data=%d name=%d items=%d expanded=%d\n",
				dbg->IsPaused(inst) ? 1 : 0, data ? 1 : 0, foundName ? 1 : 0, foundItems ? 1 : 0, itemsExpanded ? 1 : 0);
			DestroyRuntime(rt); return -1;
		}
		resumeToEnd(inst);
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	const char* stepSource =
		"fun add(var x) {\n"
		"    var y = x + 1;\n"
		"    return y;\n"
		"}\n"
		"var a = 1;\n"
		"var b = add(a);\n"
		"var c = b + 1;\n";

	// 3) StepOver (line 6 -> 7)
	{
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(stepSource, "step.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 6 });
		rt->RunGlobalInit(inst);
		if (dbg->IsPaused(inst) == false || listener.lastLocation.line != 6)
			return fail("[debug-smoke] step breakpoint failed");
		dbg->StepOver(inst);
		dbg->Run(inst);
		if (dbg->IsPaused(inst) == false || dbg->GetLocation(inst).line != 7)
			return fail("[debug-smoke] step over failed");
		resumeToEnd(inst);
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 4) StepOut (add 내부 line 2 -> 호출자 callDepth 0)
	{
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(stepSource, "step.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 2 });
		rt->RunGlobalInit(inst);
		if (dbg->IsPaused(inst) == false || listener.lastLocation.line != 2)
			return fail("[debug-smoke] step-out breakpoint failed");
		dbg->StepOut(inst);
		dbg->Run(inst);
		if (dbg->IsPaused(inst) == false || dbg->GetLocation(inst).callDepth != 0)
		{
			printf("[debug-smoke] step out failed line=%d depth=%d\n",
				dbg->GetLocation(inst).line, dbg->GetLocation(inst).callDepth);
			DestroyRuntime(rt); return -1;
		}
		resumeToEnd(inst);
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 5) export 함수 내부 BP. 구판은 host worker->Start() 로 호출했으나 v2 invoke 는
	//    BP 정지를 지원하지 않으므로, 스크립트 최상위에서 Time9(9) 를 호출해 RunGlobalInit 이
	//    함수 내부 BP 를 잡게 한다(동일 커버리지: 인자 n==9 확인, local/temp 노출 안 됨).
	{
		const char* exportSource =
			"export fun Time9(var n)\n"
			"{\n"
			"    for(var i in 1, 10, 1)\n"
			"    {\n"
			"        var value = n * i;\n"
			"        print(n..\" x \"..i..\" = \"..value);\n"
			"    }\n"
			"}\n"
			"Time9(9);\n";
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(exportSource, "export.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 5 });
		rt->RunGlobalInit(inst);
		if (dbg->IsPaused(inst) == false || listener.lastLocation.line != 5)
			return fail("[debug-smoke] export breakpoint failed");
		std::vector<DebugVariable> exportVars;
		dbg->GetFrameVariables(inst, 0, exportVars);
		bool foundN = false, foundTemp = false;
		for (const DebugVariable& var : exportVars)
		{
			if (var.name == "n" && var.value == "9") foundN = true;
			if (var.name.rfind("local", 0) == 0 || var.name.rfind("temp", 0) == 0) foundTemp = true;
		}
		if (!foundN || foundTemp)
		{
			printf("[debug-smoke] export vars failed n=%d temp=%d\n", foundN ? 1 : 0, foundTemp ? 1 : 0);
			DestroyRuntime(rt); return -1;
		}
		resumeToEnd(inst);
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 6) 런타임 예외(0 나누기). 예외는 재개 가능한 정지가 아니라 종료이므로 paused 를 요구하지
	//    않고, 리스너가 Exception 사유로 정지 보고 + VM 에러 존재를 확인한다.
	{
		const char* errorSource =
			"var a = 1;\n"
			"var b = 0;\n"
			"var c = a / b;\n";
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(errorSource, "error.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		rt->RunGlobalInit(inst);
		StringView emsg;
		bool hasErr = rt->TakeLastError(emsg);
		if (listener.stopCount == 0 || listener.lastReason != DebugStopReason::Exception || hasErr == false)
		{
			printf("[debug-smoke] exception failed stops=%d reason=%d err=%d\n",
				listener.stopCount, (int)listener.lastReason, hasErr ? 1 : 0);
			DestroyRuntime(rt); return -1;
		}
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 6.5) import 한 모듈 함수를 StepOver 로 연속해서 넘기기.
	//      게임에서 보고된 형태 그대로다: 무거운 모듈 함수를 F10 으로 넘긴 "다음" F10 에서
	//      두 번째 모듈 함수의 인자가 사라졌다. 그래서 두 번 연속 StepOver 한다.
	//      확인 항목: (a) 각 스텝이 다음 줄에 서는가 (b) 콜 깊이가 돌아오는가
	//                (c) 두 번째 모듈 함수가 인자를 제대로 받았는가(y == 4)
	{
		const char* moduleStepSource =
			"import dbgmod as dm;\n"
			"fun Run()\n"
			"{\n"
			"    var x = 3;\n"
			"    var n = dm.Heavy(40, 1.5);\n"
			"    var y = dm.Take(x);\n"
			"    var z = y + n;\n"
			"    return z;\n"
			"}\n"
			"var result = Run();\n"
			"print(result);\n";
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(moduleStepSource, "modstep.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 5 });   // var n = dm.Heavy(...);  <- 무거운 모듈 함수
		rt->RunGlobalInit(inst);
		if (dbg->IsPaused(inst) == false || listener.lastLocation.line != 5)
			return fail("[debug-smoke] module step breakpoint failed");
		const int depthBefore = dbg->GetLocation(inst).callDepth;

		// 1st F10: 무거운 모듈 함수를 넘긴다 -> 6줄
		dbg->StepOver(inst);
		dbg->Run(inst);
		if (dbg->IsPaused(inst) == false || dbg->GetLocation(inst).line != 6)
		{
			printf("[debug-smoke] module step over #1 landed line=%d (expected 6)\n",
				dbg->GetLocation(inst).line);
			DestroyRuntime(rt); return -1;
		}

		// 2nd F10: 이어서 두 번째 모듈 함수를 넘긴다 -> 7줄
		dbg->StepOver(inst);
		dbg->Run(inst);
		StringView stepErr;
		if (rt->TakeLastError(stepErr))
		{
			printf("[debug-smoke] module step over #2 error: %.*s\n", (int)stepErr.size(), stepErr.data());
			DestroyRuntime(rt); return -1;
		}
		DebugLocation after = dbg->GetLocation(inst);
		if (dbg->IsPaused(inst) == false || after.line != 7 || after.callDepth != depthBefore)
		{
			printf("[debug-smoke] module step over #2 landed line=%d depth=%d (expected 7/%d)\n",
				after.line, after.callDepth, depthBefore);
			DestroyRuntime(rt); return -1;
		}
		std::vector<DebugVariable> stepVars;
		dbg->GetFrameVariables(inst, 0, stepVars);
		bool foundY = false;
		for (const DebugVariable& var : stepVars)
			if (var.name == "y" && var.value == "4") foundY = true;
		if (!foundY)
		{
			printf("[debug-smoke] module step over lost argument (y != 4)\n");
			for (const DebugVariable& var : stepVars)
				printf("    %s = %s\n", var.name.c_str(), var.value.c_str());
			DestroyRuntime(rt); return -1;
		}
		resumeToEnd(inst);
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 6.6) 호스트 invoke(rt->Call) 안에서 브레이크포인트로 정지 → StepOver 로 재개.
	//      게임의 OnUpdate 이벤트 핸들러가 정확히 이 모양이다(RunGlobalInit 이 아니라 invoke).
	//      Invocation::invoke 가 "정지"를 "완료"로 보면 GC() 가 살아 있는 프레임의 스택을
	//      회수해서, 재개 후 지역 변수를 인자로 받는 함수만 null 을 받는다.
	//      (전역 인자는 멀쩡하므로 첫 StepOver 는 멀쩡히 지나가고 그 다음에 터진다)
	{
		const char* invokeSource =
			"import dbgmod as dm;\n"
			"var gCount = 40;\n"
			"var gSeed = 1.5;\n"
			"export fun Tick()\n"
			"{\n"
			"    var local = 3;\n"
			"    var n = dm.Heavy(gCount, gSeed);\n"
			"    var y = dm.Take(local);\n"
			"    return n + y;\n"
			"}\n";
		ProgramHandle prog; InstanceHandle inst;
		if (!compileInst(invokeSource, "invokestep.ns", prog, inst)) { DestroyRuntime(rt); return -1; }
		rt->RunGlobalInit(inst);
		V2DebugSmokeListener listener;
		dbg->SetListener(&listener);
		setBps(inst, { 7 });   // var n = dm.Heavy(gCount, gSeed);  (인자가 전역)
		{
			Invocation call = rt->Call(inst, "Tick");
			RunStatus st = call.invoke();
			if (st != RunStatus::Suspended || dbg->IsPaused(inst) == false || listener.lastLocation.line != 7)
			{
				printf("[debug-smoke] invoke breakpoint failed status=%d paused=%d line=%d\n",
					(int)st, dbg->IsPaused(inst) ? 1 : 0, listener.lastLocation.line);
				DestroyRuntime(rt); return -1;
			}
		}
		// 1st F10: 무거운 모듈 함수(전역 인자)를 넘긴다 -> 8줄
		dbg->StepOver(inst);
		dbg->Run(inst);
		if (dbg->IsPaused(inst) == false || dbg->GetLocation(inst).line != 8)
		{
			printf("[debug-smoke] invoke step over #1 landed line=%d (expected 8)\n",
				dbg->GetLocation(inst).line);
			DestroyRuntime(rt); return -1;
		}
		// 2nd F10: 지역 변수를 인자로 받는 모듈 함수를 넘긴다 -> 9줄
		dbg->StepOver(inst);
		dbg->Run(inst);
		StringView invokeErr;
		if (rt->TakeLastError(invokeErr))
		{
			printf("[debug-smoke] invoke step over #2 error: %.*s\n", (int)invokeErr.size(), invokeErr.data());
			DestroyRuntime(rt); return -1;
		}
		if (dbg->IsPaused(inst) == false || dbg->GetLocation(inst).line != 9)
		{
			printf("[debug-smoke] invoke step over #2 landed line=%d (expected 9)\n",
				dbg->GetLocation(inst).line);
			DestroyRuntime(rt); return -1;
		}
		std::vector<DebugVariable> invokeVars;
		dbg->GetFrameVariables(inst, 0, invokeVars);
		bool foundLocal = false, foundY = false;
		for (const DebugVariable& var : invokeVars)
		{
			if (var.name == "local" && var.value == "3") foundLocal = true;
			if (var.name == "y" && var.value == "4") foundY = true;
		}
		if (!foundLocal || !foundY)
		{
			printf("[debug-smoke] invoke step lost locals (local=%d y=%d)\n", foundLocal ? 1 : 0, foundY ? 1 : 0);
			for (const DebugVariable& var : invokeVars)
				printf("    %s = %s\n", var.name.c_str(), var.value.c_str());
			DestroyRuntime(rt); return -1;
		}
		resumeToEnd(inst);
		dbg->SetListener(nullptr);
		rt->DestroyInstance(inst);
		rt->DestroyProgram(prog);
	}

	// 7) 단락(short-circuit) 평가 결과 검증 (디버그 아님, 실행 결과만)
	{
		const char* shortCircuitSource =
			"fun ShortCircuitEcho(var value)\n"
			"{\n"
			"    return value;\n"
			"}\n"
			"export fun ShortCircuitTest()\n"
			"{\n"
			"    var target = null;\n"
			"    var state = 0;\n"
			"    if (target == null || target[4] < 10) state = 1;\n"
			"    if (target != null && target[4] < 10) state = 2;\n"
			"    var values = [3];\n"
			"    if (target == null || (values[0] == 3 && target[4] < 10)) state = 3;\n"
			"    var c = target == null || target[4] < 0.3;\n"
			"    var arg = ShortCircuitEcho(target == null || target[4] < 0.3);\n"
			"    var list = [target == null || target[4] < 0.3];\n"
			"    var map = { \"safe\": target == null || target[4] < 0.3 };\n"
			"    var emptyList = [];\n"
			"    var emptyMap = {};\n"
			"    var shared = { \"items\": [], \"requests\": [] };\n"
			"    var indexList = [10, 20];\n"
			"    var indexed = indexList[toint(target == null || target[4] < 0.3)];\n"
			"    if (c) state = 4;\n"
			"    if (arg && list[0] && map[\"safe\"] && indexed == 20) state = 5;\n"
			"    if (emptyList.len() == 0 && emptyMap.len() == 0) state = 6;\n"
			"    var loop = 0;\n"
			"    while (target != null && target[4] < 10) loop = loop + 1;\n"
			"    if (loop == 0) state = 7;\n"
			"    return state;\n"
			"}\n"
			"export fun ShortCircuitReturn()\n"
			"{\n"
			"    var a = null;\n"
			"    return a == null || a[4] < 0.3;\n"
			"}\n"
			"export fun ForeachIndexed()\n"
			"{\n"
			"    var groups = [[1, 2], [3, 4]];\n"
			"    var sum = 0;\n"
			"    foreach(var value in groups[1])\n"
			"    {\n"
			"        sum += value;\n"
			"    }\n"
			"    return sum;\n"
			"}\n";
		CompileDesc cd; cd.source = shortCircuitSource; cd.sourceName = "short_circuit.ns";
		CompileResult cr = rt->Compile(cd);
		if (!cr.program) return fail("[short-circuit] compile failed");
		InstanceHandle inst = rt->CreateInstance(cr.program);
		int shortCircuitResult = 0; bool shortCircuitReturn = false; int foreachIndexedResult = 0;
		{ Invocation c = rt->Call(inst, "ShortCircuitTest");   c.invoke(); shortCircuitResult = c.retInt(); }
		{ Invocation c = rt->Call(inst, "ShortCircuitReturn"); c.invoke(); shortCircuitReturn = c.retBool(); }
		{ Invocation c = rt->Call(inst, "ForeachIndexed");     c.invoke(); foreachIndexedResult = c.retInt(); }
		StringView emsg;
		bool hadErr = rt->TakeLastError(emsg);
		if (hadErr || shortCircuitResult != 7 || shortCircuitReturn == false || foreachIndexedResult != 7)
		{
			printf("[short-circuit] invalid result=%d return=%d foreach=%d err=%.*s\n",
				shortCircuitResult, shortCircuitReturn ? 1 : 0, foreachIndexedResult, (int)emsg.size(), emsg.data());
			DestroyRuntime(rt); return -1;
		}
		rt->DestroyInstance(inst);
		rt->DestroyProgram(cr.program);
	}

	// 8) 잘못된 조건식은 컴파일 거부(expected an expression)
	{
		const char* invalidConditionSources[] =
		{
			"fun InvalidIf() { if () {} }",
			"fun InvalidWhile() { while () {} }"
		};
		for (const char* invalidSource : invalidConditionSources)
		{
			CompileDesc cd; cd.source = invalidSource; cd.sourceName = "invalid.ns";
			CompileResult cr = rt->Compile(cd);
			if (cr.program || cr.error.message.find("expected an expression") == std::string::npos)
			{
				printf("[parser] empty condition was accepted: %s\n", cr.error.message.c_str());
				if (cr.program) rt->DestroyProgram(cr.program);
				DestroyRuntime(rt); return -1;
			}
		}
	}

	DestroyRuntime(rt);
	return 0;
}

static int RunCompilerErrorRegression()
{
	struct ErrorCase
	{
		const char* name;
		const char* source;
		const char* expected;
	};
	const ErrorCase cases[] =
	{
		{ "continue-semicolon", "for(var i in 0, 1, 1) { continue }", "';' after 'continue'" },
		{ "increment-literal", "var value = 1; 1++;", "invalid target for '++'" },
		{ "argument-count", "fun F(var value) {} F();", "argument" },
		{ "empty-if", "fun F() { if () {} }", "expected an expression" },
		{ "empty-while", "fun F() { while () {} }", "expected an expression" },
		// 리스트/맵 리터럴 안의 실패는 반드시 사유가 남아야 한다.
		// (toint/tofloat 를 원소로 쓰면 원소를 조용히 버리고 메시지 없이 실패하던 회귀 관련)
		{ "list-literal-bad-expression", "var v = [1 + ];", "invalid operator" },
		{ "list-literal-unknown-id", "var v = [undefinedName];", "unknown identifier" },
		{ "list-literal-missing-value", "var v = [,];", "expected an expression" },
		{ "map-literal-unknown-id", "var v = { \"a\": undefinedName };", "unknown identifier" },
		{ "map-literal-missing-value", "var v = { \"a\": };", "expected an expression" },
		{ "map-literal-missing-key", "var v = { : 1 };", "expected an expression" },
		// 첫 원소가 아니라 중간/뒤에서 빠져도 똑같이 잡혀야 한다.
		{ "list-literal-missing-middle", "var v = [1, , 2];", "expected an expression" },
		{ "map-literal-missing-value-2nd", "var v = { \"a\": 1, \"b\": };", "expected an expression" },
		// 함수 안(지역 스코프)도 같은 경로를 타는지 고정.
		{ "list-literal-missing-in-fun", "fun F() { var v = [,]; }", "expected an expression" },
		{ "map-literal-missing-in-fun", "fun F() { var v = { \"a\": }; }", "expected an expression" },
		// 순환 import 는 예전에 사유 없이 실패했다(캐시가 파싱을 마친 뒤에 채워져 재진입을
		// 못 막았고, 재진입한 모듈이 이름 충돌로 죽으면서 메시지가 사라졌다).
		// Lib/cyca.ns <-> Lib/cycb.ns, Lib/selfimp.ns 가 이 케이스의 고정 데이터다.
		{ "import-cycle", "import cyca as a;", "circular import" },
		{ "import-cycle-self", "import selfimp as s;", "circular import" },
	};

	RuntimeDesc rd;
	rd.httpTransport = CreateConsoleHttpTransport();
	// import 를 쓰는 케이스가 있어 로더를 붙인다(libPath 기본값 ../../Lib/).
	CNeoLoader regressionLoader;
	rd.nativeLoader = &regressionLoader;
	IRuntime* rt = CreateRuntime(rd);
	rt->FreezeBindings();

	int passed = 0;
	for (const ErrorCase& test : cases)
	{
		CompileDesc cd; cd.source = test.source; cd.sourceName = test.name;
		CompileResult cr = rt->Compile(cd);
		if (!cr.program && cr.error.message.find(test.expected) != std::string::npos)
		{
			++passed;
			continue;
		}
		if (cr.program) rt->DestroyProgram(cr.program);
		printf("[compiler-error] %s failed: %s\n", test.name, cr.error.message.c_str());
	}

	const ErrorCase mutationCases[] =
	{
		{ "list-append-during-foreach", "var values = [1]; foreach(var value in values) values.append(2);", "modified during foreach" },
		{ "map-insert-during-foreach", "var values = { \"a\": 1 }; foreach(var key, value in values) values[\"b\"] = 2;", "modified during foreach" },
		{ "map-remove-during-foreach", "var values = { \"a\": 1 }; foreach(var key, value in values) values[key] = null;", "modified during foreach" },
		{ "array-read-out-of-range", "import system; var values = system.array(0, 1); var value = values[1];", "array index out of range on read" },
		{ "array-write-out-of-range", "import system; var values = system.array(0, 1); values[1] = 3;", "array index out of range on assign" },
		{ "array-index-type", "import system; var values = system.array(0, 1); var value = values[true];", "array index must be an int" },
		{ "array-value-type", "import system; var values = system.array(false, 1); values[0] = 1;", "cannot assign int to bool array" },
		{ "array-append-value-type", "import system; var values = system.array(false, 1); values.append(1);", "cannot assign int to bool array" },
		{ "array-foreach-two-var", "import system; var values = system.array(0, 1); foreach(var index, value in values) {}", "foreach on array does not support two variables" },
		{ "array-append-during-foreach", "import system; var values = system.array(1, 1); foreach(var value in values) values.append(2);", "modified during foreach" },
		{ "array-resize-during-foreach", "import system; var values = system.array(1, 1); foreach(var value in values) values.resize(2);", "modified during foreach" },
	};
	for (const ErrorCase& test : mutationCases)
	{
		CompileDesc cd; cd.source = test.source; cd.sourceName = test.name;
		CompileResult cr = rt->Compile(cd);
		bool rejected = false;
		if (cr.program)
		{
			InstanceHandle inst = rt->CreateInstance(cr.program); // 전역 실행 → foreach 변형 런타임 에러
			StringView err;
			rejected = rt->TakeLastError(err) &&
				std::string(err.data(), err.size()).find(test.expected) != std::string::npos;
			rt->DestroyInstance(inst);
			rt->DestroyProgram(cr.program);
		}
		if (rejected)
		{
			++passed;
			continue;
		}
		printf("[foreach-mutation] %s failed\n", test.name);
	}

	const int total = _countof(cases) + _countof(mutationCases);
	printf("Compiler error regression %s : %d/%d\n", passed == total ? "PASS" : "FAIL", passed, total);
	DestroyRuntime(rt);
	return passed == total ? 0 : -1;
}

static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        default:
            if (c < 0x20) { char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
            else out += static_cast<char>(c);
            break;
        }
    }
    return out;
}

static FILE* g_DapOutput = stdout;
static std::mutex g_DapOutputMutex;
static constexpr size_t ProtocolMaxBody = 8 * 1024 * 1024;
static constexpr size_t ProtocolMaxHeader = 8192;

static bool ProtocolContentLength(const std::string& header, size_t& length)
{
    bool found = false;
    std::istringstream input(header);
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) return false;
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
        if (key != "content-length") continue;
        if (found) return false;
        found = true;
        size_t pos = colon + 1;
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        length = 0;
        const size_t start = pos;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9')
        {
            size_t digit = static_cast<size_t>(line[pos++] - '0');
            if (length > (ProtocolMaxBody - digit) / 10) return false;
            length = length * 10 + digit;
        }
        if (pos == start) return false;
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        if (pos != line.size() || length == 0) return false;
    }
    return found;
}

static bool DapReadMessage(std::string& body)
{
    std::string header;
    char c;
    while (std::cin.get(c))
    {
        header += c;
        if (header.size() > ProtocolMaxHeader) return false;
        if (header.size() >= 4 && header.compare(header.size() - 4, 4, "\r\n\r\n") == 0) break;
    }
    if (header.size() < 4 || header.compare(header.size() - 4, 4, "\r\n\r\n") != 0) return false;
    size_t length = 0;
    if (!ProtocolContentLength(header.substr(0, header.size() - 4), length)) return false;
    body.resize(length);
    std::cin.read(&body[0], static_cast<std::streamsize>(length));
    return static_cast<size_t>(std::cin.gcount()) == length;
}

// VM과 디버거는 같은 스레드에서 처리하고 짧은 실행 구간 사이에 요청을 읽는다.
class DapInput
{
    std::string buffered;
    size_t expected = 0;
public:
    int Poll(std::string& body)
    {
        if (!expected)
        {
            size_t end = buffered.find("\r\n\r\n");
            if (end != std::string::npos)
            {
                if (end > ProtocolMaxHeader || !ProtocolContentLength(buffered.substr(0, end), expected)) return -1;
                buffered.erase(0, end + 4);
            }
            else if (buffered.size() > ProtocolMaxHeader) return -1;
        }
        if (expected && buffered.size() >= expected)
        {
            body.assign(buffered.data(), expected);
            buffered.erase(0, expected);
            expected = 0;
            return 1;
        }
        HANDLE input = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdin)));
        DWORD available = 8192;
        if (GetFileType(input) == FILE_TYPE_PIPE)
        {
            if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) return -1;
            if (!available) return 0;
        }
        char bytes[8192];
        const int count = _read(_fileno(stdin), bytes, static_cast<unsigned>(std::min<DWORD>(available, sizeof(bytes))));
        if (count <= 0) return -1;
        buffered.append(bytes, static_cast<size_t>(count));
        return 0;
    }
};

static void DapSendMessage(const std::string& body)
{
    std::lock_guard<std::mutex> lock(g_DapOutputMutex);
    FILE* out = g_DapOutput ? g_DapOutput : stdout;
    fprintf(out, "Content-Length: %zu\r\n\r\n", body.size());
    fwrite(body.data(), 1, body.size(), out);
    fflush(out);
}

class ProtocolJson
{
    yyjson_doc* doc = nullptr;
public:
    explicit ProtocolJson(const std::string& body) : doc(yyjson_read(body.data(), body.size(), 0)) {}
    ~ProtocolJson() { if (doc) yyjson_doc_free(doc); }
    yyjson_val* Root() const { return doc ? yyjson_doc_get_root(doc) : nullptr; }
};
static yyjson_val* JsonMember(yyjson_val* object, const char* key) { return yyjson_obj_get(object, key); }
static int JsonInt(yyjson_val* object, const char* key, int fallback = 0)
{
    yyjson_val* v = JsonMember(object, key);
    if (!yyjson_is_int(v)) return fallback;
    if (yyjson_is_uint(v) && yyjson_get_uint(v) > INT_MAX) return fallback;
    int64_t n = yyjson_get_sint(v);
    return n < INT_MIN || n > INT_MAX ? fallback : static_cast<int>(n);
}
static bool JsonBool(yyjson_val* object, const char* key, bool fallback = false)
{
    yyjson_val* v = JsonMember(object, key);
    return yyjson_is_bool(v) ? yyjson_get_bool(v) : fallback;
}
static std::string JsonString(yyjson_val* object, const char* key)
{
    yyjson_val* v = JsonMember(object, key);
    return yyjson_is_str(v) ? std::string(yyjson_get_str(v), yyjson_get_len(v)) : std::string();
}
static std::string JsonValueText(yyjson_val* value)
{
    if (!value) return "null";
    size_t length = 0;
    char* text = yyjson_val_write(value, 0, &length);
    std::string result = text ? std::string(text, length) : "null";
    free(text);
    return result;
}

struct ProtocolBreakpoint { int line; bool supported; };
static std::vector<ProtocolBreakpoint> JsonBreakpoints(yyjson_val* arguments)
{
    std::vector<ProtocolBreakpoint> result;
    yyjson_val* list = JsonMember(arguments, "breakpoints");
    for (size_t i = 0; i < yyjson_arr_size(list); ++i)
    {
        yyjson_val* bp = yyjson_arr_get(list, i);
        bool supported = !JsonMember(bp, "condition") && !JsonMember(bp, "hitCondition") && !JsonMember(bp, "logMessage");
        result.push_back({JsonInt(bp, "line", -1), supported});
    }
    return result;
}

// 파이프 읽기 경계에서 잘린 UTF-8 바이트를 보관하고 EOF의 미완성 문자만 대체한다.
static size_t Utf8CompletePrefix(const std::string& text)
{
    if (text.empty()) return 0;
    size_t start = text.size() - 1;
    while (start && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) --start;
    unsigned char lead = static_cast<unsigned char>(text[start]);
    size_t width = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
    return text.size() - start < width ? start : text.size();
}

static bool ParseNeoCompileError(const std::string& err, int& line, int& column, std::string& message)
{
	const char* prefix = "Error (";
	size_t pos = err.find(prefix);
	if (pos == std::string::npos)
		return false;

	const char* p = err.c_str() + pos + strlen(prefix);
	line = atoi(p);
	const char* comma = strchr(p, ',');
	if (comma == nullptr)
		return false;
	column = atoi(comma + 1);

	const char* close = strchr(comma, ')');
	if (close == nullptr)
		return false;
	const char* msg = close + 1;
	if (*msg == ':')
		++msg;
	while (*msg && isspace((unsigned char)*msg))
		++msg;
	message = msg;
	while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
		message.pop_back();
	return line > 0 && column > 0;
}

class NeoDapSession : public IDebugListener
{
public:
	CNeoLoader* loader = nullptr;
	int seq = 1;
	IRuntime* runtime = nullptr;   // v2: VM/worker 대신 Runtime + 핸들
	IDebugger* dbg = nullptr;
	ProgramHandle program;
	InstanceHandle instance;
	std::string sourcePath;
	Error compileError;
	std::vector<std::string> sourceFiles;
	std::map<int, std::vector<int>> breakpointsByFile;
	std::set<int> executableLines;
	std::map<int, std::set<int>> executableLinesByFile;
	DebugStopReason lastStopReason = DebugStopReason::Pause;
	DebugLocation lastStopLocation;
	bool terminated = false;
	bool initialRunStarted = false;
	bool noDebugMode = false;
	int nextVariableReference = 1000000;
    std::map<int, const std::vector<DebugVariable>*> variableReferences;
    std::map<const std::vector<DebugVariable>*, int> childReferences;
    std::map<int, std::vector<DebugVariable>> frameVariables;
    std::vector<DebugStackFrame> stoppedFrames;
    std::mutex sendMutex;
    bool running = false;
    bool pendingStop = false;
    bool faultStopped = false;
    std::string lastStopError;
    int savedStdout = -1;
    int captureWrite = -1;
    std::thread outputThread;
    std::ios::fmtflags oldCoutFlags{};

    ~NeoDapSession() { StopCapture(); }


	void ClearVariableReferences()
	{
		variableReferences.clear();
        childReferences.clear();
        frameVariables.clear();
        stoppedFrames.clear();
		nextVariableReference = 1000000;
	}

    const std::vector<DebugVariable>& GetFrameVariables(int frame)
    {
        static const std::vector<DebugVariable> empty;
        if (frame < 0 || frame >= 64) return empty;
        auto found = frameVariables.find(frame);
        if (found != frameVariables.end()) return found->second;
        if (faultStopped || !dbg) return empty;
        auto& vars = frameVariables[frame];
        dbg->GetFrameVariables(instance, frame, vars);
        return vars;
    }
    int StoreVariableChildren(const std::vector<DebugVariable>& children)
    {
        if (children.empty()) return 0;
        auto found = childReferences.find(&children);
        if (found != childReferences.end()) return found->second;
        const int reference = nextVariableReference++;
        childReferences[&children] = reference;
        variableReferences[reference] = &children;
        return reference;
    }

	void WriteVariables(std::ostringstream& os, const std::vector<DebugVariable>& vars)
	{
		os << "{\"variables\":[";
		for (size_t i = 0; i < vars.size(); ++i)
		{
			if (i) os << ",";
			const int reference = StoreVariableChildren(vars[i].children);
			os << "{\"name\":\"" << JsonEscape(vars[i].name) << "\",\"type\":\"" << JsonEscape(vars[i].type)
				<< "\",\"value\":\"" << JsonEscape(vars[i].value) << "\",\"variablesReference\":" << reference << "}";
		}
		os << "]}";
	}

    virtual void OnStopped(InstanceHandle, const DebugLocation& location, DebugStopReason reason) override
    {
        ClearVariableReferences();
        faultStopped = false;
        lastStopReason = reason;
        lastStopLocation = location;
        pendingStop = true;
        lastStopError.clear();
        if (dbg)
        {
            dbg->GetStackTrace(instance, stoppedFrames);
            if (stoppedFrames.size() > 64) stoppedFrames.resize(64);
            if (reason == DebugStopReason::Exception)
                for (const auto& frame : stoppedFrames) GetFrameVariables(frame.frameId);
        }
        faultStopped = reason == DebugStopReason::Exception;
    }
    void PublishStopped()
    {
        const char* reason = lastStopReason == DebugStopReason::Breakpoint ? "breakpoint" :
            lastStopReason == DebugStopReason::Step ? "step" :
            lastStopReason == DebugStopReason::Exception ? "exception" : "pause";
        std::string body = "{\"reason\":\"" + std::string(reason) + "\",\"threadId\":1,\"allThreadsStopped\":true";
        if (!lastStopError.empty()) body += ",\"description\":\"" + JsonEscape(lastStopError) + "\"";
        SendEvent("stopped", body + "}");
        pendingStop = false;
    }

	void SendResponse(int requestSeq, const std::string& command, const std::string& body = "{}", bool success = true, const std::string& message = "")
	{
        std::lock_guard<std::mutex> lock(sendMutex);
		std::ostringstream os;
		os << "{\"seq\":" << seq++ << ",\"type\":\"response\",\"request_seq\":" << requestSeq
			<< ",\"success\":" << (success ? "true" : "false") << ",\"command\":\"" << JsonEscape(command) << "\"";
		if (!message.empty())
			os << ",\"message\":\"" << JsonEscape(message) << "\"";
		os << ",\"body\":" << body << "}";
		DapSendMessage(os.str());
	}

	void SendEvent(const std::string& event, const std::string& body = "{}")
	{
        std::lock_guard<std::mutex> lock(sendMutex);
		std::ostringstream os;
		os << "{\"seq\":" << seq++ << ",\"type\":\"event\",\"event\":\"" << JsonEscape(event) << "\",\"body\":" << body << "}";
		DapSendMessage(os.str());
	}

	void SendOutput(const std::string& output)
	{
		if (output.empty())
			return;
		SendEvent("output", "{\"category\":\"stdout\",\"output\":\"" + JsonEscape(output) + "\"}");
	}

	void SendErrorOutput(const std::string& output)
	{
		if (output.empty())
			return;
		SendEvent("output", "{\"category\":\"stderr\",\"output\":\"" + JsonEscape(output) + "\"}");
	}

	static std::string NormalizePathForCompare(std::string path)
	{
		std::replace(path.begin(), path.end(), '/', '\\');
		std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return (char)tolower(c); });
		return path;
	}

	int FileIdFromSourcePath(const std::string& path) const
	{
		if (path.empty())
			return 0;
		std::string needle = NormalizePathForCompare(path);
		for (int i = 0; i < (int)sourceFiles.size(); ++i)
		{
			if (NormalizePathForCompare(sourceFiles[i]) == needle)
				return i;
		}
		if (NormalizePathForCompare(sourcePath) == needle)
			return 0;
		return -1;
	}

	bool IsExecutableLine(int file, int line) const
	{
		std::map<int, std::set<int>>::const_iterator itFile = executableLinesByFile.find(file);
		if (itFile != executableLinesByFile.end())
			return itFile->second.find(line) != itFile->second.end();
		return instance && file == 0 && executableLines.find(line) != executableLines.end();
	}

	std::string SourcePathFromFileId(int fileId) const
	{
		if (fileId >= 0 && fileId < (int)sourceFiles.size() && sourceFiles[fileId].empty() == false)
			return sourceFiles[fileId];
		return sourcePath;
	}

	static std::string SourceNameFromPath(const std::string& path)
	{
		std::string name = path;
		size_t slash = name.find_last_of("\\/");
		if (slash != std::string::npos)
			name = name.substr(slash + 1);
		return name;
	}

	void ApplyBreakpoints()
	{
		if (!instance || !dbg)
			return;
		std::vector<DebugBreakpoint> activeBreakpoints;
		for (std::map<int, std::vector<int>>::const_iterator it = breakpointsByFile.begin(); it != breakpointsByFile.end(); ++it)
		{
			int file = it->first;
			const std::vector<int>& lines = it->second;
			for (size_t i = 0; i < lines.size(); ++i)
			{
				int line = lines[i];
				if (IsExecutableLine(file, line))
				{
					DebugBreakpoint bp;
					bp.file = file;
					bp.line = line;
					activeBreakpoints.push_back(bp);
				}
			}
		}
		dbg->SetBreakpoints(instance, activeBreakpoints);
	}

	bool LoadProgram(const std::string& path, std::string& err, bool enableDebug)
	{
        StopCapture();
        if (runtime)
        {
            if (instance) runtime->DestroyInstance(instance);
            if (program) runtime->DestroyProgram(program);
            DestroyRuntime(runtime);
        }
        runtime = nullptr; dbg = nullptr; instance = {}; program = {};
        running = pendingStop = faultStopped = initialRunStarted = false;
        sourceFiles.clear(); sourcePath.clear();
        executableLines.clear(); executableLinesByFile.clear(); breakpointsByFile.clear();
        ClearVariableReferences();
        compileError = Error{};
		std::string fullPath = FullPathOrSelf(path);
		std::wstring widePath;
		FILE* fp = nullptr;
		if (!Utf8PathToWide(fullPath, widePath) || _wfopen_s(&fp, widePath.c_str(), L"rb") != 0)
		{
			err = "cannot open program";
			return false;
		}
		std::unique_ptr<FILE, int (*)(FILE*)> sourceFile(fp, &fclose);
		std::string src;
		char sourceBuffer[4096];
		size_t readCount = 0;
		while ((readCount = fread(sourceBuffer, 1, sizeof(sourceBuffer), sourceFile.get())) != 0)
			src.append(sourceBuffer, readCount);
		if (ferror(sourceFile.get()))
		{
			err = "cannot read program";
			return false;
		}

		RuntimeDesc rd;
	rd.httpTransport = CreateConsoleHttpTransport();
		rd.nativeLoader = loader;                 // import 해석
		runtime = CreateRuntime(rd);
		runtime->FreezeBindings();

		CompileDesc cd;
		cd.source = StringView(src.data(), src.size());
		cd.sourceName = fullPath.c_str();
		cd.includeDebugInfo = enableDebug;
		if (enableDebug)
		{
			cd.debugSourcePath = StringView(fullPath.c_str(), fullPath.size());
			cd.debugSourceFiles = &sourceFiles;
		}
		CompileResult cr = runtime->Compile(cd);
		if (!cr.program)
		{
			err = cr.error.message;
			compileError = cr.error;
			return false;
		}
		program = cr.program;

		InstanceDesc idesc;
		idesc.runGlobalInit = false;              // BP 를 먼저 걸기 위해 전역 실행 지연
		instance = runtime->CreateInstance(program, idesc);
		dbg = runtime->GetDebugger();
        if (!instance) { err = "cannot create script instance"; return false; }
        if (dbg) dbg->SetSliceBudget(instance, 4096);

		executableLines.clear();
		executableLinesByFile.clear();
		if (enableDebug && dbg)
		{
			dbg->SetListener(this);
			std::vector<int> lines;
			dbg->GetExecutableLines(program, lines);
			for (int line : lines)
				executableLines.insert(line);
			std::vector<DebugLocation> locations;
			dbg->GetExecutableLocations(program, locations);
			for (size_t i = 0; i < locations.size(); ++i)
				executableLinesByFile[locations[i].file].insert(locations[i].line);
			ApplyBreakpoints();
		}
		lastStopReason = DebugStopReason::Pause;
		lastStopLocation = DebugLocation();
		sourcePath = fullPath;
		return true;
	}

    void StartCapture()
    {
        if (savedStdout >= 0) return;
        fflush(stdout);
        savedStdout = _dup(_fileno(stdout));
        int pipeFd[2] = {-1, -1};
        if (savedStdout < 0 || _pipe(pipeFd, 4096, _O_BINARY) != 0)
        {
            if (savedStdout >= 0) _close(savedStdout);
            savedStdout = -1;
            return;
        }
        captureWrite = pipeFd[1];
        _dup2(captureWrite, _fileno(stdout));
        oldCoutFlags = std::cout.flags();
        std::cout.setf(std::ios::unitbuf);
        const int readFd = pipeFd[0];
        outputThread = std::thread([this, readFd]()
        {
            std::string pending;
            char buffer[512];
            for (;;)
            {
                int count = _read(readFd, buffer, sizeof(buffer));
                if (count <= 0) break;
                pending.append(buffer, static_cast<size_t>(count));
                size_t complete = Utf8CompletePrefix(pending);
                SendOutput(pending.substr(0, complete));
                pending.erase(0, complete);
            }
            if (!pending.empty()) SendOutput("\xef\xbf\xbd");
            _close(readFd);
        });
    }
    void StopCapture()
    {
        if (savedStdout < 0) return;
        fflush(stdout);
        _dup2(savedStdout, _fileno(stdout));
        _close(captureWrite);
        captureWrite = -1;
        if (outputThread.joinable()) outputThread.join();
        std::cout.flags(oldCoutFlags);
        _close(savedStdout);
        savedStdout = -1;
    }
    void RunCurrent(bool initial)
    {
        if (!instance || terminated || faultStopped) { running = false; return; }
        if (initial) SendOutput(noDebugMode ? "[Neo Script] Run started.\n" : "[Neo Script] Debug started.\n");
        StartCapture();
        const RunStatus status = initial ? runtime->RunGlobalInit(instance) :
            (dbg ? dbg->Run(instance) : RunStatus::Failed);
        const bool paused = dbg && dbg->IsPaused(instance);
        running = status == RunStatus::Suspended && !paused && !pendingStop;
        if (running) { fflush(stdout); return; }
        StopCapture(); // 정지 이벤트보다 앞선 출력을 모두 전송한다.
        StringView error;
        if (runtime && runtime->PeekLastError(error))
        {
            lastStopError.assign(error.data(), error.size());
            if (!lastStopError.empty()) SendErrorOutput(lastStopError + "\n");
        }
        if (pendingStop) { PublishStopped(); return; }
        if (!paused && !terminated)
        {
            SendOutput(noDebugMode ? "[Neo Script] Run finished.\n" : "[Neo Script] Debug finished.\n");
            terminated = true;
            SendEvent("terminated");
        }
    }

	void Handle(const std::string& body)
	{
        ProtocolJson json(body);
        yyjson_val* root = json.Root();
        const int requestSeq = JsonInt(root, "seq", 0);
        const std::string command = JsonString(root, "command");
        yyjson_val* arguments = JsonMember(root, "arguments");
        if (!yyjson_is_obj(root) || JsonString(root, "type") != "request" || command.empty())
        { SendResponse(requestSeq, command, "{}", false, "Invalid DAP request"); return; }
        if (running && command != "pause" && command != "disconnect" && command != "threads")
        { SendResponse(requestSeq, command, "{}", false, "Script is running"); return; }
        if (command == "continue" || command == "stepIn" || command == "next" || command == "stepOut")
        {
            if (!initialRunStarted || !instance || !dbg || !dbg->IsPaused(instance) || faultStopped)
            { SendResponse(requestSeq, command, "{}", false, "No resumable paused script"); return; }
        }
		if (command == "initialize")
		{
			SendResponse(requestSeq, command, "{\"supportsConfigurationDoneRequest\":true,\"supportsStepInTargetsRequest\":false,\"supportsExceptionInfoRequest\":true,\"supportsEvaluateForHovers\":true}");
		}
		else if (command == "launch")
		{
			std::string path = JsonString(arguments, "program");
			if (path.empty())
				path = JsonString(arguments, "path");
			bool noDebug = JsonBool(arguments, "noDebug", false);
			noDebugMode = noDebug;
			std::string libPath = JsonString(arguments, "libPath");
            if (loader) loader->SetLibPath(libPath.empty() ? "Lib" : libPath);
			std::string err;
			bool ok = LoadProgram(path, err, !noDebug);
			if (!ok)
			{
				std::string out = err;
				if (!out.empty() && out.back() != '\n')
					out += "\n";
				SendErrorOutput(out);

				int line = 0;
				int column = 0;
				std::string message;
				if (ParseNeoCompileError(err, line, column, message))
				{
					// Imported diagnostics carry their own source, not the launch file.
					std::string fullPath = FullPathOrSelf(compileError.sourceName.empty() ? path : compileError.sourceName);
					if (compileError.line) line = (int)compileError.line;
					if (compileError.column) column = (int)compileError.column;
					std::ostringstream eventBody;
					eventBody << "{\"source\":{\"path\":\"" << JsonEscape(fullPath) << "\"},\"line\":" << line
						<< ",\"column\":" << column << ",\"message\":\"" << JsonEscape(message)
						<< "\",\"raw\":\"" << JsonEscape(err) << "\"}";
					SendEvent("neoScriptCompileError", eventBody.str());
				}
			}
			SendResponse(requestSeq, command, "{}", ok, ok ? "" : "Neo Script compile failed. See Debug Console.");
			if (ok && noDebug)
			{
				initialRunStarted = true;
				RunCurrent(true);
			}
			else if (ok)
				SendEvent("initialized");
		}
		else if (command == "setBreakpoints")
		{
			std::string bpSourcePath = JsonString(JsonMember(arguments, "source"), "path");
			int bpFile = FileIdFromSourcePath(bpSourcePath);
			            const auto requested = JsonBreakpoints(arguments);
            std::vector<int> breakpoints;
            for (const auto& bp : requested) if (bp.supported && bp.line > 0) breakpoints.push_back(bp.line);
			if (bpFile >= 0)
				breakpointsByFile[bpFile] = breakpoints;
			ApplyBreakpoints();
			std::ostringstream os;
			os << "{\"breakpoints\":[";
			for (size_t i = 0; i < requested.size(); ++i)
			{
				if (i) os << ",";
				bool verified = requested[i].supported && (bpFile >= 0) && IsExecutableLine(bpFile, requested[i].line);
				os << "{\"verified\":" << (verified ? "true" : "false") << ",\"line\":" << requested[i].line;
				if (!verified)
					os << ",\"message\":\"No executable instruction or unsupported breakpoint condition/log message\"";
				os << "}";
			}
			os << "]}";
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "configurationDone")
		{
            if (!instance) { SendResponse(requestSeq, command, "{}", false, "No loaded script"); return; }
			SendResponse(requestSeq, command);
			if (!initialRunStarted)
			{
				initialRunStarted = true;
				RunCurrent(true);
			}
		}
		else if (command == "threads")
		{
			SendResponse(requestSeq, command, "{\"threads\":[{\"id\":1,\"name\":\"Neo VM\"}]}");
		}
		else if (command == "continue")
		{
			if (dbg) dbg->Continue(instance);
			SendResponse(requestSeq, command, "{\"allThreadsContinued\":true}");
			RunCurrent(false);
		}
		else if (command == "stepIn")
		{
			if (dbg) dbg->StepInto(instance);
			SendResponse(requestSeq, command);
			RunCurrent(false);
		}
		else if (command == "next")
		{
			if (dbg) dbg->StepOver(instance);
			SendResponse(requestSeq, command);
			RunCurrent(false);
		}
		else if (command == "stepOut")
		{
			if (dbg) dbg->StepOut(instance);
			SendResponse(requestSeq, command);
			RunCurrent(false);
		}
		else if (command == "stackTrace")
		{
            const auto& frames = stoppedFrames;
			std::ostringstream os;
			os << "{\"stackFrames\":[";
			for (size_t i = 0; i < frames.size(); ++i)
			{
				if (i) os << ",";
				std::string name = frames[i].functionName.empty() ? ("function #" + std::to_string(frames[i].functionId)) : frames[i].functionName;
				std::string frameSourcePath = SourcePathFromFileId(frames[i].file);
				std::string frameSourceName = SourceNameFromPath(frameSourcePath);
				os << "{\"id\":" << frames[i].frameId << ",\"name\":\"" << JsonEscape(name)
					<< "\",\"line\":" << frames[i].line << ",\"column\":1,\"source\":{\"path\":\""
					<< JsonEscape(frameSourcePath) << "\",\"name\":\"" << JsonEscape(frameSourceName) << "\"}}";
			}
			os << "],\"totalFrames\":" << frames.size() << "}";
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "scopes")
		{
			int frameId = JsonInt(arguments, "frameId", 0);
            if (frameId < 0 || frameId >= static_cast<int>(stoppedFrames.size()))
            { SendResponse(requestSeq, command, "{}", false, "Invalid stack frame"); return; }
			std::ostringstream os;
			os << "{\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":" << (1000 + frameId) << ",\"expensive\":false}]}";
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "variables")
		{
			int ref = JsonInt(arguments, "variablesReference", 1000);
            static const std::vector<DebugVariable> empty;
            const std::vector<DebugVariable>* vars = &empty;
            auto childIt = variableReferences.find(ref);
            if (childIt != variableReferences.end()) vars = childIt->second;
            else if (ref >= 1000 && ref < 1064) vars = &GetFrameVariables(ref - 1000);
            std::ostringstream os;
            WriteVariables(os, *vars);
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "evaluate")
		{
			std::string expression = JsonString(arguments, "expression");
			int frameId = JsonInt(arguments, "frameId", 0);
			while (!expression.empty() && isspace((unsigned char)expression.front()))
				expression.erase(expression.begin());
			while (!expression.empty() && isspace((unsigned char)expression.back()))
				expression.pop_back();

            const auto& vars = GetFrameVariables(frameId);
			const DebugVariable* found = nullptr;
			for (const DebugVariable& var : vars)
			{
				if (var.name == expression)
				{
					found = &var;
					break;
				}
			}

			std::ostringstream os;
			if (found)
			{
				const int reference = StoreVariableChildren(found->children);
				os << "{\"result\":\"" << JsonEscape(found->value) << "\",\"type\":\""
					<< JsonEscape(found->type) << "\",\"variablesReference\":" << reference << "}";
			}
			else
			{
				os << "{\"result\":\"not available\",\"variablesReference\":0}";
			}
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "exceptionInfo")
		{
            const std::string& description = lastStopError;
			std::ostringstream os;
			os << "{\"exceptionId\":\"NeoScript.RuntimeError\",\"description\":\"" << JsonEscape(description)
				<< "\",\"breakMode\":\"always\"}";
			SendResponse(requestSeq, command, os.str());
		}
        else if (command == "pause")
        {
            if (!running || !dbg || noDebugMode) SendResponse(requestSeq, command, "{}", false, "No running debug script");
            else { dbg->Pause(instance); SendResponse(requestSeq, command); }
        }
        else if (command == "setExceptionBreakpoints") SendResponse(requestSeq, command);
        else if (command == "disconnect")
		{
			SendResponse(requestSeq, command);
            running = false;
            StopCapture();
			terminated = true;
		}
		else
		{
			SendResponse(requestSeq, command, "{}", false, "Unsupported DAP request");
		}
	}
};

static int RunDebugAdapter(CNeoLoader* loader)
{
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	int dapOutFd = _dup(_fileno(stdout));
	if (dapOutFd >= 0)
	{
#ifdef _WIN32
		_setmode(dapOutFd, _O_BINARY);
#endif
		g_DapOutput = _fdopen(dapOutFd, "wb");
	}
	else
	{
		g_DapOutput = stdout;
	}
	NeoDapSession session;
	session.loader = loader;
    DapInput input;
    std::string body;
    while (!session.terminated)
    {
        int state = input.Poll(body);
        if (state < 0) break;
        if (state > 0) session.Handle(body);
        if (!session.terminated && session.running) session.RunCurrent(false);
        else if (state == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    session.StopCapture();
	if (session.runtime)
	{
		if (session.instance) session.runtime->DestroyInstance(session.instance);
		if (session.program) session.runtime->DestroyProgram(session.program);
		DestroyRuntime(session.runtime);
	}
	if (g_DapOutput && g_DapOutput != stdout)
		fclose(g_DapOutput);
	g_DapOutput = stdout;
	return 0;
}

static bool IsNeoIdentifierChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '_';
}

static std::string GetLspLinePrefix(const std::string& source, int line, int character, bool wholePrefix = false)
{
    if (line < 0 || character < 0) return "";
    size_t begin = 0;
    for (int current = 0; current < line; ++current)
    {
        begin = source.find('\n', begin);
        if (begin == std::string::npos) return "";
        ++begin;
    }
    size_t cursor = begin;
    int units = 0;
    while (cursor < source.size() && source[cursor] != '\n' && source[cursor] != '\r' && units < character)
    {
        unsigned char lead = static_cast<unsigned char>(source[cursor]);
        size_t width = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        int cost = width == 4 ? 2 : 1;
        if (units + cost > character || width > source.size() - cursor) break;
        cursor += width;
        units += cost;
    }
    return wholePrefix ? source.substr(0, cursor) : source.substr(begin, cursor - begin);
}

static void GetLspCompletionContext(const std::string& linePrefix, std::string& module, std::string& prefix)
{
	module.clear();
	prefix.clear();

	size_t end = linePrefix.size();
	while (end > 0 && IsNeoIdentifierChar(linePrefix[end - 1]))
		--end;
	prefix = linePrefix.substr(end);
	if (end == 0 || linePrefix[end - 1] != '.')
		return;

	size_t moduleEnd = end - 1;
	size_t moduleBegin = moduleEnd;
	while (moduleBegin > 0 && IsNeoIdentifierChar(linePrefix[moduleBegin - 1]))
		--moduleBegin;
	module = linePrefix.substr(moduleBegin, moduleEnd - moduleBegin);
}

static const NeoBuiltinInfo* FindLspSignature(const std::vector<NeoBuiltinInfo>& builtins,
	const std::string& linePrefix, int& activeParameter)
{
    activeParameter = 0;
    struct Delimiter { char kind; size_t offset; int commas; };
    std::vector<Delimiter> stack;
    char quote = 0;
    bool escaped = false, lineComment = false, blockComment = false;
    for (size_t i = 0; i < linePrefix.size(); ++i)
    {
        char c = linePrefix[i];
        char next = i + 1 < linePrefix.size() ? linePrefix[i + 1] : 0;
        if (lineComment) { if (c == '\n') lineComment = false; continue; }
        if (blockComment) { if (c == '*' && next == '/') { blockComment = false; ++i; } continue; }
        if (quote)
        {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '/' && next == '/') { lineComment = true; ++i; continue; }
        if (c == '/' && next == '*') { blockComment = true; ++i; continue; }
        if (c == '"' || c == '\'') { quote = c; continue; }
        if (c == '(' || c == '[' || c == '{') stack.push_back({c, i, 0});
        else if (c == ')' || c == ']' || c == '}')
        {
            if (!stack.empty()) stack.pop_back();
        }
        else if (c == ',' && !stack.empty()) ++stack.back().commas;
    }
    auto call = std::find_if(stack.rbegin(), stack.rend(), [](const Delimiter& d) { return d.kind == '('; });
    if (call == stack.rend()) return nullptr;
    const size_t openParen = call->offset;
    activeParameter = call->commas;

	size_t nameEnd = openParen;
	while (nameEnd > 0 && isspace((unsigned char)linePrefix[nameEnd - 1]))
		--nameEnd;
	size_t nameBegin = nameEnd;
	while (nameBegin > 0 && IsNeoIdentifierChar(linePrefix[nameBegin - 1]))
		--nameBegin;
	if (nameBegin == nameEnd)
		return nullptr;
	const std::string name = linePrefix.substr(nameBegin, nameEnd - nameBegin);

	std::string module;
	if (nameBegin > 0 && linePrefix[nameBegin - 1] == '.')
	{
		size_t moduleEnd = nameBegin - 1;
		size_t moduleBegin = moduleEnd;
		while (moduleBegin > 0 && IsNeoIdentifierChar(linePrefix[moduleBegin - 1]))
			--moduleBegin;
		module = linePrefix.substr(moduleBegin, moduleEnd - moduleBegin);
	}

	for (const NeoBuiltinInfo& info : builtins)
	{
		if (info.name == name && info.module == module && !info.params.empty())
			return &info;
	}
	return nullptr;
}

static bool LspStartsWith(const std::string& value, const std::string& prefix)
{
	return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

static std::string BuildLspFunctionDetail(const NeoBuiltinInfo& info)
{
	std::ostringstream detail;
	if (!info.ret.empty())
		detail << info.ret << " ";
	if (!info.module.empty())
		detail << info.module << ".";
	detail << info.name << "(";
	for (size_t i = 0; i < info.params.size(); ++i)
	{
		if (i > 0)
			detail << ", ";
		detail << info.params[i];
	}
	detail << ")";
	return detail.str();
}

static void AppendLspCompletionItem(std::ostringstream& os, bool& first, std::set<std::string>& emitted,
	const std::string& label, int kind, const std::string& detail, const std::string& documentation = "")
{
	if (!emitted.insert(label).second)
		return;
	if (!first)
		os << ",";
	first = false;
	os << "{\"label\":\"" << JsonEscape(label) << "\",\"kind\":" << kind
		<< ",\"detail\":\"" << JsonEscape(detail) << "\",\"insertText\":\"" << JsonEscape(label) << "\"";
	if (!documentation.empty())
		os << ",\"documentation\":\"" << JsonEscape(documentation) << "\"";
	os << "}";
}

static void CollectDocumentFunctions(const std::string& source, std::vector<std::string>& functions)
{
	functions.clear();
	for (size_t pos = 0; (pos = source.find("fun", pos)) != std::string::npos; pos += 3)
	{
		if ((pos > 0 && IsNeoIdentifierChar(source[pos - 1])) ||
			(pos + 3 < source.size() && IsNeoIdentifierChar(source[pos + 3])))
			continue;
		size_t nameBegin = pos + 3;
		while (nameBegin < source.size() && isspace((unsigned char)source[nameBegin]))
			++nameBegin;
		size_t nameEnd = nameBegin;
		while (nameEnd < source.size() && IsNeoIdentifierChar(source[nameEnd]))
			++nameEnd;
		if (nameEnd > nameBegin)
			functions.push_back(source.substr(nameBegin, nameEnd - nameBegin));
	}
}

class NeoLspSession
{
public:
	std::unordered_map<std::string, std::string> documents;
	std::vector<NeoBuiltinInfo> builtins;
	bool terminated = false;

	NeoLspSession()
	{
		NeoVMSystem::GetBuiltins(builtins);
	}

	void SendResponse(const std::string& id, const std::string& result)
	{
		std::ostringstream os;
		os << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"result\":" << result << "}";
		DapSendMessage(os.str());
	}

	std::string BuildCompletionResult(const std::string& uri, int line, int character)
	{
		std::string source;
		auto document = documents.find(uri);
		if (document != documents.end())
			source = document->second;

		std::string module;
		std::string prefix;
		GetLspCompletionContext(GetLspLinePrefix(source, line, character), module, prefix);

		std::ostringstream os;
		os << "{\"isIncomplete\":false,\"items\":[";
		bool first = true;
		std::set<std::string> emitted;
		if (!module.empty())
		{
			for (const NeoBuiltinInfo& info : builtins)
			{
				if (info.module == module && LspStartsWith(info.name, prefix))
					AppendLspCompletionItem(os, first, emitted, info.name, 3, BuildLspFunctionDetail(info));
			}

			if (emitted.empty())
			{
				for (const NeoBuiltinInfo& info : builtins)
				{
					if ((info.module == "string" || info.module == "list" || info.module == "array" || info.module == "map" || info.module == "set" || info.module == "async") &&
						LspStartsWith(info.name, prefix))
						AppendLspCompletionItem(os, first, emitted, info.name, 2, BuildLspFunctionDetail(info));
				}
			}
		}
		else
		{
			static const char* keywords[] = {
				"var", "fun", "export", "if", "else", "while", "for", "foreach", "in", "continue", "break", "return",
				"true", "false", "null", "import", "public", "const", "switch", "case", "default", "yield", "sleep"
			};
			for (const char* keyword : keywords)
			{
				if (LspStartsWith(keyword, prefix))
					AppendLspCompletionItem(os, first, emitted, keyword, 14, "Neo Script keyword");
			}

			std::set<std::string> modules;
			for (const NeoBuiltinInfo& info : builtins)
			{
				if (info.module.empty())
				{
					if (LspStartsWith(info.name, prefix))
						AppendLspCompletionItem(os, first, emitted, info.name, 3, BuildLspFunctionDetail(info));
				}
				else if (info.module != "string" && info.module != "list" && info.module != "array" && info.module != "map" && info.module != "set" && info.module != "async")
					modules.insert(info.module);
			}
			for (const std::string& builtinModule : modules)
			{
				if (LspStartsWith(builtinModule, prefix))
					AppendLspCompletionItem(os, first, emitted, builtinModule, 9, "Neo Script module");
			}

			std::vector<std::string> functions;
			CollectDocumentFunctions(source, functions);
			for (const std::string& function : functions)
			{
				if (LspStartsWith(function, prefix))
					AppendLspCompletionItem(os, first, emitted, function, 3, "Neo Script function");
			}
		}
		os << "]}";
		return os.str();
	}

	std::string BuildSignatureHelpResult(const std::string& uri, int line, int character)
	{
		std::string source;
		auto document = documents.find(uri);
		if (document != documents.end())
			source = document->second;

		int activeParameter = 0;
		const NeoBuiltinInfo* info = FindLspSignature(builtins, GetLspLinePrefix(source, line, character, true), activeParameter);
		if (info == nullptr)
			return "null";

		std::ostringstream os;
		os << "{\"signatures\":[{\"label\":\"" << JsonEscape(BuildLspFunctionDetail(*info)) << "\",\"parameters\":[";
		for (size_t i = 0; i < info->params.size(); ++i)
		{
			if (i > 0)
				os << ",";
			os << "{\"label\":\"" << JsonEscape(info->params[i]) << "\"}";
		}
		os << "]}],\"activeSignature\":0,\"activeParameter\":" << activeParameter << "}";
		return os.str();
	}

    void SendError(const std::string& id, int code, const std::string& message)
    {
        DapSendMessage("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"error\":{\"code\":" +
            std::to_string(code) + ",\"message\":\"" + JsonEscape(message) + "\"}}");
    }
    void Handle(const std::string& body)
    {
        ProtocolJson json(body);
        yyjson_val* root = json.Root();
        if (!root) { SendError("null", -32700, "Invalid JSON"); return; }
        yyjson_val* idValue = JsonMember(root, "id");
        const bool request = idValue != nullptr;
        const std::string id = JsonValueText(idValue);
        const std::string method = JsonString(root, "method");
        if (!yyjson_is_obj(root) || JsonString(root, "jsonrpc") != "2.0" || method.empty() ||
            (request && !yyjson_is_str(idValue) && !yyjson_is_int(idValue) && !yyjson_is_null(idValue)))
        { SendError("null", -32600, "Invalid request"); return; }
        yyjson_val* params = JsonMember(root, "params");
        yyjson_val* document = JsonMember(params, "textDocument");
        const std::string uri = JsonString(document, "uri");
        yyjson_val* position = JsonMember(params, "position");
        if (method == "initialize")
        {
            if (request) SendResponse(id, "{\"capabilities\":{\"positionEncoding\":\"utf-16\",\"textDocumentSync\":1,\"completionProvider\":{\"triggerCharacters\":[\".\"]},\"signatureHelpProvider\":{\"triggerCharacters\":[\"(\",\",\"]}}}");
        }
        else if (method == "textDocument/didOpen")
        {
            if (!uri.empty()) documents[uri] = JsonString(document, "text");
        }
        else if (method == "textDocument/didChange")
        {
            yyjson_val* changes = JsonMember(params, "contentChanges");
            for (size_t i = 0; i < yyjson_arr_size(changes); ++i)
            {
                yyjson_val* change = yyjson_arr_get(changes, i);
                if (!uri.empty() && !JsonMember(change, "range")) documents[uri] = JsonString(change, "text");
            }
        }
        else if (method == "textDocument/didClose") documents.erase(uri);
        else if (method == "textDocument/completion")
        {
            if (request) SendResponse(id, BuildCompletionResult(uri, JsonInt(position, "line"), JsonInt(position, "character")));
        }
        else if (method == "textDocument/signatureHelp")
        {
            if (request) SendResponse(id, BuildSignatureHelpResult(uri, JsonInt(position, "line"), JsonInt(position, "character")));
        }
        else if (method == "shutdown") { if (request) SendResponse(id, "null"); }
        else if (method == "exit") terminated = true;
        else if (request) SendError(id, -32601, "Method not found");
    }

};

static int RunLanguageServer()
{
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	NeoLspSession session;
	std::string body;
	while (!session.terminated && DapReadMessage(body))
		session.Handle(body);
	return 0;
}

#ifdef _WIN32
#include <windows.h>

#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define DISABLE_NEWLINE_AUTO_RETURN  0x0008

void activateVirtualTerminal()
{
	HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD consoleMode;
	GetConsoleMode(handleOut, &consoleMode);
	consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	consoleMode |= DISABLE_NEWLINE_AUTO_RETURN;
	SetConsoleMode(handleOut, consoleMode);

	SetConsoleOutputCP(CP_UTF8);
}
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
	activateVirtualTerminal();
#endif
	CNeoLoader* pLoader = new CNeoLoader();
	NeoScript::NeoVMSystem::Initialize(pLoader);

	if (argc >= 2)
	{
		int exitCode = 0;
		std::string command = argv[1];
		if (command == "--list")
		{
			PrintSampleList();
		}
		else if (command == "--run" && argc >= 3)
		{
			exitCode = RunSample(pLoader, argv[2]);
		}
		else if (command == "--file" && argc >= 3)
		{
			bool putASM = false, debug = false, stats = false; // --file <script.ns> [--asm] [--debug] [--stats]
			std::vector<std::string> hostGlobals;
			for (int i = 3; i < argc; ++i)
			{
				std::string opt = argv[i];
				if (opt == "--asm")   putASM = true;
				else if (opt == "--debug") debug = true;
				else if (opt == "--stats") stats = true;
				else if (opt == "--globals" && i + 1 < argc)
				{
					std::string names = argv[++i];
					for (size_t begin = 0; begin < names.size();)
					{
						size_t end = names.find(',', begin);
						hostGlobals.push_back(names.substr(begin, end - begin));
						if (end == std::string::npos) break;
						begin = end + 1;
					}
				}
			}
			NeoScript::SNeoVMAllocStats s;
			exitCode = RunFile(pLoader, argv[2], putASM, debug, stats ? &s : nullptr, hostGlobals);
			if (stats)
			{
				printf("[ALLOC] str=%d map=%d list=%d array=%d buffer=%lld bytes set=%d cor=%d mod=%d async=%d closure=%d vec=%d pool=%lld bytes\n",
					s.strings, s.maps, s.lists, s.arrays, s.bufferBytes, s.sets, s.coroutines, s.modules, s.asyncs, s.closures, s.vectors,
					s.poolBytes);
				// 풀 페이지 밖의 문자열 힙. 계속 크면 유지 임계값을 낮춰야 한다.
				printf("[ALLOC] stringIdle=%lld bytes\n", s.stringIdleBytes);
			}
		}
		else if (command == "--smoke")
		{
			exitCode = RunSmokeSamples(pLoader);
		}
		else if (command == "--v2smoke")
		{
			extern int NeoScriptV2Smoke();
			exitCode = NeoScriptV2Smoke();
		}
		else if (command == "--bench")
		{
			exitCode = RunBenchmarks(pLoader);
		}
		else if (command == "--pooltrim")
		{
			exitCode = RunPoolTrim(pLoader);
		}
		else if (command == "--debug-smoke")
		{
			 exitCode = RunDebugSmoke();
		}
		else if (command == "--compiler-error-regression")
		{
			exitCode = RunCompilerErrorRegression();
		}
		else if (command == "--dap")
		{
			exitCode = RunDebugAdapter(pLoader);
		}
		else if (command == "--lsp")
		{
			exitCode = RunLanguageServer();
		}
		else
		{
			printf("usage: console.exe [--list | --run <sample> | --file <script.ns> [--asm] [--debug] | --smoke | --bench | --debug-smoke | --compiler-error-regression | --dap | --lsp]\n");
			exitCode = -1;
		}

		NeoScript::NeoVMSystem::Shutdown();
		delete pLoader;
		return exitCode;
	}

	bool blEnd = false;
	while (blEnd == false)
	{
		printf("\n");
		int idx = 0;
		printf("%d hello\n", idx++);
		printf("%d performace\n", idx++);
		printf("%d callback\n", idx++);
		printf("%d map_callback\n", idx++);
		printf("%d 9_times\n", idx++);
		printf("%d string\n", idx++);
		printf("%d list\n", idx++);
		printf("%d map\n", idx++);
		printf("%d contailer\n", idx++);
		printf("%d slice_run\n", idx++);
		printf("%d time_limit\n", idx++);
		printf("%d divide_by_zero\n", idx++);
		printf("%d delegate\n", idx++);
		printf("%d coroutine\n", idx++);
		printf("%d module\n", idx++);
		printf("%d http\n", idx++);
		printf("%d regression\n", idx++);
		printf("%d literal_totype\n", idx++);
		printf("%d cycle\n", idx++);
		printf("%d regex\n", idx++);

		printf("\nESC press to exit\n");
		printf("press the number and enter ...\n");

		std::string key = getKeyString();

		printf((key + "\n\n").c_str());

		if (key == "") break;
		else RunSample(pLoader, key);

		system("pause");
	}
	NeoScript::NeoVMSystem::Shutdown();
	delete pLoader;
    return 0;
}

