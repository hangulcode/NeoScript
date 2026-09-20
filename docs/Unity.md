# NeoScript × Unity

유니티에서 NeoScript VM 을 쓰는 방법. 언어 자체는 [../ReadMe.md](../ReadMe.md), 스크립트가
부를 수 있는 함수는 [API.md](API.md), C++ 호스트 API 는 [Embedding.md](Embedding.md) 를 본다.

## 구조

C# 은 C++ 이름장식·가상함수·`std::` 타입을 넘길 수 없다. 그래서 세 겹이다.

| 계층 | 위치 | 하는 일 |
| :--- | :--- | :--- |
| C++ 파사드 | `NeoSource/NeoScript.h` | VM 의 공개 API. 엔진(C++ 호스트)이 직접 쓴다 |
| C ABI | `Unity/Native/NeoScriptC.h` `.cpp` | 위를 평면 C 로 편 것. P/Invoke 가 부를 수 있는 유일한 모양 |
| C# 바인딩 | `Unity/com.neoscript.unity/Runtime/` | 위를 감싼 UPM 패키지 |

C ABI 가 `NeoSource/` 밖에 있는 이유는 하나다 — 엔진 저장소가 `NeoSource\*.cpp` 를 통째로
컴파일하는데, 엔진은 C++ 파사드를 직접 쓰므로 이 셰임이 필요 없다. 거기 두면 엔진 빌드에
쓰지도 않는 파일이 딸려 들어간다.

## 설치

### 1. 네이티브 플러그인 빌드

```powershell
pwsh Unity\build_windows.ps1                 # 에디터 + Win64 플레이어
pwsh Unity\build_android.ps1                 # arm64-v8a
```

```bash
./Unity/build_ios.sh                         # macOS + Xcode 에서만
```

빌드 결과는 `Unity/com.neoscript.unity/Runtime/Plugins/` 아래로 바로 복사된다.
**바이너리는 저장소에 없다** — 빌드 산출물이라 `.gitignore` 에 걸려 있다. 새로 받은 트리는
유니티로 열기 전에 위 스크립트를 먼저 돌려야 한다.

임포트 설정도 `.meta` 대신 `NeoPluginImportSettings.cs` 가 코드로 잡는다. 유니티가 대상 없는
`.meta` 를 지우기 때문에, 바이너리를 안 넣는 이상 설정만 미리 커밋해 둘 수가 없다.

### 2. 유니티 프로젝트에 패키지 물리기

프로젝트의 `Packages/manifest.json` 에 로컬 경로로 추가한다. 경로는 `Packages` 폴더 기준 상대경로다.

```json
{
  "dependencies": {
    "com.neoscript.unity": "file:../../path/to/NeoScript/Unity/com.neoscript.unity",
    ...
  }
}
```

저장소에서 고친 내용이 에디터에 바로 반영된다. 복사본을 두지 않는 게 요점이다.

## 최소 사용법

```csharp
using NeoScript;
using UnityEngine;

// print/error 는 프로세스 전역 훅이다. 앱 시작 시 한 번만 건다.
NeoRuntime.SetLogSink(Debug.Log, Debug.LogError);

var runtime = new NeoRuntime();
runtime.RegisterObject("Unity", new UnityBridge());   // 등록은 Freeze 전에
runtime.Freeze();

NeoProgram program = runtime.Compile(sourceText, "enemy.ns");
NeoInstance instance = program.CreateInstance();

int hp = instance.Call("GetHp").InvokeInt();
```

정리 순서가 있다. 네이티브 자원이라 GC 가 치워 주지 않는다.

```csharp
function?.Dispose();
instance?.Dispose();
program?.Dispose();
runtime?.Dispose();
```

## C# 객체를 스크립트에 노출하기

`INeoObject` 하나로 메서드 전부를 받는다. 프로퍼티가 필요하면 `INeoProperties` 를 같이 구현한다.

```csharp
sealed class UnityBridge : INeoObject, INeoProperties
{
    public bool Invoke(NeoCallContext ctx, NeoName method)
    {
        if (method.Is("Log"))         { Debug.Log(ctx.ArgString(0)); return true; }
        if (method.Is("SetPosition")) { target.position = ctx.ArgVector3(0); return true; }

        ctx.Fail(1, "unknown method: " + method.ToString());
        return false;
    }

    public bool GetProperty(NeoCallContext ctx, NeoName name)
    {
        if (name.Is("time")) { ctx.Return(Time.time); return true; }
        return false;
    }

    public bool SetProperty(NeoCallContext ctx, NeoName name) => false;
}
```

스크립트에서는 이렇게 보인다.

```
Unity.Log("hello");
Unity.SetPosition(math.Vector3(1.0, 2.0, 3.0));
var t = Unity.time;
```

`NeoName` 은 UTF-8 뷰지 문자열이 아니다. 디스패처는 호출마다 불리므로 `method.ToString()` 을
비교에 쓰면 프레임마다 할당이 쌓인다. 비교는 `Is()` 로 한다 — 할당이 없다.

