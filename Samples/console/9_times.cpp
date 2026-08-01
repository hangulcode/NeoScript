#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API
#include "../../NeoSource/Neo.h"         // INeoLoader (파일 로더 타입)

using namespace NeoScript;

int SAMPLE_9_times(INeoLoader* pLoader, std::string filename)
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
	StringView err;
	if (rt->TakeLastError(err))
		printf("Error - init : %.*s\n", (int)err.size(), err.data());

	for (int i = 1; i < 10; i++)
	{
		DWORD t1 = GetTickCount();
		rt->Call(inst, "Time9").argInt(i).invoke();
		DWORD t2 = GetTickCount();
		if (rt->TakeLastError(err))
			printf("Error - VM Call : %.*s\n(Elapse:%d)\n", (int)err.size(), err.data(), (int)(t2 - t1));
		else
			printf("(Elapse:%d)\n", (int)(t2 - t1));
	}

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);
	return 0;
}
