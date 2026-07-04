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

	printf("unknown sample: %s\n", key.c_str());
	return -1;
}

static int RunSmokeSamples(INeoLoader* pLoader)
{
	const char* samples[] = { "hello", "string", "list", "map", "delegate", "meta", "coroutine" };
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
	return 0;
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

	virtual void OnNeoDebugStopped(INeoVMWorker* w, const NeoDebugLocation& location, NeoDebugStopReason reason)
	{
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
			if (worker) worker->DebugGetFrameVariables(ref - 1000, vars);
			std::ostringstream os;
			os << "{\"variables\":[";
			for (size_t i = 0; i < vars.size(); ++i)
			{
				if (i) os << ",";
				os << "{\"name\":\"" << JsonEscape(vars[i].name) << "\",\"type\":\"" << JsonEscape(vars[i].type)
					<< "\",\"value\":\"" << JsonEscape(vars[i].value) << "\",\"variablesReference\":0}";
			}
			os << "]}";
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
				os << "{\"result\":\"" << JsonEscape(found->value) << "\",\"type\":\""
					<< JsonEscape(found->type) << "\",\"variablesReference\":0}";
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
		else if (command == "--dap")
		{
			exitCode = RunDebugAdapter(pLoader);
		}
		else
		{
			printf("usage: console.exe [--list | --run <sample> | --smoke | --bench | --debug-smoke | --dap]\n");
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

