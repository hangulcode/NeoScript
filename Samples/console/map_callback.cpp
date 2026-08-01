#include "stdafx.h"
#include "../../NeoSource/NeoScript.h"   // v2 public API
#include "../../NeoSource/Neo.h"         // INeoLoader (파일 로더 타입)

#include <string>

using namespace NeoScript;

// 구 CA 클래스의 상태 — g_sData 객체가 노출하는 네이티브 데이터.
struct HostData
{
	float x = 0.1f, y = 1.0f, z = 10.0f;
};

// g_sData.method(...) 디스패처
static bool DataMethod(CallContext& ctx, StringView method)
{
	std::string m(method.data(), method.size());
	if (m == "sum") { ctx.retFloat(ctx.argFloat(0) + ctx.argFloat(1)); return true; }
	if (m == "mul") { ctx.retFloat(ctx.argFloat(0) * ctx.argFloat(1)); return true; }
	return false;
}
// g_sData.Transform get/set — map {x,y,z} 로 노출(스크립트가 pos.x/.y/.z 접근).
static bool DataProperty(CallContext& ctx, StringView name, bool isGet)
{
	HostData* d = static_cast<HostData*>(ctx.userData());
	if (std::string(name.data(), name.size()) != "Transform") return false;
	if (isGet)
	{
		MapBuilder m = ctx.retMap();
		m.setFloat("x", d->x);
		m.setFloat("y", d->y);
		m.setFloat("z", d->z);
	}
	else
	{
		MapReader r;
		if (ctx.argAsMap(0, r))
		{
			r.getFloat("x", d->x);
			r.getFloat("y", d->y);
			r.getFloat("z", d->z);
		}
	}
	return true;
}

int SAMPLE_map_callback(INeoLoader* pLoader, std::string filename)
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == pLoader->Load(filename.c_str(), pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	static HostData s_data; // 인스턴스별 userData(BindObject)로 넘겨도 되지만 단일 인스턴스라 전역

	RuntimeDesc rd;
	rd.printFn = [](StringView s) { printf("%.*s\n", (int)s.size(), s.data()); };
	rd.nativeLoader = pLoader;
	IRuntime* rt = CreateRuntime(rd);

	NativeObjectDesc od;
	od.name = "g_sData";
	od.method = &DataMethod;
	od.property = &DataProperty;
	od.userData = &s_data;
	od.declareGlobal = false;   // 스크립트가 `export var g_sData` 로 직접 선언
	rt->RegisterObject(od);
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

	DWORD t1 = GetTickCount();
	{
		// Invocation 은 인스턴스보다 오래 살면 안 됨 → DestroyInstance 전에 스코프로 소멸.
		Invocation call = rt->Call(inst, "update");
		RunStatus st = call.argInt(5).argInt(15).invoke();
		DWORD t2 = GetTickCount();

		StringView err;
		if (rt->TakeLastError(err))
			printf("Error - VM Call : %.*s\n(Elapse:%d)\n", (int)err.size(), err.data(), (int)(t2 - t1));
		else if (st == RunStatus::Completed)
			printf("(Elapse:%d)\n", (int)(t2 - t1));
	}

	rt->DestroyInstance(inst);
	rt->DestroyProgram(cr.program);
	DestroyRuntime(rt);
	pLoader->Unload(nullptr, pFileBuffer, iFileLen);
	return 0;
}
