#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API
#include "../../NeoSource/Neo.h"         // INeoLoader

using namespace NeoScript;

int SAMPLE_slice_run(INeoLoader* pLoader, std::string filename)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == pLoader->Load(filename.c_str(), pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	RuntimeDesc rd;
	rd.printFn = [](StringView s) { printf("%.*s\n", (int)s.size(), s.data()); };
	rd.nativeLoader = pLoader;
	IRuntime* rt = CreateRuntime(rd);
	rt->FreezeBindings();

	CompileDesc cd;
	cd.source = StringView((const char*)pFileBuffer, (size_t)iFileLen);
	cd.sourceName = filename.c_str();
	cd.emitAsm = true; cd.includeDebugInfo = true;   // 원본 샘플 동작: ASM 덤프 + 디버그 정보
	CompileResult cr = rt->Compile(cd);
	if (!cr.program)
	{
		printf("Error - compile failed : %s\n", cr.error.message.c_str());
		DestroyRuntime(rt);
		pLoader->Unload(nullptr, pFileBuffer, iFileLen);
		return -1;
	}

	int result = 0;
	InstanceHandle inst = rt->CreateInstance(cr.program);

	// 협조적 슬라이스 실행: slice_fun 을 200ms/1000op 슬라이스로 나눠 실행.
	if (false == rt->StartSliced(inst, "slice_fun", 200, 1000))
	{
		printf("Error - StartSliced %s\n", "slice_fun");
		result = -1;
	}
	else
	{
		DWORD dwPre = GetTickCount();
		int i = 0;
		StringView err;
		while (rt->IsRunning(inst))
		{
			DWORD t1 = GetTickCount();
			rt->UpdateSliced(inst);
			DWORD t2 = GetTickCount();
			if (rt->TakeLastError(err))
			{
				printf("Error - VM Call : %.*s\n(Elapse:%d)\n", (int)err.size(), err.data(), (int)(t2 - t1));
			}
			else
			{
				DWORD dwNext = GetTickCount();
				if (dwNext - dwPre > 500)
				{
					printf("Slide Run %d\n(Elapse:%d)\n", i++, (int)(t2 - t1));
					dwPre = dwNext;
				}
			}
			Sleep(10);
		}
	}

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);
	return result;
}
