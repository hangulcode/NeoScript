#include "stdafx.h"
#include "console.h"
#include "../../NeoSource/Neo.h"
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

int main()
{
#ifdef _WIN32
	activateVirtualTerminal();
#endif
	CNeoLoader* pLoader = new CNeoLoader();
	NeoScript::INeoVM::Initialize(pLoader);

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
		else if (key == "0") SAMPLE_etc(pLoader, "hello.ns", nullptr);
		else if (key == "1") SAMPLE_etc(pLoader, "performance.ns", nullptr);
		else if (key == "2") SAMPLE_callback(pLoader);
		else if (key == "3") SAMPLE_map_callback(pLoader);
		else if (key == "4") SAMPLE_9_times(pLoader);
		else if (key == "5") SAMPLE_etc(pLoader, "string.ns", nullptr);
		else if (key == "6") SAMPLE_etc(pLoader, "list.ns", nullptr);
		else if (key == "7") SAMPLE_etc(pLoader, "map.ns", nullptr);
		else if (key == "8") SAMPLE_etc(pLoader, "contailer.ns", nullptr);
		else if (key == "9") SAMPLE_slice_run(pLoader);
		else if (key == "10") SAMPLE_time_limit(pLoader);
		else if (key == "11") SAMPLE_etc(pLoader, "etc.ns", "divide_by_zero");
		else if (key == "12") SAMPLE_etc(pLoader, "delegate.ns", nullptr);
		else if (key == "13") SAMPLE_etc(pLoader, "meta.ns", "meta");
		else if (key == "14") SAMPLE_etc(pLoader, "coroutine.ns", "test");
		else if (key == "15") SAMPLE_etc(pLoader, "module.ns", nullptr);
		else if (key == "16") SAMPLE_etc(pLoader, "http.ns", nullptr);
		else if (key == "17") SAMPLE_etc(pLoader, "class.ns", nullptr);

		system("pause");
	}
	NeoScript::INeoVM::Shutdown();
	delete pLoader;
    return 0;
}

