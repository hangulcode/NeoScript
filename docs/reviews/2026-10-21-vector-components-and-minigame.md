# 변경 이유와 검토 요청: 벡터 성분 복합 대입 및 미니 호스트 예제

- 작성일: 2026-10-21. 아래 실행 검증은 2026-10-20 KST에 수행했다.
- 검토 기준: `dev` 브랜치, HEAD `793270a0fd51ed202c99934e3637c0bed716686d` 이후의 로컬 변경.
- 현재 상태: 코드·문서는 미커밋·미푸시. GitHub 저장소 소개와 Topics만 웹에서 별도로 저장했다.
- 목적: 수정의 배경과 설계 판단을 전달한다. 아래 설명과 별개로 구현과 테스트의 타당성을 검토해 달라.

## 1. 왜 이 작업을 시작했는가

사용자는 저장소의 유입이 적은 이유를 확인하고, 소개·첫 실행 경로를 개선하며,
실제 게임 엔진에서의 사용 흐름을 참고한 간단한 예제 프로젝트를 추가해 달라고 요청했다.

기존 README는 언어와 내부 기술 설명 중심이었다. 첫 방문자가 대상 사용자와 용도를
빠르게 이해하기 어려웠고, 완결된 C++ 임베딩 예제를 빌드·실행하는 경로도 필요했다.
C# 이식을 미래 계획처럼 표현한 문장은 현재 Unity/C# 바인딩 설명과 함께 읽으면 혼동을 줄 수 있었다.
문서 개선이 유입을 늘린다는 효과는 아직 측정하지 않았다.

예제 작성 중 정상적으로 보여야 할 `position[0] += speed * dt`가 실패했다.
예제 코드를 다른 표현으로 바꾸어 피하지 않고 최소 재현으로 VM 결함을 확인했다.
결함 보고 후 사용자가 수정을 지시하여 VM 수정과 예제 완성을 함께 진행했다.

## 2. VM 결함의 재현과 원인

수정 전 Windows MSVC Release와 Linux GCC Release에서 다음 결과를 확인했다.

| 최소 시험 | 결과 |
| --- | --- |
| 네이티브 Position을 지역 변수로 받아 `var x = position[0]` | 성공 |
| 같은 값에 `position[0] = position[0] + dt` | 성공 |
| 같은 값에 `position[0] += dt` | 실패 |
| `math.Vector3`로 지역 벡터를 직접 만들고 `position[0] += dt` | 실패 |

오류는 `cannot read by index from vector`였다. 네이티브 프로퍼티 반환이나 전역 변수 보관이
필수 조건이 아니며, 벡터 성분의 복합 대입 경로 자체가 문제였다.

기존 처리 흐름:

1. 컴파일러의 `GetTableOpTypeFromOp()`가 인덱스 대상 `+=`를 `NOP_TABLE_ADD2`로 변환한다.
2. `handle_TABLE_ADD2()`는 primitive array만 별도로 처리하고 나머지는 `GetTableItemValid()`로 보낸다.
3. `GetTableItem()`은 map/list의 값 슬롯을 반환하며, float 성분을 가진 vector는 지원하지 않는다.
4. 따라서 정상적인 벡터 인덱스라도 범용 컨테이너 조회 오류로 실패한다.

`-=`, `*=`, `/=`, `%=` 핸들러도 같은 구조였다. 초기 최소 재현에서는 `+=`를 분리 확인했고,
수정 후 전용 회귀에서는 나머지 연산의 성공·실패 계약도 확인했다.

## 3. 왜 이 방식으로 고쳤는가

[NeoVMWorker_Handlers.inl](../../NeoSource/NeoVMWorker_Handlers.inl)의 다섯 핸들러에
`VAR_VEC` 분기를 추가하고, [NeoVMWorker.cpp](../../NeoSource/NeoVMWorker.cpp)의
`VectorModify()`로 보냈다. 선언은 [NeoVMWorker.h](../../NeoSource/NeoVMWorker.h)에 추가했다.

