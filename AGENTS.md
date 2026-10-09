# NeoScript — 작업 규칙

> 이 파일이 유일한 진입점이다. CLAUDE.md / GEMINI.md 는 여기를 가리킨다. 80줄을 넘기지 않는다. 스크립트 작성 전 [ScriptAuthoring.md](docs/ScriptAuthoring.md)를 먼저 읽고, 세부는 `ReadMe.md`와 `docs/`를 확인한다.

## 함께 쓰는 트리

이 저장소는 **공개**다. 사내 저장소 이름·경로·게임 프로젝트 이름을 여기에 적지 않는다.

| 트리 | 성격 |
| :--- | :--- |
| `D:\Git\NeoScript` | 스크립트 VM · 컴파일러 (**git**) — 여기 |
| 엔진 저장소 | 이 VM 을 쓰는 호스트. `NeoSource\*.cpp` 를 직접 컴파일한다 (**별도 git**) |
| 콘텐츠 저장소 | 게임 리소스와 스크립트 (**SVN**) |

## 절대 규칙

1. 커밋 메시지는 `[도구이름] 영문 제목`. **커밋한 주체를 앞에 밝힌다.**
   - Claude 가 커밋하면 `[Claude]`
   - Codex 가 커밋하면 `[Codex]`
   - Gemini 가 커밋하면 `[Gemini]`

   **성능 수치는 넣지 않는다.** 수정 항목과 이유만 쓴다.
2. **푸쉬는 하지 않는다.** 커밋까지만.
3. 한글이 든 소스는 **UTF-8 BOM**. 없으면 MSVC 가 CP949 로 읽어 주석이 다음 줄을 삼킨다.
4. 리소스 이름은 소문자로 쓴다.
5. 워킹트리를 되돌릴 때 `git checkout --` 로 남의 수정을 날리지 않는다. 계측용 수정은 백업 후 진행한다.
6. **`switch`의 `case`는 반드시 값의 오름차순으로 배치한다.** enum은 실제 열거값 순서를 따르며, `default`는 마지막에 둔다. 재배치할 때 fallthrough 동작을 보존한다.

## 빌드 · 회귀

```
빌드 : Samples\console\console.sln   Configuration=Release Platform=x64
회귀 : console.exe --smoke                      (compiler regression 2785)
       console.exe --run cycle                  (cycle_ref 368)
       console.exe --v2smoke                    (0 failures)
       console.exe --compiler-error-regression   (20/20)
```

**회귀는 `Samples\console` 에서 돌린다.** 테스트가 `../../TestScript/` 를 상대 경로로 열기 때문에,
exe 가 있는 폴더에서 돌리면 `file read error` 로 엉뚱하게 실패한다.

`--debug-smoke` 는 이전부터 실패한다(알려진 상태).

**비 Windows 확인은 반드시 한다.** 과거 `_countof` 를 무방비로 써서 Linux/Android 빌드를 깬 적이 있다.
```
NDK clang : %LOCALAPPDATA%\Android\Sdk\ndk\25.1.8937393\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe
            --target=aarch64-linux-android24 -std=c++17 -fsyntax-only -INeoSource <각 .cpp>
```

## Unity

C ABI in `Unity\Native`, C# binding in `Unity\com.neoscript.unity` (UPM). **Keep the C ABI out of
`NeoSource\`** — the engine repository compiles `NeoSource\*.cpp` wholesale and would drag in a
shim it never calls. Plugin binaries are not committed: run `pwsh Unity\build_windows.ps1` before
opening Unity. Regression is `neoscript_c_abi_smoke.exe` (compiling it as C is itself the test)
plus three Unity suites — smoke, PlayMode, reload cycle. Commands in `docs/Unity.md`.

## 성능 측정 규칙

- 벤치는 `Samples\bench` 에서 Neo / Lua / C++ 를 **한 세션 안에서 교차 실행**하고 체크섬 8/8 을 확인한다.
  Lua 는 `D:\util\lua-5.5_Win64_bin\lua55.exe`.
- **빌드마다 ±5% 흔들린다. 코드 배치 때문이지 노이즈가 아니다.** 그래서 세션이 다른 수치끼리 비교하지 않는다.
- 변경의 효과를 보려면 **두 exe 를 같은 루프에서 번갈아** 돌린다. 자세한 근거는 `ReadMe.md` 의 Methodology 5항.
- 측정하지 않은 수치를 말하지 않는다.

## 먼저 알아야 할 함정

- **함수 이름·인자 정보는 모듈별로 먼저 수집한다.** 뒤쪽 함수 정의를 호출할 수 있으며 `fun F(...);` 전방 선언은 오류다.
- **변수·const는 반드시 사용 전에 선언한다.** 함수 본문에서도 뒤에 선언된 전역변수는 읽을 수 없다.
- `import`는 컴파일 시 포함하며, 같은 컴파일의 중복 import는 재사용한다. 실행 인스턴스끼리는 전역 상태를 공유하지 않는다.
- `math.Vector2/3/4`, `Quaternion` 은 값 타입이고 `NOP_VEC_MAKE` 인트린식이라 호출이 아니다.
- 모듈 별칭은 함수와 `export const`만 노출하며 컴파일 타임에 해소한다. 변수 접근은 지원하지 않으므로 필요한 값은 함수로 전달한다.

## 문서

- `ReadMe.md` — 언어 문법, 호스트 API, 성능 결과와 측정 방법
- `docs/API.md` — 스크립트가 부를 수 있는 모든 함수(math/system/coroutine, string/list/map/async 메서드, 키워드 intrinsic)
  — 언어·라이브러리를 고치면 구현·회귀 시험과 함께 API 및 ScriptAuthoring 문서도 갱신한다
- `docs/Unity.md` — Unity integration (the three layers, setup, exposing C# objects, platforms, traps)
- [docs/ScriptAuthoring.md](docs/ScriptAuthoring.md) — AI와 사람이 먼저 읽는 스크립트 사용법·주요 기능·실행 예제. 이 문서만 줄 수 제한 예외
