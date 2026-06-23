#include "stdafx.h"
#include "console.h"
#include "../../NeoSource/Neo.h"
#include <chrono>
#include <conio.h>

using namespace NeoScript;

class CNeoLoader : public INeoLoader
{
public:
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

int SAMPLE_callback(INeoLoader* pLoader);
int SAMPLE_map_callback(INeoLoader* pLoader);
int SAMPLE_9_times(INeoLoader* pLoader);
int SAMPLE_slice_run(INeoLoader* pLoader);
int SAMPLE_time_limit(INeoLoader* pLoader);
int SAMPLE_etc(INeoLoader* pLoader, const char*pFileName, const char* pFunctionName);

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

static int RunSample(INeoLoader* pLoader, const std::string& key)
{
	if (key == "0" || key == "hello") return SAMPLE_etc(pLoader, "hello.ns", nullptr);
	if (key == "1" || key == "performance" || key == "performace") return SAMPLE_etc(pLoader, "performance.ns", nullptr);
	if (key == "2" || key == "callback") return SAMPLE_callback(pLoader);
	if (key == "3" || key == "map_callback") return SAMPLE_map_callback(pLoader);
	if (key == "4" || key == "9_times") return SAMPLE_9_times(pLoader);
	if (key == "5" || key == "string") return SAMPLE_etc(pLoader, "string.ns", nullptr);
	if (key == "6" || key == "list") return SAMPLE_etc(pLoader, "list.ns", nullptr);
	if (key == "7" || key == "map") return SAMPLE_etc(pLoader, "map.ns", nullptr);
	if (key == "8" || key == "contailer") return SAMPLE_etc(pLoader, "contailer.ns", nullptr);
	if (key == "9" || key == "slice_run") return SAMPLE_slice_run(pLoader);
	if (key == "10" || key == "time_limit") return SAMPLE_time_limit(pLoader);
	if (key == "11" || key == "divide_by_zero") return SAMPLE_etc(pLoader, "etc.ns", "divide_by_zero");
	if (key == "12" || key == "delegate") return SAMPLE_etc(pLoader, "delegate.ns", nullptr);
	if (key == "13" || key == "meta") return SAMPLE_etc(pLoader, "meta.ns", "meta");
	if (key == "14" || key == "coroutine") return SAMPLE_etc(pLoader, "coroutine.ns", "test");
	if (key == "15" || key == "module") return SAMPLE_etc(pLoader, "module.ns", nullptr);
	if (key == "16" || key == "http") return SAMPLE_etc(pLoader, "http.ns", nullptr);
	if (key == "17" || key == "class") return SAMPLE_etc(pLoader, "class.ns", nullptr);

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

static int RunBenchCase(const char* name, const char* source, const char* functionName, int arg, int iterations)
{
	std::string src = source;
	std::string err;
	NeoCompilerParam param(src.data(), (int)src.size());
	param.err = &err;
	param.putASM = false;
	param.debug = false;

	auto compileStart = std::chrono::steady_clock::now();
	INeoVM* pVM = INeoVM::CompileAndLoadRunVM(param);
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
	};

	for (const BenchCase& bench : cases)
	{
		if (RunBenchCase(bench.name, bench.source, bench.functionName, bench.arg, bench.iterations) != 0)
			return -1;
	}
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
		else
		{
			printf("usage: console.exe [--list | --run <sample> | --smoke | --bench]\n");
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

