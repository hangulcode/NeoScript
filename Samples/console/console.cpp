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

		printf("0 hello\n");
		printf("1 callback\n");
		printf("2 map_callback\n");
		printf("3 9_times\n");
		printf("4 string\n");
		printf("5 map\n");
		printf("6 list\n");
		printf("7 slice_run\n");
		printf("8 time_limit\n");
		printf("9 divide_by_zero\n");
		printf("10 delegate\n");
		printf("11 meta\n");
		printf("12 coroutine\n");
		printf("13 module\n");
		printf("14 http\n");
		printf("15 class (In development)\n");

		printf("ESC press to exit\n");
		printf("press the number and enter ...\n");

		std::string key = getKeyString();

		printf((key + "\n\n").c_str());

		if (key == "") break;
		else if (key ==  "0") SAMPLE_etc(pLoader, "hello.ns", nullptr);
		else if (key ==  "1") SAMPLE_callback(pLoader);
		else if (key ==  "2") SAMPLE_map_callback(pLoader);
		else if (key ==  "3") SAMPLE_9_times(pLoader);
		else if (key ==  "4") SAMPLE_etc(pLoader, "string.ns", nullptr);
		else if (key ==  "5") SAMPLE_etc(pLoader, "map.ns", nullptr);
		else if (key ==  "6") SAMPLE_etc(pLoader, "list.ns", nullptr);
		else if (key ==  "7") SAMPLE_slice_run(pLoader);
		else if (key ==  "8") SAMPLE_time_limit(pLoader);
		else if (key ==  "9") SAMPLE_etc(pLoader, "etc.ns", "divide_by_zero");
		else if (key == "10") SAMPLE_etc(pLoader, "delegate.ns", nullptr);
		else if (key == "11") SAMPLE_etc(pLoader, "meta.ns", "meta");
		else if (key == "12") SAMPLE_etc(pLoader, "coroutine.ns", "test");
		else if (key == "13") SAMPLE_etc(pLoader, "module.ns", nullptr);
		else if (key == "14") SAMPLE_etc(pLoader, "http.ns", nullptr);
		else if (key == "15") SAMPLE_etc(pLoader, "class.ns", nullptr);

		system("pause");
	}
	NeoScript::INeoVM::Shutdown();
	delete pLoader;
    return 0;
}

