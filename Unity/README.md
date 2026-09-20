# Unity

유니티에서 NeoScript 를 쓰기 위한 것들. 자세한 설명은 **[../docs/Unity.md](../docs/Unity.md)** 에 있다.

| 경로 | 내용 |
| :--- | :--- |
| `Native/` | C ABI (`NeoScriptC.h` / `.cpp`). C++ 파사드를 평면 C 로 편 것 |
| `com.neoscript.unity/` | UPM 패키지 — C# 바인딩, 에디터 지원, 스모크 테스트, 샘플 |
| `build_windows.ps1` | 에디터 + Win64 플레이어용 `NeoScript.dll` |
| `build_android.ps1` | `libNeoScript.so` (arm64-v8a 기본) |
| `build_ios.sh` | `libNeoScriptUnity.a` — **macOS + Xcode 에서만 돈다** |

## 빠른 시작

```powershell
pwsh Unity\build_windows.ps1
```

유니티 프로젝트의 `Packages/manifest.json` 에 추가한다(경로는 `Packages` 폴더 기준):

```json
"com.neoscript.unity": "file:../../path/to/NeoScript/Unity/com.neoscript.unity"
```

## 검증

셋 다 돌린다. 서로 못 보는 걸 본다 — 스모크는 바인딩 표면, PlayMode 는 프레임 경계를 넘는
수명, 리로드 왕복은 도메인 경계의 정리.

```powershell
Unity.exe -batchmode -nographics -projectPath <프로젝트> `
          -executeMethod NeoScript.Tests.NeoSmokeTest.RunAll -logFile -

Unity.exe -batchmode -projectPath <프로젝트> `
          -runTests -testPlatform PlayMode -testResults results.xml

Unity.exe -batchmode -nographics -projectPath <프로젝트> `
          -executeMethod NeoScript.Tests.NeoReloadCycleTest.RunAll -logFile -
```

에디터에서는 **Tools > NeoScript > Run smoke test**.

네이티브를 다시 빌드하려면 **에디터를 닫아야 한다.** 플러그인을 런타임에 놓아 주는 우회는
시도했다가 걷어냈다 — 자세한 이유는 `docs/Unity.md` 의 "에디터에서 네이티브를 다시 빌드할 때".

## 알아 둘 것

플러그인 바이너리는 저장소에 없다. 빌드 산출물이라 `.gitignore` 에 걸려 있으니
프로젝트를 열기 전에 위 빌드 스크립트를 먼저 돌린다. 임포트 설정은 `.meta` 대신
`NeoPluginImportSettings.cs` 가 코드로 잡아 준다 — 유니티가 대상 없는 `.meta` 를
지우기 때문에 설정을 미리 커밋해 둘 수 없어서다.
