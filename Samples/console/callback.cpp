#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API
#include "../../NeoSource/Neo.h"         // INeoLoader

#include <string>

using namespace NeoScript;

// v2 대안: 구 callback 은 NeoHelper::Fun 으로 C함수를 스크립트 "값(델리게이트)"으로 넘겼다.
// v2 는 함수-값 델리게이트를 두지 않는 대신, 호스트 로직을 "네이티브 객체 메서드"로 노출한다.
//   구 Mul(a,b)  → Host.mul(a,b)  (호스트가 스크립트에 제공하는 콜백/로직)
//   호스트→스크립트 호출은 rt->Call(inst, "Sum") 로.
static bool HostMethod(CallContext& ctx, StringView method)
{
	std::string m(method.data(), method.size());
	if (m == "mul") { ctx.retInt(ctx.argInt(0) * ctx.argInt(1)); return true; }
	return false;
}

int SAMPLE_callback(INeoLoader* pLoader, std::string filename)
{
	(void)pLoader; (void)filename; // 데모는 인라인 스크립트로 자족적

	RuntimeDesc rd;
	rd.printFn = [](StringView s) { printf("%.*s\n", (int)s.size(), s.data()); };
	IRuntime* rt = CreateRuntime(rd);

	NativeObjectDesc od;
	od.name = "Host";
	od.method = &HostMethod;   // 호스트 로직 노출(구 델리게이트 대체)
	rt->RegisterObject(od);
	rt->FreezeBindings();

	const char* src =
		"fun Sum1(var a, var b) { return a + b; }\n"     // 스크립트 로컬 함수
		"export fun Sum(var a, var b)\n"
		"{\n"
		"    var s = Sum1(a, b);\n"                       // 스크립트 내부 호출
		"    var m = Host.mul(a, b);\n"                   // 호스트 제공 콜백 호출
		"    print(s);\n"
		"    print(m);\n"
		"    return s + m;\n"
		"}\n";

	CompileDesc cd;
	cd.source = src;
	cd.sourceName = "callback.ns";
	cd.emitAsm = true; cd.includeDebugInfo = true;   // 원본 샘플 동작: ASM 덤프 + 디버그 정보
	CompileResult cr = rt->Compile(cd);
	if (!cr.program)
	{
		printf("Error - compile failed : %s\n", cr.error.message.c_str());
		DestroyRuntime(rt);
		return -1;
	}

	InstanceHandle inst = rt->CreateInstance(cr.program);

	DWORD t1 = GetTickCount();
	{
		Invocation call = rt->Call(inst, "Sum");         // 호스트 → 스크립트
		RunStatus st = call.argInt(100).argInt(200).invoke();
		DWORD t2 = GetTickCount();

		StringView err;
		if (rt->TakeLastError(err))
			printf("Error - VM Call : %.*s\n(Elapse:%d)\n", (int)err.size(), err.data(), (int)(t2 - t1));
		else if (st == RunStatus::Completed)
			printf("Sum %d + %d = %d\n(Elapse:%d)\n", 100, 200, call.retInt(), (int)(t2 - t1));
	}
	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	return 0;
}
