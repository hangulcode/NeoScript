#include "stdafx.h"
#include "console.h"
#include "../../NeoSource/Neo.h"
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
	printf("meta\n");
	printf("coroutine\n");
	printf("module\n");
	printf("http\n");
	printf("class\n");
	printf("regression\n");
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
	if (key == "13" || key == "meta") return SAMPLE_etc(pLoader, s_path + "meta.ns", "meta");
	if (key == "14" || key == "coroutine") return SAMPLE_etc(pLoader, s_path + "coroutine.ns", "test");
	if (key == "15" || key == "module") return SAMPLE_etc(pLoader, s_path + "module.ns", nullptr);
	if (key == "16" || key == "http") return SAMPLE_etc(pLoader, s_path + "http.ns", nullptr);
	if (key == "17" || key == "class") return SAMPLE_etc(pLoader, s_path + "class.ns", nullptr);
	if (key == "18" || key == "regression") return SAMPLE_etc(pLoader, s_path + "compiler_regression.ns", nullptr);

	printf("unknown sample: %s\n", key.c_str());
	return -1;
}

static int RunSmokeSamples(INeoLoader* pLoader)
{
	const char* samples[] = { "hello", "string", "list", "map", "delegate", "meta", "coroutine", "regression" };
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

class ScopedNeoExecPool
{
	NeoExecContextPool* m_pool = nullptr;
public:
	ScopedNeoExecPool()
	{
		m_pool = NeoExecContextPool_Create();
	}
	~ScopedNeoExecPool()
	{
		NeoExecContextPool_Destroy(m_pool);
	}
	NeoExecContextPool* Get() const
	{
		return m_pool;
	}
	NeoLoadVMParam MakeLoadParam() const
	{
		NeoLoadVMParam vparam;
		vparam.execPool = m_pool;
		return vparam;
	}
};

static int RunFile(CNeoLoader* pLoader, const std::string& filename, bool putASM, bool debug)
{
	void* pFileBuffer = nullptr;
	int iFileLen = 0;
	if (pLoader->Load(filename.c_str(), pFileBuffer, iFileLen) == false)
	{
		printf("file read error: %s\n", filename.c_str());
		return -1;
	}

	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	param.putASM = putASM;
	param.debug = debug;

	ScopedNeoExecPool execPool;
	NeoLoadVMParam vparam = execPool.MakeLoadParam();
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param, &vparam);
	pLoader->Unload(filename.c_str(), pFileBuffer, iFileLen);

	if (pVM == nullptr)
	{
		if (err.empty() == false)
			printf("%s\n", err.c_str());
		return -1;
	}

	int exitCode = 0;
	if (pVM->IsLastErrorMsg())
	{
		printf("Error - VM Call : %s\n", pVM->GetLastErrorMsg());
		pVM->ClearLastErrorMsg();
		exitCode = -1;
	}

	INeoVM::ReleaseVM(pVM);
	return exitCode;
}

static int RunBenchCase(const char* name, const char* source, const char* functionName, int arg, int iterations)
{
	std::string src = source;
	std::string err;
	NeoCompilerParam param(src.data(), (int)src.size());
	param.err = &err;
	param.putASM = false;
	param.debug = false;

	auto compileStart = std::chrono::steady_clock::now();
	ScopedNeoExecPool execPool;
	NeoLoadVMParam vparam = execPool.MakeLoadParam();
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param, &vparam);
	auto compileEnd = std::chrono::steady_clock::now();
	if (pVM == nullptr)
	{
		printf("[bench] %-14s compile failed: %s\n", name, err.c_str());
		return -1;
	}

	NS_FLOAT result = 0;
	auto runStart = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i)
	{
		pVM->Call(&result, functionName, arg);
		if (pVM->IsLastErrorMsg())
		{
			printf("[bench] %-14s runtime error: %s\n", name, pVM->GetLastErrorMsg());
			INeoVM::ReleaseVM(pVM);
			return -1;
		}
	}
	auto runEnd = std::chrono::steady_clock::now();

	printf("[bench] %-14s compile=%8.3f ms run=%8.3f ms iter=%d arg=%d result=%g\n",
		name,
		ElapsedMs(compileStart, compileEnd),
		ElapsedMs(runStart, runEnd),
		iterations,
		arg,
		(double)result);

	INeoVM::ReleaseVM(pVM);
	return 0;
}