`Vector2/3/4`·`Quaternion` 은 VM 의 값 타입이라 유니티 타입과 그대로 오간다. 변환 비용이 없다.

## 반드시 알아야 하는 제약

**`export` 가 붙은 것만 호스트에 보인다.** 함수는 `export fun`, 전역 변수는 `export var` 여야
`Call` / `TryGetInt` 등으로 닿는다. 그냥 `var` 로 선언한 전역은 못 찾는다.

**호출은 반드시 끝내야 한다.** `instance.Call(...)` 은 인스턴스의 실행 컨텍스트를 빌린다.
`Invoke*` 종결자가 자동으로 반납하지만, 인자만 쌓고 버릴 때는 `Abandon()` 을 부른다.
한 인스턴스에 동시에 살아 있는 호출은 하나뿐이다(네이티브→스크립트 중첩 호출은 예외).

**리더·빌더는 발급된 스코프 안에서만 유효하다.** `NeoMapReader` / `NeoListReader` /
`NeoArrayView` 는 VM 저장소 위의 뷰라 복사가 없다. 대신 디스패처 호출이나
`InvokeReadMap` 콜백 밖으로 들고 나가면 죽은 메모리를 가리킨다. 필요한 값은 안에서 복사한다.

```csharp
int gold = 0;
instance.Call("GetInventory").InvokeReadMap(map => map.TryGet("gold", out gold));
```

**등록 가능한 네이티브 객체 타입은 64개다.** VM 의 디스패처가 컨텍스트 없는 함수 포인터라
타입마다 고정 썽크가 필요해서 상한이 있다. 필요하면 `NeoScriptC.h` 의
`NS_MAX_OBJECT_SLOTS` 를 올리고 네이티브를 다시 빌드한다.

**VM 인스턴스는 스레드 안전하지 않다.** 한 인스턴스는 한 스레드에서만 쓴다. 유니티에서는
메인 스레드를 뜻한다.

**콜백에서 예외를 흘리면 안 된다.** 바인딩이 `INeoObject` 구현 전체를 try/catch 로 감싸서
스크립트 런타임 에러로 바꿔 주지만, 삼키는 것보다 `ctx.Fail(code, reason)` 으로 사유를 남기고
`false` 를 반환하는 편이 훨씬 낫다. 그래야 스크립트 IP·라인·스택트레이스가 같이 붙는다.

## 프레임을 지키는 법

무한루프나 폭주하는 스크립트에 프레임을 통째로 내주지 않는 손잡이가 둘 있다.

```csharp
// 1. 호출마다 상한
instance.Call(update).Arg(Time.deltaTime).Timeout(8).Invoke();

// 2. 긴 작업을 슬라이스로 쪼개기
instance.StartSliced("BuildWorld", timeoutMs: 4);
while (instance.UpdateSliced() == NeoRunStatus.Suspended)
    yield return null;
```

## 바이트코드 미리 굽기

`Compile` 은 파서를 태운다. 빌드 타임에 한 번만 굽고 플레이어에는 이미지만 싣는 게 정석이다.
로딩이 빨라지고, 소스가 그대로 패키징되지 않는다.

```csharp
// 빌드 타임 (에디터)
byte[] image = runtime.CompileToBytecode(source, "enemy.ns");
File.WriteAllBytes(outputPath, image);

// 런타임
NeoProgram program = runtime.LoadProgram(bytes);
```

## import 해석

`import` 는 컴파일 타임 인클루드다. 유니티에는 파일 시스템 경로가 없으니 로더를 직접 준다.

```csharp
var runtime = new NeoRuntime(new NeoRuntimeOptions
{
    Loader = path =>
    {
        var asset = Resources.Load<TextAsset>("NeoScripts/" + Path.GetFileNameWithoutExtension(path));
        return asset != null ? asset.bytes : null;   // 못 찾으면 null
    },
});
```

`math` 같은 내장 모듈도 로더를 먼저 거친다. 못 찾으면(`null`) 엔진이 내장 구현으로 떨어지니
그대로 두면 된다.

## 플랫폼

| 플랫폼 | 산출물 | 해소 방식 |
| :--- | :--- | :--- |
| Windows (에디터/플레이어 x64) | `NeoScript.dll` | `DllImport("NeoScript")` |
| Android arm64-v8a | `libNeoScript.so` | 같음 |
| iOS arm64 | `libNeoScriptUnity.a` | `DllImport("__Internal")` — 동적 로딩이 막혀 정적 링크 |

Android `.so` 는 릴리스에서 심볼을 벗긴다. 안 벗기면 19MB 가 APK 에 그대로 들어간다.

async(`system.async` / http)를 쓰면 VM 이 백그라운드 스레드를 띄운다. 쓰지 않으면 스레드는
생기지 않는다.

## 에디터에서 네이티브를 다시 빌드할 때

**에디터를 닫아야 한다.** 유니티는 DllImport 로 한 번 로드한 네이티브 플러그인을 놓지 않으므로,
`NeoScript.dll` 을 덮어쓰려면 에디터가 떠 있으면 안 된다.