- 인덱스는 int이면서 성분 범위 안이어야 한다. unsigned 범위 비교로 음수도 거부한다.
- 대상 성분을 float 임시값으로 읽고 기존 `Add2` / `Sub2` / `Mul2` / `Div2` / `Per2`를 재사용한다.
  별도의 산술 규칙을 만들지 않고 기존 스칼라 연산의 타입 판단과 오류 메시지를 유지하려는 선택이다.
- 계산 후 VM 오류가 있으면 원본에 기록하지 않는다. 잘못된 피연산자 때문에 실패했을 때
  벡터 값이나 그 공유 사본이 바뀌지 않게 한다.
- 성공한 뒤에만 `VecCopyOnWrite()`를 호출해 해당 성분을 기록한다.
  `var b = a; b[0] += 1;`에서 `a`의 값 의미론을 유지하기 위해 필요하다.
- 성분은 float이므로 `+=`, `-=`, `*=`, `/=`에 int/float 피연산자를 허용한다.
  `%=`는 기존 float `Per2` 정책대로 오류를 낸다. 나머지 연산 지원을 새로 확장한 것은 아니다.

map/list/primitive array의 기존 경로, 공개 호스트 API 시그니처, opcode 번호와 바이트코드 형식은
바꾸지 않았다. 전체 벡터에 대한 `v += other` 같은 연산 지원도 이번 변경 범위가 아니다.
성능 벤치마크는 수행하지 않았으므로 성능 개선·무영향을 주장하지 않는다.

## 4. 회귀 테스트를 추가한 이유와 범위

[Tests/vector_component_regression.cpp](../../Tests/vector_component_regression.cpp)를 추가하고
루트 CMake/CTest에 등록했다. 기존 회귀가 모두 통과해도 이번 오류를 잡지 못했기 때문에
성분 복합 대입의 산술 결과, 값 복사, 실패 후 상태 보존을 직접 확인할 필요가 있었다.

검사는 논리 시나리오 160개 × 스크립트 디버그 정보 유무 2 × 직접 컴파일/바이트코드 로드 2
× 같은 Program을 공유하는 독립 Instance 2 = 총 1,280회다.
여기서 디버그 정보 유무는 `CompileDesc.includeDebugInfo`이며, C++ Debug 빌드 시험을 뜻하지 않는다.

- Vector2/3/4와 Quaternion의 각 성분, 네 연산, int/float 피연산자.
- 다른 성분과 복사 원본이 유지되는지, 함수 인자로 전달한 값이 원본을 바꾸지 않는지.
- 전역 벡터 및 네이티브 프로퍼티에서 받은 벡터의 수정.
- 인덱스 식과 우변 식의 부수 효과가 각각 한 번만 실행되는지.
- 음수·범위 초과·잘못된 타입의 인덱스, 잘못된 타입의 우변, 지원하지 않는 `%=`의 오류.
- 실패 후 별도 `Verify` 호출로 원본·공유 사본 값이 유지되는지.
- map/list/primitive array의 기존 복합 대입 결과.

## 5. 예제 프로젝트와 배포 구성을 추가한 이유

[Samples/minigame](../../Samples/minigame/README.md)은 엔진 소스나 에셋 없이 빌드할 수 있는
C++17 호스트다. 실제 사용 방식의 핵심인 다음 흐름을 작은 프로젝트에서 보여준다.

`RegisterObject → FreezeBindings → Compile 한 번 → 오브젝트별 CreateInstance`
`→ onInstanceBind → 전역 초기화에서 AddEvent → 매 틱 콜백 호출 → 순서대로 해제`

- `GameObject.Name`, `GameObject.Position`, `GameObject.AddEvent("OnUpdate", fn)`,
  `Services.Log`만 새 예제 호스트에서 구현했다. 언어 기본 제공 전역이나 전체 엔진 API라고 설명하지 않는다.
- 두 객체가 같은 Program을 쓰되 서로 다른 상태·속도·콜백을 가지며 120개의 고정 틱을 실행한다.
  `--verify`는 실제 C++ 좌표와 각 인스턴스의 exported 업데이트 카운터를 검사한다.
- 네이티브 객체 주소를 안정적으로 유지하고, FunctionHandle을 먼저 해제한 다음
  Instance → Program → Runtime 순서로 파괴한다. 순환 참조 수집은 호스트 틱당 한 번 호출한다.
