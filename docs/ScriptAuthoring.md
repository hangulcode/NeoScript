# NeoScript 스크립트 사용 설명서

> **줄 수 제한 예외: 이 문서(`docs/ScriptAuthoring.md`)에만 줄 수 제한을 적용하지 않는다.**
> `AGENTS.md`를 비롯한 다른 문서의 줄 수 제한은 그대로 유지한다.

AI와 사람이 `.ns` 스크립트를 작성할 때 먼저 읽는 실용 안내서다.
현재 저장소의 컴파일러와 표준 라이브러리를 기준으로 한다.
다른 언어와 비슷한 문법만 보고 지원 기능이나 동작을 추측하지 않는다.
기능을 찾을 때는 이 문서의 예제와 [전체 API 목록](API.md)을 함께 확인한다.

## 1. 작업을 시작할 때 먼저 확인할 것

| 항목 | 반드시 지킬 규칙 |
| --- | --- |
| 함수 선언 순서 | 같은 모듈의 이름 있는 함수는 뒤에서 정의해도 호출할 수 있다. `fun F();` 전방 선언은 오류다. |
| 변수 선언 순서 | 전역·지역 변수와 `const`는 반드시 사용보다 앞에 선언한다. 함수 본문도 예외가 아니다. |
| 모듈 접근 | `alias.Function()`과 `alias.EXPORTED_CONST`를 쓴다. `alias.variable`은 지원하지 않는다. |
| 조건 | `if`·`while`의 조건은 `bool`로 만든다. 숫자나 문자열을 참으로 자동 변환하지 않는다. |
| 숫자 | `int`는 32비트 정수, `float`는 32비트 실수다. 정수끼리 `/` 하면 정수 나눗셈이다. |
| 인덱스 | 목록·기본형 배열·벡터는 0부터 시작한다. 문자열은 `s[i]`로 읽지 않는다. |
| 문자열 | 연결은 `..`, 부분 문자열은 `sub(시작, 길이)`, 검색 실패는 `find`의 `-1`이다. |
| `null` 대입 | 목록에서는 칸을 유지하고, 맵에서는 해당 키를 삭제한다. |
| 복사 | 목록·맵·기본형 배열의 대입은 같은 저장소를 참조한다. 벡터 대입은 값 복사다. |
| 익명 함수 | 지역값을 값으로 캡처한다. 바깥 지역변수를 함께 수정하는 방식이 아니다. |
| API 이름 | 대소문자와 인자 수가 중요하다. 필요한 기능이 없다고 판단하기 전에 `API.md`를 검색한다. |
| 검증 환경 | 호스트가 제공하는 API와 NeoScript 표준 API를 구분한다. 콘솔에는 게임 엔진 바인딩이 없다. |

빠른 탐색:

