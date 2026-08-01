#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API
#include "../../NeoSource/Neo.h"         // INeoLoader

using namespace NeoScript;

int SAMPLE_etc(INeoLoader* pLoader, std::string filename, const char* pFunctionName)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == pLoader->Load(filename.c_str(), pFileBuffer, iFileLen))
	{
		printf("file read error\n");
		return -1;
	}

	RuntimeDesc rd;
	// print 핸들러를 걸지 않는다(원본 동작 유지): 엔진 내장 io_print 의 네이티브 경로가
	// 1인자 print 는 개행 추가, 2인자 print(x, "") 는 개행 없이 이어붙인다. printFn 을 걸면
	// io_print 가 인자수와 무관하게 문자열만 넘겨 이 구분이 사라진다(contailer.ns 가 2인자 사용).
	rd.nativeLoader = pLoader; // import 해석용(list/map 등이 dump 모듈 import)
	IRuntime* rt = CreateRuntime(rd);

	// GameObject 전역 심볼 선언(디스패처 없음 — 구 neo_globalinterface 는 GetVar 만 했음).
	NativeObjectDesc gameObject;
	gameObject.name = "GameObject";
	rt->RegisterObject(gameObject);
	rt->FreezeBindings();

	CompileDefine defArr[1];
	defArr[0].name = "KEY_LEFT"; defArr[0].kind = DefineKind::Int; defArr[0].text = "37";
	DefineSetHandle defineSet = rt->CreateDefineSet(defArr);   // 1회 빌드(같은 defines 재컴파일 시 재구성 0)

	CompileDesc cd;
	cd.source = StringView((const char*)pFileBuffer, (size_t)iFileLen);
	cd.sourceName = filename.c_str();
	cd.defineSet = defineSet;
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
	InstanceHandle inst = rt->CreateInstance(cr.program); // 최상위(전역 초기화) 실행

	DWORD dwCallTime = 0;
	if (pFunctionName != NULL)
	{
		DWORD t1 = GetTickCount();
		if (rt->Call(inst, StringView(pFunctionName)).invoke() != RunStatus::Completed)
			result = -1;
		dwCallTime = GetTickCount() - t1;
	}

	StringView err;
	if (rt->TakeLastError(err))
	{
		printf("Error - VM Call : %.*s\n(Elapse:%d)\n", (int)err.size(), err.data(), (int)dwCallTime);
		result = -1;
	}
	else
		printf("(Elapse:%d)\n", (int)dwCallTime);

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	rt->DestroyDefineSet(defineSet);
	DestroyRuntime(rt);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);
	return result;
}
