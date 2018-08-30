#include "stdafx.h"
#include "console_callback.h"
#include "../../NeoSource/Neo.h"

float Mul(float a, float b)
{
	return a * b;
}


void Sample1()
{
	int a = 10;
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

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true);
	if (pVM != NULL)
	{
		printf("Comile Success. Code : %d bytes !!\n\n", pVM->GetBytesSize());

		NeoHelper::Register(pVM, "Mul", Mul);
		NeoHelper::Register(pVM, "Sample1", Sample1);
			

		for (int i = 1; i < 10; i++)
		{
			DWORD t1 = GetTickCount();
			double r;
			pVM->Call<double>(&r, "Sum", 100, i);
			DWORD t2 = GetTickCount();

			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s (Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
			}
			else
				printf("Sum %d + %d = %lf (Elapse:%d)\n", 100, i, r, t2 - t1);
		}
		CNeoVM::ReleaseVM(pVM);
	}
	else
	{
		printf(err.c_str());
	}
	delete[] pFileBuffer;

    return 0;
}