- [기본 문법·숫자](#basics), [함수·선언 순서](#functions), [모듈·공개 상수](#modules)
- [분기·반복·switch](#control), [자료구조](#containers), [문자열](#strings)
- [함수값·클로저](#closures), [수학·벡터·난수](#math), [코루틴](#coroutines)
- [기타 기존 기능](#other), [실행·오류 확인](#verification), [한도·검토 목록](#checklist)

예제는 별도 설명이 없으면 각각 독립적인 파일로 실행할 수 있다.
코드 블록의 `cpp` 표시는 편집기의 색상 표시용이다. 코드는 C++가 아니라 NeoScript다.
API 표의 `인자: 타입` 표기도 설명용이며 실제 함수 매개변수는 `var 이름`으로 쓴다.

<a id="basics"></a>
## 2. 기본 문법과 숫자

문장은 `;`로 끝내고 블록은 `{ ... }`로 감싼다. 주석은 `//`와 `/* ... */`를 쓴다.
식별자와 함수 이름은 대소문자를 구분한다. 파일과 리소스 이름은 소문자로 작성한다.
한글이 들어가는 소스는 저장소 규칙에 따라 UTF-8 BOM으로 저장한다.

```cpp
var count = 3;
var label = "apple";
var enabled = true;
var optional;                     // 초기값은 null

fun Describe(var name, var amount)
{
    return name .. ": " .. amount;
}

print(Describe(label, count));     // apple: 3
print(optional == null);          // true
```

자주 쓰는 연산자는 `+ - * / %`, 비교 `== != < <= > >=`, 논리 `&& ||`,
비트 연산 `& | ^ ~ << >>`다. 대입과 `+= -= *= /= %=`, `++ --`도 쓸 수 있다.
논리식은 명시적인 비교로 작성하고, 혼합 연산은 괄호로 의도를 드러낸다.
`&&`와 `||`는 단락 평가하므로 결과가 결정되면 오른쪽 식을 실행하지 않는다.

```cpp
var count = 2;
var capacity = 5;
print(count / capacity);             // 0
print(tofloat(count) / capacity);    // 0.4
print("count=" .. count);            // count=2
print((count > 0) && (count < capacity)); // true
```

- 실수 나눗셈이 필요하면 **나누기 전에** 한쪽을 `tofloat`로 바꾼다.
  `tofloat(count / capacity)`는 이미 잘린 값을 바꾸므로 소용없다.
- `float`는 단정밀도다. 큰 정수 ID를 실수로 바꿨다가 복원하면 값이 달라질 수 있다.
- `+`를 문자열 연결에 쓰지 않는다. `..`가 값들을 문자열로 바꿔 연결한다.
- `toint`, `tofloat`, `tostring`, `tosize`, `type`은 import 없이 쓴다.
  이들은 키워드이므로 함수값처럼 변수나 콜백에 담지 않는다.
- `print(value)`는 기본 콘솔에서 한 줄을 출력한다. 여러 값을 출력하려면
  `print("x=" .. x .. ", y=" .. y)`처럼 먼저 연결한다.
  `print(a, b)`는 두 값을 연결하되 기본 콘솔에서 줄바꿈을 붙이지 않는다.
  인자 세 개 이상은 지원하지 않는다. 호스트가 출력 함수를 바꾸면 줄바꿈 정책도 달라질 수 있다.

<a id="functions"></a>
## 3. 함수는 먼저 수집하고, 변수는 선언 순서를 지킨다

### 이름 있는 함수와 상호 재귀

컴파일러는 모듈마다 함수 이름과 인자 정보를 먼저 수집한다.
호출과 함수값 참조가 정의보다 앞에 와도 된다. 상호 재귀에도 전방 선언이 필요 없다.

```cpp
print(IsEven(10));                  // true

fun IsEven(var n)
{
    if (n == 0) return true;
    return IsOdd(n - 1);
}

fun IsOdd(var n)
{
    if (n == 0) return false;
    return IsEven(n - 1);
}
```

`fun IsEven(var n);`처럼 몸통 없는 전방 선언을 추가하면 **컴파일 오류**다.
함수 정의는 하나만 둔다. 직접 이름으로 호출하는 스크립트 함수는 인자 수도 맞춰야 한다.
선언 수집이 import나 전역 초기화의 실행 순서를 옮겨 주는 것은 아니다.

### 전역변수도 함수 본문보다 먼저 선언한다

```cpp
var score = 10;

fun AddScore(var amount)
{
    score += amount;
    return score;
}

print(AddScore(5));                 // 15
```

다음은 **의도적으로 잘못된 예제**다. 나중에 호출하더라도 선언 위치 때문에 실패한다.

```cpp
fun ReadScore() { return score; }   // 오류: 이 위치에서 score는 아직 선언되지 않았다
var score = 10;
```

지역변수와 `const`에도 같은 규칙을 적용한다.
호출이 허용되는 위치와 초기값이 준비되는 시점도 구분한다.
전역 초기화는 소스 순서이므로, 초기화 도중 나중에 초기화될 상태에 의존하는 호출을 피한다.

이름 있는 중첩 함수도 모듈의 함수 이름 공간에 속하며 바깥 함수의 지역변수를 캡처하지 않는다.
지역값을 캡처하려면 [익명 함수](#closures)를 쓴다.

### 호출 결과에 선택자를 이어 쓸 수 있다

```cpp
fun MakeItems() { return [10, 20, 30]; }
fun MakeText() { return "red,blue"; }
fun MakeRecord() { return {"items": [7, 8]}; }

print(MakeItems().len());            // 3
print(MakeItems()[1]);               // 20
print(MakeText().split(",")[1].len()); // 4
print(MakeRecord().items[0]);         // 7
```

`f().member`, `f()[i]`, 메서드 호출 뒤의 인덱싱을 쓸 수 있다.
이 기능을 지원하지 않는다고 생각해서 매번 중간변수를 만들 필요는 없다.
문자열 리터럴 `"abc".len()`, 문자열 상수 `TEXT.len()`, 괄호식 `("a" .. "b").len()`에도
같은 선택자를 이어 쓸 수 있다. 문자열 인덱싱 자체는 지원하지 않으므로 한 글자는 `sub(i, 1)`로 읽는다.

<a id="modules"></a>
## 4. 모듈, 전역 이름, 공개 상수

### import는 컴파일할 때 해석한다

`import settings as cfg;`는 로더의 라이브러리 경로에서 `settings.ns`를 찾고
그 모듈을 `cfg`라는 별칭으로 사용한다. 문자열 경로나 확장자를 직접 넣는 문법이 아니다.
별칭을 생략한 `import settings;`에서는 `settings.Function()`처럼 쓴다.
import는 별칭을 사용하는 코드보다 먼저 둔다.

파일 검색에 실패하면 같은 이름의 기본 모듈(`math`, `system`, `coroutine`)을 찾는다.
따라서 사용자 파일을 이 이름들과 겹치게 만들지 않는 편이 좋다.
중첩 import도 같은 로더의 라이브러리 경로를 사용한다.
현재 파일의 디렉터리를 기준으로 자동 검색한다고 가정하지 않는다. 순환 import는 오류다.

- 한 번의 컴파일에서 같은 모듈을 여러 번 import하면 이미 읽은 모듈을 재사용한다.
  별칭 두 개를 붙였다고 상태 사본 두 개가 생기지 않는다.
- 따로 생성한 실행 인스턴스끼리는 모듈 전역 상태가 자동 공유되지 않는다.
- 모듈 별칭은 전역변수 이름 충돌을 막는 이름 공간이 아니다.
  모듈을 합쳐 컴파일할 때 전역 이름이 겹치지 않도록 접두사를 사용한다.
- 다른 모듈의 런타임 데이터는 함수로 전달한다. `cfg.items[0]` 같은 변수 접근은 지원하지 않는다.

### const와 export const

`const`는 파일의 전역 위치에 선언하는 **컴파일 타임 상수**다.
사용보다 먼저 선언하며 함수 안에는 선언하지 않는다.
정수·실수·문자열·bool·null과 이미 알려진 상수의 계산식을 쓸 수 있다.
런타임 변수, 함수 호출, 목록·맵은 상수 초기값으로 쓰지 않는다.

아래 예제는 두 파일이다. `settings.ns`는 로더가 찾는 라이브러리 디렉터리에 둔다.

**settings.ns**

```cpp
export const TEXT_SPEED = 22;
export const MODE_PLAY = 2;
const INTERNAL_LIMIT = 100;         // 이 파일 안에서만 사용

var settings_volume = 5;
var settings_points = [10, 20];

fun GetVolume() { return settings_volume; }
fun SetVolume(var value) { settings_volume = value; }
fun GetPoint(var index) { return settings_points[index]; }
```

**main.ns**

```cpp
import settings as cfg;
const DOUBLE_SPEED = cfg.TEXT_SPEED * 2;

print(DOUBLE_SPEED);                // 44
cfg.SetVolume(8);
print(cfg.GetVolume());             // 8
print(cfg.GetPoint(1));              // 20

switch (2)
{
case cfg.MODE_PLAY:
    print("play");                  // play
default:
    print("other");
}
```

| 선언 | 다른 스크립트에서 사용 | C++ 호스트에 런타임 값으로 노출 |
| --- | --- | --- |
| `const NAME = ...;` | 파일 내부만 | 없음 |
| `export const NAME = ...;` | `alias.NAME` | 없음 |
| `var value = ...;` | 별칭 접근 불가. 접근 함수 제공 | export 없이 공개하지 않음 |
| `export var value = ...;` | 여전히 별칭 접근 불가 | 전역값으로 공개 |
| `fun F() { ... }` | `alias.F()` | 호스트 호출용으로는 export 사용 |
| `export fun F() { ... }` | `alias.F()` | 함수로 공개 |

공개 상수는 이름만 import한 파일에 풀어 놓지 않는다. 반드시 `cfg.TEXT_SPEED`처럼 쓴다.
`cfg.INTERNAL_LIMIT`, `cfg.settings_volume`은 오류다.
공개 상수에는 대입·복합 대입·증감이 불가능하다.
필요하면 `export const COPY = cfg.TEXT_SPEED;`로 명시적으로 다시 공개한다.
상수값을 바꾸면 사용하는 쪽의 컴파일 이미지도 다시 만들어야 한다.

함수가 목록이나 맵 전체를 반환하면 호출자도 **같은 저장소**를 받는다.
읽기 전용 인터페이스가 필요하면 `GetPoint(index)`처럼 원소를 반환하거나 복사본을 만든다.

<a id="control"></a>
## 5. 분기, 반복, switch

### bool 조건과 단락 평가

`if (1)`, `if ("text")`, `if (list)`는 참으로 취급되지 않는다.
`if (count > 0)`, `if (text != "")`, `if (value != null)`처럼 비교한다.
`else if`를 사용하며 레거시 `elif`는 제거되었다.

```cpp
var item = null;
if ((item != null) && (item.enabled == true))
{
    print("enabled");
}
else
{
    print("unavailable");           // 이것만 출력. item.enabled는 평가하지 않는다.
}
```

### 범위 for와 foreach

```cpp
var values = [10, 20, 30];
var sum = 0;
for (var i in 0, values.len())       // 0, 1, 2
{
    sum += values[i];
}
print(sum);                        // 60

var backward = "";
for (var i in 3, 0, -1)             // 3, 2, 1: 시작 포함, 끝 제외
{
    backward = backward .. i;
}
print(backward);                   // 321

var sumAgain = 0;
foreach (var value in values) sumAgain += value;
print(sumAgain);                   // 60
```

- 기본 문법은 `for (var i in 시작, 끝[, 간격])`이다. 간격을 생략하면 1이다.
  현재 기본 빌드에서 `for (var i = 0; i < n; i++)` 문법을 쓰지 않는다.
- 시작은 포함하고 끝은 제외한다. 역방향 목록 순회는 `len()-1`에서 `-1`까지 간다.
- 간격은 런타임 값도 가능하다. 방향이 범위와 반대면 0회 실행하고, 간격 0은 런타임 오류다.
- 목록·기본형 배열·set의 `foreach` 변수 하나는 **값**이다. 인덱스가 아니다.
  인덱스가 필요하면 범위 `for`를 쓴다.
- 맵은 `foreach (var key in map)`과 `foreach (var key, value in map)`을 지원한다.
- 순회 중 원소 수를 바꾸는 코드는 피한다. 특히 기본형 배열의 길이를 바꾸면 오류다.
  맵에서 여러 키를 지우려면 `keys()`로 얻은 목록을 순회하며 원본 맵을 수정한다.
- `while (조건)`도 있다. `break`는 가장 안쪽 반복문 또는 switch를 벗어나고,
  `continue`는 반복문의 다음 반복으로 간다. `do ... while`은 없다.

### case에는 상수 식을 쓸 수 있다

```cpp
const CMD_START = 1;
const CMD_PAUSE = CMD_START + 1;
var command = 2;
var result = "";

switch (command)
{
case CMD_START:
    result = "start";
case CMD_PAUSE, 3:
    result = "pause";
default:
    result = "unknown";
}
print(result);                     // pause
```

각 case 몸통이 끝나면 switch를 벗어난다. **C/C++식 fallthrough가 없다.**
따라서 위 예제에는 `break`가 없어도 된다. 한 몸통에 여러 값은 쉼표로 묶는다.

case에는 `bool`, `int`, `string`의 리터럴·상수 식·`const`·호스트 define·공개 모듈 상수를 쓴다.
`var`는 값이 변하지 않더라도 런타임 변수이므로 case 상수가 아니다.
실수 case, 같은 case의 중복, default 중복은 오류다.
타입을 엄격히 비교하므로 `true`, `1`, `"1"`은 다른 case다.
저장소 작성 규칙대로 case를 값 순서로 정리하고 default를 마지막에 둔다.

<a id="containers"></a>
## 6. 자료구조와 복사 의미

| 종류 | 생성 | 길이 | 중요한 차이 |
| --- | --- | --- | --- |
| 목록 `list` | `[1, "a", null]`, `[]` | `.len()` / `tosize` | 여러 타입을 담으며 순서를 유지 |
| 맵 `map` | `{"hp": 10}`, `{}` | `.len()` / `tosize` | 키로 접근. 순회 순서에 의존하지 않기 |
| 기본형 `array` | `system.array(0, n)` | `.len()` / `tosize` | bool·int·float 중 한 종류 |
| `set` | `system.set(list)` | `.len()` / `tosize` | 중복을 제거해 만들고 순회 |
| 벡터 | `math.Vector3(x, y, z)` | **`tosize`** | 고정 성분 수, 값 복사, 전용 수학 함수 |

### 목록: 추가, 삽입, 크기 조정

```cpp
var items = [10, 20, 30];
items[1] = null;
print(items.len());                 // 3
print(items[1] == null);             // true
print(items[2]);                    // 30

items.append(40);                   // 맨 뒤에 추가
items.append(5, 0);                 // 값, 위치 순서: 인덱스 0에 삽입
print(items[0]);                    // 5
items.resize(6);                    // 늘어난 칸은 null
print(items[5] == null);             // true
items.resize(0);                    // 비우기
print(items.len());                 // 0
```

인덱스 대입은 이미 존재하는 칸에만 한다. 먼저 `append` 또는 `resize`로 길이를 늘린다.
삽입은 `insert(위치, 값)` 또는 기존 `append(값, 위치)`, 삭제는 `remove(위치)`,
비우기는 `resize(0)`이다. `find`, `clear` 메서드는 없다.
`remove`는 지운 값을 반환하고 뒤 원소를 당겨 길이를 줄인다. `null` 대입은 칸을 유지한다.
삽입 위치는 0부터 길이까지, 삭제 위치는 0부터 길이-1까지다. 잘못된 타입·범위는 오류다.

```cpp
var items = [3, 1, 2];
items.insert(1, 9);                 // [3, 9, 1, 2]
print(items.remove(1));             // 9: 다시 [3, 1, 2]
items.sort(fun(var a, var b) { return a < b; });
print(items[0]);                    // 1
print(items[2]);                    // 3
```

`sort(비교 함수)`는 원본 목록을 정렬하며 같은 목록을 참조하는 변수에도 결과가 보인다.
비교 함수는 인자 둘을 받아 첫 값이 앞에 와야 하면 `true`, 아니면 `false`를 반환한다.
동등한 원소의 기존 순서를 유지한다. 내림차순은 `a > b`를 쓴다.
비교 함수는 동기로 실행한다. `yield`, `sleep`, 비동기 대기, `coroutine.resume`·`close`는 오류다.
호스트가 실행 시간 제한을 설정한 경우(`timeoutMs >= 0`, 0도 포함) 목록 정렬은 실행 오류다.
비교 함수 호출이나 목록 변경 전에 거절하며 시간 제한을 몰래 해제하지 않는다.
시간 제한이 없는 호출에서만 사용할 수 있다. 이때 비교 함수는 반드시 종료해야 한다.
비교 함수 안에서 정렬 대상이나 그 원소를 수정하지 않는다. 원소 추가·삭제·길이 변경·재정렬은
감지하여 오류로 중단하고 정렬 결과를 덮어쓰지 않는다. 콜백 자체의 부수 효과를 되돌리지는 않는다.
`foreach` 중 삽입·삭제·정렬도 오류가 되므로 순회가 끝난 뒤 수행한다.

### 맵: 키 조회와 삭제

```cpp
var indexes = [7, 9];
var record = {"name": "oak", "id": indexes[1]};
var byId = {indexes[0]: record};
print(record.id);                   // 9: 목록 전체가 아니라 원소값
print(byId[7].name);                // oak

record.hp = 100;                    // 없는 키에 대입하면 추가
record["hp"] = null;               // 키 삭제
print(record.hp == null);           // true
print(record.len());                // 2

var t = {};
t.fun = 3;                         // 점 뒤의 fun은 멤버 이름
print(t.fun);                       // 3
```

- `m.name`과 `m["name"]`은 같은 문자열 키에 접근한다. 동적인 키는 `m[key]`를 쓴다.
- 없는 키를 읽으면 `null`이다. `null`을 값으로 저장하는 대신 키가 삭제되므로,
  “키는 있지만 값은 비어 있음”을 구별하려면 별도 플래그나 감싼 값을 사용한다.
- `m.keys()`와 `m.values()`가 반환하는 순서는 삽입 순서가 아니다.
  게임 로직이나 저장 결과가 이 순서에 의존하도록 만들지 않는다.
- `m.reserve(n)`으로 용량을 미리 준비할 수 있다.
- `m.sort(cmp)`는 **맵 값**을 정렬하는 기능이다. 목록 정렬 API나 키 정렬 API로 생각하지 않는다.
  비교 함수의 bool 반환, 정렬 중 키 추가·삭제 제한 등은 [API 목록](API.md#8-map-methods)을 확인한다.

### 참조 공유와 얕은 복사

```cpp
fun CopyList(var source)
{
    var result = [];
    foreach (var value in source) result.append(value);
    return result;
}

var original = [10, 20];
var shared = original;
shared[0] = 99;
print(original[0]);                 // 99

var copied = CopyList(original);
copied[0] = 7;
print(original[0]);                 // 여전히 99
```

이 복사는 바깥 목록만 새로 만든다. 원소가 맵·목록·배열이면 그 안쪽 저장소는 여전히 공유한다.
맵도 같은 원리로 키·값을 순회해 복사할 수 있다. 깊은 복사가 필요하면 데이터 구조에 맞게 구현한다.
함수 인자로 컨테이너를 전달하거나 반환하는 것도 자동 복사를 만들지 않는다.

### 기본형 배열: 많은 수치와 플래그

```cpp
import system;

var ids = system.array(0, 3);
var weights = system.array(0.5, 2);
var flags = system.array(false, 2);
ids[1] = 7;
ids[1] += 2;
weights.resize(3);
flags[0] = true;

print(ids[1]);                      // 9
print(weights[2]);                  // 0.5: 생성할 때 지정한 초기값
print(flags[0]);                    // true
```

`initial`의 타입이 원소 타입을 정한다. int 배열에는 `0`, float 배열에는 `0.0`을 쓴다.
정수 인덱스만 허용하고 범위를 벗어난 읽기·쓰기는 오류다.
int↔float 대입은 변환하며 float→int는 0 방향으로 잘라낸다. bool 배열에는 bool만 넣는다.
`append(value)`와 `resize(n)`을 지원하지만 목록의 중간 삽입 형태는 지원하지 않는다.

### set: 중복 제거 후 순회

```cpp
import system;
var unique = system.set([1, 1, 2, 3]);
print(tosize(unique));              // 3
var sum = 0;
foreach (var value in unique) sum += value;
print(sum);                        // 6
```

set의 크기는 `unique.len()` 또는 `tosize(unique)`로 읽는다.
`add`, `remove`, `contains` 메서드는 없다. 없는 메서드를 호출하면 런타임 오류다.
빈번한 존재 검사나 추가·삭제가 필요하면 값을 `true`로 넣는 맵 등의 구조를 선택한다.

<a id="strings"></a>
## 7. 문자열: 검색·부분 추출·치환을 직접 다시 만들지 않기

| 기능 | 사용법 | 결과·경계 조건 |
| --- | --- | --- |
| 길이 | `s.len()` / `tosize(s)` | UTF-8 문자 수 |
| 부분 추출 | `s.sub(start, count)` | 두 번째 인자는 끝 위치가 아니라 길이 |
| 검색 | `s.find(text)` | 첫 위치, 없으면 `-1`, 빈 검색어는 `0` |
| 첫 치환 | `s.replace(find, to)` | 첫 일치만 교체, 없으면 원본 |
| 전체 치환 | `s.replaceAll(find, to)` | 겹치지 않는 모든 일치 교체, 없으면 원본 |
| 분리 | `s.split(separator)` | 여러 글자 구분자도 가능. **빈 구분자는 전달하지 않기** |
| 대소문자 | `s.upper()` / `s.lower()` | ASCII 대상 |
| 공백 제거 | `s.trim()` / `ltrim()` / `rtrim()` | 공백 문자 ` `만 제거. 탭·개행은 제거하지 않음 |
| 형식에 맞춰 표시 | `format(pattern, ...)` / `pattern.format(...)` | 정수·실수·문자열, 폭·소수 자릿수 지정 |

```cpp
var text = "A한😀글Z";
print(text.len());                  // 5
print(text.sub(1, 3));              // 한😀글
print(text.find("😀"));             // 2
print(text.find("missing"));        // -1
print(text.sub(99, 10) == "");      // true
print(text.sub(-2, 2));             // A한
print(text.sub(1, 0) == "");        // true
```

`sub`는 시작을 `[0, 길이]`로 보정하고 끝을 넘는 길이는 남은 부분까지만 읽는다.
길이 인자가 0 이하이거나 시작이 끝에 있으면 빈 문자열이다.
인덱스는 바이트 수가 아니다. 다만 조합 문자·여러 코드 포인트로 된 이모지를
화면상의 글자 한 칸으로 묶어 세는 기능이라고 가정해서도 안 된다.
문자 하나를 읽으려면 `s.sub(i, 1)`을 쓴다. `s[i]`는 지원하지 않는다.

```cpp
var text = "red/red/red";
print(text.replace("red", "blue"));    // blue/red/red
print(text.replaceAll("red", "blue")); // blue/blue/blue
print(text.replace("none", "x"));      // red/red/red
print(text);                          // 원본도 red/red/red

var grow = "aaa";
print(grow.replaceAll("a", "aa"));     // aaaaaa: 교체한 부분은 다시 검색하지 않음
print(grow.replaceAll("", "x"));       // aaa
print(grow.replace("", "x"));          // xaaa: 기존 replace의 동작

var fields = "left::right";
print(fields.split("::")[1]);          // right
```

모든 문자열 메서드는 결과값을 돌려주며 원본 변수를 바꾸지 않는다.
수정 결과를 유지하려면 `text = text.replaceAll(...)`로 대입한다.
검색·치환은 정규식이나 Lua 패턴이 아니라 **문자열의 정확한 일치**다.
`split("")`은 현재 구현에서 진행하지 못하므로 빈 구분자를 호출 전에 걸러낸다.
잘못된 인자 수·타입까지 자동 보정하는 것은 아니다.

문자열에는 `\n`, `\r`, `\t`, `\"`, `\'`, `\\` 이스케이프를 쓸 수 있다.
`\uXXXX`는 16진수 네 자리로 글자를 지정한다. `"\uAE00\uC790"`는 `"글자"`,
`"\uD83D\uDE00"`는 이모지 하나다. 직접 UTF-8 글자를 적어도 된다.
잘못된 16진수나 짝이 없는 서로게이트는 컴파일 오류다. 현재 문자열 API가 보존할 수 없는
NUL 문자 `\u0000`도 컴파일 오류다. 역슬래시-u 자체를 넣으려면 `"\\u0041"`처럼 쓴다.
`match`·정규식 API는 없다. 실수 저장·복원용 문자열에는 `math.tostr`를 검토한다.

### 자릿수와 소수점 표시

```cpp
print(format("id=%04d, weight=%.2f %s", 7, 1.25, "kg")); // id=0007, weight=1.25 kg
print("[%4.2s]".format("한😀글"));                       // [  한😀]
print("done=%d%%".format(100));                          // done=100%
```

import 없이 전역 `format` 또는 문자열 메서드 `format`을 쓴다.
`%d`는 int, `%f`는 int·float, `%s`는 string을 받고 `%%`는 퍼센트 자체를 넣는다.
`%f` 기본 소수 자릿수는 6이며 `%.2f`처럼 바꾼다. `%8d`는 최소 폭 8,
`%04d`는 숫자 앞을 0으로 채우고 `%-8s`는 왼쪽 정렬한다.
`%.2s`는 UTF-8 문자 둘까지 읽으며 폭도 문자 수 기준이다. 화면 표시 칸 수와는 다르다.
소수점은 시스템 언어와 관계없이 `.`이다. 형식이 맞지 않거나 인자가 남아도 오류다.
폭은 최대 **4,096자**, `%f`의 소수 자릿수는 최대 **1,024자리**이며,
한 호출의 전체 결과는 **UTF-8 1MiB(1,048,576바이트)**까지다. 넘으면 실행 오류다.
각 지정자의 결과와 일반 글자를 모두 합산하고, 큰 변환·복사·패딩 전에 검사한다.
`%s` 정밀도는 기존 문자열을 자르는 값이라 int 범위를 허용하되 실제 출력은 바이트 한도를 따른다.
이는 `format`의 확장 한도이며 컴파일 이미지나 다른 문자열 연산의 크기 한도와는 별개다.
지원 범위는 [format API](API.md#21-string-formatting)를 확인한다.

<a id="closures"></a>
## 8. 함수값과 익명 함수의 캡처

함수는 변수·목록·맵에 담거나 다른 함수로 전달할 수 있다.
익명 함수는 `fun(var x) { ... }`로 만든다. 화살표 함수 문법을 사용하지 않는다.

```cpp
fun Apply(var callback, var value) { return callback(value); }
fun Twice(var value) { return value * 2; }

print(Apply(Twice, 3));              // 6
var addOne = fun(var value) { return value + 1; };
print(Apply(addOne, 3));             // 4
```

### 지역값 캡처는 익명 함수별로 독립적이다

```cpp
fun MakeCounter(var start)
{
    var value = start;
    return fun()
    {
        value += 1;
        return value;
    };
}

var first = MakeCounter(10);
var second = MakeCounter(20);
print(first());                     // 11
print(first());                     // 12: 같은 익명 함수의 캡처 상태는 유지
print(second());                    // 21
```

주변 지역값은 익명 함수가 만들어질 때 값으로 캡처한다.
캡처한 스칼라를 수정해도 바깥 함수의 지역변수나 같은 지역값을 캡처한 다른 익명 함수가 바뀌지 않는다.
전역변수는 이러한 캡처 사본이 아니라 원래 전역에 접근한다.

### 여러 콜백이 상태를 공유하려면 컨테이너를 공유한다

```cpp
fun MakeSharedCounter()
{
    var state = {"value": 0};
    var increment = fun() { state.value += 1; };
    var read = fun() { return state.value; };
    return [increment, read];
}

var callbacks = MakeSharedCounter();
callbacks[0]();
callbacks[0]();
print(callbacks[1]());              // 2
```

캡처한 맵 참조가 같은 저장소를 가리키므로 상태를 공유한다.
반대로 그 맵 안에 자신을 캡처한 함수를 넣으면 순환 참조가 될 수 있다.
순환 회수는 호스트의 `CollectCycles` 지원에 달려 있다.
재귀는 이름 있는 함수로 먼저 해결하고, 재진입·중첩 캡처가 필요하면
[호스트 문서의 캡처 설명](Embedding.md)을 확인한다.

<a id="math"></a>
## 9. 이미 있는 수학·벡터 기능 활용하기

먼저 `import math;`를 쓴다. `math.Clamp`, `math.Lerp`의 대문자를 소문자로 바꾸지 않는다.
반대로 삼각함수는 `math.sin`, `math.cos`, `math.atan2`처럼 소문자다.

| 목적 | 기존 함수 |
| --- | --- |
| 절댓값·제곱근·거듭제곱 | `abs`, `sqrt`, `pow` |
| 반올림·내림·올림 | `round`, `floor`, `ceil` — 결과는 float, 정수가 필요하면 `toint` |
| 최소·최대·범위 제한 | `Min`, `Max`, `Clamp`, `Clamp01` |
| 선형 보간·역보간 | `Lerp`, `Lerp3`, `InverseLerp`, `Remap` |
| 부드러운 전이 | `SmoothStep01`, `SmoothStep` |
| 속도 제한 이동 | `MoveToward(current, target, maxDelta)` |
| 반복·왕복 | `Repeat`, `PingPong` |
| 각도 보간 | `WrapAngle`, `DeltaAngle`, `LerpAngle`, `MoveTowardAngle` |
| 라디안·도 변환 | `rad`, `deg` |
| 거리·내적·외적 | `Distance2`, `Distance3`, `DistanceSquared3`, `Length3`, `Dot3`, `Cross3` |
| 정규화 | `Normalize3(vector, fallback)` — fallback 필수 |
| 회전 | `Quaternion`, `quat_from_basis`, `quat_slerp`, `RotateVectorByQuat` |
| 색상·해시·실수 문자열 | `ColorRGB`, `ColorARGB`, `Hash32`, `tostr` |

전체 인자 순서와 타입은 [math API](API.md#3-math)를 확인한다.
`Lerp`·`InverseLerp`·`Remap`은 자동으로 `[0, 1]`에 제한하지 않는다.
필요하면 `Clamp01`을 조합한다. 각도 관련 함수는 라디안 기준이다.

```cpp
import math;

var position = math.Vector3(1.0, 2.0, 3.0);
var copy = position;
copy[0] = 9.0;
print(position[0]);                 // 1: 벡터는 값 복사
print(tosize(position));            // 3

var zero = math.Vector3(0.0, 0.0, 0.0);
var forward = math.Vector3(0.0, 0.0, 1.0);
var direction = math.Normalize3(zero, forward);
print(direction[2]);                // 1: 길이가 작으면 fallback
print(math.Lerp(10.0, 20.0, 0.5));   // 15
print(math.MoveToward(0.0, 10.0, 2.0)); // 2
```

- 벡터는 `[x, y, z]` 목록과 다른 타입이다. 벡터 API에 목록을 넘기지 않는다.
- 성분 접근은 `v[0]`, `v[1]`이다. `v.x`·`v.y`·`v.z`는 지원하지 않는다.
- 같은 성분 수의 덧셈·뺄셈, 벡터와 스칼라의 곱셈·나눗셈을 사용할 수 있다.
- 벡터에는 `.len()`, 목록 메서드, `foreach`를 쓰지 않는다. 성분 수는 `tosize`다.
- `math.Quaternion(w, x, y, z)`는 **w가 먼저**다. 런타임 타입 이름은 `"Vector4"`다.
  일반 벡터 연산 `*`를 회전 합성이라고 해석하지 않는다.
- `ColorRGB`·`ColorARGB`의 색상 성분은 0–255 정수가 아니라 0.0–1.0 값이다.

### 난수와 재현성

`math.srand(seed)`, `math.rand()`, `math.Rand01()`, `math.RandRange(min, max)`가 있다.
`rand()`는 0–32767이며 `Rand01()`에는 **1.0도 포함**된다.
따라서 `toint(math.Rand01() * list.len())`를 그대로 인덱스로 쓰면 범위를 벗어날 수 있다.
난수 상태는 worker에 속한다. 같은 시드만으로 호스트 호출 순서·worker 재사용·시간 입력·맵 순회까지
모두 결정적이라고 단정하지 않는다. 재현 시험에서는 이러한 입력과 실행 순서도 고정한다.

<a id="coroutines"></a>
## 10. 코루틴과 실행 양보

코루틴은 별도 OS 스레드가 아니라 실행을 중단·재개하는 기능이다.
`import coroutine;` 후 `create`, `resume`, `status`, `close`를 사용한다.

```cpp
import coroutine;

var progress = 0;
fun Work(var amount)
{
    progress += amount;
    yield;                         // 함수 호출이 아니라 문장
    progress += amount;
}

var co = coroutine.create(Work);
print(coroutine.status(co));        // suspended
coroutine.resume(co, 2);            // 첫 resume의 추가 인자가 Work에 전달됨
print(progress);                   // 2
print(coroutine.status(co));        // suspended
coroutine.resume(co);
print(progress);                   // 4
print(coroutine.status(co));        // dead
```

일반적인 위 흐름에서는 재개한 코루틴이 `yield` 또는 종료에 이를 때까지 진행한 뒤
호출자의 `resume` 다음 문장으로 돌아온다. 반환값 전달은 이 예제처럼 공유 상태 등으로 설계한다.
`resume`은 `suspended` 상태에만 호출한다. `dead` 상태에 다시 호출하면 오류다.
`close(co)`는 끝난 코루틴에 호출해도 괜찮다.

`sleep(ms)`는 실행 인스턴스를 일정 시간 쉬게 한다. `yield;`와 용도가 다르다.
호스트에서 스크립트로 재진입한 동기 콜백 안에서는 `sleep`·`yield` 등 중단 동작이 제한된다.
프레임별 재개, 실행 예산, 취소와 수명은 호스트 계약도 확인한다.

<a id="other"></a>
## 11. 그 밖에 알아 두면 유용한 기존 기능

| 필요 | 기능·주의점 |
| --- | --- |
| 타입 검사 | `type(x)`는 `"int"`, `"float"`, `"list"`, `"map"`, `"array"`, `"function"` 등 문자열 반환 |
| 여러 자료형의 크기 | `tosize(x)`는 문자열·목록·맵·배열·set·벡터에 사용 가능 |
| 원본 줄 번호 | `__LINE__`은 해당 소스의 현재 줄 번호 |
| 시각 | `system.time()`은 Unix 초, `system.date(format, time)`은 로컬 시간 형식화 |
| 경과 시간 측정 | `system.clock()`은 C `clock()` 기반 초. 실제 벽시계 시간으로 가정하지 않기 |
| 동적 코드 실행 | `system.load(source, name)`로 module을 만들고 `system.pcall(module)`로 최상위 코드 실행 |
| HTTP | `system.aysnc_create()`로 요청 객체 생성. **aysnc라는 철자가 실제 API 이름** |
| 편집기 | VS Code 확장의 완성·서명 도움말·중단점·단계 실행·호출 스택·지역변수 보기 |

`system.pcall`은 Lua의 보호 호출이 아니다. 내부 오류가 호출자에게 전파되고 실행을 중단한다.
`system.load` 결과에 `module.Function()` 형태로 이름 있는 함수를 호출하는 기능도 없다.
파일 간 함수 호출에는 정적 import를 쓴다. `load`의 `name` 인자는 현재 구현에서 사용되지 않으므로
동적 소스에 파일명을 붙이는 기능으로 기대하지 않는다.

HTTP 객체의 `add_header`, `get`, `post`, `wait`, `close`와 콜백 인자는
[async API](API.md#9-async-methods)에 있다. 현재 HTTP 전송 구현은 Windows용이며
`wait()`는 대기하므로 매 프레임 처리처럼 다루지 않는다.

호스트가 바인딩한 native object는 `type()`만으로 일반 `null`과 구별할 수 없다.
객체의 유효성은 해당 호스트의 바인딩 계약을 따른다.
익명 함수의 타입을 확인할 때도 먼저 변수에 담은 뒤 `type(variable)`을 사용한다.

## 12. 호스트 API와 표준 기능을 구분하기

`Services.*` 같은 엔진 API는 호스트가 등록하는 기능이다.
이 저장소의 기본 콘솔에서 자동으로 제공되지 않는다.
존재 여부, 인자 순서, 반환 타입, null 가능성은 호스트의 바인딩 목록과 실제 호출부로 확인한다.
비슷한 이름의 다른 엔진 API를 추측해서 만들지 않는다.

헤드리스 시험에서는 순수 로직을 함수로 분리하고 외부 입력을 인자로 전달한다.
렌더링·오디오·시각·파일 접근처럼 호스트에 의존하는 부분은 필요한 인터페이스만 대체한다.
콘솔에서 통과했다는 이유만으로 실제 엔진 바인딩과 수명까지 검증했다고 보고하지 않는다.

호스트에서 호출할 함수는 `export fun`으로 공개한다.
콘솔의 `--file`은 파일 최상위 코드를 실행하므로, 함수를 정의만 해 놓으면 그 함수의 시험은 실행되지 않는다.
시험 파일 끝에서 직접 호출해야 한다.

<a id="verification"></a>
## 13. 콘솔로 실행하고 오류 위치 확인하기

저장소 루트에서 콘솔을 빌드한다. 실제 설치된 Visual Studio 경로를 사용한다.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Samples\console\console.sln /m /p:Configuration=Release /p:Platform=x64
```

`Samples/console`을 작업 디렉터리로 두고 실행하면 기본 라이브러리 경로 `../../Lib/`와
기존 시험의 상대 경로가 맞는다. 스크립트 파일 경로는 실행할 파일로 바꾼다.

```powershell
Set-Location Samples\console
.\x64\Release\console.exe --file C:\scripts\main.ns
.\x64\Release\console.exe --file C:\scripts\main.ns --debug
.\x64\Release\console.exe --file C:\scripts\main.ns --asm --stats
```

`--file`에 절대 경로를 줘도 import 경로가 그 파일 옆으로 바뀌는 것은 아니다.
다른 라이브러리 디렉터리는 호스트 로더 또는 VS Code `launch.json`의 `libPath`로 지정한다.
VS Code 확장 설정과 기능은 [ReadMe](../ReadMe.md#vs-code-debugger-setup)를 참고한다.

### 최소 자체 점검 예제

```cpp
var failures = 0;
fun Check(var ok, var label)
{
    if (ok == false)
    {
        failures += 1;
        print("FAIL: " .. label);
    }
}

var values = [1, 2, 3];
values[1] = null;
Check(values.len() == 3, "list keeps length");
Check(values[1] == null, "list stores null");
var record = {"selected": values[2]};
Check(record.selected == 3, "map stores indexed value");
record.selected = null;
Check(record.len() == 0, "map removes null entry");
print("failures=" .. failures);     // failures=0
```

이 `Check`는 예제에서 정의한 함수다. 표준 `assert` 함수가 있다고 가정하지 않는다.
이처럼 실패를 출력만 하는 하네스는 실패해도 프로세스 종료 코드가 성공일 수 있으므로,
자동화에서는 실패 수와 출력까지 확인하거나 호스트가 시험 결과를 종료 코드로 반영하게 한다.

### 컴파일 오류와 런타임 오류를 구분한다

현재 컴파일 오류는 다음처럼 원본 이름과 위치를 포함한다.

```text
.../settings.ns: Error (12, 8): unknown identifier 'missing'
```

import가 여러 단계여도 실제 오류가 난 파일의 줄·열을 보존한다.
파일을 문자열로 이어 붙여 컴파일하면 원본 경계가 사라진다.
그 경우에는 import를 유지하거나 하네스에서 원본 위치 대응 정보를 관리해야 한다.
런타임 호출 스택과 디버거 소스 탐색에는 디버그 정보를 포함해 다시 컴파일한다.

| 증상 | 먼저 확인할 것 |
| --- | --- |
| `unknown identifier` | 변수·const·import가 사용 전에 선언되었는지, 철자·대소문자 |
| `forward declarations` | 몸통 없는 `fun F(...);`를 남겼는지 |
| `no public member` | 변수나 비공개 const를 별칭으로 읽었는지. 함수 또는 export const 사용 |
| `invalid function call` | API의 정확한 인자 수·타입, 필요한 vector와 실제 list의 혼동 |
| 문자열 인덱스 오류 | `s[i]` 대신 `s.sub(i, 1)` 사용 |
| 목록·배열 인덱스 오류 | 0 기반 범위와 먼저 크기를 늘렸는지 확인 |
| import 실패 | 로더의 libPath, 파일명 소문자, 작업 디렉터리, 파일 인코딩 |
| 문서 예제가 옛 오류를 냄 | 실제 실행한 console/엔진 바이너리가 현재 소스로 빌드되었는지 |

문제를 보고할 때 실행한 바이너리, 빌드 상태, 명령·작업 디렉터리, 최소 소스,
예상 결과와 실제 오류 전문을 함께 남긴다. 확인 전에 언어의 한계나 버그로 단정하지 않는다.

<a id="checklist"></a>
## 14. 크기 한도와 작성 후 점검

과거의 **상수 약 15,000개 / 컴파일 이미지 1 MiB**를 현재 한도로 안내하지 않는다.
현재는 큰 상수 인덱스와 더 큰 이미지를 지원하고 전역 인덱스도 상수 개수와 분리했다.
다만 모든 값이 무제한인 것은 아니다. 대표적으로:

- 전역변수는 32,767개이며 상수 개수와 별도다.
- 이미지에 저장하는 문자열·심볼 이름 하나는 UTF-8 **32,767바이트** 한도다.
  문자열 메서드가 세는 문자 수와 혼동하지 않는다.
- 함수·지역/임시 슬롯·인자·상대 점프·디버그 줄 번호에도 인코딩 한도가 있다.
  큰 함수나 생성된 소스는 줄 수만으로 가능 여부를 판단하지 말고 실제 컴파일한다.
- 이미지 전체는 더 이상 1 MiB 고정 버퍼가 아니며, 메모리와 32비트 오프셋 한도가 적용된다.
  한도와 버퍼 쓰기 실패는 컴파일 오류로 처리한다.

정확한 목록과 이미지 호환성은 [컴파일 이미지 한도](Embedding.md#compiled-image-capacity-format-0123)에 있다.
함수 수와 상수 수는 같은 수치가 아니다. 엔진 이미지 형식이 바뀌면 기존 바이트코드를 다시 만든다.

AI가 코드를 제출하기 전에 확인할 것:

1. 필요한 기능을 이 문서와 API 목록에서 찾았는가? 기존 `find`, 보간·벡터 함수 등을 놓치지 않았는가?
2. 함수 선언 순서와 변수·상수 선언 순서를 구분했는가?
3. 모듈 변수 접근 대신 함수를 쓰고, 공유 상수는 `export const`로 제공했는가?
4. 정수 나눗셈, bool 조건, 0 기반 인덱스, 문자열 길이 인자를 정확히 썼는가?
5. 목록의 null과 맵의 삭제, 참조 공유와 값 복사를 구분했는가?
6. 람다 캡처가 바깥 스칼라를 함께 수정한다고 가정하지 않았는가?
7. 순회 순서·시간·난수·호스트 입력에 기대는 부분을 시험에서 통제했는가?
8. 최소 실행 예제에서 기대 결과를 확인했는가? 빈 값·실패·경계 사례도 확인했는가?
9. 검증한 환경과 아직 검증하지 않은 호스트 동작을 구분해 보고했는가?

## 15. 문서와 구현을 함께 유지하기

새 기능, 변경된 경계 처리, 제거된 문법은 이 문서의 예제와 [API 목록](API.md)에 함께 반영한다.
컴파일러 내부 구현만 설명하고 스크립트 작성법을 빠뜨리지 않는다.
오래된 제약을 우회하는 예제를 새 기본 사용법으로 복사하지 않는다.
설명과 실제 동작이 다르면 작은 스크립트로 확인하고 문서·구현 중 어느 쪽이 잘못됐는지 판단한다.

| 확인할 내용 | 근거 |
| --- | --- |
| 전체 함수·인자·반환값 | [API.md](API.md), [NeoLib.cpp](../NeoSource/NeoLib.cpp) |
| 문법·선언·선택자 | [ReadMe.md](../ReadMe.md), [NeoParser.cpp](../NeoSource/NeoParser.cpp) |
| 최근 언어 동작과 오류 | [language_regression.cpp](../Tests/language_regression.cpp) |
| 분기·반환·null 대입 | [control_flow_regression.cpp](../Tests/control_flow_regression.cpp) |
| 호출 뒤 선택자 | [postfix_regression.cpp](../Tests/postfix_regression.cpp) |
| 컴파일 크기 한도 | [compiler_limits_regression.cpp](../Tests/compiler_limits_regression.cpp) |
| 호스트 호출·캡처・수명 | [Embedding.md](Embedding.md) |
| 저장소 작업·빌드·회귀 규칙 | [AGENTS.md](../AGENTS.md) |

컴파일러나 표준 라이브러리를 수정할 때는 해당 회귀 시험과 문서도 함께 갱신한다.
시험 결과는 실제 실행한 것만 기록하며, 빌드하지 않은 소스를 검증 완료로 보고하지 않는다.
