#include "stdafx.h"
#include "../../NeoSource/NeoVM.h"




int SAMPLE_time_limit()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("time_limit.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		pVM->SetTimeout(-1, 100, 1000); // -1 is main worker
		printf("Compile Success. Code : %d bytes !!\n", pVM->GetBytesSize());

		for (int i = 1; i < 10; i++)
		{
			DWORD t1 = GetTickCount();
			double r = 0;
			bool bCompleted = pVM->Call_TL<double>(&r, "TimeTest");
			DWORD t2 = GetTickCount();
			if (false == bCompleted)
			{
				printf("Job Not Completed\n");
			}
			if (pVM->IsLastErrorMsg())
			{
				printf("Error - VM Call : %s\n(Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
				pVM->ClearLastErrorMsg();
			}
			else
				printf("Sum 1 ~ = %lf\n(Elapse:%d)\n", r, t2 - t1);
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