- `--record`로 네이티브 계산 결과를 저장하며 `viewer.html`은 그 좌표를 재생한다.
  브라우저에서 VM을 실행하거나 실시간 3D 엔진을 구현한 예제는 아니다.
- 독립 CMake 빌드, 일정한 `bin/` 경로, 스크립트·뷰어 복사, CPack ZIP과 라이선스 동봉을 추가했다.
  독립 Windows 빌드는 static CRT를 사용해 VC++ 재배포 패키지 설치 부담을 줄인다.

루트에는 `NEOSCRIPT_BUILD_EXAMPLES` 옵션을 추가했다. desktop은 기본 ON, Android/iOS는
기본 OFF로 두었다. 모바일 플러그인 교차 빌드에 데스크톱 예제 실행 파일을 끼워 넣지 않기 위해서다.
독립 예제 빌드는 루트 smoke/examples 옵션을 OFF로 고정하여 전체 회귀와 자기 자신을 중복 빌드하지 않는다.

Windows CI에는 예제 빌드 → CTest → CPack → ZIP artifact 업로드를 추가했다.
기존 Linux CI는 루트 CMake를 사용하므로 새 회귀와 예제를 함께 검사한다.
GitHub Release에 ZIP을 자동 게시하는 단계는 추가하지 않았다.

## 6. 문서 및 웹 설정 변경

| 대상 | 변경 이유 |
| --- | --- |
| [ReadMe.md](../../ReadMe.md) | 대상 사용자·용도·주요 기능·완결된 실행 예제를 앞에 배치하고 오래된 C# 미래 계획 표현을 제거 |
| [docs/API.md](../API.md), [docs/ScriptAuthoring.md](../ScriptAuthoring.md) | 성분 복합 대입, 값 복사, 인덱스 조건, float `%=` 미지원 계약 명시 |
| [docs/Embedding.md](../Embedding.md) | API 조각을 읽기 전에 전체 빌드 가능한 호스트 예제로 갈 수 있는 링크 추가 |
| [docs/img/minigame.png](../img/minigame.png) | 실제 네이티브 실행 결과를 재생한 화면을 보여주어 예제의 결과를 미리 확인 가능하게 함 |

GitHub 웹에서 저장한 소개는 “Embeddable scripting language for C++ and Unity games,
with C-like syntax, a bytecode VM, native object bindings, and VS Code debugging.”이다.
Topics는 `scripting-language`, `cpp`, `embeddable`, `game-development`, `unity`,
`virtual-machine`, `debug-adapter-protocol`이다. 웹 설정 변경은 Git diff에 나타나지 않는다.

## 7. 실제 수행한 검증과 한계

| 환경·시험 | 결과 |
| --- | --- |
| Windows MSVC x64 Release, 루트 CTest | 13/13 통과 |
| WSL Ubuntu GCC 13 Release, 루트 CTest | 13/13 통과 |
| 전용 벡터 회귀 | 1,280/1,280 통과 |
| Windows console `--smoke` | compiler regression 2,828/2,828 및 literal 검사 통과 |
| Windows console `--run cycle`, `--v2smoke` | 통과, v2 smoke 0 failures |
| Windows console `--compiler-error-regression` | 28/28 통과 |
| 독립 예제 빌드·CTest·CPack·ZIP 압축 해제 후 실행 | 통과, 객체당 120 updates |
| Windows 실행 파일 의존성 확인 | WS2_32.dll, KERNEL32.dll; 별도 MSVC 런타임 DLL 없음 |
| 브라우저 뷰어 | 네이티브 기록 로드, 재생·일시정지·프레임 탐색 확인 |
| 변경 소스 UTF-8 BOM, `git diff --check`, 문서 로컬 링크 | 확인 |

코드를 푸시하지 않아 변경된 GitHub Actions의 원격 실행 결과는 아직 없다.
Android/iOS/Unity 플레이어 및 전체 게임 엔진 실행, C++ Debug 빌드, 성능 측정은 이번 검증에 포함하지 않았다.
기존 console 빌드의 post-build 복사로 별도 콘텐츠 트리의 백업 도구 실행 파일도 갱신되었다.
그 파일은 이 저장소의 diff에 포함되지 않으며 별도 커밋·푸시는 하지 않았다.

