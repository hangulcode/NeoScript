#include "stdafx.h"
#include "console.h"
#include "../../NeoSource/Neo.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API (RunFile 등이 사용; Neo.h 와 공존)
#include <cctype>
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

using namespace NeoScript;

static std::string FullPathOrSelf(const std::string& path)
{
	char fullPath[_MAX_PATH];
	if (_fullpath(fullPath, path.c_str(), _MAX_PATH) != nullptr)
		return fullPath;
	return path;
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
	virtual bool        Load(const char* pFileName, void*& pBuffer, int& iLen)
	{
		FILE* fp = NULL;
		int error_t = fopen_s(&fp, pFileName, "rb");
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
	virtual void        Unload(const char* pFileName, void* pBuffer, int iLen)
	{
		delete [] pBuffer;
	}
	virtual const char* GetLibPath()
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

static int RunFile(CNeoLoader* pLoader, const std::string& filename, bool putASM, bool debug)
{
	void* pFileBuffer = nullptr;
	int iFileLen = 0;
	if (pLoader->Load(filename.c_str(), pFileBuffer, iFileLen) == false)
	{
		printf("file read error: %s\n", filename.c_str());
		return -1;
	}

	RuntimeDesc rd;
	// print 핸들러 미설정: 임의 스크립트를 그대로 실행하므로 엔진 내장 io_print 에 맡긴다.
	// (1인자 print 는 개행, 2인자 print(x,"") 는 개행 없이 이어붙임 — printFn 을 걸면 이 구분 소실)
	rd.nativeLoader = pLoader;                 // import 해석
	IRuntime* rt = CreateRuntime(rd);
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
	DestroyRuntime(rt);
	return exitCode;
}

static int RunBenchCase(const char* name, const char* source, const char* functionName, int arg, int iterations, void* nativeLoader)
{
	std::string src = source;

	auto compileStart = std::chrono::steady_clock::now();
	RuntimeDesc rd;
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
		{ "indexed-increment", "var values = [1]; values[0]++;", "table values do not support '++'" },
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
	for (char c : s)
	{
		switch (c)
		{
		case '\\': out += "\\\\"; break;
		case '"': out += "\\\""; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if ((unsigned char)c < 0x20)
			{
				char buf[7];
				snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
				out += buf;
			}
			else
			{
				out += c;
			}
			break;
		}
	}
	return out;
}

static FILE* g_DapOutput = stdout;
static std::mutex g_DapOutputMutex;

static bool DapReadMessage(std::string& body)
{
	std::string line;
	int contentLength = -1;
	while (std::getline(std::cin, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			break;
		const char* header = "Content-Length:";
		if (line.compare(0, strlen(header), header) == 0)
			contentLength = atoi(line.c_str() + strlen(header));
	}
	if (contentLength <= 0)
		return false;
	body.resize(contentLength);
	std::cin.read(&body[0], contentLength);
	return (int)std::cin.gcount() == contentLength;
}

static void DapSendMessage(const std::string& body)
{
	std::lock_guard<std::mutex> lock(g_DapOutputMutex);
	FILE* out = g_DapOutput ? g_DapOutput : stdout;
	fprintf(out, "Content-Length: %d\r\n\r\n", (int)body.size());
	fwrite(body.data(), 1, body.size(), out);
	fflush(out);
}

static int JsonInt(const std::string& body, const char* key, int defaultValue = 0)
{
	std::string pat = std::string("\"") + key + "\"";
	size_t pos = body.find(pat);
	if (pos == std::string::npos)
		return defaultValue;
	pos = body.find(':', pos + pat.size());
	if (pos == std::string::npos)
		return defaultValue;
	++pos;
	while (pos < body.size() && isspace((unsigned char)body[pos]))
		++pos;
	return atoi(body.c_str() + pos);
}

static bool JsonBool(const std::string& body, const char* key, bool defaultValue = false)
{
	std::string pat = std::string("\"") + key + "\"";
	size_t pos = body.find(pat);
	if (pos == std::string::npos)
		return defaultValue;
	pos = body.find(':', pos + pat.size());
	if (pos == std::string::npos)
		return defaultValue;
	++pos;
	while (pos < body.size() && isspace((unsigned char)body[pos]))
		++pos;
	if (body.compare(pos, 4, "true") == 0)
		return true;
	if (body.compare(pos, 5, "false") == 0)
		return false;
	return defaultValue;
}

static std::string JsonString(const std::string& body, const char* key)
{
	std::string pat = std::string("\"") + key + "\"";
	size_t pos = body.find(pat);
	if (pos == std::string::npos)
		return "";
	pos = body.find(':', pos + pat.size());
	if (pos == std::string::npos)
		return "";
	pos = body.find('"', pos);
	if (pos == std::string::npos)
		return "";
	std::string out;
	for (++pos; pos < body.size(); ++pos)
	{
		char c = body[pos];
		if (c == '"')
			break;
		if (c == '\\' && pos + 1 < body.size())
		{
			char n = body[++pos];
			if (n == 'n') out += '\n';
			else if (n == 'r') out += '\r';
			else if (n == 't') out += '\t';
			else out += n;
		}
		else
			out += c;
	}
	return out;
}

static std::vector<int> JsonBreakpointLines(const std::string& body)
{
	std::vector<int> lines;
	size_t pos = body.find("\"breakpoints\"");
	while (pos != std::string::npos)
	{
		pos = body.find("\"line\"", pos);
		if (pos == std::string::npos)
			break;
		int line = JsonInt(body.substr(pos), "line", -1);
		if (line > 0)
			lines.push_back(line);
		++pos;
	}
	return lines;
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
	std::map<int, std::vector<DebugVariable>> variableReferences;

	void ClearVariableReferences()
	{
		variableReferences.clear();
		nextVariableReference = 1000000;
	}

	int StoreVariableChildren(const std::vector<DebugVariable>& children)
	{
		if (children.empty())
			return 0;
		const int reference = nextVariableReference++;
		variableReferences[reference] = children;
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
		lastStopReason = reason;
		lastStopLocation = location;
		const char* reasonText = "pause";
		if (reason == DebugStopReason::Breakpoint)
			reasonText = "breakpoint";
		else if (reason == DebugStopReason::Step)
			reasonText = "step";
		else if (reason == DebugStopReason::Exception)
			reasonText = "exception";
		std::ostringstream os;
		os << "{\"seq\":" << seq++ << ",\"type\":\"event\",\"event\":\"stopped\",\"body\":{\"reason\":\""
			<< reasonText << "\",\"threadId\":1,\"allThreadsStopped\":true";
		StringView err;
		if (reason == DebugStopReason::Exception && runtime && runtime->PeekLastError(err))
			os << ",\"description\":\"" << JsonEscape(std::string(err.data(), err.size())) << "\"";
		os << "}}";
		DapSendMessage(os.str());
	}

	void SendResponse(int requestSeq, const std::string& command, const std::string& body = "{}", bool success = true, const std::string& message = "")
	{
		std::ostringstream os;
		os << "{\"seq\":" << seq++ << ",\"type\":\"response\",\"request_seq\":" << requestSeq
			<< ",\"success\":" << (success ? "true" : "false") << ",\"command\":\"" << command << "\"";
		if (!message.empty())
			os << ",\"message\":\"" << JsonEscape(message) << "\"";
		os << ",\"body\":" << body << "}";
		DapSendMessage(os.str());
	}

	void SendEvent(const std::string& event, const std::string& body = "{}")
	{
		std::ostringstream os;
		os << "{\"seq\":" << seq++ << ",\"type\":\"event\",\"event\":\"" << event << "\",\"body\":" << body << "}";
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
		return executableLines.empty() || executableLines.find(line) != executableLines.end();
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
		std::string fullPath = FullPathOrSelf(path);
		std::ifstream in(fullPath.c_str(), std::ios::binary);
		if (!in)
		{
			err = "cannot open program";
			return false;
		}
		std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		RuntimeDesc rd;
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
			return false;
		}
		program = cr.program;

		InstanceDesc idesc;
		idesc.runGlobalInit = false;              // BP 를 먼저 걸기 위해 전역 실행 지연
		instance = runtime->CreateInstance(program, idesc);
		dbg = runtime->GetDebugger();

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

	void RunCurrent(bool initial)
	{
		if (!instance || terminated)
			return;
		if (initial)
			SendOutput(noDebugMode ? "\x1b[32m[Neo Script] Run started.\x1b[0m\n" : "\x1b[32m[Neo Script] Debug started.\x1b[0m\n");
		fflush(stdout);
		int savedStdout = _dup(_fileno(stdout));
		int pipeFd[2] = { -1, -1 };
		bool captureActive = savedStdout >= 0 && _pipe(pipeFd, 4096, _O_BINARY) == 0;
		std::ios::fmtflags oldCoutFlags = std::cout.flags();
		std::thread outputThread;
		if (captureActive)
		{
			_dup2(pipeFd[1], _fileno(stdout));
			std::cout.setf(std::ios::unitbuf);
			outputThread = std::thread([this, readFd = pipeFd[0]]()
			{
				char buffer[512];
				for (;;)
				{
					int readSize = _read(readFd, buffer, sizeof(buffer));
					if (readSize <= 0)
						break;
					SendOutput(std::string(buffer, buffer + readSize));
				}
				_close(readFd);
			});
		}
		if (initial)
		{
			runtime->RunGlobalInit(instance);   // 전역 코드 실행
		}
		else if (dbg)
		{
			dbg->Run(instance);                 // Continue/Step 재개
		}
		fflush(stdout);
		if (captureActive)
		{
			_dup2(savedStdout, _fileno(stdout));
			_close(pipeFd[1]);
			if (outputThread.joinable())
				outputThread.join();
			std::cout.flags(oldCoutFlags);
		}
		if (savedStdout >= 0)
		{
			_close(savedStdout);
		}
		const bool paused = dbg && dbg->IsPaused(instance);
		StringView vmErr;
		if (runtime && runtime->PeekLastError(vmErr) &&
			(paused == false || lastStopReason == DebugStopReason::Exception))
		{
			std::string error(vmErr.data(), vmErr.size());
			if (!error.empty() && error.back() != '\n')
				error += "\n";
			SendErrorOutput(error);
		}
		if (!paused && !terminated)
		{
			SendOutput(noDebugMode ? "\x1b[32m[Neo Script] Run finished.\x1b[0m\n" : "\x1b[32m[Neo Script] Debug finished.\x1b[0m\n");
			terminated = true;
			SendEvent("terminated");
		}
	}

	void Handle(const std::string& body)
	{
		int requestSeq = JsonInt(body, "seq", 0);
		std::string command = JsonString(body, "command");
		if (command == "initialize")
		{
			SendResponse(requestSeq, command, "{\"supportsConfigurationDoneRequest\":true,\"supportsStepInTargetsRequest\":false,\"supportsExceptionInfoRequest\":true,\"supportsEvaluateForHovers\":true}");
		}
		else if (command == "launch")
		{
			std::string path = JsonString(body, "program");
			if (path.empty())
				path = JsonString(body, "path");
			bool noDebug = JsonBool(body, "noDebug", false);
			noDebugMode = noDebug;
			std::string libPath = JsonString(body, "libPath");
			if (!libPath.empty() && loader)
				loader->SetLibPath(libPath);
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
					std::string fullPath = FullPathOrSelf(path);
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
			std::string bpSourcePath = JsonString(body, "path");
			int bpFile = FileIdFromSourcePath(bpSourcePath);
			std::vector<int> breakpoints = JsonBreakpointLines(body);
			if (bpFile >= 0)
				breakpointsByFile[bpFile] = breakpoints;
			ApplyBreakpoints();
			std::ostringstream os;
			os << "{\"breakpoints\":[";
			for (size_t i = 0; i < breakpoints.size(); ++i)
			{
				if (i) os << ",";
				bool verified = (bpFile >= 0) && IsExecutableLine(bpFile, breakpoints[i]);
				os << "{\"verified\":" << (verified ? "true" : "false") << ",\"line\":" << breakpoints[i];
				if (!verified)
					os << ",\"message\":\"No executable Neo Script instruction on this line\"";
				os << "}";
			}
			os << "]}";
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "configurationDone")
		{
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
			std::vector<DebugStackFrame> frames;
			if (dbg) dbg->GetStackTrace(instance, frames);
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
			int frameId = JsonInt(body, "frameId", 0);
			std::ostringstream os;
			os << "{\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":" << (1000 + frameId) << ",\"expensive\":false}]}";
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "variables")
		{
			int ref = JsonInt(body, "variablesReference", 1000);
			std::vector<DebugVariable> vars;
			std::map<int, std::vector<DebugVariable>>::const_iterator childIt = variableReferences.find(ref);
			if (childIt != variableReferences.end())
				vars = childIt->second;
			else if (dbg && ref >= 1000 && ref < 1000000)
				dbg->GetFrameVariables(instance, ref - 1000, vars);
			std::ostringstream os;
			WriteVariables(os, vars);
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "evaluate")
		{
			std::string expression = JsonString(body, "expression");
			int frameId = JsonInt(body, "frameId", 0);
			while (!expression.empty() && isspace((unsigned char)expression.front()))
				expression.erase(expression.begin());
			while (!expression.empty() && isspace((unsigned char)expression.back()))
				expression.pop_back();

			std::vector<DebugVariable> vars;
			if (dbg) dbg->GetFrameVariables(instance, frameId, vars);
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
			StringView descErr;
			std::string description = (runtime && runtime->PeekLastError(descErr)) ? std::string(descErr.data(), descErr.size()) : "";
			std::ostringstream os;
			os << "{\"exceptionId\":\"NeoScript.RuntimeError\",\"description\":\"" << JsonEscape(description)
				<< "\",\"breakMode\":\"always\"}";
			SendResponse(requestSeq, command, os.str());
		}
		else if (command == "disconnect")
		{
			SendResponse(requestSeq, command);
			terminated = true;
		}
		else
		{
			SendResponse(requestSeq, command, "{}", true);
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
	std::string body;
	while (!session.terminated && DapReadMessage(body))
		session.Handle(body);
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

static std::string GetLspLinePrefix(const std::string& source, int line, int character)
{
	if (line < 0 || character < 0)
		return "";

	size_t begin = 0;
	for (int currentLine = 0; currentLine < line; ++currentLine)
	{
		begin = source.find('\n', begin);
		if (begin == std::string::npos)
			return "";
		++begin;
	}

	size_t end = source.find('\n', begin);
	if (end == std::string::npos)
		end = source.size();
	const size_t requestedCursor = begin + (size_t)character;
	const size_t cursor = requestedCursor < end ? requestedCursor : end;
	return source.substr(begin, cursor - begin);
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
	const size_t openParen = linePrefix.rfind('(');
	if (openParen == std::string::npos)
		return nullptr;

	for (size_t i = openParen + 1; i < linePrefix.size(); ++i)
	{
		if (linePrefix[i] == ',')
			++activeParameter;
	}

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
		INeoVM::GetBuiltins(builtins);
	}

	void SendResponse(int id, const std::string& result)
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
					if ((info.module == "string" || info.module == "list" || info.module == "map" || info.module == "async") &&
						LspStartsWith(info.name, prefix))
						AppendLspCompletionItem(os, first, emitted, info.name, 2, BuildLspFunctionDetail(info));
				}
			}
		}
		else
		{
			static const char* keywords[] = {
				"var", "fun", "export", "if", "else", "while", "for", "foreach", "in", "continue", "break", "return",
				"true", "false", "null", "import", "class", "yield", "sleep"
			};
			for (const char* keyword : keywords)
			{
				if (LspStartsWith(keyword, prefix))
					AppendLspCompletionItem(os, first, emitted, keyword, 14, "Neo Script keyword");
			}

			std::set<std::string> modules;
			for (const NeoBuiltinInfo& info : builtins)
			{
				if (info.module != "string" && info.module != "list" && info.module != "map" && info.module != "async")
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
		const NeoBuiltinInfo* info = FindLspSignature(builtins, GetLspLinePrefix(source, line, character), activeParameter);
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

	void Handle(const std::string& body)
	{
		const std::string method = JsonString(body, "method");
		const int id = JsonInt(body, "id", -1);
		if (method == "initialize")
		{
			SendResponse(id, "{\"capabilities\":{\"textDocumentSync\":1,\"completionProvider\":{\"triggerCharacters\":[\".\"]}}}");
		}
		else if (method == "textDocument/didOpen" || method == "textDocument/didChange")
		{
			const std::string uri = JsonString(body, "uri");
			if (!uri.empty())
				documents[uri] = JsonString(body, "text");
		}
		else if (method == "textDocument/didClose")
		{
			documents.erase(JsonString(body, "uri"));
		}
		else if (method == "textDocument/completion")
		{
			SendResponse(id, BuildCompletionResult(JsonString(body, "uri"), JsonInt(body, "line"), JsonInt(body, "character")));
		}
		else if (method == "textDocument/signatureHelp")
		{
			SendResponse(id, BuildSignatureHelpResult(JsonString(body, "uri"), JsonInt(body, "line"), JsonInt(body, "character")));
		}
		else if (method == "shutdown")
		{
			SendResponse(id, "null");
		}
		else if (method == "exit")
		{
			terminated = true;
		}
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
	NeoScript::INeoVM::Initialize(pLoader);

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
			for (int i = 3; i < argc; ++i)
			{
				std::string opt = argv[i];
				if (opt == "--asm")   putASM = true;
				else if (opt == "--debug") debug = true;
				else if (opt == "--stats") stats = true;
			}
			exitCode = RunFile(pLoader, argv[2], putASM, debug);
			if (stats)
			{
				// 스크립트/VM 정리 후 남아있는 할당 수. 전부 0 이어야 누수가 없다.
				NeoScript::SNeoVMAllocStats s;
				NeoScript::GetNeoVMAllocStats(s);
				printf("[ALLOC] str=%d map=%d list=%d set=%d cor=%d mod=%d async=%d vec=%d pool=%lld bytes\n",
					s.strings, s.maps, s.lists, s.sets, s.coroutines, s.modules, s.asyncs, s.vectors,
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

		NeoScript::INeoVM::Shutdown();
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

		printf("\nESC press to exit\n");
		printf("press the number and enter ...\n");

		std::string key = getKeyString();

		printf((key + "\n\n").c_str());

		if (key == "") break;
		else RunSample(pLoader, key);

		system("pause");
	}
	NeoScript::INeoVM::Shutdown();
	delete pLoader;
    return 0;
}

