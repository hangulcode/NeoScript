// Neo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <windows.h>
#include <string>
#include <iostream>
#include "../../NeoSource/Neo.h"
using namespace NeoScript;

bool        FileLoad(const char* pFileName, void*& pBuffer, int& iLen)
{
	FILE* fp = NULL;
	int error_t = fopen_s(&fp, pFileName, "rb");
	if (error_t != 0)
		return false;

	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	pBuffer = new char[iFileSize + 2];
	fread(pBuffer, iFileSize, 1, fp);
	fclose(fp);

	iLen = iFileSize;
	return true;
}


int main(int argc, const char** argv)
{
	if(argc != 2)
	{
		std::cout << "Usage: Neo.exe <your_neo_file>" << std::endl;;
		return -1;
	}

	const char* pFileName = argv[1];

	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad(pFileName, pFileBuffer, iFileLen))
	{
		std::cout << "file read error";
		return -1;
	}
	std::cout << pFileName << std::endl;;

	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	//param.putASM = true;
	//param.debug = true;

	INeoVM* pVM = INeoVM::CompileAndLoadVM(param);
	if (pVM != NULL)
	{
		DWORD dwCallTime = 0;
		if (pVM->IsLastErrorMsg())
		{
			std::cout << "Error - VM Call : " << pVM->GetLastErrorMsg() << "\n";
			pVM->ClearLastErrorMsg();
		}
		std::cout << "(Elapse:" << dwCallTime << ")\n";

		INeoVM::ReleaseVM(pVM);
	}
	delete[] pFileBuffer;
}