## 8. Claude가 특히 검토할 부분

1. `VectorModify`의 검증 → 스칼라 계산 → 오류 확인 → COW 기록 순서가 별칭과 실패 의미론을 보존하는가.
2. `GetVM()->IsLastErrorMsg()` 사용이 호출 오류 초기화 규약과 일치하며 정상 기록을 잘못 막지 않는가.
3. 나눗셈 0·NaN/Infinity는 기존 float 산술을 재사용하며 전용 경계 시험에는 포함하지 않았다.
   추가 계약이나 시험이 필요한가. map/list 안의 중첩 벡터 lvalue와 네이티브 프로퍼티 직접 성분
   갱신의 write-back도 이번 전용 시험 범위 밖이므로 필요하면 컴파일러 규약과 함께 확인할 것.
4. 예제의 객체·콜백·Instance 수명이 맞는가. 동적 삭제되는 객체의 liveness 추적은 예제 범위 밖이다.
5. desktop 예제 기본 ON, 독립 빌드의 CACHE FORCE, static CRT, 설치·ZIP 구성이 다른 소비자 빌드에 적절한가.
6. 현재 Windows CI의 새 CTest는 독립 예제 시험만 실행한다. 새 벡터 전용 회귀는 Linux CI와
   로컬 Windows 전체 CTest에서 검증했다. Windows CI에도 루트 회귀 실행을 추가할 필요가 있는가.
7. README가 실제 지원·실행 환경·기록 재생의 성격을 정확히 설명하고 효과나 성능을 과장하지 않는가.

검토 시 `git diff`뿐 아니라 `git status --short`와 미추적 파일도 포함해야 한다.
`Samples/minigame/`, `Tests/vector_component_regression.cpp`, 새 이미지와 이 문서는 아직 미추적이다.

재실행 기본 명령(저장소 루트):

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
.\build\Release\neoscript_vector_component_regression.exe
```

예제 독립 빌드와 ZIP 생성은 [예제 README](../../Samples/minigame/README.md)의 명령을 따른다.
이번 설명문 작성에서는 소스 코드를 추가 수정하거나 위 테스트를 다시 실행하지 않았다.

## 9. 검토 반영 (2026-10-21, Claude)

- `VectorModify`를 지우고 다섯 핸들러가 [NeoVMWorker.inl](../../NeoSource/NeoVMWorker.inl)의 `VecComponentOp`를 부른다.
  인덱스·피연산자 검사를 먼저 끝내고 성분 float에 직접 연산하므로 VarInfo 임시값, op 재분기, 범용 산술,
  오류 문자열 검사가 없다. 실패 경로가 쓰기 전에 끝나므로 "실패하면 기록하지 않는다"가 코드에 그대로 있다.
  8절 2번의 `IsLastErrorMsg()`는 `_sErrorMsgDetail`이 실행 루프 종료 뒤 `ReportRunException`에서 채워져
  그 시점에는 항상 거짓이었다. 결과값은 같았으므로 전용 회귀는 이를 구분하지 못했다.
- `VecWritable()`([NeoVMWorker_Var.inl](../../NeoSource/NeoVMWorker_Var.inl)): 단독 소유면 호출 없이 쓰고
  공유 중일 때만 `VecCopyOnWrite`를 부른다. `v[i] = x` 경로도 같이 쓴다. 콘솔 마이크로벤치에서 성분 복합 대입이
  읽기+쓰기 두 명령 수준으로 내려왔다.
- 전용 회귀에 `v[i]++`, `++v[i]`, `v[i]--`, `--v[i]`와 전위/후위 반환값 2건을 더해 162 × 8 = 1,296회.
- Windows CI에 루트 CMake 빌드 + CTest 단계를 추가해 새 회귀가 MSVC에서도 돈다(8절 6번).
- API.md·ScriptAuthoring.md에 컨테이너 원소·네이티브 프로퍼티 안의 벡터 성분 쓰기가 임시 사본에 가서
  사라진다는 한계를 적었다(8절 3번). 되써 넣기 명령은 별도 작업이다.
- README의 릴리스 문구를 "when published"로 바꿨다(8절 7번).