static int RunBenchmarks()
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
		if (RunBenchCase(bench.name, bench.source, bench.functionName, bench.arg, bench.iterations) != 0)
			return -1;
	}
	return 0;
}

class DebugSmokeListener : public INeoVMDebugListener
{
public:
	int stopCount = 0;
	NeoDebugLocation lastLocation;
	NeoDebugStopReason lastReason = NEO_DEBUG_STOP_NONE;

	virtual void OnNeoDebugStopped(INeoVMWorker* worker, const NeoDebugLocation& location, NeoDebugStopReason reason)
	{
		++stopCount;
		lastLocation = location;
		lastReason = reason;
		printf("[debug-smoke] stopped line=%d op=%d reason=%d\n", location.line, location.opIndex, (int)reason);
	}
};

static int RunDebugSmoke()
{
	const char* source =
		"var a = 1;\n"
		"var b = 2;\n"
		"var c = a + b;\n"
		"print(c);\n";

	std::string err;
	NeoCompilerParam param(source, (int)strlen(source));
	param.err = &err;
	param.putASM = false;
	param.debug = true;
	ScopedNeoExecPool execPool;
	NeoLoadVMParam vparam = execPool.MakeLoadParam();

	INeoVM* pVM = INeoVM::CompileAndLoadVM(param, &vparam);
	if (pVM == nullptr)
	{
		printf("[debug-smoke] compile failed: %s\n", err.c_str());
		return -1;
	}

	INeoVMWorker* worker = pVM->GetMainWorker();
	std::vector<int> executableLines;
	worker->DebugGetExecutableLines(executableLines);
	if (std::find(executableLines.begin(), executableLines.end(), 1) == executableLines.end() ||
		std::find(executableLines.begin(), executableLines.end(), 4) == executableLines.end())
	{
		printf("[debug-smoke] executable line metadata failed\n");
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	DebugSmokeListener listener;
	worker->DebugSetListener(&listener);
	worker->DebugSetBreakpoints(std::vector<int>{ 3 });

	pVM->PCall(pVM->GetMainWorkerID());
	if (listener.stopCount != 1 || worker->DebugIsPaused() == false || listener.lastLocation.line != 3)
	{
		printf("[debug-smoke] breakpoint failed count=%d paused=%d line=%d\n",
			listener.stopCount, worker->DebugIsPaused() ? 1 : 0, listener.lastLocation.line);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	std::vector<NeoDebugStackFrame> frames;
	worker->DebugGetStackTrace(frames);
	std::vector<NeoDebugVariable> vars;
	worker->DebugGetFrameVariables(0, vars);
	printf("[debug-smoke] frames=%d vars=%d\n", (int)frames.size(), (int)vars.size());
	if (frames.empty() || vars.empty())
	{
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	if (frames[0].functionName.empty())
	{
		printf("[debug-smoke] function name failed\n");
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	bool foundA = false;
	bool foundB = false;
	for (const NeoDebugVariable& var : vars)
	{
		if (var.name == "a")
			foundA = true;
		if (var.name == "b")
			foundB = true;
	}
	if (!foundA || !foundB)
	{
		printf("[debug-smoke] variable names failed a=%d b=%d\n", foundA ? 1 : 0, foundB ? 1 : 0);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	worker->DebugContinue();
	worker->Run();
	if (worker->DebugIsPaused())
	{
		printf("[debug-smoke] continue failed\n");
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	INeoVM::ReleaseVM(pVM);

	const char* collectionSource =
		"var data = { \"name\": \"Neo\", \"items\": [1, 2] };\n"
		"var marker = 1;\n";
	err.clear();
	NeoCompilerParam collectionParam(collectionSource, (int)strlen(collectionSource));
	collectionParam.err = &err;
	collectionParam.putASM = false;
	collectionParam.debug = true;
	pVM = INeoVM::CompileAndLoadVM(collectionParam, &vparam);
	if (pVM == nullptr)
	{
		printf("[debug-smoke] collection compile failed: %s\n", err.c_str());
		return -1;
	}

	worker = pVM->GetMainWorker();
	DebugSmokeListener collectionListener;
	worker->DebugSetListener(&collectionListener);
	worker->DebugSetBreakpoints(std::vector<int>{ 2 });
	pVM->PCall(pVM->GetMainWorkerID());
	std::vector<NeoDebugVariable> collectionVars;
	worker->DebugGetFrameVariables(0, collectionVars);
	const NeoDebugVariable* data = nullptr;
	for (const NeoDebugVariable& var : collectionVars)
	{
		if (var.name == "data")
		{
			data = &var;
			break;
		}
	}
	bool foundName = false;
	bool foundItems = false;
	bool itemsExpanded = false;
	if (data != nullptr)
	{
		for (const NeoDebugVariable& child : data->children)
		{
			if (child.name == "[\"name\"]" && child.value == "Neo")
				foundName = true;
			if (child.name == "[\"items\"]")
			{
				foundItems = true;
				itemsExpanded = child.children.size() == 2;
			}
		}
	}
	if (worker->DebugIsPaused() == false || data == nullptr || !foundName || !foundItems || !itemsExpanded)
	{
		printf("[debug-smoke] collection variables failed paused=%d data=%d name=%d items=%d expanded=%d\n",
			worker->DebugIsPaused() ? 1 : 0, data ? 1 : 0, foundName ? 1 : 0, foundItems ? 1 : 0, itemsExpanded ? 1 : 0);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	worker->DebugContinue();
	worker->Run();
	INeoVM::ReleaseVM(pVM);

	const char* stepSource =
		"fun add(var x) {\n"
		"    var y = x + 1;\n"
		"    return y;\n"
		"}\n"
		"var a = 1;\n"
		"var b = add(a);\n"
		"var c = b + 1;\n";
	err.clear();
	NeoCompilerParam stepParam(stepSource, (int)strlen(stepSource));
	stepParam.err = &err;
	stepParam.putASM = false;
	stepParam.debug = true;
	pVM = INeoVM::CompileAndLoadVM(stepParam, &vparam);
	if (pVM == nullptr)
	{
		printf("[debug-smoke] step compile failed: %s\n", err.c_str());
		return -1;
	}

	worker = pVM->GetMainWorker();
	DebugSmokeListener stepListener;
	worker->DebugSetListener(&stepListener);
	worker->DebugSetBreakpoints(std::vector<int>{ 6 });
	pVM->PCall(pVM->GetMainWorkerID());
	if (worker->DebugIsPaused() == false || stepListener.lastLocation.line != 6)
	{
		printf("[debug-smoke] step breakpoint failed line=%d\n", stepListener.lastLocation.line);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	worker->DebugStepOver();
	worker->Run();
	if (worker->DebugIsPaused() == false || worker->DebugGetLocation().line != 7)
	{
		printf("[debug-smoke] step over failed line=%d\n", worker->DebugGetLocation().line);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	worker->DebugSetBreakpoints(std::vector<int>{});
	worker->DebugContinue();
	worker->Run();
	INeoVM::ReleaseVM(pVM);

	err.clear();
	pVM = INeoVM::CompileAndLoadVM(stepParam, &vparam);
	if (pVM == nullptr)
	{
		printf("[debug-smoke] step-out compile failed: %s\n", err.c_str());
		return -1;
	}

	worker = pVM->GetMainWorker();
	DebugSmokeListener outListener;
	worker->DebugSetListener(&outListener);
	worker->DebugSetBreakpoints(std::vector<int>{ 2 });
	pVM->PCall(pVM->GetMainWorkerID());
	if (worker->DebugIsPaused() == false || outListener.lastLocation.line != 2)
	{
		printf("[debug-smoke] step-out breakpoint failed line=%d\n", outListener.lastLocation.line);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	worker->DebugStepOut();
	worker->Run();
	if (worker->DebugIsPaused() == false || worker->DebugGetLocation().callDepth != 0)
	{
		printf("[debug-smoke] step out failed line=%d depth=%d\n",
			worker->DebugGetLocation().line, worker->DebugGetLocation().callDepth);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}

	worker->DebugContinue();
	worker->Run();
	INeoVM::ReleaseVM(pVM);

	const char* exportSource =
		"export fun Time9(var n)\n"
		"{\n"
		"    for(var i in 1, 10, 1)\n"
		"    {\n"
		"        var value = n * i;\n"
		"        print(n..\" x \"..i..\" = \"..value);\n"
		"    }\n"
		"}\n";
	err.clear();
	NeoCompilerParam exportParam(exportSource, (int)strlen(exportSource));
	exportParam.err = &err;
	exportParam.putASM = false;
	exportParam.debug = true;
	pVM = INeoVM::CompileAndLoadVM(exportParam, &vparam);
	if (pVM == nullptr)
	{
		printf("[debug-smoke] export compile failed: %s\n", err.c_str());
		return -1;
	}

	worker = pVM->GetMainWorker();
	DebugSmokeListener exportListener;
	worker->DebugSetListener(&exportListener);
	worker->DebugSetBreakpoints(std::vector<int>{ 5 });
	pVM->PCall(pVM->GetMainWorkerID());
	int time9Id = worker->FindFunction("Time9");
	std::vector<VarInfo> time9Args;
	VarInfo time9Arg;
	worker->Var_SetInt(&time9Arg, 9);
	time9Args.push_back(time9Arg);
	if (time9Id < 0 || worker->Start(time9Id, time9Args) == false)
	{
		printf("[debug-smoke] export call failed: %s\n", pVM->IsLastErrorMsg() ? pVM->GetLastErrorMsg() : "");
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	if (worker->DebugIsPaused() == false || exportListener.lastLocation.line != 5)
	{
		printf("[debug-smoke] export breakpoint failed paused=%d line=%d\n",
			worker->DebugIsPaused() ? 1 : 0, exportListener.lastLocation.line);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	std::vector<NeoDebugVariable> exportVars;
	worker->DebugGetFrameVariables(0, exportVars);
	bool foundN = false;
	bool foundTemp = false;
	for (const NeoDebugVariable& var : exportVars)
	{
		if (var.name == "n" && var.value == "9")
			foundN = true;
		if (var.name.rfind("local", 0) == 0 || var.name.rfind("temp", 0) == 0)
			foundTemp = true;
	}
	if (!foundN || foundTemp)
	{
		printf("[debug-smoke] export vars failed n=%d temp=%d\n", foundN ? 1 : 0, foundTemp ? 1 : 0);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	worker->DebugContinue();
	worker->Run();
	INeoVM::ReleaseVM(pVM);

	const char* errorSource =
		"var a = 1;\n"
		"var b = 0;\n"
		"var c = a / b;\n";
	err.clear();
	NeoCompilerParam errorParam(errorSource, (int)strlen(errorSource));
	errorParam.err = &err;
	errorParam.putASM = false;
	errorParam.debug = true;
	pVM = INeoVM::CompileAndLoadVM(errorParam, &vparam);
	if (pVM == nullptr)
	{
		printf("[debug-smoke] exception compile failed: %s\n", err.c_str());
		return -1;
	}

	worker = pVM->GetMainWorker();
	DebugSmokeListener errorListener;
	worker->DebugSetListener(&errorListener);
	pVM->PCall(pVM->GetMainWorkerID());
	if (worker->DebugIsPaused() == false || errorListener.lastReason != NEO_DEBUG_STOP_EXCEPTION || pVM->IsLastErrorMsg() == false)
	{
		printf("[debug-smoke] exception failed paused=%d reason=%d err=%d\n",
			worker->DebugIsPaused() ? 1 : 0, (int)errorListener.lastReason, pVM->IsLastErrorMsg() ? 1 : 0);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	INeoVM::ReleaseVM(pVM);

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
	err.clear();
	NeoCompilerParam shortCircuitParam(shortCircuitSource, (int)strlen(shortCircuitSource));
	shortCircuitParam.err = &err;
	shortCircuitParam.putASM = false;
	shortCircuitParam.debug = false;
	pVM = INeoVM::CompileAndLoadVM(shortCircuitParam, &vparam);
	int shortCircuitResult = 0;
	bool shortCircuitReturn = false;
	int foreachIndexedResult = 0;
	if (pVM == nullptr ||
		pVM->Call(&shortCircuitResult, "ShortCircuitTest") == false ||
		pVM->Call(&shortCircuitReturn, "ShortCircuitReturn") == false ||
		pVM->Call(&foreachIndexedResult, "ForeachIndexed") == false ||
		pVM->IsLastErrorMsg())
	{
		printf("[short-circuit] execution failed: %s\n", pVM != nullptr && pVM->IsLastErrorMsg() ? pVM->GetLastErrorMsg() : err.c_str());
		if (pVM != nullptr)
			INeoVM::ReleaseVM(pVM);
		return -1;
	}
	if (shortCircuitResult != 7 || shortCircuitReturn == false || foreachIndexedResult != 7)
	{
		printf("[short-circuit] invalid result=%d return=%d foreach=%d\n", shortCircuitResult, shortCircuitReturn ? 1 : 0, foreachIndexedResult);
		INeoVM::ReleaseVM(pVM);
		return -1;
	}
	INeoVM::ReleaseVM(pVM);

	const char* invalidConditionSources[] =
	{
		"fun InvalidIf() { if () {} }",
		"fun InvalidWhile() { while () {} }"
	};
	for (const char* invalidSource : invalidConditionSources)
	{
		err.clear();
		NeoCompilerParam invalidParam(invalidSource, (int)strlen(invalidSource));
		invalidParam.err = &err;
		pVM = INeoVM::CompileAndLoadVM(invalidParam, &vparam);
		if (pVM != nullptr || err.find("expected an expression") == std::string::npos)
		{
			printf("[parser] empty condition was accepted: %s\n", err.c_str());
			if (pVM != nullptr)
				INeoVM::ReleaseVM(pVM);
			return -1;
		}
	}
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
	};

	int passed = 0;
	for (const ErrorCase& test : cases)
	{
		std::string source = test.source;
		std::string err;
		NeoCompilerParam param(source.data(), (int)source.size());
		param.err = &err;
		ScopedNeoExecPool execPool;
		NeoLoadVMParam vparam = execPool.MakeLoadParam();
		INeoVM* vm = INeoVM::CompileAndLoadRunVM(param, &vparam);
		bool rejected = vm == nullptr;
		if (vm != nullptr)
			INeoVM::ReleaseVM(vm);
		if (rejected && err.find(test.expected) != std::string::npos)
		{
			++passed;
			continue;
		}
		printf("[compiler-error] %s failed: %s\n", test.name, err.c_str());
	}

	const ErrorCase mutationCases[] =
	{
		{ "list-append-during-foreach", "var values = [1]; foreach(var value in values) values.append(2);", "collection modified during foreach" },
		{ "map-insert-during-foreach", "var values = { \"a\": 1 }; foreach(var key, value in values) values[\"b\"] = 2;", "collection modified during foreach" },
		{ "map-remove-during-foreach", "var values = { \"a\": 1 }; foreach(var key, value in values) values[key] = null;", "collection modified during foreach" },
	};
	for (const ErrorCase& test : mutationCases)
	{
		std::string source = test.source;
		std::string err;
		NeoCompilerParam param(source.data(), (int)source.size());
		param.err = &err;
		ScopedNeoExecPool execPool;
		NeoLoadVMParam vparam = execPool.MakeLoadParam();
		INeoVM* vm = INeoVM::CompileAndLoadRunVM(param, &vparam);
		bool rejected = vm != nullptr && vm->IsLastErrorMsg() && std::string(vm->GetLastErrorMsg()).find(test.expected) != std::string::npos;
		if (vm != nullptr)
			INeoVM::ReleaseVM(vm);
		if (rejected)
		{
			++passed;
			continue;
		}
		printf("[foreach-mutation] %s failed: %s\n", test.name, err.c_str());
	}

	const int total = _countof(cases) + _countof(mutationCases);
	printf("Compiler error regression %s : %d/%d\n", passed == total ? "PASS" : "FAIL", passed, total);
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

class NeoDapSession : public INeoVMDebugListener
{
public:
	CNeoLoader* loader = nullptr;
	int seq = 1;
	INeoVM* vm = nullptr;
	INeoVMWorker* worker = nullptr;
	NeoExecContextPool* execPool = nullptr;
	std::string sourcePath;
	std::vector<std::string> sourceFiles;
	std::map<int, std::vector<int>> breakpointsByFile;
	std::set<int> executableLines;
	std::map<int, std::set<int>> executableLinesByFile;
	NeoDebugStopReason lastStopReason = NEO_DEBUG_STOP_NONE;
	NeoDebugLocation lastStopLocation;
	bool terminated = false;
	bool initialRunStarted = false;
	bool noDebugMode = false;
	int nextVariableReference = 1000000;
	std::map<int, std::vector<NeoDebugVariable>> variableReferences;

	void ClearVariableReferences()
	{
		variableReferences.clear();
		nextVariableReference = 1000000;
	}

	int StoreVariableChildren(const std::vector<NeoDebugVariable>& children)
	{
		if (children.empty())
			return 0;
		const int reference = nextVariableReference++;
		variableReferences[reference] = children;
		return reference;
	}

	void WriteVariables(std::ostringstream& os, const std::vector<NeoDebugVariable>& vars)
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

	virtual void OnNeoDebugStopped(INeoVMWorker* w, const NeoDebugLocation& location, NeoDebugStopReason reason)
	{
		ClearVariableReferences();
		lastStopReason = reason;
		lastStopLocation = location;
		const char* reasonText = "pause";
		if (reason == NEO_DEBUG_STOP_BREAKPOINT)
			reasonText = "breakpoint";
		else if (reason == NEO_DEBUG_STOP_STEP)
			reasonText = "step";
		else if (reason == NEO_DEBUG_STOP_EXCEPTION)
			reasonText = "exception";
		std::ostringstream os;
		os << "{\"seq\":" << seq++ << ",\"type\":\"event\",\"event\":\"stopped\",\"body\":{\"reason\":\""
			<< reasonText << "\",\"threadId\":1,\"allThreadsStopped\":true";
		if (reason == NEO_DEBUG_STOP_EXCEPTION && vm && vm->IsLastErrorMsg())
			os << ",\"description\":\"" << JsonEscape(vm->GetLastErrorMsg()) << "\"";
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
		if (!worker)
			return;
		std::vector<NeoDebugBreakpoint> activeBreakpoints;
		for (std::map<int, std::vector<int>>::const_iterator it = breakpointsByFile.begin(); it != breakpointsByFile.end(); ++it)
		{
			int file = it->first;
			const std::vector<int>& lines = it->second;
			for (size_t i = 0; i < lines.size(); ++i)
			{
				int line = lines[i];
				if (IsExecutableLine(file, line))
				{
					NeoDebugBreakpoint bp;
					bp.file = file;
					bp.line = line;
					activeBreakpoints.push_back(bp);
				}
			}
		}
		worker->DebugSetBreakpoints(activeBreakpoints);
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
		NeoCompilerParam param(src.data(), (int)src.size());
		param.err = &err;
		param.putASM = false;
		param.debug = enableDebug;
		if (enableDebug)
		{
			param.debugSourcePath = fullPath.c_str();
			param.debugSourceFiles = &sourceFiles;
		}
		if (execPool == nullptr)
			execPool = NeoExecContextPool_Create();
		NeoLoadVMParam vparam;
		vparam.execPool = execPool;
		vm = INeoVM::CompileAndLoadVM(param, &vparam);
		if (vm == nullptr)
			return false;
		worker = vm->GetMainWorker();
		executableLines.clear();
		executableLinesByFile.clear();
		if (enableDebug)
		{
			worker->DebugSetListener(this);
			std::vector<int> lines;
			worker->DebugGetExecutableLines(lines);
			for (int line : lines)
				executableLines.insert(line);
			std::vector<NeoDebugLocation> locations;
			worker->DebugGetExecutableLocations(locations);
			for (size_t i = 0; i < locations.size(); ++i)
				executableLinesByFile[locations[i].file].insert(locations[i].line);
			ApplyBreakpoints();
		}
		lastStopReason = NEO_DEBUG_STOP_NONE;
		lastStopLocation = NeoDebugLocation();
		sourcePath = fullPath;
		return true;
	}

	void RunCurrent(bool initial)
	{
		if (worker == nullptr || terminated)
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
			vm->PCall(vm->GetMainWorkerID());
		}
		else
		{
			worker->Run();
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
		if (vm && vm->IsLastErrorMsg() &&
			(worker->DebugIsPaused() == false || lastStopReason == NEO_DEBUG_STOP_EXCEPTION))
		{
			std::string error = vm->GetLastErrorMsg();
			if (!error.empty() && error.back() != '\n')
				error += "\n";
			SendErrorOutput(error);
		}
		if (!worker->DebugIsPaused() && !terminated)
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
			if (worker) worker->DebugContinue();
			SendResponse(requestSeq, command, "{\"allThreadsContinued\":true}");
			RunCurrent(false);
		}
		else if (command == "stepIn")
		{
			if (worker) worker->DebugStepInto();
			SendResponse(requestSeq, command);
			RunCurrent(false);
		}
		else if (command == "next")
		{
			if (worker) worker->DebugStepOver();
			SendResponse(requestSeq, command);
			RunCurrent(false);
		}
		else if (command == "stepOut")
		{
			if (worker) worker->DebugStepOut();
			SendResponse(requestSeq, command);
			RunCurrent(false);
		}
		else if (command == "stackTrace")
		{
			std::vector<NeoDebugStackFrame> frames;
			if (worker) worker->DebugGetStackTrace(frames);
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
			std::vector<NeoDebugVariable> vars;
			std::map<int, std::vector<NeoDebugVariable>>::const_iterator childIt = variableReferences.find(ref);
			if (childIt != variableReferences.end())
				vars = childIt->second;
			else if (worker && ref >= 1000 && ref < 1000000)
				worker->DebugGetFrameVariables(ref - 1000, vars);
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

			std::vector<NeoDebugVariable> vars;
			if (worker) worker->DebugGetFrameVariables(frameId, vars);
			const NeoDebugVariable* found = nullptr;
			for (const NeoDebugVariable& var : vars)
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
			std::string description = vm && vm->IsLastErrorMsg() ? vm->GetLastErrorMsg() : "";
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
	if (session.vm)
		INeoVM::ReleaseVM(session.vm);
	if (session.execPool)
		NeoExecContextPool_Destroy(session.execPool);
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
			exitCode = RunFile(pLoader, argv[2], false, false);
		}
		else if (command == "--smoke")
		{
			exitCode = RunSmokeSamples(pLoader);
		}
		else if (command == "--bench")
		{
			exitCode = RunBenchmarks();
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
			printf("usage: console.exe [--list | --run <sample> | --file <script.ns> | --smoke | --bench | --debug-smoke | --compiler-error-regression | --dap | --lsp]\n");
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
		printf("%d meta\n", idx++);
		printf("%d coroutine\n", idx++);
		printf("%d module\n", idx++);
		printf("%d http\n", idx++);
		printf("%d class (In development)\n", idx++);
		printf("%d regression\n", idx++);

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

