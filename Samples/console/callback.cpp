#include "stdafx.h"
#include "../../NeoSource/Neo.h"

using namespace NeoScript;

NS_FLOAT Mul(NS_FLOAT a, NS_FLOAT b)
{
	return a * b;
}


NS_FLOAT Sample1(NeoFunction fun, NS_FLOAT a, NS_FLOAT b)
{
	NS_FLOAT r;
	if(NeoHelper::Call<NS_FLOAT>(&r, fun, a, b))
		return r;
	return 0;
}


int SAMPLE_callback()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("callback.ns", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	NeoCompilerParam param(pFileBuffer, iFileLen);
	param.err = &err;
	param.putASM = true;
	param.debug = true;

	INeoVM* pVM = INeoVM::CompileAndLoadVM(param);
	if (pVM != NULL)
	{
//		pVM->CallN("Set", NeoHelper::Fun(Mul), NeoHelper::Fun(Sample1));

		int iFid = pVM->FindFunction("Set");
		pVM->GetMainWorker()->iCallN(iFid, NeoHelper::Fun(Mul), NeoHelper::Fun(Sample1));
			

		DWORD t1 = GetTickCount();
		NS_FLOAT r;
		pVM->Call<NS_FLOAT>(&r, "Sum", 100, 200);
		DWORD t2 = GetTickCount();

		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("Sum %d + %d = %lf\n(Elapse:%d)\n", 100, 200, r, t2 - t1);

		INeoVM::ReleaseVM(pVM);
	}
	FileUnoad(nullptr, pFileBuffer, iFileLen);

    return 0;
}