흔히 쓰는 우회가 `FreeLibrary` 로 참조를 떨구는 것인데 **여기서는 쓰지 않는다. 실측으로 두 번
막혔다.**

- 도메인 리로드 없이 해제하면 Mono 가 캐싱해 둔 P/Invoke 함수 포인터가 해제된 주소를 가리킨
  채로 남는다. 다음 호출이 그 주소로 점프해서 에디터가 통째로 죽는다.
- 도메인 리로드 안에서 해제해도 살아 돌아오지 못한다. 유니티는 플러그인 이름을 이미 로드한
  모듈 핸들로 해소하는데, 그 핸들이 죽은 뒤에는 다시 로드하지 않는다 — 이후 모든 호출이
  `EntryPointNotFoundException` 이 된다.

에디터를 닫지 않고 네이티브를 반복 수정하려면 DllImport 대신 호스트가 직접
`LoadLibrary`/`GetProcAddress` 로 함수 포인터 테이블을 잡는 구조로 가야 한다. 바인딩 계층의
설계 변경이라 우회로 될 일이 아니다.

## 도메인 리로드

패키지가 리로드 경계에서 정리를 한다(`NeoDomainReloadGuard`). 관리 쪽 `NeoRuntime` 객체는
도메인과 함께 사라지지만 네이티브 런타임은 남으므로, 리로드 직전에 전부 파괴하고 네이티브가
들고 있는 로그 콜백도 떼어 낸다 — 그 콜백은 관리 델리게이트의 썽크 주소라 리로드와 함께 죽는데
모듈은 그걸 계속 들고 있다.

그래서 **프레임마다 스크립트를 부르는 컴포넌트는 리로드를 넘길 준비가 돼 있어야 한다.**
"Recompile And Continue Playing" 이면 플레이 도중에도 리로드가 일어나고, 그때 컴포넌트는
살아남지만 들고 있던 인스턴스는 죽고 비직렬화 필드는 null 로 리셋된다. `Awake` 는 다시 불리지
않는다. `instance.IsAlive` 를 확인하고 죽었으면 다시 세우는 게 맞다 —
`Samples~/Basic/NeoScriptDemo.cs` 가 그 모양이다.

## 검증

C ABI 자체는 순수 C 로 쓴 스모크가 덮는다. C 로 컴파일하는 것 자체가 검증의 절반이다 —
헤더에 C++ 이 섞여 들면 P/Invoke 호스트가 아니라 여기서 먼저 터진다.

```powershell
cmake -S . -B build\unity -A x64
cmake --build build\unity --config Release --target neoscript_c_abi_smoke
build\unity\Release\neoscript_c_abi_smoke.exe
```

유니티 쪽은 둘이다. 에디터 모드 스모크가 바인딩 표면 전체를 훑는다.

```powershell
Unity.exe -batchmode -nographics -projectPath <프로젝트> `
          -executeMethod NeoScript.Tests.NeoSmokeTest.RunAll -logFile -
```

실패 개수를 종료 코드로 돌려주므로 CI 에 그대로 걸 수 있다. 에디터에서는
**Tools > NeoScript > Run smoke test**.

PlayMode 테스트는 그것만으로 안 잡히는 걸 본다. 에디터 모드 스모크는 한 프레임 안에서
끝나므로 **프레임 경계를 넘는 수명 문제를 볼 수 없다**. 플레이 진입 시 도메인 리로드가
살아있는 컴포넌트 밑에서 네이티브 런타임을 파괴해 `ObjectDisposedException` 이 나던 버그가
여기서만 잡혔다.

```powershell
Unity.exe -batchmode -projectPath <프로젝트> `
          -runTests -testPlatform PlayMode -testResults results.xml
```

패키지 안의 테스트라 프로젝트 `Packages/manifest.json` 에 아래가 있어야 인식된다.

```json
"testables": [ "com.neoscript.unity" ]
```

세 번째는 도메인 리로드 경계다. 위 둘은 각각 **한 도메인 안에서만** 돌기 때문에 리로드를
건너는 정리가 어긋난 것을 볼 수 없다. 리로드 전후로 플러그인을 실제로 써 본다.

```powershell
Unity.exe -batchmode -nographics -projectPath <프로젝트> `
          -executeMethod NeoScript.Tests.NeoReloadCycleTest.RunAll -logFile -
```

## ABI 가 어긋났을 때

`NeoScriptC.h` 와 `NeoNative.cs` 는 서로의 거울이다. 어긋나면 조용히 메모리가 깨지는 대신
기동 시 바로 터지게 해 두었다 — 구조체 크기를 대조하고(`NeoNativeLayout.Verify`),
ABI 버전을 확인한다. C ABI 를 고치면 `NS_ABI_VERSION` 을 올리고 C# 쪽 `NeoNative.AbiVersion`
도 같이 올린다. C++ enum 과 C 상수는 `NeoScriptC.cpp` 의 `static_assert` 가 잡는다.
