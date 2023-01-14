#include "stdafx.h"
#include "../../NeoSource/Neo.h"

float Mul(float a, float b)
{
	return a * b;
}


float Sample1(NeoFunction fun, float a, float b)
{
//	fun.Call();
	float r;
	fun._pWorker->iCall<float>(r, fun._fun_index, a, b);
	return r;
}


int SAMPLE_callback()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("callback.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		NeoHelper::Register(pVM, "Mul", Mul);
		NeoHelper::Register(pVM, "Sample1", Sample1);
			

		DWORD t1 = GetTickCount();
		double r;
		pVM->Call<double>(&r, "Sum", 100, 200);
		DWORD t2 = GetTickCount();

		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("Sum %d + %d = %lf\n(Elapse:%d)\n", 100, 200, r, t2 - t1);

		CNeoVM::ReleaseVM(pVM);
	}
	delete[] pFileBuffer;

    return 0;
}

