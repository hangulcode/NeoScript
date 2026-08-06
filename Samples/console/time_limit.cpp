#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API
#include "../../NeoSource/Neo.h"         // INeoLoader
#include <thread>
#include <chrono>

using namespace NeoScript;

int SAMPLE_time_limit(INeoLoader* pLoader, std::string filename)
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

	InstanceHandle inst = rt->CreateInstance(cr.program);

	// TimeTest 를 100ms/1000op 슬라이스로 나눠 실행(구 Setup_TL/Call_TL).
	StringView err;
	if (false == rt->StartSliced(inst, "TimeTest", 100, 1000))
	{
		printf("Error - StartSliced TimeTest\n");
	}
	else
	{
		for (int i = 1; i < 20; i++)
		{
			DWORD t1 = GetTickCount();
			RunStatus st = rt->UpdateSliced(inst);
			DWORD t2 = GetTickCount();
			if (rt->TakeLastError(err))
			{
				printf("Error - VM Call : %.*s\n(Elapse:%d)\n", (int)err.size(), err.data(), (int)(t2 - t1));
				break;
			}
			if (st != RunStatus::Completed)
			{
				printf("Job Not Completed (Elapse:%d)\n", (int)(t2 - t1));
				// 호스트의 한 프레임에 해당한다. 이 간격이 없으면 폴링 19번이 0ms 안에 끝나서
				// 스크립트의 sleep(1) x 5 = 5ms 가 흐를 시간 자체가 없다(항상 미완료).
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				printf("(Elapse:%d)\n", (int)(t2 - t1));
				break;
			}
		}
	}

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);
	return 0;
}
