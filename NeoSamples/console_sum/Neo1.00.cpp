// Neo1.00.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "stdafx.h"
#include "Neo1.00.h"
#include "../../NeoSource/NeoVM.h"
#include <iostream>

float Mul(float a, float b)
{
	return a * b;
}




BOOL        FileLoad(const char* pFileName, void*& pBuffer, int& iLen)
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

int main()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("1.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}


	int iLenTemp = 1 * 1024 * 1024;
	BYTE* pCodeTemp = new BYTE[iLenTemp];

	int iCodeLen = 0;
	if (CNeoVM::Compile(pFileBuffer, iFileLen, pCodeTemp, iLenTemp, &iCodeLen, false) == TRUE)
	{
		printf("Comile Success !!\n");
		CNeoVM* pVM = CNeoVM::LoadVM(pCodeTemp, iCodeLen);
		if (NULL != pVM)
		{
			pVM->Register("Mul", Mul);

			int i = 5;
			for (int i = 1; i < 10; i++)
			{
				DWORD t1 = GetTickCount();
				float r = pVM->Call<float>("Sum", 100, i);
				DWORD t2 = GetTickCount();

				printf("\nSum %d + %d = %f (Elapse:%d)", 100, i, r, t2 - t1);
			}
			Sleep(10);
			CNeoVM::ReleaseVM(pVM);
		}
	}
	delete[] pFileBuffer;
	delete[] pCodeTemp;

    return 0;
}

